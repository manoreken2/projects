#include "MLPngReader.h"
#include <png.h>
#include <stdio.h>
#include <Windows.h>
#include <half.h>
#include <assert.h>

static const char* PngColorTypeToStr(int t) {
    switch (t) {
    case PNG_COLOR_TYPE_GRAY: return "GRAY";
    case PNG_COLOR_TYPE_PALETTE: return "PALETTE";
    case PNG_COLOR_TYPE_RGB: return "RGB";
    case PNG_COLOR_TYPE_RGB_ALPHA: return "RGBA";
    case PNG_COLOR_TYPE_GRAY_ALPHA: return "GRAY_ALPHA";
    default:
        assert(0);
        return "";
    }
}

/// <summary>
/// PNGファイルを読む。
/// </summary>
/// <returns>0:成功。負の数:失敗。1:PNGファイルでは無かった。</returns>
int MLPngRead(const char* pngFilePath, MLImage& img_return)
{
    FILE* fp = nullptr;
    int ercd = fopen_s(&fp, pngFilePath, "rb");
    if (0 != ercd || !fp) {
        printf("Error: PngRead failed. %s\n", pngFilePath);
        return E_FAIL;
    }

    unsigned char header[9] = {};
    const size_t number_to_check = 8;
    fread(header, 1, number_to_check, fp);
    bool is_png = (0 == png_sig_cmp(header, 0, number_to_check));
    if (!is_png) {
        printf("Error: PngRead file is not PNG. %s\n", pngFilePath);
        return 1;
    }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png_ptr) {
        printf("Error: PngRead png_create_read_struct failed.\n");
        return E_FAIL;
    }
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, (png_infopp)nullptr, (png_infopp)nullptr);
        printf("Error: PngRead png_create_info_struct 1 failed.\n");
        return E_FAIL;
    }
    png_infop end_info = png_create_info_struct(png_ptr);
    if (!end_info) {
        png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)nullptr);
        printf("Error: PngRead png_create_info_struct 2 failed.\n");
        return E_FAIL;
    }

    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, number_to_check);

    png_read_info(png_ptr, info_ptr);
    png_uint_32 width, height;
    int orig_bit_depth, color_type, interlace_type;
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &orig_bit_depth, &color_type, &interlace_type, nullptr, nullptr);

    if (orig_bit_depth != 8 && orig_bit_depth != 16) {
        png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)nullptr);
        printf("Error: PngRead unsupported bit depth %d.\n", orig_bit_depth);
        return E_FAIL;
    }

    int orig_num_channels = 3;
    switch (color_type) {
    case PNG_COLOR_TYPE_RGB:
        orig_num_channels = 3;
        break;
    case PNG_COLOR_TYPE_RGB_ALPHA:
        orig_num_channels = 4;
        break;
    default:
        png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)nullptr);
        printf("Error: PngRead unsupported color format %s.\n",
            PngColorTypeToStr(color_type));
        return E_FAIL;
    }

    uint8_t* image_data = new uint8_t[width * height * orig_num_channels *  orig_bit_depth / 8];

    png_bytepp row_pointers = (png_bytepp)png_malloc(png_ptr, sizeof(png_bytepp) * height);
    for (uint32_t y = 0; y < height; y++) {
        row_pointers[y] = (png_bytep)&image_data[y*(width * orig_num_channels * orig_bit_depth / 8)];
    }
    png_set_rows(png_ptr, info_ptr, row_pointers);
    png_read_image(png_ptr, row_pointers);
    png_read_end(png_ptr, nullptr);

    png_free(png_ptr, row_pointers);
    row_pointers = nullptr;

    png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
    png_ptr = nullptr;
    info_ptr = nullptr;

    // img_return.dataに値を詰めます。
    img_return.Init(width, height,
        MLImage::IM_HALF_RGBA,
        MLImage::IFFT_PNG,
        MLImage::BFT_HalfFloat,
        ML_CG_Rec709,     //< すぐ後で変更する場合有り。
        MLImage::MLG_G22, //< すぐ後で変更する場合有り。
        orig_bit_depth,
        orig_num_channels,
        width * height * 4 * 2, // 4==numCh, 2== sizeof(half)
        new uint8_t[width * height * 4 * 2]);

    half* imgTo = (half*)img_return.data;
    if (orig_bit_depth == 8) {
        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                int readP = (x + y * width) * orig_num_channels;
                int writeP = (x + y * width) * 4;

                int r = image_data[readP + 0];
                int g = image_data[readP + 1];
                int b = image_data[readP + 2];
                int a = 255;
                imgTo[writeP + 0] = half(r / 255.0f);
                imgTo[writeP + 1] = half(g / 255.0f);
                imgTo[writeP + 2] = half(b / 255.0f);
                imgTo[writeP + 3] = half(a / 255.0f);
            }
        }
    } else if (orig_bit_depth == 16) {
        img_return.colorGamut = ML_CG_Rec2020;
        img_return.gamma = MLImage::MLG_ST2084;

        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                int readP = (x + y * width) * orig_num_channels * 2;
                int writeP = (x + y * width) * 4;

                int r = (image_data[readP + 0] << 8) + image_data[readP + 1];
                int g = (image_data[readP + 2] << 8) + image_data[readP + 3];
                int b = (image_data[readP + 4] << 8) + image_data[readP + 5];
                int a = 65535;
                imgTo[writeP + 0] = half(r / 65535.0f);
                imgTo[writeP + 1] = half(g / 65535.0f);
                imgTo[writeP + 2] = half(b / 65535.0f);
                imgTo[writeP + 3] = half(a / 65535.0f);
            }
        }
    } else {
        // ここには来ない。
        assert(0);
    }

    delete[] image_data;
    image_data = nullptr;

    return 0;
}




