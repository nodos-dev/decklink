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
protected:
	IDeckLinkVideoFrame* Frame = nullptr;
	IDeckLinkVideoBuffer* Buffer = nullptr;
	BMDBufferAccessFlags AccessFlags = bmdBufferAccessReadAndWrite;
};
}