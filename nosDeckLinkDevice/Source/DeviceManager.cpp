// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#include "DeviceManager.hpp"

#include <Nodos/Modules.h>

#include "Device.hpp"

namespace nos::decklink
{

DeviceManager::DeviceManager()
{
    Devices = InitializeDevices();
	for (auto& device : Devices)
	{
		auto profileId = device->GetActiveProfile();
		if (profileId && *profileId != bmdProfileFourSubDevicesHalfDuplex)
		{
			device->UpdateProfile(bmdProfileFourSubDevicesHalfDuplex);
		}
	}
}

DeviceManager::~DeviceManager()
{
    ClearDeviceList();
}

void DeviceManager::ClearDeviceList()
{
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

}