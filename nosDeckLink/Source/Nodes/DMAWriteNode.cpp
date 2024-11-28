// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#include <Nodos/PluginHelpers.hpp>

#include <nosVulkanSubsystem/nosVulkanSubsystem.h>
#include <nosVulkanSubsystem/Helpers.hpp>
#include <nosDeckLinkDevice/nosDeckLinkDevice.h>

#include <nosUtil/Stopwatch.hpp>

#include "Generated/DeckLink_generated.h"

namespace nos::decklink
{
	
struct DMAWriteNode : NodeContext
{
	DMAWriteNode(const nosFbNode* node) : NodeContext(node)
	{
	}

	void GetScheduleInfo(nosScheduleInfo* out) override
	{
		*out = nosScheduleInfo{
			.Importance = 1,
			.DeltaSeconds = LastDeltaSeconds,
			.Type = NOS_SCHEDULE_TYPE_ON_DEMAND,
		};
	}

	void OnPinValueChanged(nos::Name pinName, nosUUID pinId, nosBuffer value) override
	{ 
		if (pinName == NOS_NAME_STATIC("ChannelId"))
		{
			auto& newChannelId = *InterpretPinValue<ChannelId>(value);
			if (LastChannelId == newChannelId)
				return;
			LastChannelId = newChannelId;
			nosVec2u deltaSeconds{0, 0};
			if (LastChannelId.is_open())
			{
				nosDeckLink->GetCurrentDeltaSecondsOfChannel(LastChannelId.device_index(), static_cast<nosDeckLinkChannel>(LastChannelId.channel_index()), &deltaSeconds);
				if (memcmp(&deltaSeconds, &LastDeltaSeconds, sizeof(deltaSeconds)) != 0)
				{
					LastDeltaSeconds = deltaSeconds;
					nosEngine.RecompilePath(NodeId);
				}
			}
		}
	}

	nosResult ExecuteNode(nosNodeExecuteParams* params) override
	{
		nosResourceShareInfo inputBuffer{};
		auto fieldType = nos::sys::vulkan::FieldType::UNKNOWN;
		ChannelId* channelId = nullptr;
		for (size_t i = 0; i < params->PinCount; ++i)
		{
			auto& pin = params->Pins[i];
			if (pin.Name == NOS_NAME_STATIC("Input"))
				inputBuffer = vkss::ConvertToResourceInfo(*InterpretPinValue<sys::vulkan::Buffer>(*pin.Data));
			if (pin.Name == NOS_NAME("FieldType"))
				fieldType = *InterpretPinValue<sys::vulkan::FieldType>(*pin.Data);
			if (pin.Name == NOS_NAME("ChannelId"))
				channelId = InterpretPinValue<ChannelId>(*pin.Data);
		}

		auto deviceIndex = channelId->device_index();
		auto channel = static_cast<nosDeckLinkChannel>(channelId->channel_index());

		if (!inputBuffer.Memory.Handle)
			return NOS_RESULT_FAILED;

		auto buffer = nosVulkan->Map(&inputBuffer);
		{
			util::Stopwatch sw;
			nosDeckLink->DMAWrite(deviceIndex, channel, buffer, inputBuffer.Info.Buffer.Size);
			auto elapsed = sw.ElapsedString();
			char log[256];
			snprintf(log, sizeof(log), "DeckLink %d:%d DMAWrite", deviceIndex, channel);
			nosEngine.WatchLog(log, elapsed.c_str());
		}
		nosDeckLink->ScheduleNextFrame(deviceIndex, channel);

		nosScheduleNodeParams schedule {
			.NodeId = NodeId,
			.AddScheduleCount = 1
		};
		nosEngine.ScheduleNode(&schedule);
		
		return NOS_RESULT_SUCCESS;
	}

	void OnPathStart() override
	{
		nosScheduleNodeParams schedule{.NodeId = NodeId, .AddScheduleCount = 1};
		nosEngine.ScheduleNode(&schedule);
	}

	ChannelId LastChannelId;
	nosVec2u LastDeltaSeconds{0, 0};
};

nosResult RegisterDMAWriteNode(nosNodeFunctions* functions)
{
	NOS_BIND_NODE_CLASS(NOS_NAME_STATIC("DMAWrite"), DMAWriteNode, functions)
	return NOS_RESULT_SUCCESS;
}
}
