#include "MLVideoCapture.h"
#include "MLVideoCaptureEnumToStr.h"
#include <SDKDDKVer.h>
#include <assert.h>

MLVideoCapture::~MLVideoCapture(void)
{
    Term();
}

void
MLVideoCapture::Term(void)
{
    m_callback = nullptr;
    if (m_state == S_Capturing) {
        StopCapture();
    }

    while (!m_modeList.empty()) {
        m_modeList.back()->Release();
        m_modeList.pop_back();
    }

    if (m_edid != nullptr) {
        m_edid->Release();
        m_edid = nullptr;
    }

    if (m_deckLinkInput != nullptr) {
        m_deckLinkInput->Release();
        m_deckLinkInput = nullptr;
    }

    if (m_deckLink != nullptr) {
        m_deckLink->Release();
        m_deckLink = nullptr;
    }
}

HRESULT	STDMETHODCALLTYPE
MLVideoCapture::QueryInterface(REFIID iid, LPVOID *ppv)
{
    HRESULT result = E_NOINTERFACE;

    if (ppv == nullptr) {
        return E_INVALIDARG;
    }
    *ppv = nullptr;

    if (iid == IID_IUnknown) {
        *ppv = this;
        AddRef();
        result = S_OK;
    } else if (iid == IID_IDeckLinkInputCallback) {
        *ppv = (IDeckLinkInputCallback*)this;
        AddRef();
        result = S_OK;
    } else if (iid == IID_IDeckLinkNotificationCallback) {
        *ppv = (IDeckLinkNotificationCallback*)this;
        AddRef();
        result = S_OK;
    }

    return result;
}

ULONG STDMETHODCALLTYPE
MLVideoCapture::AddRef(void)
{
    return InterlockedIncrement((LONG*)&m_refCount);
}

ULONG STDMETHODCALLTYPE
MLVideoCapture::Release(void)
{
    int newRefValue;

    newRefValue = InterlockedDecrement((LONG*)&m_refCount);
    if (newRefValue == 0) {
        delete this;
        return 0;
    }

    return newRefValue;
}

bool
MLVideoCapture::Init(IDeckLink *device)
{
    m_deckLink = device;
    m_deckLink->AddRef();

    IDeckLinkAttributes_v10_11*     deckLinkAttributes = nullptr;
    IDeckLinkDisplayModeIterator*   displayModeIterator = nullptr;
    IDeckLinkDisplayMode*           displayMode = nullptr;
    BSTR                            deviceNameBSTR = nullptr;

    if (m_deckLink->QueryInterface(IID_PPV_ARGS(&m_deckLinkInput)) != S_OK) {
        MessageBox(nullptr, L"This device does not have input", L"Error", 0);
        return false;
    }

    if (m_deckLink->QueryInterface(IID_PPV_ARGS(&m_edid)) != S_OK) {
        MessageBox(nullptr, L"This device does not support EDID", L"Error", 0);
        return false;
    }

    while (!m_modeList.empty()) {
        m_modeList.back()->Release();
        m_modeList.pop_back();
    }

    if (m_deckLinkInput->GetDisplayModeIterator(&displayModeIterator) == S_OK) {
        while (displayModeIterator->Next(&displayMode) == S_OK) {
            m_modeList.push_back(displayMode);
        }
        displayModeIterator->Release();
    }

    return true;
}

