#include "MLBmpReader.h"
#include <stdio.h>
#include <Windows.h>
#include <stdint.h>
#include <half.h>

/// <summary>
/// BMPファイルを読む。
/// </summary>
/// <returns>0:成功。負の数:失敗。1:BMPファイルでは無かった。</returns>
int MLBmpRead(const char* filePath, MLImage& img_return) {
    FILE* fp = nullptr;

    int ercd = fopen_s(&fp, filePath, "rb");
    if (ercd != 0 || fp == nullptr) {
        return E_FAIL;
    }

    BITMAPFILEHEADER bmpfh;
    size_t sz = fread(&bmpfh, 1, sizeof bmpfh, fp);
    if (sz < sizeof bmpfh || bmpfh.bfType != 0x4d42) {
        // BMPファイルでは無い。
        fclose(fp);
        fp = nullptr;
        return 1;
    }

    if (bmpfh.bfOffBits != 0x36) {
        // サポートしてない形式。
        fclose(fp);
        fp = nullptr;
        return E_UNEXPECTED;
    }

    BITMAPINFOHEADER bmpih;
    sz = fread(&bmpih, 1, sizeof bmpih, fp);
    if (bmpih.biSize != 0x28 || bmpih.biCompression != 0 || bmpih.biBitCount != 24) {
        // サポートしてない形式。
        fclose(fp);
        fp = nullptr;
        return E_UNEXPECTED;
    }

    const int width = bmpih.biWidth;
    const int height = bmpih.biHeight;
    const int bytes = 3 * width * height;
    const int orig_bit_depth = 8;
    const int orig_num_channels = 3;
    uint8_t* bgrImg = new uint8_t[bytes];

    sz = fread(&bgrImg[0], 1, bytes, fp);
    if (sz != bytes) {
        // ファイル読み出しエラー。
        fclose(fp);
        fp = nullptr;
        return E_FAIL;
    }

    fclose(fp);
    fp = nullptr;

    img_return.Init(width, height,
        MLImage::IM_HALF_RGBA, MLImage::IFFT_BMP, MLImage::BFT_HalfFloat,
        ML_CG_Rec709,
        MLImage::MLG_G22,
        orig_bit_depth,
        orig_num_channels,
        width * height * 4 * 2, // 4==numCh, 2== sizeof(half)
        new uint8_t[width * height * 4 * 2]);
    
    half* imgTo = (half*)img_return.data;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // BMPは下から上、左から右、BGRの順に値が入っている。
            int readP = (x + (height -y -1) * width) * orig_num_channels;
            int writeP = (x + y * width) * 4;

            int r = bgrImg[readP + 2];
            int g = bgrImg[readP + 1];
            int b = bgrImg[readP + 0];
            int a = 255;
            imgTo[writeP + 0] = half(r / 255.0f);
            imgTo[writeP + 1] = half(g / 255.0f);
            imgTo[writeP + 2] = half(b / 255.0f);
            imgTo[writeP + 3] = half(a / 255.0f);
        }
    }

    delete[] bgrImg;
    bgrImg = nullptr;

    return 0;
}