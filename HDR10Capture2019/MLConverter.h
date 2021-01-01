#pragma once

#include <stdint.h>

class MLConverter {
public:
    MLConverter(void);
    ~MLConverter(void);

    /// <summary>
    /// bmdFormat8BitARGB Å® DXGI_FORMAT_R8G8B8A8_UNORM
    /// </summary>
    static void Argb8bitToR8G8B8A8(uint32_t* pFrom, uint32_t* pTo, const int width, const int height);

    /// <summary>
    /// bmdFormat10BitRGB Å® DXGI_FORMAT_R8G8B8A8_UNORM
    /// </summary>
    static void Rgb10bitToRGBA8bit(uint32_t *pFrom, uint32_t *pTo, const int width, const int height, const uint8_t alpha);

    /// <summary>
    /// bmdFormat10BitRGB Å® DXGI_FORMAT_R10G10B10A2_UNORM
    /// </summary>
    static void Rgb10bitToR10G10B10A2(uint32_t* pFrom, uint32_t* pTo, const int width, const int height, const uint8_t alpha);

    /// <summary>
    /// bmdFormat12BitRGB Å® DXGI_FORMAT_R8G8B8A8_UNORM
    /// </summary>
    static void Rgb12bitToR8G8B8A8(uint32_t* pFrom, uint32_t* pTo, const int width, const int height, const uint8_t alpha);
    
    /// <summary>
    /// bmdFormat12BitRGB Å® DXGI_FORMAT_R10G10B10A2_UNORM
    /// </summary>
    static void Rgb12bitToR10G10B10A2(uint32_t* pFrom, uint32_t* pTo, const int width, const int height, const uint8_t alpha);

    /// <summary>
    /// bmdFormat12BitRGB Å® DXGI_FORMAT_R16G16B16A16_UNORM
    /// </summary>
    static void Rgb12bitToR16G16B16A16(uint32_t* pFrom, uint64_t* pTo, const int width, const int height, const uint8_t alpha);

private:
};
