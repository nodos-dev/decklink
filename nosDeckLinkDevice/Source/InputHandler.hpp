#pragma once

#include "Common.hpp"

namespace nos::decklink
{

struct InputHandler : IOHandlerBase<IDeckLinkInput>
{
	~InputHandler() override;

	std::condition_variable ReadRequestedCond;
	std::condition_variable CanReadCond;
	std::mutex VideoBufferMutex;
	nos::Buffer VideoBuffer;
	
	bool Open(BMDDisplayMode displayMode, BMDPixelFormat pixelFormat) override;
	bool Close() override;
	bool WaitFrame(std::chrono::milliseconds timeout) override;
	void DmaTransfer(void* buffer, size_t size) override;

	void OnInputFrameArrived_DeckLinkThread(IDeckLinkVideoInputFrame* frame);
	void OnInputVideoModeChanged_DeckLinkThread(BMDDisplayMode newDisplayMode, BMDPixelFormat pixelFormat);
};

}