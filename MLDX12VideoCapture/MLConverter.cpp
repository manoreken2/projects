#include "MLConverter.h"
#include <ctgmath>
#include <assert.h>
#include <algorithm>

static const int BAYER_W = 3840 + 32;
static const int BAYER_H = 2160 + 32;
static const int BAYER_WH = BAYER_W * BAYER_H;

static uint32_t
NtoHL(uint32_t v)
{
    return   ((v & 0xff) << 24) |
        (((v >> 8) & 0xff) << 16) |
        (((v >> 16) & 0xff) << 8) |
        (((v >> 24) & 0xff));
}

MLConverter::MLConverter(void)
{
    memset(mGammaTableR, 0, sizeof mGammaTableR);
    memset(mGammaTableG, 0, sizeof mGammaTableG);
    memset(mGammaTableB, 0, sizeof mGammaTableB);

    /* G R
     * B G
     */
    mGammaBayerTable[0] = mGammaTableG;
    mGammaBayerTable[1] = mGammaTableR;
    mGammaBayerTable[2] = mGammaTableB;
    mGammaBayerTable[3] = mGammaTableG;

    mBayer = new uint8_t[BAYER_WH];
    CreateGammaTable(2.2f, 1.0f, 1.0f, 1.0f);
}

MLConverter::~MLConverter(void)
{
    delete[] mBayer;
    mBayer = nullptr;
}

void
MLConverter::CreateGammaTable(const float gamma, const float gainR, const float gainG, const float gainB)
{
    // 12bit value to 8bit value

    for (int i = 0; i < 4096; ++i) {
        const float y = (const float)(i) / 4095.0f;
        const float r = pow(std::min(1.0f, y * gainR), 1.0f / gamma);
        const float g = pow(std::min(1.0f, y * gainG), 1.0f / gamma);
        const float b = pow(std::min(1.0f, y * gainB), 1.0f / gamma);
        mGammaTableR[i] = (int)floor(255.0f * r + 0.5f);
        mGammaTableG[i] = (int)floor(255.0f * g + 0.5f);
        mGammaTableB[i] = (int)floor(255.0f * b + 0.5f);
    }
}

void
MLConverter::Rgb10bitToRGBA(uint32_t *pFrom, uint32_t *pTo, const int width, const int height, const uint8_t alpha)
{
    const uint32_t a = alpha;
    int pos = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const uint32_t v = NtoHL(pFrom[pos]);
            const uint32_t r = (v >> 22) & 0xff;
            const uint32_t g = (v >> 12) & 0xff;
            const uint32_t b = (v >> 2) & 0xff;
            pTo[pos] = (a << 24) + (b << 16) + (g << 8) + r;

            ++pos;
        }
    }
}

void
MLConverter::YuvV210ToYuvA(uint32_t *pFrom, uint32_t *pTo, const int width, const int height, const uint8_t alpha)
{
    const uint8_t a = alpha;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width / 6; ++x) {
            const int posF = 4 * (x + y * width / 6);
            const int posT = 6 * (x + y * width / 6);

            const uint32_t w0 = pFrom[posF];
            const uint32_t w1 = pFrom[posF + 1];
            const uint32_t w2 = pFrom[posF + 2];
            const uint32_t w3 = pFrom[posF + 3];

            const uint8_t cr0 = (w0 >> 22) & 0xff;
            const uint8_t y0 = (w0 >> 12) & 0xff;
            const uint8_t cb0 = (w0 >> 2) & 0xff;

            const uint8_t y2 = (w1 >> 22) & 0xff;
            const uint8_t cb2 = (w1 >> 12) & 0xff;
            const uint8_t y1 = (w1 >> 2) & 0xff;

            const uint8_t cb4 = (w2 >> 22) & 0xff;
            const uint8_t y3 = (w2 >> 12) & 0xff;
            const uint8_t cr2 = (w2 >> 2) & 0xff;

            const uint8_t y5 = (w3 >> 22) & 0xff;
            const uint8_t cr4 = (w3 >> 12) & 0xff;
            const uint8_t y4 = (w3 >> 2) & 0xff;

            pTo[posT + 0] = (a << 24) + (y0 << 16) + (cb0 << 8) + cr0;
            pTo[posT + 1] = (a << 24) + (y1 << 16) + (cb0 << 8) + cr0;
            pTo[posT + 2] = (a << 24) + (y2 << 16) + (cb2 << 8) + cr2;
            pTo[posT + 3] = (a << 24) + (y3 << 16) + (cb2 << 8) + cr2;
            pTo[posT + 4] = (a << 24) + (y4 << 16) + (cb4 << 8) + cr4;
            pTo[posT + 5] = (a << 24) + (y5 << 16) + (cb4 << 8) + cr4;

            //posF += 4;
            //posT += 6;
        }
    }
}

