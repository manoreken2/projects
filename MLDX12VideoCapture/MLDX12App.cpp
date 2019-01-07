#include "MLDX12App.h"
#include "DXSampleHelper.h"
#include "WinApp.h"
#include "imgui.h"
#include "imgui_impl_win32.h"

// D3D12HelloFrameBuffering sample
//*********************************************************
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//*********************************************************

static uint32_t
NtoHL(uint32_t v)
{
    return   ((v & 0xff) << 24) |
        (((v >> 8) & 0xff) << 16) |
        (((v >> 16) & 0xff) << 8) |
        (((v >> 24) & 0xff));
}

MLDX12App::MLDX12App(UINT width, UINT height) :
    MLDX12(width, height),
    m_state(S_Init),
    m_drawMode(DM_RGB),
    m_frameIndex(0),
    m_textureVideoIdToShow(0),
    m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
    m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
    m_fenceValues{},
    m_rtvDescriptorSize(0),
    m_windowedMode(true),
    m_frameSkipCount(0)
{
}

void MLDX12App::OnInit()
{
    m_videoCaptureDeviceList.Init();

    OutputDebugString(L"OnInit started\n");

    LoadPipeline();
    LoadAssets();

    m_dx12Imgui.Init(m_device.Get());
    CreateImguiTexture();

    OutputDebugString(L"OnInit end\n");
}

void MLDX12App::OnDestroy()
{
    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    WaitForGpu();

    CloseHandle(m_fenceEvent);

    m_videoCapture.SetCallback(nullptr);
    m_videoCapture.Term();

    m_videoCaptureDeviceList.Term();

    for (auto ite = m_capturedImages.begin(); ite != m_capturedImages.end(); ++ite) {
        auto & item = *ite;
        delete[] item.data;
        item.data = nullptr;
    }
    m_capturedImages.clear();

    m_dx12Imgui.Term();
}



void MLDX12App::LoadPipeline()
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

    ThrowIfFailed(D3D12CreateDevice(
        hardwareAdapter.Get(),
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&m_device)
    ));

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));
    NAME_D3D12_OBJECT(m_commandQueue);

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
        m_commandQueue.Get(),
        WinApp::GetHwnd(),
        &scDesc,
        nullptr,
        nullptr,
        &swapChain));

    ThrowIfFailed(swapChain.As(&m_swapChain));

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    {
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = FrameCount;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));
        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        NAME_D3D12_OBJECT(m_rtvHeap);
    }
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
        for (UINT n = 0; n < FrameCount; n++) {
            ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
            m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
            rtvHandle.Offset(1, m_rtvDescriptorSize);
            NAME_D3D12_OBJECT_INDEXED(m_renderTargets, n);
        }
    }

    {
        // Describe and create a shader resource view (SRV) heap for the texture.
        D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
        srvHeapDesc.NumDescriptors = TE_NUM;
        srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap)));
        NAME_D3D12_OBJECT(m_srvHeap);
        m_srvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

    {
        for (UINT n = 0; n < FrameCount; n++) {
            ThrowIfFailed(
                m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                    IID_PPV_ARGS(&m_commandAllocators[n])));
            NAME_D3D12_OBJECT_INDEXED(m_commandAllocators,n);
        }

        ThrowIfFailed(
            m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                IID_PPV_ARGS(&m_commandAllocatorTextureUpload)));
        NAME_D3D12_OBJECT(m_commandAllocatorTextureUpload);

    }
}

