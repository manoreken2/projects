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

static float
Saturate01(const float v)
{
    if (v < 0) {
        return 0;
    } else if (1.0f < v) {
        return 1.0f;
    } else {
        return v;
    }
}

/// <summary>
/// 0～1の範囲のfloat値のYUVを0～1の範囲のfloat値のRGBにする。
/// </summary>
static void
YuvToRgb(
    const float y, const float u, const float v,
    float* r_return, float *g_return, float *b_return)
{
    *r_return = Saturate01(1.1644f * y + 0.0000f * u + 1.5960f * v - 0.8742f);
    *g_return = Saturate01(1.1644f * y - 0.3918f * u - 0.8130f * v + 0.5317f);
    *b_return = Saturate01(1.1644f * y + 2.0172f * u + 0.0000f * v - 1.0856f);
}

/// <summary>
/// 8bit YUV → 8bit RGB
/// </summary>
static void
Yuv8ToRgb8(
    const uint8_t y8, const uint8_t u8, const uint8_t v8,
    uint8_t* r8_return, uint8_t* g8_return, uint8_t* b8_return)
{
    const float y = y8 / 255.0f;
    const float u = u8 / 255.0f;
    const float v = v8 / 255.0f;

    const float r = Saturate01(1.1644f * y + 0.0000f * u + 1.5960f * v - 0.8742f);
    const float g = Saturate01(1.1644f * y - 0.3918f * u - 0.8130f * v + 0.5317f);
    const float b = Saturate01(1.1644f * y + 2.0172f * u + 0.0000f * v - 1.0856f);

    *r8_return = (uint8_t)(r * 255.0f);
    *g8_return = (uint8_t)(g * 255.0f);
    *b8_return = (uint8_t)(b * 255.0f);
}

/// <summary>
/// 10bit YUV → 10bit RGB
/// </summary>
static void
Yuv10ToRgb10(
    const uint16_t y10, const uint16_t u10, const uint16_t v10,
    uint16_t* r10_return, uint16_t* g10_return, uint16_t* b10_return)
{
    const float y = y10 / 1023.0f;
    const float u = u10 / 1023.0f;
    const float v = v10 / 1023.0f;

    const float r = Saturate01(1.1644f * y + 0.0000f * u + 1.5960f * v - 0.8742f);
    const float g = Saturate01(1.1644f * y - 0.3918f * u - 0.8130f * v + 0.5317f);
    const float b = Saturate01(1.1644f * y + 2.0172f * u + 0.0000f * v - 1.0856f);

    *r10_return = (uint16_t)(r * 1023.0f);
    *g10_return = (uint16_t)(g * 1023.0f);
    *b10_return = (uint16_t)(b * 1023.0f);
}

