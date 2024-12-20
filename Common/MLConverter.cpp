#define NOMINMAX
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

// ■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■
// Quantization range conversion for R,G,B and Y

/// Limited: 16 to 235
/// Full:    0 to 255
static uint8_t
LimitedToFull_8bit(uint8_t v)
{
    return (uint8_t)((v - 16) * 255 / 219);
}

/// Full:    0 to 255
/// Limited: 16 to 235
static uint8_t
FullToLimited_8bit(uint8_t v)
{
    return (uint8_t)(16 + 219 * v / 255);
}

/// Limited: 64 to 940
/// Full:    0 to 1023
static uint16_t
LimitedToFull_10bit(uint16_t v)
{
    return (uint16_t)((v - 64) * 1023 / 876);
}

/// Full:    0 to 1023
/// Limited: 64 to 940
static uint16_t
FullToLimited_10bit(uint16_t v) {
    return (uint16_t)(64 + 876 * v / 1023);
}

// ■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■
// YUV ⇔ RGB
// https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.2020_conversion

#define Yuv_i8_to_f32 \
    const float y = y8 / 255.0f; \
    const float u = u8 / 255.0f; \
    const float v = v8 / 255.0f; \

#define Yuv_i10_to_f32 \
    const float y = y10 / 1023.0f; \
    const float u = u10 / 1023.0f; \
    const float v = v10 / 1023.0f;

#define Rgb_f32_to_i8_return \
    *r8_return = (uint8_t)(r * 255.0f); \
    *g8_return = (uint8_t)(g * 255.0f); \
    *b8_return = (uint8_t)(b * 255.0f);

#define Rgb_f32_to_i10_return \
    *r10_return = (uint16_t)(r * 1023.0f); \
    *g10_return = (uint16_t)(g * 1023.0f); \
    *b10_return = (uint16_t)(b * 1023.0f);

#define BT601_YuvRgb \
    const float r = Saturate01(1.1644f * y + 0.0000f * u + 1.5960f * v - 0.8742f); \
    const float g = Saturate01(1.1644f * y - 0.3918f * u - 0.8130f * v + 0.5317f); \
    const float b = Saturate01(1.1644f * y + 2.0172f * u + 0.0000f * v - 1.0856f);

#define Rec709_YuvRgb \
    const float r = Saturate01(1.1646f * y + 0.0005f * u + 1.7928f * v - 0.9733f); \
    const float g = Saturate01(1.1646f * y - 0.2134f * u - 0.5331f * v + 0.3017f); \
    const float b = Saturate01(1.1646f * y + 2.1125f * u + 0.0002f * v - 1.1336f);

#define Rec2020_YuvRgb \
    const float r = Saturate01(1.1644f * y + 0.0002f * u + 1.6786f * v - 0.9156f); \
    const float g = Saturate01(1.1644f * y - 0.1873f * u - 0.6504f * v + 0.3474f); \
    const float b = Saturate01(1.1644f * y + 2.1417f * u + 0.0001f * v - 1.1481f);

/// <summary>
/// BT.601 8bit YUV → 8bit RGB
/// </summary>
static void
Bt601_Yuv8ToRgb8(
    const uint8_t y8, const uint8_t u8, const uint8_t v8,
    uint8_t* r8_return, uint8_t* g8_return, uint8_t* b8_return)
{
    Yuv_i8_to_f32;
    BT601_YuvRgb;
    Rgb_f32_to_i8_return;
}

/// <summary>
/// BT.601 10bit YUV → 10bit RGB
/// </summary>
static void
Bt601_Yuv10ToRgb10(
        const uint16_t y10, const uint16_t u10, const uint16_t v10,
        uint16_t* r10_return, uint16_t* g10_return, uint16_t* b10_return)
{
    Yuv_i10_to_f32;
    BT601_YuvRgb;
    Rgb_f32_to_i10_return;
}

/// <summary>
/// Rec.709 8bit YUV → 8bit RGB
/// </summary>
static void
Rec709_Yuv8ToRgb8(
        const uint8_t y8, const uint8_t u8, const uint8_t v8,
        uint8_t* r8_return, uint8_t* g8_return, uint8_t* b8_return)
{
    Yuv_i8_to_f32;
    Rec709_YuvRgb;
    Rgb_f32_to_i8_return;
}

