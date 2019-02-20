#include "MLDX12App.h"
#include "DXSampleHelper.h"
#include "WinApp.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "MLAviCommon.h"
#include "MLDX12Common.h"

// D3D12HelloFrameBuffering sample
//*********************************************************
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//*********************************************************

MLDX12App::MLDX12App(UINT width, UINT height):
    MLDX12(width, height),
    mState(S_Init),
    mCaptureDrawMode(MLImage::IM_RGB),
    mCrosshairType(MLDrawings::CH_None),
    mGridType(MLDrawings::GR_None),
    mTitleSafeArea(false),
    mFrameIdx(0),
    mIdToShowCapVideoTex(0),
    mViewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
    mScissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
    mFenceValues{},
    mRtvDescSize(0),
    mNumVertices(0),
    mWindowedMode(true),
    mFrameSkipCount(0),
    mDrawGamma(2.2f),
    mDrawGainR(1.0f),
    mDrawGainG(1.0f),
    mDrawGainB(1.0f),
    mPlayFrameNr(0),
    mPlayAlpha(0.0f),
    mPlayDrawMode(MLImage::IM_YUV),
    mIdToShowPlayVideoTex(0)
{
    strcpy_s(mWritePath, "c:/data/output.avi");
    strcpy_s(mReadPath, "c:/data/output.avi");
    mCaptureMsg[0] = 0;
    mPlayMsg[0] = 0;

    mConverter.CreateGammaTable(2.2f, 1.0f, 1.0f, 1.0f);

    // YUV 10bit v210
    mPlayBufferBytes = 3840 * 2160 * 8 / 3;
    mPlayBuffer = new uint8_t[mPlayBufferBytes];

    {
        const int imgBytes = 3840 * 2160 * 4;
        uint8_t *buff = new uint8_t[imgBytes];
        memset(buff, 0, imgBytes);
        mPlayImage.Init(3840, 2160, MLImage::IM_RGB, "", imgBytes, buff);
    }
}

MLDX12App::~MLDX12App(void) {
    mPlayImage.Term();

    delete[] mPlayBuffer;
    mPlayBuffer = nullptr;
}

void
MLDX12App::OnInit(void) {
    mVideoCaptureDeviceList.Init();

    //OutputDebugString(L"OnInit started\n");

    LoadPipeline();

    {
        ThrowIfFailed(mDevice->CreateFence(mFenceValues[mFrameIdx],
            D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));
        NAME_D3D12_OBJECT(mFence);
        mFenceValues[mFrameIdx]++;
        mFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (mFenceEvent == nullptr) {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }
    }

    LoadAssets();

    mDx12Imgui.Init(mDevice.Get());
    CreateImguiTexture();

    //OutputDebugString(L"OnInit end\n");
}

void
MLDX12App::OnDestroy(void) {
    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    WaitForGpu();

    CloseHandle(mFenceEvent);

    mVideoCapture.SetCallback(nullptr);
    mVideoCapture.Term();

    mVideoCaptureDeviceList.Term();

    for (auto ite = mCapturedImages.begin(); ite != mCapturedImages.end(); ++ite) {
        auto & item = *ite;
        delete[] item.data;
        item.data = nullptr;
    }
    mCapturedImages.clear();

    mDx12Imgui.Term();
}

void
MLDX12App::LoadPipeline(void) {
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

    ThrowIfFailed(D3D12CreateDevice(
        hardwareAdapter.Get(),
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&mDevice)
    ));
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ThrowIfFailed(mDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCmdQ)));
    NAME_D3D12_OBJECT(mCmdQ);

    DXGI_SWAP_CHAIN_DESC1 scDesc = {};
    scDesc.BufferCount = FrameCount;
    scDesc.Width = mWidth;
    scDesc.Height = mHeight;
    scDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scDesc.SampleDesc.Count = 1;
    ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(
        mCmdQ.Get(),
        WinApp::GetHwnd(),
        &scDesc,
        nullptr,
        nullptr,
        &swapChain));

    ThrowIfFailed(swapChain.As(&mSwapChain));

    mFrameIdx = mSwapChain->GetCurrentBackBufferIndex();

    {
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = FrameCount;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(mDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&mRtvHeap)));
        mRtvDescSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        NAME_D3D12_OBJECT(mRtvHeap);
    }
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
        for (UINT n = 0; n < FrameCount; n++) {
            ThrowIfFailed(mSwapChain->GetBuffer(n, IID_PPV_ARGS(&mRenderTargets[n])));
            mDevice->CreateRenderTargetView(mRenderTargets[n].Get(), nullptr, rtvHandle);
            rtvHandle.Offset(1, mRtvDescSize);
            NAME_D3D12_OBJECT_INDEXED(mRenderTargets, n);
        }
    }

    {
        // Describe and create a shader resource view (SRV) heap for the texture.
        D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
        srvHeapDesc.NumDescriptors = TE_NUM;
        srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(mDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvHeap)));
        NAME_D3D12_OBJECT(mSrvHeap);
        mSrvDescSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

    {
        for (UINT n = 0; n < FrameCount; n++) {
            ThrowIfFailed(
                mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                IID_PPV_ARGS(&mCmdAllocators[n])));
            NAME_D3D12_OBJECT_INDEXED(mCmdAllocators, n);
        }

        ThrowIfFailed(
            mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&mCmdAllocatorTexUpload)));
        NAME_D3D12_OBJECT(mCmdAllocatorTexUpload);

        ThrowIfFailed(mDevice->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT, mCmdAllocatorTexUpload.Get(),
            mPipelineStateRGB.Get(), IID_PPV_ARGS(&mCmdListTexUpload)));
        NAME_D3D12_OBJECT(mCmdListTexUpload);
        mCmdListTexUpload->Close();
    }
}

