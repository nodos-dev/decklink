/*
 * Copyright MediaZ Teknoloji A.S. All Rights Reserved.
 */

#ifndef NOS_DECKLINK_DEVICE_SUBSYSTEM_H_INCLUDED
#define NOS_DECKLINK_DEVICE_SUBSYSTEM_H_INCLUDED

#include <Nodos/Types.h>

typedef struct nosDeckLinkDeviceDesc {
	uint32_t DeviceIndex;
	char UniqueDisplayName[256];
} nosDeckLinkDeviceDesc;

typedef struct nosDeckLinkSubsystem {
	void (NOSAPI_CALL* GetDevices)(size_t *outCount, nosDeckLinkDeviceDesc* outDeviceDescriptors);
} nosDeckLinkSubsystem;

#endif