#include "MLExrWriter.h"
#include <OpenEXRConfig.h>
#include <ImfRgbaFile.h>
#include "MLConverter.h"
#include <Windows.h>

using namespace OPENEXR_IMF_NAMESPACE;
using namespace IMATH_NAMESPACE;
using namespace std;

/// <summary>
/// OpenEXRファイルを書く。
/// </summary>
/// <returns>0:成功。負の数:失敗。</returns>
int
MLExrWriter::Write(const char* exrFilePath, const MLImage2& img)
{
    MLConverter::GammaType gamma = MLConverter::GT_SDR_22;
    switch (img.gamma) {
    case MLImage2::MLG_G22:
        gamma = MLConverter::GT_SDR_22;
        break;
    case MLImage2::MLG_ST2084:
        gamma = MLConverter::GT_HDR_PQ;
        break;
    case MLImage2::MLG_Linear:
        gamma = MLConverter::GT_Linear;
        break;
    default:
        // 作ってない。
        return E_NOTIMPL;
    }

    // 4==ch, 2 ==half
    const int buffBytes = img.width * img.height * 4 * 2;
    uint8_t* buff = nullptr;

    switch (img.bitFormat) {
    case MLImage2::BFT_UIntR10G10B10A2:
        buff = new uint8_t[buffBytes];
        mConv.R10G10B10A2ToExrHalfFloat((uint32_t*)img.data, (uint16_t*)buff, img.width, img.height, 0xff, gamma);
        break;
    case MLImage2::BFT_HalfFloatR16G16B16A16:
        buff = new uint8_t[buffBytes];
        memcpy(buff, img.data, buffBytes);
        break;
    case MLImage2::BFT_UIntR16G16B16A16:
        buff = new uint8_t[buffBytes];
        mConv.R16G16B16A16ToExrHalfFloat((uint16_t*)img.data, (uint16_t*)buff, img.width, img.height);
        break;
    case MLImage2::BFT_UIntR8G8B8A8:
        buff = new uint8_t[buffBytes];
        mConv.R8G8B8A8ToExrHalfFloat((uint8_t*)img.data, (uint16_t*)buff, img.width, img.height, (MLConverter::QuantizationRange)img.quantizationRange);
        break;
    default:
        // 作ってない。
        return E_NOTIMPL;
    }

    // OpenEXRファイルを書き込む。
    RgbaOutputFile file(exrFilePath, img.width, img.height, WRITE_RGBA);
    file.setFrameBuffer((const Rgba*)buff, 1, img.width);
    file.writePixels(img.height);

    delete[] buff;
    buff = nullptr;

    return S_OK;
}

