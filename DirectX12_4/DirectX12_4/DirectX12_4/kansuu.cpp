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
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
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

void DX12App::Render()
{
	HRESULT result;
	
	UINT backBufferIndex = m_swapChain->GetCurrentBackBufferIndex();//今描画しようとしてるバックバッファーの識別番号は？

	m_cmdAllocators[backBufferIndex]->Reset();//コマンドアロケータをリセット
	m_cmdList->Reset(m_cmdAllocators[backBufferIndex].Get(), nullptr);//コマンドリストをリセット

	D3D12_RESOURCE_BARRIER barrier = {};
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
	FbxManager* fbxManager = FbxManager::Create();//FBXManager

	FbxIOSettings* ios = FbxIOSettings::Create(fbxManager, IOSROOT);//IO設定第二引数は設定のルートパスを表している。インポートの設定
	fbxManager->SetIOSettings(ios);

	FbxImporter* fbxImporter = FbxImporter::Create(fbxManager, "");

	FbxScene* fbxScene = FbxScene::Create(fbxManager, "My scene");//シーンの作成。このシーンにマテリアルとかメッシュとかを置いていく

	fbxImporter->Initialize(filename.c_str(), -1, NULL);//第三引数はインポート時の挙動（テクスチャやアニメーションを読み込むかなど）を制御する設定オブジェクトへのポインタを指定します。

	fbxImporter->Import(fbxScene);
	fbxImporter->Destroy();//データはシーンにあるのでもういらない

	FbxGeometryConverter fbxConverter(fbxManager);
	fbxConverter.SplitMeshesPerMaterial(fbxScene, true);//マテリアルっごとにメッシュを分割する。
	fbxConverter.Triangulate(fbxScene, true, false);//ポリゴンを三角形化する。

	//マテリアル読み込み
	int numMaterials = fbxScene->GetSrcObjectCount<FbxSurfaceMaterial>();//このFBXシーン全体に登録されているマテリアルの総数を取得する
	for (int i = 0; i < numMaterials; i++)
	{
		LoadMaterial(fbxScene->GetSrcObject<FbxSurfaceMaterial>(i));
	}


	FbxGeometryConverter fbxGeometoryConverter(fbxManager);
	if (!fbxGeometoryConverter.Triangulate(fbxScene, true, false)) return false;//面を三角化
}

void Model::LoadMaterial(FbxSurfaceMaterial* material)
{
	MATERIAL tmpMaterial;

	FbxProperty prop;//ディフーズ、ラフネスなどを辞書のように「名前（文字列）でデータを検索して取り出す」仕組みになっており、その取り出した属性の入れ物が FbxProperty
	FbxFileTexture* texture = nullptr;

	prop = material->FindProperty(FbxSurfaceMaterial::sDiffuse);

	texture = prop.GetSrcObject<FbxFileTexture>(0);
}

bool Model::Draw(const RenderContext& context)
{

	return true;
}