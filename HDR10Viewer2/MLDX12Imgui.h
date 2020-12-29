#pragma once

#include "MLDX12.h"

using Microsoft::WRL::ComPtr;

struct ImDrawData;

class MLDX12Imgui {
public:
    void Init(ID3D12Device *device, DXGI_FORMAT rtvFmt);
    void Term(void);
    
    void Render(ImDrawData* draw_data, ID3D12GraphicsCommandList* ctx, int frameIdx);

private:
    struct FrameResources {
        ID3D12Resource* IB;
        ID3D12Resource* VB;
        int vbCount;
        int ibCount;
    };

    FrameResources mFrameResources[2];
    ID3D12Device * mDevice;
    DXGI_FORMAT mRtvFmt;
    ComPtr<ID3D12RootSignature> mRootSig;
    ComPtr<ID3D12PipelineState> mPso;

    void SetupPso(void);
    void SetupRootSig(void);
};