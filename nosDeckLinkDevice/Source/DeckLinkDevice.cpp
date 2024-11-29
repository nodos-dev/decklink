// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#include "DeckLinkDevice.hpp"

// Nodos
#include <Nodos/Modules.h>
#include <nosUtil/Stopwatch.hpp>

#include "ChannelMapping.inl"
#include "EnumConversions.hpp"

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

SubDevice::SubDevice(IDeckLink* deviceInterface)
	: Device(deviceInterface)
{
	dlstring_t modelName;
	Device->GetModelName(&modelName);
	ModelName = DlToStdString(modelName);
	nosEngine.LogI("DeckLink Device created: %s", ModelName.c_str());
	DeleteString(modelName);

	auto res = Device->QueryInterface(IID_IDeckLinkInput, (void**)&Input.Interface);
	if (res != S_OK)
		nosEngine.LogE("DeckLinkDevice: Failed to get input interface for device: %s", ModelName.c_str());
	res = Device->QueryInterface(IID_IDeckLinkOutput, (void**)&Output.Interface);
	if (res != S_OK)
		nosEngine.LogE("DeckLinkDevice: Failed to get output interface for device: %s", ModelName.c_str());

	res = Device->QueryInterface(IID_IDeckLinkProfileAttributes, (void**)&ProfileAttributes);
	if (res != S_OK || !ProfileAttributes)
	{
		nosEngine.LogE("DeckLinkDevice: Failed to get profile attributes for device: %s", ModelName.c_str());
		return;
	}

	BOOL supported{};
	res = ProfileAttributes->GetFlag(BMDDeckLinkSupportsInputFormatDetection, &supported);
	if ((res != S_OK) || (supported == false))
	{
		nosEngine.LogW("DeckLinkDevice: Device does not support automatic mode detection for device: %s", ModelName.c_str());
		return;
	}

	res = ProfileAttributes->GetInt(BMDDeckLinkSubDeviceIndex, &SubDeviceIndex);
	if (res != S_OK)
		nosEngine.LogE("DeckLinkDevice: Failed to get subdevice index for device: %s", ModelName.c_str());

	dlstring_t handle;
	res = ProfileAttributes->GetString(BMDDeckLinkDeviceHandle, &handle);
	if (res != S_OK)
		nosEngine.LogE("DeckLinkDevice: Failed to get device handle for device: %s", ModelName.c_str());
	if (handle)
	{
		Handle = DlToStdString(handle);
		DeleteString(handle);
	}
	res = ProfileAttributes->GetInt(BMDDeckLinkProfileID, &ProfileId);
	if (res != S_OK)
		nosEngine.LogE("DeckLinkDevice: Failed to get profile ID for device: %s", ModelName.c_str());

	res = ProfileAttributes->GetInt(BMDDeckLinkDeviceGroupID, &DeviceGroupId);
	if (res != S_OK)
		nosEngine.LogE("DeckLinkDevice: Failed to get device group ID for device: %s", ModelName.c_str());

	res = ProfileAttributes->GetInt(BMDDeckLinkPersistentID, &PersistentId);
	if (res != S_OK)
		nosEngine.LogE("DeckLinkDevice: Failed to get persistent ID for device: %s", ModelName.c_str());

	res = ProfileAttributes->GetInt(BMDDeckLinkTopologicalID, &TopologicalId);
	if (res != S_OK)
		nosEngine.LogE("DeckLinkDevice: Failed to get topological ID for device: %s", ModelName.c_str());

	res = Device->QueryInterface(IID_IDeckLinkProfileManager, (void**)&ProfileManager);
	if (res != S_OK || !ProfileManager)
	{
		nosEngine.LogE("DeckLinkDevice: Failed to get profile manager for device: %s", ModelName.c_str());
		return;
	}
}

SubDevice::~SubDevice()
{
	Release(Device);
	Release(ProfileAttributes);
	Release(ProfileManager);
}

bool SubDevice::IsBusyWith(nosMediaIODirection mode)
{
	switch (mode)
	{
	case NOS_MEDIAIO_DIRECTION_INPUT:
		return Input.IsActive;
	case NOS_MEDIAIO_DIRECTION_OUTPUT:
		return Output.IsActive;
	}
	return false;
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
	if (res != S_OK)
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
	return Output.Open(displayMode, pixelFormat);
}

bool SubDevice::CloseOutput()
{
	if (!Output)
	{
		nosEngine.LogE("SubDevice: Output interface is not available for device: %s", ModelName.c_str());
		return false;
	}
	return Output.Close();
}

