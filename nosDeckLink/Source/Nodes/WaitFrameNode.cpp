// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#include <Nodos/PluginHelpers.hpp>

#include "Device/DeckLinkDevice.hpp"

#include <nosUtil/Stopwatch.hpp>

namespace nos::decklink
{
	
struct WaitFrameNode : NodeContext
{
	WaitFrameNode(const nosFbNode* node) : NodeContext(node)
	{
	}

	nosResult ExecuteNode(nosNodeExecuteParams* params) override
	{
		{
			util::Stopwatch sw;
			Device->WaitFrameCompletion();
			nosEngine.WatchLog(("DeckLink " + Device->Handle + " Wait Time").c_str(), sw.ElapsedString().c_str());
		}

		nosEngine.SetPinValue(GetPin(NOS_NAME("SubDeviceHandle"))->Id,  nos::Buffer(Device->Handle.c_str(), Device->Handle.size() + 1));
		
		return NOS_RESULT_SUCCESS;
	}

	SubDevice* Device = nullptr;
};

nosResult RegisterWaitFrameNode(nosNodeFunctions* functions)
{
	NOS_BIND_NODE_CLASS(NOS_NAME_STATIC("WaitFrame"), WaitFrameNode, functions)
	return NOS_RESULT_SUCCESS;
}
}