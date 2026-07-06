#include<Windows.h>
#include <tchar.h>
#include<string>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <vector>
#include <d3dcompiler.h>
//#include<DirectXTex.h>
#include "game.h"

#ifdef _DEBUG
#include <iostream>
#endif //_DEBUG

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")
//#pragma comment(lib,"DirectXTex.lib")

using namespace std;
using namespace DirectX;

int WINAPI	WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	HINSTANCE hInstance = GetModuleHandle(nullptr);
	const int window_width = 800;
	const int window_height = 600;
	const float aspect = float(window_height) / float(window_width);
	HWND hwnd = CreateGameWindow(hInstance, window_width, window_height, _T("DX12 単純ポリゴンテスト"));

	IDXGIFactory6* _dxgiFactory = nullptr;
	ID3D12Device* _dev = nullptr;
	ID3D12CommandAllocator* _GraphicsCmdAllocators[2] = { nullptr, nullptr };
	ID3D12GraphicsCommandList* _GraphicsCmdList = nullptr;
	ID3D12CommandQueue* _GraphicsCmdQuene = nullptr;
	DXGI_SWAP_CHAIN_DESC1 _swapChainDesc = {};

	_swapChainDesc.Width = window_width;
	_swapChainDesc.Height = window_height;
	_swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 画面の色フォーマット
	_swapChainDesc.Stereo = FALSE;
	_swapChainDesc.SampleDesc.Count = 1;                // マルチサンプリング（MSAA）しない場合は1
	_swapChainDesc.SampleDesc.Quality = 0;              // Countが1ならQualityは0
	_swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	_swapChainDesc.BufferCount = 2;                     // バックバッファの数（表裏で2つ）
	_swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	_swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // DX12ではこれが必須
	_swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	_swapChainDesc.Flags = 0;

	IDXGISwapChain4* _swapChain = nullptr;
	ID3D12DescriptorHeap* _rtvHeap = nullptr;
	ID3D12Fence* _fence = nullptr;

	InitDX3D(
		hwnd,
		_dxgiFactory,
		_dev,
		_GraphicsCmdAllocators,
		_GraphicsCmdList,
		_GraphicsCmdQuene,
		_swapChainDesc,
		_swapChain,
		_rtvHeap,
		_fence
	);
	ShowWindow(hwnd, SW_SHOW);

	MSG msg = {};
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			
		}
	}

	return (int)msg.wParam;
}