HRESULT
MLVideoCapture::StartCapture(int videoModeIdx)
{
    HRESULT hr = S_OK;

    m_frameCounter = 0;

    m_videoModeIdx = videoModeIdx;
    BMDVideoInputFlags videoInputFlags = bmdVideoInputFlagDefault;

    videoInputFlags |= bmdVideoInputEnableFormatDetection;

    // Set capture callback
    m_deckLinkInput->SetCallback(this);

    // Set the video input mode
    hr = m_deckLinkInput->EnableVideoInput(m_modeList[m_videoModeIdx]->GetDisplayMode(), bmdFormat8BitYUV, videoInputFlags);
    if (FAILED(hr)) {
        MessageBox(nullptr,
            L"This application was unable to select the chosen video mode. Perhaps, the selected device is currently in-use.",
            L"Error starting the capture", 0);
        return hr;
    }

    hr = m_deckLinkInput->EnableAudioInput(bmdAudioSampleRate48kHz, bmdAudioSampleType16bitInteger, 2);
    if (FAILED(hr)) {
        MessageBox(nullptr,
            L"This application was unable to select 48kHz 16bit. Perhaps, the selected device is currently in-use.",
            L"Error starting the capture", 0);
        return hr;
    }

    hr = m_deckLinkInput->StartStreams();
    if (FAILED(hr)) {
        MessageBox(nullptr,
            L"This application was unable to start the capture. Perhaps, the selected device is currently in-use.",
            L"Error starting the capture", 0);
        return hr;
    }

    m_state = S_Capturing;

    return S_OK;
}

void
MLVideoCapture::StopCapture(void)
{
    if (m_deckLinkInput != nullptr) {
        m_deckLinkInput->StopStreams();
        m_deckLinkInput->SetCallback(nullptr);
    }

    m_state = S_Init;
}

HRESULT
MLVideoCapture::FlushStreams(void)
{
    if (m_deckLinkInput != nullptr) {
        return m_deckLinkInput->FlushStreams();
    }

    return E_NOT_VALID_STATE;
}

HRESULT
MLVideoCapture::VideoInputFormatChanged(
        /* in */ BMDVideoInputFormatChangedEvents notificationEvents,
        /* in */ IDeckLinkDisplayMode *newMode, 
        /* in */ BMDDetectedVideoInputFormatFlags detectedSignalFlags)
{
    HRESULT hr = S_OK;
    newMode->GetFrameRate(&m_vFmt.frameRateTV, &m_vFmt.frameRateTS);
    int width = newMode->GetWidth();
    int height = newMode->GetHeight();
    BMDDisplayMode dm = newMode->GetDisplayMode();
    BMDFieldDominance fd = newMode->GetFieldDominance();
    
    BMDTimeValue frameRateTV;
    BMDTimeScale frameRateTS;
    newMode->GetFrameRate(&frameRateTV, &frameRateTS);

    BMDDisplayModeFlags dmFlags = newMode->GetFlags();
    BMDDynamicRange dr = bmdDynamicRangeSDR;
    if (m_edid) {
        LONGLONG v;
        hr = m_edid->GetInt(bmdDeckLinkHDMIInputEDIDDynamicRange, &v);
        if (SUCCEEDED(hr)) {
            dr = (BMDDynamicRange)v;
        }
    }

    m_detectedVIFF = detectedSignalFlags;

    char s[256];
    sprintf_s(s, "VideoInputFormatChanged %dx%d %s %s %f fps %s(?) %s %s\n",
        width, height,
        BMDDisplayModeToStr(dm),
        BMDFieldDominanceToStr(fd),
        (double)frameRateTS / frameRateTV,
        BMDDisplayModeFlagsToStr(dmFlags).c_str(),
        BMDDetectedVideoInputFormatFlagsToStr(detectedSignalFlags).c_str(),
        BMDDynamicRangeToStr(dr));
    OutputDebugStringA(s);

    // pixelFormatを決定する。
    BMDPixelFormat	pixelFormat = bmdFormat8BitYUV;
    if (detectedSignalFlags & bmdDetectedVideoInputYCbCr422) {
        // YUV。
        pixelFormat = bmdFormat8BitYUV;
        if (detectedSignalFlags & bmdDetectedVideoInput10BitDepth) {
            pixelFormat = bmdFormat10BitYUV;
        } else if (detectedSignalFlags & bmdDetectedVideoInput12BitDepth) {
            pixelFormat = bmdFormat10BitYUV;
        }
    } else {
        // RGBの8ビット、10ビット、または12ビットにしたい。
        pixelFormat = bmdFormat8BitARGB;
        if (detectedSignalFlags & bmdDetectedVideoInput10BitDepth) {
            pixelFormat = bmdFormat10BitRGB;
        } else if (detectedSignalFlags & bmdDetectedVideoInput12BitDepth) {
            pixelFormat = bmdFormat12BitRGB;
        }
    }

    m_vFmt.width = width;
    m_vFmt.height = height;
    m_vFmt.displayMode = dm;
    m_vFmt.pixelFormat = pixelFormat;
    m_vFmt.frameRateTS = frameRateTS;
    m_vFmt.frameRateTV = frameRateTV;
    m_vFmt.flags = dmFlags;
    m_vFmt.fieldDominance = fd;
    m_vFmt.dynamicRange = dr;
    
    m_deckLinkInput->StopStreams();

    if (m_deckLinkInput->EnableVideoInput(newMode->GetDisplayMode(), pixelFormat, bmdVideoInputEnableFormatDetection) != S_OK) {
        MessageBox(nullptr, L"Error restarting capture 1", L"Error", 0);
        goto bail;
    }

    if (m_deckLinkInput->StartStreams() != S_OK) {
        MessageBox(nullptr, L"Error restarting capture 2", L"Error", 0);
        goto bail;
    }

    for (int i=0; i<m_modeList.size(); ++i) {
        if (m_modeList[i]->GetDisplayMode() == newMode->GetDisplayMode()) {
            m_videoModeIdx = i;
            break;
        }
    }

    if (m_callback) {
        m_callback->MLVideoCaptureCallback_VideoInputFormatChanged(m_vFmt);
    }

bail:
    return S_OK;
}

