#include "MLExrReader.h"
#include <OpenEXRConfig.h>
#include <ImfRgbaFile.h>
#include "MLConverter.h"
#include <Windows.h>

using namespace OPENEXR_IMF_NAMESPACE;
using namespace IMATH_NAMESPACE;
using namespace std;

int
MLExrWrite(const char* exrFilePath, const MLImage& img)
{
    MLConverter conv;

    MLConverter::GammaType gamma = MLConverter::GT_SDR_22;
    switch (img.gamma) {
    case MLImage::MLG_G22:
        gamma = MLConverter::GT_SDR_22;
        break;
    case MLImage::MLG_ST2084:
        gamma = MLConverter::GT_HDR_PQ;
        break;
    default:
        // 作ってない。
        return E_NOTIMPL;
    }

    // 4==ch, 2 ==half
    const int buffBytes = img.width * img.height * 4 * 2;
    uint8_t* buff = nullptr;

    switch (img.bitFormat) {
    case MLImage::BFT_UIntR10G10B10A2:
        buff = new uint8_t[buffBytes];
        conv.R10G10B10A2ToExrHalfFloat((uint32_t*)img.data, (uint16_t*)buff, img.width, img.height, 0xff, gamma);
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

