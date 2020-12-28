#pragma once

#include <stdint.h>
#include <string>
#include "MLColorGamut.h"


struct MLImage {
    enum ImageModeType {
        IM_None,
        IM_RGB,
        IM_YUV,
        IM_HALF_RGBA,
    };

    enum ImageFileFormatType {
        IFFT_None,
        IFFT_OpenEXR,
        IFFT_PNG,
    };

    enum BitFormatType {
        BFT_None,
        BFT_HalfFloat,
        BFT_UInt8,
    };

    /// <summary>
    /// new[]Ç≈ämï€ÇµÇƒâ∫Ç≥Ç¢ÅB
    /// </summary>
    uint8_t *data = nullptr;
    int bytes = 0;
    int width = 0;
    int height = 0;
    ImageModeType imgMode = IM_None;
    ImageFileFormatType imgFileFormat = IFFT_None;
    BitFormatType bitFormat = BFT_None;
    MLColorGamutType colorGamut = ML_CG_Rec709;

    void Init(int aW, int aH, ImageModeType aIm, ImageFileFormatType iff, BitFormatType bf, MLColorGamutType cg, int aBytes, uint8_t *aData) {
        data = aData;
        bytes = aBytes;
        width = aW;
        height = aH;
        imgMode = aIm;
        imgFileFormat = iff;
        bitFormat = bf;
        colorGamut = cg;
    }

    void Term(void) {
        delete[] data;
        data = nullptr;
    }

    static const char* MLImageModeToStr(ImageModeType t);

    static const char* MLImageFileFormatTypeToStr(ImageFileFormatType t);

    static const char* MLImageBitFormatToStr(BitFormatType t);
};

