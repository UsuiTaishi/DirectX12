#include<Windows.h>
#include<tchar.h>
#include<d3d12.h>
#include<dxgi1_6.h>
#include<vector>
#ifdef _DEBUG
#include<iostream>
#endif

#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3d12.lib")

using namespace std;

void DebugOutputFormatString(const char* format, ...)
{
#ifdef _DEBUG
	va_list valist;
	va_start(valist, format);
	printf(format, valist);
	va_end(valist);
#endif
}

LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	if (msg == WM_DESTROY) {//ウィンドウが破棄されたら呼ばれます
		PostQuitMessage(0);//OSに対して「もうこのアプリは終わるんや」と伝える
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);//規定の処理を行う
}

const UINT window_width = 1280;
const UINT window_height = 720;

//アドレス設定
IDXGIFactory6* _dxgiFactory = nullptr; //グラボを探したり、画面の管理をしたりする大元の窓口
ID3D12Device* _dev = nullptr;//アダプターを選択したのちにグラボの中に作られる。コマンドアロケーターとかテクスチャバッファなどが必要としてる分を切り出す働き
IDXGISwapChain4* _swapchain = nullptr;
ID3D12CommandAllocator* _cmdAllocator = nullptr;//コマンドアロケーターの宣言と初期化、コマンドリストより先に宣言する←コマンドアロケーターに小窓リストは保存されるから
ID3D12GraphicsCommandList* _cmdList = nullptr;//コマンドリストの宣言と初期化


