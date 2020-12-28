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

/*
 gammaType = 
    enum MLImage::GammaType {
        MLG_Linear = 0,
        MLG_G22 = 1,
        MLG_ST2084 = 2,
    };

    enum FlagsType {
        FLAG_OutOfRangeToBlack = 1,
    };
*/

cbuffer Consts : register(b0) {
    matrix c_mat;
    int    c_gammaType;
    int    c_flags; //< FlagsType
    float  c_maxNits;
};

struct PSInput
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD;
};

Texture2D    g_texture : register(t0);
SamplerState g_sampler : register(s0);

float3 SRGBtoLinear(float3 v) {
    // Approximately pow(color, 2.2)
    v.r = v.r < 0.04045f ? v.r / 12.92f : pow(abs(v.r + 0.055f) / 1.055f, 2.4f);
    v.g = v.g < 0.04045f ? v.g / 12.92f : pow(abs(v.g + 0.055f) / 1.055f, 2.4f);
    v.b = v.b < 0.04045f ? v.b / 12.92f : pow(abs(v.b + 0.055f) / 1.055f, 2.4f);
    return v;
}

/* ST2084toLinear: https://github.com/ampas/aces-dev/blob/v1.0.3/transforms/ctl/lib/ACESlib.Utilities_Color.ctl#L402
Academy Color Encoding System (ACES) software and tools are provided by the Academy under the
following terms and conditions: A worldwide, royalty-free, non-exclusive right to copy, modify,
create derivatives, and use, in source and binary forms, is hereby granted, subject to acceptance of this license.

Copyright (C) 2015 Academy of Motion Picture Arts and Sciences (A.M.P.A.S.). Portions contributed
by others as indicated. All rights reserved.

Performance of any of the aforementioned acts indicates acceptance to be bound by the following terms and conditions:

    Copies of source code, in whole or in part, must retain the above copyright notice, this list of conditions and the Disclaimer of Warranty.

    Use in binary form must retain the above copyright notice, this list of conditions and the Disclaimer of Warranty in the documentation and/or other materials provided with the distribution.

    Nothing in this license shall be deemed to grant any rights to trademarks, copyrights, patents, trade secrets or any other intellectual property of A.M.P.A.S. or any contributors, except as expressly stated herein.

    Neither the name "A.M.P.A.S." nor the name of any other contributors to this software may be used to endorse or promote products derivative of or based on this software without express prior written permission of A.M.P.A.S. or the contributors, as appropriate.

This license shall be construed pursuant to the laws of the State of California, and any disputes related thereto shall be subject to the jurisdiction of the courts therein.

Disclaimer of Warranty: THIS SOFTWARE IS PROVIDED BY A.M.P.A.S. AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT ARE DISCLAIMED. IN NO EVENT SHALL A.M.P.A.S., OR ANY CONTRIBUTORS OR DISTRIBUTORS, BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, RESITUTIONARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

WITHOUT LIMITING THE GENERALITY OF THE FOREGOING, THE ACADEMY SPECIFICALLY DISCLAIMS ANY REPRESENTATIONS OR WARRANTIES WHATSOEVER RELATED TO PATENT OR OTHER INTELLECTUAL PROPERTY RIGHTS IN THE ACADEMY COLOR ENCODING SYSTEM, OR APPLICATIONS THEREOF, HELD BY PARTIES OTHER THAN A.M.P.A.S.,WHETHER DISCLOSED OR UNDISCLOSED.
*/
static const float pq_m1 = 0.1593017578125f; // ( 2610.0 / 4096.0 ) / 4.0;
static const float pq_m2 = 78.84375f; // ( 2523.0 / 4096.0 ) * 128.0;
static const float pq_c1 = 0.8359375f; // 3424.0 / 4096.0 or pq_c3 - pq_c2 + 1.0;
static const float pq_c2 = 18.8515625f; // ( 2413.0 / 4096.0 ) * 32.0;
static const float pq_c3 = 18.6875f; // ( 2392.0 / 4096.0 ) * 32.0;
static const float pq_C = 10000.0f;

float3 ST2084toLinear(float3 rgb) {
    float3 Np = pow(rgb, 1.0f / pq_m2);
    float3 L = Np - pq_c1;
    if (L.r < 0.0f) {
        L.r = 0.0f;
    }
    if (L.g < 0.0f) {
        L.g = 0.0f;
    }
    if (L.b < 0.0f) {
        L.b = 0.0f;
    }
    L = L / (pq_c2 - pq_c3 * Np);
    L = pow(L, 1.0f / pq_m1);
    return L * pq_C * 0.01f; // returns 100cd/m^2 == 1
}

float3 ApplyGamma(float3 rgb) {
    if (c_gammaType == 0) {
        // MLG_Linear
        return rgb;
    } else if (c_gammaType == 1) {
        // MLG_G22 
        return SRGBtoLinear(rgb);
    } else if (c_gammaType == 2) {
        // MLG_ST2084
        return ST2084toLinear(rgb);
    } else {
        // ? 
        return rgb;
    }
}

float4 OutOfRangeToBlack(float4 v) {
    if ((c_flags & 1) != 0) {
        float maxV = c_maxNits * 0.01f;
        if (maxV < v.r
                || maxV < v.g
                || maxV < v.b) {
            return float4(0.0f, 0.0f, 0.0f, 1.0f);
        }
    }
    
    return v;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float4 rgba = g_texture.Sample(g_sampler, input.uv);

    rgba.rgb = ApplyGamma(rgba.rgb);

    rgba = mul(rgba, c_mat);

    rgba = OutOfRangeToBlack(rgba);

    return rgba;
}


