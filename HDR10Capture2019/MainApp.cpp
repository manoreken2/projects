#include "MainApp.h"
#include "DXSampleHelper.h"
#include "MLWinApp.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "MLDX12Common.h"
#include "MLEnumToStr.h"
#include "MLExrReader.h"
#include "half.h"
#include "MLImage2.h"
#include "MLPngReader.h"
#include "MLPngWriter.h"
#include "MLBmpReader.h"
#include "MLVideoCaptureEnumToStr.h"
#include "MLVideoTime.h"
#include "MLExrWriter.h"
#include <shlwapi.h>
#include <shellapi.h>
#include <assert.h>

// shader source code as string
#include "shaderVS.inc"
#include "shaderColorConvPS.inc"

// D3D12HelloFrameBuffering sampleを改造して作成。
//*********************************************************
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//*********************************************************

MainApp::MainApp(UINT width, UINT height, UINT options)
    : MLDX12(width, height),
        mSettings("Settings.ini"),
        mOptions(options),
        mState(S_Init),
        mFenceIdx(0),
        mViewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
        mScissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
        mFenceValues{},
        mRtvDescSize(0),
        mNumVertices(0),
        mWindowedMode(true)
{
    // 設定を読み出します。
    std::string imgPath = mSettings.LoadStringA("ImgFilePath");
    if (imgPath.empty()) {
        wcscpy_s(mImgFilePath, L"c:/data/OpenEXR_HDR10_test1.exr");
    } else {
        memset(mImgFilePath, 0, sizeof mImgFilePath);
        MultiByteToWideChar(CP_UTF8, 0, imgPath.c_str(), -1, mImgFilePath, _countof(mImgFilePath)-1);
    }
    std::string aviPath = mSettings.LoadStringA("AviFilePath");
    if (aviPath.empty()) {
        wcscpy_s(mAviFilePath, L"D:/test.avi");
    } else {
        memset(mAviFilePath, 0, sizeof mAviFilePath);
        MultiByteToWideChar(CP_UTF8, 0, aviPath.c_str(), -1, mAviFilePath, _countof(mAviFilePath) - 1);
    }

    int outOfRangeR = mSettings.LoadInt("OutOfRangeR", 0);
    int outOfRangeG = mSettings.LoadInt("OutOfRangeG", 0);
    int outOfRangeB = mSettings.LoadInt("OutOfRangeB", 255);
    double outOfRangeNits = mSettings.LoadDouble("OutOfRangeNits", 100.0);

    mDisplayColorGamut = (MLColorGamutType)mSettings.LoadInt("DisplayColorGamut", ML_CG_scRGB);

    mShaderConsts.colorConvMat = XMMatrixIdentity();
    mShaderConsts.outOfRangeColor = XMFLOAT4(outOfRangeR / 255.0f, outOfRangeG / 255.0f, outOfRangeB / 255.0f, 1.0f);
    mShaderConsts.imgGammaType = MLImage2::MLG_Linear;
    mShaderConsts.flags = 0;
    mShaderConsts.outOfRangeNits = (float)outOfRangeNits;
    mShaderConsts.scale = 1.0f;
}

MainApp::~MainApp(void)
{
    // 設定の書き出し。
    {
        // WriteToFile()まで消えないバッファーを作る。
        char imgPath[MAX_PATH * 3];
        memset(imgPath, 0, sizeof imgPath);
        WideCharToMultiByte(CP_UTF8, 0, mImgFilePath, -1, imgPath, sizeof imgPath - 1, nullptr, nullptr);
        mSettings.SaveStringA("ImgFilePath", imgPath);

        // WriteToFile()まで消えないバッファーを作る。
        char aviPath[MAX_PATH * 3];
        memset(aviPath, 0, sizeof aviPath);
        WideCharToMultiByte(CP_UTF8, 0, mAviFilePath, -1, aviPath, sizeof aviPath - 1, nullptr, nullptr);
        mSettings.SaveStringA("AviFilePath", aviPath);

        int outOfRangeR = (int)(mShaderConsts.outOfRangeColor.x * 255.0f);
        int outOfRangeG = (int)(mShaderConsts.outOfRangeColor.y * 255.0f);
        int outOfRangeB = (int)(mShaderConsts.outOfRangeColor.z * 255.0f);
        mSettings.SaveInt("OutOfRangeR", outOfRangeR);
        mSettings.SaveInt("OutOfRangeG", outOfRangeG);
        mSettings.SaveInt("OutOfRangeB", outOfRangeB);
        mSettings.SaveDouble("OutOfRangeNits", mShaderConsts.outOfRangeNits);

        mSettings.SaveInt("DisplayColorGamut", mDisplayColorGamut);

        mSettings.WriteToFile();
    }

    delete[] mAviImgBuf;
    mAviImgBuf = nullptr;
}

void
MainApp::OnInit(void)
{
    //OutputDebugString(L"OnInit started\n");

    LoadPipeline();

    {   // mFenceValuesと、mFenceEventを作成。
        ThrowIfFailed(mDevice->CreateFence(mFenceValues[mFenceIdx],
            D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));
        NAME_D3D12_OBJECT(mFence);
        mFenceValues[mFenceIdx]++;
        mFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (mFenceEvent == nullptr) {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }
    }

    LoadAssets();

    mDx12Imgui.Init(mDevice.Get(), mBackBufferFmt);
    CreateImguiTexture();

    //OutputDebugString(L"OnInit end\n");

    mVCU.Init(mMutex);
    mVCU.SetCallback(this);

    {
        // コマンドラインargsをパースする。
        LPCSTR arg = GetCommandLineA();
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

        if (argc == 2) {
            wchar_t* pathW = argv[1];
            wcscpy_s(mImgFilePath, pathW);
            ReadImg();
        }
    }
}

/// <summary>
/// バグっている。使用不可。
/// </summary>
void
MainApp::ReInit(void)
{
    LoadPipeline();
    LoadAssets();
    mDx12Imgui.Init(mDevice.Get(), mBackBufferFmt);
    CreateImguiTexture();
}

void
MainApp::OnDestroy(void)
{
    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    WaitForGpu();

    CloseHandle(mFenceEvent);

    mVCU.SetCallback(nullptr);

    mRenderImg.Term();
    mWriteImg.Term();

    mDx12Imgui.Term();

    mConstantBuffer->Unmap(0, nullptr);
    mPCbvDataBegin = nullptr;

    mVCU.Term();
}

void
MainApp::LoadPipeline(void)
{
    UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
    // Enable the debug layer (requires the Graphics Tools "optional feature").
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
            debugController->EnableDebugLayer();
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    ComPtr<IDXGIFactory4> factory;
    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

    ComPtr<IDXGIAdapter1> hardwareAdapter;
    GetHardwareAdapter(factory.Get(), &hardwareAdapter);

    mDevice.Reset();
    ThrowIfFailed(D3D12CreateDevice(
        hardwareAdapter.Get(),
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&mDevice)));

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    mCmdQ.Reset();
    ThrowIfFailed(mDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCmdQ)));
    NAME_D3D12_OBJECT(mCmdQ);

    if (mOptions & OE_HDR10) {
        // HDR10のためのサーフェス。
        mBackBufferFmt = DXGI_FORMAT_R16G16B16A16_FLOAT;
    } else {
        // SDRのサーフェス。
        mBackBufferFmt = DXGI_FORMAT_R8G8B8A8_UNORM;
    }

    DXGI_SWAP_CHAIN_DESC1 scDesc = {};
    scDesc.BufferCount = FenceCount;
    scDesc.Width = mWidth;
    scDesc.Height = mHeight;
    scDesc.Format = mBackBufferFmt;
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain;
    mSwapChain.Reset();

    // ↓この関数は2回呼べないようだ。
    ThrowIfFailed(factory->CreateSwapChainForHwnd(
        mCmdQ.Get(),
        MLWinApp::GetHwnd(),
        &scDesc,
        nullptr,
        nullptr,
        &swapChain));
    ThrowIfFailed(swapChain.As(&mSwapChain));

    mFenceIdx = mSwapChain->GetCurrentBackBufferIndex();

    UpdateColorSpace();

    {
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = FenceCount;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        mRtvHeap.Reset();
        ThrowIfFailed(mDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&mRtvHeap)));
        mRtvDescSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        NAME_D3D12_OBJECT(mRtvHeap);
    }
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
        for (UINT n = 0; n < FenceCount; n++) {
            mRenderTargets[n].Reset();
            ThrowIfFailed(mSwapChain->GetBuffer(n, IID_PPV_ARGS(&mRenderTargets[n])));

#if 1
            mDevice->CreateRenderTargetView(mRenderTargets[n].Get(), nullptr, rtvHandle);
#else
            D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
            rtvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            rtvDesc.Texture2D.MipSlice = 0;
            rtvDesc.Texture2D.PlaneSlice = 0;

            mDevice->CreateRenderTargetView(mRenderTargets[n].Get(), &rtvDesc, rtvHandle);
#endif
            rtvHandle.Offset(1, mRtvDescSize);
            NAME_D3D12_OBJECT_INDEXED(mRenderTargets, n);
        }
    }

    mDescHandleIncrementSz = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    {
        /* SRV + CBVヒープ。シェーダーからは b0 および t0 でアクセスする。
           ディスクリプタは1個あたりmCbvSrvHandleIncrementSzバイト。
           ディスクリプタはTCE_TEX_NUM + 1個。以下の順で並んでいる。
            0: TCE_TEX_IMG0,
            1: TCE_TEX_IMG1,
            2: TCE_TEX_PLAYVIDEO0,
            3: TCE_TEX_PLAYVIDEO1,
            4: TCE_TEX_IMGUI,
            5: constant, b0
        */
        D3D12_DESCRIPTOR_HEAP_DESC cbvSrvHeapDesc = {};
        cbvSrvHeapDesc.NumDescriptors = TCE_TEX_NUM + 1;
        cbvSrvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        cbvSrvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        mCbvSrvDescHeap.Reset();
        ThrowIfFailed(mDevice->CreateDescriptorHeap(&cbvSrvHeapDesc, IID_PPV_ARGS(&mCbvSrvDescHeap)));
        NAME_D3D12_OBJECT(mCbvSrvDescHeap);
    }

    {
        for (UINT n = 0; n < FenceCount; n++) {
            mCmdAllocators[n].Reset();
            ThrowIfFailed(
                mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                    IID_PPV_ARGS(&mCmdAllocators[n])));
            NAME_D3D12_OBJECT_INDEXED(mCmdAllocators, n);
        }

        mCmdAllocatorTexUpload.Reset();
        ThrowIfFailed(
            mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                IID_PPV_ARGS(&mCmdAllocatorTexUpload)));
        NAME_D3D12_OBJECT(mCmdAllocatorTexUpload);

        mCmdListTexUpload.Reset();
        ThrowIfFailed(mDevice->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT, mCmdAllocatorTexUpload.Get(),
            mPipelineState.Get(), IID_PPV_ARGS(&mCmdListTexUpload)));
        NAME_D3D12_OBJECT(mCmdListTexUpload);
        mCmdListTexUpload->Close();
    }
}

