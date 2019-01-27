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

struct PSInput
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD;
};

Texture2D    g_texture : register(t0);
SamplerState g_sampler : register(s0);

PSInput VSMain(float4 pos : POSITION, float4 uv : TEXCOORD)
{
    PSInput result;

    result.pos = pos;
    result.uv  = uv;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float4 rgba = g_texture.Sample(g_sampler, input.uv);
    return rgba;
}

