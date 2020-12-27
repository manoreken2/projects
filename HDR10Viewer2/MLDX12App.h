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

#pragma once
#include "MLDX12.h"
#include "MLDX12Imgui.h"
#include <mutex>
#include <list>
#include "MLAviWriter.h"
#include "MLConverter.h"
#include "MLImage.h"
#include "MLDrawings.h"
#include "MLAviReader.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

struct ImDrawData;

class MLDX12App : public MLDX12 {
public:
    MLDX12App(UINT width, UINT height, UINT options);
    virtual ~MLDX12App(void);

    virtual void OnInit(void);
    virtual void OnUpdate(void);
    virtual void OnRender(void);
    virtual void OnDestroy(void);
    virtual void OnKeyDown(int key) { }
    virtual void OnKeyUp(int key);
    virtual void OnSizeChanged(int width, int height, bool minimized);
    
    enum OptionsEnum {
        OE_HDR10 = 1,
    };

    enum TextureEnum {
        TE_IMG0,
        TE_IMG1,
        TE_PLAYVIDEO0,
        TE_PLAYVIDEO1,
        TE_IMGUI,
        TE_NUM
    };

private:
    static const UINT FrameCount = 2;

    static const int CapturedImageQueueSize = 30;

    enum State {
        S_Init,
        S_ImageLoading,
        S_ImageViewing,
    };

    struct Vertex
    {
        XMFLOAT3 position;
        XMFLOAT2 uv;
    };

    bool mShowImGui = true;
    UINT mOptions;
    State mState;
    MLImage::ImageMode mCaptureDrawMode;
    MLDrawings::CrosshairType mCrosshairType;
    bool mTitleSafeArea;
    MLDrawings::GridType mGridType;

    CD3DX12_VIEWPORT mViewport;
    CD3DX12_RECT mScissorRect;
    ComPtr<IDXGISwapChain3> mSwapChain;
    ComPtr<ID3D12Device> mDevice;
    ComPtr<ID3D12Resource> mRenderTargets[FrameCount];
    ComPtr<ID3D12CommandAllocator> mCmdAllocators[FrameCount];
    ComPtr<ID3D12CommandQueue> mCmdQ;
    ComPtr<ID3D12RootSignature> mRootSignature;
    ComPtr<ID3D12DescriptorHeap> mRtvHeap;
    ComPtr<ID3D12PipelineState> mPipelineStateRGB;
    ComPtr<ID3D12PipelineState> mPipelineStateYUV;

    MLDX12Imgui mDx12Imgui;
    ComPtr<ID3D12GraphicsCommandList> mCmdList;


    UINT mRtvDescSize;
    int mNumVertices;

    ComPtr<ID3D12Resource> mVertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW mVertexBufferView;

    ComPtr<ID3D12DescriptorHeap> mSrvHeap;
    UINT                         mSrvDescSize;
    ComPtr<ID3D12Resource>      mTexImgui;
    ComPtr<ID3D12Resource>       mTexImg[2];
    int mIdToShowCapVideoTex;

    UINT mFrameIdx;
    HANDLE mFenceEvent;
    ComPtr<ID3D12Fence> mFence;
    UINT64 mFenceValues[FrameCount];
    bool mWindowedMode;

    bool mRawSDI;    
    std::list<MLImage> mImagesToUploadToGpu;
    std::mutex mMutex;
    int64_t mFrameSkipCount;
    MLAviWriter mAviWriter;
    char mCaptureMsg[512];
    float mDrawGamma;
    float mDrawGainR;
    float mDrawGainG;
    float mDrawGainB;

    ComPtr<ID3D12Resource> mTexPlayVideo[2];
    MLImage mPlayImage;
    uint8_t *mPlayBuffer;
    int mPlayBufferBytes;
    int mPlayFrameNr;
    float mPlayAlpha;
    char mReadPath[512];
    char mPlayMsg[512];
    MLAviReader mAviReader;
    MLImage::ImageMode mPlayDrawMode;
    int mIdToShowPlayVideoTex;
    bool UpdatePlayVideoTexture(void);

    MLConverter mConverter;
    MLDrawings mDrawings;

    ComPtr<ID3D12GraphicsCommandList> mCmdListTexUpload;
    ComPtr<ID3D12CommandAllocator> mCmdAllocatorTexUpload;

    void LoadPipeline(void);
    void LoadAssets(void);
    void PopulateCommandList(void);
    void MoveToNextFrame(void);
    void WaitForGpu(void);
    void LoadSizeDependentResources(void);
    void UpdateViewAndScissor(void);

    void ClearDrawQueue(void);

    bool UpdateImgTexture(void);

    void CreateImguiTexture(void);
    void ImGuiCommands(void);

    void DrawFullscreenTexture(TextureEnum texId, MLImage::ImageMode drawMode);

    void ShowSettingsWindow(void);
    void ShowPlaybackWindow(void);
    void CreateVideoTexture(ComPtr<ID3D12Resource> &tex, int texIdx, int w, int h, DXGI_FORMAT fmt, int pixelBytes, uint8_t *data);
    void UploadImgToGpu(MLImage &ci, ComPtr<ID3D12Resource> &tex, int texIdx);

    // HDR10ÇÃê›íËÅB
    void UpdateColorSpace(void);
    bool mIsDisplayHDR10 = false;
    DXGI_FORMAT mBackBufferFmt = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_COLOR_SPACE_TYPE mColorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
    WCHAR mDeviceName[32] = {};
    RECT  mDesktopCoordinates = {};
    BOOL  mAttachedToDesktop = FALSE;
    DXGI_MODE_ROTATION mRotation = DXGI_MODE_ROTATION_IDENTITY;
    UINT mBitsPerColor = 0;
    FLOAT mMinLuminance = 0;
    FLOAT mMaxLuminance = 0;
    FLOAT mMaxFullFrameLuminance = 0;
 };
