#pragma once

#include <stdint.h>
#include <string>


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
        IFFT_AVI,
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
    ImageModeType imgMode;
    ImageFileFormatType imgFileFormat;
    BitFormatType bitFormat;

    void Init(int aW, int aH, ImageModeType aIm, ImageFileFormatType iff, BitFormatType bf, int aBytes, uint8_t *aData) {
        data = aData;
        bytes = aBytes;
        width = aW;
        height = aH;
        imgMode = aIm;
        imgFileFormat = iff;
        bitFormat = bf;
    }

    void Term(void) {
        delete[] data;
        data = nullptr;
    }
};