HRESULT
MLVideoCapture::VideoInputFrameArrived(/* in */ IDeckLinkVideoInputFrame* videoFrame, /* in */ IDeckLinkAudioInputPacket* audioPacket)
{
    HRESULT hr = S_OK;

    if (videoFrame == nullptr && audioPacket == nullptr) {
        return S_OK;
    }

#if 0
    int width = videoFrame->GetWidth();
    int height = videoFrame->GetHeight();
    char s[256];
    sprintf_s(s, "VideoInputFrameArrived %d %dx%d\n", m_frameCounter, width, height);
    OutputDebugStringA(s);
#endif
    
#if 0
    int audioSamples = 0;
    if (audioPacket) {
        audioSamples = audioPacket->GetSampleFrameCount();
    }
    char s[256];
    sprintf_s(s, "%d ", audioSamples);
    OutputDebugStringA(s);

#endif

    ++m_frameCounter;

    if (videoFrame != nullptr && (videoFrame->GetFlags() & bmdFrameContainsHDRMetadata)) {
        // HDRのメタデータが来た。
        // 内容を取得する。

        IDeckLinkVideoFrameMetadataExtensions * vfMeta = nullptr;
        videoFrame->QueryInterface(IID_PPV_ARGS(&vfMeta));

        BMDDynamicRange dr = bmdDynamicRangeSDR;
        if (m_edid) {
            LONGLONG v;
            hr = m_edid->GetInt(bmdDeckLinkHDMIInputEDIDDynamicRange, &v);
            if (SUCCEEDED(hr)) {
                dr = (BMDDynamicRange)v;
            }
        }
        m_vFmt.dynamicRange = dr;

        if (vfMeta) {
            LONGLONG v = 0;
            hr = vfMeta->GetInt(bmdDeckLinkFrameMetadataColorspace, &v);
            if (SUCCEEDED(hr)) {
                m_vFmt.colorSpace = (BMDColorspace)v;
            }

            // 他にMaxNits等幾つかHDR10関連の情報が取得できる。

            vfMeta->Release();
            vfMeta = nullptr;
        }
    } else {
        // HDRのメタデータが無い。
        // SDRである。

        m_vFmt.dynamicRange = bmdDynamicRangeSDR;

        if (m_vFmt.flags & bmdDisplayModeColorspaceRec709) {
            m_vFmt.colorSpace = bmdColorspaceRec709;
        } else {
            m_vFmt.colorSpace = bmdColorspaceRec601;
        }
    }
    if (m_callback) {
        m_callback->MLVideoCaptureCallback_VideoInputFrameArrived(videoFrame, audioPacket);
    }

    return S_OK;
}
