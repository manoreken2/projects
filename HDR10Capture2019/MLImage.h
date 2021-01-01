#pragma once

#include <stdint.h>
#include <string>
#include "MLColorGamut.h"


struct MLImage {
    enum ImageFileFormatType {
        IFFT_None,
        IFFT_OpenEXR,
        IFFT_PNG,
        IFFT_BMP,
        IFFT_CapturedImg,
    };

    enum BitFormatType {
        BFT_None,
        BFT_HalfFloatR16G16B16A16,
        BFT_UIntR8G8B8A8,
        BFT_UIntR10G10B10A2,
        BFT_UIntR16G16B16A16,
    };

    enum GammaType {
        MLG_Linear,
        MLG_G22,
        MLG_ST2084,
        MLG_HLG,
    };

    /// <summary>
    /// new[]で確保して下さい。
    /// RGBAの順、ピクセル値を左から右、上から下に並べます。
    /// </summary>
    uint8_t *data = nullptr;
    int bytes = 0;
    int width = 0;
    int height = 0;
    ImageFileFormatType imgFileFormat = IFFT_None;
    BitFormatType bitFormat = BFT_None;
    MLColorGamutType colorGamut = ML_CG_Rec709;
    GammaType gamma = MLG_G22;
    int originalBitDepth = 8;
    int originalNumChannels = 3;

    void Init(int aW, int aH, ImageFileFormatType iff, BitFormatType bf, MLColorGamutType cg, GammaType ga, int aOriginalBitDepth, int aOriginalNumChannels, int aBytes, uint8_t *aData) {
        data = aData;
        bytes = aBytes;
        width = aW;
        height = aH;
        imgFileFormat = iff;
        bitFormat = bf;
        colorGamut = cg;
        gamma = ga;
        originalBitDepth = aOriginalBitDepth;
        originalNumChannels = aOriginalNumChannels;
    }

    void Term(void) {
        width = 0;
        height = 0;
        bytes = 0;
        delete[] data;
        data = nullptr;
    }

    static const char* MLImageFileFormatTypeToStr(ImageFileFormatType t);

    static const char* MLImageBitFormatToStr(BitFormatType t);

    static const char* GammaTypeToStr(GammaType t);
};

