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
void Material::GetDefaultGPSDesc(const shaderSet& shaders)
{
	HRESULT result;

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
	//サンプルマスク
	m_GPS_DESC.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	//ラスタライズ設定
	D3D12_RASTERIZER_DESC rasterizerDesc = {};
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	rasterizerDesc.CullMode = D3D12_CULL_MODE_FRONT;
	rasterizerDesc.FrontCounterClockwise = TRUE;//表面の決定方法。TRUEなら右ねじの法則
	rasterizerDesc.DepthBias = 0;//Zファイティング対策、値が小さいほど優先して描画される(前面に描画される)
	rasterizerDesc.DepthBiasClamp = 0.0;//深度バイアスでずらす最大値
	rasterizerDesc.SlopeScaledDepthBias = 0.1;//光の向きに対してどれくらい斜めになっているか（スロープ）」に応じて、ズラす量を大きくするための倍率設定
	rasterizerDesc.DepthClipEnable = TRUE;
	rasterizerDesc.MultisampleEnable = TRUE;//D3D12_GRAPHICS_PIPELINE_STATE_DESCのなかのsampleDesc構造体によってサンプリングの設定が決まっている
	rasterizerDesc.AntialiasedLineEnable = false;//「線分（Line）」を描画する際の専用のアンチエイリアシングを有効にするかどうかの設定
	rasterizerDesc.ForcedSampleCount = 0;//ラスタライズ時のサンプル数を強制的に上書き指定する設定
	rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;//保守的ラスタライザーを有効にするかどうか
	m_GPS_DESC.RasterizerState = rasterizerDesc;
	//深度ステンシル設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;//深度テストの合格基準
	depthStencilDesc.StencilEnable = FALSE;
	depthStencilDesc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	depthStencilDesc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	D3D12_DEPTH_STENCILOP_DESC defaultDSDesc = {};
	defaultDSDesc.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	defaultDSDesc.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	defaultDSDesc.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	defaultDSDesc.StencilFunc = D3D12_COMPARISON_FUNC_LESS;
	depthStencilDesc.FrontFace = defaultDSDesc;
	depthStencilDesc.BackFace = defaultDSDesc;
	m_GPS_DESC.DepthStencilState = depthStencilDesc;
	//頂点レイアウト設定
	static D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = 4;
	m_GPS_DESC.InputLayout = inputLayoutDesc;
	//トライアングルストリップ方式を用いるか。トライアングルリスト方式（インデックス使うやつ）ならDisableでいい
	m_GPS_DESC.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	//データを頂点、辺、三角面、どうやってとらえる？
	m_GPS_DESC.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	//レンダーターゲット数
	m_GPS_DESC.NumRenderTargets = 1;
	//各レンダーターゲットの表示設定
	m_GPS_DESC.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	//深度ステンシルバッファーのデータフォーマット
	m_GPS_DESC.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	//サンプリング設定
	DXGI_SAMPLE_DESC sampleDesc = {};
	sampleDesc.Count = 1;
	sampleDesc.Quality = 0;
	m_GPS_DESC.SampleDesc = sampleDesc;
	//使用するGPUの数に応じて変動します。
	m_GPS_DESC.NodeMask = 0;
	//nazo
	//m_GPS_DESC.CachedPSO 
}

void Material::InitPipeline(const RenderContext& context)
{
	HRESULT result;

	m_GPS_DESC.pRootSignature = context.rootSignature;

	result = context.device->CreateGraphicsPipelineState(&m_GPS_DESC, IID_PPV_ARGS(&m_pipelineState));
#ifdef _DEBUG
	CheckResult(result, "CreateGraphicsPipelineState");
#endif
}

void Material::SetPipelineState(const RenderContext& context)
{
	context.cmdList->SetPipelineState(m_pipelineState.Get());
}
