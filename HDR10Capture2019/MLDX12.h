#pragma once

#include <string>
#include "Common.h"
#include "d3dx12.h"
#include <shellapi.h> //< HDROP

class MLDX12 {
public:
    MLDX12(int width, int height);
    virtual ~MLDX12(void);

    virtual void OnInit(void) = 0;
    virtual void OnUpdate(void) = 0;
    virtual void OnRender(void) = 0;
    virtual void OnDestroy(void) = 0;
    virtual void OnKeyDown(int key) = 0;
    virtual void OnKeyUp(int key) = 0;
    virtual void OnDropFiles(HDROP hDrop) = 0;
    virtual void OnSizeChanged(int width, int height, bool minimized) = 0;
    void SetWindowBounds(int left, int top, int right, int bottom);

    int Width(void) const { return mWidth; }
    int Height(void) const { return mHeight; }

    void UpdateForSizeChange(int clientWidth, int clientHeight);

protected:
    int mWidth;
    int mHeight;
    float mAspectRatio;
    RECT mWindowBounds;
    std::wstring mAssetsPath;

    void GetHardwareAdapter(IDXGIFactory2* pFactory, IDXGIAdapter1** ppAdapter);
};

