#pragma once 

#include <DirectXMath.h>
#include "MLImage.h"


struct MLColorConvShaderConstants {
    DirectX::XMMATRIX colorConvMat;
    DirectX::XMFLOAT4 outOfRangeColor;
    int imgGammaType; //< MLImage::GammaType
    int flags; //< FlagsType
    float maxNits;

    enum FlagsType {
        FLAG_OutOfRangeColor = 1,
    };
};