void
MLDX12App::LoadAssets(void) {
    {
        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
        if (FAILED(mDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData)))) {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }

        CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

        CD3DX12_ROOT_PARAMETER1 rootParameters[1];
        rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);

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
        ThrowIfFailed(mDevice->CreateRootSignature(0, signature->GetBufferPointer(),
            signature->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));
        NAME_D3D12_OBJECT(mRootSignature);
    }

    MLDX12Common::SetupPSO(mDevice.Get(), mRootSignature.Get(), L"shadersRGB.hlsl", mPipelineStateRGB);
    MLDX12Common::SetupPSO(mDevice.Get(), mRootSignature.Get(), L"shadersYUV.hlsl", mPipelineStateYUV);
    NAME_D3D12_OBJECT(mPipelineStateRGB);
    NAME_D3D12_OBJECT(mPipelineStateYUV);

    ThrowIfFailed(mDevice->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT, mCmdAllocators[mFrameIdx].Get(),
        mPipelineStateRGB.Get(), IID_PPV_ARGS(&mCmdList)));
    NAME_D3D12_OBJECT(mCmdList);

    {
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

        ThrowIfFailed(mDevice->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(vbBytes),
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

    ThrowIfFailed(mCmdList->Close());
    ID3D12CommandList* ppCommandLists[] = {mCmdList.Get()};
    mCmdQ->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
    WaitForGpu();

    {
        int texW = 3840;
        int texH = 2160;
        uint32_t *buff = new uint32_t[texW * texH];

        memset(buff, 0x80, texW*texH * 4);

        CreateVideoTexture(mTexCapturedVideo[0], TE_CAPVIDEO0, texW, texH, DXGI_FORMAT_R8G8B8A8_UNORM, 4, (uint8_t*)buff);
        CreateVideoTexture(mTexCapturedVideo[1], TE_CAPVIDEO1, texW, texH, DXGI_FORMAT_R8G8B8A8_UNORM, 4, (uint8_t*)buff);
        CreateVideoTexture(mTexPlayVideo[0], TE_PLAYVIDEO0, texW, texH, DXGI_FORMAT_R8G8B8A8_UNORM, 4, (uint8_t*)buff);
        CreateVideoTexture(mTexPlayVideo[1], TE_PLAYVIDEO1, texW, texH, DXGI_FORMAT_R8G8B8A8_UNORM, 4, (uint8_t*)buff);

        delete[] buff;
    }
}

void
MLDX12App::CreateVideoTexture(ComPtr<ID3D12Resource> &tex, int texIdx, int w, int h, DXGI_FORMAT fmt, int pixelBytes, uint8_t *data) {
    ThrowIfFailed(mCmdAllocators[mFrameIdx].Get()->Reset());
    ThrowIfFailed(mCmdList->Reset(mCmdAllocators[mFrameIdx].Get(), mPipelineStateRGB.Get()));

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

        ThrowIfFailed(mDevice->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &texDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&tex)));
        NAME_D3D12_OBJECT(tex);

        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(tex.Get(), 0, 1);

        ThrowIfFailed(mDevice->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&texUploadHeap)));

        D3D12_SUBRESOURCE_DATA textureData = {};
        textureData.pData = &data[0];
        textureData.RowPitch = w * pixelBytes;
        textureData.SlicePitch = textureData.RowPitch * h;

        UpdateSubresources(mCmdList.Get(), tex.Get(), texUploadHeap.Get(), 0, 0, 1, &textureData);
        mCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(tex.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

        CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(mSrvHeap->GetCPUDescriptorHandleForHeapStart());
        srvHandle.Offset(texIdx, mSrvDescSize);

        // Describe and create a SRV for the texture.
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = texDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        mDevice->CreateShaderResourceView(tex.Get(), &srvDesc, srvHandle);
    }

    ThrowIfFailed(mCmdList->Close());
    ID3D12CommandList* ppCommandLists[] = {mCmdList.Get()};
    mCmdQ->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
    WaitForGpu();
}



