#include "VideoFrame.hpp"

namespace nos::decklink
{
VideoFrame::VideoFrame(IDeckLinkVideoFrame* videoFrame)
	: Frame(videoFrame)
{
	Frame->QueryInterface(IID_IDeckLinkVideoBuffer, (void**)&Buffer);
}

VideoFrame::~VideoFrame()
{
	Release(Buffer);
}

bool VideoFrame::StartAccess(BMDBufferAccessFlags flags)
{
	if (!Buffer)
		return false;
	AccessFlags = flags;
	return Buffer->StartAccess(AccessFlags) == S_OK;
}

void* VideoFrame::GetBytes()
{
	if (!Buffer)
		return nullptr;
	void* bytes;
	Buffer->GetBytes(&bytes);
	return bytes;
}

bool VideoFrame::EndAccess()
{
	if (!Buffer)
		return false;
	return Buffer->EndAccess(AccessFlags) == S_OK;
}

}