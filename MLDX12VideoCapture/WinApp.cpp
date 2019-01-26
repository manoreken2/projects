//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "WinApp.h"
#include "MLDX12.h"
#include "imgui.h"
#include "imgui_impl_win32.h"

HWND WinApp::mHwnd = nullptr;

WinApp::WinApp(void)
{
}

WinApp::~WinApp(void)
{
}

int
WinApp::Run(MLDX12* pDX12, HINSTANCE hInstance, int nCmdShow)
{
    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"DXSampleClass";
    RegisterClassEx(&wc);

    RECT rect = { 0, 0, static_cast<LONG>(pDX12->Width()), static_cast<LONG>(pDX12->Height()) };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    mHwnd = CreateWindow(
        wc.lpszClassName,
        L"WinApp",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        hInstance,
        pDX12);

    pDX12->OnInit();

    ShowWindow(mHwnd, nCmdShow);

    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

    }

    pDX12->OnDestroy();

    return static_cast<char>(msg.wParam);
}

LRESULT CALLBACK
WinApp::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    MLDX12* pDX12 = reinterpret_cast<MLDX12*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam)) {
        return true;
    }

    switch (message) {
    case WM_CREATE:
        {
            LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
        }
        return 0;

    case WM_KEYDOWN:
        if (pDX12) {
            pDX12->OnKeyDown(static_cast<UINT8>(wParam));
        }
        return 0;

    case WM_KEYUP:
        if (pDX12) {
            pDX12->OnKeyUp(static_cast<UINT8>(wParam));
        }
        return 0;

    case WM_PAINT:
        if (pDX12) {
            pDX12->OnUpdate();
            pDX12->OnRender();
        }
        return 0;

    case WM_SIZE:
        if (pDX12) {
            OutputDebugString(L"WM_SIZE\n");
            RECT windowR = {};
            GetWindowRect(hWnd, &windowR);
            pDX12->SetWindowBounds(
                windowR.left, windowR.top,
                windowR.right, windowR.bottom);
            RECT clientR = {};
            GetClientRect(hWnd, &clientR);
            pDX12->OnSizeChanged(
                clientR.right - clientR.left,
                clientR.bottom - clientR.top,
                wParam == SIZE_MINIMIZED);
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}
