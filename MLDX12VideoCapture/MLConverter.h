#pragma once

#include <stdint.h>

class MLConverter {
public:
    MLConverter(void);
    ~MLConverter(void);

    void CreateGammaTable(float gamma);
    void RawYuvV210ToRGBA(uint32_t *pFrom, uint32_t *pTo, const int width, const int height);

    static void YuvV210ToYuvA(uint32_t *pFrom, uint32_t *pTo, const int width, const int height);
    static void Rgb10bitToRGBA(uint32_t *pFrom, uint32_t *pTo, const int width, const int height);
private:
    // 12bit value to 8bit value
    uint8_t mGammaTable[4096];

    uint8_t *mBayer;
};
