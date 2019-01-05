#pragma once

#include "MLDX12.h"

using Microsoft::WRL::ComPtr;

struct ImDrawData;

class MLDX12Imgui {
public:
    void Init(ID3D12Device *device);
    void Term(void);
    
    void Render(ImDrawData* draw_data, ID3D12GraphicsCommandList* ctx, int frameIdx);

private:
    ID3D12Device * m_device;
    ComPtr<ID3D12RootSignature> m_rootSig;
    ComPtr<ID3D12PipelineState> m_pso;

    struct FrameResources
    {
        ID3D12Resource* IB;
        ID3D12Resource* VB;
        int vbCount;
        int ibCount;
    };

    FrameResources m_frameResources[2];

    void SetupPso(void);
    void SetupRootSig(void);
};