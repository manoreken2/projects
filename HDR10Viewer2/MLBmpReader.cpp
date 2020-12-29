#include "MLBmpReader.h"
#include <stdio.h>
#include <Windows.h>
#include <stdint.h>
#include "half.h"

/// <summary>
/// BMP�t�@�C����ǂށB
/// </summary>
/// <returns>0:�����B���̐�:���s�B1:BMP�t�@�C���ł͖��������B</returns>
int MLBmpRead(const char* filePath, MLImage& img_return) {
    FILE* fp = nullptr;

    int ercd = fopen_s(&fp, filePath, "rb");
    if (ercd != 0 || fp == nullptr) {
        return E_FAIL;
    }

    BITMAPFILEHEADER bmpfh;
    size_t sz = fread(&bmpfh, 1, sizeof bmpfh, fp);
    if (sz < sizeof bmpfh || bmpfh.bfType != 0x4d42) {
        // BMP�t�@�C���ł͖����B
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
        // �T�|�[�g���ĂȂ��`���B
        fclose(fp);
        fp = nullptr;
        return E_UNEXPECTED;
    }

    // �摜�f�[�^�܂ŃX�L�b�v����B
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
        // 16bit�摜�̎�HDR10 PQ�摜�������Ă���Ƃ����z��B
        colorGamut = ML_CG_Rec2020;
        gammaType = MLImage::MLG_ST2084;
        break;
    case 64:
        orig_bit_depth = 16;
        orig_num_channels = 4;
        // 16bit�摜�̎�HDR10 PQ�摜�������Ă���Ƃ����z��B
        colorGamut = ML_CG_Rec2020;
        gammaType = MLImage::MLG_ST2084;
        break;
    default:
        // �����ɂ͗��Ȃ��B
        assert(0);
        break;
    }
    uint8_t* bmpImg = new uint8_t[bytes];

    sz = fread(&bmpImg[0], 1, bytes, fp);
    if (sz != bytes) {
        // �t�@�C���ǂݏo���G���[�B
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
                // 24bit BMP�͉������A������E�ABGR�̏��ɒl�������Ă���B
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
                // 48bit BMP�͉������A������E�ARGB�̏��ɒl�������IrfanView�Ő������\�������B(�H)
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