void
MLDX12App::CreateImguiTexture(void) {
    ThrowIfFailed(mCmdAllocators[mFrameIdx].Get()->Reset());
    ThrowIfFailed(mCmdList->Reset(mCmdAllocators[mFrameIdx].Get(), mPipelineStateRGB.Get()));

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

    ThrowIfFailed(mDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&mTexImgui)));
    NAME_D3D12_OBJECT(mTexImgui);

    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(mTexImgui.Get(), 0, 1);

    // Create the GPU upload buffer.
    ThrowIfFailed(mDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&texUploadHeap)));

    D3D12_SUBRESOURCE_DATA textureData = {};
    textureData.pData = &pixels[0];
    textureData.RowPitch = width * pixelBytes;
    textureData.SlicePitch = textureData.RowPitch * height;

    UpdateSubresources(mCmdList.Get(), mTexImgui.Get(), texUploadHeap.Get(), 0, 0, 1, &textureData);
    mCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mTexImgui.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

    ThrowIfFailed(mCmdList->Close());
    ID3D12CommandList* ppCommandLists[] = {mCmdList.Get()};
    mCmdQ->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
    WaitForGpu();

    // Create font texture view
    CD3DX12_CPU_DESCRIPTOR_HANDLE srvCpuHandle(mSrvHeap->GetCPUDescriptorHandleForHeapStart());
    srvCpuHandle.Offset(TE_IMGUI, mSrvDescSize);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    mDevice->CreateShaderResourceView(mTexImgui.Get(), &srvDesc, srvCpuHandle);

    // Store our identifier
    CD3DX12_GPU_DESCRIPTOR_HANDLE srvGpuHandle(mSrvHeap->GetGPUDescriptorHandleForHeapStart());
    srvGpuHandle.Offset(TE_IMGUI, mSrvDescSize);
    io.Fonts->TexID = (ImTextureID)srvGpuHandle.ptr;
}

void
MLDX12App::OnKeyUp(int key) {
    switch (key) {
    case VK_SPACE:
        {
            BOOL fullscreenState;
            ThrowIfFailed(mSwapChain->GetFullscreenState(&fullscreenState, nullptr));
            if (FAILED(mSwapChain->SetFullscreenState(!fullscreenState, nullptr))) {
                // Transitions to fullscreen mode can fail when running apps over
                // terminal services or for some other unexpected reason.  Consider
                // notifying the user in some way when this happens.
                OutputDebugString(L"Fullscreen transition failed");
                assert(false);
            }
            break;
        }
    default:
        break;
    }
}

void
MLDX12App::OnSizeChanged(int width, int height, bool minimized) {
    char s[256];
    sprintf_s(s, "\n\nOnSizeChanged(%d %d)\n", width, height);
    OutputDebugStringA(s);

    if ((width != Width() || height != Height()) && !minimized) {
        WaitForGpu();

        for (UINT n = 0; n < FrameCount; n++) {
            mRenderTargets[n].Reset();
            mFenceValues[n] = mFenceValues[mFrameIdx];
        }

        DXGI_SWAP_CHAIN_DESC desc = {};
        mSwapChain->GetDesc(&desc);
        ThrowIfFailed(mSwapChain->ResizeBuffers(FrameCount, width, height, desc.BufferDesc.Format, desc.Flags));

        BOOL fullscreenState;
        ThrowIfFailed(mSwapChain->GetFullscreenState(&fullscreenState, nullptr));
        mWindowedMode = !fullscreenState;

        mFrameIdx = mSwapChain->GetCurrentBackBufferIndex();

        UpdateForSizeChange(width, height);

        LoadSizeDependentResources();
    }
}

void
MLDX12App::UpdateViewAndScissor(void) {
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
MLDX12App::LoadSizeDependentResources(void) {
    UpdateViewAndScissor();

    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
        for (UINT n = 0; n < FrameCount; n++) {
            ThrowIfFailed(mSwapChain->GetBuffer(n, IID_PPV_ARGS(&mRenderTargets[n])));
            mDevice->CreateRenderTargetView(mRenderTargets[n].Get(), nullptr, rtvHandle);
            rtvHandle.Offset(1, mRtvDescSize);

            NAME_D3D12_OBJECT_INDEXED(mRenderTargets, n);
        }
    }
}

void
MLDX12App::OnUpdate(void) {}

void
MLDX12App::OnRender(void) {
    PopulateCommandList();

    ID3D12CommandList* ppCommandLists[] = {mCmdList.Get()};
    mCmdQ->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // texture upload
    bool bUpCap = UpdateCapturedVideoTexture();
    if (!bUpCap) {
        // テクスチャアップロードのための資源が1個しかないので。
        UpdatePlayVideoTexture();
    }

    ThrowIfFailed(mSwapChain->Present(1, 0));
    MoveToNextFrame();
}

