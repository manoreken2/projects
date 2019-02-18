#include "MLVideoCapture.h"

MLVideoCapture::MLVideoCapture(void)
{
    m_state = S_Init;
    m_refCount = 1;
    m_deckLink = nullptr;
    m_deckLinkInput = nullptr;
    m_videoModeIdx = 0;
    m_frameCounter = 0;
    m_callback = nullptr;

    mAudioSampleRate = 48000;
    mAudioBitsPerSample = 16;
    mAudioNumChannels = 2;
}


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

    IDeckLinkAttributes*            deckLinkAttributes = nullptr;
    IDeckLinkDisplayModeIterator*   displayModeIterator = nullptr;
    IDeckLinkDisplayMode*           displayMode = nullptr;
    BSTR                            deviceNameBSTR = nullptr;

    if (m_deckLink->QueryInterface(IID_IDeckLinkInput, (void**)&m_deckLinkInput) != S_OK) {
        MessageBox(nullptr, L"This device does not have input", L"Error", 0);
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

bool
MLVideoCapture::StartCapture(int videoModeIdx)
{
    m_frameCounter = 0;

    m_videoModeIdx = videoModeIdx;
    BMDVideoInputFlags		videoInputFlags = bmdVideoInputFlagDefault;

    videoInputFlags |= bmdVideoInputEnableFormatDetection;

    // Set capture callback
    m_deckLinkInput->SetCallback(this);

    // Set the video input mode
    if (S_OK != m_deckLinkInput->EnableVideoInput(m_modeList[m_videoModeIdx]->GetDisplayMode(), bmdFormat8BitYUV, videoInputFlags)) {
        MessageBox(nullptr,
            L"This application was unable to select the chosen video mode. Perhaps, the selected device is currently in-use.",
            L"Error starting the capture", 0);
        return false;
    }

    if (S_OK != m_deckLinkInput->EnableAudioInput(bmdAudioSampleRate48kHz, bmdAudioSampleType16bitInteger, 2)) {
        MessageBox(nullptr,
            L"This application was unable to select 48kHz 16bit. Perhaps, the selected device is currently in-use.",
            L"Error starting the capture", 0);
        return false;
    }

    if (m_deckLinkInput->StartStreams() != S_OK) {
        MessageBox(nullptr,
            L"This application was unable to start the capture. Perhaps, the selected device is currently in-use.",
            L"Error starting the capture", 0);
        return false;
    }

    m_state = S_Capturing;

    return true;
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

int
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
    newMode->GetFrameRate(&m_timeValue, &m_timeScale);
    int width = newMode->GetWidth();
    int height = newMode->GetHeight();
    char s[256];
    sprintf_s(s, "VideoInputFormatChanged %dx%d\n", width, height);
    OutputDebugStringA(s);

    unsigned int	modeIndex = 0;
    BMDPixelFormat	pixelFormat = bmdFormat10BitYUV;

    if (detectedSignalFlags & bmdDetectedVideoInputRGB444)
        pixelFormat = bmdFormat10BitRGB;

    m_width = width;
    m_height = height;
    m_pixelFormat = pixelFormat;

    m_deckLinkInput->StopStreams();

    if (m_deckLinkInput->EnableVideoInput(newMode->GetDisplayMode(), pixelFormat, bmdVideoInputEnableFormatDetection) != S_OK) {
        MessageBox(nullptr, L"Error restarting capture 1", L"Error", 0);
        goto bail;
    }

    if (m_deckLinkInput->StartStreams() != S_OK) {
        MessageBox(nullptr, L"Error restarting capture 2", L"Error", 0);
        goto bail;
    }

    while (modeIndex < m_modeList.size()) {
        if (m_modeList[modeIndex]->GetDisplayMode() == newMode->GetDisplayMode()) {
            m_videoModeIdx = modeIndex;
            break;
        }
        modeIndex++;
    }

bail:
    return S_OK;
}

HRESULT
MLVideoCapture::VideoInputFrameArrived(/* in */ IDeckLinkVideoInputFrame* videoFrame, /* in */ IDeckLinkAudioInputPacket* audioPacket)
{
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

    if (m_callback) {
        m_callback->MLVideoCaptureCallback_VideoInputFrameArrived(videoFrame, audioPacket);
    }

    return S_OK;
}
