#pragma once

#include "MLImage2.h"

const char* MLImage2::MLImageFileFormatTypeToStr(MLImage2::ImageFileFormatType t) {
    switch (t) {
    case IFFT_None: return "None";
    case IFFT_OpenEXR: return "OpenEXR";
    case IFFT_PNG: return "PNG";
    case IFFT_BMP: return "BMP";
    case IFFT_CapturedImg: return "CapturedImg";
    default:
        assert(0);
        return "";
    }
}

const char* MLImage2::MLImageBitFormatToStr(MLImage2::BitFormatType t) {
    switch (t) {
    case BFT_None: return "None";
    case BFT_HalfFloatR16G16B16A16: return "HalfFloatR16G16B16A16";
    case BFT_UIntR8G8B8A8: return "UIntR8G8B8A8";
    case BFT_UIntR10G10B10A2: return "UIntR10G10B10A2";
    case BFT_UIntR16G16B16A16: return "UIntR16G16B16A16";
    default:
        assert(0);
        return "";
    }
}

const char* MLImage2::GammaTypeToStr(MLImage2::GammaType t) {
    switch (t) {
    case MLG_Linear: return "Linear";
    case MLG_G22: return "Gamma2.2";
    case MLG_ST2084: return "ST.2084 PQ";
    case MLG_HLG: return "HLG";
    default:
        assert(0);
        return "";
    }
}