/// <summary>
/// Rec.709 10bit YUV → 10bit RGB
/// </summary>
static void
Rec709_Yuv10ToRgb10(
        const uint16_t y10, const uint16_t u10, const uint16_t v10,
        uint16_t* r10_return, uint16_t* g10_return, uint16_t* b10_return)
{
    Yuv_i10_to_f32;
    Rec709_YuvRgb;
    Rgb_f32_to_i10_return;
}

/// <summary>
/// Rec.2020 8bit YUV → 8bit RGB
/// </summary>
static void
Rec2020_Yuv8ToRgb8(
    const uint8_t y8, const uint8_t u8, const uint8_t v8,
    uint8_t* r8_return, uint8_t* g8_return, uint8_t* b8_return) {
    Yuv_i8_to_f32;
    Rec2020_YuvRgb;
    Rgb_f32_to_i8_return;
}

/// <summary>
/// Rec.2020 10bit YUV → 10bit RGB
/// </summary>
static void
Rec2020_Yuv10ToRgb10(
    const uint16_t y10, const uint16_t u10, const uint16_t v10,
    uint16_t* r10_return, uint16_t* g10_return, uint16_t* b10_return) {
    Yuv_i10_to_f32;
    Rec2020_YuvRgb;
    Rgb_f32_to_i10_return;
}

static void
Yuv8ToRgb8(
        const MLConverter::ColorSpace colorSpace,
        const uint8_t y8, const uint8_t u8, const uint8_t v8,
        uint8_t* r8_return, uint8_t* g8_return, uint8_t* b8_return)
{
    switch (colorSpace) {
    case MLConverter::CS_Rec601:
        Bt601_Yuv8ToRgb8(y8, u8, v8, r8_return, g8_return, b8_return);
        break;
    case MLConverter::CS_Rec709:
        Rec709_Yuv8ToRgb8(y8, u8, v8, r8_return, g8_return, b8_return);
        break;
    case MLConverter::CS_Rec2020:
        Rec2020_Yuv8ToRgb8(y8, u8, v8, r8_return, g8_return, b8_return);
        break;
    }
}

static void
Yuv10ToRgb10(
        const MLConverter::ColorSpace colorSpace,
        const uint16_t y10, const uint16_t u10, const uint16_t v10,
        uint16_t* r10_return, uint16_t* g10_return, uint16_t* b10_return)
{
    switch (colorSpace) {
    case MLConverter::CS_Rec601:
        Bt601_Yuv10ToRgb10(y10, u10, v10, r10_return, g10_return, b10_return);
        break;
    case MLConverter::CS_Rec709:
        Rec709_Yuv10ToRgb10(y10, u10, v10, r10_return, g10_return, b10_return);
        break;
    case MLConverter::CS_Rec2020:
        Rec2020_Yuv10ToRgb10(y10, u10, v10, r10_return, g10_return, b10_return);
        break;
    }
}

// ■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■■
// ガンマカーブ 

static float
SRGBtoLinear(float v)
{
    // Approximately pow(color, 2.2)
    v = v < 0.04045f ? v / 12.92f : pow(abs(v + 0.055f) / 1.055f, 2.4f);
    return v;
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

    // BM raw 12bit value to 8bit value
    CreateBMGammaTable(2.2f, 1.0f, 1.0f, 1.0f);
}

void
MLConverter::CreateBMGammaTable(const float gamma, const float gainR, const float gainG, const float gainB)
{
    memset(mBMGammaTableR, 0, sizeof mBMGammaTableR);
    memset(mBMGammaTableG, 0, sizeof mBMGammaTableG);
    memset(mBMGammaTableB, 0, sizeof mBMGammaTableB);

    /* G R
     * B G
     */
    mBMGammaBayerTable[0] = mBMGammaTableG;
    mBMGammaBayerTable[1] = mBMGammaTableR;
    mBMGammaBayerTable[2] = mBMGammaTableB;
    mBMGammaBayerTable[3] = mBMGammaTableG;

    // 12bit value to 8bit value
    for (int i = 0; i < 4096; ++i) {
        const float y = (const float)(i) / 4095.0f;
        const float r = pow(std::min(1.0f, y * gainR), 1.0f / gamma);
        const float g = pow(std::min(1.0f, y * gainG), 1.0f / gamma);
        const float b = pow(std::min(1.0f, y * gainB), 1.0f / gamma);
        mBMGammaTableR[i] = (int)floor(255.0f * r + 0.5f);
        mBMGammaTableG[i] = (int)floor(255.0f * g + 0.5f);
        mBMGammaTableB[i] = (int)floor(255.0f * b + 0.5f);
    }
}

