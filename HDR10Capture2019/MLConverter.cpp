#include "MLConverter.h"
#include <ctgmath>
#include <assert.h>
#include <algorithm>
#include <Windows.h>

static uint32_t
NtoHL(uint32_t v)
{
    return   ((v & 0xff) << 24) |
        (((v >> 8) & 0xff) << 16) |
        (((v >> 16) & 0xff) << 8) |
        (((v >> 24) & 0xff));
}

static uint32_t
HtoNL(uint32_t v)
{   // NtoHLと同じ。
    return   ((v & 0xff) << 24) |
        (((v >> 8) & 0xff) << 16) |
        (((v >> 16) & 0xff) << 8) |
        (((v >> 24) & 0xff));
}

static uint16_t
HtoNS(uint16_t v)
{
    return ((v & 0xff) << 8) |
           ((v >> 8) & 0xff);
}

static const float pq_m1 = 0.1593017578125f; // ( 2610.0 / 4096.0 ) / 4.0;
static const float pq_m2 = 78.84375f; // ( 2523.0 / 4096.0 ) * 128.0;
static const float pq_c1 = 0.8359375f; // 3424.0 / 4096.0 or pq_c3 - pq_c2 + 1.0;
static const float pq_c2 = 18.8515625f; // ( 2413.0 / 4096.0 ) * 32.0;
static const float pq_c3 = 18.6875f; // ( 2392.0 / 4096.0 ) * 32.0;
static const float pq_C = 10000.0f;

static float
ST2084toExrLinear(float v) {
    float Np = pow(v, 1.0f / pq_m2);
    float L = Np - pq_c1;
    if (L < 0.0f) {
        L = 0.0f;
    }
    L = L / (pq_c2 - pq_c3 * Np);
    L = pow(L, 1.0f / pq_m1);
    return L * pq_C * 0.01f; // returns 100cd/m^2 == 1
}

MLConverter::MLConverter(void)
{
    // ガンマ変換テーブル作成。
    for (int i = 0; i < 1024; ++i) {
        float v = (float)i / 1023.0f;
        
        float g22 = v < 0.04045f ? v / 12.92f : pow(abs(v + 0.055f) / 1.055f, 2.4f);
        mGammaInv22_10bit[i] = g22;

        float st2084 = ST2084toExrLinear(v);
        mGammaInvST2084_10bit[i] = st2084;

        /*
        char s[256];
        sprintf_s(s, "%d, %f, %f\n", i, g22, st2084);
        OutputDebugStringA(s);
        */
    }
}

void
MLConverter::Argb8bitToR8G8B8A8(const uint32_t* pFrom, uint32_t* pTo, const int width, const int height)
{
    int pos = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // bmdFormat8BitARGB
            // ビッグエンディアンのA8R8G8B8 → リトルエンディアンのR8G8B8A8
            const uint32_t v = NtoHL(pFrom[pos]);

            // v                                 LSB
            // AAAAAAAA RRRRRRRR GGGGGGGG BBBBBBBB
            // 76543210 76543210 76543210 76543210

            const uint32_t a = (v >> 24) & 0xff;
            const uint32_t r = (v >> 16) & 0xff;
            const uint32_t g = (v >> 8) & 0xff;
            const uint32_t b = (v >> 0) & 0xff;
            pTo[pos] = (a << 24) + (b << 16) + (g << 8) + r;

            ++pos;
        }
    }
}

void
MLConverter::Rgb10bitToRGBA8bit(const uint32_t *pFrom, uint32_t *pTo, const int width, const int height, const uint8_t alpha)
{
    const uint32_t a = alpha;
    int pos = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // bmdFormat10BitRGB
            // ビッグエンディアンのX2R10G10B10 → リトルエンディアンのR8G8B8A8
            const uint32_t v = NtoHL(pFrom[pos]);

            // v                                 LSB
            // XXRRRRRR RRRRGGGG GGGGGGBB BBBBBBBB
            // --987654 32109876 54321098 76543210

            const uint32_t r = (v >> 22) & 0xff;
            const uint32_t g = (v >> 12) & 0xff;
            const uint32_t b = (v >> 2) & 0xff;
            pTo[pos] = (a << 24) + (b << 16) + (g << 8) + r;

            ++pos;
        }
    }
}

