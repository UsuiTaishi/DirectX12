#pragma once
#include<Windows.h>
#include <tchar.h>
#include <DirectXMath.h>

using namespace DirectX;

// メイン関数から呼び出せるように宣言だけしておく（メニュー表）
HWND CreateGameWindow(HINSTANCE hInstance, int width, int height, const TCHAR* title);

struct Vertex
{
    XMFLOAT3 pos;
    XMFLOAT2 uv;
};