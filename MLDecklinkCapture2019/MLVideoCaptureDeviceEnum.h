#pragma once

#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <comutil.h>

#include "DeckLinkAPI_h.h"
#include <list>
#include <string>

class MLVideoCaptureDeviceEnum : public IDeckLinkDeviceNotificationCallback {
public:
    MLVideoCaptureDeviceEnum(void);
    ~MLVideoCaptureDeviceEnum(void);
    HRESULT Init(void);

    void Term(void);

    int NumOfDevices(void) const;

    struct DeviceInf {
        IDeckLink* device;
        std::string name;
    };
    bool Device(int idx, DeviceInf &di_return);

    // IDecklinkDeviceNotificationCallback impl.
    virtual HRESULT	STDMETHODCALLTYPE	QueryInterface(REFIID iid, LPVOID *ppv);
    virtual ULONG	STDMETHODCALLTYPE	AddRef();
    virtual ULONG	STDMETHODCALLTYPE	Release();
    virtual HRESULT STDMETHODCALLTYPE DeckLinkDeviceArrived(
        /* [in] */ IDeckLink *deckLinkDevice);

    virtual HRESULT STDMETHODCALLTYPE DeckLinkDeviceRemoved(
        /* [in] */ IDeckLink *deckLinkDevice);

private:
    IDeckLinkDiscovery* m_deckLinkDiscovery;
    std::list<DeviceInf> m_deviceList;

    ULONG m_refCount;
};

