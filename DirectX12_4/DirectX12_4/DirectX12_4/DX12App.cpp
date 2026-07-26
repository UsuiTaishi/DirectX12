#include<Windows.h>
#include <tchar.h>
#include<string>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <vector>
#include <d3dcompiler.h>
#include"renderer.h"
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

using namespace std;
using namespace DirectX;


HRESULT DX12App::Init(HWND hWnd, int width, int height)
//処理
{
	HRESULT result;

	result = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&m_dxgiFactory));
#ifdef _DEBUG
	CheckResult(result, "CreateDXGIFactory2");
#endif
	vector<Microsoft::WRL::ComPtr<IDXGIAdapter>> adapters;
	Microsoft::WRL::ComPtr<IDXGIAdapter> tmpAdapter = nullptr;//これから配列に入るやつに仮で名前を与える。forで回すため
	for (int i = 0; m_dxgiFactory->EnumAdapters(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i)//インデックス番号とグラボのデータのアドレス
	{
		adapters.push_back(tmpAdapter);//さっき作った可変長配列の後ろにtmpAdapterをぶち込む
	}
	//IDXGIAdapter型のポインタ配列adaptersが完成
	for (const auto& adpt : adapters)//全ビデオカードに対し
	{
		DXGI_ADAPTER_DESC desc = {};
		adpt->GetDesc(&desc);//ビデオカードの情報を取得
		wstring strDesc = desc.Description;//strDescの中にこの１ループ内で取得したビデオカードの名前が入る
		if (strDesc.find(L"NVIDIA") != wstring::npos)
		{
			tmpAdapter = adpt;
			break;
		}
	}
	D3D_FEATURE_LEVEL levels[] =
	{
		D3D_FEATURE_LEVEL_12_1,//レイトレーシングにも対応してる
		D3D_FEATURE_LEVEL_12_0
	};
	bool deviceCreated = false;
	for (auto lv : levels)
	{
		result = D3D12CreateDevice(tmpAdapter.Get(), lv, IID_PPV_ARGS(&m_dev));
		if (SUCCEEDED(result))
		{
			deviceCreated = true;
			break;
		}
	}
#ifdef _DEBUG
	CheckResult(result, "D3D12CreateDevice");
#endif
	if (!deviceCreated) return E_FAIL;

	//描画用のコマンドアロケーターを作る
	for (int i = 0; i < 2; ++i)
	{
		result = m_dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_cmdAllocators[i]));
#ifdef _DEBUG
		CheckResult(result, "CreateCommandAllocator");
#endif
	}
	result = m_dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_cmdAllocators[0].Get(), nullptr, IID_PPV_ARGS(&m_cmdList));
	m_cmdList->Close();
#ifdef _DEBUG
	CheckResult(result, "CreateCommandList");
#endif
	//描画用のコマンドキューを作る
	D3D12_COMMAND_QUEUE_DESC _GraphicsCmdQueneDesc = {};
	_GraphicsCmdQueneDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	_GraphicsCmdQueneDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	_GraphicsCmdQueneDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	_GraphicsCmdQueneDesc.NodeMask = 0;

	result = m_dev->CreateCommandQueue(&_GraphicsCmdQueneDesc, IID_PPV_ARGS(&m_cmdQueue));
#ifdef _DEBUG
	CheckResult(result, "CreateCommandQueue");
#endif
	//スワップチェーンを作る
	DXGI_SWAP_CHAIN_DESC1 m_swapChainDesc = {};
	m_swapChainDesc.Width = width;
	m_swapChainDesc.Height = height;
	m_swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	m_swapChainDesc.Stereo = FALSE;
	DXGI_SAMPLE_DESC sampleDesc = {};
	sampleDesc.Count = 1;
	sampleDesc.Quality = 0;
	m_swapChainDesc.SampleDesc = sampleDesc;
	m_swapChainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
	m_swapChainDesc.BufferCount = BACK_BUFFER_COUNT;
	m_swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	m_swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	m_swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
	m_swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	Microsoft::WRL::ComPtr<IDXGISwapChain1> sc1 = nullptr;
	result = m_dxgiFactory->CreateSwapChainForHwnd(m_cmdQueue.Get(), hWnd, &m_swapChainDesc, nullptr, nullptr, &sc1);
#ifdef _DEBUG
	CheckResult(result, "CreateSwapChainForHwnd");
#endif
	result = sc1->QueryInterface(IID_PPV_ARGS(&m_swapChain));//格納するアドレスをクラスのメンバ変数のものにするよ

	//バックバッファー用のレンダーターゲットビューのディスクリプタヒープを作る
	D3D12_DESCRIPTOR_HEAP_DESC _rtvHeapDesc = {};
	_rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	_rtvHeapDesc.NumDescriptors = BACK_BUFFER_COUNT;//表裏のレンダーターゲットビュー用のメモリを確保
	_rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	_rtvHeapDesc.NodeMask = 0;
	result = m_dev->CreateDescriptorHeap(&_rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap));
#ifdef _DEBUG
	CheckResult(result, "CreateRtvHeap");
