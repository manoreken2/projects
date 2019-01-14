#include "MLAviWriter.h"
#include <assert.h>
#include "WinApp.h"

namespace {
    struct AviMainHeader {
         uint32_t fcc;
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

    struct AviStreamHeader {
         uint32_t fcc;
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

    struct BitmapInfoHeader {
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

    struct RiffHeader {
        uint32_t riff;
        uint32_t bytes;
        uint32_t type;
    };

    struct ListHeader {
        uint32_t LIST;
        uint32_t bytes;
        uint32_t type;
    };

    struct StreamDataHeader {
        uint32_t fcc;
        uint32_t bytes;
    };

    uint32_t StringToFourCC(const char *s) {
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


};

MLAviWriter::MLAviWriter(void)
    : mFp(nullptr), mLastRiffIdx(-1), mLastMoviIdx(-1)
{
    mState = AVIS_Init;
    m_thread = nullptr;
    m_shutdownEvent = CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
    m_readyEvent    = CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
}


MLAviWriter::~MLAviWriter(void)
{
    if (nullptr != m_readyEvent) {
        CloseHandle(m_readyEvent);
        m_readyEvent = nullptr;
    }

    if (nullptr != m_shutdownEvent) {
        CloseHandle(m_shutdownEvent);
        m_shutdownEvent = nullptr;
    }
}

void MLAviWriter::WriteFccHeader(const char *fccS, int bytes) {
    uint32_t fcc = StringToFourCC(fccS);
    fwrite(&fcc, 1, 4, mFp);
    fwrite(&bytes, 1, 4, mFp);
}


bool MLAviWriter::Init(std::wstring path, int width, int height, int fps, ImageFormat imgFmt) {
    assert(mFp == nullptr);

    mWidth = width;
    mHeight = height;
    mFps = fps;
    mImgFmt = imgFmt;

    errno_t ercd = _wfopen_s(&mFp, path.c_str(), L"wb");
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
    int hAvi = WriteRiff("AVI");
    int hHdrl = WriteList("hdrl");
    WriteAviMainHeader();
    int hStrl = WriteList("strl");
    WriteAviStreamHeader();

    WriteFccHeader("strf", 40);
    WriteBitmapInfoHeader();
    FinishList(hStrl);
    FinishList(hHdrl);
    int movi = WriteList("movi");

    mLastRiffIdx = hAvi;
    mLastMoviIdx = movi;

    mTotalFrames = 0;

    StartThread();
    return true;
}

static const uint32_t  FCC_00db = 0x62643030;

void MLAviWriter::AddImage(const uint32_t * img, int bytes) {
    assert(mFp != nullptr);
    assert(m_readyEvent != nullptr);

    if (mState != AVIS_Writing) {
        return;
    }

    ImageItem ii;
    ii.buf = new uint32_t[bytes / 4];
    ii.bytes = bytes;
    memcpy(ii.buf, img, bytes);

    m_mutex.lock();
    mImageList.push_back(ii);
    m_mutex.unlock();

    SetEvent(m_readyEvent);
}

void MLAviWriter::Term(void) {
    assert(mFp != nullptr);

    StopThread();

    FinishList(mLastMoviIdx);
    FinishRiff(mLastRiffIdx);

    _fseeki64(mFp, mAviMainHeaderPos, SEEK_SET);
    WriteAviMainHeader();

    _fseeki64(mFp, mAviStreamHeaderPos, SEEK_SET);
    WriteAviStreamHeader();

    fclose(mFp);
    mFp = nullptr;
}

int MLAviWriter::RecQueueSize(void) {
    int count;
    m_mutex.lock();
    count = (int)mImageList.size();
    m_mutex.unlock();
    return count;

}

int MLAviWriter::WriteRiff(const char *fccS) {
    uint32_t fcc = StringToFourCC(fccS);

    int riffIdx = (int)mRiffChunks.size();

    RiffChunk rc;
    rc.pos = _ftelli64(mFp);
    rc.size = 0;
    rc.bDone = false;
    mRiffChunks.push_back(rc);

    RiffHeader rh;
    rh.riff = StringToFourCC("RIFF");
    rh.bytes = 0;
    rh.type = fcc;
    fwrite(&rh, 1, sizeof rh, mFp);

    return riffIdx;
}

void MLAviWriter::FinishRiff(int idx) {
    uint64_t pos = _ftelli64(mFp);

    RiffChunk &rc = mRiffChunks[idx];
    rc.size = pos - rc.pos - 8;

    assert(rc.size <= UINT32_MAX);
    uint32_t size32 = (uint32_t)rc.size;

    _fseeki64(mFp, rc.pos+4, SEEK_SET);
    fwrite(&size32, 1, 4, mFp);

    _fseeki64(mFp, pos, SEEK_SET);

    rc.bDone = true;
}

int MLAviWriter::WriteList(const char * fccS) {
    uint32_t fcc = StringToFourCC(fccS);

    int listIdx = (int)mListChunks.size();

    ListChunk lc;
    lc.pos = _ftelli64(mFp);
    lc.size = 0;
    lc.bDone = false;
    mListChunks.push_back(lc);

    ListHeader lh;
    lh.LIST = StringToFourCC("LIST");
    lh.bytes = 0;
    lh.type = fcc;
    fwrite(&lh, 1, sizeof lh, mFp);

    return listIdx;
}

void MLAviWriter::FinishList(int idx) {
    uint64_t pos = _ftelli64(mFp);

    ListChunk &lc = mListChunks[idx];
    lc.size = pos - lc.pos - 8;

    assert(lc.size <= UINT32_MAX);

    uint32_t size32 = (uint32_t)lc.size;

    _fseeki64(mFp, lc.pos+4, SEEK_SET);
    fwrite(&size32, 1, 4, mFp);

    _fseeki64(mFp, pos, SEEK_SET);

    lc.bDone = true;
}

uint32_t MLAviWriter::ImageBytes(void) const {
    assert(mImgFmt == IF_YUV422v210);
    return mWidth * mHeight * 8 / 3;
}

void MLAviWriter::WriteAviMainHeader(void) {
    mAviMainHeaderPos = _ftelli64(mFp);

    AviMainHeader mh;
    mh.fcc = StringToFourCC("avih");
    mh.cb = 0x38;
    mh.dwMicroSecPerFrame = (uint32_t)((1.0 / mFps) * 1000 * 1000);
    mh.dwMaxBytesPersec = 0;
    mh.dwPaddingGranularity = 0x1000;
    mh.dwFlags = 0;
    mh.dwTotalFrames = mTotalFrames;
    mh.dwInitialFrames = 0;
    mh.dwStreams = 1;
    mh.dwSuggestedBufferSize = ImageBytes();
    mh.dwWidth = mWidth;
    mh.dwHeight = mHeight;
    mh.dwReserved0 = 0;
    mh.dwReserved1 = 0;
    mh.dwReserved2 = 0;
    mh.dwReserved3 = 0;

    fwrite(&mh, 1, sizeof mh, mFp);
}

void MLAviWriter::WriteAviStreamHeader(void) {
    assert(mImgFmt == IF_YUV422v210);
        
    mAviStreamHeaderPos = _ftelli64(mFp);

    AviStreamHeader sh;
    sh.fcc = StringToFourCC("strh");
    sh.cb = 0x38;
    sh.fccType = StringToFourCC("vids");
    sh.fccHandler = StringToFourCC("v210");
    sh.dwFlags = 0;
    sh.wPriority = 0;
    sh.wLanguage = 0;
    sh.dwInitialFrames = 0;
    sh.dwScale = 1;
    sh.dwRate = mFps;
    sh.dwStart = 0;
    sh.dwLength = mTotalFrames;
    sh.dwSuggestedBufferSize = ImageBytes();
    sh.dwQuality = 9800;
    sh.dwSampleSize = sh.dwSuggestedBufferSize;
    sh.left = 0;
    sh.top = 0;
    sh.right = 0;
    sh.bottom = 0;

    fwrite(&sh, 1, sizeof sh, mFp);
}
void MLAviWriter::WriteBitmapInfoHeader(void) {
    assert(mImgFmt == IF_YUV422v210);

    BitmapInfoHeader ih;
    ih.biSize = 0x28;
    ih.biWidth = mWidth;
    ih.biHeight = mHeight;
    ih.biPlanes = 1;
    ih.biBitCount = 20;
    ih.biCompression = StringToFourCC("v210");
    ih.biSizeImage = ImageBytes();
    ih.biXPelsPerMeter = 0;
    ih.biYPelsPerMeter = 0;
    ih.biClrUsed = 0;
    ih.biClrImportant = 0;

    fwrite(&ih, 1, sizeof ih, mFp);
}

void MLAviWriter::WriteStreamDataHeader(uint32_t fcc, int bytes) {
    StreamDataHeader sd;
    sd.fcc = fcc;
    sd.bytes = bytes;

    fwrite(&sd, 1, sizeof sd, mFp);
}

void MLAviWriter::RestartRiff(void) {
    FinishList(mLastMoviIdx);
    FinishRiff(mLastRiffIdx);

    mLastRiffIdx = WriteRiff("AVIX");
    mLastMoviIdx = WriteList("movi");
}

// ============================================================
// thread

void MLAviWriter::StartThread(void)
{
    if (m_thread != nullptr) {
        return;
    }

    mState = AVIS_Writing;
    m_thread = CreateThread(nullptr, 0, AviWriterEntry, this, 0, nullptr);
}

void MLAviWriter::StopThread(void) {
    if (m_thread == nullptr) {
        return;
    }

    mState = AVIS_Init;

    assert(m_shutdownEvent != nullptr);
    SetEvent(m_shutdownEvent);
    WaitForSingleObject(m_thread, INFINITE);
    CloseHandle(m_thread);
    m_thread = nullptr;
}


DWORD WINAPI
MLAviWriter::AviWriterEntry(LPVOID param) {
    MLAviWriter *self = (MLAviWriter *)param;

    return self->ThreadMain();
}

DWORD
MLAviWriter::ThreadMain(void) {
    DWORD rv = 0;
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    HANDLE waitAry[2] = {m_shutdownEvent, m_readyEvent};
    DWORD waitRv;

    while (true) {
        waitRv = WaitForMultipleObjects(2, waitAry, FALSE, INFINITE);
        if (waitRv == WAIT_OBJECT_0) {
            // flush remaining items.
            WriteAll();
            OutputDebugStringA("MLAviWriter::ThreadMain() exit\n");
            break;
        }

        // ready event
        WriteAll();
    }

    CoUninitialize();
    return rv;
}

void
MLAviWriter::WriteAll(void) {
    ImageItem ii;

    m_mutex.lock();
    int count = (int)mImageList.size();
    m_mutex.unlock();

    while (0 < count) {
        m_mutex.lock();
        ii = mImageList.front();
        mImageList.pop_front();
        m_mutex.unlock();

        WriteOne(ii);
        delete[] ii.buf;
        ii.buf = nullptr;

        m_mutex.lock();
        count = (int)mImageList.size();
        m_mutex.unlock();
    }
}

void
MLAviWriter::WriteOne(ImageItem& ii)
{
    if ((mTotalFrames % 30) == 29) {
        RestartRiff();
    }

    WriteStreamDataHeader(FCC_00db, ii.bytes);
    fwrite(ii.buf, 1, ii.bytes, mFp);

    ++mTotalFrames;
}

