// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include <shared_mutex>
#include <Nodos/Modules.h>

#include "DeckLink_generated.h"
#include "nosDeckLinkSubsystem/nosDeckLinkSubsystem.h"

namespace nos::decklink
{
class Device;

class DeviceManager
{
public:
	void ClearDeviceList();
	Device* GetDevice(int64_t groupId);
	Device* GetDevice(uint32_t deviceIndex);
	const std::vector<std::unique_ptr<Device>>& GetDevices() { return Devices; }
	void LockDevice(uint32_t deviceIndex, bool shared = true);
	void UnlockDevice(uint32_t deviceIndex, bool shared = true);
	static DeviceManager* Instance();
	static void Destroy();
	~DeviceManager();
	sys::decklink::TSettings Settings;
	void LoadDefaultSettings();
	void LoadSettings(sys::decklink::Settings const& settings);
	bool ValidatePortMappings();
	void InitializeDeviceList();
	std::optional<std::string> GetPortMappedChannelName(uint32_t deviceIndex, nosDeckLinkChannel channel);
	nosDeckLinkChannel GetChannelFromPortMappedName(uint32_t deviceIndex, std::string_view portMappedName);
protected:
	std::unordered_map<uint32_t, std::unique_ptr<std::shared_mutex>> DeviceMutexes;
	std::vector<std::unique_ptr<Device>> Devices;
private:
	DeviceManager();
	static DeviceManager* SingleInstance;
};

struct DeviceLock
{
	DeviceLock(uint32_t deviceIndex, bool shared = true)
		: DeviceIndex(deviceIndex), SharedLock(shared)
	{
		DeviceManager::Instance()->LockDevice(deviceIndex, SharedLock);
	}
	~DeviceLock()
	{
		DeviceManager::Instance()->UnlockDevice(DeviceIndex, SharedLock);
	}

	DeviceLock(const DeviceLock&) = delete;
	DeviceLock& operator=(const DeviceLock&) = delete;
	DeviceLock(DeviceLock&&) = delete;
protected:
	uint32_t DeviceIndex;
	bool SharedLock;
};
}
