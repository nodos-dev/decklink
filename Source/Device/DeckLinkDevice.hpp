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

namespace nos::decklink
{

class SubDevice
{
public:
	friend class OutputCallback;
	SubDevice(IDeckLink* deviceInterface);
	~SubDevice();
	static bool InitializeSubDeviceList();
	static void ClearSubDeviceList();
	static SubDevice* GetDevice(uint32_t index);

	std::string ModelName;
	int64_t SubDeviceIndex = -1;
	std::string Handle;
	int64_t ProfileId;
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
private:
	inline static std::unordered_map<uint32_t, std::unique_ptr<SubDevice>> SubDevices;
};

}