void
MLConverter::R8G8B8A8ToB8G8R8_DIB(const uint32_t* pFrom, uint8_t* pTo, const int width, const int height)
{
#pragma omp parallel for
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int posR = x + y * width;

            // BMPは上下反転する。
            const int posW = 3 * (x + (height - y - 1) * width);

            const uint32_t w = pFrom[posR];

            const uint8_t r = (w >> 0) & 0xff;
            const uint8_t g = (w >> 8) & 0xff;
            const uint8_t b = (w >> 16) & 0xff;
            pTo[posW + 0] = b;
            pTo[posW + 1] = g;
            pTo[posW + 2] = r;
        }
    }
}


void
MLConverter::R8G8B8A8ToB8G8R8A8_DIB(const uint32_t* pFrom, uint8_t* pTo, const int width, const int height)
{
#pragma omp parallel for
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int posR = x + y * width;
            // BMPは上下反転する。
            const int posW = 4 * (x + (height - y - 1) * width);

            const uint32_t w = pFrom[posR];

            const uint8_t r = (w >> 0) & 0xff;
            const uint8_t g = (w >> 8) & 0xff;
            const uint8_t b = (w >> 16) & 0xff;
            const uint8_t a = (w >> 24) & 0xff;

            pTo[posW + 0] = b;
            pTo[posW + 1] = g;
            pTo[posW + 2] = r;
            pTo[posW + 3] = a;
        }
    }
}


void
MLConverter::R8G8B8ToB8G8R8_DIB(const uint8_t* pFrom, uint8_t* pTo, const int width, const int height)
{
#pragma omp parallel for
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int posR = 3 * (x + y * width);
            // BMPは上下反転する。
            const int posW = 3 * (x + (height-y-1) * width);

            const uint8_t r = pFrom[posR + 0];
            const uint8_t g = pFrom[posR + 1];
            const uint8_t b = pFrom[posR + 2];
            pTo[posW + 0] = b;
            pTo[posW + 1] = g;
            pTo[posW + 2] = r;
        }
    }
}


void
MLConverter::R8G8B8ToB8G8R8A8_DIB(const uint8_t* pFrom, uint8_t* pTo, const int width, const int height, const uint8_t alpha)
{
#pragma omp parallel for
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int posR = 3 * (x + y * width);
            // BMPは上下反転する。
            const int posW = 4 * (x + (height - y - 1) * width);

            const uint8_t r = pFrom[posR + 0];
            const uint8_t g = pFrom[posR + 1];
            const uint8_t b = pFrom[posR + 2];
            pTo[posW + 0] = b;
            pTo[posW + 1] = g;
            pTo[posW + 2] = r;
            pTo[posW + 3] = alpha;
        }
    }
}

void
MLConverter::B8G8R8DIBToR8G8B8A8(const uint8_t* pFrom, uint32_t* pTo, const int width, const int height, const uint8_t alpha)
{
#pragma omp parallel for
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int posR = 3 * (x + y * width);
            // BMPは上下反転する。
            const int posW = x + (height - y - 1) * width;

            const uint8_t b = pFrom[posR + 0];
            const uint8_t g = pFrom[posR + 1];
            const uint8_t r = pFrom[posR + 2];
            pTo[posW] = r + (g << 8) + (b << 16) + (alpha << 24);
        }
    }
}


