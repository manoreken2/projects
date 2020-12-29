#include "MLBmpReader.h"
#include <stdio.h>
#include <Windows.h>
#include <stdint.h>
#include "half.h"

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

    BITMAPINFOHEADER bmpih;
    sz = fread(&bmpih, 1, sizeof bmpih, fp);
    if (bmpih.biSize < 0x28 
        || (bmpih.biCompression != BI_RGB
            && bmpih.biCompression != BI_BITFIELDS)
        || (bmpih.biBitCount != 24
            && bmpih.biBitCount != 32
            && bmpih.biBitCount != 48
            && bmpih.biBitCount != 64)) {
        // サポートしてない形式。
        fclose(fp);
        fp = nullptr;
        return E_UNEXPECTED;
    }

    // 画像データまでスキップする。
    fseek(fp, bmpfh.bfOffBits, SEEK_SET);

    const int width = bmpih.biWidth;
    const int height = bmpih.biHeight;
    const int bytes = width * height * bmpih.biBitCount /8;
    int orig_bit_depth = 8;
    int orig_num_channels = 3;
    MLColorGamutType colorGamut = ML_CG_Rec709;
    MLImage::GammaType gammaType = MLImage::MLG_G22;
    switch (bmpih.biBitCount) {
    case 24:
        orig_bit_depth = 8;
        orig_num_channels = 3;
        colorGamut = ML_CG_Rec709;
        gammaType = MLImage::MLG_G22;
        break;
    case 32:
        orig_bit_depth = 8;
        orig_num_channels = 4;
        colorGamut = ML_CG_Rec709;
        gammaType = MLImage::MLG_G22;
        break;
    case 48:
        orig_bit_depth = 16;
        orig_num_channels = 3;
        // 16bit画像の時HDR10 PQ画像が入っているという想定。
        colorGamut = ML_CG_Rec2020;
        gammaType = MLImage::MLG_ST2084;
        break;
    case 64:
        orig_bit_depth = 16;
        orig_num_channels = 4;
        // 16bit画像の時HDR10 PQ画像が入っているという想定。
        colorGamut = ML_CG_Rec2020;
        gammaType = MLImage::MLG_ST2084;
        break;
    default:
        // ここには来ない。
        assert(0);
        break;
    }
    uint8_t* bmpImg = new uint8_t[bytes];

    sz = fread(&bmpImg[0], 1, bytes, fp);
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
        colorGamut,
        gammaType,
        orig_bit_depth,
        orig_num_channels,
        width * height * 4 * 2, // 4==numCh, 2== sizeof(half)
        new uint8_t[width * height * 4 * 2]);
    
    half* imgTo = (half*)img_return.data;
    if (orig_bit_depth == 8) {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                // 24bit BMPは下から上、左から右、BGRの順に値が入っている。
                int readP = (x + (height - y - 1) * width) * orig_num_channels;
                int writeP = (x + y * width) * 4;

                int r = bmpImg[readP + 2];
                int g = bmpImg[readP + 1];
                int b = bmpImg[readP + 0];
                int a = 255;
                imgTo[writeP + 0] = half(r / 255.0f);
                imgTo[writeP + 1] = half(g / 255.0f);
                imgTo[writeP + 2] = half(b / 255.0f);
                imgTo[writeP + 3] = half(a / 255.0f);
            }
        }
    } else if (orig_bit_depth == 16) {
        uint16_t* bmp16 = (uint16_t*)bmpImg;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                // 48bit BMPは下から上、左から右、RGBの順に値を入れるとIrfanViewで正しく表示される。(？)
                int readP = (x + (height - y - 1) * width) * orig_num_channels;
                int writeP = (x + y * width) * 4;

                int r = bmp16[readP + 0];
                int g = bmp16[readP + 1];
                int b = bmp16[readP + 2];
                int a = 65535;
                imgTo[writeP + 0] = half(r / 65535.0f);
                imgTo[writeP + 1] = half(g / 65535.0f);
                imgTo[writeP + 2] = half(b / 65535.0f);
                imgTo[writeP + 3] = half(a / 65535.0f);
            }
        }
    } else {
        assert(0);
    }

    delete[] bmpImg;
    bmpImg = nullptr;

    return 0;
}