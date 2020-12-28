#pragma once 

#include <DirectXMath.h>
#include "MLImage.h"


struct MLColorConvShaderConstants {
    DirectX::XMMATRIX colorConvMat;
    int imgGammaType; //< MLImage::GammaType
    int flags; //< FlagsType
    float maxNits;

    enum FlagsType {
        FLAG_OutOfRangeToBlack = 1,
    };
};

