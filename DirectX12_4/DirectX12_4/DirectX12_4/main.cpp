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
	DX12App app;

	app.Init(hwnd, window_width, window_height);

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
			app.Render();
		}
	}

	return (int)msg.wParam;
}