#pragma once
#include "MLDX12.h"
#include "Common.h"

using Microsoft::WRL::ComPtr;

class MLDX12Common {
public:
    static void SetupPSO(ID3D12Device *device, ID3D12RootSignature * rootSign, const wchar_t *shaderName, ComPtr<ID3D12PipelineState> & pso);
};
