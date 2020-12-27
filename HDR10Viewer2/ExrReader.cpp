#include "ExrReader.h"
#include <OpenEXRConfig.h>
#include <ImfRgbaFile.h>
using namespace OPENEXR_IMF_NAMESPACE;
using namespace IMATH_NAMESPACE;
using namespace std;

int
ExrRead(const char* exrFilePath, MLImage& img_return)
{
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

    img_return.width = w;
    img_return.height = h;
    img_return.imgMode = MLImage::IM_HALF_RGBA;
    img_return.imgFormat = "";
    img_return.bytes = w * h * 4 * 2; // 4==numCh, 2== sizeof(half)
    img_return.data = new uint8_t[w * h * 4 * 2]; 

    in.setFrameBuffer(ComputeBasePointer((Rgba*)&img_return.data[0], dw), 1, w);
    in.readPixels(dw.min.y, dw.max.y);

    return 0;
}