bool SubDevice::OpenInput(BMDPixelFormat pixelFormat)
{
	if (!Input)
	{
		nosEngine.LogE("SubDevice: Input interface is not available for device: %s", ModelName.c_str());
		return false;
	}
	return Input.Open(bmdModeHD1080p5994, pixelFormat); // Display mode will be auto-detected
}

bool SubDevice::CloseInput()
{
	if (!Input)
	{
		nosEngine.LogE("SubDevice: Input interface is not available for device: %s", ModelName.c_str());
		return false;
	}
	return Input.Close();
}

constexpr IOHandlerBaseI& SubDevice::GetIO(nosMediaIODirection dir)
{
	if (dir == NOS_MEDIAIO_DIRECTION_INPUT)
		return Input;
	return Output;
}

bool SubDevice::WaitFrame(nosMediaIODirection dir, std::chrono::milliseconds timeout)
{
	return GetIO(dir).WaitFrame(timeout);
}

void SubDevice::DmaTransfer(nosMediaIODirection dir, void* buffer, size_t size)
{
	GetIO(dir).DmaTransfer(buffer, size);
}

std::optional<nosVec2u> SubDevice::GetDeltaSeconds(nosMediaIODirection dir)
{
	return GetIO(dir).GetDeltaSeconds();
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
	if (res != S_OK)
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
	if (res != S_OK || !profileIterator)
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

bool Device::OpenOutput(nosDeckLinkChannel channel, BMDDisplayMode displayMode, BMDPixelFormat pixelFormat)
{
	auto subDevice = GetSubDeviceOfChannel(NOS_MEDIAIO_DIRECTION_OUTPUT, channel);
	if (!subDevice)
	{
		nosEngine.LogE("No sub-device found for channel %s", GetChannelName(channel));
		return false;
	}
	if (subDevice->OpenOutput(displayMode, pixelFormat))
	{
		OpenChannels[channel] = { subDevice, NOS_MEDIAIO_DIRECTION_OUTPUT };
		return true;
	}
	return false;
}

bool Device::CloseChannel(nosDeckLinkChannel channel)
{
	auto it = OpenChannels.find(channel);
	if (it == OpenChannels.end())
	{
		nosEngine.LogE("No open channel found for channel %s", GetChannelName(channel));
		return false;
	}
	auto [subDevice, mode] = it->second;
	if (mode == NOS_MEDIAIO_DIRECTION_INPUT)
	{
		if (!subDevice->CloseInput())
			return false;
	}
	else
	{
		if (!subDevice->CloseOutput())
			return false;
	}
	OpenChannels.erase(it);
	return true;
}

std::optional<nosVec2u> Device::GetCurrentDeltaSecondsOfChannel(nosDeckLinkChannel channel)
{
	auto it = OpenChannels.find(channel);
	if (it == OpenChannels.end())
	{
		nosEngine.LogE("No open channel found for channel %s", GetChannelName(channel));
		return std::nullopt;
	}
	auto [subDevice, mode] = it->second;
	return subDevice->GetDeltaSeconds(mode);
}
	
bool Device::WaitFrame(nosDeckLinkChannel channel, std::chrono::milliseconds timeout)
{
	auto it = OpenChannels.find(channel);
	if (it == OpenChannels.end())
	{
		nosEngine.LogE("No open channel found for channel %s", GetChannelName(channel));
		return false;
	}
	auto [subDevice, mode] = it->second;
	return subDevice->WaitFrame(mode, timeout);
}

bool Device::DmaTransfer(nosDeckLinkChannel channel, void* buffer, size_t size)
{
	auto it = OpenChannels.find(channel);
	if (it == OpenChannels.end())
	{
		nosEngine.LogE("No open channel found for channel %s", GetChannelName(channel));
		return false;
	}
	auto [subDevice, mode] = it->second;
	subDevice->DmaTransfer(mode, buffer, size);
	return true;
}

bool Device::OpenInput(nosDeckLinkChannel channel, BMDPixelFormat pixelFormat)
{
	auto subDevice = GetSubDeviceOfChannel(NOS_MEDIAIO_DIRECTION_INPUT, channel);
	if (!subDevice)
	{
		nosEngine.LogE("No sub-device found for channel %s", GetChannelName(channel));
		return false;
	}
	if (subDevice->OpenInput(pixelFormat))
	{
		OpenChannels[channel] = { subDevice, NOS_MEDIAIO_DIRECTION_INPUT };
		return true;
	}
	return false;
}

void Device::ClearSubDevices()
{
	SubDevices.clear();
}
}