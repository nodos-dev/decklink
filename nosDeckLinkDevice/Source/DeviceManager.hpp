#pragma once

#include <vector>
#include <memory>

#include "DeckLinkDevice.hpp"

namespace nos::decklink
{
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