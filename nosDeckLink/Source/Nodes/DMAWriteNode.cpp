// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#include <Nodos/PluginHelpers.hpp>

#include <nosVulkanSubsystem/nosVulkanSubsystem.h>
#include <nosVulkanSubsystem/Helpers.hpp>

#include <nosUtil/Stopwatch.hpp>

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
			.DeltaSeconds = /*Device ? Device->GetDeltaSeconds() : */nosVec2u{0, 0},
			.Type = NOS_SCHEDULE_TYPE_ON_DEMAND,
		};
	}

	void OnPinValueChanged(nos::Name pinName, nosUUID pinId, nosBuffer value) override
	{
	}

	nosResult ExecuteNode(nosNodeExecuteParams* params) override
	{
		nosResourceShareInfo inputBuffer{};
		auto fieldType = nos::sys::vulkan::FieldType::UNKNOWN;
		for (size_t i = 0; i < params->PinCount; ++i)
		{
			auto& pin = params->Pins[i];
			if (pin.Name == NOS_NAME_STATIC("Input"))
				inputBuffer = vkss::ConvertToResourceInfo(*InterpretPinValue<sys::vulkan::Buffer>(*pin.Data));
			if (pin.Name == NOS_NAME("FieldType"))
				fieldType = *InterpretPinValue<sys::vulkan::FieldType>(*pin.Data);
		}

		if (!inputBuffer.Memory.Handle)
			return NOS_RESULT_FAILED;

		auto buffer = nosVulkan->Map(&inputBuffer);
		/*Device->DmaWrite(buffer, inputBuffer.Info.Buffer.Size);
		Device->ScheduleNextFrame();*/

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
};

nosResult RegisterDMAWriteNode(nosNodeFunctions* functions)
{
	NOS_BIND_NODE_CLASS(NOS_NAME_STATIC("DMAWrite"), DMAWriteNode, functions)
	return NOS_RESULT_SUCCESS;
}
}