/// <summary>
/// bmdFormat10BitRGB → DXGI_FORMAT_R10G10B10A2_UNORM
/// </summary>
void
MLConverter::Rgb10bitToR10G10B10A2(const uint32_t* pFrom, uint32_t* pTo, const int width, const int height, const uint8_t alpha)
{
    const uint32_t a = (alpha >> 6) & 0x3;
    int pos = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // bmdFormat10BitRGB
            // ビッグエンディアンのX2R10G10B10 → リトルエンディアンのR8G8B8A8
            const uint32_t v = NtoHL(pFrom[pos]);

            // v                                 LSB
            // XXRRRRRR RRRRGGGG GGGGGGBB BBBBBBBB
            // --987654 32109876 54321098 76543210

            const uint32_t r = (v >> 20) & 0x3ff;
            const uint32_t g = (v >> 10) & 0x3ff;
            const uint32_t b = (v >> 0) & 0x3ff;
            pTo[pos] = (a << 30) + (b << 20) + (g << 10) + r;

            ++pos;
        }
    }
}

/// <summary>
/// R10G10B10A2_UNorm → half float for Exr write
/// </summary>
void
MLConverter::R10G10B10A2ToExrHalfFloat(const uint32_t* pFrom, uint16_t* pTo, const int width, const int height, const uint8_t alpha, GammaType gamma)
{
    // アルファチャンネルは別の計算式。
    // 0.0～1.0の範囲の値。
    half aF = (float)(alpha /255.0f);

    int readPos = 0;
    int writePos = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const uint32_t v = pFrom[readPos];
            // v                                 LSB
            // XXRRRRRR RRRRGGGG GGGGGGBB BBBBBBBB
            // --987654 32109876 54321098 76543210

            // 10bit RGB値。
            const uint32_t r = (v >> 20) & 0x3ff;
            const uint32_t g = (v >> 10) & 0x3ff;
            const uint32_t b = (v >> 0) & 0x3ff;

            switch (gamma){
            case GT_SDR_22:
                {
                    const half& rF = mGammaInv22_10bit[r];
                    const half& gF = mGammaInv22_10bit[g];
                    const half& bF = mGammaInv22_10bit[b];
                    // アルファチャンネルは別の計算式。
                    pTo[writePos + 0] = bF.bits();
                    pTo[writePos + 1] = gF.bits();
                    pTo[writePos + 2] = rF.bits();
                    pTo[writePos + 3] = aF.bits();
                }
                break;
            case GT_HDR_PQ:
                {
                    const half& rF = mGammaInvST2084_10bit[r];
                    const half& gF = mGammaInvST2084_10bit[g];
                    const half& bF = mGammaInvST2084_10bit[b];
                    // アルファチャンネルは別の計算式。
                    pTo[writePos + 0] = bF.bits();
                    pTo[writePos + 1] = gF.bits();
                    pTo[writePos + 2] = rF.bits();
                    pTo[writePos + 3] = aF.bits();
                }
                break;
            default:
                assert(0);
                break;
            } 

            ++readPos;
            writePos += 4;
        }
    }
}

/// <summary>
/// DXGI_FORMAT_R10G10B10A2_UNORM → bmdFormat10BitRGB(r210)
/// </summary>
void
MLConverter::R10G10B10A2ToR210(const uint32_t* pFrom, uint32_t* pTo, const int width, const int height, const uint8_t alpha)
{
    const uint32_t a = (alpha >> 6) & 0x3;
    int pos = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const uint32_t v = pFrom[pos];
            // v                                 LSB
            // XXRRRRRR RRRRGGGG GGGGGGBB BBBBBBBB
            // --987654 32109876 54321098 76543210

            const uint32_t r = (v >> 20) & 0x3ff;
            const uint32_t g = (v >> 10) & 0x3ff;
            const uint32_t b = (v >> 0) & 0x3ff;

            // bmdFormat10BitRGB
            // ビッグエンディアンのX2R10G10B10

            const uint32_t r210 = (a << 30) + (r << 20) + (g << 10) + b;
            pTo[pos] = HtoNL(r210);
            ++pos;
        }
    }
}

