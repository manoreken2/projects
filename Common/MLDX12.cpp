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

#include "MLDX12.h"
#include "DXSampleHelper.h"

using namespace Microsoft::WRL;

MLDX12::MLDX12(int width, int height) :
    mWidth(width),
    mHeight(height)
{
    WCHAR assetsPath[512];
    GetAssetsPath(assetsPath, _countof(assetsPath));
    mAssetsPath = assetsPath;

    mAspectRatio = static_cast<float>(width) / static_cast<float>(height);
    mWindowBounds.left = 0;
    mWindowBounds.top = 0;
    mWindowBounds.right = width;
    mWindowBounds.bottom = height;
}

MLDX12::~MLDX12(void)
{
}

void MLDX12::GetHardwareAdapter(IDXGIFactory2* pFactory, IDXGIAdapter1** ppAdapter)
{
    ComPtr<IDXGIAdapter1> adapter;
    *ppAdapter = nullptr;

    for (int adapterIndex = 0;
            DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(adapterIndex, &adapter);
            ++adapterIndex) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            // Don't select it.
            continue;
        }

        // Check to see if the adapter supports Direct3D 12, but don't create the
        // actual device yet.
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_1, _uuidof(ID3D12Device), nullptr))) {
            break;
        }
    }

    *ppAdapter = adapter.Detach();
}

void MLDX12::UpdateForSizeChange(int clientWidth, int clientHeight)
{
    mWidth = clientWidth;
    mHeight = clientHeight;
    mAspectRatio = static_cast<float>(clientWidth) / static_cast<float>(clientHeight);
}

void MLDX12::SetWindowBounds(int left, int top, int right, int bottom)
{
    mWindowBounds.left = static_cast<LONG>(left);
    mWindowBounds.top = static_cast<LONG>(top);
    mWindowBounds.right = static_cast<LONG>(right);
    mWindowBounds.bottom = static_cast<LONG>(bottom);
}