void
MLConverter::RawYuvV210ToRGBA(uint32_t *pFrom, uint32_t *pTo, const int width, const int height, const uint8_t alpha)
{
    assert(width == 3840);
    assert(height == 2160);
    
    int x = 0;
    int y = 0;
    int posT = 0;
    for (int i = 0;; i += 4) {
        // Decklink SDK p.217
        // extract lower 8bit from yuv422 10bit
        // upper 2bits are discarded
        const uint32_t pF0 = pFrom[i + 0];
        const uint32_t pF1 = pFrom[i + 1];
        const uint32_t pF2 = pFrom[i + 2];
        const uint32_t pF3 = pFrom[i + 3];

        const uint8_t cr0 = (pF0 & 0x0ff00000) >> 20;
        const uint8_t y0  = (pF0 & 0x0003fc00) >> 10;
        const uint8_t cb0 = (pF0 & 0x000000ff);

        const uint8_t y2  = (pF1 & 0x0ff00000) >> 20;
        const uint8_t cb2 = (pF1 & 0x0003fc00) >> 10;
        const uint8_t y1  = (pF1 & 0x000000ff);

        const uint8_t cb4 = (pF2 & 0x0ff00000) >> 20;
        const uint8_t y3  = (pF2 & 0x0003fc00) >> 10;
        const uint8_t cr2 = (pF2 & 0x000000ff);

        const uint8_t y5  = (pF3 & 0x0ff00000) >> 20;
        const uint8_t cr4 = (pF3 & 0x0003fc00) >> 10;
        const uint8_t y4  = (pF3 & 0x000000ff);

#if 0
        // extract full 10bit YUV data for debug.
        const uint16_t cr0_10 = (pF0 & 0x3ff00000) >> 20;
        const uint16_t y0_10 = (pF0 & 0x000ffc00) >> 10;
        const uint16_t cb0_10 = (pF0 & 0x000003ff);
        const uint16_t y2_10 = (pF1 & 0x3ff00000) >> 20;
        const uint16_t cb2_10 = (pF1 & 0x000ffc00) >> 10;
        const uint16_t y1_10 = (pF1 & 0x000003ff);

        const uint16_t cb4_10 = (pF2 & 0x3ff00000) >> 20;
        const uint16_t y3_10 = (pF2 & 0x000ffc00) >> 10;
        const uint16_t cr2_10 = (pF2 & 0x000003ff);

        const uint16_t y5_10 = (pF3 & 0x3ff00000) >> 20;
        const uint16_t cr4_10 = (pF3 & 0x000ffc00) >> 10;
        const uint16_t y4_10 = (pF3 & 0x000003ff);
#endif

        // Blackmagic Studio Camera Manual p.59
        // 12bit RAW sensor data
        const uint16_t p0_12 = (uint16_t)(cb0 | ((y0 & 0xf) << 8));
        const uint16_t p1_12 = (uint16_t)((cr0 << 4) | (y0 >> 4));
        const uint16_t p2_12 = (uint16_t)(y1 | ((cb2 & 0xf) << 8));
        const uint16_t p3_12 = (uint16_t)((cb2 >> 4) | (y2 << 4));

        const uint16_t p4_12 = (uint16_t)(cr2 | ((y3 & 0xf) << 8));
        const uint16_t p5_12 = (uint16_t)((cb4 << 4) | (y3 >> 4));
        const uint16_t p6_12 = (uint16_t)(y4 | ((cr4 & 0xf) << 8));
        const uint16_t p7_12 = (uint16_t)((cr4 >> 4) | (y5 << 4));

        // store gamma corrected 8bit value
        mBayer[posT + 0] = GammaTable(x + 0, y, p0_12);
        mBayer[posT + 1] = GammaTable(x + 1, y, p1_12);
        mBayer[posT + 2] = GammaTable(x + 2, y, p2_12);
        mBayer[posT + 3] = GammaTable(x + 3, y, p3_12);

        mBayer[posT + 4] = GammaTable(x + 4, y, p4_12);
        mBayer[posT + 5] = GammaTable(x + 5, y, p5_12);
        mBayer[posT + 6] = GammaTable(x + 6, y, p6_12);
        mBayer[posT + 7] = GammaTable(x + 7, y, p7_12);

        posT += 8;
        if (BAYER_WH <= posT) {
            break;
        }

        x += 8;
        if (BAYER_W <= x) {
            x = 0;
            ++y;
        }
    }

    const uint8_t a = alpha;
    for (y = 0; y < height; y += 2) {
        for (x = 0; x < width; x += 2) {
            /* G0 R
             * B  G1
             */
            const uint8_t g0 = mBayer[x + 0 + 16 + (y + 0 + 16)*BAYER_W];
            const uint8_t r = mBayer[x + 1 + 16 + (y + 0 + 16)*BAYER_W];
            const uint8_t b = mBayer[x + 0 + 16 + (y + 1 + 16)*BAYER_W];
            const uint8_t g1 = mBayer[x + 1 + 16 + (y + 1 + 16)*BAYER_W];

            pTo[x + 0 + (y + 0)*width] = (a << 24) + (b << 16) + (g0 << 8) + r;
            pTo[x + 1 + (y + 0)*width] = (a << 24) + (b << 16) + (g0 << 8) + r;
            pTo[x + 0 + (y + 1)*width] = (a << 24) + (b << 16) + (g1 << 8) + r;
            pTo[x + 1 + (y + 1)*width] = (a << 24) + (b << 16) + (g1 << 8) + r;
        }
    }
}

