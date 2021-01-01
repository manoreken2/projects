
struct PSInput
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD;
};

PSInput VSMain(float4 pos : POSITION, float4 uv : TEXCOORD)
{
    PSInput result;

    result.pos = pos;
    result.uv  = uv;

    return result;
}

