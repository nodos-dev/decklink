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

// nos.sys.mediaio
#include <nosMediaIO/nosMediaIO.h>

typedef struct nosDeckLinkDeviceDesc {
	uint32_t DeviceIndex;
	char UniqueDisplayName[256];
} nosDeckLinkDeviceDesc;

typedef enum nosDeckLinkChannel
{
	NOS_DECKLINK_CHANNEL_INVALID,
	NOS_DECKLINK_CHANNEL_SINGLE_LINK_1,
	NOS_DECKLINK_CHANNEL_SINGLE_LINK_2,
	NOS_DECKLINK_CHANNEL_SINGLE_LINK_3,
	NOS_DECKLINK_CHANNEL_SINGLE_LINK_4,
	NOS_DECKLINK_CHANNEL_SINGLE_LINK_5,
	NOS_DECKLINK_CHANNEL_SINGLE_LINK_6,
	NOS_DECKLINK_CHANNEL_SINGLE_LINK_7,
	NOS_DECKLINK_CHANNEL_SINGLE_LINK_8,
	NOS_DECKLINK_CHANNEL_MAX = NOS_DECKLINK_CHANNEL_SINGLE_LINK_8
} nosDeckLinkChannel;

#define NOS_DECKLINK_CHANNEL_COUNT (NOS_DECKLINK_CHANNEL_SINGLE_LINK_8 - NOS_DECKLINK_CHANNEL_INVALID)

inline const char* NOS_DECKLINK_CHANNEL_NAMES[] = {
	"INVALID",
	"Single Link 1",
	"Single Link 2",
	"Single Link 3",
	"Single Link 4",
	"Single Link 5",
	"Single Link 6",
	"Single Link 7",
	"Single Link 8",
};

typedef struct nosDeckLinkChannelList {
	size_t Count;
	nosDeckLinkChannel Channels[NOS_DECKLINK_CHANNEL_COUNT];
} nosDeckLinkAvailableChannels;

typedef struct nosDeckLinkOpenChannelParams
{
	nosMediaIODirection Direction;
	nosDeckLinkChannel Channel;
	nosMediaIOPixelFormat PixelFormat;
	struct
	{
		nosMediaIOFrameGeometry Geometry;
		nosMediaIOFrameRate FrameRate;	
	} Output; // Don't care if Direction == NOS_MEDIAIO_DIRECTION_INPUT
} nosDeckLinkOpenOutputParams;
	
typedef struct nosDeckLinkSubsystem {
	void (NOSAPI_CALL* GetDevices)(size_t *inoutCount, nosDeckLinkDeviceDesc* outDeviceDescriptors);
	nosResult (NOSAPI_CALL* GetAvailableChannels)(uint32_t deviceIndex, nosMediaIODirection direction, nosDeckLinkChannelList* outChannels);
	const char* (NOSAPI_CALL* GetChannelName)(nosDeckLinkChannel channel);
	nosDeckLinkChannel (NOSAPI_CALL* GetChannelByName)(const char* channelName);
	nosResult (NOSAPI_CALL* GetDeviceByUniqueDisplayName)(const char* uniqueDisplayName, uint32_t* outDeviceIndex);

	// Channels
	nosResult (NOSAPI_CALL* GetSupportedOutputFrameGeometries)(uint32_t deviceIndex, nosDeckLinkChannel channel, nosMediaIOFrameGeometryList* outGeometries);
	nosResult (NOSAPI_CALL* GetSupportedOutputFrameRatesForGeometry)(uint32_t deviceIndex, nosDeckLinkChannel channel, nosMediaIOFrameGeometry geometry, nosMediaIOFrameRateList* outFrameRates);
	nosResult (NOSAPI_CALL* OpenChannel)(uint32_t deviceIndex, nosDeckLinkOpenChannelParams* params);
	nosResult (NOSAPI_CALL* CloseChannel)(uint32_t deviceIndex, nosDeckLinkChannel channel);
	nosResult (NOSAPI_CALL* GetCurrentDeltaSecondsOfChannel)(uint32_t deviceIndex, nosDeckLinkChannel channel, nosVec2u* outDeltaSeconds);

	// I/O
	nosResult (NOSAPI_CALL* WaitFrame)(uint32_t deviceIndex, nosDeckLinkChannel channel, uint32_t timeoutMs);
	nosResult (NOSAPI_CALL* DMAWrite)(uint32_t deviceIndex, nosDeckLinkChannel channel, const void* data, size_t size);
	nosResult (NOSAPI_CALL* ScheduleNextFrame)(uint32_t deviceIndex, nosDeckLinkChannel channel);
	nosResult (NOSAPI_CALL* DMARead)(uint32_t deviceIndex, nosDeckLinkChannel channel, void* outData, size_t size);
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