void
MainApp::UpdateColorSpace(void)
{
    // SDRの場合。
    DXGI_COLOR_SPACE_TYPE colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;

    mIsDisplayHDR10 = false;

    if (mSwapChain) {
        ComPtr<IDXGIOutput> output;
        if (SUCCEEDED(mSwapChain->GetContainingOutput(output.GetAddressOf()))) {
            ComPtr<IDXGIOutput6> output6;
            if (SUCCEEDED(output.As(&output6))) {
                DXGI_OUTPUT_DESC1 desc;
                ThrowIfFailed(output6->GetDesc1(&desc));

                mColorSpace = desc.ColorSpace;
                if (desc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020) {
                    // HDR10ディスプレイに出力している。
                    mIsDisplayHDR10 = true;
                    //mDisplayColorGamut = ML_CG_Rec2020;
                } else {
                    mIsDisplayHDR10 = false;
                    //mDisplayColorGamut = ML_CG_Rec709;
                }

                wcscpy_s(mDeviceName, desc.DeviceName);
                mDesktopCoordinates = desc.DesktopCoordinates;
                mAttachedToDesktop = desc.AttachedToDesktop;
                mRotation = desc.Rotation;
                mBitsPerColor = desc.BitsPerColor;
                mMinLuminance = desc.MinLuminance;
                mMaxLuminance = desc.MaxLuminance;
                mMaxFullFrameLuminance = desc.MaxFullFrameLuminance;
            }
        }
    }

    //HDR10で、Floatのバックバッファーの場合、ガンマをリニアに変更する。
    if ((mOptions & OE_HDR10) && mIsDisplayHDR10) {
        switch (mBackBufferFmt) {
        case DXGI_FORMAT_R10G10B10A2_UNORM:
            // The application creates the HDR10 signal.
            // アプリがHDR10のガンマの処理をする必要がある。
            colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
            break;
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
            // The system creates the HDR10 signal; application uses linear values.
            // アプリはHDR10のガンマの処理をしなくて良い。
            // scRGB色空間のピクセル値を書き込む必要あり。
            // https://docs.microsoft.com/en-us/windows/win32/direct3darticles/high-dynamic-range
            colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709;
            break;
        default:
            assert(0);
            break;
        }

        ComPtr<IDXGISwapChain3> swapChain3;
        if (SUCCEEDED(mSwapChain.As(&swapChain3))) {
            UINT colorSpaceSupport = 0;
            if (SUCCEEDED(swapChain3->CheckColorSpaceSupport(colorSpace, &colorSpaceSupport))
                && (colorSpaceSupport & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT)) {
                ThrowIfFailed(swapChain3->SetColorSpace1(colorSpace));
            }
        }
    }
}

