#pragma once

#include <string>
#include <stdint.h>
#include <vector>
#include <stdio.h>
#include "Common.h"
#include <list>
#include <mutex>

class MLAviWriter {
public:
    MLAviWriter(void);
    ~MLAviWriter(void);

    enum ImageFormat {
        IF_YUV422v210,
    };

    bool Start(std::wstring path, int width, int height, int fps, ImageFormat imgFmt);

    void AddImage(const uint32_t * img, int bytes);

    // send thread to flush remaining data and end.
    // StopAsync() then call PallThreadEnd() every frame until PollThreadEnd() returns true
    void StopAsync(void);

    // returns true when writing thread ends
    bool PollThreadEnd(void);

    // blocking termination
    void StopBlocking(void);

    bool IsOpen(void) const { return mFp != nullptr; }

    int RecQueueSize(void);

private:

    int mWidth;
    int mHeight;
    int mFps;
    int mImgFmt;

    enum AVIState {
        AVIS_Init,
        AVIS_Writing,
        AVIS_WaitThreadEnd,
    };

    AVIState mState;

    struct RiffChunk {
        uint64_t pos;
        uint64_t size;
        bool bDone;
    };

    struct ListChunk {
        uint64_t pos;
        uint64_t size;
        bool bDone;
    };

    std::vector<RiffChunk> mRiffChunks;
    std::vector<ListChunk> mListChunks;

    struct ImageItem {
        uint32_t *buf;
        int bytes;
    };

    std::list<ImageItem> mImageList;

    int mLastRiffIdx;
    int mLastMoviIdx;

    int mTotalFrames;

    uint64_t mAviMainHeaderPos;
    uint64_t mAviStreamHeaderPos;

    FILE *mFp;

    std::mutex m_mutex;
    HANDLE       m_thread;
    HANDLE       m_shutdownEvent;
    HANDLE       m_readyEvent;

    int WriteRiff(const char *fccS);
    void FinishRiff(int idx);

    int WriteList(const char *fccS);
    void FinishList(int idx);

    void WriteAviMainHeader(void);
    void WriteAviStreamHeader(void);
    void WriteBitmapInfoHeader(void);

    void WriteFccHeader(const char *fcc, int bytes);

    void WriteStreamDataHeader(uint32_t fcc, int bytes);

    uint32_t ImageBytes(void) const;

    void RestartRiff(void);

    static DWORD WINAPI AviWriterEntry(LPVOID param);
    DWORD ThreadMain(void);
    void WriteAll(void);
    void WriteOne(ImageItem& ii);
    void StartThread(void);
    void StopThreadBlock(void);

    void StopThreadAsync(void);

};

