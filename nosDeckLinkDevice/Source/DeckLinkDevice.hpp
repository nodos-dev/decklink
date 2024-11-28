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
#include <Nodos/Modules.h>

#include "nosDeckLinkDevice/nosDeckLinkDevice.h"

namespace nos::decklink
{
template <typename T>
void Release(T& obj)
{
	if (obj)
	{
		obj->Release();
		obj = nullptr;
	}
}

#if _WIN32
#define dlbool_t	BOOL
#define dlstring_t	BSTR
inline const std::function<void(dlstring_t)> DeleteString = SysFreeString;
inline const std::function<std::string(dlstring_t)> DlToStdString = [](dlstring_t dl_str) -> std::string
{
	int wlen = ::SysStringLen(dl_str);
	int mblen = ::WideCharToMultiByte(CP_ACP, 0, (wchar_t*)dl_str, wlen, NULL, 0, NULL, NULL);

	std::string ret_str(mblen, '\0');
	mblen = ::WideCharToMultiByte(CP_ACP, 0, (wchar_t*)dl_str, wlen, &ret_str[0], mblen, NULL, NULL);

	return ret_str;
};
inline const std::function<dlstring_t(std::string)> StdToDlString = [](std::string std_str) -> dlstring_t
{
	int wlen = ::MultiByteToWideChar(CP_ACP, 0, std_str.data(), (int)std_str.length(), NULL, 0);

	dlstring_t ret_str = ::SysAllocStringLen(NULL, wlen);
	::MultiByteToWideChar(CP_ACP, 0, std_str.data(), (int)std_str.length(), ret_str, wlen);

	return ret_str;
};
#endif

template <typename T>
class Object : public T
{
public:
	Object() :
		RefCount(1)
	{
	}

	// IUnknown needs only a dummy implementation
	HRESULT	STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID *ppv) override
	{
		return E_NOINTERFACE;
	}

	ULONG STDMETHODCALLTYPE AddRef() override
	{
		return ++RefCount;
	}

	ULONG STDMETHODCALLTYPE Release() override
	{
		ULONG newRefValue = --RefCount;

		if (newRefValue == 0)
			delete this;

		return newRefValue;
	}

private:
	std::atomic<int32_t> RefCount;
};

std::vector<std::unique_ptr<class Device>> InitializeDevices();

template <typename T>
struct IOBase
{
	virtual ~IOBase() = default;
	T* Interface = nullptr;
	bool IsActive = false;

	BMDTimeValue FrameDuration = 0;
	BMDTimeScale TimeScale = 0;

	std::mutex VideoFramesMutex;
	std::condition_variable FrameAvailableCond;
	std::deque<IDeckLinkVideoFrame*> FrameQueue;
	
	operator bool() const
	{
		return Interface != nullptr;
	}
	T* operator->() const
	{
		return Interface;
	}

	virtual bool Open(BMDDisplayMode displayMode, BMDPixelFormat pixelFormat) = 0;
	virtual bool Close() = 0;

	virtual bool WaitFrame(std::chrono::milliseconds timeout);
	virtual void OnDmaTransferCompleted() {}
	void DmaTransfer(nosMediaIODirection dir, void* buffer, size_t size)
	{
		{
			std::unique_lock lock (VideoFramesMutex);
			if (FrameQueue.empty())
				return;
			auto frameBuffer = FrameQueue.front();
			void* frameBytes = nullptr;
			frameBuffer->GetBytes(&frameBytes);
			size_t actualBufferSize = frameBuffer->GetRowBytes() * frameBuffer->GetHeight();
			if (frameBytes && buffer)
			{
				if (size != actualBufferSize)
				{
					nosEngine.LogE("Buffer size does not match frame size");
				}
				size_t copySize = std::min(size, actualBufferSize);
				auto* dst = dir == NOS_MEDIAIO_DIRECTION_INPUT ? buffer : frameBytes;
				auto* src = dir == NOS_MEDIAIO_DIRECTION_INPUT ? frameBytes : buffer;
				std::memcpy(dst, src, copySize);
			}
			if (dir == NOS_MEDIAIO_DIRECTION_INPUT)
				frameBuffer->Release();
			FrameQueue.pop_front();
		}
		OnDmaTransferCompleted();
	}
	std::optional<nosVec2u> GetDeltaSeconds() const;
};

template <typename T>
bool IOBase<T>::WaitFrame(std::chrono::milliseconds timeout)
{
	std::unique_lock lock(VideoFramesMutex);
	return FrameAvailableCond.wait_for(lock, timeout, [this]()
	{
		return !FrameQueue.empty();
	});
}

template <typename T>
std::optional<nosVec2u> IOBase<T>::GetDeltaSeconds() const
{
	if (!IsActive)
		return std::nullopt;
	return nosVec2u{ (uint32_t)FrameDuration, (uint32_t)TimeScale };
}

struct OutputHandler : IOBase<IDeckLinkOutput>
{
	std::array<IDeckLinkMutableVideoFrame*, 4> VideoFrames{};
	
	uint32_t TotalFramesScheduled = 0;
	uint32_t NextFrameToSchedule = 0;

	~OutputHandler() override;

	bool Open(BMDDisplayMode displayMode, BMDPixelFormat pixelFormat) override;
	bool Close() override;
	
	void ScheduleNextFrame();
	void OnDmaTransferCompleted() override;
};
	
struct InputHandler : IOBase<IDeckLinkInput>
{
	~InputHandler() override;
	
	bool Open(BMDDisplayMode displayMode, BMDPixelFormat pixelFormat) override;
	bool Close() override;
};
	
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

	// Output
	bool DoesSupportOutputVideoMode(BMDDisplayMode displayMode, BMDPixelFormat pixelFormat);
	bool OpenOutput(BMDDisplayMode displayMode, BMDPixelFormat pixelFormat);
	bool CloseOutput();
	bool WaitFrame(nosMediaIODirection dir, std::chrono::milliseconds timeout);
	void DmaTransfer(nosMediaIODirection dir, void* buffer, size_t size);
	std::optional<nosVec2u> GetDeltaSeconds(nosMediaIODirection dir) const;
	
	bool OpenInput(BMDPixelFormat pixelFormat);
	bool CloseInput();

	template<typename T>
	constexpr IOBase<T>& GetIO(nosMediaIODirection dir);

	IDeckLinkProfileManager* ProfileManager = nullptr;
protected:
	IDeckLink* Device = nullptr;
	IDeckLinkProfileAttributes* ProfileAttributes = nullptr;

	OutputHandler Output;
	InputHandler Input;
};

template <typename T>
constexpr IOBase<T>& SubDevice::GetIO(nosMediaIODirection dir)
{
	switch (dir)
	{
	case NOS_MEDIAIO_DIRECTION_INPUT:
		return Input;
	case NOS_MEDIAIO_DIRECTION_OUTPUT:
		return Output;
	}
	assert(false);
}

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