void
MainApp::LoadAssets(void)
{
    {
        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
        if (FAILED(mDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof featureData))) {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }

        // ピクセルシェーダーから、テクスチャが1個、定数バッファが1個見える。
        CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

        CD3DX12_ROOT_PARAMETER1 rootParameters[2];
        rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL);

        D3D12_STATIC_SAMPLER_DESC sampler = {};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        sampler.MipLODBias = 0;
        sampler.MaxAnisotropy = 0;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        sampler.MinLOD = 0.0f;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
        sampler.ShaderRegister = 0;
        sampler.RegisterSpace = 0;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters,
            1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc,
            featureData.HighestVersion, &signature, &error));

        mRootSignature.Reset();
        ThrowIfFailed(mDevice->CreateRootSignature(0, signature->GetBufferPointer(),
            signature->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));
        NAME_D3D12_OBJECT(mRootSignature);
    }

    mPipelineState.Reset();
    MLDX12Common::SetupPSOFromMemory(mDevice.Get(), mBackBufferFmt, mRootSignature.Get(), 
           "shaderVS", strlen(g_shaderVS), g_shaderVS,
           "shaderPS", strlen(g_shaderColorConvPS), g_shaderColorConvPS,
            mPipelineState);
    NAME_D3D12_OBJECT(mPipelineState);

    mCmdList.Reset();
    ThrowIfFailed(mDevice->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT, mCmdAllocators[mFenceIdx].Get(),
        mPipelineState.Get(), IID_PPV_ARGS(&mCmdList)));
    NAME_D3D12_OBJECT(mCmdList);

    {   // 画面全体を覆う矩形を作成。
        // 仮形状。後で形状を調整する。AdjustFullScreenQuadAspectRatio()参照。
        Vertex verts[] =
        {
            {{-1.0f, +1.0f, 0.0f}, {0.0f, 0.0f}},
            {{+1.0f, +1.0f, 0.0f}, {1.0f, 0.0f}},
            {{-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
            {{-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
            {{+1.0f, +1.0f, 0.0f}, {1.0f, 0.0f}},
            {{+1.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
        };

        int numTriangles = 2;
        mNumVertices = 3 * numTriangles;

        const UINT vbBytes = sizeof verts;

        const CD3DX12_HEAP_PROPERTIES heapTypeUpload(D3D12_HEAP_TYPE_UPLOAD);
        const CD3DX12_RESOURCE_DESC resDescvbBytes = CD3DX12_RESOURCE_DESC::Buffer(vbBytes);

        mVertexBuffer.Reset();
        ThrowIfFailed(mDevice->CreateCommittedResource(
            &heapTypeUpload,
            D3D12_HEAP_FLAG_NONE,
            &resDescvbBytes,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&mVertexBuffer)));
        NAME_D3D12_OBJECT(mVertexBuffer);

        UINT8* pVertexDataBegin;
        // We do not intend to read from this resource on the CPU.
        CD3DX12_RANGE readRange(0, 0);
        ThrowIfFailed(mVertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
        memcpy(pVertexDataBegin, verts, vbBytes);
        mVertexBuffer->Unmap(0, nullptr);

        mVertexBufferView.BufferLocation = mVertexBuffer->GetGPUVirtualAddress();
        mVertexBufferView.StrideInBytes = sizeof(Vertex);
        mVertexBufferView.SizeInBytes = vbBytes;
    }

    UpdateViewAndScissor();

    {
        // 定数バッファ。
        const CD3DX12_HEAP_PROPERTIES heapTypeUpload(D3D12_HEAP_TYPE_UPLOAD);
        const CD3DX12_RESOURCE_DESC resDescBytes = CD3DX12_RESOURCE_DESC::Buffer(1024 * 64);
        mConstantBuffer.Reset();
        ThrowIfFailed(mDevice->CreateCommittedResource(
            &heapTypeUpload,
            D3D12_HEAP_FLAG_NONE,
            &resDescBytes,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&mConstantBuffer)));

        // 定数バッファービュー。
        CD3DX12_CPU_DESCRIPTOR_HANDLE cbvHandle(mCbvSrvDescHeap->GetCPUDescriptorHandleForHeapStart());
        cbvHandle.Offset(TCE_TEX_NUM, mDescHandleIncrementSz);

        // Describe and create a constant buffer view.
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = mConstantBuffer->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = (sizeof mShaderConsts + 255) & ~255;    //< CB size is required to be 256-byte aligned.
        mDevice->CreateConstantBufferView(&cbvDesc, cbvHandle);

        // Map and initialize the constant buffer. We don't unmap this until the
        // app closes. Keeping things mapped for the lifetime of the resource is okay.
        CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
        ThrowIfFailed(mConstantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&mPCbvDataBegin)));
        memcpy(mPCbvDataBegin, &mShaderConsts, sizeof mShaderConsts);
    }

    ThrowIfFailed(mCmdList->Close());
    ID3D12CommandList* ppCommandLists[] = { mCmdList.Get() };
    mCmdQ->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
    WaitForGpu();

    SetDefaultImgTexture();
}

void
MainApp::SetDefaultImgTexture(void)
{
    const int texW = 3840;
    const int texH = 2160;
    const int pixelBytes = 2 * 4; // 2==sizeof(half), 4==r,g,b,a
    half* buff = new half[texW * texH * 4];
    for (int y = 0; y < texH; ++y) {
        for (int x = 0; x < texW; ++x) {
            int pos = (x + y * texW) * 4;
            buff[pos + 0] = 0.01f;
            buff[pos + 1] = 0.01f;
            buff[pos + 2] = 0.01f;
            buff[pos + 3] = 1.0f;
        }
    }

    mMutex.lock();

    mRenderImg.Term();
    mRenderImg.Init(texW, texH, MLImage2::IFFT_OpenEXR, MLImage2::BFT_HalfFloatR16G16B16A16, ML_CG_Rec2020, MLImage2::MLG_Linear, 16, 4, pixelBytes, nullptr);

    mMutex.unlock();

    mTexImg[mTexImgIdx].Reset();
    CreateTexture(mTexImg[mTexImgIdx], TCE_TEX_IMG0, texW, texH, DXGI_FORMAT_R16G16B16A16_FLOAT, pixelBytes, (uint8_t*)buff);

    delete[] buff;
}

void
MainApp::AdjustFullScreenQuadAspectRatio(int w, int h)
{
    assert(0 < w);
    assert(0 < h);

    double windowAR = (double)Width() / Height();
    double imgAR = (double)w / h;

    float xP = 1.0f;
    float yP = 1.0f;
    if (imgAR < windowAR) {
        // 画面よりも表示画像が縦長の場合、Vertsの横の長さを短くする。
        xP = (float)(imgAR / windowAR);
    }
    if (windowAR < imgAR) {
        // 画面よりも表示画像が横長。
        yP = (float)(windowAR / imgAR);
    }

    {   // 3d vtx is x+:right, y+:up
        // UV     is x+:right, y+:down
        //
        // 0 1
        //  □
        // 2
        //
        //   4
        //  □
        // 3 5
        //
        Vertex verts[] =
        {
            {{-xP, +yP, 0.0f}, {0.0f, 0.0f}}, // ↖
            {{+xP, +yP, 0.0f}, {1.0f, 0.0f}}, // ↗
            {{-xP, -yP, 0.0f}, {0.0f, 1.0f}}, // ↙
            {{-xP, -yP, 0.0f}, {0.0f, 1.0f}}, // ↙
            {{+xP, +yP, 0.0f}, {1.0f, 0.0f}}, // ↗
            {{+xP, -yP, 0.0f}, {1.0f, 1.0f}}, // ↘
        };

        int numTriangles = 2;
        mNumVertices = 3 * numTriangles;

        const UINT vbBytes = sizeof verts;
        assert(mVertexBuffer.Get());

        UINT8* pVertexDataBegin;
        CD3DX12_RANGE readRange(0, 0); //< We do not intend to read from this resource on the CPU.

        ThrowIfFailed(mVertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
        memcpy(pVertexDataBegin, verts, vbBytes);
        mVertexBuffer->Unmap(0, nullptr);
    }
}

void
MainApp::CreateTexture(ComPtr<ID3D12Resource>& tex, int texIdx,
        int w, int h, DXGI_FORMAT fmt, int pixelBytes, uint8_t* data)
{
    ThrowIfFailed(mCmdAllocators[mFenceIdx].Get()->Reset());
    ThrowIfFailed(mCmdList->Reset(mCmdAllocators[mFenceIdx].Get(), mPipelineState.Get()));

    // texUploadHeapがスコープから外れる前にcommandListを実行しなければならない。
    ComPtr<ID3D12Resource> texUploadHeap;
    {
        D3D12_RESOURCE_DESC texDesc = {};
        texDesc.MipLevels = 1;
        texDesc.Format = fmt;
        texDesc.Width = w;
        texDesc.Height = h;
        texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        texDesc.DepthOrArraySize = 1;
        texDesc.SampleDesc.Count = 1;
        texDesc.SampleDesc.Quality = 0;
        texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

        const CD3DX12_HEAP_PROPERTIES heapTypeDefault = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        ThrowIfFailed(mDevice->CreateCommittedResource(
            &heapTypeDefault,
            D3D12_HEAP_FLAG_NONE,
            &texDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&tex)));
        NAME_D3D12_OBJECT(tex);

        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(tex.Get(), 0, 1);

        const CD3DX12_HEAP_PROPERTIES heapTypeUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        const CD3DX12_RESOURCE_DESC resDescUploadBufSz = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
        ThrowIfFailed(mDevice->CreateCommittedResource(
            &heapTypeUpload,
            D3D12_HEAP_FLAG_NONE,
            &resDescUploadBufSz,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&texUploadHeap)));

        D3D12_SUBRESOURCE_DATA textureData = {};
        textureData.pData = &data[0];
        textureData.RowPitch = w * pixelBytes;
        textureData.SlicePitch = textureData.RowPitch * h;

        UpdateSubresources(mCmdList.Get(), tex.Get(), texUploadHeap.Get(), 0, 0, 1, &textureData);
        const CD3DX12_RESOURCE_BARRIER barrierTransitionCopyToPS = CD3DX12_RESOURCE_BARRIER::Transition(tex.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        mCmdList->ResourceBarrier(1, &barrierTransitionCopyToPS);

        // texIdx番目のディスクリプタを更新。
        CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(mCbvSrvDescHeap->GetCPUDescriptorHandleForHeapStart());
        srvHandle.Offset(texIdx, mDescHandleIncrementSz);

        // Describe and create a SRV for the texture.
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = texDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        mDevice->CreateShaderResourceView(tex.Get(), &srvDesc, srvHandle);
    }

    ThrowIfFailed(mCmdList->Close());
    ID3D12CommandList* ppCommandLists[] = { mCmdList.Get() };
    mCmdQ->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
    WaitForGpu();
}

void
MainApp::CreateImguiTexture(void)
{
    ThrowIfFailed(mCmdAllocators[mFenceIdx].Get()->Reset());
    ThrowIfFailed(mCmdList->Reset(mCmdAllocators[mFenceIdx].Get(), mPipelineState.Get()));

    // Build texture atlas
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    int pixelBytes = 4;

    // Upload texture to graphics system
    // texUploadHeapがスコープから外れる前にcommandListを実行しなければならない。
    ComPtr<ID3D12Resource> texUploadHeap;
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    texDesc.DepthOrArraySize = 1;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    const CD3DX12_HEAP_PROPERTIES heapTypeDefault = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    mTexImgui.Reset();
    ThrowIfFailed(mDevice->CreateCommittedResource(
        &heapTypeDefault,
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&mTexImgui)));
    NAME_D3D12_OBJECT(mTexImgui);

    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(mTexImgui.Get(), 0, 1);

    // Create the GPU upload buffer.
    const CD3DX12_HEAP_PROPERTIES heapTypeUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    const CD3DX12_RESOURCE_DESC resDescUploadBufSz = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
    ThrowIfFailed(mDevice->CreateCommittedResource(
        &heapTypeUpload,
        D3D12_HEAP_FLAG_NONE,
        &resDescUploadBufSz,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&texUploadHeap)));

    D3D12_SUBRESOURCE_DATA textureData = {};
    textureData.pData = &pixels[0];
    textureData.RowPitch = width * pixelBytes;
    textureData.SlicePitch = textureData.RowPitch * height;

    UpdateSubresources(mCmdList.Get(), mTexImgui.Get(), texUploadHeap.Get(), 0, 0, 1, &textureData);
    const CD3DX12_RESOURCE_BARRIER barrierTransitionCopyToPS = CD3DX12_RESOURCE_BARRIER::Transition(mTexImgui.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    mCmdList->ResourceBarrier(1, &barrierTransitionCopyToPS);

    ThrowIfFailed(mCmdList->Close());
    ID3D12CommandList* ppCommandLists[] = { mCmdList.Get() };
    mCmdQ->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
    WaitForGpu();

    // IMGUIのテクスチャー用SRVの位置srvCpuHandle。
    CD3DX12_CPU_DESCRIPTOR_HANDLE srvCpuHandle(mCbvSrvDescHeap->GetCPUDescriptorHandleForHeapStart());
    srvCpuHandle.Offset(TCE_TEX_IMGUI, mDescHandleIncrementSz);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory(&srvDesc, sizeof srvDesc);
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    mDevice->CreateShaderResourceView(mTexImgui.Get(), &srvDesc, srvCpuHandle);

    // Store our identifier
    CD3DX12_GPU_DESCRIPTOR_HANDLE srvGpuHandle(mCbvSrvDescHeap->GetGPUDescriptorHandleForHeapStart());
    srvGpuHandle.Offset(TCE_TEX_IMGUI, mDescHandleIncrementSz);
    io.Fonts->TexID = (ImTextureID)srvGpuHandle.ptr;
}

void
MainApp::OnKeyUp(int key)
{
    switch (key) {
    case VK_F7:
        mShowImGui = !mShowImGui;
        break;
    case VK_ESCAPE:
    {
        BOOL fullscreenState;
        ThrowIfFailed(mSwapChain->GetFullscreenState(&fullscreenState, nullptr));
        if (fullscreenState) {
            // フルスクリーンの時、ウィンドウモードに遷移する。
            if (FAILED(mSwapChain->SetFullscreenState(!fullscreenState, nullptr))) {
                // エラーは起こりうる。
            }
        }
    }
    break;
    case VK_SPACE:
    {
        BOOL fullscreenState;
        ThrowIfFailed(mSwapChain->GetFullscreenState(&fullscreenState, nullptr));
        if (FAILED(mSwapChain->SetFullscreenState(!fullscreenState, nullptr))) {
            // Transitions to fullscreen mode can fail when running apps over
            // terminal services or for some other unexpected reason.  Consider
            // notifying the user in some way when this happens.
            OutputDebugString(L"Fullscreen transition failed\n");
        }
        break;
    }
    default:
        break;
    }
}

void
MainApp::OnDropFiles(HDROP hDrop)
{
    wchar_t path[MAX_PATH];
    memset(path, 0, sizeof path);

    UINT rv = DragQueryFile(hDrop, 0, path, _countof(path) - 1);
    if (0 < rv) {
        size_t sz = wcslen(path);

        if (APS_Playing == mAviPlayState) {
            mAviReader.Close();
            mAviPlayState = APS_PreInit;
            mState = S_Init;
        }

        if (0 == _wcsicmp(L".avi", &path[sz - 4])) {
            // AVIファイル。
            wcscpy_s(mAviFilePath, path);
            mRequestReadAvi = true;
        } else {
            wcscpy_s(mImgFilePath, path);
            mRequestReadImg = true;
        }
    }
}

void
MainApp::OnSizeChanged(int width, int height, bool minimized)
{
    /*
    char s[256];
    sprintf_s(s, "\n\nOnSizeChanged(%d %d)\n", width, height);
    OutputDebugStringA(s);
    */

    if ((width != Width() || height != Height()) && !minimized) {
        WaitForGpu();

        for (UINT n = 0; n < FenceCount; n++) {
            mRenderTargets[n].Reset();
            mFenceValues[n] = mFenceValues[mFenceIdx];
        }

        DXGI_SWAP_CHAIN_DESC desc = {};
        mSwapChain->GetDesc(&desc);
        ThrowIfFailed(mSwapChain->ResizeBuffers(FenceCount, width, height, desc.BufferDesc.Format, desc.Flags));

        BOOL fullscreenState;
        ThrowIfFailed(mSwapChain->GetFullscreenState(&fullscreenState, nullptr));
        mWindowedMode = !fullscreenState;

        mFenceIdx = mSwapChain->GetCurrentBackBufferIndex();

        UpdateForSizeChange(width, height);

        LoadSizeDependentResources();
    }
}

void
MainApp::UpdateViewAndScissor(void)
{
    float x = 1.0f;
    float y = 1.0f;

    mViewport.TopLeftX = mWidth * (1.0f - x) / 2.0f;
    mViewport.TopLeftY = mHeight * (1.0f - y) / 2.0f;
    mViewport.Width = x * mWidth;
    mViewport.Height = y * mHeight;

    mScissorRect.left = static_cast<LONG>(mViewport.TopLeftX);
    mScissorRect.right = static_cast<LONG>(mViewport.TopLeftX + mViewport.Width);
    mScissorRect.top = static_cast<LONG>(mViewport.TopLeftY);
    mScissorRect.bottom = static_cast<LONG>(mViewport.TopLeftY + mViewport.Height);
}

void
MainApp::LoadSizeDependentResources(void)
{
    UpdateViewAndScissor();

    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
        for (UINT n = 0; n < FenceCount; n++) {
            ThrowIfFailed(mSwapChain->GetBuffer(n, IID_PPV_ARGS(&mRenderTargets[n])));
            mDevice->CreateRenderTargetView(mRenderTargets[n].Get(), nullptr, rtvHandle);
            rtvHandle.Offset(1, mRtvDescSize);

            NAME_D3D12_OBJECT_INDEXED(mRenderTargets, n);
        }
    }
}

