#include"BasicPixelShader.hlsli"


float4 BasicPS(Output input) : SV_TARGET
{
    return tex.Sample(smp, input.uv);
}