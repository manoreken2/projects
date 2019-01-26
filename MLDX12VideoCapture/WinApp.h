#pragma once

#include "Common.h"

class MLDX12;

class WinApp {
public:
    WinApp(void) { }
    virtual ~WinApp(void) { }
    static int Run(MLDX12* pSample, HINSTANCE hInstance, int nCmdShow);
    static HWND GetHwnd() { return mHwnd; }

protected:
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    static HWND mHwnd;
};

