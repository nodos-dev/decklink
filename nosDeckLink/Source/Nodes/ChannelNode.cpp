// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#include <Nodos/PluginHelpers.hpp>

#include <nosDeckLinkDevice/nosDeckLinkDevice.h>

namespace nos::decklink
{

NOS_REGISTER_NAME(Device);
NOS_REGISTER_NAME(ChannelName);
NOS_REGISTER_NAME(IsInput);
NOS_REGISTER_NAME(Resolution);
NOS_REGISTER_NAME(FrameRate);
NOS_REGISTER_NAME(IsInterlaced);
NOS_REGISTER_NAME(IsOpen);
NOS_REGISTER_NAME(FrameBufferFormat);

enum class ChangedPinType
{
	IsInput,
	Device,
	ChannelName,
	Resolution,
	FrameRate
};

class ChannelNode : public nos::NodeContext
{
public:
    ChannelNode(const nosFbNode* node) : NodeContext(node)
    {
		SetPinVisualizer(NSN_Device, {.type = nos::fb::VisualizerType::COMBO_BOX, .name = GetDeviceStringListName()});
		SetPinVisualizer(NSN_ChannelName, {.type = nos::fb::VisualizerType::COMBO_BOX, .name = GetChannelStringListName()});
		SetPinVisualizer(NSN_Resolution, {.type = nos::fb::VisualizerType::COMBO_BOX, .name = GetResolutionStringListName()});
		SetPinVisualizer(NSN_FrameRate, {.type = nos::fb::VisualizerType::COMBO_BOX, .name = GetFrameRateStringListName()});
		SetPinVisualizer(NSN_IsInterlaced, {.type = nos::fb::VisualizerType::COMBO_BOX, .name = GetInterlacedStringListName()});

		UpdateStringList(GetDeviceStringListName(), GetPossibleDeviceNames());

		AddPinValueWatcher(NSN_IsOpen, [this](const nos::Buffer& newVal, std::optional<nos::Buffer> oldValue) {
	
		});
		AddPinValueWatcher(NSN_IsInput, [this](const nos::Buffer& newVal, std::optional<nos::Buffer> oldValue) {

		});
		AddPinValueWatcher(NSN_Device, [this](const nos::Buffer& newVal, std::optional<nos::Buffer> oldValue) {
			
		});
		AddPinValueWatcher(NSN_ChannelName, [this](const nos::Buffer& newVal, std::optional<nos::Buffer> oldValue) {
			
		});
		AddPinValueWatcher(NSN_Resolution, [this](const nos::Buffer& newVal, std::optional<nos::Buffer> oldValue) {
			
		});
		AddPinValueWatcher(NSN_FrameRate, [this](const nos::Buffer& newVal, std::optional<nos::Buffer> oldValue) {
			
		});
		AddPinValueWatcher(NSN_IsInterlaced, [this](const nos::Buffer& newVal, std::optional<nos::Buffer> oldValue) {

		});
		AddPinValueWatcher(NSN_FrameBufferFormat, [this](const nos::Buffer& newVal, std::optional<nos::Buffer> oldValue) {
			
		});
    }

	void AutoSelectIfSingle(nosName pinName, std::vector<std::string> const& list)
	{
		if (list.size() == 2)
			SetPinValue(pinName, nosBuffer{.Data = (void*)list[1].c_str(), .Size = list[1].size() + 1});
	}

	void UpdateAfter(ChangedPinType pin, bool first)
	{
		switch (pin)
		{
		case ChangedPinType::IsInput: {
			ChangePinReadOnly(NSN_Resolution, IsInput);
			ChangePinReadOnly(NSN_FrameRate, IsInput);
			ChangePinReadOnly(NSN_IsInterlaced, IsInput);
			// ChangePinReadOnly(NSN_ReferenceSource, IsInput);

			auto deviceList = GetPossibleDeviceNames();
			UpdateStringList(GetDeviceStringListName(), deviceList);

			if (!first)
				AutoSelectIfSingle(NSN_Device, deviceList);
			break;
		}
		case ChangedPinType::Device: {
			auto channelList = GetPossibleChannelNames();
			UpdateStringList(GetChannelStringListName(), channelList);
			if (!first)
				AutoSelectIfSingle(NSN_ChannelName, channelList);
			// if (IsInput || !Device)
			// {
			// 	UpdateStringList(GetReferenceStringListName(), { "NONE" });
			// 	SetPinValue(NSN_ReferenceSource, nosBuffer{ .Data = (void*)"NONE", .Size = 5 });
			// }
			// else
			// {
			// 	std::vector<std::string> list{"Reference In", "Free Run"};
			// 	for (int i = 1; i <= NTV2DeviceGetNumVideoInputs(Device->ID); ++i)
			// 		list.push_back("SDI In " + std::to_string(i));
			// 	UpdateStringList(GetReferenceStringListName(), list);
			// 	if (!first && Device->GetReference(ReferenceSource))
			// 	{
			// 		auto refStr = NTV2ReferenceSourceToString(ReferenceSource, true);
			// 		SetPinValue(NSN_ReferenceSource, nosBuffer{ .Data = (void*)refStr.c_str(), .Size = refStr.size() + 1 });
			// 	}
			// }
			break;
		}
		case ChangedPinType::ChannelName: {
			// if (IsInput)
			// 	TryUpdateChannel();
			auto resolutionList = GetPossibleResolutions();
			UpdateStringList(GetResolutionStringListName(), resolutionList);
			if(!IsInput && !first)
				AutoSelectIfSingle(NSN_Resolution, resolutionList);
			break;
		}
		case ChangedPinType::Resolution: {
			auto frameRateList = GetPossibleFrameRates();
			UpdateStringList(GetFrameRateStringListName(), frameRateList);
			if (!first)
				AutoSelectIfSingle(NSN_FrameRate, frameRateList);
			break;
		}
		case ChangedPinType::FrameRate: {
			auto interlacedList = GetPossibleInterlaced();
			UpdateStringList(GetInterlacedStringListName(), interlacedList);
			if (!first)
				AutoSelectIfSingle(NSN_IsInterlaced, interlacedList);
			break;
		}
		}
	}