void MLDX12App::LoadAssets()
{
    {
        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
        if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData)))) {
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
        ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(),
            signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
        NAME_D3D12_OBJECT(m_rootSignature);
    }

    SetupPSO(L"shadersRGB.hlsl", m_pipelineStateRGB);
    SetupPSO(L"shadersYUV.hlsl", m_pipelineStateYUV);
    NAME_D3D12_OBJECT(m_pipelineStateRGB);
    NAME_D3D12_OBJECT(m_pipelineStateYUV);

    ThrowIfFailed(m_device->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[m_frameIndex].Get(),
        m_pipelineStateRGB.Get(), IID_PPV_ARGS(&m_commandList)));
    NAME_D3D12_OBJECT(m_commandList);

    ThrowIfFailed(m_device->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocatorTextureUpload.Get(),
        m_pipelineStateRGB.Get(), IID_PPV_ARGS(&m_commandListTextureUpload)));
    NAME_D3D12_OBJECT(m_commandListTextureUpload);

    {
        Vertex verts[] =
        {
            { { -1.0f, +1.0f, 0.0f }, { 0.0f, 0.0f } },
            { { +1.0f, +1.0f, 0.0f }, { 1.0f, 0.0f } },
            { { -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } },
            { { -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } },
            { { +1.0f, +1.0f, 0.0f }, { 1.0f, 0.0f } },
            { { +1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f } },
        };

        int numTriangles = 2;
        m_numVertices = 3 * numTriangles;

        const UINT vbBytes = sizeof verts;

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(vbBytes),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_vertexBuffer)));
        NAME_D3D12_OBJECT(m_vertexBuffer);

        UINT8* pVertexDataBegin;
        // We do not intend to read from this resource on the CPU.
        CD3DX12_RANGE readRange(0, 0);
        ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
        memcpy(pVertexDataBegin, verts, vbBytes);
        m_vertexBuffer->Unmap(0, nullptr);

        m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
        m_vertexBufferView.StrideInBytes = sizeof(Vertex);
        m_vertexBufferView.SizeInBytes = vbBytes;
    }

    {
        ThrowIfFailed(m_device->CreateFence(m_fenceValues[m_frameIndex],
            D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        NAME_D3D12_OBJECT(m_fence);
        m_fenceValues[m_frameIndex]++;
        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr) {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }

    }

    UpdateViewAndScissor();
    
    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
    WaitForGpu();

    {
        int texW = 3840;
        int texH = 2160;
        uint32_t *buff = new uint32_t[texW * texH];

        memset(buff, 0x80, texW*texH * 4);

        m_commandListTextureUpload->Close();

        CreateVideoTexture(0, texW, texH, DXGI_FORMAT_R8G8B8A8_UNORM, 4, (uint8_t*)buff);
        CreateVideoTexture(1, texW, texH, DXGI_FORMAT_R8G8B8A8_UNORM, 4, (uint8_t*)buff);

        delete[] buff;
    }

}

void MLDX12App::SetupPSO(const wchar_t *shaderName, ComPtr<ID3D12PipelineState> & pso)
{
    ComPtr<ID3DBlob> vertexShader;
    ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
    // Enable better shader debugging with the graphics debugging tools.
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compileFlags = 0;
#endif

    ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(shaderName).c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
    ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(shaderName).c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));

    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

#if 1
    // 普通のアルファーブレンディング。
    {
        D3D12_BLEND_DESC& desc = psoDesc.BlendState;
        desc.AlphaToCoverageEnable = false;
        desc.RenderTarget[0].BlendEnable = true;
        desc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        desc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
        desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }
#else
    // ブレンディングなしの上書き。
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
#endif
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso)));
}

