#pragma once

#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <comutil.h>

#include "DeckLinkAPI_h.h"
#include <vector>

#include "IMLVideoCaptureCallback.h"

class MLVideoCapture : public IDeckLinkInputCallback
{
public:
    MLVideoCapture(void) {
        m_vFmt = {};
    }
    ~MLVideoCapture(void);

    void SetCallback(IMLVideoCaptureCallback *cb) {
        m_callback = cb;
    }

    bool Init(IDeckLink *device);

    int NumOfDisplayModes(void) const { return (int)m_modeList.size(); }
    IDeckLinkDisplayMode* DisplayMode(int idx) { return m_modeList[idx]; }

    void Term(void);
    HRESULT StartCapture(int videoModeIdx);

    void StopCapture(void);

    HRESULT FlushStreams(void);

    struct VideoFormat {
        int width;
        int height;

        /// <summary>
        /// NTSC, HD1080p5994, ...
        /// </summary>
        BMDDisplayMode displayMode;

        /// <summary>
        /// bmdFormat8BitARGB, bmdFormat10BitRGB, bmdFormat12BitRGB
        /// </summary>
        BMDPixelFormat pixelFormat;

        /// <summary>
        /// frameRateTS / frameRateTVがframes per second。
        /// </summary>
        BMDTimeValue frameRateTV;
        BMDTimeScale frameRateTS;

        /// <summary>
        /// bmdDisplayModeColorspaceRec709 等。
        /// しかし他のフィールドを見るほうが良い。
        /// </summary>
        BMDDisplayModeFlags flags;

        /// <summary>
        /// bmdProgressiveFrame, LowerFieldFirst等。
        /// </summary>
        BMDFieldDominance fieldDominance;

        /// <summary>
        /// SDR, PQ, HLG
        /// </summary>
        BMDDynamicRange dynamicRange;

        /// <summary>
        /// Rec.601, Rec.709, Rec.2020
        /// </summary>
        BMDColorspace colorSpace;

        /*
        // HDRのメタデータ。
        bool hdrMetaExists;
        int eotf;
        float primaryRedX;
        float primaryRedY;
        float primaryGreenX;
        float primaryGreenY;
        float primaryBlueX;
        float primaryBlueY;
        float whitePointX;
        float whitePointY;
        float maxLuma;
        float minLuma;
        float maxContentLightLevel;
        float maxFrameAvgLightLevel;
        */
    };
    const VideoFormat & CurrentVideoFormat(void) const { return m_vFmt; }

    BMDDetectedVideoInputFormatFlags DetectedVideoInputFormatFlags(void) const {
        return m_detectedVIFF;
    }

    // IUnknown interface
    virtual HRESULT STDMETHODCALLTYPE   QueryInterface(REFIID iid, LPVOID *ppv);
    virtual ULONG   STDMETHODCALLTYPE   AddRef();
    virtual ULONG   STDMETHODCALLTYPE   Release();

    // IDeckLinkInputCallback interface
    virtual HRESULT STDMETHODCALLTYPE   VideoInputFormatChanged(/* in */ BMDVideoInputFormatChangedEvents notificationEvents, /* in */ IDeckLinkDisplayMode *newDisplayMode, /* in */ BMDDetectedVideoInputFormatFlags detectedSignalFlags);
    virtual HRESULT STDMETHODCALLTYPE   VideoInputFrameArrived(/* in */ IDeckLinkVideoInputFrame* videoFrame, /* in */ IDeckLinkAudioInputPacket* audioPacket);

private:
    enum State {
        S_Init,
        S_Capturing,
    };
    State m_state = S_Init;
    ULONG   m_refCount = 1;
    IDeckLink*  m_deckLink = nullptr;
    IDeckLinkHDMIInputEDID* m_edid = nullptr;
    IDeckLinkInput* m_deckLinkInput = nullptr;
    int m_videoModeIdx = 0;
    std::vector<IDeckLinkDisplayMode*>	m_modeList;
    int m_frameCounter = 0;

    IMLVideoCaptureCallback *m_callback = nullptr;

    VideoFormat m_vFmt;
    BMDDetectedVideoInputFormatFlags m_detectedVIFF = 0;

    int mAudioSampleRate = 48000;
    int mAudioNumChannels = 2;
    int mAudioBitsPerSample = 16;
};

