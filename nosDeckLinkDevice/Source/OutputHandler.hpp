#pragma once

#include "Common.hpp"

namespace nos::decklink
{
struct OutputHandler : IOHandlerBase<IDeckLinkOutput>
{
	std::array<IDeckLinkMutableVideoFrame*, 4> VideoFrames{};
	
	uint32_t TotalFramesScheduled = 0;
	uint32_t NextFrameToSchedule = 0;

	std::mutex VideoFramesMutex;
	std::condition_variable WriteCond;
	std::deque<IDeckLinkVideoFrame*> WriteQueue;

	~OutputHandler() override;

	bool Open(BMDDisplayMode displayMode, BMDPixelFormat pixelFormat) override;
	bool Close() override;
	bool WaitFrame(std::chrono::milliseconds timeout) override;
	void DmaTransfer(void* buffer, size_t size) override;
	
	void ScheduleNextFrame();
	void ScheduledFrameCompleted_DeckLinkThread(IDeckLinkVideoFrame* completedFrame, BMDOutputFrameCompletionResult result);
};
}