#include "OutputHandler.hpp"

#include <Nodos/Modules.h>

#include "VideoFrame.hpp"

namespace nos::decklink
{
	
class OutputCallback : public Object<IDeckLinkVideoOutputCallback>
{
public:
	OutputCallback(OutputHandler* outputHandler) :
		Output(outputHandler)
	{
	}

	HRESULT	STDMETHODCALLTYPE ScheduledFrameCompleted(IDeckLinkVideoFrame* completedFrame, BMDOutputFrameCompletionResult result) override;
	HRESULT	STDMETHODCALLTYPE ScheduledPlaybackHasStopped(void) override;

	// IUnknown needs only a dummy implementation
	HRESULT	STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID *ppv) override
	{
		return E_NOINTERFACE;
	}

private:
	OutputHandler*  Output;
};
	
HRESULT	OutputCallback::ScheduledFrameCompleted(IDeckLinkVideoFrame* completedFrame, BMDOutputFrameCompletionResult result)
{
	Output->ScheduledFrameCompleted_DeckLinkThread(completedFrame, result);
	return S_OK;
}

HRESULT	OutputCallback::ScheduledPlaybackHasStopped(void)
{
	return S_OK;
}

OutputHandler::~OutputHandler()
{
	Release(Interface);
	for (auto& frame : VideoFrames)
		Release(frame);
}

bool OutputHandler::Open(BMDDisplayMode displayMode, BMDPixelFormat pixelFormat)
{
	HRESULT res;
	{
		std::unique_lock lock(VideoFramesMutex);
		for (auto& frame : VideoFrames)
			Release(frame);
		for (auto& frame : VideoFrames)
		{
			// Get width and height from display mode
			long width, height;
			{
				IDeckLinkDisplayMode* displayModeInterface = nullptr;
				res = Interface->GetDisplayMode(displayMode, &displayModeInterface);
				if (res != S_OK)
					return false;
				width = displayModeInterface->GetWidth();
				height = displayModeInterface->GetHeight();
				res = displayModeInterface->GetFrameRate(&FrameDuration, &TimeScale);
				if (res != S_OK)
					return false;
				Release(displayModeInterface);
			}
			Interface->CreateVideoFrame(width, height, width * 2, pixelFormat, bmdFrameFlagDefault, &frame);
			if (!frame)
				return false;
			WriteQueue.push_back(frame);
		}
	}

	res = Interface->EnableVideoOutput(displayMode, bmdVideoOutputFlagDefault);
	if (res != S_OK)
		return false;

	auto outputCallback = new OutputCallback(this);
	if (outputCallback == nullptr)
	{
		nosEngine.LogE("Could not create output callback");
		return false;
	}
	res = Interface->SetScheduledFrameCompletionCallback(outputCallback);
	Release(outputCallback);
	if (res != S_OK)
	{
		nosEngine.LogE("SubDevice: Failed to set output callback");
		Close();
		return false;
	}

	return true;
}

bool OutputHandler::Start()
{
	{
		std::unique_lock lock(VideoFramesMutex);
		TotalFramesScheduled = 0;
	}
	auto res = Interface->StartScheduledPlayback(0, TimeScale, 1.0);
	if (res != S_OK)
	{
		nosEngine.LogE("SubDevice: Failed to start scheduled playback");
		return false;
	}
	return true;
}

bool OutputHandler::Stop()
{
	if (Interface->StopScheduledPlayback(0, nullptr, TimeScale) != S_OK)
	{
		nosEngine.LogE("Failed to stop scheduled playback");
		return false;
	}
	return true;
}

bool OutputHandler::Close()
{
	auto res = Interface->DisableVideoOutput();
	if (res != S_OK)
	{
		nosEngine.LogE("SubDevice: Failed to disable video output");
		return false;
	}
	{
		std::unique_lock lock(VideoFramesMutex);
		for (auto& frame : VideoFrames)
			Release(frame);
		WriteQueue.clear();
		NextFrameToSchedule = 0;
	}
	return true;
}

bool OutputHandler::WaitFrame(std::chrono::milliseconds timeout)
{
	std::unique_lock lock(VideoFramesMutex);
	auto res = WriteCond.wait_for(lock, timeout, [this] {
		return !WriteQueue.empty();
	});
	return res;
}

void OutputHandler::DmaTransfer(void* buffer, size_t size)
{
	{
		std::unique_lock lock(VideoFramesMutex);
		if (WriteQueue.empty())
			return;
		auto frame = WriteQueue.front();
		VideoFrame output(frame);
		output.StartAccess(bmdBufferAccessWrite);
		size_t actualBufferSize = frame->GetRowBytes() * frame->GetHeight();
		auto videoBufferBytes = output.GetBytes();
		if (videoBufferBytes && buffer)
		{
			if (size != actualBufferSize)
			{
				nosEngine.LogE("DMA Write: Buffer size does not match frame size");
			}
			size_t copySize = std::min(size, actualBufferSize);
			std::memcpy(videoBufferBytes, buffer, copySize);
		}
		output.EndAccess();
		WriteQueue.pop_front();
	}
	ScheduleNextFrame();
}

void OutputHandler::ScheduleNextFrame()
{
	IDeckLinkVideoFrame* frameToSchedule = VideoFrames[NextFrameToSchedule];
	NextFrameToSchedule = ++NextFrameToSchedule % VideoFrames.size();
	HRESULT result = Interface->ScheduleVideoFrame(frameToSchedule, TotalFramesScheduled * FrameDuration, FrameDuration, TimeScale);
	if (result != S_OK)
		nosEngine.LogE("Failed to schedule video frame");
	++TotalFramesScheduled;
}

void OutputHandler::ScheduledFrameCompleted_DeckLinkThread(IDeckLinkVideoFrame* completedFrame, BMDOutputFrameCompletionResult result)
{
	{
		std::unique_lock lock(VideoFramesMutex);
		WriteQueue.push_back(completedFrame);
		nosEngine.WatchLog("DeckLink Output Queue Size", std::to_string(WriteQueue.size()).c_str());
	}
	WriteCond.notify_one();
	if (result != bmdOutputFrameCompleted)
	{
		const char* resultStr = nullptr;
		switch (result) {
		case bmdOutputFrameDropped: resultStr = "'Dropped'"; break;
		case bmdOutputFrameDisplayedLate: resultStr = "'DisplayedLate'"; break;
		case bmdOutputFrameFlushed: resultStr = "'Flushed'"; break;
		}
		nosEngine.LogW("DeckLink: A frame completed with %s", resultStr);
	}
}
}
