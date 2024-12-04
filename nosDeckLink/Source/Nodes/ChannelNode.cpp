// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#include <Nodos/PluginHelpers.hpp>

#include <nosDeckLinkSubsystem/nosDeckLinkSubsystem.h>

#include "Generated/Conversion_generated.h"
#include "Generated/DeckLink_generated.h"

namespace nos::decklink
{

NOS_REGISTER_NAME(Device);
NOS_REGISTER_NAME(ChannelName);
NOS_REGISTER_NAME(IsInput);
NOS_REGISTER_NAME(Resolution);
NOS_REGISTER_NAME(FrameRate);
NOS_REGISTER_NAME(PixelFormat);
NOS_REGISTER_NAME(IsOpen);
NOS_REGISTER_NAME(ChannelId);
NOS_REGISTER_NAME(ChannelResolution);
NOS_REGISTER_NAME(ChannelPixelFormat);

enum class ChangedPinType
{
	IsInput,
	Device,
	ChannelName,
	Resolution,
	FrameRate,
	PixelFormat,
};

enum class ChannelUpdateResult
{
	NothingChanged,
	UnsupportedSettings,
	Opened,
};

void InputVideoFormatChanged(void* userData, nosMediaIOFrameGeometry frameGeometry, nosMediaIOFrameRate frameRate, nosMediaIOPixelFormat pixelFormat);
void FrameResultCallback(void* userData, nosDeckLinkFrameResult result, uint32_t processedFrameNumber);
	
struct ChannelHandler
{
	class ChannelNode* Node;
	bool ShouldOpen = true;
	bool IsOpen = false;
	bool IsStreamStarted = false;
	nosUUID OutChannelPinId;
	nosUUID OutResolutionPinId;
	nosUUID OutPixelFormatPinId;
	nosUUID ResolutionPinId;
	nosUUID FrameRatePinId;
	nosUUID PixelFormatPinId; 
	int32_t DeviceIndex = -1;
	nosMediaIODirection Direction = NOS_MEDIAIO_DIRECTION_OUTPUT;
	nosDeckLinkChannel Channel = NOS_DECKLINK_CHANNEL_INVALID;
	nosMediaIOFrameGeometry Resolution = NOS_MEDIAIO_FRAME_GEOMETRY_INVALID;
	nosMediaIOFrameRate FrameRate = NOS_MEDIAIO_FRAME_RATE_INVALID;
	nosMediaIOPixelFormat PixelFormat = NOS_MEDIAIO_PIXEL_FORMAT_INVALID;
	int32_t VideoInputChangeCallbackId = -1;
	int32_t FrameResultCallbackId = -1;

	struct
	{
		uint32_t DropCount = 0;
		uint32_t FramesSinceLastDrop = 0;
		bool DropDetected = false;
	} DeckLinkThread;

	template<auto Member, typename T>
	ChannelUpdateResult Update(const T& value, bool reopen = true)
	{
		if (this->*Member == value)
			return ChannelUpdateResult::NothingChanged;
		if (reopen && IsOpen)
			Close();
		this->*Member = value;
		UpdateChannelStatusAndOutPins();
		if (reopen && !Open())
			return ChannelUpdateResult::UnsupportedSettings;
		return ChannelUpdateResult::Opened;
	}

	ChannelHandler(ChannelNode* node) : Node(node)
	{
		UpdateChannelStatus();
	}

	~ChannelHandler()
	{
		Close();
	}

	void OnInputVideoFormatChanged_DeckLinkThread(nosMediaIOFrameGeometry frameGeometry, nosMediaIOFrameRate frameRate, nosMediaIOPixelFormat pixelFormat)
	{
		const char* frameGeometryCstr = nosMediaIO->GetFrameGeometryName(frameGeometry);
		const char* frameRateCstr = nosMediaIO->GetFrameRateName(frameRate);
		const char* pixelFormatCstr = nosMediaIO->GetPixelFormatName(pixelFormat);
		Resolution = frameGeometry;
		FrameRate = frameRate;
		PixelFormat = pixelFormat;
		nosEngine.SetPinValue(ResolutionPinId, nos::Buffer(frameGeometryCstr, strlen(frameGeometryCstr) + 1));
		nosEngine.SetPinValue(FrameRatePinId, nos::Buffer(frameRateCstr, strlen(frameRateCstr) + 1));
		nosEngine.SetPinValue(PixelFormatPinId, nos::Buffer(pixelFormatCstr, strlen(pixelFormatCstr) + 1));
		UpdateChannelStatus();
	}

