#include "MLDrawings.h"

void
MLDrawings::AddCrosshair(MLImage &ci, CrosshairType ct)
{
    if (ct == CH_None) {
        return;
    }

    uint32_t *pTo = (uint32_t*)ci.data;

    int HALF_LENGTH = 40;
    int HALF_THICKNESS = 3;
    if (ci.width <= 1920) {
        HALF_LENGTH /= 2;
        HALF_THICKNESS /= 2;
    }

    const int width = ci.width;
    const int height = ci.height;

    //                 a             y              cb         cr
    uint32_t color = (0xff << 24) + (254 << 16) + (128 << 8) + 128;
    switch (ci.imgMode) {
    case MLImage::IM_RGB:
    case MLImage::IM_HALF_RGBA:
        //        A B G R
        color = 0xffffffff;
        break;
    default:
        break;
    }

    switch (ct) {
    case CH_CenterCrosshair:
        // 横線
        for (int y = height / 2 - HALF_THICKNESS; y < height / 2 + HALF_THICKNESS; ++y) {
            for (int x = width / 2 - HALF_LENGTH; x < width / 2 + HALF_LENGTH; ++x) {
                int pos = x + y * width;
                pTo[pos] = color;
            }
        }
        // 縦線
        for (int y = height / 2 - HALF_LENGTH; y < height / 2 + HALF_LENGTH; ++y) {
            for (int x = width / 2 - HALF_THICKNESS; x < width / 2 + HALF_THICKNESS; ++x) {
                int pos = x + y * width;
                pTo[pos] = color;
            }
        }
        break;
    case CH_4Crosshairs:
        for (int yi = 0; yi <= 1; ++yi) {
            for (int xi = 0; xi <= 1; ++xi) {
                // 横線
                for (int y = height / 4 - HALF_THICKNESS + yi * height / 2; y < height / 4 + HALF_THICKNESS + yi * height / 2; ++y) {
                    for (int x = width / 4 - HALF_LENGTH + xi * width / 2; x < width / 4 + HALF_LENGTH + xi * width / 2; ++x) {
                        int pos = x + y * width;
                        pTo[pos] = color;
                    }
                }
                // 縦線
                for (int y = height / 4 - HALF_LENGTH + yi * height / 2; y < height / 4 + HALF_LENGTH + yi * height / 2; ++y) {
                    for (int x = width / 4 - HALF_THICKNESS + xi * width / 2; x < width / 4 + HALF_THICKNESS + xi * width / 2; ++x) {
                        int pos = x + y * width;
                        pTo[pos] = color;
                    }
                }
            }
        }
        break;
    default:
        break;
    }
}

void
MLDrawings::AddTitleSafeArea(MLImage &ci)
{
    uint32_t *pTo = (uint32_t*)ci.data;

    int HALF_THICKNESS = 3;
    if (ci.width <= 1920) {
        HALF_THICKNESS /= 2;
    }

    const int width = ci.width;
    const int height = ci.height;

    //                 a             y              cb         cr
    uint32_t color = (0xff << 24) + (254 << 16) + (128 << 8) + 128;
    switch (ci.imgMode) {
    case MLImage::IM_RGB:
    case MLImage::IM_HALF_RGBA:
        //        A B G R
        color = 0xffffffff;
        break;
    default:
        break;
    }

    // 横線
    for (int y = height / 10 - HALF_THICKNESS; y < height / 10 + HALF_THICKNESS; ++y) {
        for (int x = width / 10; x < width - width / 10; ++x) {
            int pos = x + y * width;
            pTo[pos] = color;
        }
    }
    for (int y = height - height / 10 - HALF_THICKNESS; y < height - height / 10 + HALF_THICKNESS; ++y) {
        for (int x = width / 10; x < width - width / 10; ++x) {
            int pos = x + y * width;
            pTo[pos] = color;
        }
    }
    // 縦線
    for (int y = height / 10 - HALF_THICKNESS; y < height - height / 10 + HALF_THICKNESS; ++y) {
        for (int x = width / 10 - HALF_THICKNESS; x < width / 10 + HALF_THICKNESS; ++x) {
            int pos = x + y * width;
            pTo[pos] = color;
        }
    }
    for (int y = height / 10 - HALF_THICKNESS; y < height - height / 10 + HALF_THICKNESS; ++y) {
        for (int x = width - width / 10 - HALF_THICKNESS; x < width - width / 10 + HALF_THICKNESS; ++x) {
            int pos = x + y * width;
            pTo[pos] = color;
        }
    }
}

void
MLDrawings::AddGrid(MLImage &ci, GridType gt)
{
    if (gt == GR_None) {
        return;
    }

    uint32_t *pTo = (uint32_t*)ci.data;

    int HALF_THICKNESS = 3;
    if (ci.width <= 1920) {
        HALF_THICKNESS /= 2;
    }

    const int width = ci.width;
    const int height = ci.height;

    //                 a             y              cb         cr
    uint32_t color = (0xff << 24) + (254 << 16) + (128 << 8) + 128;
    switch (ci.imgMode) {
    case MLImage::IM_RGB:
    case MLImage::IM_HALF_RGBA:
        //        A B G R
        color = 0xffffffff;
        break;
    default:
        break;
    }

    int nBlocks = 3;
    if (gt == GR_6x6) {
        nBlocks = 6;
    }

    // 横線
    for (int i = 1; i < nBlocks; ++i) {
        for (int y = i * height / nBlocks - HALF_THICKNESS; y < i * height / nBlocks + HALF_THICKNESS; ++y) {
            for (int x = 0; x < width; ++x) {
                int pos = x + y * width;
                pTo[pos] = color;
            }
        }
    }
    // 縦線
    for (int i = 1; i < nBlocks; ++i) {
        for (int y = 0; y < height; ++y) {
            for (int x = i * width / nBlocks - HALF_THICKNESS; x < i * width / nBlocks + HALF_THICKNESS; ++x) {
                int pos = x + y * width;
                pTo[pos] = color;
            }
        }
    }
}



