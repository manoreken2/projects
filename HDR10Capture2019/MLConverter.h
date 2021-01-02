#pragma once

#include <stdint.h>
#include "half.h"

class MLConverter {
public:
    MLConverter(void);
    ~MLConverter(void) { }

    /// <summary>
    /// bmdFormat8BitARGB Å® DXGI_FORMAT_R8G8B8A8_UNORM
    /// </summary>
    static void Argb8bitToR8G8B8A8(const uint32_t* pFrom, uint32_t* pTo, const int width, const int height);

    /// <summary>
    /// bmdFormat10BitRGB r210 Å® DXGI_FORMAT_R8G8B8A8_UNORM
    /// </summary>
    static void Rgb10bitToRGBA8bit(const uint32_t* pFrom, uint32_t* pTo, const int width, const int height, const uint8_t alpha);

    /// <summary>
    /// bmdFormat10BitRGB r210 Å® DXGI_FORMAT_R10G10B10A2_UNORM
    /// </summary>
    static void Rgb10bitToR10G10B10A2(const uint32_t* pFrom, uint32_t* pTo, const int width, const int height, const uint8_t alpha);

    enum GammaType {
        GT_SDR_22,
        GT_HDR_PQ,
    };

    /// <summary>
    /// R10G10B10A2_Unorm Å® half float to write to Exr
    /// </summary>
    void R10G10B10A2ToExrHalfFloat(const uint32_t* pFrom, uint16_t* pTo, const int width, const int height, const uint8_t alpha, GammaType gamma);

    /// <summary>
    /// bmdFormat12BitRGB Å® DXGI_FORMAT_R8G8B8A8_UNORM
    /// </summary>
    static void Rgb12bitToR8G8B8A8(const uint32_t* pFrom, uint32_t* pTo, const int width, const int height, const uint8_t alpha);
    
    /// <summary>
    /// bmdFormat12BitRGB Å® DXGI_FORMAT_R10G10B10A2_UNORM
    /// </summary>
    static void Rgb12bitToR10G10B10A2(const uint32_t* pFrom, uint32_t* pTo, const int width, const int height, const uint8_t alpha);

    /// <summary>
    /// bmdFormat12BitRGB Å® RGB10bit r210
    /// </summary>
    static void Rgb12bitToR210(const uint32_t* pFrom, uint32_t* pTo, const int width, const int height);

    /// <summary>
    /// bmdFormat12BitRGB Å® DXGI_FORMAT_R16G16B16A16_UNORM
    /// </summary>
    static void Rgb12bitToR16G16B16A16(const uint32_t* pFrom, uint64_t* pTo, const int width, const int height, const uint8_t alpha);

    /// <summary>
    /// DXGI_FORMAT_R10G10B10A2_UNORM Å® bmdFormat10BitRGB(r210)
    /// </summary>
    static void R10G10B10A2ToR210(const uint32_t* pFrom, uint32_t* pTo, const int width, const int height, const uint8_t alpha);

private:
    half mGammaInv22_10bit[1024];
    half mGammaInvST2084_10bit[1024];
};