	void OnFrameEnd_DeckLinkThread(nosDeckLinkFrameResult result, uint32_t processedFrameNumber)
	{
		switch (result)
		{
		case NOS_DECKLINK_FRAME_DROPPED:
		{
			++DeckLinkThread.DropCount;
			DeckLinkThread.FramesSinceLastDrop = 0;
			DeckLinkThread.DropDetected = true;
			SetStatus(StatusType::DropCount, fb::NodeStatusMessageType::WARNING, "Drop Count: " + std::to_string(DeckLinkThread.DropCount));
			UpdateStatus();
			break;
		}
		case NOS_DECKLINK_FRAME_COMPLETED:
		{
			if (DeckLinkThread.DropDetected)
				++DeckLinkThread.FramesSinceLastDrop;
			if (DeckLinkThread.FramesSinceLastDrop > 50)
			{
				nosEngine.LogW("Requesting path restart due to frame drops");
				nosEngine.SendPathRestart(OutChannelPinId);
			}
			break;
		}
		}
	}

	bool CanOpen()
	{
		if (!ShouldOpen || DeviceIndex == -1 || Channel == NOS_DECKLINK_CHANNEL_INVALID)
			return false;
		if (Direction == NOS_MEDIAIO_DIRECTION_OUTPUT)
			return Resolution != NOS_MEDIAIO_FRAME_GEOMETRY_INVALID && FrameRate != NOS_MEDIAIO_FRAME_RATE_INVALID && PixelFormat != NOS_MEDIAIO_PIXEL_FORMAT_INVALID;
		return true;
	}
	
	bool Open()
	{
		if (IsOpen)
			return true;
		if (!CanOpen())
			return false;
		nosDeckLinkOpenChannelParams params {
			.Direction = Direction,
			.Channel = Channel,
			.PixelFormat = Direction == NOS_MEDIAIO_DIRECTION_INPUT ? NOS_MEDIAIO_PIXEL_FORMAT_YCBCR_8BIT : PixelFormat,
			.Output = {}
		};
		if (Direction == NOS_MEDIAIO_DIRECTION_OUTPUT)
		{
			params.Output.Geometry = Resolution;
			params.Output.FrameRate = FrameRate;
		}
		else
		{
			VideoInputChangeCallbackId = nosDeckLink->RegisterInputVideoFormatChangeCallback(DeviceIndex, Channel, &InputVideoFormatChanged, this);
		}
		auto res = nosDeckLink->OpenChannel(DeviceIndex, &params);
		if (res == NOS_RESULT_SUCCESS)
		{
			IsOpen = true;
			ChannelId id(DeviceIndex, Channel, Direction);
			nosEngine.SetPinValue(OutChannelPinId, nos::Buffer::From(id));
			nosEngine.SendPathRestart(OutChannelPinId);
			FrameResultCallbackId = nosDeckLink->RegisterFrameResultCallback(DeviceIndex, Channel, &FrameResultCallback, this);
		}
		UpdateChannelStatusAndOutPins();
		return res == NOS_RESULT_SUCCESS;
	}

	void StartIfOpen()
	{
		if (IsOpen)
			nosDeckLink->StartStream(DeviceIndex, Channel);
	}

	void StopIfOpen()
	{
		if (IsOpen)
		{
			nosDeckLink->StopStream(DeviceIndex, Channel);
			ClearStatus(StatusType::DropCount);
			DeckLinkThread = {};
		}
	}

	void Close()
	{
		if (Direction == NOS_MEDIAIO_DIRECTION_INPUT)
			nosDeckLink->UnregisterInputVideoFormatChangeCallback(DeviceIndex, Channel, VideoInputChangeCallbackId);
		nosDeckLink->UnregisterFrameResultCallback(DeviceIndex, Channel, FrameResultCallbackId);
		nosDeckLink->CloseChannel(DeviceIndex, Channel);
		IsOpen = false;
		nosEngine.SetPinValue(OutChannelPinId, nos::Buffer::From(ChannelId(-1, 0, false)));
		nosEngine.SetPinValue(OutResolutionPinId, nos::Buffer::From(nosVec2u{ 0, 0 }));
		nosEngine.SendPathRestart(OutChannelPinId);
		auto [type, text] = GetChannelStatusString();
		SetStatus(StatusType::Channel, type, text);
		UpdateStatus();
	}

