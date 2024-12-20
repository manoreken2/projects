#pragma once

#include <stdint.h>
#include <string>
#include "MLColorGamut.h"


struct MLImage2 {
    enum ImageFileFormatType {
        IFFT_None,
        IFFT_OpenEXR,
        IFFT_PNG,
        IFFT_BMP,
        IFFT_CapturedImg,
    };

    enum BitFormatType {
        BFT_None,
        BFT_HalfFloatR16G16B16A16,
        BFT_UIntR8G8B8A8,
        BFT_UIntR10G10B10A2,
        BFT_UIntR16G16B16A16,
    };

    enum GammaType {
        MLG_Linear, //< 100nits == 1.0
        MLG_G22,
        MLG_ST2084,
        MLG_HLG,
    };

    /// <summary>
    /// MLConverter::QuantizationRange�Ɠ����B
    /// </summary>
    enum QuantizationRange {
        QR_Full,
        QR_Limited,
    };

    /// <summary>
    /// new[]�Ŋm�ۂ��ĉ������B
    /// </summary>
    uint8_t *data = nullptr;

    int bytes = 0;
    int width = 0;
    int height = 0;
    ImageFileFormatType imgFileFormat = IFFT_None;
    BitFormatType bitFormat = BFT_None;
    MLColorGamutType colorGamut = ML_CG_Rec709;
    GammaType gamma = MLG_G22;
    QuantizationRange quantizationRange = QR_Full;

    /// <summary>
    /// �r�b�g�f�v�X�B8bit, 10bit���B
    /// </summary>
    int originalBitDepth = 8;

    /// <summary>
    /// �`�����l�����BRGB�̂Ƃ�3�ARGBA�̂Ƃ�4�B
    /// </summary>
    int originalNumChannels = 3;

    /// <summary>
    /// ���ׂẴ����o�ϐ����Z�b�g���܂��B
    /// </summary>
    /// <param name="aOriginalBitDepth">�r�b�g�f�v�X�B8bit�A10bit���B</param>
    /// <param name="aOriginalNumChannels">�`�����l�����BRGB�̂Ƃ�3�ARGBA�̂Ƃ�4�B</param>
    /// <param name="aBytes">aData�̃o�C�g���B</param>
    /// <param name="aData">�摜�������Ă���o�b�t�@�Bnew[]�Ŋm�ۂ��ēn���܂��B</param>
    void Init(int aW, int aH, ImageFileFormatType iff, BitFormatType bf, MLColorGamutType cg, GammaType ga, int aOriginalBitDepth, int aOriginalNumChannels, int aBytes, uint8_t *aData) {
        data = aData;
        bytes = aBytes;
        width = aW;
        height = aH;
        imgFileFormat = iff;
        bitFormat = bf;
        colorGamut = cg;
        gamma = ga;
        originalBitDepth = aOriginalBitDepth;
        originalNumChannels = aOriginalNumChannels;
    }

    /// <summary>
    /// data��delete[]���Abytes==0���Z�b�g���܂��B
    /// </summary>
    void DeleteData(void) {
        delete[] data;
        data = nullptr;
        bytes = 0;
    }

    void Term(void) {
        width = 0;
        height = 0;
        bytes = 0;
        delete[] data;
        data = nullptr;
    }

    void DeepCopyFrom(const MLImage2 &r) {
        assert(data == nullptr);

        data = nullptr;
        bytes = r.bytes;
        width = r.width;
        height = r.height;
        imgFileFormat = r.imgFileFormat;
        bitFormat = r.bitFormat;
        colorGamut = r.colorGamut;
        gamma = r.gamma;
        originalBitDepth = r.originalBitDepth;
        originalNumChannels = r.originalNumChannels;

        if (r.data != nullptr) {
            data = new uint8_t[bytes];
            memcpy(data, r.data, bytes);
        }
    }

    static const char* MLImageFileFormatTypeToStr(ImageFileFormatType t);

    static const char* MLImageBitFormatToStr(BitFormatType t);

    static const char* GammaTypeToStr(GammaType t);
};
