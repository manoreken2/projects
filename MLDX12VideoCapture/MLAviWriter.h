#pragma once

#include <string>
#include <stdint.h>
#include <vector>
#include <stdio.h>
#include "Common.h"
#include <list>
#include <mutex>
#include "MLAVICommon.h"

class MLAviWriter {
public:
    MLAviWriter(void);
    ~MLAviWriter(void);

    bool Start(std::wstring path, int width, int height, int fps, MLAviImageFormat imgFmt,
            bool bAudio);

    void AddImage(const uint8_t * img, int bytes);

    void AddAudio(const uint8_t * buff, int frames);

    // send thread to flush remaining data and end.
    // StopAsync() then call PallThreadEnd() every frame until PollThreadEnd() returns true
    void StopAsync(void);

    // returns true when writing thread ends
    bool PollThreadEnd(void);

    // blocking termination
    void StopBlocking(void);

    bool IsOpen(void) const { return mFp != nullptr; }

    int RecQueueSize(void);

    int FramesPerSec(void) const { return mFps; }

    int TotalVideoFrames(void) const { return mTotalVideoFrames; }

private:

    int mWidth;
    int mHeight;
    int mFps;
    int mImgFmt;
    bool mAudio;

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

    struct IncomingItem {
        uint8_t *buf;
        int bytes;
        bool bAudio;
    };

    std::list<IncomingItem> mAudioVideoList;

    int mLastRiffIdx;
    int mLastMoviIdx;

    int mTotalVideoFrames;
    int mTotalAudioSamples;

    uint64_t mAviMainHeaderPos;
    uint64_t mAviStreamHeaderVideoPos;
    uint64_t mAviStreamHeaderAudioPos;

    FILE *mFp;

    std::mutex m_mutex;
    HANDLE       m_thread;
    HANDLE       m_shutdownEvent;
    HANDLE       m_readyEvent;

    int mAudioSampleRate;
    int mAudioBitsPerSample;
    int mAudioNumChannels;

    int WriteRiff(const char *fccS);
    void FinishRiff(int idx);

    int WriteList(const char *fccS);
    void FinishList(int idx);

    void WriteAviMainHeader(int nStreams);
    void WriteAviStreamHeaderVideo(void);
    void WriteAviStreamHeaderAudio(void);
    void WriteBitmapInfoHeader(void);
    void WriteWaveFormatExHeader(void);

    void WriteFccHeader(const char *fcc, int bytes);

    void WriteStreamDataHeader(uint32_t fcc, int bytes);

    uint32_t ImageBytes(void) const;

    void RestartRiff(void);

    static DWORD WINAPI AviWriterEntry(LPVOID param);
    DWORD ThreadMain(void);
    void WriteAll(void);
    void WriteOne(IncomingItem& ii);
    void StartThread(void);
    void StopThreadBlock(void);

    void StopThreadAsync(void);

};

