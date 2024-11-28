// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#pragma once

#include <unordered_map>
#include <string>
#include <memory>
#include <mutex>

// DeckLink
#if _WIN32
#include <comdef.h>
#endif
#include <DeckLinkAPI.h>

// Nodos
#include <Nodos/Types.h>

#include "nosDeckLinkDevice/nosDeckLinkDevice.h"

namespace nos::decklink
{
std::vector<std::unique_ptr<class Device>> InitializeDevices();

class SubDevice
{
public:
	friend class OutputCallback;
	SubDevice(IDeckLink* deviceInterface);
	~SubDevice();
	bool IsBusyWith(nosMediaIODirection mode);
	std::map<nosMediaIOFrameGeometry, std::set<nosMediaIOFrameRate>> GetSupportedOutputFrameGeometryAndFrameRates(std::unordered_set<nosMediaIOPixelFormat> const& pixelFormats);

	std::string ModelName;
	int64_t SubDeviceIndex = -1;
	int64_t PersistentId = -1;
	int64_t ProfileId = -1;
	int64_t DeviceGroupId = -1;
	int64_t TopologicalId = -1;
	std::string Handle;
	std::vector<std::string> ProfileNames;

	// Output
	bool DoesSupportOutputVideoMode(BMDDisplayMode displayMode, BMDPixelFormat pixelFormat);
	bool OpenOutput(BMDDisplayMode displayMode, BMDPixelFormat pixelFormat);
	bool CloseOutput();
	bool WaitFrame(std::chrono::milliseconds timeout);
	void ScheduleNextFrame();
	void DmaWrite(const void* buffer, size_t size);
	nosVec2u GetDeltaSeconds() const;
	
	bool OpenInput(BMDPixelFormat pixelFormat);
	bool CloseInput();

	IDeckLinkProfileManager* ProfileManager = nullptr;
protected:
	IDeckLink* Device = nullptr;
	IDeckLinkInput* Input = nullptr;
	IDeckLinkOutput* Output = nullptr;
	IDeckLinkProfileAttributes* ProfileAttributes = nullptr;
	BMDTimeValue FrameDuration = 0;
	BMDTimeScale TimeScale = 0;
	
	std::array<IDeckLinkMutableVideoFrame*, 4> VideoFrames{};
	std::mutex VideoFramesMutex;
	std::condition_variable FrameCompletionCondition;
	std::deque<IDeckLinkVideoFrame*> CompletedFramesQueue;

	uint32_t TotalFramesScheduled = 0;
	uint32_t NextFrameToSchedule = 0;

	std::unordered_map<nosMediaIODirection, bool> ActiveModes;
};

class Device
{
public:
	Device(uint32_t index, std::vector<std::unique_ptr<SubDevice>>&& subDevices);

	void Reinit(uint32_t groupId);

	std::string GetUniqueDisplayName() const;

	std::vector<nosDeckLinkChannel> GetAvailableChannels(nosMediaIODirection mode);

	bool CanOpenChannel(nosMediaIODirection dir, nosDeckLinkChannel channel, SubDevice** outSubDevice = nullptr) const;

	bool OpenOutputChannel(nosDeckLinkChannel channel,
						   BMDDisplayMode displayMode,
						   BMDPixelFormat pixelFormat,
						   SubDevice** outSubDevice);

	SubDevice* GetSubDeviceOfChannel(nosMediaIODirection dir, nosDeckLinkChannel channel) const;

	SubDevice* GetSubDevice(int64_t index) const;

	IDeckLinkProfileManager* GetProfileManager() const;

	std::optional<BMDProfileID> GetActiveProfile() const;
	
	/// Once used, this device should be recreated.
	void UpdateProfile(BMDProfileID profileId);

	void ClearSubDevices();

	uint32_t Index = -1;
	int64_t GroupId = -1;
	std::string ModelName;
protected:
	std::vector<std::unique_ptr<SubDevice>> SubDevices;
	std::unordered_map<nosMediaIODirection, std::unordered_map<nosDeckLinkChannel, SubDevice*>> Channel2SubDevice;
};
	
}
