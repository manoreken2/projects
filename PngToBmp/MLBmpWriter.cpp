#include "MLBmpWriter.h"
#include <Windows.h>
#include "half.h"
#include <stdint.h>

static uint16_t
FloatToUint16(float v) {
    if (v < 0) {
        return 0;
    }
    if (1.0f < v) {
        return 65535;
    }
    return (uint16_t)(65535.0f * v);
}

/// <summary>
/// 24bit BMP または 48bit BMPファイルを書き込む。
/// </summary>
/// <returns>0:成功。負の数:失敗。</returns>
int MLBmpWrite(const char* filePath, MLImage& img)
{
    FILE* fp = nullptr;
    int ercd = fopen_s(&fp, filePath, "wb");
    if (ercd != 0 || fp == nullptr) {
        printf("Error: MLBmpWrite fopen failed. %s\n", filePath);
        return E_FAIL;
    }

    int64_t imgBytes = 0;
    uint8_t* imgTo = nullptr;
    int biBitCount = 0;
    if (img.bitFormat == MLImage::BFT_UInt8) {
        // 24bit BGR画像データ作成。
        biBitCount = 24;
        imgBytes = (int64_t)img.width * img.height * 3 * sizeof(uint8_t); // 3==書き込みデータのnumCh
        imgTo = new uint8_t[imgBytes];
        for (int y = 0; y < img.height; ++y) {
            for (int x = 0; x < img.width; ++x) {
                // imgにはRGBAの4チャンネルデータが左から右、上から下の順に入っている。
                // 24bit BMPはBGR、左から右、下から上の順に書き込む。
                int readP = (x + y * img.width) * 4;
                int writeP = (x + (img.height - y - 1) * img.width) * 3;

                uint8_t r = img.data[readP + 0];
                uint8_t g = img.data[readP + 1];
                uint8_t b = img.data[readP + 2];

                imgTo[writeP + 0] = b;
                imgTo[writeP + 1] = g;
                imgTo[writeP + 2] = r;
            }
        }

    } else if (img.originalBitDepth == 16) {
        // 48bit RGB画像データ作成。
        biBitCount = 48;

        const uint16_t* imgFrom = (const uint16_t*)img.data;

        imgBytes = (int64_t)img.width * img.height * 3 * sizeof(uint16_t); // 3==書き込みデータのnumCh
        imgTo = new uint8_t[imgBytes];
        uint16_t* imgTo16 = (uint16_t*)imgTo;
        for (int y = 0; y < img.height; ++y) {
            for (int x = 0; x < img.width; ++x) {
                // imgにはRGBAの4チャンネルデータが左から右、上から下の順に入っている。
                // 48bit BMPはRGB、左から右、下から上の順に書き込むとirfanviewで正しく表示される。
                int readP = (x + y * img.width) * 4;
                int writeP = (x + (img.height - y - 1) * img.width) * 3;

                uint16_t r = imgFrom[readP + 0];
                uint16_t g = imgFrom[readP + 1];
                uint16_t b = imgFrom[readP + 2];

                imgTo16[writeP + 0] = r;
                imgTo16[writeP + 1] = g;
                imgTo16[writeP + 2] = b;
            }
        }
    } else {
        // 作ってない。
        printf("Error: MLBmpWrite Unsupported bitformat %d\n", img.bitFormat);
        return E_NOTIMPL;
    }

    BITMAPFILEHEADER bmpfh = {};
    BITMAPINFOHEADER bmpih = {};

    bmpfh.bfType = 0x4d42;
    bmpfh.bfSize = (DWORD)(sizeof bmpfh + sizeof bmpih + imgBytes);
    bmpfh.bfOffBits = sizeof bmpfh + sizeof bmpih;

    bmpih.biSize = sizeof bmpih;
    bmpih.biWidth = img.width;
    bmpih.biHeight = img.height;
    bmpih.biPlanes = 1;
    bmpih.biBitCount = biBitCount;
    bmpih.biCompression = BI_RGB;
    bmpih.biSizeImage = (DWORD)imgBytes;
    bmpih.biXPelsPerMeter = 2835;
    bmpih.biYPelsPerMeter = 2835;
    bmpih.biClrUsed = 0;
    bmpih.biClrImportant = 0;

    fwrite(&bmpfh, 1, sizeof bmpfh, fp);
    fwrite(&bmpih, 1, sizeof bmpih, fp);
    fwrite(imgTo, 1, imgBytes, fp);

    fclose(fp);
    fp = nullptr;

    return 0;
}