#endif
	//ディスクリプタとスワップチェーン上のバックバッファを紐づけ
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();//ディスクリプタを入れるヒープの先頭アドレスを取得する。
	for (UINT i = 0; i < BACK_BUFFER_COUNT; ++i)
	{
		result = m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_buckBuffer[i]));
#ifdef _DEBUG
		CheckResult(result, "SwapChain->GetBuffer");
#endif
	//ディスクリプタ（レンダーターゲットビュー）を作って入れる
		m_dev->CreateRenderTargetView(m_buckBuffer[i].Get(), nullptr, rtvHandle);// 取り出したバッファを、RTVとしてメモリに登録
		rtvHandle.ptr += m_dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);//次のバッファここでいう２枚目のバックバッファのために次の保存開始位置を指定する
	}

	//CBV、SRV,UAVのディスクリプタヒープを作る
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.NumDescriptors = MAX_SRV_COUNT;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; // シェーダーから見えるようにする
	srvHeapDesc.NodeMask = 0;
	result = m_dev->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap));
#ifdef _DEBUG
	CheckResult(result, "CreateSrvHeap");
#endif
	//サンプラー用のディスクリプタヒープを作る
	D3D12_DESCRIPTOR_HEAP_DESC smpHeapDesc = {};
	smpHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
	smpHeapDesc.NumDescriptors = 1;
	smpHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	smpHeapDesc.NodeMask = 0;
	result = m_dev->CreateDescriptorHeap(&smpHeapDesc, IID_PPV_ARGS(&m_smpHeap));
#ifdef _DEBUG
	CheckResult(result, "CreateSmpHeap");
#endif
	//サンプラーを作る。サンプラーはディスクリプタ―の一種
	D3D12_SAMPLER_DESC smpDesc = {};
	smpDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	smpDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	smpDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	smpDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	m_dev->CreateSampler(&smpDesc, m_smpHeap->GetCPUDescriptorHandleForHeapStart());

	//深度ステンシル用のディスクリプタヒープを作る
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;

	result = m_dev->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap));
#ifdef _DEBUG
	CheckResult(result, "CreateDsvHeap");
#endif

	//深度テクスチャ（Zバッファ）リソースの設定
	D3D12_RESOURCE_DESC depthResDesc = {};
	depthResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthResDesc.Width = width;   // ウィンドウの幅と同じ
	depthResDesc.Height = height; // ウィンドウの高さと同じ
	depthResDesc.DepthOrArraySize = 1;
	depthResDesc.MipLevels = 1;
	depthResDesc.Format = DXGI_FORMAT_D32_FLOAT; // 32ビット浮動小数点数で深度を記録
	depthResDesc.SampleDesc.Count = 1;
	depthResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	//リソースのクリア操作を最適化するための構造体を設定
	D3D12_CLEAR_VALUE depthClearValue = {};
	depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.DepthStencil.Stencil = 0;

	D3D12_HEAP_PROPERTIES depthHeapProp = {};
	depthHeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;

	result = m_dev->CreateCommittedResource(
		&depthHeapProp,//VRAMのメモリの設定どういうメモリに置くか
		D3D12_HEAP_FLAG_NONE,
		&depthResDesc,//リソースの設計図
		D3D12_RESOURCE_STATE_DEPTH_WRITE, // 最初から深度書き込み用の状態にしておく
		&depthClearValue,//クリア時の初期値
		IID_PPV_ARGS(&m_depthBuffer)//出力先
	);
#ifdef _DEBUG
	CheckResult(result, "CreateDepthBuffer");
#endif
	//ディスクリプタ（深度ビュー）をつくってディスクリプタヒープに入れる
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	m_dev->CreateDepthStencilView(m_depthBuffer.Get(), &dsvDesc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());

	//フェンスを作る
	UINT fenceVal = 0;
	result = m_dev->CreateFence(fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));//fenceはCPUがGPUに送ったコマンドキュー　フェンスの値はGPUが終えた処理のフレーム番号
#ifdef _DEBUG
	CheckResult(result, "CreateFence");
