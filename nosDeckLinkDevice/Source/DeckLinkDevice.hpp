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

class SubDevice
{
public:
	friend class OutputCallback;
	SubDevice(IDeckLink* deviceInterface);
	~SubDevice();
	bool CanDoMode(nosDeckLinkMode mode);

	std::string ModelName;
	int64_t SubDeviceIndex = -1;
	int64_t PersistentId = -1;
	int64_t ProfileId = -1;
	int64_t DeviceGroupId = -1;
	int64_t TopologicalId = -1;
	std::string Handle;
	std::vector<std::string> ProfileNames;
	bool OpenOutput(BMDDisplayMode displayMode, BMDPixelFormat pixelFormat);

	bool WaitFrameCompletion();

	void ScheduleNextFrame();

	void DmaWrite(void* buffer, size_t size);
	nosVec2u GetDeltaSeconds() const;
	
protected:
	IDeckLink* Device = nullptr;
	IDeckLinkInput* Input = nullptr;
	IDeckLinkOutput* Output = nullptr;
	IDeckLinkProfileAttributes* ProfileAttributes = nullptr;
	IDeckLinkProfileManager* ProfileManager = nullptr;
	BMDTimeValue FrameDuration = 0;
	BMDTimeScale TimeScale = 0;
	
	std::array<IDeckLinkMutableVideoFrame*, 4> VideoFrames{};
	std::mutex VideoFramesMutex;
	std::condition_variable FrameCompletionCondition;
	std::deque<IDeckLinkVideoFrame*> CompletedFramesQueue;

	uint32_t TotalFramesScheduled = 0;
	uint32_t NextFrameToSchedule = 0;

	std::unordered_map<nosDeckLinkMode, bool> ActiveModes;
};

class Device
{
public:
	Device(uint32_t index, std::vector<std::unique_ptr<SubDevice>>&& subDevices);
	static bool InitializeDeviceList();
	static void ClearDeviceList();
	static Device* GetDevice(int64_t groupId);
	static Device* GetDevice(uint32_t deviceIndex);
	static const std::vector<std::unique_ptr<Device>>& GetDevices() { return Devices; }

	std::string GetUniqueDisplayName() const;

	std::vector<nosDeckLinkChannel> GetAvailableChannels(nosDeckLinkMode mode);

	bool CanOpenChannel(nosDeckLinkMode mode, nosDeckLinkChannel channel) const;

	SubDevice* GetSubDevice(int64_t index) const;

	uint32_t Index = -1;
	int64_t GroupId = -1;
	std::string ModelName;
protected:
	std::vector<std::unique_ptr<SubDevice>> SubDevices;

	inline static std::vector<std::unique_ptr<Device>> Devices;
};
	
}