void
MainApp::OnUpdate(void)
{
    mShaderConsts.colorConvMat = mGamutConv.ConvMat(mRenderImg.colorGamut, mDisplayColorGamut);
    mShaderConsts.imgGammaType = mRenderImg.gamma;

    if (mPCbvDataBegin) {
        memcpy(mPCbvDataBegin, &mShaderConsts, sizeof mShaderConsts);
    }
}

void
MainApp::OnRender(void)
{
    PopulateCommandList();

    ID3D12CommandList* ppCommandLists[] = { mCmdList.Get() };
    mCmdQ->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // texture upload
    bool bUpCap = UpdateImgTexture();
    if (!bUpCap) {
        // 画像テクスチャをアップロードしなかった場合ここに来る。
        // テクスチャアップロードのための資源が1個しかないので、
        // 他のアップロード物がある場合、1個づつ順番に処理する。
    }

    ThrowIfFailed(mSwapChain->Present(1, 0));
    MoveToNextFrame();
}

void
MainApp::PopulateCommandList(void)
{
    ThrowIfFailed(mCmdAllocators[mFenceIdx]->Reset());
    ThrowIfFailed(mCmdList->Reset(mCmdAllocators[mFenceIdx].Get(), mPipelineState.Get()));

    const CD3DX12_RESOURCE_BARRIER barrier_Transition_Present_To_RenderTarget = CD3DX12_RESOURCE_BARRIER::Transition(
        mRenderTargets[mFenceIdx].Get(), D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    mCmdList->ResourceBarrier(1, &barrier_Transition_Present_To_RenderTarget);

    {
        const float clearColorRGBA[] = { 0.0f, 0.0f, 0.0f, 0.0f };
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
            mFenceIdx, mRtvDescSize);
        mCmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
        mCmdList->ClearRenderTargetView(rtvHandle, clearColorRGBA, 0, nullptr);
    }

    // 全クライアント領域に画像を描画。
    DrawFullscreenTexture(
        (TextureEnum)(TCE_TEX_IMG0 + mTexImgIdx),
        mRenderImg);

    // Start the Dear ImGui frame
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGuiCommands();
    mDx12Imgui.Render(ImGui::GetDrawData(), mCmdList.Get(), mFenceIdx);

    // Indicate that the back buffer will now be used to present.
    const CD3DX12_RESOURCE_BARRIER barrier_Transition_RenderTarget_to_Present = CD3DX12_RESOURCE_BARRIER::Transition(
        mRenderTargets[mFenceIdx].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    mCmdList->ResourceBarrier(1, &barrier_Transition_RenderTarget_to_Present);

    ThrowIfFailed(mCmdList->Close());
}

void
MainApp::DrawFullscreenTexture(TextureEnum texId, MLImage2& img)
{
    // シェーダーの選択。
    mCmdList->SetPipelineState(mPipelineState.Get());

    AdjustFullScreenQuadAspectRatio(img.width, img.height);

    mCmdList->SetGraphicsRootSignature(mRootSignature.Get());

    ID3D12DescriptorHeap* ppHeaps[] = { mCbvSrvDescHeap.Get() };
    mCmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    {
        // 定数バッファをセット。
        CD3DX12_GPU_DESCRIPTOR_HANDLE dhC(mCbvSrvDescHeap->GetGPUDescriptorHandleForHeapStart());
        dhC.Offset(TCE_TEX_NUM, mDescHandleIncrementSz);
        mCmdList->SetGraphicsRootDescriptorTable(0, dhC);
    }

    {
        // 使用するテクスチャを選ぶ。
        CD3DX12_GPU_DESCRIPTOR_HANDLE dhTex(mCbvSrvDescHeap->GetGPUDescriptorHandleForHeapStart());
        dhTex.Offset(texId, mDescHandleIncrementSz);
        mCmdList->SetGraphicsRootDescriptorTable(1, dhTex);
    }

    mCmdList->RSSetViewports(1, &mViewport);
    mCmdList->RSSetScissorRects(1, &mScissorRect);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
        mFenceIdx, mRtvDescSize);
    mCmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    mCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    mCmdList->IASetVertexBuffers(0, 1, &mVertexBufferView);
    mCmdList->DrawInstanced(mNumVertices, 1, 0, 0);
}

