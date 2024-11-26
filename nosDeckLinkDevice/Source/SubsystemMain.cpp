// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#include <Nodos/SubsystemAPI.h>

NOS_INIT_WITH_MIN_REQUIRED_MINOR(0); // APITransition: Reminder that this should be reset after next major!

NOS_BEGIN_IMPORT_DEPS()
NOS_END_IMPORT_DEPS()

#include "nosDeckLinkDevice.h"

namespace nos::decklink
{

nosResult NOSAPI_CALL UnloadSubsystem()
{
	return NOS_RESULT_SUCCESS;
}

nosResult NOSAPI_CALL GetDevices(size_t* outCount, nosDeckLinkDeviceDesc* outDeviceDescriptors)
{
	return NOS_RESULT_SUCCESS;
}

std::unordered_map<uint32_t, nosDeckLinkSubsystem*> GExportedSubsystemVersions;

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
	*outSubsystemContext = subsystem;
	GExportedSubsystemVersions[minorVersion] = subsystem;
	return NOS_RESULT_SUCCESS;
}

extern "C"
{
NOSAPI_ATTR nosResult NOSAPI_CALL nosExportSubsystem(nosSubsystemFunctions* subsystemFunctions)
{
	subsystemFunctions->OnRequest = Export;
	subsystemFunctions->OnPreUnloadSubsystem = UnloadSubsystem;
	return NOS_RESULT_SUCCESS;
}
}
}