void
MLConverter::Uyvy8bitToR8G8B8A8(const ColorSpace colorSpace, const uint32_t* pFrom, uint32_t* pTo, const int width, const int height)
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

            Yuv8ToRgb8(colorSpace, y0, u, v, &r, &g, &b);
            pTo[writeP + 0] = (a << 24) + (b << 16) + (g << 8) + r;

            Yuv8ToRgb8(colorSpace, y1, u, v, &r, &g, &b);
            pTo[writeP + 1] = (a << 24) + (b << 16) + (g << 8) + r;
        }
    }
}
/// <summary>
/// bmdFormat10BitYUV v210 → DXGI_FORMAT_R10G10B10A2_UNORM
/// </summary>
void
MLConverter::Yuv422_10bitToR10G10B10A2(const ColorSpace colorSpace, const uint32_t* pFrom, uint32_t* pTo, const int width, const int height, const uint8_t alpha)
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

            Yuv10ToRgb10(colorSpace, y0, u0, v0, &r, &g, &b);
            pTo[writeP + 0] = (a << 30) + (b << 20) + (g << 10) + r;

            Yuv10ToRgb10(colorSpace, y1, u0, v0, &r, &g, &b);
            pTo[writeP + 1] = (a << 30) + (b << 20) + (g << 10) + r;

            Yuv10ToRgb10(colorSpace, y2, u2, v2, &r, &g, &b);
            pTo[writeP + 2] = (a << 30) + (b << 20) + (g << 10) + r;

            Yuv10ToRgb10(colorSpace, y3, u2, v2, &r, &g, &b);
            pTo[writeP + 3] = (a << 30) + (b << 20) + (g << 10) + r;

            Yuv10ToRgb10(colorSpace, y4, u4, v4, &r, &g, &b);
            pTo[writeP + 4] = (a << 30) + (b << 20) + (g << 10) + r;

            Yuv10ToRgb10(colorSpace, y5, u4, v4, &r, &g, &b);
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

void
MLConverter::YuvV210ToYuvA(uint32_t* pFrom, uint32_t* pTo, const int width, const int height, const uint8_t alpha)
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

static const int BAYER_W = 3840 + 32;
static const int BAYER_H = 2160 + 32;
static const int BAYER_WH = BAYER_W * BAYER_H;

void
MLConverter::BMRawYuvV210ToRGBA(uint32_t* pFrom, uint32_t* pTo, const int width, const int height, const uint8_t alpha)
{
    assert(width == 3840);
    assert(height == 2160);

    uint8_t* bayer = new uint8_t[BAYER_WH];

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
        const uint8_t y0 = (pF0 & 0x0003fc00) >> 10;
        const uint8_t cb0 = (pF0 & 0x000000ff);

        const uint8_t y2 = (pF1 & 0x0ff00000) >> 20;
        const uint8_t cb2 = (pF1 & 0x0003fc00) >> 10;
        const uint8_t y1 = (pF1 & 0x000000ff);

        const uint8_t cb4 = (pF2 & 0x0ff00000) >> 20;
        const uint8_t y3 = (pF2 & 0x0003fc00) >> 10;
        const uint8_t cr2 = (pF2 & 0x000000ff);

        const uint8_t y5 = (pF3 & 0x0ff00000) >> 20;
        const uint8_t cr4 = (pF3 & 0x0003fc00) >> 10;
        const uint8_t y4 = (pF3 & 0x000000ff);

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
        bayer[posT + 0] = BMGammaTable(x + 0, y, p0_12);
        bayer[posT + 1] = BMGammaTable(x + 1, y, p1_12);
        bayer[posT + 2] = BMGammaTable(x + 2, y, p2_12);
        bayer[posT + 3] = BMGammaTable(x + 3, y, p3_12);

        bayer[posT + 4] = BMGammaTable(x + 4, y, p4_12);
        bayer[posT + 5] = BMGammaTable(x + 5, y, p5_12);
        bayer[posT + 6] = BMGammaTable(x + 6, y, p6_12);
        bayer[posT + 7] = BMGammaTable(x + 7, y, p7_12);

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
            const uint8_t g0 = bayer[x + 0 + 16 + (y + 0 + 16) * BAYER_W];
            const uint8_t r = bayer[x + 1 + 16 + (y + 0 + 16) * BAYER_W];
            const uint8_t b = bayer[x + 0 + 16 + (y + 1 + 16) * BAYER_W];
            const uint8_t g1 = bayer[x + 1 + 16 + (y + 1 + 16) * BAYER_W];

            pTo[x + 0 + (y + 0) * width] = (a << 24) + (b << 16) + (g0 << 8) + r;
            pTo[x + 1 + (y + 0) * width] = (a << 24) + (b << 16) + (g0 << 8) + r;
            pTo[x + 0 + (y + 1) * width] = (a << 24) + (b << 16) + (g1 << 8) + r;
            pTo[x + 1 + (y + 1) * width] = (a << 24) + (b << 16) + (g1 << 8) + r;
        }
    }

    delete[] bayer;
    bayer = nullptr;
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

