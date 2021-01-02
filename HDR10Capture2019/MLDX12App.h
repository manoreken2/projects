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
#include "MLImage.h"
#include "MLColorGamut.h"
#include <stdint.h>
#include "MLColorConvShaderConstants.h"
#include "MLSaveSettings.h"
#include "MLVideoCapUser.h"


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
        TCE_TEX_IMG0,
        TCE_TEX_IMG1,
        TCE_TEX_PLAYVIDEO0,
        TCE_TEX_PLAYVIDEO1,
        TCE_TEX_IMGUI,
        TCE_TEX_NUM,
    };

private:
    static const UINT FenceCount = 2;

    static const int CapturedImageQueueSize = 30;

    enum State {
        S_Init,
        S_ImageViewing,
        S_Capturing,
    };

    struct Vertex
    {
        XMFLOAT3 position;
        XMFLOAT2 uv;
    };

    MLSaveSettings mSettings;

    bool mShowImGui = true;

    UINT mOptions;
    State mState;

    CD3DX12_VIEWPORT mViewport;
    CD3DX12_RECT mScissorRect;
    ComPtr<IDXGISwapChain3> mSwapChain;
    ComPtr<ID3D12Device> mDevice;
    ComPtr<ID3D12Resource> mRenderTargets[FenceCount];
    ComPtr<ID3D12CommandAllocator> mCmdAllocators[FenceCount];
    ComPtr<ID3D12CommandQueue> mCmdQ;
    ComPtr<ID3D12RootSignature> mRootSignature;
    ComPtr<ID3D12DescriptorHeap> mRtvHeap;
    ComPtr<ID3D12PipelineState> mPipelineState;

    MLDX12Imgui mDx12Imgui;
    ComPtr<ID3D12GraphicsCommandList> mCmdList;

    UINT mRtvDescSize;
    int mNumVertices;

    ComPtr<ID3D12Resource> mVertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW mVertexBufferView;

    ComPtr<ID3D12Resource> mConstantBuffer;
    /// <summary>
    /// �萔�o�b�t�@��map�����ςȂ��ɂ���B
    /// </summary>
    uint8_t* mPCbvDataBegin = nullptr;
    MLColorConvShaderConstants mShaderConsts;

    ComPtr<ID3D12DescriptorHeap> mCbvSrvDescHeap;
    UINT                         mDescHandleIncrementSz;

    ComPtr<ID3D12Resource>       mTexImgui;


    MLImage                      mShowImg;
    MLImage                      mWriteImg;

    /// <summary>
    /// �`�撆�e�N�X�`�����A�b�v���[�h����ƃN���b�V������̂ŁB
    /// �`�撆�e�N�X�`���ƃA�b�v���[�h�p�e�N�X�`����ʂɂ���B
    /// </summary>
    ComPtr<ID3D12Resource>       mTexImg[2];

    /// <summary>
    /// mTexImg[]�̗v�f�ԍ��B
    /// �`�撆�e�N�X�`����0�Ԃ̏ꍇ�A�A�b�v���[�h�p�e�N�X�`����1�ԁB
    /// �`�撆�e�N�X�`����1�Ԃ̏ꍇ�A�A�b�v���[�h�p�e�N�X�`����0�ԁB
    /// </summary>
    int                          mRenderTexImgIdx = 0;

    /// <summary>
    /// �g�p����mFenceValues[]�v�f�ԍ��B
    /// </summary>
    UINT mFenceIdx;
    HANDLE mFenceEvent;
    ComPtr<ID3D12Fence> mFence;
    UINT64 mFenceValues[FenceCount];

    bool mWindowedMode;

    std::mutex mMutex;

    char mErrorSettingsMsg[512] = {};
    char mErrorFileReadMsg[512] = {};

    char mImgFilePath[512] = {};

    ComPtr<ID3D12GraphicsCommandList> mCmdListTexUpload;
    ComPtr<ID3D12CommandAllocator> mCmdAllocatorTexUpload;

    void LoadPipeline(void);
    void LoadAssets(void);
    void PopulateCommandList(void);
    void MoveToNextFrame(void);
    void WaitForGpu(void);
    void LoadSizeDependentResources(void);
    void UpdateViewAndScissor(void);

    void SetDefaultImgTexture(void);
    bool UpdateImgTexture(void);


    void CreateTexture(ComPtr<ID3D12Resource>& tex, int texIdx, int w, int h, DXGI_FORMAT fmt, int pixelBytes, uint8_t* data);
    void CreateImguiTexture(void);
    void ImGuiCommands(void);

    void DrawFullscreenTexture(TextureEnum texId, MLImage & drawMode);

    void ShowSettingsWindow(void);
    void ShowImageFileRWWindow(void);

    void UploadImgToGpu(MLImage &ci, ComPtr<ID3D12Resource> &tex, int texIdx);
    void AdjustFullScreenQuadAspectRatio(int w, int h);
    void ReInit(void);

    // HDR10�̐ݒ�B
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
    MLColorGamutType mDisplayColorGamut = ML_CG_Rec709;

    MLColorGamutConv mGamutConv;

    // �r�f�I�L���v�`���[�B
    MLVideoCapUser mVCU;
    void ShowVideoCaptureWindow(void);
    int  mUseVCDeviceIdx = 0;
    MLVideoCaptureDeviceEnum::DeviceInf mVCDeviceToUse;
    enum VideoCaptureState {
        VCS_PreInit,
        VCS_CapturePreview,
        VCS_Recording,
        VCS_WaitRecordEnd,
    };
    VideoCaptureState mVCState = VCS_PreInit;
    char mErrorVCMsg[512] = {};
    char mAviFilePath[512] = {};
 };
