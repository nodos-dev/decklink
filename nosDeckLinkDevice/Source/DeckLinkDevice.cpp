// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#include "DeckLinkDevice.hpp"

// Nodos
#include <Nodos/Modules.h>
#include <nosUtil/Stopwatch.hpp>

#include "ChannelMapping.inl"
#include "EnumConversions.hpp"

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

HRESULT GetDeckLinkIterator(IDeckLinkIterator **deckLinkIterator)
{
	HRESULT result = S_OK;

	// Create an IDeckLinkIterator object to enumerate all DeckLink cards in the system
	result = CoCreateInstance(CLSID_CDeckLinkIterator, NULL, CLSCTX_ALL, IID_IDeckLinkIterator, (void**)deckLinkIterator);
	if (FAILED(result))
		nosEngine.LogE("A DeckLink iterator could not be created. The DeckLink drivers may not be installed.");

	return result;
}

std::vector<std::unique_ptr<Device>> InitializeDevices()
{
	IDeckLinkIterator* deckLinkIterator = nullptr;

	HRESULT result = E_FAIL;
#ifdef _WIN32
	result = CoInitialize(NULL);
	if (FAILED(result))
	{
		nosEngine.LogE("DeckLinkDevice: Initialization of COM failed with error: %s", _com_error(result).ErrorMessage());
		return {};
	}
#endif

	result = GetDeckLinkIterator(&deckLinkIterator);
	if (FAILED(result))
	{
		return {};
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

	std::vector<std::unique_ptr<Device>> devices;
	uint32_t deviceIndex = 0;
	for (auto& [groupId, subDevices] : subDevicePerDevice)
	{
		devices.push_back(std::make_unique<Device>(deviceIndex, std::move(subDevices)));
		++deviceIndex;
	}

	return devices;
}

template <typename T>
class Object : public T
{
public:
	Object() :
		RefCount(1)
	{
	}

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
	std::atomic<int32_t> RefCount;
};
	
class OutputCallback : public Object<IDeckLinkVideoOutputCallback>
{
public:
	OutputCallback(SubDevice* deckLinkDevice) :
		Device(deckLinkDevice)
	{
	}

	HRESULT	STDMETHODCALLTYPE ScheduledFrameCompleted(IDeckLinkVideoFrame* completedFrame, BMDOutputFrameCompletionResult result) override;
	HRESULT	STDMETHODCALLTYPE ScheduledPlaybackHasStopped(void) override;

	// IUnknown needs only a dummy implementation
	HRESULT	STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID *ppv) override
	{
		return E_NOINTERFACE;
	}

private:
	SubDevice*  Device;
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

bool SubDevice::IsBusyWith(nosMediaIODirection mode)
{
	return ActiveModes[mode];
}

std::map<nosMediaIOFrameGeometry, std::set<nosMediaIOFrameRate>> SubDevice::GetSupportedOutputFrameGeometryAndFrameRates(std::unordered_set<nosMediaIOPixelFormat> const& pixelFormats)
{
	std::map<nosMediaIOFrameGeometry, std::set<nosMediaIOFrameRate>> supported;
	if (!Output)
	{
		nosEngine.LogE("SubDevice: Output interface is not available for device: %s", ModelName.c_str());
		return supported;
	}

	for (auto& pixelFormat : pixelFormats)
	{
		for (int i = NOS_MEDIAIO_FG_MIN; i < NOS_MEDIAIO_FG_MAX; ++i)
		{
			auto fg = static_cast<nosMediaIOFrameGeometry>(i);
			for (auto& displayMode : GetDisplayModesForFrameGeometry(fg))
			{
				if (DoesSupportOutputVideoMode(displayMode, GetDeckLinkPixelFormat(pixelFormat)))
				{
					supported[fg].insert(GetFrameRateFromDisplayMode(displayMode));
					break;
				}
			}
		}
	}
	return supported;
}

bool SubDevice::DoesSupportOutputVideoMode(BMDDisplayMode displayMode, BMDPixelFormat pixelFormat)
{
	if (!Output)
		return false;
	BOOL supported{};
	BMDDisplayMode actualDisplayMode{};
	auto res = Output->DoesSupportVideoMode(bmdVideoConnectionSDI, displayMode, pixelFormat, bmdNoVideoOutputConversion, bmdSupportedVideoModeDefault, &actualDisplayMode, &supported);
	if (FAILED(res))
	{
		nosEngine.LogE("SubDevice: Failed to check video mode support for device: %s", ModelName.c_str());
		return false;
	}
	if (!supported && actualDisplayMode == displayMode)
	{
		// Supported but with conversion
		nosEngine.LogW("SubDevice: Video mode %d is supported but with conversion for device: %s", displayMode, ModelName.c_str());
		return true;
	}
	return supported;
}

bool SubDevice::OpenOutput(BMDDisplayMode displayMode, BMDPixelFormat pixelFormat)
{
	if (!Output) 
	{
		nosEngine.LogE("SubDevice: Output interface is not available for device: %s", ModelName.c_str());
		return false;
	}

	auto res = Output->EnableVideoOutput(displayMode, bmdVideoOutputFlagDefault);
	if (FAILED(res))
	{
		nosEngine.LogE("SubDevice: Failed to enable video output for device: %s", ModelName.c_str());
		return false;
	}

	ActiveModes[NOS_MEDIAIO_DIRECTION_OUTPUT] = true;

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
				nosEngine.LogE("SubDevice: Failed to get display mode for device: %s", ModelName.c_str());
				return false;
			}
			width = displayModeInterface->GetWidth();
			height = displayModeInterface->GetHeight();
			res = displayModeInterface->GetFrameRate(&FrameDuration, &TimeScale);
			if (FAILED(res))
			{
				nosEngine.LogE("SubDevice: Failed to get frame rate for device: %s", ModelName.c_str());
				return false;
			}
			Release(displayModeInterface);
		}
		Output->CreateVideoFrame(width, height, width * 2, pixelFormat, bmdFrameFlagDefault, &frame);
		if (!frame)
		{
			nosEngine.LogE("SubDevice: Failed to create video frame for device: %s", ModelName.c_str());
			return false;
		}
	}

	auto outputCallback = new OutputCallback(this);
	if (outputCallback == nullptr)
		nosEngine.LogE("Could not create output callback");
	res = Output->SetScheduledFrameCompletionCallback(outputCallback);
	if (FAILED(res))
		nosEngine.LogE("SubDevice: Failed to set output callback for device: %s", ModelName.c_str());

	res = Output->StartScheduledPlayback(0, TimeScale, 1.0);
	if (FAILED(res))
		nosEngine.LogE("SubDevice: Failed to start scheduled playback for device: %s", ModelName.c_str());

	for (size_t i = 0; i < VideoFrames.size(); ++i)
		ScheduleNextFrame();
	return true;
}

