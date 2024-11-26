// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#include "DeckLinkDevice.hpp"

// Nodos
#include <Nodos/Modules.h>
#include <nosUtil/Stopwatch.hpp>

#include "ChannelMapping.inl"

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

class OutputCallback : public IDeckLinkVideoOutputCallback
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
	{
		std::unique_lock lock(Device->VideoFramesMutex);
		Device->CompletedFramesQueue.push_back(completedFrame);
	}
	Device->FrameCompletionCondition.notify_one();
	if (result != bmdOutputFrameCompleted)
	{
		const char* resultStr = nullptr;
		switch (result) {
		case bmdOutputFrameDropped: resultStr = "'Dropped'"; break;
		case bmdOutputFrameDisplayedLate: resultStr = "'DisplayedLate'"; break;
		case bmdOutputFrameFlushed: resultStr = "'Flushed'"; break;
		}
		nosEngine.LogW("DeckLink %s: A frame completed with %s", Device->Handle.c_str(), resultStr);
	}

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
	if (FAILED(res) || !ProfileAttributes)
	{
		nosEngine.LogE("DeckLinkDevice: Failed to get profile attributes for device: %s", ModelName.c_str());
		return;
	}

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

	res = ProfileAttributes->GetInt(BMDDeckLinkDeviceGroupID, &DeviceGroupId);
	if (FAILED(res))
		nosEngine.LogE("DeckLinkDevice: Failed to get device group ID for device: %s", ModelName.c_str());

	res = ProfileAttributes->GetInt(BMDDeckLinkPersistentID, &PersistentId);
	if (FAILED(res))
		nosEngine.LogE("DeckLinkDevice: Failed to get persistent ID for device: %s", ModelName.c_str());

	res = ProfileAttributes->GetInt(BMDDeckLinkTopologicalID, &TopologicalId);
	if (FAILED(res))
		nosEngine.LogE("DeckLinkDevice: Failed to get topological ID for device: %s", ModelName.c_str());

	res = Device->QueryInterface(IID_IDeckLinkProfileManager, (void**)&ProfileManager);
	if (FAILED(res) || !ProfileManager)
	{
		nosEngine.LogE("DeckLinkDevice: Failed to get profile manager for device: %s", ModelName.c_str());
		return;
	}
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

bool SubDevice::CanDoMode(nosDeckLinkMode mode)
{
	return !ActiveModes[mode];
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

	ActiveModes[NOS_DECK_LINK_MODE_OUTPUT] = true;

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

	res = Output->StartScheduledPlayback(0, TimeScale, 1.0);
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
		nosEngine.LogE("Failed to schedule video frame for device: %s", ModelName.c_str());
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
		{
			nosEngine.LogW("%s: No frame to write", Handle.c_str());
			return;
		}
		auto frameBuffer = CompletedFramesQueue.front();
		frameBuffer->GetBytes(&frameBytes);
		CompletedFramesQueue.pop_front();
	}
	if (frameBytes && buffer)
	{
		std::memcpy(frameBytes, buffer, size);
	}
	nosEngine.WatchLog(("DeckLink " + Handle + " DMA Time").c_str(), sw.ElapsedString().c_str());
}

nosVec2u SubDevice::GetDeltaSeconds() const
{
	return nosVec2u{ (uint32_t)FrameDuration, (uint32_t)TimeScale };
}

Device::Device(uint32_t index, std::vector<std::unique_ptr<SubDevice>>&& subDevices)
	: Index(index), SubDevices(std::move(subDevices))
{
	if (SubDevices.empty())
		nosEngine.LogE("No sub-device provided for device index: %d", index);
	else
	{
		ModelName = SubDevices[0]->ModelName;
		GroupId = SubDevices[0]->DeviceGroupId;
	}
}

bool Device::InitializeDeviceList()
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
	std::vector<std::unique_ptr<SubDevice>> subDevices;
	while (deckLinkIterator->Next(&deckLink) == S_OK)
	{
		auto bmDevice = std::make_unique<SubDevice>(deckLink);
		subDevices.push_back(std::move(bmDevice));
		deviceNumber++;
	}

	if (deckLinkIterator)
		deckLinkIterator->Release();

	std::unordered_map<int64_t, std::vector<std::unique_ptr<SubDevice>>> subDevicePerDevice;
	for (auto& subDevice : subDevices)
		subDevicePerDevice[subDevice->DeviceGroupId].push_back(std::move(subDevice));

	uint32_t deviceIndex = 0;
	for (auto& [groupId, subDevices] : subDevicePerDevice)
	{
		Devices.push_back(std::make_unique<Device>(deviceIndex, std::move(subDevices)));
		++deviceIndex;
	}
	
	return true;
}

void Device::ClearDeviceList()
{
	Devices.clear();
}

Device* Device::GetDevice(int64_t groupId)
{
	for (auto& device : Devices)
	{
		if (device->GroupId == groupId)
			return device.get();
	}
	return nullptr;
}

Device* Device::GetDevice(uint32_t deviceIndex)
{
	if (deviceIndex < Devices.size())
		return Devices[deviceIndex].get();
	return nullptr;
}

std::string Device::GetUniqueDisplayName() const
{
	return ModelName + " - " + std::to_string(Index);
}

std::vector<nosDeckLinkChannel> Device::GetAvailableChannels(nosDeckLinkMode mode)
{
	std::vector<nosDeckLinkChannel> channels;
	static std::array allChannels {
		NOS_DECK_LINK_CHANNEL_SINGLE_LINK_1,
		NOS_DECK_LINK_CHANNEL_SINGLE_LINK_2,
		NOS_DECK_LINK_CHANNEL_SINGLE_LINK_3,
		NOS_DECK_LINK_CHANNEL_SINGLE_LINK_4,
		NOS_DECK_LINK_CHANNEL_SINGLE_LINK_5,
		NOS_DECK_LINK_CHANNEL_SINGLE_LINK_6,
		NOS_DECK_LINK_CHANNEL_SINGLE_LINK_7,
		NOS_DECK_LINK_CHANNEL_SINGLE_LINK_8
	};
	for (auto& channel : allChannels)
	{
		if (CanOpenChannel(mode, channel))
			channels.push_back(channel);
	}
	return channels;
}

bool Device::CanOpenChannel(nosDeckLinkMode mode, nosDeckLinkChannel channel) const
{
	// Determine which sub-devices are capable of opening the channel
	for (auto& [modelName, rest] : GetChannelMap())
	{
		if (modelName != ModelName)
			continue;
		for (auto& [profile, rest2] : rest)
		{
			if (profile != bmdProfileFourSubDevicesHalfDuplex)
				continue;
			for (auto& [subDeviceIndex, rest3] : rest2)
			{
				for (auto& [curChannel, modes] : rest3)
				{
					if (curChannel == channel && modes.contains(mode))
					{
						if (auto subDevice = GetSubDevice(subDeviceIndex))
						{
							if (subDevice->CanDoMode(mode))
								return true;
						}
						return true;
					}
				}
			}
		}
	}
	
	return false;
}

SubDevice* Device::GetSubDevice(int64_t index) const
{
	if (index < SubDevices.size())
		return SubDevices[index].get();
	return nullptr;
}
}
