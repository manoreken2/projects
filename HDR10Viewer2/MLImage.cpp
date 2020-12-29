#pragma once

#include "MLImage.h"

const char* MLImage::MLImageModeToStr(MLImage::ImageModeType t)
{
    switch (t){
    case IM_None: return "None";
    case IM_RGB: return "R8G8B8";
    case IM_HALF_RGBA: return "R16G16B16A16";
    default:
        assert(0);
        return "";
    }
}

const char* MLImage::MLImageFileFormatTypeToStr(MLImage::ImageFileFormatType t) {
    switch (t) {
    case IFFT_None: return "None";
    case IFFT_OpenEXR: return "OpenEXR";
    case IFFT_PNG: return "PNG";
    case IFFT_BMP: return "BMP";
    default:
        assert(0);
        return "";
    }
}

const char* MLImage::MLImageBitFormatToStr(MLImage::BitFormatType t) {
    switch (t) {
    case BFT_None: return "None";
    case BFT_HalfFloat: return "HalfFloat";
    case BFT_UInt8: return "UInt8";
    default:
        assert(0);
        return "";
    }
}

const char* MLImage::GammaTypeToStr(MLImage::GammaType t) {
    switch (t) {
    case MLG_Linear: return "Linear";
    case MLG_G22: return "Gamma2.2";
    case MLG_ST2084: return "ST.2084 PQ";
    default:
        assert(0);
        return "";
    }
}
