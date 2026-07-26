#include "shader_header.hlsli"

cbuffer b0 : register(b0)
{
    matrix VPmatrix;
}
cbuffer b1 : register(b1)
{
    matrix WorldMat;
}

Output vsMain(
    float4 pos : POSITION,
    float4 normal : NORMAL,
    float2 uv : TEXCOORD,
    float4 tangent : TANGENT
)
{
    Output output = (Output)0;
    float4 worldPos = mul(pos, WorldMat);
    output.svPos = mul(worldPos, VPmatrix);
    output.normal = normal;
    return output;
    
} 