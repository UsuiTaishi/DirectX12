#include<Windows.h>
#include <tchar.h>
#include<string>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <vector>
#include <d3dcompiler.h>
#include"game.h"

#ifdef _DEBUG
#include <iostream>
#endif //_DEBUG

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")
//#pragma comment(lib,"DirectXTex.lib")

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
		// 成功時
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
	w.cbSize = sizeof(WNDCLASSEX);
	w.lpfnWndProc = (WNDPROC)WndProc; // 同じファイル内にあるから使える
	w.lpszClassName = _T("DirectXTest");
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
	result = m_dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_cmdAllocators[0], nullptr, IID_PPV_ARGS(&m_cmdList));
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
	IDXGISwapChain1* sc1 = nullptr;
	result = m_dxgiFactory->CreateSwapChainForHwnd(m_cmdQueue, hWnd, &m_swapChainDesc, nullptr, nullptr, &sc1);
#ifdef _DEBUG
	CheckResult(result, "CreateSwapChainForHwnd");
#endif
	result = sc1->QueryInterface(IID_PPV_ARGS(&m_swapChain));//格納するアドレスをクラスのメンバ変数のものにするよ
	sc1->Release();

	//バックバッファー用のディスクリプタヒープを作る
	D3D12_DESCRIPTOR_HEAP_DESC _rtvHeapDesc = {};
	_rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	_rtvHeapDesc.NumDescriptors = 2;
	_rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	_rtvHeapDesc.NodeMask = 0;

	result = m_dev->CreateDescriptorHeap(&_rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap));
#ifdef _DEBUG
	CheckResult(result, "CreateDescriptorHeap");
#endif
	//フェンスを作る
	UINT fenceVal = 0;
	result = m_dev->CreateFence(fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));//fenceはCPUがGPUに送ったコマンドキュー　フェンスの値はGPUが終えた処理のフレーム番号
#ifdef _DEBUG
	CheckResult(result, "CreateFence");
#endif
	return S_OK;
}