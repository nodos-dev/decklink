#include "InputHandler.hpp"

#include <Nodos/Modules.h>

namespace nos::decklink
{
	
// The input callback class
class InputCallback : public Object<IDeckLinkInputCallback>
{
	
public:
	InputHandler* Input;
	
	InputCallback(InputHandler *input) : Input(input)
	{
	}
	
	// The callback that is called when a property of the video input stream has changed.
	HRESULT		STDMETHODCALLTYPE VideoInputFormatChanged (/* in */ BMDVideoInputFormatChangedEvents notificationEvents, /* in */ IDeckLinkDisplayMode *newDisplayMode, /* in */ BMDDetectedVideoInputFormatFlags detectedSignalFlags)
	{
		BMDPixelFormat      pixelFormat = bmdFormat10BitYUV;
		// STRINGOBJ           displayModeString = NULL;
		BMDVideoInputFlags  videoInputFlags = bmdVideoInputEnableFormatDetection;
		
		// // Check for video field changes
		// if (notificationEvents & bmdVideoInputFieldDominanceChanged)
		// {
		//     BMDFieldDominance fieldDominance;
		//     
		//     fieldDominance = newDisplayMode->GetFieldDominance();
		//     printf("Input field dominance changed to ");
		//     switch (fieldDominance) {
		//         case bmdUnknownFieldDominance:
		//             printf("unknown\n");
		//             break;
		//         case bmdLowerFieldFirst:
		//             printf("lower field first\n");
		//             break;
		//         case bmdUpperFieldFirst:
		//             printf("upper field first\n");
		//             break;
		//         case bmdProgressiveFrame:
		//             printf("progressive\n");
		//             break;
		//         case bmdProgressiveSegmentedFrame:
		//             printf("progressive segmented frame\n");
		//             break;
		//         default:
		//             printf("\n");
		//             return E_FAIL;
		//     }
		// }
		//
		// // Check if the pixel format has changed
		// if (notificationEvents & bmdVideoInputColorspaceChanged)
		// {
		//     printf("Input color space changed to ");
		//     if (detectedSignalFlags & bmdDetectedVideoInputYCbCr422)
		//     {
		//         printf("YCbCr422 ");
		//         if (detectedSignalFlags & bmdDetectedVideoInput8BitDepth)
		//         {
		//             printf("8-bit depth\n");
		//             pixelFormat = bmdFormat8BitYUV;
		//         }
		//         else if (detectedSignalFlags & bmdDetectedVideoInput10BitDepth)
		//         {
		//             printf("10-bit depth\n");
		//             pixelFormat = bmdFormat10BitYUV;
		//         }
		//         else
		//         {
		//             printf("\n");
		//             return E_FAIL;
		//         }
		//     }
		//     else if (detectedSignalFlags & bmdDetectedVideoInputRGB444)
		//     {
		//         printf("RGB444 ");
		//         if (detectedSignalFlags & bmdDetectedVideoInput8BitDepth)
		//         {
		//             printf("8-bit depth\n");
		//             pixelFormat = bmdFormat8BitARGB;
		//         }
		//         else if (detectedSignalFlags & bmdDetectedVideoInput10BitDepth)
		//         {
		//             printf("10-bit depth\n");
		//             pixelFormat = bmdFormat10BitRGB;
		//         }
		//         else if (detectedSignalFlags & bmdDetectedVideoInput12BitDepth)
		//         {
		//             printf("12-bit depth\n");
		//             pixelFormat = bmdFormat12BitRGB;
		//         }
		//         else
		//         {
		//             printf("\n");
		//             return E_FAIL;
		//         }
		//     }
		// }
		
		// // Check if the video mode has changed
		// if (notificationEvents & bmdVideoInputDisplayModeChanged)
		// {
		//     std::string modeName;
		//     
		//     // Obtain the name of the video mode 
		//     if (newDisplayMode->GetName(&displayModeString) == S_OK)
		//     {
		//         StringToStdString(displayModeString, modeName);
		//         
		//         printf("Input display mode changed to: %s", modeName.c_str());
		//         
		//         if (detectedSignalFlags & bmdDetectedVideoInputDualStream3D)
		//         {
		//             videoInputFlags |= bmdVideoInputDualStream3D;
		//             printf(" (3D)");
		//         }
		//         printf("\n");
		//         
		//         // Release the video mode name string
		//         STRINGFREE(displayModeString);
		//     }
		// }
		
		if (notificationEvents & (bmdVideoInputDisplayModeChanged | bmdVideoInputColorspaceChanged))
		{
			Input->Close();
			Input->Open(newDisplayMode->GetDisplayMode(), pixelFormat);
		}
		
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE VideoInputFrameArrived (/* in */ IDeckLinkVideoInputFrame* videoFrame, /* in */ IDeckLinkAudioInputPacket* audioPacket)
	{
		Input->OnInputFrameArrived_DeckLinkThread(videoFrame);
		return S_OK;
	}

private:
	virtual ~InputCallback(void) {}

};

InputHandler::~InputHandler()
{
	Release(Interface);
}

bool InputHandler::Open(BMDDisplayMode displayMode, BMDPixelFormat pixelFormat)
{
	if (IsActive)
		return false;

	// Create an instance of notification callback
	auto callback = new InputCallback(this);
	if (callback == nullptr)
	{
		nosEngine.LogE("Could not create input callback object");
		return false;
	}
	
	// Set the callback object to the DeckLink device's input interface
	auto res = Interface->SetCallback(callback);
	if (res != S_OK)
	{
		nosEngine.LogE("Could not set callback - result = %08x", res);
		return false;
	}
	Release(callback);
	res = Interface->EnableVideoInput(displayMode, pixelFormat, bmdVideoInputFlagDefault);
	if (res != S_OK)
	{
		nosEngine.LogE("Could not enable video input - result = %08x", res);
		return false;
	}
	{
		IDeckLinkDisplayMode* displayModeInterface = nullptr;
		res = Interface->GetDisplayMode(displayMode, &displayModeInterface);
		if (res != S_OK)
			return false;
		res = displayModeInterface->GetFrameRate(&FrameDuration, &TimeScale);
		if (res != S_OK)
			return false;
		Release(displayModeInterface);
	}
	IsActive = true;
	
	// Start capture
	res = Interface->StartStreams();
	if (res != S_OK)
	{
		nosEngine.LogE("Could not start streams - result = %08x", res);
		Close();
		return false;
	}

	return true;
}

bool InputHandler::Close()
{
	if (!IsActive)
		return false;

	//if (S_OK != Interface->PauseStreams())
	//	return false;

	//if (S_OK != Interface->FlushStreams())
	//	return false;

	if (S_OK != Interface->StopStreams())
		return false;

	if (S_OK != Interface->DisableVideoInput())
		return false;

	IsActive = false;
	return true;
}

bool InputHandler::WaitFrame(std::chrono::milliseconds timeout)
{
	ReadRequestedCond.notify_one();
	std::unique_lock lock(VideoBufferMutex);
	CanReadCond.wait_for(lock, timeout);
	return true;
}

void InputHandler::DmaTransfer(void* buffer, size_t size)
{
	{
		std::unique_lock lock(VideoBufferMutex);
		size_t actualSize = VideoBuffer.Size();
		if (!actualSize)
			return;
		if (actualSize != size)
		{
			nosEngine.LogE("DMA Read: Buffer size does not match frame size");
		}
		auto copySize = std::min(actualSize, size);
		std::memcpy(buffer, VideoBuffer.Data(), copySize);
	}
}

void InputHandler::OnInputFrameArrived_DeckLinkThread(IDeckLinkVideoInputFrame* frame)
{
	{
		std::unique_lock lock(VideoBufferMutex);
		ReadRequestedCond.wait_for(lock, std::chrono::milliseconds(100));
	}
	{
		std::unique_lock lock(VideoBufferMutex);
		void* bytes;
		frame->GetBytes(&bytes);
		size_t bufSize = frame->GetHeight() * frame->GetRowBytes();
		if (VideoBuffer.Size() != bufSize)
			VideoBuffer = nos::Buffer(bufSize);
		std::memcpy(VideoBuffer.Data(), bytes, bufSize);
	}
	CanReadCond.notify_one();
}
}
