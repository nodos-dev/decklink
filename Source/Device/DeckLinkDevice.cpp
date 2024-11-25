#include "DeckLinkDevice.hpp"

// Nodos
#include <Nodos/Modules.h>
#include <nosUtil/Stopwatch.hpp>

#if _WIN32
#define dlbool_t	BOOL
#define dlstring_t	BSTR
const std::function<void(dlstring_t)> DeleteString = SysFreeString;
const std::function<std::string(dlstring_t)> DlToStdString = [](dlstring_t dl_str) -> std::string
	{
		int wlen = ::SysStringLen(dl_str);
		int mblen = ::WideCharToMultiByte(CP_ACP, 0, (wchar_t*)dl_str, wlen, NULL, 0, NULL, NULL);

		std::string ret_str(mblen, '\0');
		mblen = ::WideCharToMultiByte(CP_ACP, 0, (wchar_t*)dl_str, wlen, &ret_str[0], mblen, NULL, NULL);

		return ret_str;
	};
const std::function<dlstring_t(std::string)> StdToDlString = [](std::string std_str) -> dlstring_t
	{
		int wlen = ::MultiByteToWideChar(CP_ACP, 0, std_str.data(), (int)std_str.length(), NULL, 0);

		dlstring_t ret_str = ::SysAllocStringLen(NULL, wlen);
		::MultiByteToWideChar(CP_ACP, 0, std_str.data(), (int)std_str.length(), ret_str, wlen);

		return ret_str;
	};
#endif

namespace nos::decklink
{

class OutputCallback: public IDeckLinkVideoOutputCallback
{
public:
	OutputCallback(SubDevice* deckLinkDevice) :
		Device(deckLinkDevice),
		RefCount(1)
	{
	}

	HRESULT	STDMETHODCALLTYPE ScheduledFrameCompleted(IDeckLinkVideoFrame* completedFrame, BMDOutputFrameCompletionResult result) override;
	HRESULT	STDMETHODCALLTYPE ScheduledPlaybackHasStopped(void) override;

	// IUnknown needs only a dummy implementation
	HRESULT	STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID *ppv) override
	{
		return E_NOINTERFACE;
	}

	ULONG STDMETHODCALLTYPE AddRef() override
	{
		return ++RefCount;
	}

	ULONG STDMETHODCALLTYPE Release() override
	{
		ULONG newRefValue = --RefCount;

		if (newRefValue == 0)
			delete this;

		return newRefValue;
	}

private:
	SubDevice*  Device;
	std::atomic<int32_t> RefCount;
};
	
HRESULT	OutputCallback::ScheduledFrameCompleted(IDeckLinkVideoFrame* completedFrame, BMDOutputFrameCompletionResult result)
{
	 // When a video frame completes, reschedule another frame
	{
		std::unique_lock lock(Device->VideoFramesMutex);
		Device->CompletedFramesQueue.push_back(completedFrame);
	}
	Device->FrameCompletionCondition.notify_one();
	return S_OK;
}

HRESULT	OutputCallback::ScheduledPlaybackHasStopped(void)
{
	return S_OK;
}

template <typename T>
void Release(T& obj)
{
	if (obj)
	{
		obj->Release();
		obj = nullptr;
	}
}
	
HRESULT GetDeckLinkIterator(IDeckLinkIterator **deckLinkIterator)
{
	HRESULT result = S_OK;

	// Create an IDeckLinkIterator object to enumerate all DeckLink cards in the system
	result = CoCreateInstance(CLSID_CDeckLinkIterator, NULL, CLSCTX_ALL, IID_IDeckLinkIterator, (void**)deckLinkIterator);
	if (FAILED(result))
		nosEngine.LogE("A DeckLink iterator could not be created. The DeckLink drivers may not be installed.");

	return result;
}

