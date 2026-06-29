#include <Windows.h>
#include <tchar.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <vector>
#include <d3dcompiler.h>

#include "window.h" 

#ifdef _DEBUG
#include <iostream>
#endif //_DEBUG

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")

using namespace std;
using namespace DirectX;

void DebugOutputFormatString(const char* format, ...)
{
#ifdef _DEBUG
	va_list valist;
	va_start(valist, format);
	vprintf(format, valist);
	va_end(valist);
#endif // _DEBUG
}

#ifdef _DEBUG
int main()
{
	HINSTANCE hInstance = GetModuleHandle(nullptr);
#else
int WINAPI	WinMain(HINSTANCE, HINSTANCE, LPSTR, int) 
    {
	    DebugOutputFormatString("Show window test.");
	    getchar();
	    return 0;
    }
#endif // _DEBUG
	const int window_width = 800;
	const int window_height = 600;
	HWND hwnd = CreateGameWindow(hInstance, window_width, window_height, _T("DX12 単純ポリゴンテスト"));
	
	//こっから初期化

	

	ShowWindow(hwnd, SW_SHOW);

	MSG msg = {};
	unsigned int frame = 0;
	while (true) {

		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		//もうアプリケーションが終わるって時にmessageがWM_QUITになる
		if (msg.message == WM_QUIT) {
			break;
		}

		//こっから



	}
}