	std::pair<fb::NodeStatusMessageType, std::string> GetChannelStatusString()
	{
		std::string channelString = nosDeckLink->GetChannelName(Channel);
		channelString += " ";
		if (Resolution != NOS_MEDIAIO_FRAME_GEOMETRY_INVALID)
		{
			channelString += nosMediaIO->GetFrameGeometryName(Resolution);
			channelString += " ";
		}
		if (FrameRate != NOS_MEDIAIO_FRAME_RATE_INVALID)
			channelString += nosMediaIO->GetFrameRateName(FrameRate);
		if (ShouldOpen && IsOpen)
		{
			return {fb::NodeStatusMessageType::INFO, channelString};
		}
		if (ShouldOpen && !IsOpen && CanOpen())
		{
			return {fb::NodeStatusMessageType::FAILURE, "Failed to open: " + channelString};
		}
		if (!ShouldOpen)
		{
			return {fb::NodeStatusMessageType::WARNING, "Channel closed"};
		}
		return { fb::NodeStatusMessageType::WARNING, "Idle" };
	}

	void UpdateChannelStatus()
	{
		auto [type, text] = GetChannelStatusString();
		SetStatus(StatusType::Channel, type, text);
		UpdateStatus();
	}

	void UpdateOutPins()
	{
		nosVec2u resolution{};
		nosMediaIO->Get2DFrameResolution(Resolution, &resolution);
		nosEngine.SetPinValue(OutResolutionPinId, nos::Buffer::From(resolution));
		nos::mediaio::YCbCrPixelFormat ycbcrFormat{};
		switch (PixelFormat)
		{
		case NOS_MEDIAIO_PIXEL_FORMAT_YCBCR_8BIT:
			ycbcrFormat = nos::mediaio::YCbCrPixelFormat::YUV8;
			break;
		case NOS_MEDIAIO_PIXEL_FORMAT_YCBCR_10BIT:
			ycbcrFormat = nos::mediaio::YCbCrPixelFormat::V210;
			break;
		}
		nosEngine.SetPinValue(OutPixelFormatPinId, nos::Buffer::From(ycbcrFormat));
	}

	void UpdateChannelStatusAndOutPins()
	{
		UpdateChannelStatus();
		UpdateOutPins();
	}
	
	void UpdateStatus();

	enum class StatusType
	{
		Channel,
		Reference,
		ReferenceInvalid,
		DeltaSecondsCompatible,
		Firmware,
		DropCount,
	};

