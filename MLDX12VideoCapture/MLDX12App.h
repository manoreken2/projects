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
#include "MLVideoCaptureEnum.h"
#include "MLVideoCapture.h"
#include "IMLVideoCaptureCallback.h"
#include "MLDX12Imgui.h"
#include <mutex>
#include <list>
#include "MLAviWriter.h"
#include "MLConverter.h"
#include "MLCapturedImage.h"
#include "MLDrawings.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

struct ImDrawData;

class MLDX12App : public MLDX12, IMLVideoCaptureCallback {
public:
    MLDX12App(UINT width, UINT height);

    virtual void OnInit(void);
    virtual void OnUpdate(void);
    virtual void OnRender(void);
    virtual void OnDestroy(void);
    virtual void OnKeyDown(int key) { }
    virtual void OnKeyUp(int key);
    virtual void OnSizeChanged(int width, int height, bool minimized);
    
    void MLVideoCaptureCallback_VideoInputFrameArrived(IDeckLinkVideoInputFrame* videoFrame);


private:
    static const UINT FrameCount = 2;

    static const int CapturedImageQueueSize = 30;

    enum State {
        S_Init,
        S_Previewing,
        S_Recording,
        S_WaitRecordEnd,
    };

    struct Vertex
    {
        XMFLOAT3 position;
        XMFLOAT2 uv;
    };

    enum TextureEnum {
        TE_VIDEO0,
        TE_VIDEO1,
        TE_IMGUI,
        TE_NUM
    };

    State mState;
    MLCapturedImage::ImageMode mDrawMode;
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

    ComPtr<ID3D12GraphicsCommandList> mCmdListTexUpload;
    ComPtr<ID3D12CommandAllocator> mCmdAllocatorTexUpload;

    UINT mRtvDescSize;
    int mNumVertices;

    ComPtr<ID3D12Resource> mVertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW mVertexBufferView;

    ComPtr<ID3D12DescriptorHeap> mSrvHeap;
    UINT                         mSrvDescSize;
    ComPtr<ID3D12Resource>      mTexImgui;
    ComPtr<ID3D12Resource>       mTexVideo[2];
    int mTexVideoIdToShow;

    UINT mFrameIdx;
    HANDLE mFenceEvent;
    ComPtr<ID3D12Fence> mFence;
    UINT64 mFenceValues[FrameCount];
    bool mWindowedMode;
    bool mRawSDI;
    
    MLVideoCaptureEnum mVideoCaptureDeviceList;
    MLVideoCapture mVideoCapture;

    std::list<MLCapturedImage> mCapturedImages;
    std::mutex mMutex;

    int64_t mFrameSkipCount;

    MLAviWriter mAviWriter;

    char mWritePath[512];

    char mMsg[512];

    float mDrawGamma;

    MLConverter mConverter;
    MLDrawings mDrawings;
    
    void LoadPipeline(void);
    void LoadAssets(void);
    void PopulateCommandList(void);
    void MoveToNextFrame(void);
    void WaitForGpu(void);
    void LoadSizeDependentResources(void);
    void UpdateViewAndScissor(void);

    void ClearDrawQueue(void);

    void CreateVideoTexture(int texIdx, int w, int h, DXGI_FORMAT fmt, int pixelBytes, uint8_t *data);
    void UpdateVideoTexture(void);

    void SetupPSO(const wchar_t *shaderName, ComPtr<ID3D12PipelineState> & pso);

    void CreateImguiTexture(void);
    void ImGuiCommands(void);

    void DrawFullscreenTexture(void);

};
