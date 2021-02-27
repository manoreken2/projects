#pragma once 

#include <DirectXMath.h>
#include "MLImage2.h"


struct MLColorConvShaderConstants {
    DirectX::XMMATRIX colorConvMat;
    DirectX::XMFLOAT4 outOfRangeColor;
    int imgGammaType; //< MLImage2::GammaType
    int flags; //< FlagsType
    float outOfRangeNits;
    float scale;

    enum FlagsType {
        FLAG_OutOfRangeColor = 1,
        FLAG_SwapRedBlue     = 2,
    };
};