bool SubDevice::CloseOutput()
{
	if (!Output)
	{
		nosEngine.LogE("SubDevice: Output interface is not available for device: %s", ModelName.c_str());
		return false;
	}
	if (!ActiveModes[NOS_MEDIAIO_DIRECTION_OUTPUT])
	{
		nosEngine.LogE("SubDevice: Output is not active for device: %s", ModelName.c_str());
		return false;
	}

	if (Output->StopScheduledPlayback(0, nullptr, TimeScale) != S_OK)
	{
		nosEngine.LogE("Failed to stop scheduled playback for device: %s", ModelName.c_str());
	}

	auto res = Output->DisableVideoOutput();
	if (FAILED(res))
	{
		nosEngine.LogE("SubDevice: Failed to disable video output for device: %s", ModelName.c_str());
		return false;
	}
	return true;
}

bool SubDevice::OpenInput(BMDPixelFormat pixelFormat)
{
	nosEngine.LogE("Unimplemented");
	return false;
}

bool SubDevice::CloseInput()
{
	nosEngine.LogE("Unimplemented");
	return false;
}

bool SubDevice::WaitFrame(std::chrono::milliseconds timeout)
{
	std::unique_lock lock(VideoFramesMutex);
	return FrameCompletionCondition.wait_for(lock, timeout, [this]()
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

void SubDevice::DmaWrite(const void* buffer, size_t size)
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

class ProfileChangeCallback : public Object<IDeckLinkProfileCallback>
{
public:
	ProfileChangeCallback(Device* deckLinkDevice) :
		Device(deckLinkDevice)
	{
	}

	HRESULT	STDMETHODCALLTYPE ProfileChanging(IDeckLinkProfile* newProfile, dlbool_t streamsWillBeForcedToStop) override
	{
		Device->ClearSubDevices();
		return S_OK;
	}
	HRESULT	STDMETHODCALLTYPE ProfileActivated(IDeckLinkProfile* newProfile) override
	{
		auto groupId = Device->GroupId;
		Device->Reinit(groupId);
		return S_OK;
	}

	Device* Device;
};

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
	
	// Determine which sub-devices are capable of opening the channel
	auto& channelMap = GetChannelMap();
	auto modelIt = channelMap.find(ModelName);
	if (modelIt == channelMap.end())
	{
		nosEngine.LogE("No channel map found for device: %s", ModelName.c_str());
		return;
	}
	auto& mapping = modelIt->second;
	for (auto& [profile, rest2] : mapping)
	{
		if (profile != bmdProfileFourSubDevicesHalfDuplex)
			continue; // TODO.
		for (auto& [subDeviceIndex, rest3] : rest2)
		{
			for (auto& [curChannel, modes] : rest3)
			{
				if (auto subDevice = GetSubDevice(subDeviceIndex))
				{
					for (auto mode : modes)
					{
						Channel2SubDevice[mode][curChannel] = subDevice;
					}
				}
			}
		}
	}

	auto profileChangeCallback = new ProfileChangeCallback(this);
	if (profileChangeCallback == nullptr)
		nosEngine.LogE("Could not create profile change callback");
	else
		GetProfileManager()->SetCallback(profileChangeCallback);
}

void Device::Reinit(uint32_t groupId)
{
	ClearSubDevices();
	auto devices = InitializeDevices();
	for (auto& device : devices)
	{
		if (device->GroupId == groupId)
		{
			*this = std::move(*device);
			break;
		}
	}
}

std::string Device::GetUniqueDisplayName() const
{
	return ModelName + " - " + std::to_string(Index);
}

std::vector<nosDeckLinkChannel> Device::GetAvailableChannels(nosMediaIODirection mode)
{
	std::vector<nosDeckLinkChannel> channels;
	static std::array allChannels {
		NOS_DECKLINK_CHANNEL_SINGLE_LINK_1,
		NOS_DECKLINK_CHANNEL_SINGLE_LINK_2,
		NOS_DECKLINK_CHANNEL_SINGLE_LINK_3,
		NOS_DECKLINK_CHANNEL_SINGLE_LINK_4,
		NOS_DECKLINK_CHANNEL_SINGLE_LINK_5,
		NOS_DECKLINK_CHANNEL_SINGLE_LINK_6,
		NOS_DECKLINK_CHANNEL_SINGLE_LINK_7,
		NOS_DECKLINK_CHANNEL_SINGLE_LINK_8
	};
	for (auto& channel : allChannels)
	{
		if (CanOpenChannel(mode, channel))
			channels.push_back(channel);
	}
	return channels;
}

bool Device::CanOpenChannel(nosMediaIODirection dir, nosDeckLinkChannel channel, SubDevice** outSubDevice) const
{
	auto dit = Channel2SubDevice.find(dir);
	if (dit != Channel2SubDevice.end())
	{
		auto cit = dit->second.find(channel);
		if (cit != dit->second.end())
		{
			auto* subDevice = cit->second;
			if (!subDevice->IsBusyWith(dir))
			{
				if (outSubDevice)
					*outSubDevice = subDevice;
				return true;
			}
		}
	}
	return false;
}

bool Device::OpenOutputChannel(nosDeckLinkChannel channel,
							   BMDDisplayMode displayMode, BMDPixelFormat pixelFormat,
							   SubDevice** outSubDevice)
{
	SubDevice* subDevice = nullptr;
	if (!CanOpenChannel(NOS_MEDIAIO_DIRECTION_OUTPUT, channel, &subDevice))
		return false;
	if (outSubDevice)
		*outSubDevice = subDevice;
	if (!subDevice->OpenOutput(displayMode, pixelFormat))
		return false;
	return true;
}

SubDevice* Device::GetSubDeviceOfChannel(nosMediaIODirection dir, nosDeckLinkChannel channel) const
{
	auto dit = Channel2SubDevice.find(dir);
	if (dit != Channel2SubDevice.end())
	{
		auto cit = dit->second.find(channel);
		if (cit != dit->second.end())
			return cit->second;
	}
	return nullptr;
}

SubDevice* Device::GetSubDevice(int64_t index) const
{
	if (index < SubDevices.size())
		return SubDevices[index].get();
	return nullptr;
}

IDeckLinkProfileManager* Device::GetProfileManager() const
{
	if (SubDevices.empty())
		return nullptr;
	return SubDevices[0]->ProfileManager;
}

std::optional<BMDProfileID> Device::GetActiveProfile() const
{
	if (SubDevices.empty())
		return std::nullopt;
	auto& subDevice = SubDevices[0];
	IDeckLinkProfileIterator* profileIterator = nullptr;
	auto res = subDevice->ProfileManager->GetProfiles(&profileIterator);
	if (FAILED(res))
	{
		nosEngine.LogE("DeckLinkDevice: Failed to get profile iterator for device: %s", ModelName.c_str());
		return std::nullopt;
	}
	IDeckLinkProfile* profile = nullptr;
	while (profileIterator->Next(&profile) == S_OK)
	{
		BOOL active = false;
		if (profile->IsActive(&active) == S_OK && active)
		{
			IDeckLinkProfileAttributes* profileAttributes = nullptr;
			if (profile->QueryInterface(IID_IDeckLinkProfileAttributes, (void**)&profileAttributes) == S_OK)
			{
				int64_t profileId{};
				if (profileAttributes->GetInt(BMDDeckLinkProfileID, &profileId) == S_OK)
				{
					Release(profileAttributes);
					Release(profileIterator);
					return BMDProfileID(profileId);
				}
				Release(profileAttributes);
			}
		}
		Release(profile);
	}
	return std::nullopt;
}

void Device::UpdateProfile(BMDProfileID profileId)
{
	if (SubDevices.empty())
		return;
	auto& subDevice = SubDevices[0];
	IDeckLinkProfileIterator* profileIterator = nullptr;
	auto res = subDevice->ProfileManager->GetProfiles(&profileIterator);
	if (FAILED(res) || !profileIterator)
	{
		nosEngine.LogE("DeckLinkDevice: Failed to get profile iterator for device: %s", ModelName.c_str());
		return;
	}
	IDeckLinkProfile* profile = nullptr;
	while (profileIterator->Next(&profile) == S_OK)
	{
		IDeckLinkProfileAttributes* profileAttributes = nullptr;
		if (profile->QueryInterface(IID_IDeckLinkProfileAttributes, (void**)&profileAttributes) == S_OK)
		{
			int64_t profileId{};
			if (profileAttributes->GetInt(BMDDeckLinkProfileID, &profileId) == S_OK)
			{
				if (profileId == profileId)
				{
					if (profile->SetActive() != S_OK)
						nosEngine.LogE("DeckLinkDevice: Failed to set profile for device: %s", ModelName.c_str());
				Release(profileAttributes);
				Release(profileIterator);
			}
			Release(profileAttributes);
		}
		Release(profile);
	}
	Release(profileIterator);
}
}

void Device::ClearSubDevices()
{
	SubDevices.clear();
}
}