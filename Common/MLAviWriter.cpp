#include "MLAviWriter.h"
#include <assert.h>
#include "MLWinApp.h"
#include "MLConverter.h"

MLAviWriter::MLAviWriter(void)
        : mLastRiffIdx(-1), mLastMoviIdx(-1), mTotalVideoFrames(0), mTotalAudioSamples(0),
          mAviMainHeaderPos(0), mAviStreamHeaderVideoPos(0),mAviStreamHeaderAudioPos(0),
          mFp(nullptr)
{
    mState = AVIS_Init;
    m_thread = nullptr;
    m_shutdownEvent = CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
    m_readyEvent    = CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);

    mAudioSampleRate = 48000;
    mAudioBitsPerSample = 16;
    mAudioNumChannels = 2;
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

void
MLAviWriter::WriteFccHeader(const char *fccS, int bytes)
{
    uint32_t fcc = MLStringToFourCC(fccS);
    fwrite(&fcc, 1, 4, mFp);
    fwrite(&bytes, 1, 4, mFp);
}

bool
MLAviWriter::Start(std::wstring path, int width, int height, double fps, MLAviImageFormat imgFmt, bool bAudio)
{
    assert(mFp == nullptr);

    mWidth = width;
    mHeight = height;
    mFps = fps;
    mInputImgFmt = imgFmt;
    mAudio = bAudio;

    switch (mInputImgFmt) {
    case MLIF_YUV422_v210:
    case MLIF_RGB10bit_r210:
        // そのままAVIファイルに保存する。
        mWriteImgFmt = mInputImgFmt;
        break;
    case MLIF_RGB12bit_R12B:
        // r210に変換して保存。
        mWriteImgFmt = MLIF_RGB10bit_r210;
        break;
    default:
        // ここには来ない。
        assert(0);
        break;
    }

    errno_t ercd = _wfopen_s(&mFp, path.c_str(), L"wb");
    if (ercd != 0) {
        printf("Error: MLAviWriter::Init() fopen failed\n");
        mFp = nullptr;
        return false;
    }

    // ビデオのみの場合。
    /* RIFF "AVI "
         LIST "hdrl"
           AviMainHeader 1stream
           LIST "strl"
             AviStreamHeader vids
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

    // ビデオ+オーディオの場合。
    /* RIFF "AVI "
         LIST "hdrl"
           AviMainHeader 2streams
           LIST "strl"
             AviStreamHeader vids
             "strf" BitmapInfoHeader
           LIST "strl"
             AviStreamHeader auds
             "strf" WaveFormatEx
         LIST "movi"
           00db
           00db
           ...
           01wb
           ...
       RIFF "AVIX"
         LIST "movi"
           00db
           00db
           ...
    */

    int hAvi = WriteRiff("AVI");
    int hHdrl = WriteList("hdrl");

    WriteAviMainHeader(1 + (int)mAudio);

    {
        // Video
        int hStrlVideo = WriteList("strl");
        WriteAviStreamHeaderVideo();

        WriteFccHeader("strf", 40);
        WriteBitmapInfoHeader();
        FinishList(hStrlVideo);
    }
    if (mAudio) {
        int hStrlAudio = WriteList("strl");
        WriteAviStreamHeaderAudio();

        WriteFccHeader("strf", 40);
        WriteWaveFormatExHeader();
        FinishList(hStrlAudio);
    }

    FinishList(hHdrl);
    int movi = WriteList("movi");

    mLastRiffIdx = hAvi;
    mLastMoviIdx = movi;

    mTotalVideoFrames = 0;

    StartThread();
    return true;
}

uint8_t*
MLAviWriter::ConvCapturedImg(const uint8_t* imgFrom, int fromBytes, int * toBytes_return)
{
    uint8_t* imgTo = nullptr;
    switch (mInputImgFmt) {
    case MLIF_YUV422_v210:
    case MLIF_RGB10bit_r210:
        {
            // そのままコピーします。
            const int toBytes = fromBytes;
            *toBytes_return = toBytes;
            imgTo = new uint8_t[toBytes];
            memcpy(imgTo, imgFrom, fromBytes);
        }
        break;
    case MLIF_RGB12bit_R12B:
        {
            // r210に変換する。
            int toBytes = mWidth * mHeight * 4;
            *toBytes_return = toBytes;
            imgTo = new uint8_t[toBytes];
            MLConverter::Rgb12bitToR210((uint32_t*)imgFrom, (uint32_t*)imgTo, mWidth, mHeight);
        }
        break;
    default:
        assert(0);
    }

    return imgTo;
}

