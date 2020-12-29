#include "MLDX12Common.h"
#include "DXSampleHelper.h"

std::wstring GetAssetFullPath(const std::wstring & assetsPath, const wchar_t * assetName) {
    return assetsPath + assetName;
}

void
MLDX12Common::SetupPSO(ID3D12Device *device, DXGI_FORMAT rtvFormat, ID3D12RootSignature * rootSignature, const wchar_t* vsShaderName, const wchar_t *psShaderName, ComPtr<ID3D12PipelineState> & pso) {
    WCHAR s[512];
    GetAssetsPath(s, _countof(s));
    std::wstring assetsPath = assetsPath;

    ComPtr<ID3DBlob> vertexShader;
    ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
    // Enable better shader debugging with the graphics debugging tools.
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compileFlags = 0;
#endif

    ComPtr<ID3DBlob> vsCompileMsg;
    ComPtr<ID3DBlob> psCompileMsg;
    HRESULT hr;
    hr = D3DCompileFromFile(GetAssetFullPath(assetsPath, vsShaderName).c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, &vsCompileMsg);
    if (FAILED(hr)) {
        char *s = (char*)vsCompileMsg->GetBufferPointer();
        OutputDebugStringA(s);
        ThrowIfFailed(hr);
    }
    hr = D3DCompileFromFile(GetAssetFullPath(assetsPath, psShaderName).c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, &psCompileMsg);
    if (FAILED(hr)) {
        char* s = (char*)psCompileMsg->GetBufferPointer();
        OutputDebugStringA(s);
        ThrowIfFailed(hr);
    }

    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = {inputElementDescs, _countof(inputElementDescs)};
    psoDesc.pRootSignature = rootSignature;
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
    psoDesc.RTVFormats[0] = rtvFormat;
    psoDesc.SampleDesc.Count = 1;
    ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso)));
}

