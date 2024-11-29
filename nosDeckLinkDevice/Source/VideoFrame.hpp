#pragma once

#include "Common.hpp"

namespace nos::decklink
{
class VideoFrame
{
public:
	VideoFrame(IDeckLinkVideoFrame* videoFrame);
	~VideoFrame();

	bool StartAccess(BMDBufferAccessFlags);
	void* GetBytes();
	bool EndAccess();
	size_t Size;
protected:
	IDeckLinkVideoFrame* Frame = nullptr;
	IDeckLinkVideoBuffer* Buffer = nullptr;
	std::optional<BMDBufferAccessFlags> AccessFlags = std::nullopt;
};
}