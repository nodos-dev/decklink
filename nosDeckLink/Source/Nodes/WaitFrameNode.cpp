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

	void OnPinValueChanged(nos::Name pinName, nosUUID pinId, nosBuffer value) override
	{ 
		if (pinName == NOS_NAME_STATIC("ChannelId"))
		{
			auto& newChannelId = *InterpretPinValue<ChannelId>(value);
			if (CurChannelId == newChannelId)
				return;
			CurChannelId = newChannelId;
		}
	}

	nosResult ExecuteNode(nosNodeExecuteParams* params) override
	{
		auto deviceIndex = CurChannelId.device_index();
		auto channel = static_cast<nosDeckLinkChannel>(CurChannelId.channel_index());

		{
			util::Stopwatch sw;
			nosDeckLink->WaitFrame(deviceIndex, channel, 100);
			auto elapsed = sw.ElapsedString();
			char log[256];
			snprintf(log, sizeof(log), "DeckLink %d:%d WaitFrame", deviceIndex, channel);
			nosEngine.WatchLog(log, elapsed.c_str());
		}
		return NOS_RESULT_SUCCESS;
	}

	ChannelId CurChannelId;
	
};

nosResult RegisterWaitFrameNode(nosNodeFunctions* functions)
{
	NOS_BIND_NODE_CLASS(NOS_NAME_STATIC("WaitFrame"), WaitFrameNode, functions)
	return NOS_RESULT_SUCCESS;
}
}
