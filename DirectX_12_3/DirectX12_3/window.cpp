#include"window.h"

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

HWND CreateGameWindow(HINSTANCE hInstance, int width, int height, const TCHAR* title ) {
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