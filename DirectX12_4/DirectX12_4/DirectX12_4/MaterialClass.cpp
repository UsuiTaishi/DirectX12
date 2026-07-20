#include<Windows.h>
#include <tchar.h>
#include<string>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <vector>
#include <d3dcompiler.h>
#include"game.h"
#include<cassert>

#ifdef _DEBUG
#include <iostream>
#endif

#include <fbxsdk.h>
#pragma comment(lib, "libfbxsdk-md.lib")
#pragma comment(lib, "libxml2-md.lib")
#pragma comment(lib, "zlib-md.lib")
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")

//デフォルト設定のD3D12_GRAPHICS_PIPELINE_STATE_DESCを作る
D3D12_GRAPHICS_PIPELINE_STATE_DESC Material::GetDefaultGPSDesc(const shaderSet& shaders)
{
	HRESULT result;

	m_GPS_DESC.pRootSignature = m_rootSignature.Get();

	//シェーダーをコンパイル

	result = D3DCompileFromFile(
		shaders.vs.c_str(),
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"vsMain",
		"vs_5_1",
		D3DCOMPILE_DEBUG,
		0,
		&_vsBlob,
		&errorBlob
	);
#ifdef _DEBUG
	CheckResult(result, "CompileVertexShader");
#endif
	if (result == S_OK) 
	{
		m_GPS_DESC.VS.pShaderBytecode = _vsBlob->GetBufferPointer();
		m_GPS_DESC.VS.BytecodeLength = _vsBlob->GetBufferSize();
	}

	result = D3DCompileFromFile(
		shaders.ps.c_str(),
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"psMain",
		"ps_5_1",
		D3DCOMPILE_DEBUG,
		0,
		&_psBlob,
		&errorBlob
	);
#ifdef _DEBUG
	CheckResult(result, "CompileVertexShader");
#endif
	if (result == S_OK)
	{
		m_GPS_DESC.PS.pShaderBytecode = _psBlob->GetBufferPointer();
		m_GPS_DESC.PS.BytecodeLength = _psBlob->GetBufferSize();
	}

	if (!shaders.hs.empty())
	{
		result = D3DCompileFromFile(
			shaders.hs.c_str(),
			nullptr,
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			"hsMain",
			"hs_5_1",
			D3DCOMPILE_DEBUG,
			0,
			&_hsBlob,
			&errorBlob
		);
#ifdef _DEBUG
		CheckResult(result, "CompileHalShader");
#endif
		if (result == S_OK)
		{
			m_GPS_DESC.HS.pShaderBytecode = _hsBlob->GetBufferPointer();
			m_GPS_DESC.HS.BytecodeLength = _hsBlob->GetBufferSize();
		}
	}

	if (!shaders.ds.empty())
	{
		result = D3DCompileFromFile(
			shaders.ds.c_str(),
			nullptr,
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			"dsMain",
			"ds_5_1",
			D3DCOMPILE_DEBUG,
			0,
			&_dsBlob,
			&errorBlob
		);
#ifdef _DEBUG
		CheckResult(result, "CompileDomainShader");
#endif
		if (result == S_OK)
		{
			m_GPS_DESC.DS.pShaderBytecode = _dsBlob->GetBufferPointer();
			m_GPS_DESC.DS.BytecodeLength = _dsBlob->GetBufferSize();
		}
	}

	if (!shaders.gs.empty())
	{
		result = D3DCompileFromFile(
			shaders.gs.c_str(),
			nullptr,
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			"gsMain",
			"gs_5_1",
			D3DCOMPILE_DEBUG,
			0,
			&_gsBlob,
			&errorBlob
		);
#ifdef _DEBUG
		CheckResult(result, "CompileGeometoryShader");
#endif
		if (result == S_OK)
		{
			m_GPS_DESC.GS.pShaderBytecode = _gsBlob->GetBufferPointer();
			m_GPS_DESC.GS.BytecodeLength = _gsBlob->GetBufferSize();
		}
	}
	//ブレンドパラメーター
	D3D12_BLEND_DESC blendDesc = {};
	blendDesc.AlphaToCoverageEnable = false;
	blendDesc.IndependentBlendEnable = false;
	D3D12_RENDER_TARGET_BLEND_DESC rtBlendDesc = {};
	rtBlendDesc.BlendEnable = false;
	rtBlendDesc.LogicOpEnable = false;
	rtBlendDesc.SrcBlend = D3D12_BLEND_ONE;
	rtBlendDesc.DestBlend = D3D12_BLEND_ZERO;
	rtBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	rtBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	rtBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	rtBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	rtBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	rtBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0] = rtBlendDesc;
	m_GPS_DESC.BlendState = blendDesc;
}

HRESULT Material::InitPipeline(const RenderContext& context)
{
	HRESULT result;
	//頂点の説明書
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	//ルートシグネチャ設定
	D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
	rootSigDesc.NumParameters = 0;
	rootSigDesc.pParameters = nullptr;
	rootSigDesc.NumStaticSamplers = 0;
	rootSigDesc.pStaticSamplers = nullptr;
	rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ID3DBlob* serializedRootSig = nullptr;
	result = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serializedRootSig, &errorBlob);
#ifdef _DEBUG
	CheckResult(result, "SerializeRootSignature");
#endif
	result = context.device->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature));
	serializedRootSig->Release();
#ifdef _DEBUG
	CheckResult(result, "CreateRootSignature");
#endif


	result = context.device->CreateGraphicsPipelineState(&m_GPS_DESC, IID_PPV_ARGS(&m_pipelineState));
#ifdef _DEBUG
	CheckResult(result, "CreateGraphicsPipelineState");
#endif
	return S_OK;
}