SubDevice::SubDevice(IDeckLink* deviceInterface)
	: Device(deviceInterface)
{
	dlstring_t modelName;
	Device->GetModelName(&modelName);
	ModelName = DlToStdString(modelName);
	nosEngine.LogI("DeckLink Device created: %s", ModelName.c_str());
	DeleteString(modelName);

	auto res = Device->QueryInterface(IID_IDeckLinkInput, (void**)&Input);
	if (FAILED(res))
		nosEngine.LogE("DeckLinkDevice: Failed to get input interface for device: %s", ModelName.c_str());
	res = Device->QueryInterface(IID_IDeckLinkOutput, (void**)&Output);
	if (FAILED(res))
		nosEngine.LogE("DeckLinkDevice: Failed to get output interface for device: %s", ModelName.c_str());

	res = Device->QueryInterface(IID_IDeckLinkProfileAttributes, (void**)&ProfileAttributes);
	if (FAILED(res))
		nosEngine.LogE("DeckLinkDevice: Failed to get profile attributes for device: %s", ModelName.c_str());

	res = ProfileAttributes->GetInt(BMDDeckLinkSubDeviceIndex, &SubDeviceIndex);
	if (FAILED(res))
		nosEngine.LogE("DeckLinkDevice: Failed to get subdevice index for device: %s", ModelName.c_str());

	dlstring_t handle;
	res = ProfileAttributes->GetString(BMDDeckLinkDeviceHandle, &handle);
	if (FAILED(res))
		nosEngine.LogE("DeckLinkDevice: Failed to get device handle for device: %s", ModelName.c_str());
	if (handle)
	{
		Handle = DlToStdString(handle);
		DeleteString(handle);
	}
	res = ProfileAttributes->GetInt(BMDDeckLinkProfileID, &ProfileId);
	if (FAILED(res))
		nosEngine.LogE("DeckLinkDevice: Failed to get profile ID for device: %s", ModelName.c_str());

	res = Device->QueryInterface(IID_IDeckLinkProfileManager, (void**)&ProfileManager);
	if (FAILED(res))
		nosEngine.LogE("DeckLinkDevice: Failed to get profile manager for device: %s", ModelName.c_str());

	{
		// TODO: Should this be here?
		IDeckLinkProfile *profile = nullptr;
		res = ProfileManager->GetProfile(bmdProfileFourSubDevicesHalfDuplex, &profile);
		if (FAILED(res))
			nosEngine.LogE("DeckLinkDevice: Failed to get profile for device: %s", ModelName.c_str());
		int active = false;
		res = profile->IsActive(&active);
		if (FAILED(res))
			nosEngine.LogE("DeckLinkDevice: Failed to get profile active status for device: %s", ModelName.c_str());
		if (!active)
			profile->SetActive();
		Release(profile);
	}
}

SubDevice::~SubDevice()
{
	Release(Device);
	Release(Input);
	Release(Output);
	Release(ProfileAttributes);
	Release(ProfileManager);
	for (auto& frame : VideoFrames)
		Release(frame);
}

bool SubDevice::InitializeSubDeviceList()
{
	IDeckLinkIterator* deckLinkIterator = nullptr;

	HRESULT result = E_FAIL;
#ifdef _WIN32
	result = CoInitialize(NULL);
	if (FAILED(result))
	{
		nosEngine.LogE("DeckLinkDevice: Initialization of COM failed with error: %s", _com_error(result).ErrorMessage());
		return false;
	}
#endif

	result = GetDeckLinkIterator(&deckLinkIterator);
	if (FAILED(result))
	{
		return false;
	}

	IDeckLink* deckLink = NULL;
	uint32_t   deviceNumber = 0;

	// Obtain an IDeckLink instance for each device on the system
	while (deckLinkIterator->Next(&deckLink) == S_OK)
	{
		auto bmDevice = std::make_unique<SubDevice>(deckLink);
		SubDevices[deviceNumber] = std::move(bmDevice);
		deviceNumber++;
	}

	if (deckLinkIterator)
		deckLinkIterator->Release();
	return true;
}

void SubDevice::ClearSubDeviceList()
{
	SubDevices.clear();
}

SubDevice* SubDevice::GetDevice(uint32_t index)
{
	auto it = SubDevices.find(index);
	if (it != SubDevices.end())
		return it->second.get();
	return nullptr;
}

