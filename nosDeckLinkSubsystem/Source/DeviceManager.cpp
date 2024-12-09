// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#include "DeviceManager.hpp"

#include "Device.hpp"

namespace nos::decklink
{

DeviceManager::DeviceManager()
{
	Devices = InitializeDevices();
	for (auto& device : Devices)
		DeviceMutexes[device->Index] = std::make_unique<std::shared_mutex>();
}

DeviceManager::~DeviceManager()
{
	ClearDeviceList();
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
