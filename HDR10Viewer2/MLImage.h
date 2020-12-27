#pragma once

#include <stdint.h>
#include <string>


struct MLImage {
    enum ImageMode {
        IM_RGB,
        IM_YUV
    };

    uint8_t *data;
    int bytes;
    int width;
    int height;
    ImageMode imgMode;
    std::string imgFormat;

    void Init(int aW, int aH, ImageMode aIm, const char *aFmtString, int aBytes, uint8_t *aData) {
        data = aData;
        bytes = aBytes;
        width = aW;
        height = aH;
        imgMode = aIm;
        imgFormat = aFmtString;
    }

    void Term(void) {
        delete[] data;
        data = nullptr;
    }
};

