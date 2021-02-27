#include "MLPngReader.h"
#include "MLAviWriter.h"
#include "MLConverter.h"
#include "MLImage.h"

static void
PrintUsage(void) {
    printf("Usage: PngSequenceToAvi pngFolder fps aviFile.avi\n");
}

int
wmain(int argc, wchar_t* argv[]) {
    int rv;
    MLConverter conv;

    if (argc != 4) {
        PrintUsage();
        return 1;
    }

    const wchar_t* fromPngFolder = argv[1];
    double fps = wcstod(argv[2], nullptr);
    const wchar_t* toAviFile = argv[3];

    if (fps <= 0) {
        printf("Error: fps value is out of range\n");
        PrintUsage();
        return 1;
    }

    // PNGファイルの一覧を取得。
    std::list<std::wstring> pngFileList;
    {
        WIN32_FIND_DATA findD = { 0 };
        HANDLE hFind = INVALID_HANDLE_VALUE;

        std::wstring path = std::wstring(fromPngFolder) + L"\\*.png";

        hFind = FindFirstFile(path.c_str(), &findD);
        if (hFind == INVALID_HANDLE_VALUE) {
            printf("Error: no png file was found on the specified folder\n");
            PrintUsage();
            return 1;
        }

        do {
            pngFileList.push_back(std::wstring(fromPngFolder) + L"\\" + findD.cFileName);
        } while (FindNextFile(hFind, &findD) != 0);

        pngFileList.sort();
    }

    // 最初のPNGを取り出しサイズとビットフォーマットを調べます。
    MLImage* firstImg = new MLImage();
    rv = MLPngRead(pngFileList.front().c_str(), *firstImg);
    if (rv < 0) {
        printf("Error: png read failed. %S\n", pngFileList.front().c_str());
        PrintUsage();
        return 1;
    }

    const int w = firstImg->width;
    const int h = firstImg->height;
    const int bitDepth = firstImg->originalBitDepth;
    const int ch = firstImg->originalNumChannels;
    MLAviImageFormat aviImgFmt;

    if (w & 1 || h & 1) {
        printf("Error: image width/height should be even number %d x %d \n", w, h);
        PrintUsage();
        return 1;
    }

    if (bitDepth == 8) {
        aviImgFmt = MLIF_B8G8R8;
    } else if (bitDepth == 16) {
        aviImgFmt = MLIF_RGB10bit_r210;
    } else {
        printf("Error: unsupported png bit depth %d\n", bitDepth);
        PrintUsage();
        return 1;
    }
    const int  bitsPerPixel = MLAviImageFormatToBitsPerPixel2(aviImgFmt);

    if (ch != 3 && ch != 4) {
        printf("Error: unsupported png channels %d\n", ch);
        PrintUsage();
        return 1;
    }

    firstImg->Term();
    delete firstImg;
    firstImg = nullptr;

    // AVIファイル書き込む。
    {
        MLAviWriter aviW;
        const int imgBufBytes = bitsPerPixel * w * h /8;
        uint8_t* imgBuf = new uint8_t[imgBufBytes];
        aviW.Start(toAviFile, w, h, fps, aviImgFmt, false);

        for (const std::wstring& pngPath : pngFileList) {
            MLImage img;
            rv = MLPngRead(pngPath.c_str(), img);
            if (rv < 0) {
                printf("Error: png read failed. %S\n", pngPath.c_str());
                PrintUsage();
                return 1;
            }

            if (w != img.width || h != img.height || bitDepth != img.originalBitDepth || ch != img.originalNumChannels) {
                printf("Error: image format different from the first png. %S\n", pngPath.c_str());
                PrintUsage();
                return 1;
            }

            if (img.bitFormat == MLImage::BFT_UIntR8G8B8A8) {
                conv.R8G8B8A8ToB8G8R8_DIB((const uint32_t*)img.data, imgBuf, w, h);
            } else if (img.bitFormat == MLImage::BFT_UIntR16G16B16A16) {
                conv.R16G16B16A16ToR210((const uint16_t*)img.data, (uint32_t*)imgBuf, w, h);
            } else {
                printf("Error: img bitformat is unsupported. %s\n", MLImage::MLImageBitFormatToStr(img.bitFormat));
                PrintUsage();
                return 1;
            }
            aviW.AddImage((const uint8_t*)imgBuf, imgBufBytes);
        }

        aviW.StopBlocking();

        delete[] imgBuf;
        imgBuf = nullptr;
    }

    return 0;
}
