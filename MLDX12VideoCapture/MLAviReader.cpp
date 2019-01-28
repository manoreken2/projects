#include "MLAviReader.h"
#include <assert.h>

MLAviReader::MLAviReader(void)
        : mFp(nullptr)
{
    memset(&mAviMainHeader, 0, sizeof mAviMainHeader);
    memset(&mAviStreamHeader, 0, sizeof mAviStreamHeader);
    memset(&mImageFormat, 0, sizeof mImageFormat);
    mImageCache.Set(-1, 0, nullptr);
}

MLAviReader::~MLAviReader(void)
{
    Close();
}

MLVideoTime
MLAviReader::FrameNrToTime(const int frameNr)
{
    MLVideoTime r;
    memset(&r, 0, sizeof r);

    if (FramesPerSec() == 0) {
        return r;
    }

    r.frame = frameNr % FramesPerSec();
    const int totalSec = frameNr / FramesPerSec();

    r.hour = totalSec / 3600;
    int remain = totalSec - r.hour * 3600;
    r.min = remain / 60;
    remain -= r.min * 60;
    r.sec = remain;

    return r;
}

float
MLAviReader::DurationSec(void) const
{
    if (mAviStreamHeader.dwRate <= 0) {
        return 0.0f;
    }
    return (float)mImages.size() / mAviStreamHeader.dwRate;
}

bool
MLAviReader::ReadFourBytes(uint32_t & fourcc_return)
{
    assert(mFp);

    fourcc_return = 0;

    int count = (int)fread(&fourcc_return, 1, 4, mFp);
    if (count != 4) {
        fourcc_return = 0;
        return false;
    }

    return true;
}

void
MLAviReader::Close(void)
{
    if (mFp != nullptr) {
        fclose(mFp);
        mFp = nullptr;
    }

    mImages.clear();
    memset(&mAviMainHeader, 0, sizeof mAviMainHeader);
    memset(&mAviStreamHeader, 0, sizeof mAviStreamHeader);
    memset(&mImageFormat, 0, sizeof mImageFormat);

    delete [] mImageCache.buff;
    mImageCache.Set(-1, 0, nullptr);
}

bool
MLAviReader::Open(std::wstring path)
{
    if (mFp != nullptr) {
        printf("Error: MLAviReader::Open() already open\n");
        return false;
    }

    mImages.clear();

    errno_t ercd = _wfopen_s(&mFp, path.c_str(), L"rb");
    if (ercd != 0) {
        printf("Error: MLAviWriter::Init() fopen failed\n");
        mFp = nullptr;
        return false;
    }

    /* RIFF "AVI "
         LIST "hdrl"
           AviMainHeader
           LIST "strl"
             AviStreamHeader
             "strf" BitmapInfoHeader
         LIST "movi"
           00db
           00db
           ...
       RIFF "AVIX"
         LIST "movi"
           00db
           00db
           ...
    */

    do {
        uint32_t fourcc;
        if (!ReadFourBytes(fourcc)) {
            break;
        }

#ifndef NDEBUG
        int64_t pos = _ftelli64(mFp);
        char s[256];
        sprintf_s(s, "%016llx Header %s\n", pos, MLFourCCtoString(fourcc).c_str());
        OutputDebugStringA(s);
#endif

        switch (fourcc) {
        case MLFOURCC_RIFF:
            if (!ReadRiff()) {
                printf("Error: ReadRiff failed\n");
                Close();
                return false;
            }
            break;
        case MLFOURCC_LIST:
            if (!ReadListHeader()) {
                printf("Error: ReadListHeader failed\n");
                Close();
                return false;
            }
            break;
        case MLFOURCC_avih:
            if (!ReadAviMainHeader()) {
                printf("Error: ReadAviMainHeader failed\n");
                Close();
                return false;
            }
            break;
        case MLFOURCC_strh:
            if (!ReadAviStreamHeader()) {
                printf("Error: ReadAviStreamHeader failed\n");
                Close();
                return false;
            }
            break;
        case MLFOURCC_strf:
            if (!ReadAviStreamFormatHeader()) {
                printf("Error: ReadAviStreamFormatHeader failed\n");
                Close();
                return false;
            }
            break;
        case MLFOURCC_00db:
        case MLFOURCC_00dc:
            if (!ReadOneImage()) {
                printf("Error: ReadOneImage failed\n");
                Close();
                return false;
            }
            break;
        default:
            if (!SkipUnknownHeader(fourcc)) {
                printf("Error: SkipUnknownHeader failed\n");
                Close();
                return false;
            }
        }
    } while (true);

    if (mImages.size() <= 0) {
        printf("Error: failed to read AVI\n");
        Close();
        return false;
    }

    return true;
}

