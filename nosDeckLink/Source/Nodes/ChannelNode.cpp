// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#include <Nodos/PluginHelpers.hpp>

#include <nosDeckLinkDevice/nosDeckLinkDevice.h>

#include "Generated/DeckLink_generated.h"

namespace nos::decklink
{

NOS_REGISTER_NAME(Device);
NOS_REGISTER_NAME(ChannelName);
NOS_REGISTER_NAME(IsInput);
NOS_REGISTER_NAME(Resolution);
NOS_REGISTER_NAME(FrameRate);
NOS_REGISTER_NAME(IsOpen);
NOS_REGISTER_NAME(ChannelId);

enum class ChangedPinType
{
	IsInput,
	Device,
	ChannelName,
	Resolution,
	FrameRate
};

enum class ChannelUpdateResult
{
	NothingChanged,
	UnsupportedSettings,
	Opened,
};
	
struct ChannelHandler
{
	bool IsOpen = false;
	nosUUID OutChannelPinId;
	int32_t DeviceIndex = -1;
	nosMediaIODirection Direction = NOS_MEDIAIO_DIRECTION_OUTPUT;
	nosDeckLinkChannel Channel = NOS_DECKLINK_CHANNEL_INVALID;
	nosMediaIOFrameGeometry Resolution = NOS_MEDIAIO_FG_INVALID;
	nosMediaIOFrameRate FrameRate = NOS_MEDIAIO_FRAME_RATE_INVALID;

	template<auto Member, typename T>
	ChannelUpdateResult Update(const T& value)
	{
		if (this->*Member == value)
			return ChannelUpdateResult::NothingChanged;
		if (IsOpen)
			Close();
		this->*Member = value;
		if (!Open())
			return ChannelUpdateResult::UnsupportedSettings;
		return ChannelUpdateResult::Opened;
	}
	
	void Close()
	{
		nosDeckLink->CloseChannel(DeviceIndex, Channel);
		IsOpen = false;
		nosEngine.SetPinValue(OutChannelPinId, nos::Buffer::From(ChannelId{}));
	}
	
	bool Open()
	{
		if (DeviceIndex == -1)
			return false;
		if (Channel == NOS_DECKLINK_CHANNEL_INVALID)
			return false;
		if (IsOpen)
			return true;
		nosDeckLinkOpenChannelParams params {
			.Direction = Direction,
			.Channel = Channel,
			.PixelFormat = NOS_MEDIAIO_PIXEL_FORMAT_YCBCR_8BIT,
			.Output = {}
		};
		if (Direction == NOS_MEDIAIO_DIRECTION_OUTPUT)
		{
			params.Output.Geometry = Resolution;
			params.Output.FrameRate = FrameRate;
		}
		auto res = nosDeckLink->OpenChannel(DeviceIndex, &params);
		if (res != NOS_RESULT_SUCCESS)
			return false;
		IsOpen = true;
		ChannelId id(true, DeviceIndex, Channel, Direction);
		nosEngine.SetPinValue(OutChannelPinId, nos::Buffer::From(id));
		return true;
	}
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

		Channel.OutChannelPinId = *GetPinId(NSN_ChannelId);

		UpdateStringList(GetDeviceStringListName(), GetPossibleDeviceNames());

