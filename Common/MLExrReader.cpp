#include "MLExrReader.h"
#include <OpenEXRConfig.h>
#include <ImfRgbaFile.h>
using namespace OPENEXR_IMF_NAMESPACE;
using namespace IMATH_NAMESPACE;
using namespace std;

int
MLExrRead(const char* exrFilePath, MLImage2& img_return)
{
    img_return.data = nullptr;

    RgbaInputFile in(exrFilePath);

    Box2i dw = in.dataWindow();
    int w = dw.max.x - dw.min.x + 1;
    int h = dw.max.y - dw.min.y + 1;
    //float a = in.pixelAspectRatio();
    Header header = in.header();
    if (header.hasTileDescription()) {
        printf("Error: TileDescription not supported\n");
        return -1;
    }

    img_return.Term();
    img_return.Init(w, h, MLImage2::IFFT_OpenEXR, MLImage2::BFT_HalfFloatR16G16B16A16,
        ML_CG_Rec2020, MLImage2::MLG_Linear, 16, 4,
        w * h * 4 * 2, // 4==numCh, 2== sizeof(half)
        new uint8_t[w * h * 4 * 2]);

    in.setFrameBuffer(ComputeBasePointer((Rgba*)&img_return.data[0], dw), 1, w);
    in.readPixels(dw.min.y, dw.max.y);

    return 0;
}

