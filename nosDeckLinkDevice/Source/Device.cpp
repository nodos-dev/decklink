// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#include "Device.hpp"

// Nodos
#include <Nodos/Modules.h>
#include <nosUtil/Stopwatch.hpp>

#include "ChannelMapping.inl"
#include "EnumConversions.hpp"
#include "SubDevice.hpp"

namespace nos::decklink
{

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

	for (auto& subDevice : SubDevices)
		subDevice->TagDevice(Index);
	
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
						subDevice->TagChannel(mode, curChannel);
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
	Release(profileChangeCallback);
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
			if (!subDevice->IsBusy())
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

std::pair<SubDevice*, nosMediaIODirection> Device::GetSubDeviceOfOpenChannel(nosDeckLinkChannel channel) const
{
	auto it = OpenChannels.find(channel);
	if (it != OpenChannels.end())
		return it->second;
	return {nullptr, {}};
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

bool Device::StartStream(nosDeckLinkChannel channel)
{
	auto it = OpenChannels.find(channel);
	if (it == OpenChannels.end())
	{
		nosEngine.LogE("No open channel found for channel %s", GetChannelName(channel));
		return false;
	}
	auto [subDevice, mode] = it->second;
	return subDevice->StartStream(mode);
}

bool Device::StopStream(nosDeckLinkChannel channel)
{
	auto it = OpenChannels.find(channel);
	if (it == OpenChannels.end())
	{
		nosEngine.LogE("No open channel found for channel %s", GetChannelName(channel));
		return false;
	}
	auto [subDevice, mode] = it->second;
	return subDevice->StopStream(mode);
}

void Device::ClearSubDevices()
{
	SubDevices.clear();
}
}