/// <summary>
/// bmdFormat12BitRGB → DXGI_FORMAT_R8G8B8A8_UNORM
/// </summary>
void
MLConverter::Rgb12bitToR8G8B8A8(const uint32_t* pFrom, uint32_t* pTo, const int width, const int height, const uint8_t alpha)
{
    const uint8_t a = alpha;
    int fromPos = 0;
    int toPos = 0;
    const int pixelCount = width * height;

    while (toPos < pixelCount) {
        // 8pixelsのRGB3チャンネルのデータが36バイト=9個のuint32に入っている。
        // 3ch * 8px * 12bit / 8bit = 36バイト。
        const uint32_t w0 = NtoHL(pFrom[fromPos + 0]);
        const uint32_t w1 = NtoHL(pFrom[fromPos + 1]);
        const uint32_t w2 = NtoHL(pFrom[fromPos + 2]);
        const uint32_t w3 = NtoHL(pFrom[fromPos + 3]);
        const uint32_t w4 = NtoHL(pFrom[fromPos + 4]);
        const uint32_t w5 = NtoHL(pFrom[fromPos + 5]);
        const uint32_t w6 = NtoHL(pFrom[fromPos + 6]);
        const uint32_t w7 = NtoHL(pFrom[fromPos + 7]);
        const uint32_t w8 = NtoHL(pFrom[fromPos + 8]);

        // w0                                LSB
        // BBBBBBBB GGGGGGGG GGGGRRRR RRRRRRRR
        // 00000000 00000000 00000000 00000000 pixelIdx
        // 00000000 11000000 00001100 00000000
        // 76543210 10987654 32101098 76543210

        // w1                                LSB
        // BBBBGGGG GGGGGGGG RRRRRRRR RRRRBBBB
        // 11111111 11111111 11111111 11110000 pixelIdx
        // 00001100 00000000 11000000 00001100
        // 32101098 76543210 10987654 32101098

        // w2                                LSB
        // GGGGGGGG GGGGRRRR RRRRRRRR BBBBBBBB
        // 22222222 22222222 22222222 11111111 pixelIdx
        // 11000000 00001100 00000000 11000000
        // 10987654 32101098 76543210 10987654

        // w3                                LSB
        // GGGGGGGG RRRRRRRR RRRRBBBB BBBBBBBB
        // 33333333 33333333 33332222 22222222 pixelIdx
        // 00000000 11000000 00001100 00000000
        // 76543210 10987654 32101098 76543210

        // w4                                LSB
        // GGGGRRRR RRRRRRRR BBBBBBBB BBBBGGGG
        // 44444444 44444444 33333333 33333333 pixelIdx
        // 00001100 00000000 11000000 00001100
        // 32101098 76543210 10987654 32101098

        // w5                                LSB
        // RRRRRRRR RRRRBBBB BBBBBBBB GGGGGGGG
        // 55555555 55554444 44444444 44444444 pixelIdx
        // 11000000 00001100 00000000 11000000
        // 10987654 32101098 76543210 10987654

        // w6                                LSB
        // RRRRRRRR BBBBBBBB BBBBGGGG GGGGGGGG
        // 66666666 55555555 55555555 55555555 pixelIdx
        // 00000000 11000000 00001100 00000000
        // 76543210 10987654 32101098 76543210

        // w7                                LSB
        // RRRRBBBB BBBBBBBB GGGGGGGG GGGGRRRR
        // 77776666 66666666 66666666 66666666 pixelIdx
        // 00001100 00000000 11000000 00001100
        // 32101098 76543210 10987654 32101098

        // w8                                LSB
        // BBBBBBBB BBBBGGGG GGGGGGGG RRRRRRRR
        // 77777777 77777777 77777777 77777777 pixelIdx
        // 11000000 00001100 00000000 11000000
        // 10987654 32101098 76543210 10987654

        const uint8_t r0 = (w0 >> 4)  & 0xff;
        const uint8_t g0 = (w0 >> 16) & 0xff;
        const uint8_t b0 = ((w1 << 4) & 0xf0) + ((w0 >> 28) & 0x0f);
        const uint8_t r1 = (w1 >> 8)  & 0xff;
        const uint8_t g1 = (w1 >> 20) & 0xff;
        const uint8_t b1 = (w2 >> 0)  & 0xff;
        const uint8_t r2 = (w2 >> 12) & 0xff;
        const uint8_t g2 = (w2 >> 24) & 0xff;

        const uint8_t b2 = (w3 >> 4)  & 0xff;
        const uint8_t r3 = (w3 >> 16) & 0xff;
        const uint8_t g3 = ((w4 << 4) & 0xf0) + ((w3 >> 28) & 0x0f);
        const uint8_t b3 = (w4 >> 8)  & 0xff;
        const uint8_t r4 = (w4 >> 20) & 0xff;
        const uint8_t g4 = (w5 >> 0)  & 0xff;
        const uint8_t b4 = (w5 >> 12) & 0xff;
        const uint8_t r5 = (w5 >> 24) & 0xff;

        const uint8_t g5 = (w6 >> 4)  & 0xff;
        const uint8_t b5 = (w6 >> 16) & 0xff;
        const uint8_t r6 = ((w7 << 4) & 0xf0) + ((w6 >> 28) & 0x0f);
        const uint8_t g6 = (w7 >> 8)  & 0xff;
        const uint8_t b6 = (w7 >> 20) & 0xff;
        const uint8_t r7 = (w8 >> 4)  & 0xff;
        const uint8_t g7 = (w8 >> 12) & 0xff;
        const uint8_t b7 = (w8 >> 24) & 0xff;

        pTo[toPos + 0] = (a << 24) + (b0 << 16) + (g0 << 8) + r0;
        pTo[toPos + 1] = (a << 24) + (b1 << 16) + (g1 << 8) + r1;
        pTo[toPos + 2] = (a << 24) + (b2 << 16) + (g2 << 8) + r2;
        pTo[toPos + 3] = (a << 24) + (b3 << 16) + (g3 << 8) + r3;
        pTo[toPos + 4] = (a << 24) + (b4 << 16) + (g4 << 8) + r4;
        pTo[toPos + 5] = (a << 24) + (b5 << 16) + (g5 << 8) + r5;
        pTo[toPos + 6] = (a << 24) + (b6 << 16) + (g6 << 8) + r6;
        pTo[toPos + 7] = (a << 24) + (b7 << 16) + (g7 << 8) + r7;

        fromPos   += 9;
        toPos     += 8;
    }
}

