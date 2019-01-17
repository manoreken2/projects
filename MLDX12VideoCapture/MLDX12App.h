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

    enum DrawMode {
        DM_RGB,
        DM_YUV
    };

    enum CrosshairType {
        CH_None,
        CH_CenterCrosshair,
        CH_4Crosshairs,
    };

    enum GridType {
        GR_None,
        GR_3x3,
        GR_6x6,
    };

    State m_state;
    DrawMode m_drawMode;
    CrosshairType m_crosshairType;
    bool m_titleSafeArea;
    GridType m_gridType;

    CD3DX12_VIEWPORT m_viewport;
    CD3DX12_RECT m_scissorRect;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
    ComPtr<ID3D12CommandAllocator> m_commandAllocators[FrameCount];
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12PipelineState> m_pipelineStateRGB;
    ComPtr<ID3D12PipelineState> m_pipelineStateYUV;

    MLDX12Imgui m_dx12Imgui;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;

    ComPtr<ID3D12GraphicsCommandList> m_commandListTextureUpload;
    ComPtr<ID3D12CommandAllocator> m_commandAllocatorTextureUpload;

    UINT m_rtvDescriptorSize;
    int m_numVertices;

    ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

    ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    UINT m_srvDescriptorSize;
    ComPtr<ID3D12Resource>      m_textureImgui;
    ComPtr<ID3D12Resource>       m_textureVideo[2];
    int m_textureVideoIdToShow;

    UINT m_frameIndex;
    HANDLE m_fenceEvent;
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValues[FrameCount];
    bool m_windowedMode;
    
    MLVideoCaptureEnum m_videoCaptureDeviceList;
    MLVideoCapture m_videoCapture;

    struct CapturedImage {
        uint8_t *data;
        int bytes;
        int width;
        int height;
        DrawMode drawMode;
        std::string imgFormat;
    };

    std::list<CapturedImage> m_capturedImages;
    std::mutex m_mutex;

    int64_t m_frameSkipCount;

    MLAviWriter m_aviWriter;

    char m_writePath[512];

    char m_msg[512];

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

    void AddCrosshair(CapturedImage &ci);
    void AddTitleSafeArea(CapturedImage &ci);
    void AddGrid(CapturedImage &ci);
};
