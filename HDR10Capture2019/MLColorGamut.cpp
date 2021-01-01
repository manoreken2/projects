#include "MLColorGamut.h"
#include <assert.h>

const char* MLColorGamutToStr(MLColorGamutType t)
{
    switch (t) {
    case     ML_CG_Rec709: return "Rec.709";
    case     ML_CG_AdobeRGB: return "Adobe RGB";
    case     ML_CG_Rec2020: return "Rec.2020";
    case     ML_CG_DCIP3: return "DCIP3";
    default:
        assert(0);
        return "";
    }
}


MLColorGamutConv::MLColorGamutConv(void) {
    //        from         to
    mConvMat[ML_CG_Rec709][ML_CG_Rec709] = DirectX::XMMatrixIdentity();
    mConvMat[ML_CG_Rec709][ML_CG_AdobeRGB] = DirectX::XMMATRIX(
        0.7151f, 0.2849f, 0.0000f, 0.0f,
        0.0000f, 1.0000f, 0.0000f, 0.0f,
        -0.0000f, 0.0412f, 0.9588f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);
    mConvMat[ML_CG_Rec709][ML_CG_Rec2020] = DirectX::XMMATRIX(
        0.6274f, 0.3293f, 0.0434f, 0.0f,
        0.0691f, 0.9196f, 0.0114f, 0.0f,
        0.0164f, 0.0880f, 0.8958f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);
    mConvMat[ML_CG_Rec709][ML_CG_DCIP3] = DirectX::XMMATRIX(
        0.8224f, 0.1775f, - 0.0000f, 0.0f,
        0.0332f, 0.9668f, - 0.0000f, 0.0f,
        0.0171f, 0.0724f, 0.9107f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);

    mConvMat[ML_CG_AdobeRGB][ML_CG_Rec709] = DirectX::XMMATRIX(
        1.3984f, - 0.3983f,    0.0000f, 0.0f,
        - 0.0000f, 1.0000f, - 0.0000f, 0.0f,
        - 0.0000f, - 0.0429f, 1.0429f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);
    mConvMat[ML_CG_AdobeRGB][ML_CG_AdobeRGB] = DirectX::XMMatrixIdentity();
    mConvMat[ML_CG_AdobeRGB][ML_CG_Rec2020] = DirectX::XMMATRIX(
        0.8773f, 0.0775f, 0.0452f, 0.0f,
        0.0966f, 0.8915f, 0.0119f, 0.0f,
        0.0229f, 0.0430f, 0.9342f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);
    mConvMat[ML_CG_AdobeRGB][ML_CG_DCIP3] = DirectX::XMMATRIX(
        1.1500f, - 0.1501f, - 0.0000f, 0.0f,
        0.0464f, 0.9536f, - 0.0000f, 0.0f,
        0.0239f, 0.0265f, 0.9498f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);

    mConvMat[ML_CG_Rec2020][ML_CG_Rec709] = DirectX::XMMATRIX(
        1.6606f, - 0.5876f, - 0.0728f, 0.0f,
        - 0.1246f, 1.1329f, - 0.0083f, 0.0f,
        - 0.0182f, - 0.1006f, 1.1185f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);
    mConvMat[ML_CG_Rec2020][ML_CG_AdobeRGB] = DirectX::XMMATRIX(
        1.1521f, - 0.0975f, - 0.0545f, 0.0f,
        - 0.1246f, 1.1329f, - 0.0083f, 0.0f,
        - 0.0225f, - 0.0498f, 1.0721f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);
    mConvMat[ML_CG_Rec2020][ML_CG_Rec2020] = DirectX::XMMatrixIdentity();
    mConvMat[ML_CG_Rec2020][ML_CG_DCIP3] = DirectX::XMMATRIX(
        1.3435f, - 0.2822f, - 0.0614f, 0.0f,
        - 0.0653f, 1.0758f, - 0.0105f, 0.0f,
        0.0028f, - 0.0196f, 1.0168f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);

    mConvMat[ML_CG_DCIP3][ML_CG_Rec709] = DirectX::XMMATRIX(
        1.2251f, - 0.2249f, - 0.0000f, 0.0f,
        - 0.0421f, 1.0420f, 0.0000f, 0.0f,
        - 0.0196f, - 0.0786f, 1.0980f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);
    mConvMat[ML_CG_DCIP3][ML_CG_AdobeRGB] = DirectX::XMMATRIX(
        0.8641f, 0.1360f, 0.0000f, 0.0f,
        - 0.0421f, 1.0420f, 0.0000f, 0.0f,
        - 0.0206f, - 0.0325f, 1.0528f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);
    mConvMat[ML_CG_DCIP3][ML_CG_Rec2020] = DirectX::XMMATRIX(
        0.7539f, 0.1986f, 0.0476f, 0.0f,
        0.0457f, 0.9418f, 0.0125f, 0.0f,
        - 0.0012f, 0.0176f, 0.9836f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);
    mConvMat[ML_CG_DCIP3][ML_CG_DCIP3] = DirectX::XMMatrixIdentity();
}

const DirectX::XMMATRIX&
MLColorGamutConv::ConvMat(MLColorGamutType from, MLColorGamutType to)
{
    return mConvMat[from][to];
}
