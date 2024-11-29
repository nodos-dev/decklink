#include "InputHandler.hpp"

#include <Nodos/Modules.h>

#include "EnumConversions.hpp"
#include "VideoFrame.hpp"

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
		BMDVideoInputFlags  videoInputFlags = bmdVideoInputEnableFormatDetection;
		
		// // Check for video field changes
		if (notificationEvents & bmdVideoInputFieldDominanceChanged)
		{
		    BMDFieldDominance fieldDominance = newDisplayMode->GetFieldDominance();
		}
		
		// Check if the pixel format has changed
		if (notificationEvents & bmdVideoInputColorspaceChanged)
		{
		    if (detectedSignalFlags & bmdDetectedVideoInputYCbCr422)
		    {
		        if (detectedSignalFlags & bmdDetectedVideoInput8BitDepth)
		            pixelFormat = bmdFormat8BitYUV;
		        else if (detectedSignalFlags & bmdDetectedVideoInput10BitDepth)
		            pixelFormat = bmdFormat10BitYUV;
		        else
		            return E_FAIL;
		    }
		    else if (detectedSignalFlags & bmdDetectedVideoInputRGB444)
		    {
		        if (detectedSignalFlags & bmdDetectedVideoInput8BitDepth)
		            pixelFormat = bmdFormat8BitARGB;
		        else if (detectedSignalFlags & bmdDetectedVideoInput10BitDepth)
		            pixelFormat = bmdFormat10BitRGB;
		        else if (detectedSignalFlags & bmdDetectedVideoInput12BitDepth)
		            pixelFormat = bmdFormat12BitRGB;
		        else
		        {
		            return E_FAIL;
		        }
		    }
		}
		
		// Check if the video mode has changed
		if (notificationEvents & bmdVideoInputDisplayModeChanged)
		{
		    // Obtain the name of the video mode
			dlstring_t displayModeString;
		    if (newDisplayMode->GetName(&displayModeString) == S_OK)
		    {
		        std::string modeName = DlToStdString(displayModeString);
		        if (detectedSignalFlags & bmdDetectedVideoInputDualStream3D)
		            videoInputFlags |= bmdVideoInputDualStream3D;
		        // Release the video mode name string
		        DeleteString(displayModeString);
		    }
		}
		
		if (notificationEvents & (bmdVideoInputDisplayModeChanged | bmdVideoInputColorspaceChanged))
		{
			Input->OnInputVideoFormatChanged_DeckLinkThread(newDisplayMode->GetDisplayMode(), pixelFormat);
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
	res = Interface->EnableVideoInput(displayMode, pixelFormat, bmdVideoInputEnableFormatDetection);
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
		// TODO: Start access here, end it in DmaTransfer instead of this copy.
		VideoFrame input(frame);
		input.StartAccess(bmdBufferAccessRead);
		auto bytes = input.GetBytes();
		size_t bufSize = frame->GetHeight() * frame->GetRowBytes();
		if (VideoBuffer.Size() != bufSize)
			VideoBuffer = nos::Buffer(bufSize);
		std::memcpy(VideoBuffer.Data(), bytes, bufSize);
		input.EndAccess();
	}
	CanReadCond.notify_one();
}

void InputHandler::OnInputVideoFormatChanged_DeckLinkThread(BMDDisplayMode newDisplayMode, BMDPixelFormat pixelFormat)
{
	// Pause video capture
	Interface->PauseStreams();
            
	// Enable video input with the properties of the new video stream
	Interface->EnableVideoInput(newDisplayMode, pixelFormat, bmdVideoInputEnableFormatDetection);

	// Flush any queued video frames
	Interface->FlushStreams();

	// Start video capture
	Interface->StartStreams();

	auto [frameGeometry, frameRate] = GetFrameGeometryAndRatePairFromDeckLinkDisplayMode(newDisplayMode);
	{
		std::unique_lock lock(CallbacksMutex);
		for (auto& [_, pair] : VideoFormatChangeCallbacks)
		{
			auto& [callback, userData] = pair;
			callback(userData, frameGeometry, frameRate, GetPixelFormatFromDeckLink(pixelFormat));
		}
	}
}

int32_t InputHandler::AddInputVideoFormatChangeCallback(nosDeckLinkInputVideoFormatChangeCallback callback, void* userData)
{
	std::unique_lock lock(CallbacksMutex);
	auto callbackId = NextCallbackId++;
	VideoFormatChangeCallbacks[callbackId] = {callback, userData};
	return callbackId;
}

void InputHandler::RemoveInputVideoFormatChangeCallback(int32_t callbackId)
{
	std::unique_lock lock(CallbacksMutex);
	VideoFormatChangeCallbacks.erase(callbackId);
}
}
