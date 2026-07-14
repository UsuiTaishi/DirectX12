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

using namespace std;

void EnableDebugLayer() {
	ID3D12Debug* debugLayer = nullptr;//デバッグレイヤーを使うためのインターフェースを宣言
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer)))) {
		debugLayer->EnableDebugLayer();
		debugLayer->Release();
	}
}

void CheckResult(HRESULT result, string process)
{
	if (FAILED(result))
	{
		// 失敗時：メッセージとエラーコード（10進数）を表示
		std::string log = "[FAILED] " + process + " (Error Code: " + std::to_string(result) + ")\n";
		OutputDebugStringA(log.c_str());
		__debugbreak();
	}
	else
	{
		//成功
		std::string log = "[SUCCESS] " + process + "\n";
		OutputDebugStringA(log.c_str());
	}
}

void OnClose(HWND hWnd) {
	DestroyWindow(hWnd);
}

void OnDestroy() {
	PostQuitMessage(0);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
	case WM_CLOSE:
		OnClose(hWnd);
		return 0;
	case WM_DESTROY:
		OnDestroy();
		return 0;
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

HWND CreateGameWindow(HINSTANCE hInstance, int width, int height, const TCHAR* title) {
	WNDCLASSEX w = {};
	w.lpszClassName = _T("DirectXTest");
	w.cbSize = sizeof(WNDCLASSEX);
	w.lpfnWndProc = (WNDPROC)WndProc; // 同じファイル内にあるから使える
	w.hCursor = LoadCursor(NULL, IDC_UPARROW);
	w.hInstance = hInstance;
	RegisterClassEx(&w);

	RECT wrc = { 0, 0, width, height };
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	HWND hwnd = CreateWindow(
		w.lpszClassName, title, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		wrc.right - wrc.left, wrc.bottom - wrc.top,
		nullptr, nullptr, hInstance, nullptr
	);

	return hwnd;
}

HRESULT DX12App::Init(HWND hWnd, int width, int height)
//処理
{
	HRESULT result;

	result = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&m_dxgiFactory));
#ifdef _DEBUG
	CheckResult(result, "CreateDXGIFactory2");