	void SetStatus(StatusType statusType, fb::NodeStatusMessageType msgType, std::string text);
	void ClearStatus(StatusType statusType);
	std::map<StatusType, fb::TNodeStatusMessage> StatusMessages;
};

void InputVideoFormatChanged(void* userData, nosMediaIOFrameGeometry frameGeometry, nosMediaIOFrameRate frameRate, nosMediaIOPixelFormat pixelFormat)
{
	static_cast<ChannelHandler*>(userData)->OnInputVideoFormatChanged_DeckLinkThread(frameGeometry, frameRate, pixelFormat);
}

void FrameResultCallback(void* userData, nosDeckLinkFrameResult result, uint32_t processedFrameNumber)
{
	static_cast<ChannelHandler*>(userData)->OnFrameEnd_DeckLinkThread(result, processedFrameNumber);
}


class ChannelNode : public nos::NodeContext
{
public:
	ChannelNode(const nosFbNode* node) : NodeContext(node), Channel(this)
	{
		SetPinVisualizer(NSN_Device, {.type = nos::fb::VisualizerType::COMBO_BOX, .name = GetDeviceStringListName()});
		SetPinVisualizer(NSN_ChannelName, {.type = nos::fb::VisualizerType::COMBO_BOX, .name = GetChannelStringListName()});
		SetPinVisualizer(NSN_Resolution, {.type = nos::fb::VisualizerType::COMBO_BOX, .name = GetResolutionStringListName()});
		SetPinVisualizer(NSN_FrameRate, {.type = nos::fb::VisualizerType::COMBO_BOX, .name = GetFrameRateStringListName()});
		SetPinVisualizer(NSN_PixelFormat, {.type = nos::fb::VisualizerType::COMBO_BOX, .name = GetPixelFormatStringListName()});

		Channel.OutChannelPinId = *GetPinId(NSN_ChannelId);
		Channel.OutResolutionPinId = *GetPinId(NSN_ChannelResolution);
		Channel.OutPixelFormatPinId = *GetPinId(NSN_ChannelPixelFormat);
		Channel.ResolutionPinId = *GetPinId(NSN_Resolution);
		Channel.FrameRatePinId = *GetPinId(NSN_FrameRate);
		Channel.PixelFormatPinId = *GetPinId(NSN_PixelFormat);

		UpdateStringList(GetDeviceStringListName(), GetPossibleDeviceNames());

		AddPinValueWatcher(NSN_IsOpen, [this](const nos::Buffer& newVal, std::optional<nos::Buffer> oldValue) {
			Channel.ShouldOpen = *InterpretPinValue<bool>(newVal);
			if (!Channel.ShouldOpen)
				Channel.Close();
			else
				Channel.Open();
		});
		AddPinValueWatcher(NSN_IsInput, [this](const nos::Buffer& newVal, std::optional<nos::Buffer> oldValue) {
			auto newValue = *InterpretPinValue<bool>(newVal) ? NOS_MEDIAIO_DIRECTION_INPUT : NOS_MEDIAIO_DIRECTION_OUTPUT;
			Channel.Update<&ChannelHandler::Direction>(newValue);
			UpdateAfter(ChangedPinType::IsInput, !oldValue);
		});
		AddPinValueWatcher(NSN_Device, [this](const nos::Buffer& newVal, std::optional<nos::Buffer> oldValue) {
			DevicePinValue = InterpretPinValue<const char>(newVal);
			uint32_t newDeviceIndex = 0;
			if (NOS_RESULT_SUCCESS != nosDeckLink->GetDeviceByUniqueDisplayName(DevicePinValue.c_str(), &newDeviceIndex))
				newDeviceIndex = -1;
			Channel.Update<&ChannelHandler::DeviceIndex>(newDeviceIndex);
			if (DevicePinValue != "NONE" && Channel.DeviceIndex == -1)
				ResetPin(NSN_Device);
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
				ResetPin(NSN_ChannelName);
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
			Channel.Update<&ChannelHandler::Resolution>(newResolution, Channel.Direction != NOS_MEDIAIO_DIRECTION_INPUT);
			if (ResolutionPinValue != "NONE" && newResolution == NOS_MEDIAIO_FRAME_GEOMETRY_INVALID)
				ResetPin(NSN_Resolution);
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
			Channel.Update<&ChannelHandler::FrameRate>(newFrameRate, Channel.Direction != NOS_MEDIAIO_DIRECTION_INPUT);
			if (FrameRatePinValue != "NONE" && newFrameRate == NOS_MEDIAIO_FRAME_RATE_INVALID)
				ResetPin(NSN_FrameRate);
			else
			{
				if (oldValue)
					ResetAfter(ChangedPinType::FrameRate);
				else if (FrameRatePinValue == "NONE")
					AutoSelectIfSingle(NSN_FrameRate, GetPossibleFrameRates());
			}
			UpdateAfter(ChangedPinType::FrameRate, !oldValue);
		});
		AddPinValueWatcher(NSN_PixelFormat, [this](const nos::Buffer& newVal, std::optional<nos::Buffer> oldValue) {
			PixelFormatPinValue = InterpretPinValue<const char>(newVal);
			auto newPixelFormat = nosMediaIO->GetPixelFormatFromString(PixelFormatPinValue.c_str());
			Channel.Update<&ChannelHandler::PixelFormat>(newPixelFormat, Channel.Direction != NOS_MEDIAIO_DIRECTION_INPUT);
			if (PixelFormatPinValue != "NONE" && newPixelFormat == NOS_MEDIAIO_PIXEL_FORMAT_INVALID)
				ResetPin(NSN_FrameRate);
			else
			{
				if (oldValue)
					ResetAfter(ChangedPinType::PixelFormat);
				else if (PixelFormatPinValue == "NONE")
					AutoSelectIfSingle(NSN_PixelFormat, GetPossiblePixelFormats());
			}
			UpdateAfter(ChangedPinType::PixelFormat, !oldValue);
		});
	}

	void AutoSelectIfSingle(nosName pinName, std::vector<std::string> const& list)
	{
		if (list.size() == 2)
			SetPinValue(pinName, nosBuffer{.Data = (void*)list[1].c_str(), .Size = list[1].size() + 1});
	}

	void UpdateAfter(ChangedPinType pin, bool first)
	{
		bool isInput = Channel.Direction == NOS_MEDIAIO_DIRECTION_INPUT;
		switch (pin)
		{
		case ChangedPinType::IsInput: {
			ChangePinReadOnly(NSN_Resolution, isInput);
			ChangePinReadOnly(NSN_FrameRate, isInput);
			ChangePinReadOnly(NSN_PixelFormat, isInput);
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
			auto resolutionList = GetPossibleResolutions();
			UpdateStringList(GetResolutionStringListName(), resolutionList);
			if(!isInput && !first)
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
			auto pixelFormatList = GetPossiblePixelFormats();
			UpdateStringList(GetPixelFormatStringListName(), pixelFormatList);
			if (!first)
				AutoSelectIfSingle(NSN_PixelFormat, pixelFormatList);
			break;
		}
		}
	}

