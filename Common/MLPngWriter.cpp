#include "MLPngWriter.h"
#include <png.h>
#include <stdio.h>
#include <Windows.h>
#include <half.h>
#include <assert.h>
#include <stdint.h>

static uint16_t
HtoNS(uint16_t v)
{
    return   ((v & 0xff) << 8)
           | ((v >> 8) & 0xff);
}

/// <summary>
/// PNGファイルを書き込む。
/// </summary>
/// <returns>0:成功。負の数:失敗。</returns>
int MLPngWrite(const wchar_t* pngFilePath, const MLImage& img)
{
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        printf("Error: png_create_write_struct failed.\n");
        return E_FAIL;
    }
    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, &info);
        printf("Error: png_create_info_struct failed.\n");
        return E_FAIL;
    }

    // 書き込む画像データを準備します。
    uint8_t* data = nullptr;
    png_bytepp rows = nullptr;

    int bitDepth = 8;
    if (8 == img.originalBitDepth) {
        // R8G8B8A8
        bitDepth = 8;
        switch (img.bitFormat) {
        case MLImage::BFT_UIntR8G8B8A8:
            break;
        default:
            // 作ってない。
            return E_NOTIMPL;
        }

        rows = (png_bytepp)png_malloc(png, img.height * sizeof(png_bytep));
        data = new uint8_t[(int64_t)img.width * img.height * 3 * bitDepth / 8];
        for (int y = 0; y < img.height; ++y) {
            rows[y] = (png_bytep)(data + (0 + y * img.width) * 3);
            for (int x = 0; x < img.width; ++x) {
                const int readPos = (x + y * img.width) * 4;

                const uint8_t r = img.data[readPos + 0];
                const uint8_t g = img.data[readPos + 1];
                const uint8_t b = img.data[readPos + 2];

                const int writePos = (x + y * img.width) * 3;
                data[writePos + 0] = r;
                data[writePos + 1] = g;
                data[writePos + 2] = b;
            }
        }
    } else {
        // R10G10B10A2
        bitDepth = 16;
        switch (img.bitFormat) {
        case MLImage::BFT_UIntR10G10B10A2:
            break;
        default:
            // 作ってない。
            return E_NOTIMPL;
        }

        uint32_t* from32 = (uint32_t*)img.data;
        rows = (png_bytepp)png_malloc(png, img.height * sizeof(png_bytep));
        data = new uint8_t[(int64_t)img.width * img.height * 3 * bitDepth / 8];
        uint16_t* data16 = (uint16_t*)data;
        for (int y = 0; y < img.height; ++y) {
            rows[y] = (uint8_t*)&data16[(0 + y * img.width) * 3];
            for (int x = 0; x < img.width; ++x) {
                const int readPos = (x + y * img.width);
                const uint32_t v = from32[readPos];
                // v: AABBBBBB BBBBGGGG GGGGGGRR RRRRRRRR

                uint16_t r = (v << 6) & 0xffc0;
                uint16_t g = (v >> 4) & 0xffc0;
                uint16_t b = (v >> 14) & 0xffc0;

                const int writePos = (x + y * img.width) * 3;
                data16[writePos + 0] = HtoNS(r);
                data16[writePos + 1] = HtoNS(g);
                data16[writePos + 2] = HtoNS(b);
            }
        }
    }

    // ここでファイルを書き込みオープン。
    FILE* fp = nullptr;
    int ercd = _wfopen_s(&fp, pngFilePath, L"wb");
    if (0 != ercd || !fp) {
        printf("Error: PngWrite failed. %S\n", pngFilePath);

        png_free(png, rows);
        rows = nullptr;
        
        png_destroy_write_struct(&png, &info);

        delete[] data;
        return E_FAIL;
    }

    png_init_io(png, fp);

    png_set_IHDR(png, info, img.width, img.height, bitDepth, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    png_write_info(png, info);
    png_set_packing(png);

    png_write_image(png, rows);
    png_write_end(png, info);

    png_free(png, rows);
    rows = nullptr;

    png_destroy_write_struct(&png, &info);

    fclose(fp);

    return S_OK;
}




