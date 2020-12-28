#pragma once 

#include <DirectXMath.h>
#include "MLImage.h"


struct MLColorConvShaderConstants {
    DirectX::XMMATRIX colorConvMat;
    int imgGammaType; //< MLImage::GammaType
};

