#include <sdkddkver.h>

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include "MLDX12App.h"
#include "MLWinApp.h"

int APIENTRY
wWinMain(
        _In_ HINSTANCE hInstance,
        _In_opt_ HINSTANCE hPrevInstance,
        _In_ LPWSTR    lpCmdLine,
        _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    bool coInitialized = false;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr)) {
        coInitialized = true;
    }

    MLDX12App app(1920, 1080, MLDX12App::OE_HDR10);

    int rv = MLWinApp::Run(&app, hInstance, nCmdShow, L"HDR10Capture");

    if (coInitialized) {
        CoUninitialize();
    }

    return rv;
}