void
MainApp::MoveToNextFrame(void) {
    const UINT64 currentFenceValue = mFenceValues[mFenceIdx];
    ThrowIfFailed(mCmdQ->Signal(mFence.Get(), currentFenceValue));
    mFenceIdx = mSwapChain->GetCurrentBackBufferIndex();

    if (mFence->GetCompletedValue() < mFenceValues[mFenceIdx]) {
        ThrowIfFailed(mFence->SetEventOnCompletion(mFenceValues[mFenceIdx], mFenceEvent));

        WaitForSingleObjectEx(mFenceEvent, INFINITE, FALSE);
    }

    mFenceValues[mFenceIdx] = currentFenceValue + 1;
}

void
MainApp::WaitForGpu(void) {
    ThrowIfFailed(mCmdQ->Signal(mFence.Get(), mFenceValues[mFenceIdx]));
    ThrowIfFailed(mFence->SetEventOnCompletion(mFenceValues[mFenceIdx], mFenceEvent));
    WaitForSingleObjectEx(mFenceEvent, INFINITE, FALSE);
    mFenceValues[mFenceIdx]++;
}

MLAviImageFormat
BMDPixelFormatToMLAviImageFormat(BMDPixelFormat t)
{
    switch (t) {
    case bmdFormat8BitYUV:
        return MLIF_YUV422_UYVY;
    case bmdFormat10BitYUV:
        return MLIF_YUV422_v210;
    case bmdFormat10BitRGB:
        return MLIF_RGB10bit_r210;
    case bmdFormat12BitRGB:
        return MLIF_RGB12bit_R12B;
    default:
        return MLIF_Unknown;
    }
}

void
MainApp::UpdateCaptureImg(void)
{
    HRESULT hr = S_OK;

    if (mRenderImg.data == nullptr) {
        // mRenderImgをGPUに送り終えるとdata == nullptrになる。

        MLImage2 newImg;
        hr = mVCU.PopCapturedImg(newImg);
        if (SUCCEEDED(hr)) {
            // 新しい表示用キャプチャー画像がある場合、GPUに送り出す。
            // 表示用キャプチャー画像を静止画書き込み用バッファーにコピー。

            // ガンマの設定を確定する。
            const MLVideoCaptureVideoFormat& fmt = mVCU.CurrentVideoFormat();
            if (fmt.colorSpace == bmdColorspaceRec2020
                && fmt.dynamicRange == bmdDynamicRangeHDRStaticPQ) {
                mCaptureImgGamma = MLImage2::MLG_ST2084;
            } else {
                mCaptureImgGamma = MLImage2::MLG_G22;
            }

            mWriteImg.Term();
            mWriteImg.DeepCopyFrom(newImg);

            mMutex.lock();
            mRenderImg = newImg;

            // キャプチャー画像のガンマが信用できないので上書きする。
            mRenderImg.gamma = mCaptureImgGamma;
            mMutex.unlock();
        }
    }
}

void
MainApp::MLVideoCapUserCallback_VideoInputFormatChanged(const MLVideoCaptureVideoFormat & vFmt)
{
    // VideoFormatの変更イベントは解像度が変わったときと、ピクセルフォーマットが変わったときに起きるようだ。
}

