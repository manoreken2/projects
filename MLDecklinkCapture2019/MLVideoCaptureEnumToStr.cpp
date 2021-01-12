#include "MLVideoCaptureEnumToStr.h"

const char*
BMDColorspaceToStr(BMDColorspace t)
{
    switch (t) {
    case bmdColorspaceRec601:
        return "Rec.601";
    case bmdColorspaceRec709:
        return "Rec.709";
    case bmdColorspaceRec2020:
        return "Rec.2020";
    default:
        return "Unknown";
    }
}

const char *
BMDDynamicRangeToStr(BMDDynamicRange t) {
    switch (t) {
    case bmdDynamicRangeSDR:
        return "SDR";
    case bmdDynamicRangeHDRStaticPQ:
        return "PQ";
    case bmdDynamicRangeHDRStaticHLG:
        return "HLG";
    default:
        return "Unknown";
    }
}

const std::string
BMDDetectedVideoInputFormatFlagsToStr(int t)
{
    std::string s = "";

    if (t & bmdDetectedVideoInputYCbCr422) {
        s = "YCbCr422";
    }
    if (t& bmdDetectedVideoInputRGB444) {
        if (s.length() != 0) { s += ","; }
        s += "RGB444";
    }
    if (t& bmdDetectedVideoInputDualStream3D) {
        if (s.length() != 0) { s += ","; }
        s += "DualStream3D";
    }
    if (t& bmdDetectedVideoInput12BitDepth) {
        if (s.length() != 0) { s += ","; }
        s += "12BitDepth";
    }
    if (t& bmdDetectedVideoInput10BitDepth) {
        if (s.length() != 0) { s += ","; }
        s += "10BitDepth";
    }
    if (t& bmdDetectedVideoInput8BitDepth) {
        if (s.length() != 0) { s += ","; }
        s += "8BitDepth";
    }

    if (s.length() == 0) {
        s = "None";
    }

    return s;
}


const std::string
BMDDisplayModeFlagsToStr(BMDDisplayModeFlags t)
{
    std::string s = "";

    if (t & bmdDisplayModeSupports3D) {
        s = "Supports3D";
    }
    if (t & bmdDisplayModeColorspaceRec601) {
        if (s.length() != 0) { s += ","; }
        s += "Rec.601";
    }
    if (t & bmdDisplayModeColorspaceRec709) {
        if (s.length() != 0) { s += ","; }
        s += "Rec.709";
    }
    if (t & bmdDisplayModeColorspaceRec2020) {
        if (s.length() != 0) { s += ","; }
        s += "Rec.2020";
    }

    if (s.length() == 0) {
        s = "None";
    }

    return s;
}


const char *
BMDFieldDominanceToStr(BMDFieldDominance t)
{
    switch (t) {
    case bmdUnknownFieldDominance:
    default:
        return "UnknownFieldDominance";
    case bmdLowerFieldFirst: return "LowerFieldFirst";
    case bmdUpperFieldFirst: return "UpperFieldFirst";
    case bmdProgressiveFrame: return "ProgressiveFrame";
    case bmdProgressiveSegmentedFrame: return "ProgressiveSegmentedFrame";
    }
}

const char*
BMDPixelFormatToStr(BMDPixelFormat t)
{
    switch (t) {
    case bmdFormatUnspecified: return "Unspecified";
    case bmdFormat8BitYUV: return "YUV422_8Bit(UYVY)";
    case bmdFormat10BitYUV: return "YUV422_10Bit(v210)";
    case bmdFormat8BitARGB: return "ARGB_8Bit";
    case bmdFormat8BitBGRA: return "BGRA_8Bit";
    case bmdFormat10BitRGB: return "RGB_10Bit(r210)";
    case bmdFormat12BitRGB: return "RGB_12Bit(R12B)";
    case bmdFormat12BitRGBLE: return "RGBLE_12Bit";
    case bmdFormat10BitRGBXLE: return "RGBXLE_10Bit";
    case bmdFormat10BitRGBX: return "RGBX_10Bit";
    case bmdFormatH265: return "H265";
    case bmdFormatDNxHR: return "DNxHR";
    default:
        return "Unknown";
    }
}