//プロトタイプ宣言
#ifdef _DEBUG
int main(){
#else
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)//Windowsアプリ起動のためのMain関数
{
#endif
	DebugOutputFormatString("Show window test.");

	WNDCLASSEX w = {};
	w.cbSize = sizeof(WNDCLASSEX);
	w.lpfnWndProc = (WNDPROC)WindowProcedure;//コールバック関数の指定
	w.lpszClassName = _T("DirectXTest");//アプリケーションクラス名(適当でいいです)
	w.hInstance = GetModuleHandle(0);//ハンドルの取得
	RegisterClassEx(&w);//アプリケーションクラス(こういうの作るからよろしくってOSに予告する)

	RECT wrc = { 0,0, window_width, window_height };//ウィンドウサイズを決める
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);//ウィンドウのサイズはちょっと面倒なので関数を使って補正する
	//ウィンドウオブジェクトの生成
	HWND hwnd = CreateWindow(w.lpszClassName,//クラス名指定
		_T("DX12テスト"),//タイトルバーの文字
		WS_OVERLAPPEDWINDOW,//タイトルバーと境界線があるウィンドウです
		CW_USEDEFAULT,//表示X座標はOSにお任せします
		CW_USEDEFAULT,//表示Y座標はOSにお任せします
		wrc.right - wrc.left,//ウィンドウ幅
		wrc.bottom - wrc.top,//ウィンドウ高
		nullptr,//親ウィンドウハンドル
		nullptr,//メニューハンドル
		w.hInstance,//呼び出しアプリケーションハンドル
		nullptr);//追加パラメータ
	ShowWindow(hwnd, SW_SHOW);//ウィンドウ表示

	MSG msg = {};

	auto result = CreateDXGIFactory1(IID_PPV_ARGS(&_dxgiFactory));//最初のほうで宣言した _dxgiFactoryにぶち込む	二つ目の変数は作った工場をぶち込む場所
	//IID_PPY_ARGSはポインターを渡すとインターフェースIDと保存場所を返す

	vector<IDXGIAdapter*> adapters;//可変長配列　ドライバーをこれからここに追加していく
	IDXGIAdapter* tmpAdapter = nullptr;//これから配列に入るやつに仮で名前を与える。forで回すため？
	for (int i = 0; _dxgiFactory->EnumAdapters(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i)//インデックス番号とグラボのデータのアドレス
	{
		adapters.push_back(tmpAdapter);//さっき作った可変長配列の後ろにtmpAdapterをぶち込む
	}
	//ドライバー情報のアドレスが入った可変長配列ができた

	for (auto adpt : adapters)//adaptersから1っ子取り出しそれをadptとする
	{
		DXGI_ADAPTER_DESC adesc = {};//初期化 DXGI_ADAPTER_DESC（構造体）にはグラボのデータ（スペック、名前とか）が乗ってる。
		adpt->GetDesc(&adesc);
		wstring strDesc = adesc.Description;
		//		構造体DXGI_ADAPTER_DESCについて
		/*
		Description (型: WCHAR[128])グラフィックボードの名前（型番）です。「NVIDIA GeForce RTX 4070」や「Intel(R) Iris(R) Xe Graphics」といった、人間が見て一目でわかる文字列が格納されます。
		VendorId (型: UINT)グラボを作ったメーカー（ベンダー）の識別番号です。例えば、NVIDIAなら 0x10DE、AMDなら 0x1002、Intelなら 0x8086 といった、業界共通の決まった数値が入ります。
		2. グラボの型番（システム用）
		DeviceId (型: UINT)グラボの具体的なモデル（製品）を表す識別番号です。メーカーが製品ごとに割り当てています。
		SubSysId (型: UINT)サブシステム（ボード全体の設計元）の識別番号です。例えば、同じNVIDIAのチップを使っていても、ASUS製かMSI製かといった違いを区別するために使われます。
		Revision (型: UINT)グラボの改訂番号（バージョン）です。ハードウェアの細かい仕様変更の度合いを表します。
		3. メモリ（VRAM）の容量プログラムが一番よくチェックする重要な項目です（すべてバイト単位なので、GBに直すには $1024 \times 1024 \times 1024$ で割ります）。
		DedicatedVideoMemory (型: SIZE_T)グラボ専用の超高速メモリ（VRAM）の容量です。ゲームや3D処理の快適さに直結するパーツです。
		DedicatedSystemMemory (型: SIZE_T)グラボ専用として、パソコンのメインメモリ（RAM）から起動時に確保された容量です。主に「ビデオメモリを搭載していない内蔵グラフィックス（Intel CoreやRyzenのCPU内蔵グラボ）」などで使われます。普通のグラボなら通常 0 です。
		SharedSystemMemory (型: SIZE_T)グラボ専用のメモリが足りなくなったときに、パソコンのメインメモリ（RAM）から最大でどれだけ借りてこれるかという容量です。4. 個別の識別子AdapterLuid (型: LUID)システム内でこのグラボを絶対に呼び間違えないための、一意の識別番号（LUID）です。パソコンに2枚以上のグラボが刺さっている場合、どちらのグラボかをOSが内部的に見分けるために使われます。
		*/

		if (strDesc.find(L"NVIDIA") != string::npos)
		{
			tmpAdapter = adpt;
			break;
		}
	}//strDescにグラボの名前が登録される。高性能グラボが入れるにはもう少しコードが必要
	D3D_FEATURE_LEVEL levels[] =
	{
		D3D_FEATURE_LEVEL_12_1,//レイトレーシングにも対応してる
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,//PS4くらいの描画性能
		D3D_FEATURE_LEVEL_11_0,
	};
	//FEATURE_LEVELについて　https://learn.microsoft.com/en-us/windows/win32/api/d3dcommon/ne-d3dcommon-d3d_feature_level

	D3D_FEATURE_LEVEL featureLevel;

	for (auto lv : levels)//autoは変数の型を自動で保管してくれる	for (int i = 0; i < 4; ++i){D3D_FEATURE_LEVEL lv = levels[i];}
	{
		if (D3D12CreateDevice(nullptr, lv, IID_PPV_ARGS(&_dev)) == S_OK)//ppDeviceがNULLで関数が成功した場合、 S_OKではなくS_FALSEが返されます。今回の場合ちゃんと入っている。４引数は作るデバイスを格納する場所。
		{
			featureLevel = lv;
			cout << lv;
			break;
		}
	}
	
	while (true)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		if (msg.message == WM_QUIT)
		{
			break;
		}
	}

	result = _dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_cmdAllocator));//_devというクラス型の変数にCreateCommandAllocator　一つ目の引数はコマンドアロケーターの種類
	result = _dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmdAllocator, nullptr, IID_PPV_ARGS(&_cmdList));
	/*
	1	[in]	UINT	nodeMask	マルチGPU環境で、どのグラフィックボードでこのリストを作るかを指定するマスク値。単一GPUの場合は 0 を指定します。
	2	[in]	D3D12_COMMAND_LIST_TYPE	type	作成するコマンドリストの種類（DIRECT や BUNDLE など）。アロケータの種類と一致させる必要があります。
	3	[in]	ID3D12CommandAllocator*	pCommandAllocator	先ほど作成したコマンドアロケータのポインタをここに渡します。リストが命令を記録する際のメモリの提供元になります。
	4	[in, optional]	ID3D12PipelineState*	pInitialState	コマンドリストの初期パイプライン状態（PSO）。何も指定しない（既定値のままにする）場合は nullptr で大丈夫です。
	5	[out]	void**	ppCommandList	【出口】 作成されたコマンドリストの受け取り場所です。ここも通常は IID_PPV_ARGS(&myCommandList) の形でお決まりの指定をします。
	*/

	UnregisterClass(w.lpszClassName, w.hInstance);
	return 0;
}