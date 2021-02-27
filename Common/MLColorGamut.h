#pragma once

#include <DirectXMath.h>

enum MLColorGamutType {
    ML_CG_Rec709,
    ML_CG_AdobeRGB,
    ML_CG_Rec2020,
    ML_CG_DCIP3,
    ML_CG_scRGB,
};

const char* MLColorGamutToStr(MLColorGamutType t);

class MLColorGamutConv {
public:
    MLColorGamutConv(void);
    const DirectX::XMMATRIX & ConvMat(MLColorGamutType from, MLColorGamutType to);

private:
    //                      from to
    DirectX::XMMATRIX mConvMat[5][5];
};