void
MLAviWriter::AddImage(const uint8_t * img, int bytes)
{
    if (mState != AVIS_Writing) {
        return;
    }

    assert(mFp != nullptr);
    assert(m_readyEvent != nullptr);

    IncomingItem ii;

    int writeBytes;
    ii.buf = ConvCapturedImg(img, bytes, &writeBytes);

    ii.bytes = writeBytes;
    ii.bAudio = false;

    m_mutex.lock();
    mAudioVideoList.push_back(ii);
    m_mutex.unlock();

    SetEvent(m_readyEvent);
}

void
MLAviWriter::AddAudio(const uint8_t *buff, int frames)
{
    if (mState != AVIS_Writing) {
        return;
    }

    assert(mFp != nullptr);
    assert(m_readyEvent != nullptr);

    if (!mAudio) {
        return;
    }

    int bytes = frames * mAudioBitsPerSample * mAudioNumChannels / 8;

    IncomingItem ii;
    ii.buf = new uint8_t[bytes];
    ii.bytes = bytes;
    ii.bAudio = true;
    memcpy(ii.buf, buff, bytes);

    m_mutex.lock();
    mAudioVideoList.push_back(ii);
    m_mutex.unlock();

    SetEvent(m_readyEvent);
}

void
MLAviWriter::StopBlocking(void)
{
    if (mFp == nullptr) {
        return;
    }

    StopThreadBlock();
}

void
MLAviWriter::StopAsync(void)
{
    StopThreadAsync();
}

int
MLAviWriter::RecQueueSize(void)
{
    int count;
    m_mutex.lock();
    count = (int)mAudioVideoList.size();
    m_mutex.unlock();
    return count;

}

int
MLAviWriter::WriteRiff(const char *fccS)
{
    uint32_t fcc = MLStringToFourCC(fccS);

    int riffIdx = (int)mRiffChunks.size();

    RiffChunk rc;
    rc.pos = _ftelli64(mFp);
    rc.size = 0;
    rc.bDone = false;
    mRiffChunks.push_back(rc);

    MLRiffHeader rh;
    rh.riff = MLStringToFourCC("RIFF");
    rh.bytes = 0;
    rh.type = fcc;
    fwrite(&rh, 1, sizeof rh, mFp);

    return riffIdx;
}

