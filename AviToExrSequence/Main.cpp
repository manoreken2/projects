#include <stdio.h>
#include <Windows.h>
#include "MLAviReader.h"
#include "MLExrWriter.h"
#include "MLAviCommon.h"
#include "MLConverter.h"

static void
PrintUsage(void) {
    printf("Usage:\n");
    printf("  to extract SDR image : AviToExrSequence - SDR input.avi outputExrPrefix\n");
    printf("  to extract HDR10 PQ image: AviToExrSequence -PQ input.avi outputExrPrefix\n");
    printf("Example: AviToExrSequence -PQ input.avi out : inputs input.avi and outputs out000001.exr, out000002.exr, ...\n");
}

static bool
Process(const wchar_t* inAviPath, const char* outExrPrefix, const bool PQ) {
    MLAviReader aviR;
    MLExrWriter exrW;
    MLConverter conv;

    bool b = aviR.Open(inAviPath);
    if (!b) {
        printf("Error: File open error %S\n", inAviPath);
        return false;
    }

    MLColorGamutType gamut = ML_CG_Rec709;
    MLImage::GammaType gamma = MLImage::MLG_G22;
    if (PQ) {
        gamut = ML_CG_Rec2020;
        gamma = MLImage::MLG_ST2084;
    }

    uint8_t* buf1 = nullptr;
    uint8_t* buf2 = nullptr;

    do {
        if (aviR.NumFrames() == 0) {
            printf("Error: AVI file does not contain image. %S\n", inAviPath);
            b = false;
            break;
        }

        const MLBitmapInfoHeader& imgFmt = aviR.ImageFormat();
        if (imgFmt.biCompression != MLStringToFourCC("r210") &&
            imgFmt.biCompression != MLStringToFourCC("v210")) {
            printf("Error: Unsupported AVI image format %s. Only r210 and v210 is supported. %S\n",
                MLFourCCtoString(imgFmt.biCompression).c_str(),
                inAviPath);
            b = false;
            break;
        }

        // AVIの画像読み出しバッファー。
        buf1 = new uint8_t[imgFmt.biSizeImage];

        // R10G10B10A2の画像置き場。1ピクセル4バイト。
        buf2 = new uint8_t[imgFmt.biWidth * imgFmt.biHeight * 4];

        for (int i = 0; i < aviR.NumFrames(); ++i) {
            int rv = aviR.GetImage(i, imgFmt.biSizeImage, buf1);
            if (rv != imgFmt.biSizeImage) {
                printf("Error: Read image #%d failed. %S\n",
                    i,
                    inAviPath);
                b = false;
                break;
            }

            if (imgFmt.biCompression == MLStringToFourCC("v210")) {
                conv.Yuv422_10bitToR10G10B10A2(
                    (const uint32_t*)buf1,
                    (uint32_t*)buf2,
                    imgFmt.biWidth, imgFmt.biHeight, 0xff);
            } else if (imgFmt.biCompression == MLStringToFourCC("r210")) {
                conv.Rgb10bitToR10G10B10A2(
                    (const uint32_t*)buf1,
                    (uint32_t*)buf2,
                    imgFmt.biWidth, imgFmt.biHeight, 0xff);
            } else {
                // ここには来ない。
                assert(0);
            }

            MLImage mi;
            mi.Init(imgFmt.biWidth, imgFmt.biHeight,
                MLImage::IFFT_CapturedImg,
                MLImage::BFT_UIntR10G10B10A2,
                gamut, gamma,
                10, 3, imgFmt.biSizeImage, buf2);

            char outExrPath[MAX_PATH] = {};
            sprintf_s(outExrPath, "%s%06d.exr", outExrPrefix, i + 1);

            rv = exrW.Write(outExrPath, mi);
            if (rv != S_OK) {
                printf("Error: Write image #%d failed. %s\n",
                    i,
                    outExrPath);
                b = false;
                break;
            }

            printf("  Output %s\n", outExrPath);
        }

    } while (false);

    delete[] buf2;
    buf2 = nullptr;

    delete[] buf1;
    buf1 = nullptr;

    aviR.Close();
    return b;
}

int
main(int argc, char *argv[])
{
    if (argc != 4) {
        PrintUsage();
        return 1;
    }

    bool PQ = false;
    if (0 == strcmp("-PQ", argv[1])) {
        PQ = true;
    } else if (0 == strcmp("-SDR", argv[1])) {
        PQ = false;
    } else {
        printf("Error: Unknown command %s\n", argv[1]);
        PrintUsage();
        return 1;
    }

    wchar_t inAviPath[MAX_PATH] = {};
    MultiByteToWideChar(CP_UTF8, 0, argv[2], -1, inAviPath, _countof(inAviPath) - 1);
    const char* outExrPrefix = argv[3];

    bool b = Process(inAviPath, outExrPrefix, PQ);

    return b ? 0 : 1;
}

