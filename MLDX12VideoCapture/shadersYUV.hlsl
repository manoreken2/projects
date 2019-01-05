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
    float4 vuya = g_texture.Sample(g_sampler, input.uv);

    float4 r;

    float3 yuv = float3(vuya.z, vuya.y - 0.5f, vuya.x - 0.5f);

    float3 rgb = mul(yuv, float3x3(
        +1.0f,   +1.0f,    +1.0f,
        +0.0f,   -0.3441f, +1.772f,
        +1.402f, -0.7141f, +0.0f));

    r.b = rgb.x;
    r.g = rgb.y;
    r.r = rgb.z;
    r.a = 1.0f;

    return r;
}