#endif
	m_viewport.Width = 800;
	m_viewport.Height = 600;
	m_viewport.TopLeftX = 0;
	m_viewport.TopLeftY = 0;
	m_viewport.MaxDepth = 1.0;
	m_viewport.MinDepth = 0.0;

	m_scissorrect.top = 0;
	m_scissorrect.left = 0;
	m_scissorrect.right = 800;
	m_scissorrect.bottom = 600;

	//ルートシグネチャを設定
	D3D12_ROOT_PARAMETER rootparams[3] = {};
	//カメラ用のルートパラメーター
	rootparams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootparams[0].Descriptor.ShaderRegister = 0;//レジスタのb0
	rootparams[0].Descriptor.RegisterSpace = 0;
	rootparams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	//オブジェクト用の定数バッファ　ヒープを経由せずに直接送る
	rootparams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootparams[1].Descriptor.ShaderRegister = 1;//レジスタのb1
	rootparams[1].Descriptor.RegisterSpace = 0;
	rootparams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;//座標変換は頂点シェーダーで行う

	//テクスチャ
	D3D12_DESCRIPTOR_RANGE srvRange = {};
	srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvRange.NumDescriptors = 1;//使うテクスチャの数
	srvRange.BaseShaderRegister = 0;//レジスターt0
	srvRange.RegisterSpace = 0;
	srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	rootparams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootparams[2].DescriptorTable.NumDescriptorRanges = 1;
	rootparams[2].DescriptorTable.pDescriptorRanges = &srvRange;
	rootparams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	//サンプラーの設定
	D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.MipLODBias = 0;
	samplerDesc.MaxAnisotropy = 0;
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	samplerDesc.MinLOD = 0.0f;
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	samplerDesc.ShaderRegister = 0; // HLSLの register(s0)
	samplerDesc.RegisterSpace = 0;
	samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	//ルートシグネチャ設定
	D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
	rootSigDesc.NumParameters = _countof(rootparams);
	rootSigDesc.pParameters = rootparams;
	rootSigDesc.NumStaticSamplers = 1;
	rootSigDesc.pStaticSamplers = &samplerDesc;
	rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	//ルートシグネチャのバイナリコードを生成
	ID3DBlob* rootSigBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;
	result = D3D12SerializeRootSignature
	(
		&rootSigDesc,
		D3D_ROOT_SIGNATURE_VERSION_1_0,
		&rootSigBlob,
		&errorBlob
	);
#ifdef _DEBUG
	CheckResult(result, "TranslateRootSignature");
#endif
	//ルートシグネチャのレイアウトを作成
	result = m_dev->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature));
#ifdef _DEBUG
	CheckResult(result, "TranslateRootSignature");
#endif
	return S_OK;
}

RenderContext DX12App::CreateRenderContext()
{
	RenderContext rContext = {};
	rContext.device = m_dev.Get();
	rContext.cmdList = m_cmdList.Get();
	rContext.rootSignature = m_rootSignature.Get();
	rContext.app = this;
	return rContext;
}

void DX12App::AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE& outCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE& outGpuHandle)
{
	UINT incrementSIze = m_dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	//CPUとGPUではメモリのアドレス空間（仮想アドレス）が異なる
	outCpuHandle = m_srvHeap->GetCPUDescriptorHandleForHeapStart();//CPUから見たヒープのアドレス
	outCpuHandle.ptr += m_nextSrvIndex * incrementSIze;

	outGpuHandle = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
	outGpuHandle.ptr += m_nextSrvIndex * incrementSIze;//VRAMへの保存先（）の先頭位置をずらします

	m_nextSrvIndex++;//インデックスをずらして次にSrvHeapに描きこまれる時に備えます。
	//この間数が呼び出されるたびにヒープにスロットが確保されていく
}

void DX12App::BeginFrame()
{
	HRESULT result;
	
	UINT backBufferIndex = m_swapChain->GetCurrentBackBufferIndex();//今描画しようとしてるバックバッファーの識別番号は？

	m_cmdAllocators[backBufferIndex]->Reset();//コマンドアロケータをリセット
	m_cmdList->Reset(m_cmdAllocators[backBufferIndex].Get(), nullptr);//コマンドリストをリセット

	m_cmdList->RSSetViewports(1, &m_viewport);
	m_cmdList->RSSetScissorRects(1, &m_scissorrect);

	

	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = m_buckBuffer[backBufferIndex].Get();
	barrier.Transition.Subresource = 0;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	m_cmdList->ResourceBarrier(1, &barrier);//ここまではリソースは読み込み用でこの後からは書き込み(我々が想像する描画処理)！読み込み前に描きこんでオブジェクトが表示されないなんてことを防ぐため

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();//表裏でループを回すために毎ループリセットする。
	rtvHandle.ptr += backBufferIndex * m_dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);;//表裏どっちのバックバッファ持ってくるか計算している。

	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

	m_cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	float clearColor[] = { 1.0f, 0.0f, 0.9f, 1.0f };
	m_cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);//RTVのクリア処理。これをやらないと残像が黒くなって現れる場合がある。
	m_cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	ID3D12DescriptorHeap* ppHeaps[] = { m_srvHeap.Get() };
	m_cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	m_cmdList->SetGraphicsRootSignature(m_rootSignature.Get());
}

void DX12App::EndFrame()
{
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	m_cmdList->ResourceBarrier(1, &barrier);//ここまではリソースは書き込み用だったがここからは読み込み用

	m_cmdList->Close();

	ID3D12CommandList* cmdLists[] = { m_cmdList.Get() };
	m_cmdQueue->ExecuteCommandLists(1, cmdLists);

	m_swapChain->Present(1, 0);

	m_fenceVal++;
	m_cmdQueue->Signal(m_fence.Get(), m_fenceVal);
	if (m_fence->GetCompletedValue() < m_fenceVal)//こっからの処理はCPUの待ち時間をつぶすための処理、GetCompletedValueはGPUが今、処理しているフレーム数と思ってもらえれば、今回の場合は
	{
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
		m_fence->SetEventOnCompletion(m_fenceVal, eventHandle);
		WaitForSingleObject(eventHandle, INFINITE);//特定の条件（シグナル）が満たされるか、指定した時間が経過するまで、プログラムの処理を一時停止して待つ
		CloseHandle(eventHandle);
	}
}