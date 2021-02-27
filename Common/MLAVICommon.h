#pragma once

#include <stdint.h>
#include <string>

uint32_t MLStringToFourCC(const char* s);

const std::string MLFourCCtoString(uint32_t fourcc);

enum MLAviImageFormat {
    MLIF_Unknown = -1,
    MLIF_YUV422_UYVY,
    MLIF_YUV422_v210,
    MLIF_RGB10bit_r210,
    MLIF_RGB12bit_R12B,
};

uint32_t
MLAviImageFormatToFourcc(MLAviImageFormat t);

int
MLAviImageFormatToBitsPerPixel(MLAviImageFormat t);


enum MLFOURCC {
    MLFOURCC_RIFF = 0x46464952,
    MLFOURCC_AVI  = 0x20495641,
    MLFOURCC_LIST = 0x5453494c,
    MLFOURCC_avih = 0x68697661,
    MLFOURCC_strh = 0x68727473,
    MLFOURCC_strf = 0x66727473,
    MLFOURCC_vids = 0x73646976,
    MLFOURCC_auds = 0x73647561,
    MLFOURCC_v210 = 0x30313276,
    MLFOURCC_AVIX = 0x58495641,
    MLFOURCC_movi = 0x69766f6d,
    MLFOURCC_00db = 0x62643030,
    MLFOURCC_00dc = 0x63643030,
    MLFOURCC_01wb = 0x62773130,
};

struct MLAviMainHeader {
    uint32_t fcc; //< "avih"
    uint32_t cb;
    uint32_t dwMicroSecPerFrame;
    uint32_t dwMaxBytesPersec;
    uint32_t dwPaddingGranularity;
    uint32_t dwFlags;

    uint32_t dwTotalFrames;
    uint32_t dwInitialFrames;
    uint32_t dwStreams;
    uint32_t dwSuggestedBufferSize;
    uint32_t dwWidth;

    uint32_t dwHeight;
    uint32_t dwReserved0;
    uint32_t dwReserved1;
    uint32_t dwReserved2;
    uint32_t dwReserved3;
};

struct MLAviStreamHeader {
    uint32_t fcc; //< "strh"
    uint32_t cb;
    uint32_t fccType;
    uint32_t fccHandler;
    uint32_t dwFlags;
    uint16_t wPriority;
    uint16_t wLanguage;

    uint32_t dwInitialFrames;
    uint32_t dwScale;
    uint32_t dwRate;
    uint32_t dwStart;
    uint32_t dwLength;

    uint32_t dwSuggestedBufferSize;
    uint32_t dwQuality;
    uint32_t dwSampleSize;
    short left;
    short top;

    short right;
    short bottom;
};

struct MLBitmapInfoHeader {
    uint32_t biSize;
    int biWidth;
    int biHeight;
    short biPlanes;
    short biBitCount;

    uint32_t biCompression;
    uint32_t biSizeImage;
    int biXPelsPerMeter;
    int biYPelsPerMeter;
    uint32_t biClrUsed;

    uint32_t biClrImportant;
};

// 18bytes
struct MLWaveFormatExtensible {
        uint16_t  wFormatTag;
        short  nChannels;
        uint32_t nSamplesPerSec;
        uint32_t nAvgBytesPerSec;
        short  nBlockAlign;
        short  wBitsPerSample;
        short  cbSize;
        short  wValidBitsPerSample;
        uint32_t dwChannelMask;
        uint32_t subFormatGuid[4];
};

struct MLRiffHeader {
    uint32_t riff;
    uint32_t bytes;
    uint32_t type;
};

struct MLListHeader {
    uint32_t LIST;
    uint32_t bytes;
    uint32_t type;
};

struct MLStreamDataHeader {
    uint32_t fcc;
    uint32_t bytes;
};