template <typename T>
bool MLAviReader::ReadHeader(T &v)
{
    assert(mFp);

    uint8_t *p = (uint8_t*)&v;
    int count = (int)fread(p+4, 1, sizeof v - 4, mFp);
    if (count != sizeof v - 4) {
        printf("Error: ReadHeader() fread failed\n");
        return false;
    }

    return true;
}

bool
MLAviReader::ReadRiff(void)
{
    MLRiffHeader h;
    if (!ReadHeader(h)) {
        printf("Error: ReadRiff() ReadHeader() failed\n");
    }

    if (h.type != MLFOURCC_AVI
        && h.type != MLFOURCC_AVIX) {
        printf("Error: ReadRiff() found non-AVI RIFF\n");
        return false;
    }

    return true;
}

bool
MLAviReader::ReadListHeader(void)
{
    assert(mFp);

    MLListHeader h;
    if (!ReadHeader(h)) {
        printf("Error: ReadListHeader() ReadHeader() failed\n");
    }

    return true;
}

bool
MLAviReader::ReadAviMainHeader(void)
{
    assert(mFp);

    if (!ReadHeader(mAviMainHeader)) {
        printf("Error: ReadAviMainHeader() ReadHeader() failed\n");
    }

    return true;
}

bool
MLAviReader::ReadAviStreamHeader(void)
{
    assert(mFp);

    if (!ReadHeader(mAviStreamHeader)) {
        printf("Error: ReadAviStreamHeader() ReadHeader() failed\n");
    }

    return true;
}

bool
MLAviReader::ReadAviStreamFormatHeader(void)
{
    assert(mFp);

    uint32_t bytes = 0;
    if (!ReadFourBytes(bytes)) {
        printf("Error: ReadAviStreamFormatHeader() read size failed\n");
        return false;
    }

    if (mAviStreamHeader.fccType == MLFOURCC_vids
            && bytes == 40) {
        // BITMAPINFOHEADER
        if (40 != fread(&mImageFormat, 1, 40, mFp)) {
            printf("Error: ReadAviStreamFormatHeader() read BITMAPINFOHEADER failed\n");
            return false;
        }
    } else {
        _fseeki64(mFp, bytes, SEEK_CUR);
    }

    return true;
}

bool
MLAviReader::ReadOneImage(void)
{
    assert(mFp);

    uint32_t bytes = 0;
    if (!ReadFourBytes(bytes)) {
        printf("Error: ReadOneImage() read size failed\n");
        return false;
    }

    int64_t pos = _ftelli64(mFp);

    ImagePosBytes pb;
    pb.pos = pos;
    pb.bytes = bytes;
    mImages.push_back(pb);

    _fseeki64(mFp, bytes, SEEK_CUR);

    return true;
}

bool
MLAviReader::SkipUnknownHeader(uint32_t fourcc)
{
    assert(mFp);

    struct UnknownHeader {
        uint32_t fourcc;
        uint32_t bytes;
    };

    UnknownHeader h;
    h.fourcc = fourcc;
    if (!ReadHeader(h)) {
        printf("Error: SkipUnknownHeader() ReadHeader() failed\n");
    }

    int64_t pos = _ftelli64(mFp);
    _fseeki64(mFp, h.bytes, SEEK_CUR);

#ifndef NDEBUG
    char s[256];
    sprintf_s(s, "%016llx SkipUnknownHeader(%s) %u bytes\n", pos, MLFourCCtoString(fourcc).c_str(), h.bytes);
    OutputDebugStringA(s);
#endif

    return true;
}

int
MLAviReader::GetImage(const int frameNr, const uint32_t buffBytes, uint8_t *buff)
{
    if (frameNr < 0 || mImages.size() <= frameNr) {
        printf("Error: MLAviReader::GetImage() out of range\n");
        return E_FAIL;
    }

    if (mFp == nullptr) {
        printf("Error: MLAviReader::GetImage() when file is not open\n");
        return E_FAIL;
    }

    ImagePosBytes pb = mImages[frameNr];
    if (buffBytes < pb.bytes) {
        printf("Error: buffBytes is too small\n");
        return E_FAIL;
    }

    if (mImageCache.frameNr == frameNr) {
        assert(mImageCache.bytes <= buffBytes);
        // cache hit.
        memcpy(buff, mImageCache.buff, mImageCache.bytes);
        return mImageCache.bytes;
    }

    _fseeki64(mFp, pb.pos, SEEK_SET);

    int nBytes = (int)fread(buff, 1, buffBytes, mFp);

    delete [] mImageCache.buff;
    mImageCache.buff = nullptr;

    mImageCache.frameNr = frameNr;
    mImageCache.bytes = nBytes;
    mImageCache.buff = new uint8_t[nBytes];
    memcpy(mImageCache.buff, buff, nBytes);

    return nBytes;
}
