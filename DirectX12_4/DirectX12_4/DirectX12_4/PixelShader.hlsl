#include "shader_header.hlsli"

float4 psMain(Output vsOutput): SV_Target
{
	return float4(vsOutput.normal.xyz, 1.0f);
}