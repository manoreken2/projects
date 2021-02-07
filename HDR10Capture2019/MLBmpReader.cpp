#include "MLBmpReader.h"
#include <stdio.h>
#include <Windows.h>
#include <stdint.h>
#include "half.h"

/// <summary>
/// BMPファイルを読む。
/// </summary>
/// <returns>0:成功。負の数:失敗。1:BMPファイルでは無かった。</returns>
int MLBmpRead(const wchar_t* filePath, MLImage& img_return) {
    FILE* fp = nullptr;

    int ercd = _wfopen_s(&fp, filePath, L"rb");
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
    const int64_t bytes = (int64_t)width * height * bmpih.biBitCount /8;
    int orig_bit_depth = 8;
    int orig_num_channels = 3;
    MLColorGamutType colorGamut = ML_CG_Rec709;
    MLImage::GammaType gammaType = MLImage::MLG_G22;
    MLImage::BitFormatType bitFmt = MLImage::BFT_UIntR8G8B8A8;
    switch (bmpih.biBitCount) {
    case 24:
        orig_bit_depth = 8;
        orig_num_channels = 3;
        bitFmt = MLImage::BFT_UIntR8G8B8A8;
        colorGamut = ML_CG_Rec709;
        gammaType = MLImage::MLG_G22;
        break;
    case 32:
        orig_bit_depth = 8;
        orig_num_channels = 4;
        bitFmt = MLImage::BFT_UIntR8G8B8A8;
        colorGamut = ML_CG_Rec709;
        gammaType = MLImage::MLG_G22;
        break;
    case 48:
        orig_bit_depth = 16;
        orig_num_channels = 3;
        // 16bit画像の時HDR10 PQ画像が入っているという想定。
        bitFmt = MLImage::BFT_UIntR16G16B16A16;
        colorGamut = ML_CG_Rec2020;
        gammaType = MLImage::MLG_ST2084;
        break;
    case 64:
        orig_bit_depth = 16;
        orig_num_channels = 4;
        // 16bit画像の時HDR10 PQ画像が入っているという想定。
        bitFmt = MLImage::BFT_UIntR16G16B16A16;
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

    if (orig_bit_depth == 8) {
        img_return.Term();
        img_return.Init(width, height, MLImage::IFFT_BMP, bitFmt,
            colorGamut,
            gammaType,
            orig_bit_depth,
            orig_num_channels,
            width * height * 4 * sizeof(uint8_t), // 4==numCh
            new uint8_t[width * height * 4 * sizeof(uint8_t)]);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                // 24bit BMPはBGR、左から右、下から上の順に値が入っている。
                // MLImageはRGBA、左から右、上から下の順。
                int readP = (x + (height - y - 1) * width) * orig_num_channels;
                int writeP = (x + y * width) * 4;

                uint8_t b = bmpImg[readP + 0];
                uint8_t g = bmpImg[readP + 1];
                uint8_t r = bmpImg[readP + 2];
                uint8_t a = 255;
                img_return.data[writeP + 0] = r;
                img_return.data[writeP + 1] = g;
                img_return.data[writeP + 2] = b;
                img_return.data[writeP + 3] = a;
            }
        }
    } else if (orig_bit_depth == 16) {
        img_return.Term();
        img_return.Init(width, height, MLImage::IFFT_BMP, bitFmt,
            colorGamut,
            gammaType,
            orig_bit_depth,
            orig_num_channels,
            width * height * 4 * sizeof(uint16_t), // 4==numCh
            new uint8_t[width * height * 4 * sizeof(uint16_t)]);

        uint16_t* bmpFrom = (uint16_t*)bmpImg;
        uint16_t* imgTo = (uint16_t*)img_return.data;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int readP = (x + (height - y - 1) * width) * orig_num_channels;
                int writeP = (x + y * width) * 4;

                uint16_t b = bmpFrom[readP + 0];
                uint16_t g = bmpFrom[readP + 1];
                uint16_t r = bmpFrom[readP + 2];
                uint16_t a = 65535;
                imgTo[writeP + 0] = r;
                imgTo[writeP + 1] = g;
                imgTo[writeP + 2] = b;
                imgTo[writeP + 3] = a;
            }
        }
    } else {
        assert(0);
    }

    delete[] bmpImg;
    bmpImg = nullptr;

    return 0;
}