void
MLAviWriter::FinishRiff(int idx)
{
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

int
MLAviWriter::WriteList(const char * fccS)
{
    uint32_t fcc = MLStringToFourCC(fccS);

    int listIdx = (int)mListChunks.size();

    ListChunk lc;
    lc.pos = _ftelli64(mFp);
    lc.size = 0;
    lc.bDone = false;
    mListChunks.push_back(lc);

    MLListHeader lh;
    lh.LIST = MLStringToFourCC("LIST");
    lh.bytes = 0;
    lh.type = fcc;
    fwrite(&lh, 1, sizeof lh, mFp);

    return listIdx;
}

void
MLAviWriter::FinishList(int idx)
{
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

uint32_t
MLAviWriter::ImageBytes(void) const
{
    switch (mWriteImgFmt) {
    case MLIF_YUV422_v210:
        return mWidth * mHeight * 8 / 3;
    case MLIF_RGB10bit_r210:
        return mWidth * mHeight * 4;
    //case MLIF_RGB12bit_R12B:
    //    return ((mWidth * 36) / 8) * mHeight;
    default:
        assert(0);
        return 0;
    }
}

void
MLAviWriter::WriteAviMainHeader(int nStreams)
{
    mAviMainHeaderPos = _ftelli64(mFp);

    MLAviMainHeader mh;
    mh.fcc = MLStringToFourCC("avih");
    mh.cb = 0x38;
    mh.dwMicroSecPerFrame = (uint32_t)((1.0 / mFps) * 1000 * 1000);
    mh.dwMaxBytesPersec = 0;
    mh.dwPaddingGranularity = 0x1000;
    mh.dwFlags = 0;
    mh.dwTotalFrames = mTotalVideoFrames;
    mh.dwInitialFrames = 0;
    mh.dwStreams = nStreams;
    mh.dwSuggestedBufferSize = ImageBytes();
    mh.dwWidth = mWidth;
    mh.dwHeight = mHeight;
    mh.dwReserved0 = 0;
    mh.dwReserved1 = 0;
    mh.dwReserved2 = 0;
    mh.dwReserved3 = 0;

    fwrite(&mh, 1, sizeof mh, mFp);
}

uint32_t
MLAviImageFormatToFourcc(MLAviImageFormat t)
{
    switch (t) {
    case MLIF_YUV422_UYVY:
        return MLStringToFourCC("UYVY");
    case MLIF_YUV422_v210:
        return MLStringToFourCC("v210");
    case MLIF_RGB10bit_r210:
        return MLStringToFourCC("r210");
    case MLIF_RGB12bit_R12B:
        return MLStringToFourCC("R12B");
    default:
        assert(0);
        return 0;
    }
}

int
MLAviImageFormatToBitsPerPixel(MLAviImageFormat t)
{
    switch (t) {
    case MLIF_YUV422_UYVY:
        return 16;
    case MLIF_YUV422_v210:
        return 20;
    case MLIF_RGB10bit_r210:
        return 30;
    case MLIF_RGB12bit_R12B:
        return 36;
    default:
        assert(0);
        return 0;
    }
}

void
MLAviWriter::WriteAviStreamHeaderVideo(void)
{
    mAviStreamHeaderVideoPos = _ftelli64(mFp);

    MLAviStreamHeader sh;
    sh.fcc = MLStringToFourCC("strh");
    sh.cb = 0x38;
    sh.fccType = MLStringToFourCC("vids");
    sh.fccHandler = MLAviImageFormatToFourcc(mWriteImgFmt);
    sh.dwFlags = 0;
    sh.wPriority = 0;
    sh.wLanguage = 0;
    sh.dwInitialFrames = 0;
    sh.dwScale = 1;
    sh.dwRate = (uint32_t)mFps;
    sh.dwStart = 0;
    sh.dwLength = mTotalVideoFrames;
    sh.dwSuggestedBufferSize = ImageBytes();
    sh.dwQuality = 9800;
    sh.dwSampleSize = sh.dwSuggestedBufferSize;
    sh.left = 0;
    sh.top = 0;
    sh.right = 0;
    sh.bottom = 0;

    fwrite(&sh, 1, sizeof sh, mFp);
}

void
MLAviWriter::WriteAviStreamHeaderAudio(void) {
    mAviStreamHeaderAudioPos = _ftelli64(mFp);

    MLAviStreamHeader sh;
    sh.fcc = MLStringToFourCC("strh");
    sh.cb = 0x38;
    sh.fccType = MLStringToFourCC("auds");
    sh.fccHandler = 0;
    sh.dwFlags = 0;
    sh.wPriority = 0;
    sh.wLanguage = 0;
    sh.dwInitialFrames = 0;
    sh.dwScale = 1;
    sh.dwRate = mAudioSampleRate;
    sh.dwStart = 0;
    sh.dwLength = mTotalAudioSamples;
    sh.dwSuggestedBufferSize = mAudioSampleRate * mAudioBitsPerSample * mAudioNumChannels / 8;
    sh.dwQuality = 0;
    sh.dwSampleSize = mAudioBitsPerSample * mAudioNumChannels / 8;
    sh.left = 0;
    sh.top = 0;
    sh.right = 0;
    sh.bottom = 0;

    fwrite(&sh, 1, sizeof sh, mFp);
}

void
MLAviWriter::WriteBitmapInfoHeader(void)
{
    MLBitmapInfoHeader ih;
    ih.biSize = 0x28;
    ih.biWidth = mWidth;
    ih.biHeight = mHeight;
    ih.biPlanes = 1;
    ih.biBitCount = MLAviImageFormatToBitsPerPixel(mWriteImgFmt);
    ih.biCompression = MLAviImageFormatToFourcc(mWriteImgFmt);
    ih.biSizeImage = ImageBytes();
    ih.biXPelsPerMeter = 0;
    ih.biYPelsPerMeter = 0;
    ih.biClrUsed = 0;
    ih.biClrImportant = 0;

    fwrite(&ih, 1, sizeof ih, mFp);
}

void
MLAviWriter::WriteWaveFormatExHeader(void) {
    MLWaveFormatExtensible wh;
    wh.wFormatTag = 0xfffe;
    wh.nChannels = 2;
    wh.nSamplesPerSec = mAudioSampleRate;
    wh.nAvgBytesPerSec = mAudioSampleRate * mAudioNumChannels * mAudioBitsPerSample / 8;
    wh.nBlockAlign = mAudioBitsPerSample * mAudioNumChannels / 8;
    wh.wBitsPerSample = mAudioBitsPerSample;
    wh.cbSize = 22;
    wh.wValidBitsPerSample = mAudioBitsPerSample;
    wh.dwChannelMask = 0;
    wh.subFormatGuid[0] = 0x00000001;
    wh.subFormatGuid[1] = 0x00100000;
    wh.subFormatGuid[2] = 0xaa000080;
    wh.subFormatGuid[3] = 0x719b3800;

    fwrite(&wh, 1, sizeof wh, mFp);
}

void
MLAviWriter::WriteStreamDataHeader(uint32_t fcc, int bytes)
{
    MLStreamDataHeader sd;
    sd.fcc = fcc;
    sd.bytes = bytes;

    fwrite(&sd, 1, sizeof sd, mFp);
}

void
MLAviWriter::RestartRiff(void)
{
    FinishList(mLastMoviIdx);
    FinishRiff(mLastRiffIdx);

    mLastRiffIdx = WriteRiff("AVIX");
    mLastMoviIdx = WriteList("movi");
}

// ============================================================
// thread

void
MLAviWriter::StartThread(void)
{
    if (m_thread != nullptr) {
        return;
    }

    mState = AVIS_Writing;
    m_thread = CreateThread(nullptr, 0, AviWriterEntry, this, 0, nullptr);
}

void
MLAviWriter::StopThreadBlock(void)
{
    if (m_thread == nullptr) {
        return;
    }

    assert(m_shutdownEvent != nullptr);
    SetEvent(m_shutdownEvent);
    WaitForSingleObject(m_thread, INFINITE);
    CloseHandle(m_thread);
    m_thread = nullptr;
    mState = AVIS_Init;
}

void
MLAviWriter::StopThreadAsync(void)
{
    if (m_thread == nullptr) {
        return;
    }

    mState = AVIS_WaitThreadEnd;

    assert(m_shutdownEvent != nullptr);
    SetEvent(m_shutdownEvent);
}

bool
MLAviWriter::PollThreadEnd(void)
{
    if (m_thread == nullptr) {
        return true;
    }

    if (mState == AVIS_Init) {
        CloseHandle(m_thread);
        m_thread = nullptr;
        return true;
    }

    return false;
}

DWORD WINAPI
MLAviWriter::AviWriterEntry(LPVOID param)
{
    MLAviWriter *self = (MLAviWriter *)param;

    return self->ThreadMain();
}

DWORD
MLAviWriter::ThreadMain(void)
{
    DWORD rv = 0;
    
    bool coInitialized = false;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr)) {
        coInitialized = true;
    }

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

    FinishList(mLastMoviIdx);
    FinishRiff(mLastRiffIdx);

    // mTotalVideoFrames, mTotalAudioSamplesが決まったので書き込む。

    _fseeki64(mFp, mAviMainHeaderPos, SEEK_SET);
    WriteAviMainHeader(1 + (int)mAudio);

    _fseeki64(mFp, mAviStreamHeaderVideoPos, SEEK_SET);
    WriteAviStreamHeaderVideo();

    if (mAudio) {
        _fseeki64(mFp, mAviStreamHeaderAudioPos, SEEK_SET);
        WriteAviStreamHeaderAudio();
    }

    fclose(mFp);
    mFp = nullptr;

    if (coInitialized) {
        CoUninitialize();
    }

    mState = AVIS_Init;
    return rv;
}

