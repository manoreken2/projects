#include "MLVideoCaptureEnum.h"

MLVideoCaptureEnum::MLVideoCaptureEnum(void) :
        m_deckLinkDiscovery(nullptr),
        m_refCount(1)
{
}


MLVideoCaptureEnum::~MLVideoCaptureEnum(void)
{
    Term();
}

void
MLVideoCaptureEnum::Init(void)
{
    if (nullptr == m_deckLinkDiscovery) {
        CoCreateInstance(CLSID_CDeckLinkDiscovery, nullptr, CLSCTX_ALL, IID_IDeckLinkDiscovery, (void**)&m_deckLinkDiscovery);
        if (m_deckLinkDiscovery == nullptr) {
            return;
        }
        m_deckLinkDiscovery->InstallDeviceNotifications(this);
    }
}

void
MLVideoCaptureEnum::Term(void)
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
MLVideoCaptureEnum::DeckLinkDeviceArrived(/* in */ IDeckLink* deckLink)
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
MLVideoCaptureEnum::DeckLinkDeviceRemoved(/* in */ IDeckLink* deckLink)
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

int MLVideoCaptureEnum::NumOfDevices(void) const
{
    return (int)m_deviceList.size();
}

std::string MLVideoCaptureEnum::DeviceName(int idx)
{
    auto ite = m_deviceList.begin();
    for (int i = 0; i < idx; ++ite);
    auto d = *ite;
    return d.name;
}

IDeckLink *MLVideoCaptureEnum::Device(int idx)
{
    auto ite = m_deviceList.begin();
    for (int i = 0; i < idx; ++ite);
    auto d = *ite;
    IDeckLink * p = d.device;
    return p;
}

HRESULT	STDMETHODCALLTYPE MLVideoCaptureEnum::QueryInterface(REFIID iid, LPVOID *ppv)
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

ULONG STDMETHODCALLTYPE MLVideoCaptureEnum::AddRef(void)
{
    return InterlockedIncrement((LONG*)&m_refCount);
}

ULONG STDMETHODCALLTYPE MLVideoCaptureEnum::Release(void)
{
    ULONG		newRefValue;

    newRefValue = InterlockedDecrement((LONG*)&m_refCount);
    if (newRefValue == 0) {
        delete this;
        return 0;
    }

    return newRefValue;
}