		AddPinValueWatcher(NSN_IsOpen, [this](const nos::Buffer& newVal, std::optional<nos::Buffer> oldValue) {
			ShouldOpen = *InterpretPinValue<bool>(newVal);
		});
		AddPinValueWatcher(NSN_IsInput, [this](const nos::Buffer& newVal, std::optional<nos::Buffer> oldValue) {
			
		});
		AddPinValueWatcher(NSN_Device, [this](const nos::Buffer& newVal, std::optional<nos::Buffer> oldValue) {
			DevicePinValue = InterpretPinValue<const char>(newVal);
			uint32_t newDeviceIndex = 0;
			if (NOS_RESULT_SUCCESS != nosDeckLink->GetDeviceByUniqueDisplayName(DevicePinValue.c_str(), &newDeviceIndex))
				newDeviceIndex = -1;
			Channel.Update<&ChannelHandler::DeviceIndex>(newDeviceIndex);
			if (DevicePinValue != "NONE" && Channel.DeviceIndex == -1)
				SetPinValue(NSN_Device, nosBuffer{ .Data = (void*)"NONE", .Size = 5 });
			else
			{
				if (oldValue)
					ResetAfter(ChangedPinType::Device);
				else if (DevicePinValue == "NONE")
					AutoSelectIfSingle(NSN_Device, GetPossibleDeviceNames());
			}
			UpdateAfter(ChangedPinType::Device, !oldValue);
		});
		AddPinValueWatcher(NSN_ChannelName, [this](const nos::Buffer& newVal, std::optional<nos::Buffer> oldValue) {
			ChannelPinValue = InterpretPinValue<const char>(newVal);
			auto newChannel = nosDeckLink->GetChannelByName(ChannelPinValue.c_str());
			Channel.Update<&ChannelHandler::Channel>(newChannel);
			if (ChannelPinValue != "NONE" && newChannel == NOS_DECKLINK_CHANNEL_INVALID)
				SetPinValue(NSN_ChannelName, nosBuffer{.Data = (void*)"NONE", .Size = 5});
			else
			{
				if (oldValue)
					ResetAfter(ChangedPinType::ChannelName);
				else if (ChannelPinValue == "NONE")
					AutoSelectIfSingle(NSN_ChannelName, GetPossibleChannelNames());
			}
			UpdateAfter(ChangedPinType::ChannelName, !oldValue);
			
		});
		AddPinValueWatcher(NSN_Resolution, [this](const nos::Buffer& newVal, std::optional<nos::Buffer> oldValue) {
			ResolutionPinValue = InterpretPinValue<const char>(newVal);
			auto newResolution = nosMediaIO->GetFrameGeometryFromString(ResolutionPinValue.c_str());
			Channel.Update<&ChannelHandler::Resolution>(newResolution);
			if (ResolutionPinValue != "NONE" && newResolution == NOS_MEDIAIO_FG_INVALID)
				SetPinValue(NSN_Resolution, nosBuffer{.Data = (void*)"NONE", .Size = 5});
			else
			{
				if (oldValue)
					ResetAfter(ChangedPinType::Resolution);
				else if (ResolutionPinValue == "NONE")
					AutoSelectIfSingle(NSN_Resolution, GetPossibleResolutions());
			}
			UpdateAfter(ChangedPinType::Resolution, !oldValue);
		});
		AddPinValueWatcher(NSN_FrameRate, [this](const nos::Buffer& newVal, std::optional<nos::Buffer> oldValue) {
			FrameRatePinValue = InterpretPinValue<const char>(newVal);
			auto newFrameRate = nosMediaIO->GetFrameRateFromString(FrameRatePinValue.c_str());
			Channel.Update<&ChannelHandler::FrameRate>(newFrameRate);
			if (FrameRatePinValue != "NONE" && newFrameRate == NOS_MEDIAIO_FRAME_RATE_INVALID)
				SetPinValue(NSN_FrameRate, nosBuffer{.Data = (void*)"NONE", .Size = 5});
			else
			{
				if (oldValue)
					ResetAfter(ChangedPinType::FrameRate);
				else if (FrameRatePinValue == "NONE")
					AutoSelectIfSingle(NSN_FrameRate, GetPossibleFrameRates());
			}
			UpdateAfter(ChangedPinType::FrameRate, !oldValue);
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

	nosResult ExecuteNode(nosNodeExecuteParams* params) override
	{
		return NOS_RESULT_SUCCESS;
	}
	
	std::string GetDeviceStringListName() { return "decklink.DeviceList." + UUID2STR(NodeId); }
	std::string GetChannelStringListName() { return "decklink.ChannelList." + UUID2STR(NodeId); }
	std::string GetResolutionStringListName() { return "decklink.ResolutionList." + UUID2STR(NodeId); }
	std::string GetFrameRateStringListName() { return "decklink.FrameRateList." + UUID2STR(NodeId); }

	std::vector<std::string> GetPossibleDeviceNames()
	{
		std::vector<std::string> devices = {"NONE"};
		size_t count = 0;
		nosDeckLink->GetDevices(&count, nullptr);
		std::vector<nosDeckLinkDeviceDesc> deviceDescs(count);
		nosDeckLink->GetDevices(&count, deviceDescs.data());
		for (auto& deviceDesc : deviceDescs)
		{
			devices.push_back(deviceDesc.UniqueDisplayName);
		}
		return devices;
	}
	std::vector<std::string> GetPossibleChannelNames() 
	{
		std::vector<std::string> channels = {"NONE"};
		if (Channel.DeviceIndex == -1)
			return channels;
		nosDeckLinkChannelList channelList{};
		nosDeckLink->GetAvailableChannels(Channel.DeviceIndex, IsInput ? NOS_MEDIAIO_DIRECTION_INPUT : NOS_MEDIAIO_DIRECTION_OUTPUT, &channelList);
		for (size_t i = 0; i < channelList.Count; i++)
		{
			auto channelName = nosDeckLink->GetChannelName(channelList.Channels[i]);
			channels.push_back(channelName);
		}
		return channels;
	}
	
	std::vector<std::string> GetPossibleResolutions() 
	{
		std::vector<std::string> possibleResolutions = {"NONE"};
		if (Channel.DeviceIndex == -1 || Channel.Channel == NOS_DECKLINK_CHANNEL_INVALID)
			return possibleResolutions;
		nosMediaIOFrameGeometryList frameGeometryList{};
		nosDeckLink->GetSupportedOutputFrameGeometries(Channel.DeviceIndex, Channel.Channel, &frameGeometryList);
		for (size_t i = 0; i < frameGeometryList.Count; i++)
		{
			auto& fg = frameGeometryList.Geometries[i];
			auto name = nosMediaIO->GetFrameGeometryName(fg);
			possibleResolutions.push_back(name);
		}
		return possibleResolutions;
	}
	
	std::vector<std::string> GetPossibleFrameRates() 
	{
		std::vector<std::string> possibleFrameRates = {"NONE"};
		if (Channel.DeviceIndex == -1 || Channel.Channel == NOS_DECKLINK_CHANNEL_INVALID || Channel.Resolution == NOS_MEDIAIO_FG_INVALID)
			return possibleFrameRates;
		nosMediaIOFrameRateList frameRates{};
		nosDeckLink->GetSupportedOutputFrameRatesForGeometry(Channel.DeviceIndex, Channel.Channel, Channel.Resolution, &frameRates);
		for (size_t i = 0; i < frameRates.Count; i++)
		{
			auto& rate = frameRates.FrameRates[i];
			auto name = nosMediaIO->GetFrameRateName(rate);
			possibleFrameRates.push_back(name);
		}
		return possibleFrameRates;
	}

	bool ShouldOpen = false;
	bool IsInput = false;
	std::string DevicePinValue = "NONE";
	std::string ChannelPinValue = "NONE";
	std::string ResolutionPinValue = "NONE";
	std::string FrameRatePinValue = "NONE";

	ChannelHandler Channel;
};

nosResult RegisterChannelNode(nosNodeFunctions* funcs)
{
	NOS_BIND_NODE_CLASS(NOS_NAME_STATIC("Channel"), ChannelNode, funcs)
	return NOS_RESULT_SUCCESS;
}
}
