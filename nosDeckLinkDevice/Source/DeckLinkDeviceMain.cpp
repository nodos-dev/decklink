// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#include <Nodos/SubsystemAPI.h>
#include <Nodos/Name.hpp>

#include "EnumConversions.hpp"


NOS_INIT_WITH_MIN_REQUIRED_MINOR(0); // APITransition: Reminder that this should be reset after next major!

#include <nosMediaIO/nosMediaIO.h>

NOS_MEDIAIO_SUBSYSTEM_INIT()

NOS_BEGIN_IMPORT_DEPS()
	NOS_MEDIAIO_SUBSYSTEM_IMPORT()
NOS_END_IMPORT_DEPS()

#include "nosDeckLinkDevice/nosDeckLinkDevice.h"
#include "Device.hpp"
#include "SubDevice.hpp"
#include "DeviceManager.hpp"

namespace nos::decklink
{
std::unordered_map<uint32_t, nosDeckLinkSubsystem*> GExportedSubsystemVersions;
std::unique_ptr<DeviceManager> GInstance;

nosResult NOSAPI_CALL UnloadSubsystem()
{
	GInstance.reset();
	return NOS_RESULT_SUCCESS;
}

const char* NOSAPI_CALL GetChannelName(nosDeckLinkChannel channel)
{
	if (channel < NOS_DECKLINK_CHANNEL_MIN || channel > NOS_DECKLINK_CHANNEL_MAX)
		return NOS_DECKLINK_CHANNEL_NAMES[NOS_DECKLINK_CHANNEL_INVALID];
	return NOS_DECKLINK_CHANNEL_NAMES[channel];
}

namespace internal
{
SubDevice* GetSubDevice(uint32_t deviceIndex, nosMediaIODirection dir, nosDeckLinkChannel channel)
{
	auto* device = GInstance->GetDevice(deviceIndex);
	if (!device)
	{
		nosEngine.LogE("No such device with index %d", deviceIndex);
		return nullptr;
	}
	auto* subDevice = device->GetSubDeviceOfChannel(dir, channel);
	if (!subDevice)
	{
		nosEngine.LogE("No such sub-device for channel %s", GetChannelName(channel));
		return nullptr;
	}
	return subDevice;
}
}

void NOSAPI_CALL GetDevices(size_t* outCount, nosDeckLinkDeviceDesc* outDeviceDescriptors)
{
	if (outCount == nullptr)
		return;
	auto& devices = GInstance->GetDevices();
	*outCount = devices.size();
	if (outDeviceDescriptors == nullptr)
		return;
	for (size_t i = 0; i < *outCount; i++)
	{
		auto& device = devices[i];
		outDeviceDescriptors[i].DeviceIndex = device->Index;
		auto uniqueDisplayName = device->GetUniqueDisplayName();
		size_t maxSize = sizeof(outDeviceDescriptors[i].UniqueDisplayName);
		strncpy(outDeviceDescriptors[i].UniqueDisplayName, uniqueDisplayName.c_str(), std::min(uniqueDisplayName.size() + 1, maxSize));
		outDeviceDescriptors[i].UniqueDisplayName[maxSize - 1] = '\0';
	}
}

nosResult NOSAPI_CALL GetAvailableChannels(uint32_t deviceIndex, nosMediaIODirection dir, nosDeckLinkChannelList* outChannels)
{
	auto device = GInstance->GetDevice(deviceIndex);
	if (device == nullptr)
	{
		nosEngine.LogE("No such device with index %d", deviceIndex);
		return NOS_RESULT_NOT_FOUND;
	}
	if (!outChannels)
		return NOS_RESULT_INVALID_ARGUMENT;

	auto availableChannels = device->GetAvailableChannels(dir);
	outChannels->Count = availableChannels.size();
	for (size_t i = 0; i < availableChannels.size(); i++)
	{
		outChannels->Channels[i] = availableChannels[i];
	}
	return NOS_RESULT_SUCCESS;
}

nosDeckLinkChannel NOSAPI_CALL GetChannelByName(const char* channelName)
{
	for (int i = 0; i < NOS_DECKLINK_CHANNEL_COUNT; i++)
	{
		if (strcmp(NOS_DECKLINK_CHANNEL_NAMES[i], channelName) == 0)
			return nosDeckLinkChannel(i);
	}
	return NOS_DECKLINK_CHANNEL_INVALID;
}

nosResult NOSAPI_CALL GetDeviceByUniqueDisplayName(const char* uniqueDisplayName, uint32_t* outDeviceIndex)
{
	auto& devices = GInstance->GetDevices();
	for (size_t i = 0; i < devices.size(); i++)
	{
		auto& device = devices[i];
		if (device->GetUniqueDisplayName() == uniqueDisplayName)
		{
			*outDeviceIndex = device->Index;
			return NOS_RESULT_SUCCESS;
		}
	}
	return NOS_RESULT_NOT_FOUND;
}

nosResult NOSAPI_CALL GetSupportedOutputFrameGeometries(uint32_t deviceIndex, nosDeckLinkChannel channel, nosMediaIOFrameGeometryList* outGeometries)
{
	if (!outGeometries)
	{
		nosEngine.LogE("Invalid argument: outGeometries is nullptr");
		return NOS_RESULT_INVALID_ARGUMENT;
	}
	auto* subDevice = internal::GetSubDevice(deviceIndex, NOS_MEDIAIO_DIRECTION_OUTPUT, channel);
	if (!subDevice)
		return NOS_RESULT_NOT_FOUND;
	auto supported = subDevice->GetSupportedOutputFrameGeometryAndFrameRates({NOS_MEDIAIO_PIXEL_FORMAT_YCBCR_8BIT, NOS_MEDIAIO_PIXEL_FORMAT_YCBCR_10BIT});
	outGeometries->Count = supported.size();
	int i = 0;
	for (auto& [fg, _] : supported)
		outGeometries->Geometries[i++] = fg;
	return NOS_RESULT_SUCCESS;
}

nosResult NOSAPI_CALL GetSupportedOutputFrameRatesForGeometry(uint32_t deviceIndex, nosDeckLinkChannel channel, nosMediaIOFrameGeometry frameGeo, nosMediaIOFrameRateList* outFrameRates)
{
	if (!outFrameRates)
	{
		nosEngine.LogE("Invalid argument: outFrameRates is nullptr");
		return NOS_RESULT_INVALID_ARGUMENT;
	}
	auto* subDevice = internal::GetSubDevice(deviceIndex, NOS_MEDIAIO_DIRECTION_OUTPUT, channel);
	if (!subDevice)
		return NOS_RESULT_NOT_FOUND;
	auto supported = subDevice->GetSupportedOutputFrameGeometryAndFrameRates({NOS_MEDIAIO_PIXEL_FORMAT_YCBCR_8BIT, NOS_MEDIAIO_PIXEL_FORMAT_YCBCR_10BIT});
	std::set<nosMediaIOFrameRate> frameRates;
	for (auto& [_, frs] : supported)
		frameRates.insert(frs.begin(), frs.end());
	int i = 0;
	outFrameRates->Count = frameRates.size();
	for (auto it = frameRates.rbegin(); it != frameRates.rend(); ++it)
		outFrameRates->FrameRates[i++] = *it;
	return NOS_RESULT_SUCCESS;
	
}

nosResult NOSAPI_CALL OpenChannel(uint32_t deviceIndex, nosDeckLinkOpenChannelParams* params)
{
	auto* device = GInstance->GetDevice(deviceIndex);
	if (!device)
	{
		nosEngine.LogE("No such device with index %d", deviceIndex);
		return NOS_RESULT_NOT_FOUND;
	}
	if (params->Direction == NOS_MEDIAIO_DIRECTION_OUTPUT)
	{
		if (!device->OpenOutput(params->Channel, GetDeckLinkDisplayMode(params->Output.Geometry, params->Output.FrameRate), GetDeckLinkPixelFormat(params->PixelFormat)))
		{
			nosEngine.LogE("Failed to open output for channel %s", GetChannelName(params->Channel));
			return NOS_RESULT_FAILED;
		}
	}
	else
	{
		if (!device->OpenInput(params->Channel, GetDeckLinkPixelFormat(params->PixelFormat)))
		{
			nosEngine.LogE("Failed to open input for channel %s", GetChannelName(params->Channel));
			return NOS_RESULT_FAILED;
		}
	}
	return NOS_RESULT_SUCCESS;
}

nosResult NOSAPI_CALL CloseChannel(uint32_t deviceIndex, nosDeckLinkChannel channel)
{
	auto* device = GInstance->GetDevice(deviceIndex);
	if (!device)
	{
		nosEngine.LogE("No such device with index %d", deviceIndex);
		return NOS_RESULT_NOT_FOUND;
	}
	if (!device->CloseChannel(channel))
	{
		nosEngine.LogE("Failed to close channel %s", GetChannelName(channel));
		return NOS_RESULT_FAILED;
	}
	return NOS_RESULT_SUCCESS;
}

nosResult NOSAPI_CALL GetCurrentDeltaSecondsOfChannel(uint32_t deviceIndex, nosDeckLinkChannel channel, nosVec2u* outDeltaSeconds)
{
	auto* device = GInstance->GetDevice(deviceIndex);
	if (!device)
	{
		nosEngine.LogE("No such device with index %d", deviceIndex);
		return NOS_RESULT_NOT_FOUND;
	}
	auto deltaSeconds = device->GetCurrentDeltaSecondsOfChannel(channel);
	if (!deltaSeconds)
	{
		nosEngine.LogE("Failed to get delta seconds for channel %s", GetChannelName(channel));
		return NOS_RESULT_FAILED;
	}
	*outDeltaSeconds = *deltaSeconds;
	return NOS_RESULT_SUCCESS;
}

nosResult NOSAPI_CALL WaitFrame(uint32_t deviceIndex, nosDeckLinkChannel channel, uint32_t timeoutMs)
{
	auto* device = GInstance->GetDevice(deviceIndex);
	if (!device)
	{
		nosEngine.LogE("No such device with index %d", deviceIndex);
		return NOS_RESULT_NOT_FOUND;
	}
	if (!device->WaitFrame(channel, std::chrono::milliseconds(timeoutMs)))
	{
		nosEngine.LogE("Failed to wait frame for channel %s", GetChannelName(channel));
		return NOS_RESULT_FAILED;
	}
	return NOS_RESULT_SUCCESS;
}

nosResult NOSAPI_CALL DMATransfer(uint32_t deviceIndex, nosDeckLinkChannel channel, void* data, size_t size)
{
	auto* device = GInstance->GetDevice(deviceIndex);
	if (!device)
	{
		nosEngine.LogE("No such device with index %d", deviceIndex);
		return NOS_RESULT_NOT_FOUND;
	}
	if (!device->DmaTransfer(channel, data, size))
	{
		nosEngine.LogE("Failed to wait frame for channel %s", GetChannelName(channel));
		return NOS_RESULT_FAILED;
	}
	return NOS_RESULT_SUCCESS;
}

int32_t NOSAPI_CALL RegisterInputVideoFormatChangeCallback(uint32_t deviceIndex, nosDeckLinkChannel channel, nosDeckLinkInputVideoFormatChangeCallback callback, void* userData)
{
	auto* device = GInstance->GetDevice(deviceIndex);
	if (!device)
	{
		nosEngine.LogE("No such device with index %d", deviceIndex);
		return NOS_RESULT_NOT_FOUND;
	}
	auto* subDevice = device->GetSubDeviceOfChannel(NOS_MEDIAIO_DIRECTION_INPUT, channel);
	if (!subDevice)
	{
		nosEngine.LogE("No such sub-device for channel %s", GetChannelName(channel));
		return -1;
	}
	return subDevice->AddInputVideoFormatChangeCallback(callback, userData);
}

nosResult NOSAPI_CALL UnregisterInputVideoFormatChangeCallback(uint32_t deviceIndex, nosDeckLinkChannel channel, int32_t callbackId)
{
	auto* device = GInstance->GetDevice(deviceIndex);
	if (!device)
	{
		nosEngine.LogE("No such device with index %d", deviceIndex);
		return NOS_RESULT_NOT_FOUND;
	}
	auto* subDevice = device->GetSubDeviceOfChannel(NOS_MEDIAIO_DIRECTION_INPUT, channel);
	if (!subDevice)
	{
		nosEngine.LogE("No such sub-device for channel %s", GetChannelName(channel));
		return NOS_RESULT_NOT_FOUND;
	}
	subDevice->RemoveInputVideoFormatChangeCallback(callbackId);
	return NOS_RESULT_SUCCESS;	
}

nosResult NOSAPI_CALL StartStream(uint32_t deviceIndex, nosDeckLinkChannel channel)
{
	auto* device = GInstance->GetDevice(deviceIndex);
	if (!device)
	{
		nosEngine.LogE("No such device with index %d", deviceIndex);
		return NOS_RESULT_NOT_FOUND;
	}
	return device->StartStream(channel) ? NOS_RESULT_SUCCESS : NOS_RESULT_FAILED;
}

nosResult NOSAPI_CALL StopStream(uint32_t deviceIndex, nosDeckLinkChannel channel)
{
	auto* device = GInstance->GetDevice(deviceIndex);
	if (!device)
	{
		nosEngine.LogE("No such device with index %d", deviceIndex);
		return NOS_RESULT_NOT_FOUND;
	}
	return device->StopStream(channel) ? NOS_RESULT_SUCCESS : NOS_RESULT_FAILED;
}

nosResult NOSAPI_CALL FlushInput(uint32_t deviceIndex, nosDeckLinkChannel channel)
{
	auto* subDevice = internal::GetSubDevice(deviceIndex, NOS_MEDIAIO_DIRECTION_INPUT, channel);
	if (!subDevice)
		return NOS_RESULT_NOT_FOUND;
	if (!subDevice->FlushInput())
		return NOS_RESULT_FAILED;
	return NOS_RESULT_SUCCESS;
}

nosResult NOSAPI_CALL Export(uint32_t minorVersion, void** outSubsystemContext)
{
	auto it = GExportedSubsystemVersions.find(minorVersion);
	if (it != GExportedSubsystemVersions.end())
	{
		*outSubsystemContext = it->second;
		return NOS_RESULT_SUCCESS;
	}
	nosDeckLinkSubsystem* subsystem = new nosDeckLinkSubsystem();
	subsystem->GetDevices = GetDevices;
	subsystem->GetAvailableChannels = GetAvailableChannels;
	subsystem->GetChannelName = GetChannelName;
	subsystem->GetChannelByName = GetChannelByName;
	subsystem->GetDeviceByUniqueDisplayName = GetDeviceByUniqueDisplayName;
	subsystem->GetSupportedOutputFrameGeometries = GetSupportedOutputFrameGeometries;
	subsystem->GetSupportedOutputFrameRatesForGeometry = GetSupportedOutputFrameRatesForGeometry;
	subsystem->OpenChannel = OpenChannel;
	subsystem->CloseChannel = CloseChannel;
	subsystem->GetCurrentDeltaSecondsOfChannel = GetCurrentDeltaSecondsOfChannel;
	subsystem->WaitFrame = WaitFrame;
	subsystem->DMATransfer = DMATransfer;
	subsystem->RegisterInputVideoFormatChangeCallback = RegisterInputVideoFormatChangeCallback;
	subsystem->UnregisterInputVideoFormatChangeCallback = UnregisterInputVideoFormatChangeCallback;
	subsystem->StartStream = StartStream;
	subsystem->StopStream = StopStream;
	subsystem->FlushInput = FlushInput;
	*outSubsystemContext = subsystem;
	GExportedSubsystemVersions[minorVersion] = subsystem;
	return NOS_RESULT_SUCCESS;
}

nosResult NOSAPI_CALL Initialize()
{
	GInstance = std::make_unique<DeviceManager>();
	return NOS_RESULT_SUCCESS;
}

extern "C"
{
NOSAPI_ATTR nosResult NOSAPI_CALL nosExportSubsystem(nosSubsystemFunctions* subsystemFunctions)
{
	subsystemFunctions->OnRequest = Export;
	subsystemFunctions->Initialize = Initialize;
	subsystemFunctions->OnPreUnloadSubsystem = UnloadSubsystem;
	return NOS_RESULT_SUCCESS;
}
}
}
