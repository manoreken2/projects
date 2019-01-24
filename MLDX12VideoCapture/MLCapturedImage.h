#pragma once

#include <stdint.h>
#include <string>


struct MLCapturedImage {
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
};

