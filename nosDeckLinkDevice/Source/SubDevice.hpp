#pragma once

#include "Common.hpp"

namespace nos::decklink
{
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
	std::optional<nosVec2u> GetDeltaSeconds(nosMediaIODirection dir);
	
	bool OpenInput(BMDPixelFormat pixelFormat);
	bool CloseInput();

	constexpr IOHandlerBaseI& GetIO(nosMediaIODirection dir);

	IDeckLinkProfileManager* ProfileManager = nullptr;
protected:
	IDeckLink* Device = nullptr;
	IDeckLinkProfileAttributes* ProfileAttributes = nullptr;

	OutputHandler Output;
	InputHandler Input;
};
}