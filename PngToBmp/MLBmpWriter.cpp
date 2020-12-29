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
/// 48bit BMPファイルを書き込む。
/// </summary>
/// <returns>0:成功。負の数:失敗。</returns>
int MLBmpWrite(const char* filePath, MLImage& img) {
    FILE* fp = nullptr;
    int ercd = fopen_s(&fp, filePath, "wb");
    if (ercd != 0 || fp == nullptr) {
        printf("Error: MLBmpWrite fopen failed. %s\n", filePath);
        return E_FAIL;
    }

    // 48bit BGR画像データ作成。
    int imgBytes = img.width * img.height * 3 * 2; // 3==numCh, 2 == bytesPerChannel
    uint16_t* imgTo = new uint16_t[imgBytes];
    const half* imgFrom = (const half*)img.data;
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            int readP = (x + y * img.width) * 4;
            int writeP = (x + (img.height - y - 1) * img.width) * 3;

            uint16_t r = FloatToUint16(imgFrom[readP + 0]);
            uint16_t g = FloatToUint16(imgFrom[readP + 1]);
            uint16_t b = FloatToUint16(imgFrom[readP + 2]);

            imgTo[writeP + 0] = r;
            imgTo[writeP + 1] = g;
            imgTo[writeP + 2] = b;
        }
    }

    BITMAPFILEHEADER bmpfh = {};
    BITMAPINFOHEADER bmpih = {};

    bmpfh.bfType = 0x4d42;
    bmpfh.bfSize = sizeof bmpfh + sizeof bmpih + imgBytes;
    bmpfh.bfOffBits = sizeof bmpfh + sizeof bmpih;

    bmpih.biSize = sizeof bmpih;
    bmpih.biWidth = img.width;
    bmpih.biHeight = img.height;
    bmpih.biPlanes = 1;
    bmpih.biBitCount = 48; // 48bit BMP
    bmpih.biCompression = BI_RGB;
    bmpih.biSizeImage = imgBytes;
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