	void ResetAfter(ChangedPinType pin)
	{
		// CurrentChannel.Update({}, true);
		nos::Name pinToSet;
		switch (pin)
		{
		case ChangedPinType::IsInput: 
			pinToSet = NSN_Device;
			break;
		case ChangedPinType::Device: 
			pinToSet = NSN_ChannelName; 
			break;
		case ChangedPinType::ChannelName: 
			pinToSet = NSN_Resolution; 
			break;
		case ChangedPinType::Resolution: 
			pinToSet = NSN_FrameRate; 
			break;
		case ChangedPinType::FrameRate: 
			pinToSet = NSN_IsInterlaced; 
			break;
		}
		SetPinValue(pinToSet, nosBuffer{.Data = (void*)"NONE", .Size = 5});
	}

	void ResetDevicePin()
	{ 
		SetPinValue(NSN_Device, nosBuffer{.Data = (void*)"NONE", .Size = 5});
	}
	void ResetChannelNamePin()
	{
		SetPinValue(NSN_ChannelName, nosBuffer{.Data = (void*)"NONE", .Size = 5});
	}
	void ResetResolutionPin()
	{
		SetPinValue(NSN_Resolution, nosBuffer{.Data = (void*)"NONE", .Size = 5});
	}
	void ResetFrameRatePin()
	{ 
		SetPinValue(NSN_FrameRate, nosBuffer{.Data = (void*)"NONE", .Size = 5});
	}
	void ResetInterlacedPin()
	{ 
		SetPinValue(NSN_IsInterlaced, nosBuffer{.Data = (void*)"NONE", .Size = 5});
	}

    nosResult ExecuteNode(nosNodeExecuteParams* params) override
    {
        return NOS_RESULT_SUCCESS;
    }
	
	std::string GetDeviceStringListName() { return "decklink.DeviceList." + UUID2STR(NodeId); }
	std::string GetChannelStringListName() { return "decklink.ChannelList." + UUID2STR(NodeId); }
	std::string GetResolutionStringListName() { return "decklink.ResolutionList." + UUID2STR(NodeId); }
	std::string GetFrameRateStringListName() { return "decklink.FrameRateList." + UUID2STR(NodeId); }
	std::string GetInterlacedStringListName() { return "decklink.InterlacedList." + UUID2STR(NodeId); }

	std::vector<std::string> GetPossibleDeviceNames()
    {
    	std::vector<std::string> devices = {"NONE"};
    	for (auto& device : Device::GetDevices())
    	{
    		devices.push_back(device->GetUniqueDisplayName());
    	}
    	return devices;
    }
	
	std::vector<std::string> GetPossibleChannelNames() 
	{
		std::vector<std::string> channels = {"NONE"};
		if (!Device)
			return channels;
		return channels;
	}
	
	std::vector<std::string> GetPossibleResolutions() 
	{
		std::vector<std::string> possibleResolutions = {"NONE"};
		return possibleResolutions;
	}
	
	std::vector<std::string> GetPossibleFrameRates() 
	{
		std::vector<std::string> possibleFrameRates = {"NONE"};
		return possibleFrameRates;
	}

	std::vector<std::string> GetPossibleInterlaced()
	{
		std::vector<std::string> possibleInterlaced = {"NONE"};
		return possibleInterlaced;
	}

	bool ShouldOpen = false;
	bool IsInput = false;
	bool ForceInterlaced = false;
	std::string DevicePinValue = "NONE";
	std::string ChannelPinValue = "NONE";
	std::string ResolutionPinValue = "NONE";
	std::string FrameRatePinValue = "NONE";
	std::string InterlacedPinValue = "NONE";
	std::string ReferenceSourcePinValue = "NONE";

	Device* Device{};
	enum class InterlacedState
	{
		NONE,
		INTERLACED,
		PROGRESSIVE
	} InterlacedState = InterlacedState::NONE;
};

nosResult RegisterChannelNode(nosNodeFunctions* funcs)
{
	NOS_BIND_NODE_CLASS(NOS_NAME_STATIC("Channel"), ChannelNode, funcs)
    return NOS_RESULT_SUCCESS;
}
}