#endif
	vector<IDXGIAdapter*> adapters;
	IDXGIAdapter* tmpAdapter = nullptr;//これから配列に入るやつに仮で名前を与える。forで回すため
	for (int i = 0; m_dxgiFactory->EnumAdapters(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i)//インデックス番号とグラボのデータのアドレス
	{
		adapters.push_back(tmpAdapter);//さっき作った可変長配列の後ろにtmpAdapterをぶち込む
	}
	//IDXGIAdapter型のポインタ配列adaptersが完成
	for (auto adpt : adapters)//全ビデオカードに対し
	{
		DXGI_ADAPTER_DESC desc = {};
		adpt->GetDesc(&desc);//ビデオカードの情報を取得
		wstring strDesc = desc.Description;//strDescの中にこの１ループ内で取得したビデオカードの名前が入る
		if (strDesc.find(L"NVIDIA") != string::npos)
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
		result = D3D12CreateDevice(tmpAdapter, lv, IID_PPV_ARGS(&m_dev));
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

	for (auto adpt : adapters) { if (adpt != tmpAdapter) adpt->Release(); }
	if (tmpAdapter) tmpAdapter->Release();

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
	IDXGISwapChain1* sc1 = nullptr;
	result = m_dxgiFactory->CreateSwapChainForHwnd(m_cmdQueue.Get(), hWnd, &m_swapChainDesc, nullptr, nullptr, &sc1);
#ifdef _DEBUG
	CheckResult(result, "CreateSwapChainForHwnd");
#endif
	result = sc1->QueryInterface(IID_PPV_ARGS(&m_swapChain));//格納するアドレスをクラスのメンバ変数のものにするよ
	sc1->Release();

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
	return S_OK;
}

HRESULT DX12App::InitPipeline()
{
	//パイプラインステートを設定する。
	HRESULT result;

	ID3DBlob* _vsBlob = nullptr;
	ID3DBlob* _psBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;

	result = D3DCompileFromFile(
		L"VertexShader.hlsl",
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

	result = D3DCompileFromFile(
		L"PixelShader.hlsl",
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
	result = m_dev->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature));
	serializedRootSig->Release();
#ifdef _DEBUG
	CheckResult(result, "CreateRootSignature");
#endif

	//グラフィックパイプラインステート設定
	D3D12_GRAPHICS_PIPELINE_STATE_DESC m_gps = {};
	m_gps.pRootSignature = m_rootSignature.Get();
	m_gps.VS = { _vsBlob->GetBufferPointer(), _vsBlob->GetBufferSize() };
	m_gps.PS = { _psBlob->GetBufferPointer(), _psBlob->GetBufferSize() };

	D3D12_RASTERIZER_DESC m_gpsRasterrizerDesc = {};
	m_gpsRasterrizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	m_gpsRasterrizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	m_gps.RasterizerState = m_gpsRasterrizerDesc;

	D3D12_BLEND_DESC m_gpsBlendDesc = {};
	m_gpsBlendDesc.AlphaToCoverageEnable = false;
	m_gpsBlendDesc.IndependentBlendEnable = false;
	D3D12_RENDER_TARGET_BLEND_DESC m_rtBlendDesc = {};
	m_rtBlendDesc.BlendEnable = false;
	m_rtBlendDesc.LogicOpEnable = false;
	m_rtBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	m_gpsBlendDesc.RenderTarget[0] = m_rtBlendDesc;

	m_gps.BlendState.RenderTarget->RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	m_gps.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
	m_gps.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.StencilEnable = false;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;

	m_gps.DepthStencilState = depthStencilDesc;

	m_gps.NumRenderTargets = 1;
	m_gps.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; // スワップチェーンと一致させる
	m_gps.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	m_gps.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	m_gps.SampleDesc.Count = 1;

	result = m_dev->CreateGraphicsPipelineState(&m_gps, IID_PPV_ARGS(&m_pipelineState));
#ifdef _DEBUG
	CheckResult(result, "CreateGraphicsPipelineState");
#endif
	_vsBlob->Release();
	_psBlob->Release();

	return S_OK;
}

RenderContext DX12App::CreateRenderContext()
{
	RenderContext rContext = {};
	rContext.device = m_dev.Get();
	rContext.cmdList = m_cmdList.Get();
	rContext.srvHeap = m_srvHeap.Get();

	return rContext;
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

	float clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };
	m_cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);//RTVのクリア処理。これをやらないと残像が黒くなって現れる場合がある。
	m_cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
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

bool Model::LoadModel(const RenderContext& context, const string& filename)
{
	fbxsdk::FbxManager* fbxManager = fbxsdk::FbxManager::Create();//FBXManager

	FbxIOSettings* ios = FbxIOSettings::Create(fbxManager, IOSROOT);//IO設定第二引数は設定のルートパスを表している。インポートの設定
	fbxManager->SetIOSettings(ios);

	FbxImporter* fbxImporter = FbxImporter::Create(fbxManager, "");

	FbxScene* fbxScene = FbxScene::Create(fbxManager, "My scene");//シーンの作成。このシーンにマテリアルとかメッシュとかを置いていく

	if (!fbxImporter->Initialize(filename.c_str(), -1, NULL))
	{
		OutputDebugStringA("[FAILED] FBXファイルの読み込みに失敗しました。パスを確認してください。\n");
		return false;
	}

	fbxImporter->Initialize(filename.c_str(), -1, NULL);//第三引数はインポート時の挙動（テクスチャやアニメーションを読み込むかなど）を制御する設定オブジェクトへのポインタを指定します。

	fbxImporter->Import(fbxScene);
	fbxImporter->Destroy();//データはシーンにあるのでもういらない

	FbxGeometryConverter fbxConverter(fbxManager);
	fbxConverter.SplitMeshesPerMaterial(fbxScene, true);//マテリアルごとにメッシュを分割する。
	fbxConverter.Triangulate(fbxScene, true, false);//ポリゴンを三角形化する。

	/*マテリアル読み込み
	int numMaterials = fbxScene->GetMaterialCount();//このFBXシーン全体に登録されているマテリアルの総数を取得する
	for (int i = 0; i < numMaterials; i++)
	{
		//マテリアル名のみ抽出する。
		LoadMateial(fbxScene->GetMaterial(i));
	}*/

	int numMesh = fbxScene->GetSrcObjectCount<FbxMesh>();
	for (int i = 0; i < numMesh; i++)
	{
		LoadMesh(fbxScene->GetSrcObject<FbxMesh>(i));
	}
	std::vector<Vertex> allVertices;
	for (const auto& mesh : m_meshes)
	{
		allVertices.insert(allVertices.end(), mesh.m_mVertexData.begin(), mesh.m_mVertexData.end());
	}

	// 頂点バッファを作成
	CreateVertexBuffer(context, allVertices);
}

