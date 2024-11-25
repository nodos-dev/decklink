#include <Nodos/PluginHelpers.hpp>

#include <nosVulkanSubsystem/nosVulkanSubsystem.h>
#include <nosVulkanSubsystem/Helpers.hpp>

#include "Device/DeckLinkDevice.hpp"

#include <nosUtil/Stopwatch.hpp>

namespace nos::decklink
{
	
struct DMAWriteNodeContext : NodeContext
{
	DMAWriteNodeContext(const nosFbNode* node) : NodeContext(node)
	{
		Device = SubDevice::GetDevice(0);
		if (!Device->OpenOutput(bmdModeHD1080p5994, bmdFormat8BitYUV))
			nosEngine.LogE("Unable to open output for device: %s", Device->ModelName.c_str());
	}

	void GetScheduleInfo(nosScheduleInfo* out) override
	{
		*out = nosScheduleInfo{
			.Importance = 1,
			.DeltaSeconds = Device->GetDeltaSeconds(),
			.Type = NOS_SCHEDULE_TYPE_ON_DEMAND,
		};
	}

	nosResult ExecuteNode(nosNodeExecuteParams* params) override
	{
		nosResourceShareInfo inputBuffer{};
		auto fieldType = nos::sys::vulkan::FieldType::UNKNOWN;
		uint32_t curVBLCount = 0;
		for (size_t i = 0; i < params->PinCount; ++i)
		{
			auto& pin = params->Pins[i];
			if (pin.Name == NOS_NAME_STATIC("Input"))
				inputBuffer = vkss::ConvertToResourceInfo(*InterpretPinValue<sys::vulkan::Buffer>(*pin.Data));
			if (pin.Name == NOS_NAME("FieldType"))
				fieldType = *InterpretPinValue<sys::vulkan::FieldType>(*pin.Data);
			if (pin.Name == NOS_NAME("CurrentVBL"))
				curVBLCount = *InterpretPinValue<uint32_t>(*pin.Data);
		}

		if (!inputBuffer.Memory.Handle)
			return NOS_RESULT_FAILED;

		auto buffer = nosVulkan->Map(&inputBuffer); {
			util::Stopwatch sw;
			Device->WaitFrameCompletion();
			nosEngine.WatchLog(("DeckLink " + Device->Handle + " Wait Time").c_str(), sw.ElapsedString().c_str());
		}
		Device->DmaWrite(buffer, inputBuffer.Info.Buffer.Size);
		Device->ScheduleNextFrame();

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

	SubDevice* Device = nullptr;
};

nosResult RegisterDMAWriteNode(nosNodeFunctions* functions)
{
	NOS_BIND_NODE_CLASS(NOS_NAME_STATIC("DMAWrite"), DMAWriteNodeContext, functions)
	return NOS_RESULT_SUCCESS;
}
}