#pragma once

#include <stdint.h>
#include <vector>
#include <string>
#include <stdio.h>
#include "Common.h"
#include "MLAVICommon.h"
#include "MLVideoTime.h"

class MLAviReader {
public:
    MLAviReader(void);
    ~MLAviReader(void);

    bool Open(std::wstring path);

    const MLBitmapInfoHeader & ImageFormat(void) const { return mImageFormat; }
    int NumFrames(void) const { return (int)mImages.size(); }

    int FramesPerSec(void) const { return mAviStreamHeader.dwRate; }

    float DurationSec(void) const;

    MLVideoTime FrameNrToTime(const int frameNr);

    /// @return > 0 : copied bytes, negative value: error.
    int GetImage(const int frameNr,
            const uint32_t buffBytes, uint8_t *buff);

    void Close(void);

private:
    FILE *mFp;

    MLAviMainHeader mAviMainHeader;
    MLAviStreamHeader mAviStreamHeader;
    MLBitmapInfoHeader mImageFormat;

    struct ImagePosBytes {
        int64_t pos;
        uint32_t bytes;
    };

    std::vector<ImagePosBytes> mImages;

    bool ReadFourBytes(uint32_t &fcc_return);
    bool ReadRiff(void);
    bool ReadListHeader(void);
    bool ReadAviMainHeader(void);
    bool ReadAviStreamHeader(void);
    bool ReadAviStreamFormatHeader(void);
    bool ReadOneImage(void);
    bool SkipUnknownHeader(uint32_t fourcc);

    template <typename T>
    bool ReadHeader(T &v);

};

