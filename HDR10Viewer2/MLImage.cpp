#pragma once

#include "MLImage.h"

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
    case BFT_UInt16: return "UInt16";
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
