#include "SubDevice.hpp"

#include <Nodos/Modules.h>
#include <EnumConversions.hpp>

namespace nos::decklink
{

SubDevice::SubDevice(IDeckLink* deviceInterface)
	: Device(deviceInterface)
{
	dlstring_t modelName;
	Device->GetModelName(&modelName);
	ModelName = DlToStdString(modelName);
	nosEngine.LogI("DeckLink Device created: %s", ModelName.c_str());
	DeleteString(modelName);

	auto res = Device->QueryInterface(IID_IDeckLinkInput, (void**)&Input.Interface);
	if (res != S_OK)
		nosEngine.LogE("DeckLinkDevice: Failed to get input interface for device: %s", ModelName.c_str());
	res = Device->QueryInterface(IID_IDeckLinkOutput, (void**)&Output.Interface);
	if (res != S_OK)
		nosEngine.LogE("DeckLinkDevice: Failed to get output interface for device: %s", ModelName.c_str());

	res = Device->QueryInterface(IID_IDeckLinkProfileAttributes, (void**)&ProfileAttributes);
	if (res != S_OK || !ProfileAttributes)
	{
		nosEngine.LogE("DeckLinkDevice: Failed to get profile attributes for device: %s", ModelName.c_str());
		return;
	}

	BOOL supported{};
	res = ProfileAttributes->GetFlag(BMDDeckLinkSupportsInputFormatDetection, &supported);
	if ((res != S_OK) || (supported == false))
	{
		nosEngine.LogW("DeckLinkDevice: Device does not support automatic mode detection for device: %s", ModelName.c_str());
		return;
	}

	res = ProfileAttributes->GetInt(BMDDeckLinkSubDeviceIndex, &SubDeviceIndex);
	if (res != S_OK)
		nosEngine.LogE("DeckLinkDevice: Failed to get subdevice index for device: %s", ModelName.c_str());

	dlstring_t handle;
	res = ProfileAttributes->GetString(BMDDeckLinkDeviceHandle, &handle);
	if (res != S_OK)
		nosEngine.LogE("DeckLinkDevice: Failed to get device handle for device: %s", ModelName.c_str());
	if (handle)
	{
		Handle = DlToStdString(handle);
		DeleteString(handle);
	}
	res = ProfileAttributes->GetInt(BMDDeckLinkProfileID, &ProfileId);
	if (res != S_OK)
		nosEngine.LogE("DeckLinkDevice: Failed to get profile ID for device: %s", ModelName.c_str());

	res = ProfileAttributes->GetInt(BMDDeckLinkDeviceGroupID, &DeviceGroupId);
	if (res != S_OK)
		nosEngine.LogE("DeckLinkDevice: Failed to get device group ID for device: %s", ModelName.c_str());

	res = ProfileAttributes->GetInt(BMDDeckLinkPersistentID, &PersistentId);
	if (res != S_OK)
		nosEngine.LogE("DeckLinkDevice: Failed to get persistent ID for device: %s", ModelName.c_str());

	res = ProfileAttributes->GetInt(BMDDeckLinkTopologicalID, &TopologicalId);
	if (res != S_OK)
		nosEngine.LogE("DeckLinkDevice: Failed to get topological ID for device: %s", ModelName.c_str());

	res = Device->QueryInterface(IID_IDeckLinkProfileManager, (void**)&ProfileManager);
	if (res != S_OK || !ProfileManager)
	{
		nosEngine.LogE("DeckLinkDevice: Failed to get profile manager for device: %s", ModelName.c_str());
		return;
	}
}

SubDevice::~SubDevice()
{
	Release(Device);
	Release(ProfileAttributes);
	Release(ProfileManager);
}

bool SubDevice::IsBusyWith(nosMediaIODirection mode)
{
	switch (mode)
	{
	case NOS_MEDIAIO_DIRECTION_INPUT:
		return Input.IsCurrentlyOpen();
	case NOS_MEDIAIO_DIRECTION_OUTPUT:
		return Output.IsCurrentlyOpen();
	}
	return false;
}

std::map<nosMediaIOFrameGeometry, std::set<nosMediaIOFrameRate>> SubDevice::GetSupportedOutputFrameGeometryAndFrameRates(std::unordered_set<nosMediaIOPixelFormat> const& pixelFormats)
{
	std::map<nosMediaIOFrameGeometry, std::set<nosMediaIOFrameRate>> supported;
	if (!Output)
	{
		nosEngine.LogE("SubDevice: Output interface is not available for device: %s", ModelName.c_str());
		return supported;
	}

	for (auto& pixelFormat : pixelFormats)
	{
		for (int i = NOS_MEDIAIO_FRAME_GEOMETRY_MIN; i < NOS_MEDIAIO_FRAME_GEOMETRY_MAX; ++i)
		{
			auto fg = static_cast<nosMediaIOFrameGeometry>(i);
			for (auto& displayMode : GetDisplayModesForFrameGeometry(fg))
			{
				if (DoesSupportOutputVideoMode(displayMode, GetDeckLinkPixelFormat(pixelFormat)))
				{
					supported[fg].insert(GetFrameRateFromDisplayMode(displayMode));
				}
			}
		}
	}
	return supported;
}

int32_t SubDevice::AddInputVideoFormatChangeCallback(nosDeckLinkInputVideoFormatChangeCallback callback, void* userData)
{
	return Input.AddInputVideoFormatChangeCallback(callback, userData);
}

void SubDevice::RemoveInputVideoFormatChangeCallback(uint32_t callbackId)
{
	Input.RemoveInputVideoFormatChangeCallback(callbackId);
}

bool SubDevice::StartStream(nosMediaIODirection mode)
{
	return GetIO(mode).StartStream();
}

bool SubDevice::StopStream(nosMediaIODirection mode)
{
	return GetIO(mode).StopStream();
}

bool SubDevice::DoesSupportOutputVideoMode(BMDDisplayMode displayMode, BMDPixelFormat pixelFormat)
{
	if (!Output)
		return false;
	BOOL supported{};
	BMDDisplayMode actualDisplayMode{};
	auto res = Output->DoesSupportVideoMode(bmdVideoConnectionSDI, displayMode, pixelFormat, bmdNoVideoOutputConversion, bmdSupportedVideoModeDefault, &actualDisplayMode, &supported);
	if (res != S_OK)
	{
		nosEngine.LogE("SubDevice: Failed to check video mode support for device: %s", ModelName.c_str());
		return false;
	}
	if (!supported && actualDisplayMode == displayMode)
	{
		// Supported but with conversion
		nosEngine.LogW("SubDevice: Video mode %d is supported but with conversion for device: %s", displayMode, ModelName.c_str());
		return true;
	}
	return supported;
}

bool SubDevice::OpenOutput(BMDDisplayMode displayMode, BMDPixelFormat pixelFormat)
{
	if (!Output) 
	{
		nosEngine.LogE("SubDevice: Output interface is not available for device: %s", ModelName.c_str());
		return false;
	}
	return Output.OpenStream(displayMode, pixelFormat);
}

bool SubDevice::CloseOutput()
{
	if (!Output)
	{
		nosEngine.LogE("SubDevice: Output interface is not available for device: %s", ModelName.c_str());
		return false;
	}
	return Output.CloseStream();
}

bool SubDevice::OpenInput(BMDPixelFormat pixelFormat)
{
	if (!Input)
	{
		nosEngine.LogE("SubDevice: Input interface is not available for device: %s", ModelName.c_str());
		return false;
	}
	return Input.OpenStream(bmdModeNTSC, pixelFormat); // Display mode will be auto-detected
}

bool SubDevice::CloseInput()
{
	if (!Input)
	{
		nosEngine.LogE("SubDevice: Input interface is not available for device: %s", ModelName.c_str());
		return false;
	}
	return Input.CloseStream();
}

constexpr IOHandlerBaseI& SubDevice::GetIO(nosMediaIODirection dir)
{
	if (dir == NOS_MEDIAIO_DIRECTION_INPUT)
		return Input;
	return Output;
}

bool SubDevice::WaitFrame(nosMediaIODirection dir, std::chrono::milliseconds timeout)
{
	return GetIO(dir).WaitFrame(timeout);
}

void SubDevice::DmaTransfer(nosMediaIODirection dir, void* buffer, size_t size)
{
	GetIO(dir).DmaTransfer(buffer, size);
}

std::optional<nosVec2u> SubDevice::GetDeltaSeconds(nosMediaIODirection dir)
{
	return GetIO(dir).GetDeltaSeconds();
}

}