const char*
BMDDisplayModeToStr(BMDDisplayMode t)
{
    switch (t) {
    case bmdModeNTSC: return "NTSC";
    case bmdModeNTSC2398: return "NTSC23.98";
    case bmdModePAL: return "PAL";
    case bmdModeNTSCp: return "NTSCp";
    case bmdModePALp: return "PALp";
    case bmdModeHD1080p2398: return "HD1080p23.98";
    case bmdModeHD1080p24: return "HD1080p24";
    case bmdModeHD1080p25: return "HD1080p25";
    case bmdModeHD1080p2997: return "HD1080p29.97";
    case bmdModeHD1080p30: return "HD1080p30";
    case bmdModeHD1080p4795: return "HD1080p47.95";
    case bmdModeHD1080p48: return "HD1080p48";
    case bmdModeHD1080p50: return "HD1080p50";
    case bmdModeHD1080p5994: return "HD1080p59.94";
    case bmdModeHD1080p6000: return "HD1080p60.00";
    case bmdModeHD1080p9590: return "HD1080p95.90";
    case bmdModeHD1080p96: return "HD1080p96";
    case bmdModeHD1080p100: return "HD1080p100";
    case bmdModeHD1080p11988: return "HD1080p119.88";
    case bmdModeHD1080p120: return "HD1080p120";
    case bmdModeHD1080i50: return "HD1080i50";
    case bmdModeHD1080i5994: return "HD1080i59.94";
    case bmdModeHD1080i6000: return "HD1080i60.00";
    case bmdModeHD720p50: return "HD720p50";
    case bmdModeHD720p5994: return "HD720p59.94";
    case bmdModeHD720p60: return "HD720p60";
    case bmdMode2k2398: return "2k23.98";
    case bmdMode2k24: return "2k24";
    case bmdMode2k25: return "2k25";
    case bmdMode2kDCI2398: return "2kDCI23.98";
    case bmdMode2kDCI24: return "2kDCI24";
    case bmdMode2kDCI25: return "2kDCI25";
    case bmdMode2kDCI2997: return "2kDCI29.97";
    case bmdMode2kDCI30: return "2kDCI30";
    case bmdMode2kDCI4795: return "2kDCI47.95";
    case bmdMode2kDCI48: return "2kDCI48";
    case bmdMode2kDCI50: return "2kDCI50";
    case bmdMode2kDCI5994: return "2kDCI59.94";
    case bmdMode2kDCI60: return "2kDCI60";
    case bmdMode2kDCI9590: return "2kDCI95.90";
    case bmdMode2kDCI96: return "2kDCI96";
    case bmdMode2kDCI100: return "2kDCI100";
    case bmdMode2kDCI11988: return "2kDCI119.88";
    case bmdMode2kDCI120: return "2kDCI120";
    case bmdMode4K2160p2398: return "4K2160p23.98";
    case bmdMode4K2160p24: return "4K2160p24";
    case bmdMode4K2160p25: return "4K2160p25";
    case bmdMode4K2160p2997: return "4K2160p29.97";
    case bmdMode4K2160p30: return "4K2160p30";
    case bmdMode4K2160p4795: return "4K2160p47.95";
    case bmdMode4K2160p48: return "4K2160p48";
    case bmdMode4K2160p50: return "4K2160p50";
    case bmdMode4K2160p5994: return "4K2160p59.94";
    case bmdMode4K2160p60: return "4K2160p60";
    case bmdMode4K2160p9590: return "4K2160p95.90";
    case bmdMode4K2160p96: return "4K2160p96";
    case bmdMode4K2160p100: return "4K2160p100";
    case bmdMode4K2160p11988: return "4K2160p119.88";
    case bmdMode4K2160p120: return "4K2160p120";
    case bmdMode4kDCI2398: return "4kDCI23.98";
    case bmdMode4kDCI24: return "4kDCI24";
    case bmdMode4kDCI25: return "4kDCI25";
    case bmdMode4kDCI2997: return "4kDCI29.97";
    case bmdMode4kDCI30: return "4kDCI30";
    case bmdMode4kDCI4795: return "4kDCI47.95";
    case bmdMode4kDCI48: return "4kDCI48";
    case bmdMode4kDCI50: return "4kDCI50";
    case bmdMode4kDCI5994: return "4kDCI59.94";
    case bmdMode4kDCI60: return "4kDCI60";
    case bmdMode4kDCI9590: return "4kDCI95.90";
    case bmdMode4kDCI96: return "4kDCI96";
    case bmdMode4kDCI100: return "4kDCI100";
    case bmdMode4kDCI11988: return "4kDCI119.88";
    case bmdMode4kDCI120: return "4kDCI120";
    case bmdMode8K4320p2398: return "8K4320p23.98";
    case bmdMode8K4320p24: return "8K4320p24";
    case bmdMode8K4320p25: return "8K4320p25";
    case bmdMode8K4320p2997: return "8K4320p29.97";
    case bmdMode8K4320p30: return "8K4320p30";
    case bmdMode8K4320p4795: return "8K4320p47.95";
    case bmdMode8K4320p48: return "8K4320p48";
    case bmdMode8K4320p50: return "8K4320p50";
    case bmdMode8K4320p5994: return "8K4320p59.94";
    case bmdMode8K4320p60: return "8K4320p60";
    case bmdMode8kDCI2398: return "8kDCI23.98";
    case bmdMode8kDCI24: return "8kDCI24";
    case bmdMode8kDCI25: return "8kDCI25";
    case bmdMode8kDCI2997: return "8kDCI29.97";
    case bmdMode8kDCI30: return "8kDCI30";
    case bmdMode8kDCI4795: return "8kDCI47.95";
    case bmdMode8kDCI48: return "8kDCI48";
    case bmdMode8kDCI50: return "8kDCI50";
    case bmdMode8kDCI5994: return "8kDCI59.94";
    case bmdMode8kDCI60: return "8kDCI60";
    case bmdMode640x480p60: return "640x480p60";
    case bmdMode800x600p60: return "800x600p60";
    case bmdMode1440x900p50: return "1440x900p50";
    case bmdMode1440x900p60: return "1440x900p60";
    case bmdMode1440x1080p50: return "1440x1080p50";
    case bmdMode1440x1080p60: return "1440x1080p60";
    case bmdMode1600x1200p50: return "1600x1200p50";
    case bmdMode1600x1200p60: return "1600x1200p60";
    case bmdMode1920x1200p50: return "1920x1200p50";
    case bmdMode1920x1200p60: return "1920x1200p60";
    case bmdMode1920x1440p50: return "1920x1440p50";
    case bmdMode1920x1440p60: return "1920x1440p60";
    case bmdMode2560x1440p50: return "2560x1440p50";
    case bmdMode2560x1440p60: return "2560x1440p60";
    case bmdMode2560x1600p50: return "2560x1600p50";
    case bmdMode2560x1600p60: return "2560x1600p60";
    case bmdModeUnknown:
    default:
        return "Unknown";
    }
}
