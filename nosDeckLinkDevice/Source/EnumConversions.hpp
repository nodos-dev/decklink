#pragma once

#include <vector>

#include <DeckLinkAPI.h>
#include <nosMediaIO/nosMediaIO.h>

namespace nos::decklink
{
constexpr std::vector<BMDDisplayMode> GetDisplayModesForFrameGeometry(nosMediaIOFrameGeometry frameGeometry)
{
	// Map nosMediaIOFrameGeometry to BMDDisplayMode
	switch (frameGeometry)
	{
	case NOS_MEDIAIO_FG_NTSC:
		return {bmdModeNTSC, bmdModeNTSC2398, bmdModeNTSCp};
	case NOS_MEDIAIO_FG_PAL:
		return {bmdModePAL, bmdModePALp};
	case NOS_MEDIAIO_FG_HD720:
		return {bmdModeHD720p50, bmdModeHD720p5994, bmdModeHD720p60};
	case NOS_MEDIAIO_FG_HD1080:
		return {
			bmdModeHD1080p2398, bmdModeHD1080p24, bmdModeHD1080p25, bmdModeHD1080p2997,
			bmdModeHD1080p30, bmdModeHD1080p4795, bmdModeHD1080p48, bmdModeHD1080p50,
			bmdModeHD1080p5994, bmdModeHD1080p6000, bmdModeHD1080i50, bmdModeHD1080i5994,
			bmdModeHD1080i6000
		};
	case NOS_MEDIAIO_FG_2K:
		return {bmdMode2k2398, bmdMode2k24, bmdMode2k25};
	case NOS_MEDIAIO_FG_2KDCI:
		return {
			bmdMode2kDCI2398, bmdMode2kDCI24, bmdMode2kDCI25, bmdMode2kDCI2997,
			bmdMode2kDCI30, bmdMode2kDCI4795, bmdMode2kDCI48, bmdMode2kDCI50,
			bmdMode2kDCI5994, bmdMode2kDCI60, bmdMode2kDCI9590, bmdMode2kDCI96,
			bmdMode2kDCI100, bmdMode2kDCI11988, bmdMode2kDCI120
		};
	case NOS_MEDIAIO_FG_4K2160:
		return {
			bmdMode4K2160p2398, bmdMode4K2160p24, bmdMode4K2160p25, bmdMode4K2160p2997,
			bmdMode4K2160p30, bmdMode4K2160p4795, bmdMode4K2160p48, bmdMode4K2160p50,
			bmdMode4K2160p5994, bmdMode4K2160p60, bmdMode4K2160p9590, bmdMode4K2160p96,
			bmdMode4K2160p100, bmdMode4K2160p11988, bmdMode4K2160p120
		};
	case NOS_MEDIAIO_FG_4KDCI:
		return {
			bmdMode4kDCI2398, bmdMode4kDCI24, bmdMode4kDCI25, bmdMode4kDCI2997,
			bmdMode4kDCI30, bmdMode4kDCI4795, bmdMode4kDCI48, bmdMode4kDCI50,
			bmdMode4kDCI5994, bmdMode4kDCI60
		};
	case NOS_MEDIAIO_FG_8K4320:
		return {
			bmdMode8K4320p2398, bmdMode8K4320p24, bmdMode8K4320p25, bmdMode8K4320p2997,
			bmdMode8K4320p30, bmdMode8K4320p4795, bmdMode8K4320p48, bmdMode8K4320p50,
			bmdMode8K4320p5994, bmdMode8K4320p60
		};
	case NOS_MEDIAIO_FG_8KDCI:
		return {
			bmdMode8kDCI2398, bmdMode8kDCI24, bmdMode8kDCI25, bmdMode8kDCI2997,
			bmdMode8kDCI30, bmdMode8kDCI4795, bmdMode8kDCI48, bmdMode8kDCI50,
			bmdMode8kDCI5994, bmdMode8kDCI60
		};
	case NOS_MEDIAIO_FG_640x480:
		return {bmdMode640x480p60};
	case NOS_MEDIAIO_FG_800x600:
		return {bmdMode800x600p60};
	case NOS_MEDIAIO_FG_1440x900:
		return {bmdMode1440x900p50, bmdMode1440x900p60};
	case NOS_MEDIAIO_FG_1440x1080:
		return {bmdMode1440x1080p50, bmdMode1440x1080p60};
	case NOS_MEDIAIO_FG_1600x1200:
		return {bmdMode1600x1200p50, bmdMode1600x1200p60};
	case NOS_MEDIAIO_FG_1920x1200:
		return {bmdMode1920x1200p50, bmdMode1920x1200p60};
	case NOS_MEDIAIO_FG_1920x1440:
		return {bmdMode1920x1440p50, bmdMode1920x1440p60};
	case NOS_MEDIAIO_FG_2560x1440:
		return {bmdMode2560x1440p50, bmdMode2560x1440p60};
	case NOS_MEDIAIO_FG_2560x1600:
		return {bmdMode2560x1600p50, bmdMode2560x1600p60};
	case NOS_MEDIAIO_FG_INVALID:
	default:
		return {};
	}
}
constexpr BMDPixelFormat GetDeckLinkPixelFormat(nosMediaIOPixelFormat fmt)
{
	switch (fmt)
	{
	case NOS_MEDIAIO_PIXEL_FORMAT_YCBCR_8BIT:
		return bmdFormat8BitYUV;
	case NOS_MEDIAIO_PIXEL_FORMAT_YCBCR_10BIT:
		return bmdFormat10BitYUV;
	}
	return bmdFormatUnspecified;
}
}
