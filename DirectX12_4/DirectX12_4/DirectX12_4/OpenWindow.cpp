#include<Windows.h>
#include <tchar.h>
#include<string>
#include <vector>
#include"game.h"
#include<cassert>

#ifdef _DEBUG
#include <iostream>
#endif

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