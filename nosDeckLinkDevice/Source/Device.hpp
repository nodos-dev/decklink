// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#pragma once

#include <unordered_map>
#include <string>
#include <memory>
#include <mutex>

#include "Common.hpp"
#include "SubDevice.hpp"

// Nodos
#include <Nodos/Types.h>
#include <Nodos/Modules.h>

#include "nosDeckLinkDevice/nosDeckLinkDevice.h"

namespace nos::decklink
{

std::vector<std::unique_ptr<class Device>> InitializeDevices();

class Device
{
public:
	Device(uint32_t index, std::vector<std::unique_ptr<SubDevice>>&& subDevices);

	void Reinit(uint32_t groupId);

	std::string GetUniqueDisplayName() const;

	std::vector<nosDeckLinkChannel> GetAvailableChannels(nosMediaIODirection mode);

	bool CanOpenChannel(nosMediaIODirection dir, nosDeckLinkChannel channel, SubDevice** outSubDevice = nullptr) const;

	IDeckLinkProfileManager* GetProfileManager() const;

	std::optional<BMDProfileID> GetActiveProfile() const;
	
	/// Once used, this device should be recreated.
	void UpdateProfile(BMDProfileID profileId);

	SubDevice* GetSubDeviceOfChannel(nosMediaIODirection dir, nosDeckLinkChannel channel) const;
	SubDevice* GetSubDevice(int64_t index) const;

	// Channels
	bool OpenOutput(nosDeckLinkChannel channel, BMDDisplayMode displayMode, BMDPixelFormat pixelFormat);
	bool OpenInput(nosDeckLinkChannel channel, BMDPixelFormat pixelFormat);
	bool CloseChannel(nosDeckLinkChannel channel);
	std::optional<nosVec2u> GetCurrentDeltaSecondsOfChannel(nosDeckLinkChannel channel);

	bool WaitFrame(nosDeckLinkChannel channel, std::chrono::milliseconds timeout);
	bool DmaTransfer(nosDeckLinkChannel channel, void* buffer, size_t size);

	void ClearSubDevices();

	uint32_t Index = -1;
	int64_t GroupId = -1;
	std::string ModelName;
protected:

	std::vector<std::unique_ptr<SubDevice>> SubDevices;
	std::unordered_map<nosMediaIODirection, std::unordered_map<nosDeckLinkChannel, SubDevice*>> Channel2SubDevice;
	std::unordered_map<nosDeckLinkChannel, std::pair<SubDevice*, nosMediaIODirection>> OpenChannels;
};
	
}