void
MLDX12App::CreateVideoTexture(int texIdx, int w, int h, DXGI_FORMAT fmt, int pixelBytes, uint8_t *data)
{
    ThrowIfFailed(m_commandAllocatorTextureUpload->Reset());
    ThrowIfFailed(m_commandListTextureUpload->Reset(m_commandAllocatorTextureUpload.Get(), m_pipelineStateRGB.Get()));

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

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &texDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&m_textureVideo[texIdx])));
        NAME_D3D12_OBJECT_INDEXED(m_textureVideo, texIdx);

        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_textureVideo[texIdx].Get(), 0, 1);

        ThrowIfFailed(m_device->CreateCommittedResource(
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

        UpdateSubresources(m_commandListTextureUpload.Get(), m_textureVideo[texIdx].Get(), texUploadHeap.Get(), 0, 0, 1, &textureData);
        m_commandListTextureUpload->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_textureVideo[texIdx].Get(),
            D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

        CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(m_srvHeap->GetCPUDescriptorHandleForHeapStart());
        srvHandle.Offset(texIdx, m_srvDescriptorSize);
        
        // Describe and create a SRV for the texture.
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = texDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        m_device->CreateShaderResourceView(m_textureVideo[texIdx].Get(), &srvDesc, srvHandle);
    }

    ThrowIfFailed(m_commandListTextureUpload->Close());
    ID3D12CommandList* ppCommandLists[] = { m_commandListTextureUpload.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
    WaitForGpu();
}

void
MLDX12App::CreateImguiTexture(void)
{
    ThrowIfFailed(m_commandAllocatorTextureUpload->Reset());
    ThrowIfFailed(m_commandListTextureUpload->Reset(m_commandAllocatorTextureUpload.Get(), m_pipelineStateRGB.Get()));

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

    ThrowIfFailed(m_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&m_textureImgui)));
    NAME_D3D12_OBJECT(m_textureImgui);

    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_textureImgui.Get(), 0, 1);

    // Create the GPU upload buffer.
    ThrowIfFailed(m_device->CreateCommittedResource(
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

    UpdateSubresources(m_commandListTextureUpload.Get(), m_textureImgui.Get(), texUploadHeap.Get(), 0, 0, 1, &textureData);
    m_commandListTextureUpload->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_textureImgui.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

    ThrowIfFailed(m_commandListTextureUpload->Close());
    ID3D12CommandList* ppCommandLists[] = { m_commandListTextureUpload.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
    WaitForGpu();

    // Create font texture view
    CD3DX12_CPU_DESCRIPTOR_HANDLE srvCpuHandle(m_srvHeap->GetCPUDescriptorHandleForHeapStart());
    srvCpuHandle.Offset(TE_IMGUI, m_srvDescriptorSize);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    m_device->CreateShaderResourceView(m_textureImgui.Get(), &srvDesc, srvCpuHandle);

    // Store our identifier
    CD3DX12_GPU_DESCRIPTOR_HANDLE srvGpuHandle(m_srvHeap->GetGPUDescriptorHandleForHeapStart());
    srvGpuHandle.Offset(TE_IMGUI, m_srvDescriptorSize);
    io.Fonts->TexID = (ImTextureID)srvGpuHandle.ptr;
}



void MLDX12App::OnKeyUp(int key)
{
    switch (key) {
    case VK_SPACE:
    {
        BOOL fullscreenState;
        ThrowIfFailed(m_swapChain->GetFullscreenState(&fullscreenState, nullptr));
        if (FAILED(m_swapChain->SetFullscreenState(!fullscreenState, nullptr))) {
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

void MLDX12App::OnSizeChanged(int width, int height, bool minimized)
{
    char s[256];
    sprintf_s(s, "\n\nOnSizeChanged(%d %d)\n", width, height);
    OutputDebugStringA(s);

    if ((width != Width() || height != Height()) && !minimized) {
        WaitForGpu();

        for (UINT n = 0; n < FrameCount; n++) {
            m_renderTargets[n].Reset();
            m_fenceValues[n] = m_fenceValues[m_frameIndex];
        }

        DXGI_SWAP_CHAIN_DESC desc = {};
        m_swapChain->GetDesc(&desc);
        ThrowIfFailed(m_swapChain->ResizeBuffers(FrameCount, width, height, desc.BufferDesc.Format, desc.Flags));

        BOOL fullscreenState;
        ThrowIfFailed(m_swapChain->GetFullscreenState(&fullscreenState, nullptr));
        m_windowedMode = !fullscreenState;

        m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

        UpdateForSizeChange(width, height);

        LoadSizeDependentResources();
    }
}

void MLDX12App::UpdateViewAndScissor()
{
    float x = 1.0f;
    float y = 1.0f;

    m_viewport.TopLeftX = mWidth * (1.0f - x) / 2.0f;
    m_viewport.TopLeftY = mHeight * (1.0f - y) / 2.0f;
    m_viewport.Width = x * mWidth;
    m_viewport.Height = y * mHeight;

    m_scissorRect.left = static_cast<LONG>(m_viewport.TopLeftX);
    m_scissorRect.right = static_cast<LONG>(m_viewport.TopLeftX + m_viewport.Width);
    m_scissorRect.top = static_cast<LONG>(m_viewport.TopLeftY);
    m_scissorRect.bottom = static_cast<LONG>(m_viewport.TopLeftY + m_viewport.Height);
}

void MLDX12App::LoadSizeDependentResources()
{
    UpdateViewAndScissor();

    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
        for (UINT n = 0; n < FrameCount; n++) {
            ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
            m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
            rtvHandle.Offset(1, m_rtvDescriptorSize);

            NAME_D3D12_OBJECT_INDEXED(m_renderTargets, n);
        }
    }
}

void MLDX12App::OnUpdate()
{
}

void MLDX12App::OnRender()
{
    PopulateCommandList();

    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    UpdateVideoTexture();

    ThrowIfFailed(m_swapChain->Present(1, 0));
    MoveToNextFrame();
}

void MLDX12App::DrawFullscreenTexture(void)
{
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

    ID3D12DescriptorHeap* ppHeaps[] = { m_srvHeap.Get() };
    m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    CD3DX12_GPU_DESCRIPTOR_HANDLE dh(m_srvHeap->GetGPUDescriptorHandleForHeapStart());
    dh.Offset(m_textureVideoIdToShow, m_srvDescriptorSize);
    m_commandList->SetGraphicsRootDescriptorTable(0, dh);

    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET));

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(),
        m_frameIndex, m_rtvDescriptorSize);
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    m_commandList->DrawInstanced(m_numVertices, 1, 0, 0);
}

void MLDX12App::PopulateCommandList()
{
    ThrowIfFailed(m_commandAllocators[m_frameIndex]->Reset());

    switch (m_drawMode) {
    case DM_RGB:
        ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), m_pipelineStateRGB.Get()));
        break;
    case DM_YUV:
        ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), m_pipelineStateYUV.Get()));
        break;
    default:
        break;
    }

    // 全クライアント領域を覆う矩形にテクスチャを貼って描画。
    DrawFullscreenTexture();

    // Start the Dear ImGui frame
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGuiCommands();
    m_dx12Imgui.Render(ImGui::GetDrawData(), m_commandList.Get(), m_frameIndex);

    // Indicate that the back buffer will now be used to present.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT));

    ThrowIfFailed(m_commandList->Close());
}