void Model::LoadMesh(FbxMesh* mesh)
{
	MeshData data = {};
	vertexCount = mesh->GetPolygonVertexCount();//全頂点数を調べる(blenderなどで表示される頂点数ではなく。頂点インデックスで並べたときの要素数)
	int* vertexIndex = mesh->GetPolygonVertices();//頂点番号配列の先頭ポインタを取得。インデックスバッファ

	//頂点座標用
	FbxVector4* vertexPos = mesh->GetControlPoints();//x,y,z,w配列を取得（座標は記録されてないあくまで(x1,y1,z1,w1)[0],(x2,y2,z3,w3)[1],()[3],()[4]....）、配列の先頭ポインタが返される。

	//ノーマル用
	FbxArray<FbxVector4> normals;
	mesh->GetPolygonVertexNormals(normals);//引数は結果を収納するところ

	//UV用
	FbxStringList uvset_names;
	mesh->GetUVSetNames(uvset_names);//UVセット名前リストを取得。
	FbxArray<FbxVector2> texcoord;
	mesh->GetPolygonVertexUVs(uvset_names.GetStringAt(0), texcoord);

	//接空間用 専用ヘルパー関数がないのでレイヤー構造から直接取得します。
	if (!mesh->GetElementTangentCount())
	{
		mesh->GenerateTangentsData(0, false, false);
	}
	//レイヤー0から接空間を取得
	FbxGeometryElementTangent* tangentElement = mesh->GetElementTangent(0);
	FbxLayerElementArrayTemplate<FbxVector4>& tangent = tangentElement->GetDirectArray();
	
	//なんのマテリアルが割り当てられてるか調べるこれも例や構造から直接取得します。
	if (mesh->GetElementMaterialCount() == 0) 
	{
		data.materialName = "";
	}
	else
	{
		FbxLayerElementMaterial* material = mesh->GetElementMaterial(0);
		int index = material->GetIndexArray().GetAt(0);
		FbxNode* node = mesh->GetNode();
		FbxSurfaceMaterial* surface_material = nullptr;
		if (node != nullptr) {
			surface_material = node->GetSrcObject<FbxSurfaceMaterial>(index);//レイヤー構造似ないなら直接取ってきて
		}

		if (surface_material != nullptr) {
			data.materialName = surface_material->GetName();
		}
		else {
			data.materialName = "";
		}
	}

	data.m_mVertexData = {};
	data.m_mVertexData.resize(vertexCount);//メモリ確保

	for (int i = 0; i < vertexCount; i++)
	{
		int controlPointIndex = vertexIndex[i];//インデックスバッファ用にインデックスに従って通りに座標を配置していきたいので


        //MESH構造体に格納（float型へ変換）まずは頂点座標から
		data.m_mVertexData[i].Position[0] = static_cast<float>(vertexPos[controlPointIndex][0]);//0つまりx座標
		data.m_mVertexData[i].Position[1] = static_cast<float>(vertexPos[controlPointIndex][1]);//0つまりy座標
		data.m_mVertexData[i].Position[2] = static_cast<float>(vertexPos[controlPointIndex][2]);//0つまりz座標
		//これでv0(座標),v1(座標),v2(座標),v0(座標),v1(座標),v3(座標)みたいな感じでインデックス順に座標データの配列ができる重複してるとこもあるのでDirectXに渡してインデックスを利用する際は重複を消してインデックスを付与しないといけない

		//ノーマル
		data.m_mVertexData[i].Normal[0] = static_cast<float>(normals[i][0]);
		data.m_mVertexData[i].Normal[1] = static_cast<float>(normals[i][1]);
		data.m_mVertexData[i].Normal[2] = static_cast<float>(normals[i][2]);
		
		//UV
		data.m_mVertexData[i].UV[0] = static_cast<float>(texcoord[i][0]);
		data.m_mVertexData[i].UV[1] = static_cast<float>(texcoord[i][1]);

		//接空間
		data.m_mVertexData[i].Tangent[0] = static_cast<float>(tangent[i][0]);
		data.m_mVertexData[i].Tangent[1] = static_cast<float>(tangent[i][1]);
		data.m_mVertexData[i].Tangent[2] = static_cast<float>(tangent[i][2]);
	}
	m_meshes.push_back(data);
}