void
MLConverter::R10G10B10A2ToExrHalfFloat(
        const uint32_t* pFrom, uint16_t* pTo, const int width, const int height, const uint8_t alpha, GammaType gamma, QuantizationRange qr)
{
    // アルファチャンネルは別の計算式。
    // 0.0〜1.0の範囲の値。
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

            // 10bit RGB値。0〜1023。
            uint16_t r = (v >> 20) & 0x3ff;
            uint16_t g = (v >> 10) & 0x3ff;
            uint16_t b = (v >> 0) & 0x3ff;

            switch (gamma){
            case GT_SDR_22:
                {
                    if (qr == QR_Limited) {
                        r = LimitedToFull_10bit(r);
                        g = LimitedToFull_10bit(g);
                        b = LimitedToFull_10bit(b);
                    }

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
/// 16bit RGBA to R210
/// </summary>
void
MLConverter::R16G16B16A16ToR210(const uint16_t* pFrom, uint32_t* pTo, const int width, const int height)
{
#pragma omp parallel for
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int posR = 4 * (x + y * width);
            const int posW = x + y * width;

            // quantize to 10bit
            const uint32_t r = (pFrom[posR + 0] >> 6) & 0x3ff;
            const uint32_t g = (pFrom[posR + 1] >> 6) & 0x3ff;
            const uint32_t b = (pFrom[posR + 2] >> 6) & 0x3ff;

            // alpha: quantize to 2bit
            const uint32_t a = (pFrom[posR + 3] >> 14) & 0x3;

            // bmdFormat10BitRGB
            // ビッグエンディアンのX2R10G10B10

            const uint32_t r210 = (a << 30) + (r << 20) + (g << 10) + b;
            pTo[posW] = HtoNL(r210);
        }
    }
}


void
MLConverter::R16G16B16A16ToExrHalfFloat(
        const uint16_t* pFrom, uint16_t* pTo, const int width, const int height)
{
#pragma omp parallel for
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int pos = 4 * (x + y * width);

            const float rF = ST2084toExrLinear(pFrom[pos + 0] / 65535.0f);
            const float gF = ST2084toExrLinear(pFrom[pos + 1] / 65535.0f);
            const float bF = ST2084toExrLinear(pFrom[pos + 2] / 65535.0f);
            const float aF =                   pFrom[pos + 3] / 65535.0f; //< アルファチャンネルは別の計算式。

            const half& rH = rF;
            const half& gH = gF;
            const half& bH = bF;
            const half& aH = aF;

            pTo[pos + 0] = rH.bits();
            pTo[pos + 1] = gH.bits();
            pTo[pos + 2] = bH.bits();
            pTo[pos + 3] = aH.bits();
        }
    }
}



void
MLConverter::R8G8B8A8ToExrHalfFloat(
        const uint8_t* pFrom, uint16_t* pTo, const int width, const int height, QuantizationRange qr)
{
#pragma omp parallel for
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int pos = 4 * (x + y * width);

            uint8_t r = pFrom[pos + 0];
            uint8_t g = pFrom[pos + 1];
            uint8_t b = pFrom[pos + 2];
            uint8_t a = pFrom[pos + 3];

            if (qr == QR_Limited) {
                r = LimitedToFull_8bit(r);
                g = LimitedToFull_8bit(g);
                b = LimitedToFull_8bit(b);
            }

            // Quantization range == full
            const float rF = SRGBtoLinear(r / 255.0f);
            const float gF = SRGBtoLinear(g / 255.0f);
            const float bF = SRGBtoLinear(b / 255.0f);
            const float aF = a / 255.0f; //< アルファチャンネルは別の計算式。

            const half& rH = rF;
            const half& gH = gF;
            const half& bH = bF;
            const half& aH = aF;

            pTo[pos + 0] = rH.bits();
            pTo[pos + 1] = gH.bits();
            pTo[pos + 2] = bH.bits();
            pTo[pos + 3] = aH.bits();
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