/// <summary>
/// bmdFormat12BitRGB → DXGI_FORMAT_R10G10B10A2_UNORM
/// </summary>
void
MLConverter::Rgb12bitToR10G10B10A2(const uint32_t* pFrom, uint32_t* pTo, const int width, const int height, const uint8_t alpha)
{
    const uint8_t a = (alpha>>6) & 0x3;
    int fromPos = 0;
    int toPos = 0;
    const int pixelCount = width * height;

    while (toPos < pixelCount) {
        // 8pixelsのRGB3チャンネルのデータが36バイト=9個のuint32に入っている。
        // 3ch * 8px * 12bit / 8bit = 36バイト。
        const uint32_t w0 = NtoHL(pFrom[fromPos + 0]);
        const uint32_t w1 = NtoHL(pFrom[fromPos + 1]);
        const uint32_t w2 = NtoHL(pFrom[fromPos + 2]);
        const uint32_t w3 = NtoHL(pFrom[fromPos + 3]);
        const uint32_t w4 = NtoHL(pFrom[fromPos + 4]);
        const uint32_t w5 = NtoHL(pFrom[fromPos + 5]);
        const uint32_t w6 = NtoHL(pFrom[fromPos + 6]);
        const uint32_t w7 = NtoHL(pFrom[fromPos + 7]);
        const uint32_t w8 = NtoHL(pFrom[fromPos + 8]);

        // w0                                LSB
        // BBBBBBBB GGGGGGGG GGGGRRRR RRRRRRRR
        // 00000000 00000000 00000000 00000000 pixelIdx
        // 00000000 11000000 00001100 00000000
        // 76543210 10987654 32101098 76543210

        // w1                                LSB
        // BBBBGGGG GGGGGGGG RRRRRRRR RRRRBBBB
        // 11111111 11111111 11111111 11110000 pixelIdx
        // 00001100 00000000 11000000 00001100
        // 32101098 76543210 10987654 32101098

        // w2                                LSB
        // GGGGGGGG GGGGRRRR RRRRRRRR BBBBBBBB
        // 22222222 22222222 22222222 11111111 pixelIdx
        // 11000000 00001100 00000000 11000000
        // 10987654 32101098 76543210 10987654

        // w3                                LSB
        // GGGGGGGG RRRRRRRR RRRRBBBB BBBBBBBB
        // 33333333 33333333 33332222 22222222 pixelIdx
        // 00000000 11000000 00001100 00000000
        // 76543210 10987654 32101098 76543210

        // w4                                LSB
        // GGGGRRRR RRRRRRRR BBBBBBBB BBBBGGGG
        // 44444444 44444444 33333333 33333333 pixelIdx
        // 00001100 00000000 11000000 00001100
        // 32101098 76543210 10987654 32101098

        // w5                                LSB
        // RRRRRRRR RRRRBBBB BBBBBBBB GGGGGGGG
        // 55555555 55554444 44444444 44444444 pixelIdx
        // 11000000 00001100 00000000 11000000
        // 10987654 32101098 76543210 10987654

        // w6                                LSB
        // RRRRRRRR BBBBBBBB BBBBGGGG GGGGGGGG
        // 66666666 55555555 55555555 55555555 pixelIdx
        // 00000000 11000000 00001100 00000000
        // 76543210 10987654 32101098 76543210

        // w7                                LSB
        // RRRRBBBB BBBBBBBB GGGGGGGG GGGGRRRR
        // 77776666 66666666 66666666 66666666 pixelIdx
        // 00001100 00000000 11000000 00001100
        // 32101098 76543210 10987654 32101098

        // w8                                LSB
        // BBBBBBBB BBBBGGGG GGGGGGGG RRRRRRRR
        // 77777777 77777777 77777777 77777777 pixelIdx
        // 11000000 00001100 00000000 11000000
        // 10987654 32101098 76543210 10987654

        // 12ビットのデータの上位10ビットを取り出す。
        const uint16_t r0 = (w0 >> 2) & 0x3ff;
        const uint16_t g0 = (w0 >> 14) & 0x3ff;
        const uint16_t b0 = ((w1 << 6) & 0x3c0) + ((w0 >> 26) & 0x3f);
        const uint16_t r1 = (w1 >> 6) & 0x3ff;
        const uint16_t g1 = (w1 >> 18) & 0x3ff;
        const uint16_t b1 = ((w2 << 2) & 0x3fc) + ((w1 >> 30) & 0x3);
        const uint16_t r2 = (w2 >> 10) & 0x3ff;
        const uint16_t g2 = (w2 >> 22) & 0x3ff;

        const uint16_t b2 = (w3 >> 2) & 0x3ff;
        const uint16_t r3 = (w3 >> 14) & 0x3ff;
        const uint16_t g3 = ((w4 << 6) & 0x3c0) + ((w3 >> 26) & 0x3f);
        const uint16_t b3 = (w4 >> 6) & 0x3ff;
        const uint16_t r4 = (w4 >> 18) & 0x3ff;
        const uint16_t g4 = ((w5 << 2) & 0x3fc) + ((w4 >> 30) & 0x3);
        const uint16_t b4 = (w5 >> 10) & 0x3ff;
        const uint16_t r5 = (w5 >> 22) & 0x3ff;

        const uint16_t g5 = (w6 >> 2) & 0x3ff;
        const uint16_t b5 = (w6 >> 14) & 0x3ff;
        const uint16_t r6 = ((w7 << 6) & 0x3c0) + ((w6 >> 26) & 0x3f);
        const uint16_t g6 = (w7 >> 6) & 0x3ff;
        const uint16_t b6 = (w7 >> 18) & 0x3ff;
        const uint16_t r7 = ((w8 << 2) & 0x3fc) + ((w7 >> 30) & 0x3);
        const uint16_t g7 = (w8 >> 10) & 0x3ff;
        const uint16_t b7 = (w8 >> 22) & 0x3ff;

        pTo[toPos + 0] = (a << 30) + (b0 << 20) + (g0 << 10) + r0;
        pTo[toPos + 1] = (a << 30) + (b1 << 20) + (g1 << 10) + r1;
        pTo[toPos + 2] = (a << 30) + (b2 << 20) + (g2 << 10) + r2;
        pTo[toPos + 3] = (a << 30) + (b3 << 20) + (g3 << 10) + r3;
        pTo[toPos + 4] = (a << 30) + (b4 << 20) + (g4 << 10) + r4;
        pTo[toPos + 5] = (a << 30) + (b5 << 20) + (g5 << 10) + r5;
        pTo[toPos + 6] = (a << 30) + (b6 << 20) + (g6 << 10) + r6;
        pTo[toPos + 7] = (a << 30) + (b7 << 20) + (g7 << 10) + r7;

        fromPos += 9;
        toPos += 8;
    }
}

