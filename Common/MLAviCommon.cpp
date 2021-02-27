#include "MLAVICommon.h"
#include <string.h>

uint32_t MLStringToFourCC(const char *s)
{
    uint32_t fcc = 0;
    int count = (int)strlen(s);
    if (4 < count) {
        count = 4;
    }

    for (int i = 0; i < count; ++i) {
        fcc |= (((unsigned char)s[i]) << i * 8);
    }

    for (int i = count; i < 4; ++i) {
        fcc |= (((unsigned char)' ') << i * 8);
    }

    return fcc;
}



const std::string
MLFourCCtoString(uint32_t fourcc)
{
    char s[5];
    memset(s, 0, sizeof s);
    s[0] = (fourcc >> 0) & 0xff;
    s[1] = (fourcc >> 8) & 0xff;
    s[2] = (fourcc >> 16) & 0xff;
    s[3] = (fourcc >> 24) & 0xff;

    for (int i = 0; i < 4; ++i) {
        if (s[i] < 0) {
            // ASCIIではない文字。
            s[i] = '?';
        } else if (s[i] < 0x20) {
            // 制御文字をスペースにする。
            s[i] = ' ';
        }
    }

    return std::string(s);
}