void MLDX12App::MoveToNextFrame()
{
    const UINT64 currentFenceValue = m_fenceValues[m_frameIndex];
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), currentFenceValue));
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex]) {
        ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));

        WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
    }

    m_fenceValues[m_frameIndex] = currentFenceValue + 1;
}

void MLDX12App::WaitForGpu()
{
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValues[m_frameIndex]));
    ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
    WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
    m_fenceValues[m_frameIndex]++;
}


void
MLDX12App::ImGuiCommands(void)
{
    ImGui::Begin("Settings");

    ImGui::Text("Average redraw frames per second : %.1f", ImGui::GetIO().Framerate);

    ImGui::Text("ALT+Enter to toggle fullscreen.");

    static int selectedDeviceIdx = 0;

    switch (m_state) {
    case S_Init:
        ImGui::Text("Video capture device list:");
        for (int i = 0; i < m_videoCaptureDeviceList.NumOfDevices(); ++i) {
            auto name = m_videoCaptureDeviceList.DeviceName(i);
            ImGui::RadioButton(name.c_str(), &selectedDeviceIdx, 0);
        }
        if (m_videoCaptureDeviceList.NumOfDevices() == 0) {
            ImGui::Text("No video capture device found.");
        }
        if (ImGui::Button("Start Capture")) {
            m_frameSkipCount = 0;

            IDeckLink *p = m_videoCaptureDeviceList.Device(selectedDeviceIdx);
            m_videoCapture.Init(p);
            m_videoCapture.SetCallback(this);
            bool brv = m_videoCapture.StartCapture(0);
            if (brv) {
                m_state = S_Capturing;
            }
        }
        break;
    case S_Capturing:
        {
            int queueSize = 0;

            ImGui::Text("Now Capturing...");

            m_mutex.lock();
            if (!m_capturedImages.empty()) {
                queueSize = (int)m_capturedImages.size();
                auto & front = m_capturedImages.front();
                ImGui::Text(front.imgFormat.c_str());
            } else {
                ImGui::Text("");
            }
            m_mutex.unlock();

            ImGui::Text("Queue size : %d", queueSize);
            ImGui::Text("Frame skip count : %lld", m_frameSkipCount);

            if (ImGui::Button("Stop Capture")) {
            
                m_videoCapture.StopCapture();
                m_videoCapture.Term();
            
                m_videoCaptureDeviceList.Term();
                m_videoCaptureDeviceList.Init();

                m_state = S_Init;
            }
        }
        break;
    default:
        assert(0);
        break;
    }

    ImGui::End();
    ImGui::Render();
}

