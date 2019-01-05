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
    MLVideoCapture(void);
    ~MLVideoCapture(void);

    void SetCallback(IMLVideoCaptureCallback *cb) {
        m_callback = cb;
    }

    bool Init(IDeckLink *device);
    void Term(void);
    bool StartCapture(int videoModeIdx);
    void StopCapture(void);

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
    State m_state;
    ULONG   m_refCount;
    IDeckLink*  m_deckLink;
    IDeckLinkInput* m_deckLinkInput;
    int m_videoModeIdx;
    std::vector<IDeckLinkDisplayMode*>	m_modeList;
    int m_frameCounter;

    IMLVideoCaptureCallback *m_callback;
};

