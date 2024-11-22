#include "DeckLinkDevice.hpp"

// Nodos
#include <Nodos/Modules.h>

#if _WIN32
#define dlbool_t	BOOL
#define dlstring_t	BSTR
const std::function<void(dlstring_t)> DeleteString = SysFreeString;
const std::function<std::string(dlstring_t)> DlToStdString = [](dlstring_t dl_str) -> std::string
	{
		int wlen = ::SysStringLen(dl_str);
		int mblen = ::WideCharToMultiByte(CP_ACP, 0, (wchar_t*)dl_str, wlen, NULL, 0, NULL, NULL);

		std::string ret_str(mblen, '\0');
		mblen = ::WideCharToMultiByte(CP_ACP, 0, (wchar_t*)dl_str, wlen, &ret_str[0], mblen, NULL, NULL);

		return ret_str;
	};
const std::function<dlstring_t(std::string)> StdToDlString = [](std::string std_str) -> dlstring_t
	{
		int wlen = ::MultiByteToWideChar(CP_ACP, 0, std_str.data(), (int)std_str.length(), NULL, 0);

		dlstring_t ret_str = ::SysAllocStringLen(NULL, wlen);
		::MultiByteToWideChar(CP_ACP, 0, std_str.data(), (int)std_str.length(), ret_str, wlen);

		return ret_str;
	};
#endif


namespace nos::decklink
{

HRESULT GetDeckLinkIterator(IDeckLinkIterator **deckLinkIterator)
{
	HRESULT result = S_OK;

	// Create an IDeckLinkIterator object to enumerate all DeckLink cards in the system
	result = CoCreateInstance(CLSID_CDeckLinkIterator, NULL, CLSCTX_ALL, IID_IDeckLinkIterator, (void**)deckLinkIterator);
	if (FAILED(result))
		nosEngine.LogE("A DeckLink iterator could not be created. The DeckLink drivers may not be installed.");

	return result;
}

DeckLinkDevice::DeckLinkDevice(IDeckLink* deviceInterface)
	: DeviceInterface(deviceInterface)
{
	dlstring_t modelName;
	DeviceInterface->GetModelName(&modelName);
	ModelName = DlToStdString(modelName);
	nosEngine.LogI("DeckLink Device created: %s", ModelName.c_str());
	DeleteString(modelName);
}

DeckLinkDevice::~DeckLinkDevice()
{
	if (DeviceInterface)
	{
		DeviceInterface->Release();
		DeviceInterface = nullptr;
	}
}

bool DeckLinkDevice::InitializeDeviceList()
{
	IDeckLinkIterator* deckLinkIterator = nullptr;

	HRESULT result = E_FAIL;
#ifdef _WIN32
	result = CoInitialize(NULL);
	if (FAILED(result))
	{
		nosEngine.LogE("DeckLinkDevice: Initialization of COM failed with error: %s", _com_error(result).ErrorMessage());
		return false;
	}
#endif

	result = GetDeckLinkIterator(&deckLinkIterator);
	if (FAILED(result))
	{
		return false;
	}

	IDeckLink* deckLink = NULL;
	uint32_t   deviceNumber = 0;

	// Obtain an IDeckLink instance for each device on the system
	while (deckLinkIterator->Next(&deckLink) == S_OK)
	{
		auto bmDevice = std::make_unique<DeckLinkDevice>(deckLink);
		Devices[deviceNumber] = std::move(bmDevice);
		deviceNumber++;
	}

	if (deckLinkIterator)
		deckLinkIterator->Release();
	return true;
}

void DeckLinkDevice::ClearDeviceList()
{
	Devices.clear();
}

DeckLinkDevice* DeckLinkDevice::GetDevice(uint32_t index)
{
	auto it = Devices.find(index);
	if (it != Devices.end())
		return it->second.get();
	return nullptr;
}
}