/// <summary>
/// 0～1の範囲のfloat値のRGBを0～1の範囲のfloat値のYUVにする。
/// </summary>
static void
RgbToYuv(
    const float r, const float g, const float b,
    float* y_return, float* u_return, float* v_return)
{
    *y_return = Saturate01( 0.2568f * r + 0.5041f * g + 0.0979f * b + 0.0627f);
    *u_return = Saturate01(-0.1482f * r - 0.2910f * g + 0.4392f * b + 0.5020f);
    *v_return = Saturate01( 0.4392f * r - 0.3678f * g - 0.0714f * b + 0.5020f);
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
MLConverter::Uyvy8bitToR8G8B8A8(const uint32_t* pFrom, uint32_t* pTo, const int width, const int height)
{
    // widthは2で割り切れる。
    assert((width & 1) == 0);

    const uint32_t a = 0xff;

#pragma omp parallel for
    for (int y = 0; y < height; ++y) {
        for (int x2 = 0; x2 < width/2; ++x2) {
            //2ピクセルずつ処理。

            //入力のUYVY: 2ピクセルが1個のuint32に入る。
            const int readP = x2 + y * (width/2);
            //出力: 1ピクセルが1個のuint32に入る。
            const int writeP = (x2 * 2) + y * width;

            // bmdFormat8BitYUV: UYVY
            // ビッグエンディアンのUYVU
            // w                                 LSB
            // YYYYYYYY VVVVVVVV YYYYYYYY UUUUUUUU
            // 76543210 76543210 76543210 76543210
            const uint32_t w = pFrom[readP];

            const uint32_t y0 = (w >> 8) & 0xff;
            const uint32_t y1 = (w >> 24) & 0xff;

            const uint32_t u = (w >> 0) & 0xff;
            const uint32_t v = (w >> 16) & 0xff;

            // yuv → RGB
            uint8_t r, g, b;

            Yuv8ToRgb8(y0, u, v, &r, &g, &b);
            pTo[writeP + 0] = (a << 24) + (b << 16) + (g << 8) + r;

            Yuv8ToRgb8(y1, u, v, &r, &g, &b);
            pTo[writeP + 1] = (a << 24) + (b << 16) + (g << 8) + r;
        }
    }
}
/// <summary>
/// bmdFormat10BitYUV v210 → DXGI_FORMAT_R10G10B10A2_UNORM
/// </summary>
void
MLConverter::Yuv422_10bitToR10G10B10A2(const uint32_t* pFrom, uint32_t* pTo, const int width, const int height, const uint8_t alpha)
{
    const uint32_t a = (alpha >> 6) & 0x3;

    assert((width % 48) == 0);

#pragma omp parallel for
    for (int y = 0; y < height; ++y) {
        for (int x6 = 0; x6 < width/6; ++x6) {
            // 6ピクセルずつ処理。

            //入力のv210: 6ピクセルが4個のuint32に入る。
            const int readP = x6*4 + y * (width *4 / 6);

            // 出力：1ピクセルが1個のuint32に入る。
            const int writeP = (x6 * 6) + y * width;

            // bmdFormat10BitYUV v210
            const uint32_t w0 = pFrom[readP + 0];
            const uint32_t w1 = pFrom[readP + 1];
            const uint32_t w2 = pFrom[readP + 2];
            const uint32_t w3 = pFrom[readP + 3];

            // w0                                LSB
            // --vvvvvv vvvvyyyy yyyyyyuu uuuuuuuu
            // --000000 00000000 00000000 00000000 pixelIdx
            // --987654 32109876 54321098 76543210

            // w1                                LSB
            // --yyyyyy yyyyuuuu uuuuuuyy yyyyyyyy
            // --222222 22222222 22222211 11111111 pixelIdx
            // --987654 32109876 54321098 76543210

            // w0                                LSB
            // --uuuuuu uuuuyyyy yyyyyyvv vvvvvvvv
            // --444444 44443333 33333322 22222222 pixelIdx
            // --987654 32109876 54321098 76543210
            // w0                                LSB
            // --yyyyyy yyyyvvvv vvvvvvyy yyyyyyyy
            // --555555 55554444 44444444 44444444 pixelIdx
            // --987654 32109876 54321098 76543210

            const uint32_t u0 = (w0 >> 0) & 0x3ff;
            const uint32_t y0 = (w0 >> 10) & 0x3ff;
            const uint32_t v0 = (w0 >> 20) & 0x3ff;

            const uint32_t y1 = (w1 >> 0) & 0x3ff;
            const uint32_t u2 = (w1 >> 10) & 0x3ff;
            const uint32_t y2 = (w1 >> 20) & 0x3ff;

            const uint32_t v2 = (w2 >> 0) & 0x3ff;
            const uint32_t y3 = (w2 >> 10) & 0x3ff;
            const uint32_t u4 = (w2 >> 20) & 0x3ff;

            const uint32_t y4 = (w3 >> 0) & 0x3ff;
            const uint32_t v4 = (w3 >> 10) & 0x3ff;
            const uint32_t y5 = (w3 >> 20) & 0x3ff;

            // yuv → RGB
            uint16_t r, g, b;

            Yuv10ToRgb10(y0, u0, v0, &r, &g, &b);
            pTo[writeP + 0] = (a << 30) + (b << 20) + (g << 10) + r;

            Yuv10ToRgb10(y1, u0, v0, &r, &g, &b);
            pTo[writeP + 1] = (a << 30) + (b << 20) + (g << 10) + r;

            Yuv10ToRgb10(y2, u2, v2, &r, &g, &b);
            pTo[writeP + 2] = (a << 30) + (b << 20) + (g << 10) + r;

            Yuv10ToRgb10(y3, u2, v2, &r, &g, &b);
            pTo[writeP + 3] = (a << 30) + (b << 20) + (g << 10) + r;

            Yuv10ToRgb10(y4, u4, v4, &r, &g, &b);
            pTo[writeP + 4] = (a << 30) + (b << 20) + (g << 10) + r;

            Yuv10ToRgb10(y5, u4, v4, &r, &g, &b);
            pTo[writeP + 5] = (a << 30) + (b << 20) + (g << 10) + r;
        }
    }
}

void
MLConverter::Argb8bitToR8G8B8A8(const uint32_t* pFrom, uint32_t* pTo, const int width, const int height)
{
#pragma omp parallel for
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int pos = x + y * width;

            // bmdFormat8BitARGB
            // ビッグエンディアンのA8R8G8B8 → リトルエンディアンのR8G8B8A8
            const uint32_t w = pFrom[pos];

            // w                                 LSB
            // BBBBBBBB GGGGGGGG RRRRRRRR AAAAAAAA
            // 76543210 76543210 76543210 76543210

            const uint8_t a = (w >> 0) & 0xff;
            const uint8_t r = (w >> 8) & 0xff;
            const uint8_t g = (w >> 16) & 0xff;
            const uint8_t b = (w >> 24) & 0xff;
            pTo[pos] = (a << 24) + (b << 16) + (g << 8) + r;
        }
    }
}

