// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#include <Nodos/SubsystemAPI.h>

NOS_INIT_WITH_MIN_REQUIRED_MINOR(0); // APITransition: Reminder that this should be reset after next major!

NOS_BEGIN_IMPORT_DEPS()
NOS_END_IMPORT_DEPS()

#include "nosDeckLinkDevice/nosDeckLinkDevice.h"
#include "DeckLinkDevice.hpp"

namespace nos::decklink
{
std::unordered_map<uint32_t, nosDeckLinkSubsystem*> GExportedSubsystemVersions;

nosResult NOSAPI_CALL UnloadSubsystem()
{
	Device::ClearDeviceList();
	return NOS_RESULT_SUCCESS;
}

void NOSAPI_CALL GetDevices(size_t* outCount, nosDeckLinkDeviceDesc* outDeviceDescriptors)
{
	if (outCount == nullptr)
		return;
	auto& devices = Device::GetDevices();
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

nosResult NOSAPI_CALL GetAvailableChannels(uint32_t deviceIndex, nosDeckLinkMode mode, nosDeckLinkChannelList* outChannels)
{
	auto device = Device::GetDevice(deviceIndex);
	if (device == nullptr)
	{
		nosEngine.LogE("No such device with index %d", deviceIndex);
		return NOS_RESULT_NOT_FOUND;
	}
	if (!outChannels)
		return NOS_RESULT_INVALID_ARGUMENT;

	auto availableChannels = device->GetAvailableChannels(mode);
	outChannels->Count = availableChannels.size();
	for (size_t i = 0; i < availableChannels.size(); i++)
	{
		outChannels->Channels[i] = availableChannels[i];
	}
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
	*outSubsystemContext = subsystem;
	GExportedSubsystemVersions[minorVersion] = subsystem;
	return NOS_RESULT_SUCCESS;
}

nosResult NOSAPI_CALL Initialize()
{
	Device::InitializeDeviceList();
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
