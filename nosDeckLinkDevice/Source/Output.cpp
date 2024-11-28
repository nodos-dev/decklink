#include "DeckLinkDevice.hpp"

#include <Nodos/Modules.h>

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
	{
		std::scoped_lock lock(Output->VideoFramesMutex);
		Output->FrameQueue.push_back(completedFrame);
	}
	Output->FrameAvailableCond.notify_one();
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
	if (IsActive)
		return false;

	HRESULT res;
	{
		std::scoped_lock lock(VideoFramesMutex);
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
			FrameQueue.push_back(frame);
		}
	}

	res = Interface->EnableVideoOutput(displayMode, bmdVideoOutputFlagDefault);
	if (res != S_OK)
		return false;

	IsActive = true;

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

	res = Interface->StartScheduledPlayback(0, TimeScale, 1.0);
	if (res != S_OK)
	{
		nosEngine.LogE("SubDevice: Failed to start scheduled playback");
		Close();
		return false;
	}

	return true;
}

bool OutputHandler::Close()
{
	if (!IsActive)
	{
		nosEngine.LogE("SubDevice: Output is not active");
		return false;
	}

	if (Interface->StopScheduledPlayback(0, nullptr, TimeScale) != S_OK)
	{
		nosEngine.LogE("Failed to stop scheduled playback");
	}

	auto res = Interface->DisableVideoOutput();
	if (res != S_OK)
	{
		nosEngine.LogE("SubDevice: Failed to disable video output");
		return false;
	}
	IsActive = false;
	{
		std::scoped_lock lock(VideoFramesMutex);
		for (auto& frame : VideoFrames)
			Release(frame);
		FrameQueue.clear();
		TotalFramesScheduled = 0;
		NextFrameToSchedule = 0;
	}
	return true;
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

void OutputHandler::OnDmaTransferCompleted()
{
	ScheduleNextFrame();
}
}