void
MLConverter::Rgb10bitToRGBA8bit(const uint32_t *pFrom, uint32_t *pTo, const int width, const int height, const uint8_t alpha)
{
    const uint32_t a = alpha;

#pragma omp parallel for
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int pos = x + y * width;

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

#pragma omp parallel for
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int pos = x + y * width;

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

#pragma omp parallel for
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int readPos = x + y * width;
            const int writePos = readPos * 4;

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

#pragma omp parallel for
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int pos = x + y * width;

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
        }
    }
}

/// <summary>
/// bmdFormat12BitRGB → DXGI_FORMAT_R8G8B8A8_UNORM
/// </summary>
void
MLConverter::Rgb12bitToR8G8B8A8(const uint32_t* pFrom, uint32_t* pTo, const int width, const int height, const uint8_t alpha)
{
    /// widthは8で割り切れます。
    assert((width & 7) == 0);

    const uint8_t a = alpha;
    const int pixelCount = width * height;

#pragma omp parallel for
    for (int i=0; i<pixelCount/8; ++i) {
        const int fromPos = i * 9;
        const int toPos = i * 8;

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
    }
}

/// <summary>
/// bmdFormat12BitRGB → DXGI_FORMAT_R10G10B10A2_UNORM
/// </summary>
void
MLConverter::Rgb12bitToR10G10B10A2(const uint32_t* pFrom, uint32_t* pTo, const int width, const int height, const uint8_t alpha)
{
    /// widthは8で割り切れます。
    assert((width & 7) == 0);

    const uint8_t a = (alpha>>6) & 0x3;
    const int pixelCount = width * height;

#pragma omp parallel for
    for (int i=0; i<pixelCount/8; ++i) {
        const int fromPos = i * 9;
        const int toPos = i * 8;

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
    }
}

/// <summary>
/// bmdFormat12BitRGB → RGB10bit r210
/// </summary>
void
MLConverter::Rgb12bitToR210(const uint32_t* pFrom, uint32_t* pTo, const int width, const int height)
{
    /// widthは8で割り切れます。
    assert((width & 7) == 0);

    const uint8_t a = 0x3;
    const int pixelCount = width * height;

#pragma omp parallel for
    for (int i=0; i<pixelCount/8; ++i) {
        const int fromPos = i * 9;
        const int toPos = i * 8;

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
    }
}


/// <summary>
/// bmdFormat12BitRGB → DXGI_FORMAT_R16G16B16A16_UNORM
/// </summary>
void
MLConverter::Rgb12bitToR16G16B16A16(const uint32_t* pFrom, uint64_t* pTo, const int width, const int height, const uint8_t alpha)
{
    /// widthは8で割り切れます。
    assert((width & 7) == 0);

    const uint16_t a = alpha * 257; //< 255 * 257 = 65535
    const int pixelCount = width * height;

#pragma omp parallel for
    for (int i = 0; i < pixelCount / 8; ++i) {
        const int fromPos = i * 9;
        const int toPos = i * 8;

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
    }
}
