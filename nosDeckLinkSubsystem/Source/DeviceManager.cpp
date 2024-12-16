// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#include "DeviceManager.hpp"

#include "ChannelMapping.inl"
#include "Device.hpp"

namespace nos::decklink
{

const char* NOSAPI_CALL GetChannelName(nosDeckLinkChannel channel);
nosDeckLinkChannel NOSAPI_CALL GetChannelFromName(const char* channelName);

DeviceManager::DeviceManager()
{
}

DeviceManager::~DeviceManager()
{
	ClearDeviceList();
}

void DeviceManager::LoadDefaultSettings()
{
	Settings = {};
}

void DeviceManager::LoadSettings(sys::decklink::Settings const& settings)
{
	settings.UnPackTo(&Settings);
	if (!ValidatePortMappings())
	{
		nosEngine.LogE("DeviceManager: Invalid port mappings in settings. Loading default settings.");
		LoadDefaultSettings();
	}
}

bool DeviceManager::ValidatePortMappings()
{
	bool success = true;
	char errorMsg[256];
	for (auto& portMappingSetting : Settings.sdi_port_mappings)
	{
		auto& modelName = portMappingSetting->model_name;
		if (modelName.empty())
		{
			std::snprintf(errorMsg, sizeof(errorMsg), "Empty model name in port mappings.");
			success = false;
			break;
		}
		auto it = GetSDIPortCounts().find(modelName);
		if (it == GetSDIPortCounts().end())
		{
			std::snprintf(errorMsg, sizeof(errorMsg), "Device model not supported: %s", modelName.c_str());
			success = false;
			break;
		}
		auto portCount = it->second;
		auto& portMapping = portMappingSetting->sdi_port_mapping;
		std::unordered_set<int> targetPorts, sourcePorts;
		for (auto& entry : portMapping)
		{
			auto src = entry.source_port();
			auto dst = entry.target_port();
			if (src < 1 || src > portCount || dst < 1 || dst > portCount)
			{
				std::snprintf(errorMsg, sizeof(errorMsg), "Invalid port mapping for device %s: %d > %d", modelName.c_str(), src, dst);
				success = false;
				break;
			}
			if (targetPorts.find(dst) != targetPorts.end() || sourcePorts.find(src) != sourcePorts.end())
			{
				std::snprintf(errorMsg, sizeof(errorMsg), "Duplicate source or target port in port mapping for device %s: %d > %d", modelName.c_str(), src, dst);
				success = false;
				break;
			}
			targetPorts.insert(dst);
			sourcePorts.insert(src);
		}
		if (!success)
			break;
	}
	if (!success)
	{
		nosEngine.LogE("DeviceManager: %s", errorMsg);
		char completeMsg[256];
		std::snprintf(completeMsg, sizeof(completeMsg), "%s. Using default settings.", errorMsg);
		nosModuleStatusMessage msg {
			.ModuleId = nosEngine.Module->Id,
			.UpdateType = NOS_MODULE_STATUS_MESSAGE_UPDATE_TYPE_APPEND,
			.MessageType = NOS_MODULE_STATUS_MESSAGE_TYPE_ERROR,
			.Message = completeMsg
		};
		nosEngine.SendModuleStatusMessageUpdate(&msg);
	}
	return success;
}

void DeviceManager::InitializeDeviceList()
{
	Devices = CreateDevices();
	for (auto& device : Devices)
		DeviceMutexes[device->Index] = std::make_unique<std::shared_mutex>();
}

std::string SimultaneousReplace(std::string_view input, const std::map<std::string, std::string>& transformations)
{
	// A vector to store matches (start position, end position, replacement)
	std::vector<std::tuple<size_t, size_t, std::string>> matches;

	// Collect all matches from the input string for all transformations
	for (const auto& [oldStr, newStr] : transformations) {
		size_t start = 0;
		while ((start = input.find(oldStr, start)) != std::string::npos) {
			matches.emplace_back(start, start + oldStr.length(), newStr);
			start += oldStr.length(); // Move past the current match
		}
	}

	// Sort matches by start position (to handle multiple replacements in order)
	std::sort(matches.begin(), matches.end(), [](const auto& a, const auto& b) {
		return std::get<0>(a) < std::get<0>(b); // Compare start positions
	});

	// Rebuild the string with simultaneous replacements
	std::string result;
	size_t lastPos = 0;

	for (const auto& [start, end, replacement] : matches) {
		if (start >= lastPos) { // Ensure no overlap
			result.append(input.substr(lastPos, start - lastPos)); // Append unchanged part
			result.append(replacement); // Append replacement
			lastPos = end; // Update the last processed position
		}
	}

	// Append any remaining part of the original string
	result.append(input.substr(lastPos));

	return result;
}

std::optional<std::string> DeviceManager::GetPortMappedChannelName(uint32_t deviceIndex, nosDeckLinkChannel channel)
{
	std::string modelName;
	{
		DeviceLock lock(deviceIndex);
		auto* device = GetDevice(deviceIndex);
		if (!device)
		{
			nosEngine.LogE("DeviceManager: No such device with index %d", deviceIndex);
			return std::nullopt;
		}
		modelName = device->ModelName;
	}
	std::string originalChannelName = GetChannelName(channel);
	std::map<std::string, std::string> transformations;
	for (auto& portMappingSetting : Settings.sdi_port_mappings)
	{
		if (portMappingSetting->model_name != modelName)
			continue;
		for (auto& entry : portMappingSetting->sdi_port_mapping)
		{
			std::string sourcePortStr = std::to_string(entry.source_port());
			std::string targetPortStr = std::to_string(entry.target_port());
			transformations[sourcePortStr] = targetPortStr;
		}
	}
	return SimultaneousReplace(originalChannelName, transformations);
}

nosDeckLinkChannel DeviceManager::GetChannelFromPortMappedName(uint32_t deviceIndex, std::string_view portMappedName)
{
	std::string modelName;
	{
		DeviceLock lock(deviceIndex);
		auto* device = GetDevice(deviceIndex);
		if (!device)
		{
			nosEngine.LogE("DeviceManager: No such device with index %d", deviceIndex);
			return NOS_DECKLINK_CHANNEL_INVALID;
		}
		modelName = device->ModelName;
	}
	std::map<std::string, std::string> transformations;
	for (auto& portMappingSetting : Settings.sdi_port_mappings)
	{
		if (portMappingSetting->model_name != modelName)
			continue;
		for (auto& entry : portMappingSetting->sdi_port_mapping)
		{
			std::string sourcePortStr = std::to_string(entry.source_port());
			std::string targetPortStr = std::to_string(entry.target_port());
			transformations[targetPortStr] = sourcePortStr; // reverse
		}
	}
	auto originalName = SimultaneousReplace(portMappedName, transformations);
	return GetChannelFromName(originalName.c_str());
}

void DeviceManager::ClearDeviceList()
{
	for (auto it = Devices.begin(); it != Devices.end(); ++it)
	{
		auto device = it->get();
		IDeckLink* dlDevice = nullptr;
		if (auto mainSubDevice = device->GetSubDevice(0))
			dlDevice = mainSubDevice->DLDevice;
		auto index = it->get()->Index;
		LockDevice(index, false);
		it->reset();
		if (dlDevice)
			Release(dlDevice);
		UnlockDevice(index, false);
	}
	Devices.clear();
}

Device* DeviceManager::GetDevice(int64_t groupId)
{
	for (auto& device : Devices)
	{
		if (device->GroupId == groupId)
			return device.get();
	}
	return nullptr;
}

Device* DeviceManager::GetDevice(uint32_t deviceIndex)
{
	if (deviceIndex < Devices.size())
		return Devices[deviceIndex].get();
	return nullptr;
}

void DeviceManager::LockDevice(uint32_t deviceIndex, bool shared)
{
	auto it = DeviceMutexes.find(deviceIndex);
	if (it == DeviceMutexes.end())
		return;
	if (shared)
		it->second->lock_shared();
	else
		it->second->lock();
}

void DeviceManager::UnlockDevice(uint32_t deviceIndex, bool shared)
{
	auto it = DeviceMutexes.find(deviceIndex);
	if (it == DeviceMutexes.end())
		return;
	if (shared)
		it->second->unlock_shared();
	else
		it->second->unlock();
}

DeviceManager* DeviceManager::Instance()
{
	if (!SingleInstance)
		SingleInstance = new DeviceManager;
	return SingleInstance;
}

void DeviceManager::Destroy()
{
	delete SingleInstance;
	SingleInstance = nullptr;
}

DeviceManager* DeviceManager::SingleInstance = nullptr;
}