void Model::CreateVertexBuffer(const RenderContext& context, std::vector<Vertex>& vertices)
{
	HRESULT result;
	D3D12_RESOURCE_DESC vertexResouceDesc = {};
	vertexResouceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	vertexResouceDesc.Width = sizeof(Vertex)*vertices.size();
	vertexResouceDesc.Height = 1;
	vertexResouceDesc.SampleDesc.Count = 1;
	vertexResouceDesc.SampleDesc.Quality = 0;
	vertexResouceDesc.DepthOrArraySize = 1;
	vertexResouceDesc.MipLevels = 1;
	vertexResouceDesc.Format = DXGI_FORMAT_UNKNOWN;
	vertexResouceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	vertexResouceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	D3D12_HEAP_PROPERTIES vertexheap = {};
	vertexheap.Type = D3D12_HEAP_TYPE_UPLOAD;//CPUからアクセス可能
	vertexheap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;//カスタムのとき使うやつ
	vertexheap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;//カスタムのとき使うやつ

	result = context.device->CreateCommittedResource
	(
		&vertexheap,
		D3D12_HEAP_FLAG_NONE,
		&vertexResouceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,//Gpuからは読み取り専用
		nullptr,
		IID_PPV_ARGS(m_vertexBuffer.GetAddressOf())
	);
#ifdef _DEBUG
	CheckResult(result, "CommittedVertexResources");
#endif

	void* pMappedData = nullptr;
	m_vertexBuffer->Map(0, nullptr, &pMappedData);
	Vertex* pVertexData = static_cast<Vertex*>(pMappedData);

	size_t offset = 0;
	for (size_t i = 0; i < m_meshes.size(); i++)
	{
		MeshData& mesh = m_meshes[i];
		copy(mesh.m_mVertexData.begin(), mesh.m_mVertexData.end(), pVertexData +  offset);//第３引数は配置するアドレス
		offset += mesh.m_mVertexData.size();//MeshDataの頂点デーや分だけオフセットをずらす。
	}
	m_vertexBuffer->Unmap(0, nullptr);

	m_vbView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
	m_vbView.SizeInBytes = sizeof(Vertex)*vertices.size();
	m_vbView.StrideInBytes = sizeof(Vertex);
}

bool Model::Draw(const RenderContext& context)
{
	context.cmdList->IASetVertexBuffers(0, 1, &m_vbView);
	context.cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context.cmdList->DrawInstanced(vertexCount, 1, 0, 0);

	return true;
}