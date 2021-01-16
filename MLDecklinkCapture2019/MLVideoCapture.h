#pragma once

#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <comutil.h>

#include "DeckLinkAPI_h.h"
#include <vector>

#include "IMLVideoCaptureCallback.h"
#include "MLVideoCaptureVideoFormat.h"

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

    const MLVideoCaptureVideoFormat & CurrentVideoFormat(void) const { return m_vFmt; }

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

    MLVideoCaptureVideoFormat m_vFmt;
    BMDDetectedVideoInputFormatFlags m_detectedVIFF = 0;

    int mAudioSampleRate = 48000;
    int mAudioNumChannels = 2;
    int mAudioBitsPerSample = 16;
};

