// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#include <Nodos/PluginHelpers.hpp>

#include <nosUtil/Stopwatch.hpp>

#include <nosDeckLinkDevice/nosDeckLinkDevice.h>

#include "Generated/DeckLink_generated.h"

namespace nos::decklink
{
	
struct WaitFrameNode : NodeContext
{
	WaitFrameNode(const nosFbNode* node) : NodeContext(node)
	{
	}

	nosResult ExecuteNode(nosNodeExecuteParams* params) override
	{
		ChannelId* channelId = nullptr;
		for (size_t i = 0; i < params->PinCount; ++i)
		{
			auto& pin = params->Pins[i];
			if (pin.Name == NOS_NAME("ChannelId"))
				channelId = InterpretPinValue<ChannelId>(*pin.Data);
		}
		auto deviceIndex = channelId->device_index();
		auto channel = static_cast<nosDeckLinkChannel>(channelId->channel_index());
		{
			util::Stopwatch sw;
			nosDeckLink->WaitFrame(deviceIndex, channel, 1000);
			auto elapsed = sw.ElapsedString();
			char log[256];
			snprintf(log, sizeof(log), "DeckLink %d:%d WaitFrame", deviceIndex, channel);
			nosEngine.WatchLog(log, elapsed.c_str());
		}
		return NOS_RESULT_SUCCESS;
	}
};

nosResult RegisterWaitFrameNode(nosNodeFunctions* functions)
{
	NOS_BIND_NODE_CLASS(NOS_NAME_STATIC("WaitFrame"), WaitFrameNode, functions)
	return NOS_RESULT_SUCCESS;
}
}