void
MainApp::ShowVideoCaptureWindow(void)
{
    HRESULT hr = S_OK;
    char s[MAX_PATH * 3];
    memset(s, 0, sizeof s);

    ImGui::Begin("Video Capture Settings ##VCS");

    if (ImGui::BeginPopupModal("ErrorVCPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text(mErrorVCMsg);
        if (ImGui::Button("OK ##EVM", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
            mErrorVCMsg[0] = 0;
        }
        ImGui::EndPopup();
    }

    switch (mVCState) {
    case VCS_PreInit:
        {
            if (ImGui::TreeNodeEx("Device to Use ##VCS", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_CollapsingHeader)) {

                // デバイス一覧を表示。
                int deviceCount = 0;
                for (int i = 0; i < mVCU.NumOfDevices(); ++i) {
                    if (mVCU.Device(i, mVCDeviceToUse) && mVCDeviceToUse.device != nullptr) {
                        ImGui::RadioButton(mVCDeviceToUse.name.c_str(), &mUseVCDeviceIdx, 0);
                        ++deviceCount;
                    }
                }

                if (deviceCount <= mUseVCDeviceIdx) {
                    // 選択中デバイスが取り外された場合。
                    mUseVCDeviceIdx = 0;
                }

                if (0 < deviceCount) {
                    if (ImGui::Button("Use selected device ##VCS")) {
                        // 選択デバイスを使用開始。
                        hr = mVCU.UseDevice(mVCDeviceToUse.device);
                        if (FAILED(hr)) {
                            sprintf_s(mErrorVCMsg, "Error: Use DeckLink Device failed %08x", hr);
                            ImGui::OpenPopup("ErrorVCPopup");
                        } else {
                            // 成功。
                            mVCState = VCS_CapturePreview;
                            mState = S_Capturing;
                        }
                    }
                } else {
                    // 1台も無い。
                    ImGui::Text("No DeckLink Video Capture device found.");
                }
            }
        }
        break;
    case VCS_CapturePreview:
        {
            const MLVideoCaptureVideoFormat& fmt = mVCU.CurrentVideoFormat();

            ImGui::Text("Now Using \"%s\"", mVCDeviceToUse.name.c_str());
            if (ImGui::Button("Unuse ##VCS")) {
                mVCU.UnuseDevice();
                mVCState = VCS_PreInit;
                mState = S_Init;
            } else {
                if (ImGui::Button("Flush Streams ##VCS")) {
                    mVCU.FlushStreams();
                    mVCU.ClearCapturedImageList();
                }

                ImGui::Text("Input Signal: %s, %d x %d, %.2f fps, %s, %s(?), %s, %s",
                    BMDDisplayModeToStr(fmt.displayMode),
                    fmt.width, fmt.height,
                    (double)fmt.frameRateTS/fmt.frameRateTV,
                    BMDColorspaceToStr(fmt.colorSpace),
                    BMDDynamicRangeToStr(fmt.dynamicRange),
                    BMDFieldDominanceToStr(fmt.fieldDominance),
                    BMDDetectedVideoInputFormatFlagsToStr(mVCU.DetectedVideoInputFormatFlags()).c_str());

                if ((mVCU.DetectedVideoInputFormatFlags() & bmdDetectedVideoInputRGB444) ||
                    (mVCU.DetectedVideoInputFormatFlags() & bmdDetectedVideoInputYCbCr422)) {
                    // キャプチャー可能。
                    ImGui::Text("Captured As: %s", BMDPixelFormatToStr(fmt.pixelFormat));
                } else {
                    // キャプチャー不可。対応してない。
                    ImGui::Text("Unsupported format! %s",
                        BMDDetectedVideoInputFormatFlagsToStr(mVCU.DetectedVideoInputFormatFlags()).c_str());
                }

                ImGui::Text("Captured image gamma = %s", MLImage2::GammaTypeToStr(mCaptureImgGamma));

                ImGui::Text("Frame Skip : %d", mVCU.FrameSkipCount());

                MLAviImageFormat aviIF = BMDPixelFormatToMLAviImageFormat(fmt.pixelFormat);

                if (MLIF_Unknown != aviIF) {
                    WideCharToMultiByte(CP_UTF8, 0, mAviFilePath, -1, s, sizeof s - 1, nullptr, nullptr);
                    if (ImGui::InputText("Record AVI filename ##VCS", s, sizeof s - 1)) {
                        // text Updated.
                        MultiByteToWideChar(CP_UTF8, 0, s, -1, mAviFilePath, _countof(mAviFilePath));
                    }

                    if (ImGui::Button("Record ## VCS", ImVec2(256, 48))) {
                        if (PathFileExists(mAviFilePath)) {
                            sprintf_s(mErrorVCMsg, "Error: File exists.\nPlease input different file name.");
                            ImGui::OpenPopup("ErrorVCPopup");
                        } else {
                            bool bRv = mVCU.AviWriter().Start(
                                mAviFilePath, fmt.width, fmt.height,
                                (double)fmt.frameRateTS/fmt.frameRateTV,
                                aviIF, true);
                            if (bRv) {
                                mVCState = VCS_Recording;
                                mErrorVCMsg[0] = 0;
                            } else {
                                sprintf_s(mErrorVCMsg, "Error: Record Failed.\nFile open error.");
                                ImGui::OpenPopup("ErrorVCPopup");
                            }
                        }
                    }
                }

                UpdateCaptureImg();
            }
        }
        break;
    case VCS_Recording:
        ImGui::Text("Now Recording...");
        ImGui::Text("Record filename : %S", mAviFilePath);
        {
            // hour:min:sec:frameを算出。
            auto vt = MLFrameNrToTime((int)(mVCU.AviWriter().FramesPerSec()+0.5), mVCU.AviWriter().TotalVideoFrames());
            ImGui::Text("%02d:%02d:%02d:%02d",
                vt.hour, vt.min, vt.sec, vt.frame);
        }

        ImGui::Text("Captured image gamma = %s", MLImage2::GammaTypeToStr(mCaptureImgGamma));

        ImGui::Text("Display Queue size : %d", mVCU.CapturedImageCount());
        ImGui::Text("Rec Queue size : %d", mVCU.AviWriter().RecQueueSize());

        if (ImGui::Button("Stop Recording", ImVec2(256, 64))) {
            mVCState = VCS_WaitRecordEnd;

            mVCU.AviWriter().StopAsync();

            ImGui::OpenPopup("WriteFlushPopup");
        }
        
        UpdateCaptureImg();

        break;
    case VCS_WaitRecordEnd:
        {
            ImGui::Text("Now Writing AVI...\nRemaining %d frames.", mVCU.AviWriter().RecQueueSize());
            bool bEnd = mVCU.AviWriter().PollThreadEnd();
            if (bEnd) {
                mVCState = VCS_CapturePreview;
            }
        }
        break;
    }

    ImGui::End();
}

void
MainApp::ShowSettingsWindow(void) {
    ImGui::Begin("Settings");

    if (ImGui::BeginPopupModal("ErrorSettingsPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text(mErrorSettingsMsg);
        if (ImGui::Button("OK ##ESM", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
            mErrorSettingsMsg[0] = 0;
        }
        ImGui::EndPopup();
    }

    ImGui::Text("Frames Per Second : %.1f", ImGui::GetIO().Framerate);

    ImGui::Separator();

    ImGui::Text("ALT+Enter to toggle fullscreen.");
    ImGui::Text("F7 to show/hide UI.");
    ImGui::Text("Drop file to display it.");

    switch (mState) {
    case S_Init:
        ImGui::Text("Specify image to show.");
        break;
    case S_ImageViewing:
        ImGui::Text("Image loaded.");
        break;
    case S_VideoViewing:
        ImGui::Text("Video viewing.");
        break;
    case S_Capturing:
        ImGui::Text("Video Capturing.");
        break;
    default:
        assert(0);
        break;
    }

    if (ImGui::TreeNodeEx("Display Properties", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_CollapsingHeader)) {
        ImGui::Text("Display is %s, %d x %d, %d bit",
            mIsDisplayHDR10 ? "HDR10" : "SDR",
            mDesktopCoordinates.right - mDesktopCoordinates.left,
            mDesktopCoordinates.bottom - mDesktopCoordinates.top,
            mBitsPerColor);

        ImGui::Text("BackBuffer = %s",
            DxgiFormatToStr(mBackBufferFmt));

        ImGui::Text("Display ColorSpace = %s",
            DxgiColorSpaceToStr(mColorSpace));

        ImGui::Text("AttachedToDesktop = %s, Rotation = %s",
            mAttachedToDesktop ? "True" : "False",
            DxgiModeRotationToStr(mRotation));

        ImGui::Text("Display Luminance: min = %f, max = %f, maxFullFrame = %f",
            mMinLuminance, mMaxLuminance, mMaxFullFrameLuminance);
    }

    if (ImGui::TreeNodeEx("Backbuffer Color Gamut (scRGB is recommended)", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_CollapsingHeader)) {
        int cg = (int)mDisplayColorGamut;
        ImGui::RadioButton("Rec.709 ##DCG", &cg, ML_CG_Rec709);
        ImGui::RadioButton("Adobe RGB ##DCG", &cg, ML_CG_AdobeRGB);
        ImGui::RadioButton("DCI-P3 ##DCG", &cg, ML_CG_DCIP3);
        ImGui::RadioButton("Rec.2020 ##DCG", &cg, ML_CG_Rec2020);
        ImGui::RadioButton("scRGB ##DCG", &cg, ML_CG_scRGB);
        mDisplayColorGamut = (MLColorGamutType)cg;
    }

    if (ImGui::TreeNodeEx("Out of Range Data", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_CollapsingHeader)) {
        bool outofRange = 0 != (mShaderConsts.flags & MLColorConvShaderConstants::FLAG_OutOfRangeColor);
        ImGui::Checkbox("Fill Color ##OR", &outofRange);
        if (outofRange) {
            mShaderConsts.flags |= MLColorConvShaderConstants::FLAG_OutOfRangeColor;

            int cg;
            if (mShaderConsts.outOfRangeNits == 100.0f) {
                cg = 0;
            } else if (mShaderConsts.outOfRangeNits == 1000.0f) {
                cg = 1;
            } else if (mShaderConsts.outOfRangeNits == 2000.0f) {
                cg = 2;
            } else if (mShaderConsts.outOfRangeNits == 10000.0f) {
                cg = 3;
            } else {
                cg = 3;
            }
            ImGui::RadioButton("Max 100 nits ##COR", &cg, 0);
            ImGui::RadioButton("Max 1000 nits ##COR", &cg, 1);
            ImGui::RadioButton("Max 2000 nits ##COR", &cg, 2);
            ImGui::RadioButton("Max 10000 nits ##COR", &cg, 3);

            switch (cg) {
            case 0:
                mShaderConsts.outOfRangeNits = 100.0f;
                break;
            case 1:
                mShaderConsts.outOfRangeNits = 1000.0f;
                break;
            case 2:
                mShaderConsts.outOfRangeNits = 2000.0f;
                break;
            case 3:
                mShaderConsts.outOfRangeNits = 10000.0f;
                break;
            }

            float c[3] = { mShaderConsts.outOfRangeColor.x,
                           mShaderConsts.outOfRangeColor.y,
                           mShaderConsts.outOfRangeColor.z };
            ImGui::ColorPicker3("Fill color ##CP", c);
            mShaderConsts.outOfRangeColor.x = c[0];
            mShaderConsts.outOfRangeColor.y = c[1];
            mShaderConsts.outOfRangeColor.z = c[2];
        } else {
            mShaderConsts.flags = mShaderConsts.flags & (~MLColorConvShaderConstants::FLAG_OutOfRangeColor);
        }
    }

    if (mState == S_ImageViewing && mVCState == VCS_PreInit) {
        MLImage2& img = mRenderImg;

        if (ImGui::TreeNodeEx("Image Properties", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_CollapsingHeader)) {
            ImGui::Text("Original is %d bit %d ch %s",
                img.originalNumChannels * img.originalBitDepth,
                img.originalNumChannels,
                MLImage2::MLImageFileFormatTypeToStr(img.imgFileFormat));
            ImGui::Text("%d x %d, %s",
                img.width, img.height, MLImage2::MLImageBitFormatToStr(img.bitFormat));

            ImGui::SliderFloat("Image Brightness Scaling", &mShaderConsts.scale, 0.001f, 100.0f);
            ImGui::SameLine();
            if (ImGui::Button("Reset ##IBS")) {
                mShaderConsts.scale = 1.0f;
            }

            {
                bool b = 0 != (mShaderConsts.flags & MLColorConvShaderConstants::FLAG_SwapRedBlue);
                ImGui::Checkbox("Swap Red and Blue", &b);
                if (b) {
                    mShaderConsts.flags |= MLColorConvShaderConstants::FLAG_SwapRedBlue;
                } else {
                    mShaderConsts.flags = mShaderConsts.flags & (~MLColorConvShaderConstants::FLAG_SwapRedBlue);
                }
            }
        }

        if (ImGui::TreeNodeEx("Image Color Gamut", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_CollapsingHeader)) {
            //ImGui::Text("Color Gamut is %s", MLColorGamutToStr(img.colorGamut));

            int cg = (int)img.colorGamut;
            ImGui::RadioButton("Rec.709 ##ICG", &cg, 0);
            ImGui::RadioButton("Adobe RGB ##ICG", &cg, 1);
            ImGui::RadioButton("Rec.2020 ##ICG", &cg, 2);
            img.colorGamut = (MLColorGamutType)cg;

            //ImGui::TreePop();
        }
        if (ImGui::TreeNodeEx("Image Gamma curve", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_CollapsingHeader)) {
            int cg = (int)img.gamma;
            ImGui::RadioButton("Linear ##IGC", &cg, 0);
            ImGui::RadioButton("Gamma 2.2 ##IGC", &cg, 1);
            ImGui::RadioButton("ST.2084 PQ ##IGC", &cg, 2);
            img.gamma = (MLImage2::GammaType)cg;
        }
    }

    if (ImGui::TreeNodeEx("Image Quantization Range (Set Full for HDR)", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_CollapsingHeader)) {
        int cg = 0 != (mShaderConsts.flags & MLColorConvShaderConstants::FLAG_LimitedRange);
        ImGui::RadioButton("Full ##ICG", &cg, 0);
        ImGui::RadioButton("Limited ##ICG", &cg, 1);

        if (cg) {
            mShaderConsts.flags |= MLColorConvShaderConstants::FLAG_LimitedRange;
        } else {
            mShaderConsts.flags = mShaderConsts.flags & (~MLColorConvShaderConstants::FLAG_LimitedRange);
        }
    }

    ImGui::End();
}

enum ExtensionType {
    ET_PNG,
    ET_EXR,
    ET_BMP,
    ET_Other,
};

static ExtensionType
PathNameToExtensionType(const wchar_t* path)
{
    assert(path);

    int len = (int)wcslen(path);
    if (len < 5) {
        return ET_Other;
    }

    if (0 == _wcsicmp(&path[len - 4], L".PNG")) {
        return ET_PNG;
    }
    if (0 == _wcsicmp(&path[len - 4], L".EXR")) {
        return ET_EXR;
    }
    if (0 == _wcsicmp(&path[len - 4], L".BMP")) {
        return ET_BMP;
    }
    return ET_Other;
}

HRESULT
MainApp::ReadImg(void)
{
    HRESULT hr = 0;

    if (ET_Other == PathNameToExtensionType(mImgFilePath)) {
        sprintf_s(mErrorFileReadMsg, "Error: Unsupported Image format.");
        ImGui::OpenPopup("ErrorImageFileRWPopup");
        return E_FAIL;
    }

    mMutex.lock();
    hr = MLBmpRead(mImgFilePath, mRenderImg);
    if (hr == 1) {
        // ファイルは存在するがBMPではなかった場合。
        hr = MLPngRead(mImgFilePath, mRenderImg);
        if (hr == 1) {
            // ファイルは存在するがPNGではなかった場合。
            char s[MAX_PATH * 3];
            memset(s, 0, sizeof s);
            WideCharToMultiByte(CP_UTF8, 0, mImgFilePath, -1, s, sizeof s - 1, nullptr, nullptr);
            hr = MLExrRead(s, mRenderImg);
        }
    }
    mMutex.unlock();

    if (hr < 0) {
        sprintf_s(mErrorFileReadMsg, "Error: Read Image Failed.");
        ImGui::OpenPopup("ErrorImageFileRWPopup");
    } else {
        mState = S_ImageViewing;
    }

    return hr;
}


void
MainApp::ShowImageFileRWWindow(void)
{
    char path[MAX_PATH];
    memset(path, 0, sizeof path);

    int hr = S_OK;
    ImGui::Begin("File Read / Write");

    if (ImGui::BeginPopupModal("ErrorImageFileRWPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text(mErrorFileReadMsg);
        if (ImGui::Button("OK ## EFM", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
            mErrorFileReadMsg[0] = 0;
        }
        ImGui::EndPopup();
    }

    if (mState == S_Capturing) {
        // キャプチャー中。
        memset(path, 0, sizeof path);
        WideCharToMultiByte(CP_UTF8, 0, mImgFilePath, -1, path, sizeof path - 1, nullptr, nullptr);
        if (ImGui::InputText("PNG / EXR Image Filename to Write", path, sizeof path - 1)) {
            // text has changed. update mImgFilePath.
            MultiByteToWideChar(CP_UTF8, 0, path, -1, mImgFilePath, _countof(mImgFilePath) - 1);
        }

        if (mWriteImg.data != nullptr) {
            // 静止画をファイルに保存する。

            ExtensionType et = PathNameToExtensionType(mImgFilePath);
            switch (et) {
            case ET_PNG:
                if (ImGui::Button("Write PNG Image ##RF0", ImVec2(256, 48))) {
                    if (PathFileExists(mImgFilePath)) {
                        sprintf_s(mErrorFileReadMsg, "Error: File exists.\nPlease input different file name.");
                        ImGui::OpenPopup("ErrorImageFileRWPopup");
                    } else {
                        hr = MLPngWrite(mImgFilePath, mWriteImg);

                        if (FAILED(hr)) {
                            sprintf_s(mErrorFileReadMsg, "Error: Write Image Failed.\nFile Write error.");
                            ImGui::OpenPopup("ErrorImageFileRWPopup");
                        }
                    }
                }
                break;
            case ET_EXR:
                if (ImGui::Button("Write EXR Image ##RF0", ImVec2(256, 48))) {
                    if (PathFileExists(mImgFilePath)) {
                        sprintf_s(mErrorFileReadMsg, "Error: File exists.\nPlease input different file name.");

                        ImGui::OpenPopup("ErrorImageFileRWPopup");
                    } else {
                        WideCharToMultiByte(CP_UTF8, 0, mImgFilePath, -1, path, sizeof path - 1, nullptr, nullptr);
                        hr = mExrWriter.Write(path, mWriteImg);
                        if (FAILED(hr)) {
                            sprintf_s(mErrorFileReadMsg, "Error: Write Image Failed.\nFile Write error.");
                            ImGui::OpenPopup("ErrorImageFileRWPopup");
                        }
                    }
                }
                break;
            default:
                ImGui::Text("Cannot save image file: Unknown file extension.");
                break;
            }
        }
    } else {
        memset(path, 0, sizeof path);
        WideCharToMultiByte(CP_UTF8, 0, mImgFilePath, -1, path, sizeof path - 1, nullptr, nullptr);

        if (ImGui::InputText("EXR/PNG/BMP Image Filename to Read", path, sizeof path - 1)) {
            // text has changed. update mImgFilePath.
            MultiByteToWideChar(CP_UTF8, 0, path, -1, mImgFilePath, _countof(mImgFilePath) - 1);
        }

        if (mRequestReadImg || ImGui::Button("Read Image ##RF0", ImVec2(256, 48))) {
            ReadImg();
            mRequestReadImg = false;
        }
    }

    ImGui::End();
}

const char *
MainApp::AviPlayStateToStr(AviPlayState t)
{
    switch (t) {
    case APS_PreInit:
        return "PreInit";
    case APS_Playing:
        return "Playing";
    case APS_Stopped:
        return "Stopped";
    default:
        assert(0);
        return "Unknown";
    }
}

void
MainApp::ShowAviPlaybackWindow(void) {
    char path[MAX_PATH];
    memset(path, 0, sizeof path);

    ImGui::Begin("AVI Playback Control");

    if (ImGui::BeginPopupModal("ErrorPlayPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text(mPlayMsg);
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
            mPlayMsg[0] = 0;
        }
        ImGui::EndPopup();
    }

    {
        memset(path, 0, sizeof path);
        WideCharToMultiByte(CP_UTF8, 0, mAviFilePath, -1, path, sizeof path - 1, nullptr, nullptr);
        if (ImGui::InputText("Read filename", path, sizeof path - 1)) {
            // text has changed. update mImgFilePath.
            MultiByteToWideChar(CP_UTF8, 0, path, -1, mAviFilePath, _countof(mAviFilePath) - 1);
        }
    }

    if (mAviReader.NumFrames() <= 0) {
        if (mRequestReadAvi || ImGui::Button("Open")) {
            mRequestReadAvi = false;
            if (mAviReader.Open(mAviFilePath)) {
                const MLBitmapInfoHeader& bi = mAviReader.ImageFormat();

                if (bi.biCompression != 0
                        && bi.biCompression != MLFOURCC_r210
                        && bi.biCompression != MLFOURCC_v210) {
                    sprintf_s(mPlayMsg, "Error: Not supported AVI format : %S", mAviFilePath);
                    ImGui::OpenPopup("ErrorPlayPopup");
                    mAviReader.Close();
                } else {
                    // success
                    mPlayMsg[0] = 0;
                    mPlayFrameNr = 0;

                    const MLBitmapInfoHeader& bmpih = mAviReader.ImageFormat();

                    // bitsPerPixelを求めます。
                    int bitsPerPixel = 0;
                    switch (bi.biCompression) {
                    case 0:
                        bitsPerPixel = bmpih.biBitCount;
                        break;
                    case MLFOURCC_r210:
                        bitsPerPixel = 32; //< X2R10G10B10
                        break;
                    case MLFOURCC_v210:
                        bitsPerPixel = 24; //< 若干多めに確保する。
                        break;
                    default:
                        assert(0);
                        break;
                    }

                    mAviImgBufBytes = bmpih.biWidth * bmpih.biHeight * bitsPerPixel / 8;

                    delete[] mAviImgBuf;
                    mAviImgBuf = nullptr;
                    mAviImgBuf = new uint8_t[mAviImgBufBytes];
                    mAviPlayState = APS_Playing;
                    mState = S_VideoViewing;
                }
            } else {
                sprintf_s(mPlayMsg, "Read AVI Failed.\nFile open error : %S", mAviFilePath);
                ImGui::OpenPopup("ErrorPlayPopup");
            }
        }
    } else {
        ImGui::Text("%s %d x %d, %d fps, %.1f sec %s",
            AviPlayStateToStr(mAviPlayState),
            mAviReader.ImageFormat().biWidth, mAviReader.ImageFormat().biHeight,
            mAviReader.FramesPerSec(), mAviReader.DurationSec(),
            MLFourCCtoString(mAviReader.ImageFormat().biCompression).c_str());

        {
            // hour:min:sec:frameを算出。
            auto vt = MLFrameNrToTime(mAviReader.FramesPerSec(), mPlayFrameNr);

            ImGui::Text("%02d:%02d:%02d:%02d",
                vt.hour, vt.min, vt.sec, vt.frame);
        }

        // シークバー。
        if (ImGui::DragInt("Frame number", &mPlayFrameNr, 1.0f, 0, mAviReader.NumFrames() - 1)) {
            // ユーザー操作によりフレーム番号が変更。
            UpdateAviTexture();
        }

        switch (mAviPlayState) {
        case APS_Stopped:
            if (ImGui::Button("Play")) {
                mAviPlayState = APS_Playing;
            }
            break;
        case APS_Playing:
            if (ImGui::Button("Stop")) {
                mAviPlayState = APS_Stopped;
            } else {
                // 再生中。次のフレームの画像を読んで表示します。
                ++mPlayFrameNr;
                if (mAviReader.NumFrames() <= mPlayFrameNr) {
                    mPlayFrameNr = 0;
                }

                UpdateAviTexture();
            }
            break;
        case APS_PreInit:
            assert(0);
            break;
        }

        if (ImGui::Button("Close")) {
            mAviReader.Close();
            mAviPlayState = APS_PreInit;
            mState = S_Init;
        }
    }

    ImGui::End();
}

bool
MainApp::UpdateAviTexture(void)
{
    assert(mRenderImg.data == nullptr);

    if (mAviReader.NumFrames() == 0) {
        return false;
    }

    if (mPlayFrameNr < 0 || mAviReader.NumFrames() <= mPlayFrameNr) {
        return false;
    }

    int bytes = mAviReader.GetImage(mPlayFrameNr, mAviImgBufBytes, mAviImgBuf);
    if (bytes < 0) {
        return false;
    }

    const MLBitmapInfoHeader& bi = mAviReader.ImageFormat();

    if (bi.biCompression == 0 && bi.biBitCount == 24) {
        int rgbaBytes = bi.biWidth * bi.biHeight * 4;
        uint8_t* rgbaBuf = new uint8_t[rgbaBytes];

        mRenderImg.Init(bi.biWidth, bi.biHeight, MLImage2::IFFT_BMP, MLImage2::BFT_UIntR8G8B8A8,
                ML_CG_Rec709, MLImage2::MLG_G22, 8, 3, bytes, rgbaBuf);
        mConverter.B8G8R8DIBToR8G8B8A8(mAviImgBuf, (uint32_t*)rgbaBuf, bi.biWidth, bi.biHeight, 0xff);
    } else if (bi.biCompression == MLFOURCC_r210) {
        int rgbaBytes = bi.biWidth * bi.biHeight * 4;
        uint8_t* rgbaBuf = new uint8_t[rgbaBytes];

        mRenderImg.Init(bi.biWidth, bi.biHeight, MLImage2::IFFT_BMP, MLImage2::BFT_UIntR10G10B10A2,
                ML_CG_Rec2020, MLImage2::MLG_ST2084, 10, 3, bytes, rgbaBuf);
        mConverter.Rgb10bitToR10G10B10A2((const uint32_t*)mAviImgBuf, (uint32_t*)rgbaBuf, bi.biWidth, bi.biHeight, 0xff);
    } else if (bi.biCompression == MLFOURCC_v210) {
        int rgbaBytes = bi.biWidth * bi.biHeight * 4;
        uint8_t* rgbaBuf = new uint8_t[rgbaBytes];

        mRenderImg.Init(bi.biWidth, bi.biHeight, MLImage2::IFFT_BMP, MLImage2::BFT_UIntR10G10B10A2,
                ML_CG_Rec2020, MLImage2::MLG_ST2084, 10, 3, bytes, rgbaBuf);
        mConverter.Yuv422_10bitToR10G10B10A2(
                MLConverter::CS_Rec2020, (const uint32_t*)mAviImgBuf, (uint32_t*)rgbaBuf, bi.biWidth, bi.biHeight, 0xff);
    } else {
        assert(0);
    }

    return true;
}
void
MainApp::ImGuiCommands(void)
{
    if (mShowImGui) {
        //ImGui::ShowDemoWindow();

        // 順番が重要。キャプチャー画像を保存するため。
        ShowVideoCaptureWindow();
        ShowAviPlaybackWindow();
        ShowImageFileRWWindow();
        ShowSettingsWindow();
    } else {

        // キャプチャー画像を更新。
        switch (mVCState) {
        case VCS_CapturePreview:
        case VCS_Recording:
            UpdateCaptureImg();
            break;
        default:
            break;
        }

        // AVIの次のフレームの画像を読んで表示。
        switch (mAviPlayState) {
        case APS_Playing:
            // 再生中。次のフレームの画像を読んで表示します。
            ++mPlayFrameNr;
            if (mAviReader.NumFrames() <= mPlayFrameNr) {
                mPlayFrameNr = 0;
            }

            UpdateAviTexture();
            break;
        default:
            break;
        }
    }

    ImGui::Render();
}

bool
MainApp::UpdateImgTexture(void)
{
    mMutex.lock();

    if (mRenderImg.data == nullptr) {
        // GPUにアップロードするテクスチャが無い。
        //OutputDebugString(L"Upload Image Not Available\n");
        mMutex.unlock();
        return false;
    }

    mMutex.unlock();

    int uploadTexIdx = !mTexImgIdx;
    UploadImgToGpu(mRenderImg, mTexImg[uploadTexIdx],
        (TextureEnum)(TCE_TEX_IMG0 + uploadTexIdx));

    mRenderImg.DeleteData();
    mTexImgIdx = uploadTexIdx;

    return true;
}

void
MainApp::UploadImgToGpu(MLImage2& ci, ComPtr<ID3D12Resource>& tex, int texIdx)
{
    DXGI_FORMAT pixelFormat;
    int         pixelBytes;

    switch (ci.bitFormat) {
    case MLImage2::BFT_UIntR8G8B8A8:
        pixelFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        pixelBytes = 4;
        break;
    case MLImage2::BFT_UIntR10G10B10A2:
        pixelFormat = DXGI_FORMAT_R10G10B10A2_UNORM;
        pixelBytes = 4;
        break;
    case MLImage2::BFT_UIntR16G16B16A16:
        pixelFormat = DXGI_FORMAT_R16G16B16A16_UNORM;
        pixelBytes = 8;
        break;
    case MLImage2::BFT_HalfFloatR16G16B16A16:
        pixelFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
        pixelBytes = 8;
        break;
    default:
        assert(0);
        break;
    }

    ThrowIfFailed(mCmdAllocatorTexUpload->Reset());
    ThrowIfFailed(mCmdListTexUpload->Reset(mCmdAllocatorTexUpload.Get(), mPipelineState.Get()));

    if (tex.Get() == nullptr
        || ci.width != tex->GetDesc().Width
        || ci.height != tex->GetDesc().Height
        || pixelFormat != tex->GetDesc().Format) {
        // テクスチャの種類が違うので、作り直します。
        // 中でInternalRelease()される。
        tex = nullptr;

        D3D12_RESOURCE_DESC texDesc = {};
        texDesc.MipLevels = 1;
        texDesc.Format = pixelFormat;
        texDesc.Width = ci.width;
        texDesc.Height = ci.height;
        texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        texDesc.DepthOrArraySize = 1;
        texDesc.SampleDesc.Count = 1;
        texDesc.SampleDesc.Quality = 0;
        texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

        const CD3DX12_HEAP_PROPERTIES heapType_Default = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        ThrowIfFailed(mDevice->CreateCommittedResource(
            &heapType_Default,
            D3D12_HEAP_FLAG_NONE,
            &texDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&tex)));
        NAME_D3D12_OBJECT(tex);

        CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(mCbvSrvDescHeap->GetCPUDescriptorHandleForHeapStart());
        srvHandle.Offset(texIdx, mDescHandleIncrementSz);

        // Describe and create a SRV for the texture.
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = texDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        mDevice->CreateShaderResourceView(tex.Get(), &srvDesc, srvHandle);
    } else {
        const CD3DX12_RESOURCE_BARRIER barrier_Transision_PS_to_CopyDest
            = CD3DX12_RESOURCE_BARRIER::Transition(tex.Get(),
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
        mCmdListTexUpload->ResourceBarrier(1, &barrier_Transision_PS_to_CopyDest);
    }

    // GPUテクスチャメモリに画像をアップロードします。
    // texUploadHeapがスコープから外れる前にcommandListを実行しなければならない。
    ComPtr<ID3D12Resource> texUploadHeap;
    {
        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(tex.Get(), 0, 1);

        // Create the GPU upload buffer.
        const CD3DX12_HEAP_PROPERTIES heapType_Upload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        const CD3DX12_RESOURCE_DESC resDesc_UploadByfSz = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
        ThrowIfFailed(mDevice->CreateCommittedResource(
            &heapType_Upload,
            D3D12_HEAP_FLAG_NONE,
            &resDesc_UploadByfSz,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&texUploadHeap)));

        D3D12_SUBRESOURCE_DATA textureData = {};
        textureData.pData = &ci.data[0];
        textureData.RowPitch = ci.width * pixelBytes;
        textureData.SlicePitch = textureData.RowPitch * ci.height;
        UpdateSubresources(mCmdListTexUpload.Get(), tex.Get(), texUploadHeap.Get(), 0, 0, 1, &textureData);
        const CD3DX12_RESOURCE_BARRIER barrier_Transision_CopyDest_to_PS
            = CD3DX12_RESOURCE_BARRIER::Transition(tex.Get(),
                D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        mCmdListTexUpload->ResourceBarrier(1, &barrier_Transision_CopyDest_to_PS);
    }

    ThrowIfFailed(mCmdListTexUpload->Close());
    ID3D12CommandList* ppCommandLists[] = { mCmdListTexUpload.Get() };
    mCmdQ->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
    WaitForGpu();
}

