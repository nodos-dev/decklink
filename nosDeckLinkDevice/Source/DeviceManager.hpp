// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#pragma once

#include <vector>
#include <memory>

namespace nos::decklink
{
class Device;
class DeviceManager
{
public:
    DeviceManager();
    ~DeviceManager();
    void ClearDeviceList();
    Device* GetDevice(int64_t groupId);
    Device* GetDevice(uint32_t deviceIndex);
    const std::vector<std::unique_ptr<Device>>& GetDevices() { return Devices; }
protected:
    std::vector<std::unique_ptr<Device>> Devices;

};
}