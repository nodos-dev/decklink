// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#pragma once

#include <unordered_map>
#include <string>

#include "DeckLinkAPI.h"
#include "nosDeckLinkDevice/nosDeckLinkDevice.h"

namespace nos::decklink
{

// Model Name -> Profile -> SubDeviceIndex -> Mode -> Connectors
typedef std::unordered_map<std::string, std::unordered_map<BMDProfileID, std::unordered_map<int64_t, std::unordered_map<nosDeckLinkChannel, std::unordered_set<nosMediaIODirection>>>>> ConnectorMap;
inline const ConnectorMap& GetChannelMap()
{
	static ConnectorMap map{};
	if (!map.empty())
		return map;
	auto& deckLink8KPro = map["DeckLink 8K Pro"] = {};
	deckLink8KPro[bmdProfileFourSubDevicesHalfDuplex][0][NOS_DECKLINK_CHANNEL_SINGLE_LINK_1] = {NOS_MEDIAIO_DIRECTION_INPUT, NOS_MEDIAIO_DIRECTION_OUTPUT};
	deckLink8KPro[bmdProfileFourSubDevicesHalfDuplex][1][NOS_DECKLINK_CHANNEL_SINGLE_LINK_3] = {NOS_MEDIAIO_DIRECTION_INPUT, NOS_MEDIAIO_DIRECTION_OUTPUT};
	deckLink8KPro[bmdProfileFourSubDevicesHalfDuplex][2][NOS_DECKLINK_CHANNEL_SINGLE_LINK_2] = {NOS_MEDIAIO_DIRECTION_INPUT, NOS_MEDIAIO_DIRECTION_OUTPUT};
	deckLink8KPro[bmdProfileFourSubDevicesHalfDuplex][3][NOS_DECKLINK_CHANNEL_SINGLE_LINK_4] = {NOS_MEDIAIO_DIRECTION_INPUT, NOS_MEDIAIO_DIRECTION_OUTPUT};
	return map;
}
	
}
