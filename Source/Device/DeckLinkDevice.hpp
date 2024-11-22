#pragma once

#include <unordered_map>
#include <string>
#include <memory>

// DeckLink
#if _WIN32
#include <comdef.h>
#endif
#include <DeckLinkAPI.h>

namespace nos::decklink
{

class DeckLinkDevice
{
public:
	DeckLinkDevice(IDeckLink* deviceInterface);
	~DeckLinkDevice();
	static bool InitializeDeviceList();
	static void ClearDeviceList();
	static DeckLinkDevice* GetDevice(uint32_t index);
	std::string ModelName;
protected:
	IDeckLink* DeviceInterface = nullptr;
private:
	inline static std::unordered_map<uint32_t, std::unique_ptr<DeckLinkDevice>> Devices;
};

}