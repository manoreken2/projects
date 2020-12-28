#pragma once

#include "MLImage.h"

const char* MLImage::MLImageModeToStr(MLImage::ImageModeType t)
{
    switch (t){
    case IM_None: return "None";
    case IM_RGB: return "R8G8B8";
    case IM_YUV: return "YUV";
    case IM_HALF_RGBA: return "R16G16B16A16";
    }
}

const char* MLImage::MLImageFileFormatTypeToStr(MLImage::ImageFileFormatType t) {
    switch (t) {
    case IFFT_None: return "None";
    case IFFT_OpenEXR: return "OpenEXR";
    case IFFT_AVI: return "AVI";
    }
}

const char* MLImage::MLImageBitFormatToStr(MLImage::BitFormatType t) {
    switch (t) {
    case BFT_None: return "None";
    case BFT_HalfFloat: return "HalfFloat";
    case BFT_UInt8: return "UInt8";
    }
}