/// <summary>
/// bmdFormat12BitRGB → RGB10bit r210
/// </summary>
void
MLConverter::Rgb12bitToR210(const uint32_t* pFrom, uint32_t* pTo, const int width, const int height)
{
    const uint8_t a = 0x3;
    int fromPos = 0;
    int toPos = 0;
    const int pixelCount = width * height;

    while (toPos < pixelCount) {
        // 8pixelsのRGB3チャンネルのデータが36バイト=9個のuint32に入っている。
        // 3ch * 8px * 12bit / 8bit = 36バイト。
        const uint32_t w0 = NtoHL(pFrom[fromPos + 0]);
        const uint32_t w1 = NtoHL(pFrom[fromPos + 1]);
        const uint32_t w2 = NtoHL(pFrom[fromPos + 2]);
        const uint32_t w3 = NtoHL(pFrom[fromPos + 3]);
        const uint32_t w4 = NtoHL(pFrom[fromPos + 4]);
        const uint32_t w5 = NtoHL(pFrom[fromPos + 5]);
        const uint32_t w6 = NtoHL(pFrom[fromPos + 6]);
        const uint32_t w7 = NtoHL(pFrom[fromPos + 7]);
        const uint32_t w8 = NtoHL(pFrom[fromPos + 8]);

        // w0                                LSB
        // BBBBBBBB GGGGGGGG GGGGRRRR RRRRRRRR
        // 00000000 00000000 00000000 00000000 pixelIdx
        // 00000000 11000000 00001100 00000000
        // 76543210 10987654 32101098 76543210

        // w1                                LSB
        // BBBBGGGG GGGGGGGG RRRRRRRR RRRRBBBB
        // 11111111 11111111 11111111 11110000 pixelIdx
        // 00001100 00000000 11000000 00001100
        // 32101098 76543210 10987654 32101098

        // w2                                LSB
        // GGGGGGGG GGGGRRRR RRRRRRRR BBBBBBBB
        // 22222222 22222222 22222222 11111111 pixelIdx
        // 11000000 00001100 00000000 11000000
        // 10987654 32101098 76543210 10987654

        // w3                                LSB
        // GGGGGGGG RRRRRRRR RRRRBBBB BBBBBBBB
        // 33333333 33333333 33332222 22222222 pixelIdx
        // 00000000 11000000 00001100 00000000
        // 76543210 10987654 32101098 76543210

        // w4                                LSB
        // GGGGRRRR RRRRRRRR BBBBBBBB BBBBGGGG
        // 44444444 44444444 33333333 33333333 pixelIdx
        // 00001100 00000000 11000000 00001100
        // 32101098 76543210 10987654 32101098

        // w5                                LSB
        // RRRRRRRR RRRRBBBB BBBBBBBB GGGGGGGG
        // 55555555 55554444 44444444 44444444 pixelIdx
        // 11000000 00001100 00000000 11000000
        // 10987654 32101098 76543210 10987654

        // w6                                LSB
        // RRRRRRRR BBBBBBBB BBBBGGGG GGGGGGGG
        // 66666666 55555555 55555555 55555555 pixelIdx
        // 00000000 11000000 00001100 00000000
        // 76543210 10987654 32101098 76543210

        // w7                                LSB
        // RRRRBBBB BBBBBBBB GGGGGGGG GGGGRRRR
        // 77776666 66666666 66666666 66666666 pixelIdx
        // 00001100 00000000 11000000 00001100
        // 32101098 76543210 10987654 32101098

        // w8                                LSB
        // BBBBBBBB BBBBGGGG GGGGGGGG RRRRRRRR
        // 77777777 77777777 77777777 77777777 pixelIdx
        // 11000000 00001100 00000000 11000000
        // 10987654 32101098 76543210 10987654

        // 12ビットのデータの上位10ビットを取り出す。
        const uint16_t r0 = (w0 >> 2) & 0x3ff;
        const uint16_t g0 = (w0 >> 14) & 0x3ff;
        const uint16_t b0 = ((w1 << 6) & 0x3c0) + ((w0 >> 26) & 0x3f);
        const uint16_t r1 = (w1 >> 6) & 0x3ff;
        const uint16_t g1 = (w1 >> 18) & 0x3ff;
        const uint16_t b1 = ((w2 << 2) & 0x3fc) + ((w1 >> 30) & 0x3);
        const uint16_t r2 = (w2 >> 10) & 0x3ff;
        const uint16_t g2 = (w2 >> 22) & 0x3ff;

        const uint16_t b2 = (w3 >> 2) & 0x3ff;
        const uint16_t r3 = (w3 >> 14) & 0x3ff;
        const uint16_t g3 = ((w4 << 6) & 0x3c0) + ((w3 >> 26) & 0x3f);
        const uint16_t b3 = (w4 >> 6) & 0x3ff;
        const uint16_t r4 = (w4 >> 18) & 0x3ff;
        const uint16_t g4 = ((w5 << 2) & 0x3fc) + ((w4 >> 30) & 0x3);
        const uint16_t b4 = (w5 >> 10) & 0x3ff;
        const uint16_t r5 = (w5 >> 22) & 0x3ff;

        const uint16_t g5 = (w6 >> 2) & 0x3ff;
        const uint16_t b5 = (w6 >> 14) & 0x3ff;
        const uint16_t r6 = ((w7 << 6) & 0x3c0) + ((w6 >> 26) & 0x3f);
        const uint16_t g6 = (w7 >> 6) & 0x3ff;
        const uint16_t b6 = (w7 >> 18) & 0x3ff;
        const uint16_t r7 = ((w8 << 2) & 0x3fc) + ((w7 >> 30) & 0x3);
        const uint16_t g7 = (w8 >> 10) & 0x3ff;
        const uint16_t b7 = (w8 >> 22) & 0x3ff;

        // bmdFormat10BitRGB r210
        // ビッグエンディアンのX2R10G10B10

        const uint32_t r210_0 = (a << 30) + (r0 << 20) + (g0 << 10) + b0;
        const uint32_t r210_1 = (a << 30) + (r1 << 20) + (g1 << 10) + b1;
        const uint32_t r210_2 = (a << 30) + (r2 << 20) + (g2 << 10) + b2;
        const uint32_t r210_3 = (a << 30) + (r3 << 20) + (g3 << 10) + b3;

        const uint32_t r210_4 = (a << 30) + (r4 << 20) + (g4 << 10) + b4;
        const uint32_t r210_5 = (a << 30) + (r5 << 20) + (g5 << 10) + b5;
        const uint32_t r210_6 = (a << 30) + (r6 << 20) + (g6 << 10) + b6;
        const uint32_t r210_7 = (a << 30) + (r7 << 20) + (g7 << 10) + b7;

        pTo[toPos + 0] = HtoNL(r210_0);
        pTo[toPos + 1] = HtoNL(r210_1);
        pTo[toPos + 2] = HtoNL(r210_2);
        pTo[toPos + 3] = HtoNL(r210_3);
        pTo[toPos + 4] = HtoNL(r210_4);
        pTo[toPos + 5] = HtoNL(r210_5);
        pTo[toPos + 6] = HtoNL(r210_6);
        pTo[toPos + 7] = HtoNL(r210_7);

        fromPos += 9;
        toPos += 8;
    }
}


