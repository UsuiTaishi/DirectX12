#pragma once
#include<Windows.h>
#include <tchar.h>

// メイン関数から呼び出せるように宣言だけしておく（メニュー表）
HWND CreateGameWindow(HINSTANCE hInstance, int width, int height, const TCHAR* title);