static const char *
BMDPixelFormatToStr(int a)
{
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
MLDX12App::MLVideoCaptureCallback_VideoInputFrameArrived(IDeckLinkVideoInputFrame* videoFrame)
{
    m_mutex.lock();
    if (CapturedImageQueueSize <= m_capturedImages.size()) {
        ++m_frameSkipCount;
        m_mutex.unlock();
        return;
    }
    m_mutex.unlock();

    void *buffer = nullptr;
    videoFrame->GetBytes(&buffer);

    int width = videoFrame->GetWidth();
    int height = videoFrame->GetHeight();
    int fmt = videoFrame->GetPixelFormat();
    int rowBytes = videoFrame->GetRowBytes();
    char s[256];
    sprintf_s(s, "%dx%d %s", width, height, BMDPixelFormatToStr(fmt));
    int bytes = width * height * 4;

    CapturedImage ci;
    ci.imgFormat = s;
    ci.bytes = bytes;
    ci.width = width;
    ci.height = height;
    ci.drawMode = DM_RGB;
    ci.data = new uint8_t[bytes];
        
    switch (fmt) {
    case bmdFormat10BitRGB:
        {
            uint32_t *pFrom = (uint32_t *)buffer;
            uint32_t *pTo = (uint32_t *)ci.data;
            int pos = 0;
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    uint32_t v = NtoHL(pFrom[pos]);
                    uint32_t r = (v >> 22) & 0xff;
                    uint32_t g = (v >> 12) & 0xff;
                    uint32_t b = (v >> 2) & 0xff;
                    uint32_t a = 0xff;
                    pTo[pos] = (a << 24) + (b << 16) + (g << 8) + r;

                    ++pos;
                }
            }
            ci.drawMode = DM_RGB;
        }
        break;
    case bmdFormat10BitYUV:
        {
            uint32_t *pFrom = (uint32_t *)buffer;
            uint32_t *pTo = (uint32_t *)ci.data;
            int posF = 0;
            int posT = 0;
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width/6; ++x) {
                    uint32_t w0 = pFrom[posF];
                    uint32_t w1 = pFrom[posF+1];
                    uint32_t w2 = pFrom[posF+2];
                    uint32_t w3 = pFrom[posF+3];

                    uint8_t cr0 = (w0 >> 22) & 0xff;
                    uint8_t y0 = (w0 >> 12) & 0xff;
                    uint8_t cb0 = (w0 >> 2) & 0xff;

                    uint8_t y2 = (w1 >> 22) & 0xff;
                    uint8_t cb2 = (w1 >> 12) & 0xff;
                    uint8_t y1 = (w1 >> 2) & 0xff;

                    uint8_t cb4 = (w2 >> 22) & 0xff;
                    uint8_t y3  = (w2 >> 12) & 0xff;
                    uint8_t cr2 = (w2 >> 2) & 0xff;

                    uint8_t y5 = (w3 >> 22) & 0xff;
                    uint8_t cr4 = (w3 >> 12) & 0xff;
                    uint8_t y4 = (w3 >> 2) & 0xff;

                    uint8_t a = 0xff;
                    pTo[posT+0] = (a << 24) + (y0 << 16) + (cb0 << 8) + cr0;
                    pTo[posT+1] = (a << 24) + (y1 << 16) + (cb0 << 8) + cr0;
                    pTo[posT+2] = (a << 24) + (y2 << 16) + (cb2 << 8) + cr2;
                    pTo[posT+3] = (a << 24) + (y3 << 16) + (cb2 << 8) + cr2;
                    pTo[posT+4] = (a << 24) + (y4 << 16) + (cb4 << 8) + cr4;
                    pTo[posT+5] = (a << 24) + (y5 << 16) + (cb4 << 8) + cr4;

                    posF += 4;
                    posT += 6;
                }
            }
            ci.drawMode = DM_YUV;
        }
        break;
    default:
        break;
    }

    m_mutex.lock();
    m_capturedImages.push_back(ci);
    m_mutex.unlock();
}

