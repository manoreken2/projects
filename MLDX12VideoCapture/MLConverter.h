#pragma once

#include <stdint.h>

class MLConverter {
public:
    MLConverter(void);
    ~MLConverter(void);

    void CreateGammaTable(const float gamma, const float gainR, const float gainG, const float gainB);
    void RawYuvV210ToRGBA(uint32_t *pFrom, uint32_t *pTo, const int width, const int height, const uint8_t alpha);

    static void YuvV210ToYuvA(uint32_t *pFrom, uint32_t *pTo, const int width, const int height, const uint8_t alpha);
    static void Rgb10bitToRGBA(uint32_t *pFrom, uint32_t *pTo, const int width, const int height, const uint8_t alpha);
private:
    // 12bit value to 8bit value
    uint8_t mGammaTableR[4096];
    uint8_t mGammaTableG[4096];
    uint8_t mGammaTableB[4096];
    uint8_t *mGammaBayerTable[4];
    uint8_t *mBayer;

    uint8_t GammaTable(const int x, const int y, const uint16_t v) {
        const uint8_t *table = mGammaBayerTable[(x & 1) + 2 * (y & 1)];
        return table[v];
    }
};