void
MLDX12App::DrawFullscreenTexture(TextureEnum texId, MLImage::ImageMode drawMode) {
    switch (drawMode) {
    case MLImage::IM_RGB:
        mCmdList->SetPipelineState(mPipelineStateRGB.Get());
        break;
    case MLImage::IM_YUV:
        mCmdList->SetPipelineState(mPipelineStateYUV.Get());
        break;
    default:
        assert(0);
        break;
    }

    mCmdList->SetGraphicsRootSignature(mRootSignature.Get());

    ID3D12DescriptorHeap* ppHeaps[] = {mSrvHeap.Get()};
    mCmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    CD3DX12_GPU_DESCRIPTOR_HANDLE dh(mSrvHeap->GetGPUDescriptorHandleForHeapStart());
    dh.Offset(texId, mSrvDescSize);
    mCmdList->SetGraphicsRootDescriptorTable(0, dh);

    mCmdList->RSSetViewports(1, &mViewport);
    mCmdList->RSSetScissorRects(1, &mScissorRect);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
        mFrameIdx, mRtvDescSize);
    mCmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    mCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    mCmdList->IASetVertexBuffers(0, 1, &mVertexBufferView);
    mCmdList->DrawInstanced(mNumVertices, 1, 0, 0);
}

void
MLDX12App::PopulateCommandList(void) {
    ThrowIfFailed(mCmdAllocators[mFrameIdx]->Reset());
    ThrowIfFailed(mCmdList->Reset(mCmdAllocators[mFrameIdx].Get(), mPipelineStateRGB.Get()));

    mCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        mRenderTargets[mFrameIdx].Get(), D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET));

    {   // clear screen
        const float clearColorRGBA[] = {0.4f, 0.4f, 0.4f, 1.0f};
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
            mFrameIdx, mRtvDescSize);
        mCmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
        mCmdList->ClearRenderTargetView(rtvHandle, clearColorRGBA, 0, nullptr);
    }

    // 全クライアント領域を覆う矩形にテクスチャを貼って描画。
    DrawFullscreenTexture(
        (TextureEnum)(TE_CAPVIDEO0 + mIdToShowCapVideoTex),
        mCaptureDrawMode);

    if (0.0f < mPlayAlpha) {
        DrawFullscreenTexture(
            (TextureEnum)(TE_PLAYVIDEO0 + mIdToShowPlayVideoTex),
            mPlayDrawMode);
    }

    // Start the Dear ImGui frame
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGuiCommands();
    mDx12Imgui.Render(ImGui::GetDrawData(), mCmdList.Get(), mFrameIdx);

    // Indicate that the back buffer will now be used to present.
    mCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        mRenderTargets[mFrameIdx].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT));

    ThrowIfFailed(mCmdList->Close());
}

void
MLDX12App::MoveToNextFrame(void) {
    const UINT64 currentFenceValue = mFenceValues[mFrameIdx];
    ThrowIfFailed(mCmdQ->Signal(mFence.Get(), currentFenceValue));
    mFrameIdx = mSwapChain->GetCurrentBackBufferIndex();

    if (mFence->GetCompletedValue() < mFenceValues[mFrameIdx]) {
        ThrowIfFailed(mFence->SetEventOnCompletion(mFenceValues[mFrameIdx], mFenceEvent));

        WaitForSingleObjectEx(mFenceEvent, INFINITE, FALSE);
    }

    mFenceValues[mFrameIdx] = currentFenceValue + 1;
}

void
MLDX12App::WaitForGpu(void) {
    ThrowIfFailed(mCmdQ->Signal(mFence.Get(), mFenceValues[mFrameIdx]));
    ThrowIfFailed(mFence->SetEventOnCompletion(mFenceValues[mFrameIdx], mFenceEvent));
    WaitForSingleObjectEx(mFenceEvent, INFINITE, FALSE);
    mFenceValues[mFrameIdx]++;
}