void
MLDX12App::UpdateVideoTexture(void)
{
    m_mutex.lock();
    if (m_capturedImages.empty()) {
        m_mutex.unlock();
        //OutputDebugString(L"Not Available\n");
        return;
    }

    //char s[256];
    //sprintf_s(s, "Available %d\n", (int)m_capturedImages.size());
    //OutputDebugStringA(s);

    CapturedImage ci = m_capturedImages.front();
    m_capturedImages.pop_front();

    m_mutex.unlock();

    DXGI_FORMAT pixelFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    int         pixelBytes  = 4;

    ThrowIfFailed(m_commandAllocatorTextureUpload->Reset());
    ThrowIfFailed(m_commandListTextureUpload->Reset(m_commandAllocatorTextureUpload.Get(), m_pipelineStateRGB.Get()));

    ID3D12Resource *tex = m_textureVideo[!m_textureVideoIdToShow].Get();

    if (ci.width != tex->GetDesc().Width
            || ci.height != tex->GetDesc().Height) {
        // 中でInternalRelease()される。
        m_textureVideo[!m_textureVideoIdToShow] = nullptr;

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

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &texDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&m_textureVideo[!m_textureVideoIdToShow])));
        NAME_D3D12_OBJECT_INDEXED(m_textureVideo, !m_textureVideoIdToShow);

        CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(m_srvHeap->GetCPUDescriptorHandleForHeapStart());
        srvHandle.Offset(!m_textureVideoIdToShow, m_srvDescriptorSize);

        // Describe and create a SRV for the texture.
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = texDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        m_device->CreateShaderResourceView(m_textureVideo[!m_textureVideoIdToShow].Get(), &srvDesc, srvHandle);
    }

    // texUploadHeapがスコープから外れる前にcommandListを実行しなければならない。
    ComPtr<ID3D12Resource> texUploadHeap;
    {
        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_textureVideo[!m_textureVideoIdToShow].Get(), 0, 1);

        // Create the GPU upload buffer.
        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&texUploadHeap)));

        m_commandListTextureUpload->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_textureVideo[!m_textureVideoIdToShow].Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST));

        D3D12_SUBRESOURCE_DATA textureData = {};
        textureData.pData = &ci.data[0];
        textureData.RowPitch = ci.width * pixelBytes;
        textureData.SlicePitch = textureData.RowPitch * ci.height;
        UpdateSubresources(m_commandListTextureUpload.Get(), m_textureVideo[!m_textureVideoIdToShow].Get(), texUploadHeap.Get(), 0, 0, 1, &textureData);
        m_commandListTextureUpload->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_textureVideo[!m_textureVideoIdToShow].Get(),
            D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
    }

    ThrowIfFailed(m_commandListTextureUpload->Close());
    ID3D12CommandList* ppCommandLists[] = { m_commandListTextureUpload.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
    WaitForGpu();

    m_textureVideoIdToShow = !m_textureVideoIdToShow;

    m_drawMode = ci.drawMode;
    delete[] ci.data;
    ci.data = nullptr;
}