bool SubDevice::OpenOutput(BMDDisplayMode displayMode, BMDPixelFormat pixelFormat)
{
	if (!Output) 
	{
		nosEngine.LogE("DeckLinkDevice: Output interface is not available for device: %s", ModelName.c_str());
		return false;
	}
	
	auto res = Output->EnableVideoOutput(displayMode, bmdVideoOutputFlagDefault);
	if (FAILED(res))
	{
		nosEngine.LogE("DeckLinkDevice: Failed to enable video output for device: %s", ModelName.c_str());
		return false;
	}

	for (auto& frame : VideoFrames)
		Release(frame);
	for (auto& frame : VideoFrames)
	{
		// Get width and height from display mode
		long width, height;
		{
			IDeckLinkDisplayMode *displayModeInterface = nullptr;
			res = Output->GetDisplayMode(displayMode, &displayModeInterface);
			if (FAILED(res))
			{
				nosEngine.LogE("DeckLinkDevice: Failed to get display mode for device: %s", ModelName.c_str());
				return false;
			}
			width = displayModeInterface->GetWidth();
			height = displayModeInterface->GetHeight();
			res = displayModeInterface->GetFrameRate(&FrameDuration, &TimeScale);
			if (FAILED(res))
			{
				nosEngine.LogE("DeckLinkDevice: Failed to get frame rate for device: %s", ModelName.c_str());
				return false;
			}
			Release(displayModeInterface);
		}
		Output->CreateVideoFrame(width, height, width * 2, pixelFormat, bmdFrameFlagDefault, &frame);
		if (!frame)
		{
			nosEngine.LogE("DeckLinkDevice: Failed to create video frame for device: %s", ModelName.c_str());
			return false;
		}
	}

	auto outputCallback = new OutputCallback(this);
	if (outputCallback == nullptr)
		nosEngine.LogE("Could not create output callback");
	res = Output->SetScheduledFrameCompletionCallback(outputCallback);
	if (FAILED(res))
		nosEngine.LogE("DeckLinkDevice: Failed to set output callback for device: %s", ModelName.c_str());

	res = Output->StartScheduledPlayback(FrameDuration, TimeScale, 1.0);
	if (FAILED(res))
		nosEngine.LogE("DeckLinkDevice: Failed to start scheduled playback for device: %s", ModelName.c_str());

	for (size_t i = 0; i < VideoFrames.size(); ++i)
		ScheduleNextFrame();
	return true;
}

bool SubDevice::WaitFrameCompletion()
{
	std::unique_lock lock(VideoFramesMutex);
	return FrameCompletionCondition.wait_for(lock, std::chrono::seconds(1), [this]()
	{
		return !CompletedFramesQueue.empty();
	});
}

void SubDevice::ScheduleNextFrame()
{
	IDeckLinkVideoFrame* frameToSchedule = VideoFrames[NextFrameToSchedule];
	NextFrameToSchedule = ++NextFrameToSchedule % VideoFrames.size();

	HRESULT result = Output->ScheduleVideoFrame(frameToSchedule, TotalFramesScheduled * FrameDuration, FrameDuration, TimeScale);
	if (result != S_OK)
	{
		nosEngine.LogE("DeckLinkDevice: Failed to schedule video frame for device: %s", ModelName.c_str());
	}

	++TotalFramesScheduled;
}

void SubDevice::DmaWrite(void* buffer, size_t size)
{
	util::Stopwatch sw;
	void* frameBytes = nullptr;
	{
		std::unique_lock lock (VideoFramesMutex);
		if (CompletedFramesQueue.empty())
			return;
		auto frameBuffer = CompletedFramesQueue.front();
		frameBuffer->GetBytes(&frameBytes);
		if (frameBytes && buffer)
		{
			std::memcpy(frameBytes, buffer, size);
		}
		CompletedFramesQueue.pop_front();
	}
	nosEngine.WatchLog(("DeckLink " + Handle + " DMA Time").c_str(), sw.ElapsedString().c_str());
}

nosVec2u SubDevice::GetDeltaSeconds() const
{
	return nosVec2u{ (uint32_t)FrameDuration, (uint32_t)TimeScale };
}
}