/// <summary>
/// bmdFormat12BitRGB → DXGI_FORMAT_R16G16B16A16_UNORM
/// </summary>
void
MLConverter::Rgb12bitToR16G16B16A16(const uint32_t* pFrom, uint64_t* pTo, const int width, const int height, const uint8_t alpha)
{
    const uint16_t a = alpha * 257; //< 255 * 257 = 65535
    int fromPos = 0;
    int toPos = 0;
    const int pixelCount = width * height;

    while (toPos < pixelCount) {
        // 8pixelsのRGB3チャンネルのデータが36バイト=9個のuint32に入っている。
        // 3ch * 8px * 12bit / 8bit = 36バイト。
        const uint32_t w0 = NtoHL(pFrom[fromPos + 0]);
        const uint32_t w1 = NtoHL(pFrom[fromPos + 1]);
        const uint32_t w2 = NtoHL(pFrom[fromPos + 2]);
        const uint32_t w3 = NtoHL(pFrom[fromPos + 3]);
        const uint32_t w4 = NtoHL(pFrom[fromPos + 4]);
        const uint32_t w5 = NtoHL(pFrom[fromPos + 5]);
        const uint32_t w6 = NtoHL(pFrom[fromPos + 6]);
        const uint32_t w7 = NtoHL(pFrom[fromPos + 7]);
        const uint32_t w8 = NtoHL(pFrom[fromPos + 8]);

        // w0                                LSB
        // BBBBBBBB GGGGGGGG GGGGRRRR RRRRRRRR
        // 00000000 00000000 00000000 00000000 pixelIdx
        // 00000000 11000000 00001100 00000000
        // 76543210 10987654 32101098 76543210

        // w1                                LSB
        // BBBBGGGG GGGGGGGG RRRRRRRR RRRRBBBB
        // 11111111 11111111 11111111 11110000 pixelIdx
        // 00001100 00000000 11000000 00001100
        // 32101098 76543210 10987654 32101098

        // w2                                LSB
        // GGGGGGGG GGGGRRRR RRRRRRRR BBBBBBBB
        // 22222222 22222222 22222222 11111111 pixelIdx
        // 11000000 00001100 00000000 11000000
        // 10987654 32101098 76543210 10987654

        // w3                                LSB
        // GGGGGGGG RRRRRRRR RRRRBBBB BBBBBBBB
        // 33333333 33333333 33332222 22222222 pixelIdx
        // 00000000 11000000 00001100 00000000
        // 76543210 10987654 32101098 76543210

        // w4                                LSB
        // GGGGRRRR RRRRRRRR BBBBBBBB BBBBGGGG
        // 44444444 44444444 33333333 33333333 pixelIdx
        // 00001100 00000000 11000000 00001100
        // 32101098 76543210 10987654 32101098

        // w5                                LSB
        // RRRRRRRR RRRRBBBB BBBBBBBB GGGGGGGG
        // 55555555 55554444 44444444 44444444 pixelIdx
        // 11000000 00001100 00000000 11000000
        // 10987654 32101098 76543210 10987654

        // w6                                LSB
        // RRRRRRRR BBBBBBBB BBBBGGGG GGGGGGGG
        // 66666666 55555555 55555555 55555555 pixelIdx
        // 00000000 11000000 00001100 00000000
        // 76543210 10987654 32101098 76543210

        // w7                                LSB
        // RRRRBBBB BBBBBBBB GGGGGGGG GGGGRRRR
        // 77776666 66666666 66666666 66666666 pixelIdx
        // 00001100 00000000 11000000 00001100
        // 32101098 76543210 10987654 32101098

        // w8                                LSB
        // BBBBBBBB BBBBGGGG GGGGGGGG RRRRRRRR
        // 77777777 77777777 77777777 77777777 pixelIdx
        // 11000000 00001100 00000000 11000000
        // 10987654 32101098 76543210 10987654

        const uint16_t r0 = (w0 << 4)   & 0xfff0;
        const uint16_t g0 = (w0 >> 8)   & 0xfff0;
        const uint16_t b0 = ((w1 << 12) & 0xf000) + ((w0 >> 20) & 0x0ff0);
        const uint16_t r1 = (w1 >> 0)   & 0xfff0;
        const uint16_t g1 = (w1 >> 12)  & 0xfff0;
        const uint16_t b1 = ((w2 << 8)  & 0xff00) + ((w1 >> 24) & 0x00f0);
        const uint16_t r2 = (w2 >> 4)   & 0xfff0;
        const uint16_t g2 = (w2 >> 16)  & 0xfff0;

        const uint16_t b2 = (w3 << 4)   & 0xfff0;
        const uint16_t r3 = (w3 >> 8)   & 0xfff0;
        const uint16_t g3 = ((w4 << 12) & 0xf000) + ((w3 >> 20) & 0x0ff0);
        const uint16_t b3 = (w4 >> 0)   & 0xfff0;
        const uint16_t r4 = (w4 >> 12)  & 0xfff0;
        const uint16_t g4 = ((w5 << 8)  & 0xff00) + ((w4 >> 24) & 0x00f0);
        const uint16_t b4 = (w5 >> 4)   & 0xfff0;
        const uint16_t r5 = (w5 >> 16)  & 0xfff0;

        const uint16_t g5 = (w6 << 4)   & 0xfff0;
        const uint16_t b5 = (w6 >> 8)   & 0xfff0;
        const uint16_t r6 = ((w7 << 12) & 0xf000) + ((w6 >> 20) & 0x0ff0);
        const uint16_t g6 = (w7 >> 0)   & 0xfff0;
        const uint16_t b6 = (w7 >> 12)  & 0xfff0;
        const uint16_t r7 = ((w8 << 8)  & 0xff00) + ((w7 >> 24) & 0x00f0);
        const uint16_t g7 = (w8 >> 4)   & 0xfff0;
        const uint16_t b7 = (w8 >> 16)  & 0xfff0;

        pTo[toPos + 0] = ((uint64_t)a << 48) + ((uint64_t)b0 << 32) + ((uint32_t)g0 << 16) + r0;
        pTo[toPos + 1] = ((uint64_t)a << 48) + ((uint64_t)b1 << 32) + ((uint32_t)g1 << 16) + r1;
        pTo[toPos + 2] = ((uint64_t)a << 48) + ((uint64_t)b2 << 32) + ((uint32_t)g2 << 16) + r2;
        pTo[toPos + 3] = ((uint64_t)a << 48) + ((uint64_t)b3 << 32) + ((uint32_t)g3 << 16) + r3;
        pTo[toPos + 4] = ((uint64_t)a << 48) + ((uint64_t)b4 << 32) + ((uint32_t)g4 << 16) + r4;
        pTo[toPos + 5] = ((uint64_t)a << 48) + ((uint64_t)b5 << 32) + ((uint32_t)g5 << 16) + r5;
        pTo[toPos + 6] = ((uint64_t)a << 48) + ((uint64_t)b6 << 32) + ((uint32_t)g6 << 16) + r6;
        pTo[toPos + 7] = ((uint64_t)a << 48) + ((uint64_t)b7 << 32) + ((uint32_t)g7 << 16) + r7;

        fromPos += 9;
        toPos += 8;
    }

}
