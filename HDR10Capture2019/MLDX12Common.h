#pragma once
#include "MLDX12.h"
#include "Common.h"

using Microsoft::WRL::ComPtr;

class MLDX12Common {
public:
    static void SetupPSO(ID3D12Device *device, DXGI_FORMAT rtvFormat, ID3D12RootSignature * rootSign, const wchar_t *vsShaderName, const wchar_t* psShaderName, ComPtr<ID3D12PipelineState> & pso);
    static void SetupPSOFromMemory(ID3D12Device* device, DXGI_FORMAT rtvFormat, ID3D12RootSignature* rootSign,
        const char * vsShaderName, size_t vsShaderBytes, const char* vsShaderStr,
        const char * psShaderName, size_t psShaderBytes, const char* psShaderStr,
        ComPtr<ID3D12PipelineState>& pso);
};
