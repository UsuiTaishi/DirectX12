#include"BasicPixelShader.hlsli"

float4 BasicPS(Output input) : SV_TARGET
{
    return float4(input.uv, 0, 1);
}