#include<Windows.h>
#include <tchar.h>
#include<string>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <vector>
#include <d3dcompiler.h>
//#include<DirectXTex.h>
#include "renderer.h"

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
#ifdef DEBUG
	void EnableDebugLayer();
#endif // DEBUG

	HINSTANCE hInstance = GetModuleHandle(nullptr);
	const int window_width = 800;
	const int window_height = 600;
	const float aspect = float(window_height) / float(window_width);
	HWND hwnd = CreateGameWindow(hInstance, window_width, window_height, _T("DX12 単純ポリゴンテスト"));
	DX12App app;
	Model erika;
	Material material;
	Camera camera;

	app.Init(hwnd, window_width, window_height);
	RenderContext renderContext = app.CreateRenderContext();

	erika.LoadModel(renderContext, "monkey.fbx");
	erika.InitTransform(renderContext);

	shaderSet shaders = {};
	shaders.vs = L"VertexShader.hlsl";
	shaders.ps = L"PixelShader.hlsl";
	material.GetDefaultGPSDesc(shaders);
	material.InitPipeline(renderContext);

	camera.Init(renderContext);



	ShowWindow(hwnd, SW_SHOW);

	MSG msg = { 0 };
	while (msg.message != WM_QUIT)
	{

		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			DispatchMessage(&msg);
		}
		else
		{
			app.BeginFrame();
			//ここに
			// からモデルを描きこむ処理を
			erika.UpdateTransform();
			camera.SetCamera(renderContext, window_width, window_height);
			material.SetPipelineState(renderContext);
			erika.Draw(renderContext);
			//ここまで描きこむ処理
			app.EndFrame();
		}
	}
	//app.Release();
	return (int)msg.wParam;
}