void
MLDX12App::ShowCaptureWindow(void) {
    ImGui::Begin("Capture Settings");

    if (ImGui::BeginPopupModal("ErrorCapturePopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text(mCaptureMsg);
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
            mCaptureMsg[0] = 0;
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("WriteFlushPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text(mCaptureMsg);

        if (mState != S_WaitRecordEnd) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::Text("ALT+Enter to toggle fullscreen.");
    ImGui::Text("Average redraw frames per second : %.1f", ImGui::GetIO().Framerate);

    static int selectedDeviceIdx = 0;

    switch (mState) {
    case S_Init:
        ImGui::Text("Video capture device list:");
        for (int i = 0; i < mVideoCaptureDeviceList.NumOfDevices(); ++i) {
            auto name = mVideoCaptureDeviceList.DeviceName(i);
            ImGui::RadioButton(name.c_str(), &selectedDeviceIdx, 0);
        }
        if (mVideoCaptureDeviceList.NumOfDevices() == 0) {
            ImGui::TextColored(ImColor(255, 100, 100), "No video capture device found.");
        } else if (ImGui::Button("Start Capture")) {
            mFrameSkipCount = 0;

            IDeckLink *p = mVideoCaptureDeviceList.Device(selectedDeviceIdx);
            mVideoCapture.Init(p);
            mVideoCapture.SetCallback(this);
            bool brv = mVideoCapture.StartCapture(0);
            if (brv) {
                mState = S_Previewing;
            }
        }
        break;
    case S_WaitRecordEnd:
        {
            sprintf_s(mCaptureMsg, "Now Writing AVI...\nRemaining %d frames.", mAviWriter.RecQueueSize());
            mMutex.lock();
            bool bEnd = mAviWriter.PollThreadEnd();
            mMutex.unlock();
            if (bEnd) {
                mState = S_Previewing;
            }
        }
        break;
    case S_Recording:
    case S_Previewing:
        {
            int queueSize = 0;
            mMutex.lock();
            if (!mCapturedImages.empty()) {
                queueSize = (int)mCapturedImages.size();
                auto & front = mCapturedImages.front();
                ImGui::Text(front.imgFormat.c_str());
            } else {
                ImGui::Text("");
            }
            mMutex.unlock();

            BMDTimeScale ts = mVideoCapture.FrameRateTS();
            BMDTimeValue tv = mVideoCapture.FrameRateTV();
            ImGui::Text("Frame rate : %.1f", (double)ts / tv);

            ImGui::Separator();

            if (mState == S_Previewing) {
                ImGui::Text("Now Previewing...");
                BMDPixelFormat pixFmt = mVideoCapture.PixelFormat();
                if (bmdFormat10BitYUV == pixFmt) {
                    ImGui::InputText("Record filename", mWritePath, sizeof mWritePath - 1);
                    if (ImGui::Button("Record", ImVec2(256, 64))) {
                        wchar_t path[512];
                        memset(path, 0, sizeof path);
                        MultiByteToWideChar(CP_UTF8, 0, mWritePath, sizeof mWritePath, path, 511);

                        bool bRv = mAviWriter.Start(path, mVideoCapture.Width(),
                            mVideoCapture.Height(), (int)(ts / 1000),
                            MLIF_YUV422v210, true);
                        if (bRv) {
                            mState = S_Recording;
                            mCaptureMsg[0] = 0;
                        } else {
                            sprintf_s(mCaptureMsg, "Record Failed.\nFile open error : %s", mWritePath);
                            ImGui::OpenPopup("ErrorCapturePopup");
                        }
                    }
                }

                if (ImGui::Button("Stop Capture")) {

                    mVideoCapture.StopCapture();
                    mVideoCapture.Term();

                    mVideoCaptureDeviceList.Term();
                    mVideoCaptureDeviceList.Init();

                    mState = S_Init;
                }
            } else if (mState == S_Recording) {
                ImGui::Text("Now Recording...");
                ImGui::Text("Record filename : %s", mWritePath);
                {
                    // hour:min:sec:frameを算出。
                    auto vt = MLFrameNrToTime(mAviWriter.FramesPerSec(), mAviWriter.TotalVideoFrames());
                    ImGui::Text("%02d:%02d:%02d:%02d",
                        vt.hour, vt.min, vt.sec, vt.frame);
                }
                if (ImGui::Button("Stop Recording", ImVec2(256, 64))) {
                    mState = S_WaitRecordEnd;

                    mMutex.lock();
                    mAviWriter.StopAsync();
                    mMutex.unlock();

                    ImGui::OpenPopup("WriteFlushPopup");
                }

                ImGui::Text("Rec Queue size : %d", mAviWriter.RecQueueSize());
            }

            if (mVideoCapture.Width() == 3840 && mVideoCapture.Height() == 2160 && mVideoCapture.PixelFormat() == bmdFormat10BitYUV) {
                // Maybe Blackmagic Micro Studio Camera 4K...
                ImGui::Checkbox("Raw SDI preview", &mRawSDI);
                if (mRawSDI) {
                    ImGui::DragFloat("Preview Gamma", &mDrawGamma, 0.01f, 0.4f, 4.0f);
                    ImGui::DragFloat("Preview Gain R", &mDrawGainR, 0.01f, 0.5f, 2.0f);
                    ImGui::DragFloat("Preview Gain G", &mDrawGainG, 0.01f, 0.5f, 2.0f);
                    ImGui::DragFloat("Preview Gain B", &mDrawGainB, 0.01f, 0.5f, 2.0f);

                    mConverter.CreateGammaTable(mDrawGamma, mDrawGainR, mDrawGainG, mDrawGainB);
                }
            }


            ImGui::Text("Draw Queue size : %d", queueSize);
            ImGui::SameLine();
            if (ImGui::Button("Clear Draw Queue")) {
                ClearDrawQueue();
            }
            ImGui::Text("Frame Draw skip count : %lld", mFrameSkipCount);

            if (ImGui::Button("Flush streams")) {
                mVideoCapture.FlushStreams();
            }

            ImGui::BeginGroup();
            if (ImGui::RadioButton("Center Crosshair", mCrosshairType == MLDrawings::CH_CenterCrosshair)) {
                mCrosshairType = MLDrawings::CH_CenterCrosshair;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("4 Crosshairs", mCrosshairType == MLDrawings::CH_4Crosshairs)) {
                mCrosshairType = MLDrawings::CH_4Crosshairs;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("CHNone", mCrosshairType == MLDrawings::CH_None)) {
                mCrosshairType = MLDrawings::CH_None;
            }
            ImGui::EndGroup();

            ImGui::Checkbox("Title safe area", &mTitleSafeArea);

            ImGui::BeginGroup();
            if (ImGui::RadioButton("3x3 Grid", mGridType == MLDrawings::GR_3x3)) {
                mGridType = MLDrawings::GR_3x3;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("6x6 Grid", mGridType == MLDrawings::GR_6x6)) {
                mGridType = MLDrawings::GR_6x6;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("GRNone", mGridType == MLDrawings::GR_None)) {
                mGridType = MLDrawings::GR_None;
            }
            ImGui::EndGroup();
        }
        break;
    default:
        assert(0);
        break;
    }

    ImGui::End();
}

void
MLDX12App::ShowPlaybackWindow(void) {
    ImGui::Begin("Playback Control");

    if (ImGui::BeginPopupModal("ErrorPlayPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text(mPlayMsg);
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
            mPlayMsg[0] = 0;
        }
        ImGui::EndPopup();
    }

    ImGui::InputText("Read filename", mReadPath, sizeof mReadPath - 1);

    if (mAviReader.NumFrames() <= 0) {
        if (ImGui::Button("Open")) {
            wchar_t path[512];
            memset(path, 0, sizeof path);
            MultiByteToWideChar(CP_UTF8, 0, mReadPath, sizeof mReadPath, path, 511);
            if (mAviReader.Open(path)) {
                const MLBitmapInfoHeader &bi = mAviReader.ImageFormat();

                if (bi.biCompression != MLFOURCC_v210) {
                    sprintf_s(mPlayMsg, "Error: Not supported AVI format : %s", mReadPath);
                    ImGui::OpenPopup("ErrorPlayPopup");
                    mAviReader.Close();
                } else {
                    // success
                    mPlayMsg[0] = 0;
                    mPlayAlpha = 1.0f;
                    mPlayFrameNr = 0;
                }
            } else {
                sprintf_s(mPlayMsg, "Read AVI Failed.\nFile open error : %s", mReadPath);
                ImGui::OpenPopup("ErrorPlayPopup");
            }
        }
    } else {
        if (ImGui::Button("Close")) {
            mAviReader.Close();
            mPlayAlpha = 0.0f;
        }
        ImGui::Text("%d x %d, %d fps, %.1f sec",
            mAviReader.ImageFormat().biWidth, mAviReader.ImageFormat().biHeight,
            mAviReader.FramesPerSec(), mAviReader.DurationSec());

        {
            // hour:min:sec:frameを算出。
            auto vt = MLFrameNrToTime(mAviReader.FramesPerSec(), mPlayFrameNr);

            ImGui::Text("%02d:%02d:%02d:%02d",
                vt.hour, vt.min, vt.sec, vt.frame);
        }

        // シークバー。
        ImGui::DragInt("Frame number", &mPlayFrameNr, 1.0f, 0, mAviReader.NumFrames() - 1);

        ImGui::DragFloat("Alpha", &mPlayAlpha, 0.01f, 0.0f, 1.0f);

        const MLBitmapInfoHeader &bi = mAviReader.ImageFormat();

        if (bi.biWidth == 3840 && bi.biHeight == 2160 && bi.biCompression == MLFOURCC_v210) {
            // Maybe Blackmagic Micro Studio Camera 4K...
            ImGui::Checkbox("Raw SDI preview", &mRawSDI);
            if (mRawSDI) {
                ImGui::DragFloat("Preview Gamma", &mDrawGamma, 0.01f, 0.4f, 4.0f);
                ImGui::DragFloat("Preview Gain R", &mDrawGainR, 0.01f, 0.5f, 2.0f);
                ImGui::DragFloat("Preview Gain G", &mDrawGainG, 0.01f, 0.5f, 2.0f);
                ImGui::DragFloat("Preview Gain B", &mDrawGainB, 0.01f, 0.5f, 2.0f);

                mConverter.CreateGammaTable(mDrawGamma, mDrawGainR, mDrawGainG, mDrawGainB);
            }
        }
    }

    ImGui::End();
}

void
MLDX12App::ImGuiCommands(void) {
    //ImGui::ShowDemoWindow();

    ShowCaptureWindow();
    ShowPlaybackWindow();

    ImGui::Render();
}

static const char *
BMDPixelFormatToStr(int a) {
    switch (a) {
    case bmdFormat8BitYUV: return "8bitYUV";
    case bmdFormat10BitYUV: return "10bitYUV";
    case bmdFormat8BitARGB: return "8bitARGB";
    case bmdFormat8BitBGRA: return "8bitRGBA";
    case bmdFormat10BitRGB: return "10bitRGB";
    case bmdFormat12BitRGB: return "12bitRGB";
    case bmdFormat12BitRGBLE: return "12bitRGBLE";
    case bmdFormat10BitRGBXLE: return "10bitRGBXLE";
    case bmdFormat10BitRGBX: return "10bitRGBX";
    case bmdFormatH265: return "H265";
    case bmdFormatDNxHR: return "DNxHR";
    case bmdFormat12BitRAWGRBG: return "12bitRAWGRBG";
    case bmdFormat12BitRAWJPEG: return "12bitRAWJPEG";
    default: return "unknown";
    }
}

void
MLDX12App::MLVideoCaptureCallback_VideoInputFrameArrived(
    IDeckLinkVideoInputFrame* videoFrame, IDeckLinkAudioInputPacket* audioPacket) {

    if (audioPacket != nullptr
        && mState == S_Recording) {
        // オーディオを記録する。
        void *audioBuff = nullptr;
        audioPacket->GetBytes(&audioBuff);

        int audioFrameCount = audioPacket->GetSampleFrameCount();
        if (0 < audioFrameCount) {
            /*
            char s[256];
            sprintf_s(s, "audio frame %d\n", audioFrameCount);
            OutputDebugStringA(s);
            */

            mMutex.lock();
            mAviWriter.AddAudio((const uint8_t*)audioBuff, audioFrameCount);
            mMutex.unlock();
        }
    }

    // Video

    void *buffer = nullptr;
    videoFrame->GetBytes(&buffer);

    int width = videoFrame->GetWidth();
    int height = videoFrame->GetHeight();
    int fmt = videoFrame->GetPixelFormat();
    int rowBytes = videoFrame->GetRowBytes();

    mMutex.lock();
    if (mState == S_Recording) {
        mAviWriter.AddImage((const uint8_t*)buffer, rowBytes*height);
    }
    mMutex.unlock();

    mMutex.lock();
    if (CapturedImageQueueSize <= mCapturedImages.size()) {
        // 詰まっているときは描画更新をスキップする。
        ++mFrameSkipCount;
        mMutex.unlock();
        return;
    }
    mMutex.unlock();

    char s[256];
    sprintf_s(s, "%dx%d %s", width, height, BMDPixelFormatToStr(fmt));
    int bytes = width * height * 4;

    MLImage ci;
    ci.imgFormat = s;
    ci.bytes = bytes;
    ci.width = width;
    ci.height = height;
    ci.imgMode = MLImage::IM_RGB;
    ci.data = new uint8_t[bytes];

    uint8_t alpha = 255;
    uint32_t *pFrom = (uint32_t *)buffer;
    uint32_t *pTo = (uint32_t *)ci.data;
    switch (fmt) {
    case bmdFormat10BitRGB:
        {
            MLConverter::Rgb10bitToRGBA(pFrom, pTo, width, height, alpha);
            ci.imgMode = MLImage::IM_RGB;
        }
        break;
    case bmdFormat10BitYUV:
        if (mRawSDI) {
            mConverter.RawYuvV210ToRGBA(pFrom, pTo, width, height, alpha);
            ci.imgMode = MLImage::IM_RGB;
        } else {
            MLConverter::YuvV210ToYuvA(pFrom, pTo, width, height, alpha);
            ci.imgMode = MLImage::IM_YUV;
        }
        break;
    default:
        break;
    }

    mDrawings.AddCrosshair(ci, mCrosshairType);
    if (mTitleSafeArea) {
        mDrawings.AddTitleSafeArea(ci);
    }
    mDrawings.AddGrid(ci, mGridType);

    mMutex.lock();
    mCapturedImages.push_back(ci);
    mMutex.unlock();
}

void
MLDX12App::ClearDrawQueue(void) {
    mMutex.lock();
    for (auto ite = mCapturedImages.begin(); ite != mCapturedImages.end(); ++ite) {
        MLImage &ci = *ite;
        delete[] ci.data;
        ci.data = nullptr;
    }
    mCapturedImages.clear();
    mMutex.unlock();
}

bool
MLDX12App::UpdateCapturedVideoTexture(void) {
    mMutex.lock();
    if (mCapturedImages.empty()) {
        mMutex.unlock();
        //OutputDebugString(L"Not Available\n");
        return false;
    }

    //char s[256];
    //sprintf_s(s, "Available %d\n", (int)m_capturedImages.size());
    //OutputDebugStringA(s);

    MLImage ci = mCapturedImages.front();
    mCapturedImages.pop_front();

    mMutex.unlock();

    UpdateVideoTexture(ci, mTexCapturedVideo[!mIdToShowCapVideoTex],
        (TextureEnum)(TE_CAPVIDEO0 + !mIdToShowCapVideoTex));

    mIdToShowCapVideoTex = !mIdToShowCapVideoTex;

    mCaptureDrawMode = ci.imgMode;
    delete[] ci.data;
    ci.data = nullptr;

    return true;
}

bool
MLDX12App::UpdatePlayVideoTexture(void) {
    if (mAviReader.NumFrames() == 0) {
        return false;
    }

    if (mPlayFrameNr < 0 || mAviReader.NumFrames() <= mPlayFrameNr) {
        return false;
    }

    int bytes = mAviReader.GetImage(mPlayFrameNr, mPlayBufferBytes, mPlayBuffer);
    if (bytes < 0) {
        return false;
    }

    const MLBitmapInfoHeader &bi = mAviReader.ImageFormat();

    MLImage &ci = mPlayImage;
    ci.width = bi.biWidth;
    ci.height = bi.biHeight;
    ci.bytes = bi.biWidth * bi.biHeight * 4;
    ci.imgFormat = "";
    ci.imgMode = MLImage::IM_RGB;

    uint8_t alpha = (uint8_t)(mPlayAlpha * 255.0f + 0.5f);

    uint32_t *pFrom = (uint32_t *)mPlayBuffer;
    uint32_t *pTo = (uint32_t *)ci.data;
    if (bi.biCompression == MLFOURCC_v210) {
        if (mRawSDI) {
            mConverter.RawYuvV210ToRGBA(pFrom, pTo, bi.biWidth, bi.biHeight, alpha);
            ci.imgMode = MLImage::IM_RGB;
        } else {
            MLConverter::YuvV210ToYuvA(pFrom, pTo, bi.biWidth, bi.biHeight, alpha);
            ci.imgMode = MLImage::IM_YUV;
        }
    } else {
        return false;
    }

    UpdateVideoTexture(ci, mTexPlayVideo[!mIdToShowPlayVideoTex],
        (TextureEnum)(TE_PLAYVIDEO0 + !mIdToShowPlayVideoTex));

    mPlayDrawMode = ci.imgMode;

    mIdToShowPlayVideoTex = !mIdToShowPlayVideoTex;
    return true;
}

void
MLDX12App::UpdateVideoTexture(MLImage &ci, ComPtr<ID3D12Resource> &tex, int texIdx) {
    assert(tex.Get());

    DXGI_FORMAT pixelFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    int         pixelBytes = 4;

    ThrowIfFailed(mCmdAllocatorTexUpload->Reset());
    ThrowIfFailed(mCmdListTexUpload->Reset(mCmdAllocatorTexUpload.Get(), mPipelineStateRGB.Get()));

    if (ci.width != tex->GetDesc().Width
        || ci.height != tex->GetDesc().Height) {
        // 中でInternalRelease()される。
        tex = nullptr;

        // サイズが変わった。
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

        ThrowIfFailed(mDevice->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &texDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&tex)));
        NAME_D3D12_OBJECT(tex);

        CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(mSrvHeap->GetCPUDescriptorHandleForHeapStart());
        srvHandle.Offset(texIdx, mSrvDescSize);

        // Describe and create a SRV for the texture.
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = texDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        mDevice->CreateShaderResourceView(tex.Get(), &srvDesc, srvHandle);
    } else {
        mCmdListTexUpload->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(tex.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST));
    }

    // texUploadHeapがスコープから外れる前にcommandListを実行しなければならない。
    ComPtr<ID3D12Resource> texUploadHeap;
    {
        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(tex.Get(), 0, 1);

        // Create the GPU upload buffer.
        ThrowIfFailed(mDevice->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&texUploadHeap)));

        D3D12_SUBRESOURCE_DATA textureData = {};
        textureData.pData = &ci.data[0];
        textureData.RowPitch = ci.width * pixelBytes;
        textureData.SlicePitch = textureData.RowPitch * ci.height;
        UpdateSubresources(mCmdListTexUpload.Get(), tex.Get(), texUploadHeap.Get(), 0, 0, 1, &textureData);
        mCmdListTexUpload->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(tex.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
    }

    ThrowIfFailed(mCmdListTexUpload->Close());
    ID3D12CommandList* ppCommandLists[] = {mCmdListTexUpload.Get()};
    mCmdQ->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
    WaitForGpu();
}

