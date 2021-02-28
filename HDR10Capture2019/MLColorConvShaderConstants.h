#pragma once 

#include <DirectXMath.h>
#include "MLImage2.h"


struct MLColorConvShaderConstants {
    DirectX::XMMATRIX colorConvMat;
    DirectX::XMFLOAT4 outOfRangeColor;
    int imgGammaType; //< MLImage2::GammaType
    int flags; //< FlagsType. 1=HighlightOutOfRange, 2=SwapRedBlue, 4=Limited Range
    float outOfRangeNits;
    float scale;

    enum FlagsType {
        FLAG_OutOfRangeColor = 1,
        FLAG_SwapRedBlue     = 2,
        FLAG_LimitedRange   = 4,
    };
};