	void ResetAfter(ChangedPinType pin)
	{
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
			pinToSet = NSN_PixelFormat; 
			break;
		}
		ResetPin(pinToSet);
	}

	void ResetPin(nosName name)
	{
		SetPinValue(name, nosBuffer{.Data = (void*)"NONE", .Size = 5});
	}

	nosResult ExecuteNode(nosNodeExecuteParams* params) override
	{
		Channel.StartIfOpen();
		return Channel.IsOpen ? NOS_RESULT_SUCCESS : NOS_RESULT_FAILED;
	}
	
	std::string GetDeviceStringListName() { return "decklink.DeviceList." + UUID2STR(NodeId); }
	std::string GetChannelStringListName() { return "decklink.ChannelList." + UUID2STR(NodeId); }
	std::string GetResolutionStringListName() { return "decklink.ResolutionList." + UUID2STR(NodeId); }
	std::string GetFrameRateStringListName() { return "decklink.FrameRateList." + UUID2STR(NodeId); }
	std::string GetPixelFormatStringListName() { return "decklink.PixelFormatList." + UUID2STR(NodeId); }

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
		nosDeckLink->GetAvailableChannels(Channel.DeviceIndex, Channel.Direction, &channelList);
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
		if (Channel.DeviceIndex == -1 || Channel.Channel == NOS_DECKLINK_CHANNEL_INVALID || Channel.Resolution == NOS_MEDIAIO_FRAME_GEOMETRY_INVALID)
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

	std::vector<std::string> GetPossiblePixelFormats()
	{
		std::vector<std::string> possiblePixelFormats = {"NONE"};
		if (Channel.DeviceIndex == -1 || Channel.FrameRate == NOS_MEDIAIO_FRAME_RATE_INVALID)
			return possiblePixelFormats;
		nosMediaIOPixelFormatList pixelFormats{};
		nosDeckLink->GetSupportedOutputPixelFormats(Channel.DeviceIndex, Channel.Channel, Channel.Resolution, Channel.FrameRate, &pixelFormats);
		for (size_t i = 0; i < pixelFormats.Count; i++)
		{
			auto& format = pixelFormats.PixelFormats[i];
			auto name = nosMediaIO->GetPixelFormatName(format);
			possiblePixelFormats.push_back(name);
		}
		return possiblePixelFormats;
	}
	
	std::string DevicePinValue = "NONE";
	std::string ChannelPinValue = "NONE";
	std::string ResolutionPinValue = "NONE";
	std::string FrameRatePinValue = "NONE";
	std::string PixelFormatPinValue = "NONE";

	ChannelHandler Channel;

	void OnPathStop() override
	{
		Channel.StopIfOpen();
	}
};

void ChannelHandler::UpdateStatus()
{
	std::vector<fb::TNodeStatusMessage> messages;
	if (DeviceIndex == -1)
		messages.push_back(fb::TNodeStatusMessage{{}, "No device selected", fb::NodeStatusMessageType::WARNING});
	else
	{
		nosDeckLinkDeviceInfo deviceInfo{};
		nosDeckLink->GetDeviceInfoByIndex(DeviceIndex, &deviceInfo);
		messages.push_back(fb::TNodeStatusMessage{{}, deviceInfo.ModelName, fb::NodeStatusMessageType::INFO});
	}
	for (auto& [type, message] : StatusMessages)
		messages.push_back(message);
	Node->SetNodeStatusMessages(messages);
}

void ChannelHandler::SetStatus(StatusType statusType, fb::NodeStatusMessageType msgType, std::string text)
{
	StatusMessages[statusType] = fb::TNodeStatusMessage{{}, std::move(text), msgType};
	UpdateStatus();
}

void ChannelHandler::ClearStatus(StatusType statusType)
{
	StatusMessages.erase(statusType);
	UpdateStatus();
}

nosResult RegisterChannelNode(nosNodeFunctions* funcs)
{
	NOS_BIND_NODE_CLASS(NOS_NAME_STATIC("Channel"), ChannelNode, funcs)
	return NOS_RESULT_SUCCESS;
}
}