void
MLAviWriter::WriteAll(void)
{
    IncomingItem ii;

    m_mutex.lock();
    int count = (int)mAudioVideoList.size();
    m_mutex.unlock();

    while (0 < count) {
        m_mutex.lock();
        ii = mAudioVideoList.front();
        mAudioVideoList.pop_front();
        m_mutex.unlock();

        WriteOne(ii);
        delete[] ii.buf;
        ii.buf = nullptr;

        m_mutex.lock();
        count = (int)mAudioVideoList.size();
        m_mutex.unlock();
    }
}

void
MLAviWriter::WriteOne(IncomingItem& ii)
{
    if (ii.bAudio) {
        WriteStreamDataHeader(MLFOURCC_01wb, ii.bytes);
        fwrite(ii.buf, 1, ii.bytes, mFp);

        int nSamples = ii.bytes / (mAudioBitsPerSample * mAudioNumChannels / 8);
        mTotalAudioSamples += nSamples;
        return;
    }

    // video

    if ((mTotalVideoFrames % 30) == 29) {
        RestartRiff();
    }

    // compressed video == 00dc
    // uncompressed video == 00db
    WriteStreamDataHeader(MLFOURCC_00db, ii.bytes);
    fwrite(ii.buf, 1, ii.bytes, mFp);

    ++mTotalVideoFrames;
}

