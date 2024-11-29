#include "VideoFrame.hpp"

namespace nos::decklink
{
VideoFrame::VideoFrame(IDeckLinkVideoFrame* videoFrame)
	: Frame(videoFrame), Size(videoFrame->GetRowBytes() * videoFrame->GetHeight())
{
	Frame->QueryInterface(IID_IDeckLinkVideoBuffer, (void**)&Buffer);
}

VideoFrame::~VideoFrame()
{
	if (AccessFlags)
		EndAccess();
	Release(Buffer);
}

bool VideoFrame::StartAccess(BMDBufferAccessFlags flags)
{
	if (!Buffer)
		return false;
	AccessFlags = flags;
	return Buffer->StartAccess(flags) == S_OK;
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
	if (!AccessFlags)
		return false;
	if (!Buffer)
		return false;
	return Buffer->EndAccess(*AccessFlags) == S_OK;
}

}