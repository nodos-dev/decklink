#pragma once

#include "Common.hpp"
#include "nosDeckLinkDevice/nosDeckLinkDevice.h"

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
	void OnInputVideoFormatChanged_DeckLinkThread(BMDDisplayMode newDisplayMode, BMDPixelFormat pixelFormat);

	int32_t AddInputVideoFormatChangeCallback(nosDeckLinkInputVideoFormatChangeCallback callback, void* userData);
	void RemoveInputVideoFormatChangeCallback(int32_t callbackId);
	
	std::mutex CallbacksMutex;
	std::unordered_map<int32_t, std::pair<nosDeckLinkInputVideoFormatChangeCallback, void*>> VideoFormatChangeCallbacks;
	int32_t NextCallbackId = 0;
};

}
