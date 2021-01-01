#include "MLVideoCaptureDeviceEnum.h"

MLVideoCaptureDeviceEnum::MLVideoCaptureDeviceEnum(void) :
        m_deckLinkDiscovery(nullptr),
        m_refCount(1)
{
}


MLVideoCaptureDeviceEnum::~MLVideoCaptureDeviceEnum(void)
{
    Term();
}

HRESULT
MLVideoCaptureDeviceEnum::Init(void)
{
    if (nullptr == m_deckLinkDiscovery) {
        // decklinkDiscoveryを作成し、
        // デバイスが出現、消滅したときイベントを受け取る。
        // NumOfDevices()
        // DeviceName()
        // Device()
        // でデバイスの一覧を取得します。
        // 一覧データのMutexによるロックは行ってません。
        HRESULT hr = CoCreateInstance(CLSID_CDeckLinkDiscovery, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&m_deckLinkDiscovery));
        if (FAILED(hr)) {
            return hr;
        }
        
        if (m_deckLinkDiscovery == nullptr) {
            return E_FAIL;
        }

        m_deckLinkDiscovery->InstallDeviceNotifications(this);
    }
    else {
        // 既に初期化済。
    }

    return S_OK;
}

void
MLVideoCaptureDeviceEnum::Term(void)
{
    if (m_deckLinkDiscovery != nullptr) {
        m_deckLinkDiscovery->UninstallDeviceNotifications();
        m_deckLinkDiscovery->Release();
        m_deckLinkDiscovery = nullptr;
    }
    
    for (auto ite = m_deviceList.begin(); ite != m_deviceList.end(); ++ite) {
        auto item = *ite;
        item.device->Release();
        item.device = nullptr;
    }
    m_deviceList.clear();
}

HRESULT
MLVideoCaptureDeviceEnum::DeckLinkDeviceArrived(/* in */ IDeckLink* deckLink)
{
    deckLink->AddRef();

    DeviceInf di;
    di.device = deckLink;

    BSTR bstr = nullptr;
    if (deckLink->GetDisplayName(&bstr) == S_OK) {
        std::wstring wstr = std::wstring(bstr, SysStringLen(bstr));
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), nullptr, 0, nullptr, nullptr);
        std::string strTo(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, nullptr, nullptr);
        ::SysFreeString(bstr);

        di.name = strTo;
    } else {
        di.name = "UnknownDevice";
    }

    m_deviceList.push_back(di);
    return S_OK;
}

HRESULT
MLVideoCaptureDeviceEnum::DeckLinkDeviceRemoved(/* in */ IDeckLink* deckLink)
{
    for (auto ite = m_deviceList.begin(); ite != m_deviceList.end(); ++ite) {
        auto item = *ite;
        if (deckLink == item.device) {
            deckLink->Release();
            m_deviceList.erase(ite);
        }
    }
    return S_OK;
}

int MLVideoCaptureDeviceEnum::NumOfDevices(void) const
{
    return (int)m_deviceList.size();
}

bool
MLVideoCaptureDeviceEnum::Device(int idx, MLVideoCaptureDeviceEnum::DeviceInf & di_return)
{
    if (idx < 0 || m_deviceList.size() <= idx) {
        return false;
    }

    auto ite = m_deviceList.begin();
    for (int i = 0; i < idx; ++ite);
    di_return = *ite;
    return true;
}

HRESULT	STDMETHODCALLTYPE MLVideoCaptureDeviceEnum::QueryInterface(REFIID iid, LPVOID *ppv)
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
    } else if (iid == IID_IDeckLinkDeviceNotificationCallback) {
        *ppv = (IDeckLinkDeviceNotificationCallback*)this;
        AddRef();
        result = S_OK;
    }

    return result;
}

ULONG STDMETHODCALLTYPE MLVideoCaptureDeviceEnum::AddRef(void)
{
    return InterlockedIncrement((LONG*)&m_refCount);
}

ULONG STDMETHODCALLTYPE MLVideoCaptureDeviceEnum::Release(void)
{
    ULONG		newRefValue;

    newRefValue = InterlockedDecrement((LONG*)&m_refCount);
    if (newRefValue == 0) {
        delete this;
        return 0;
    }

    return newRefValue;
}
