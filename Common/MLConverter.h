#pragma once

#include <stdint.h>
#include "half.h"

class MLConverter {
public:
    MLConverter(void);
    ~MLConverter(void) { }

    enum ColorSpace {
        CS_Rec601,
        CS_Rec709,
        CS_Rec2020,
    };

    /// <summary>
    /// R8G8B8A8 to B8G8R8 for BMP save
    /// </summary>
    static void R8G8B8A8ToB8G8R8_DIB(const uint32_t* pFrom, uint8_t* pTo, const int width, const int height);
    static void R8G8B8A8ToB8G8R8A8_DIB(const uint32_t* pFrom, uint8_t* pTo, const int width, const int height);

    /// <summary>
    /// R8G8B8 to B8G8R8 for BMP save
    /// </summary>
    static void R8G8B8ToB8G8R8_DIB(const uint8_t* pFrom, uint8_t* pTo, const int width, const int height);
    static void R8G8B8ToB8G8R8A8_DIB(const uint8_t* pFrom, uint8_t* pTo, const int width, const int height, const uint8_t alpha);

    /// <summary>
    /// bmdFormat8BitYUV UYVY Å® DXGI_FORMAT_R8G8B8A8_UNORM
    /// </summary>
    static void Uyvy8bitToR8G8B8A8(const ColorSpace colorSpace, const uint32_t* pFrom, uint32_t* pTo, const int width, const int height);

    /// <summary>
    /// bmdFormat10BitYUV v210 Å® DXGI_FORMAT_R10G10B10A2_UNORM
    /// </summary>
    static void Yuv422_10bitToR10G10B10A2(const ColorSpace colorSpace, const uint32_t* pFrom, uint32_t* pTo, const int width, const int height, const uint8_t alpha = 0xff);

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
        GT_Linear = -1,
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

    /// <summary>
    /// 16bit RGBA to R210
    /// </summary>
    static void R16G16B16A16ToR210(const uint16_t* pFrom, uint32_t* pTo, const int width, const int height);

    void BMRawYuvV210ToRGBA(uint32_t* pFrom, uint32_t* pTo, const int width, const int height, const uint8_t alpha);

    static void YuvV210ToYuvA(uint32_t* pFrom, uint32_t* pTo, const int width, const int height, const uint8_t alpha);

	void CreateBMGammaTable(const float gamma, const float gainR, const float gainG, const float gainB);

private:
    half mGammaInv22_10bit[1024];
    half mGammaInvST2084_10bit[1024];

    // BM 12bit value to 8bit value
    uint8_t mBMGammaTableR[4096];
    uint8_t mBMGammaTableG[4096];
    uint8_t mBMGammaTableB[4096];
    uint8_t* mBMGammaBayerTable[4];

    uint8_t BMGammaTable(const int x, const int y, const uint16_t v) const {
        const uint8_t* table = mBMGammaBayerTable[(x & 1) + 2 * (y & 1)];
        return table[v];
    }
};
