/*
 * Copyright MediaZ Teknoloji A.S. All Rights Reserved.
 */

#ifndef NOS_DECKLINK_DEVICE_SUBSYSTEM_H_INCLUDED
#define NOS_DECKLINK_DEVICE_SUBSYSTEM_H_INCLUDED

#if __cplusplus
extern "C"
{
#endif

#include <Nodos/Types.h>

typedef struct nosDeckLinkDeviceDesc {
	uint32_t DeviceIndex;
	char UniqueDisplayName[256];
} nosDeckLinkDeviceDesc;

typedef enum nosDeckLinkChannel
{
	NOS_DECK_LINK_CHANNEL_INVALID,
	NOS_DECK_LINK_CHANNEL_SINGLE_LINK_1,
	NOS_DECK_LINK_CHANNEL_SINGLE_LINK_2,
	NOS_DECK_LINK_CHANNEL_SINGLE_LINK_3,
	NOS_DECK_LINK_CHANNEL_SINGLE_LINK_4,
	NOS_DECK_LINK_CHANNEL_SINGLE_LINK_5,
	NOS_DECK_LINK_CHANNEL_SINGLE_LINK_6,
	NOS_DECK_LINK_CHANNEL_SINGLE_LINK_7,
	NOS_DECK_LINK_CHANNEL_SINGLE_LINK_8,
} nosDeckLinkChannel;

typedef struct nosDeckLinkChannelList {
	size_t Count;
	nosDeckLinkChannel Channels[NOS_DECK_LINK_CHANNEL_SINGLE_LINK_8 - NOS_DECK_LINK_CHANNEL_INVALID];
} nosDeckLinkAvailableChannels;

typedef enum nosDeckLinkMode
{
	NOS_DECK_LINK_MODE_INPUT,
	NOS_DECK_LINK_MODE_OUTPUT,
} nosDeckLinkMode;

typedef struct nosDeckLinkSubsystem {
	void (NOSAPI_CALL* GetDevices)(size_t *inoutCount, nosDeckLinkDeviceDesc* outDeviceDescriptors);
	nosResult (NOSAPI_CALL* GetAvailableChannels)(uint32_t deviceIndex, nosDeckLinkMode mode, nosDeckLinkChannelList* outChannels);
	const char* (NOSAPI_CALL* GetChannelName)(nosDeckLinkChannel channel);
} nosDeckLinkSubsystem;

#pragma region Helper Declarations & Macros

// Make sure these are same with nossys file.
#define NOS_DECKLINK_DEVICE_SUBSYSTEM_NAME "nos.device.decklink"
#define NOS_DECKLINK_DEVICE_SUBSYSTEM_VERSION_MAJOR 1
#define NOS_DECKLINK_DEVICE_SUBSYSTEM_VERSION_MINOR 0

extern struct nosModuleInfo nosDeckLinkSubsystemModuleInfo;
extern nosDeckLinkSubsystem* nosDeckLink;

#define NOS_DECKLINK_DEVICE_SUBSYSTEM_INIT()      \
	nosModuleInfo nosDeckLinkSubsystemModuleInfo; \
	nosDeckLinkSubsystem* nosDeckLink = nullptr;

#define NOS_DECKLINK_DEVICE_SUBSYSTEM_IMPORT() NOS_IMPORT_DEP(NOS_DECKLINK_DEVICE_SUBSYSTEM_NAME, nosDeckLinkSubsystemModuleInfo, nosDeckLink)

#pragma endregion

#if __cplusplus
}
#endif

#endif