// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/PluginHelpers.hpp>
#include <nosVulkanSubsystem/nosVulkanSubsystem.h>
#include <nosDeckLinkDevice/nosDeckLinkDevice.h>
#include <nosMediaIO/nosMediaIO.h>

NOS_INIT_WITH_MIN_REQUIRED_MINOR(0)
NOS_VULKAN_INIT()
NOS_DECKLINK_DEVICE_SUBSYSTEM_INIT()
NOS_MEDIAIO_SUBSYSTEM_INIT()

NOS_BEGIN_IMPORT_DEPS()
	NOS_DECKLINK_DEVICE_SUBSYSTEM_IMPORT()
	NOS_VULKAN_IMPORT()
	NOS_MEDIAIO_SUBSYSTEM_IMPORT()
NOS_END_IMPORT_DEPS()

namespace nos::decklink
{
enum class Nodes : int
{
	DMAWrite,
	DMARead,
	WaitFrame,
	Channel,
	Count
};

nosResult RegisterDMAWriteNode(nosNodeFunctions*);
nosResult RegisterDMAReadNode(nosNodeFunctions*);
nosResult RegisterWaitFrameNode(nosNodeFunctions*);
nosResult RegisterChannelNode(nosNodeFunctions*);

struct DeckLinkPluginFunctions : nos::PluginFunctions
{
	using PluginFunctions::PluginFunctions;
	nosResult ExportNodeFunctions(size_t& outSize, nosNodeFunctions** outList) override
	{
		outSize = static_cast<size_t>(Nodes::Count);
		if (!outList)
			return NOS_RESULT_SUCCESS;

		NOS_RETURN_ON_FAILURE(RegisterDMAWriteNode(outList[(int)Nodes::DMAWrite]))
		NOS_RETURN_ON_FAILURE(RegisterWaitFrameNode(outList[(int)Nodes::WaitFrame]))
		NOS_RETURN_ON_FAILURE(RegisterChannelNode(outList[(int)Nodes::Channel]))
		NOS_RETURN_ON_FAILURE(RegisterDMAReadNode(outList[(int)Nodes::DMARead]))
		return NOS_RESULT_SUCCESS;
	}
};
NOS_EXPORT_PLUGIN_FUNCTIONS(DeckLinkPluginFunctions)
} // namespace nos::aja
