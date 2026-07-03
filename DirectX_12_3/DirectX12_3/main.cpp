#include <Windows.h>
#include <tchar.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <vector>
#include <d3dcompiler.h>
#include<DirectXTex.h>

#include "window.h" 

#ifdef _DEBUG
#include <iostream>
#endif //_DEBUG

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib,"DirectXTex.lib")

using namespace std;

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

	HRESULT result;

	//こっから初期化

	Vertex vertices[] =
	{
		{{-1.0f, -1.0f, 0.0f},{0.0f, 1.0f}},
		{{-1.0f, 1.0f, 0.0f},{0.0f, 0.0f}},
		{{1.0f, -1.0f, 0.0f},{1.0f, 1.0f}},
		{{1.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
	};

	/*XMFLOAT3 vertices[] = {
	{-1.0f, -1.0f, 0.0f},//0
	{-1.0f, 1.0f, 0.0f},//1
	{1.0f, -1.0f, 0.0f},//2
	{1.0f, 1.0f, 0.0f},//3
	};*/

	unsigned short indices[] = {
		0,1,2,
		2,1,3
	};

	//こっから仮テクスチャ
	/*struct TexRGBA
	{
		unsigned char R, G, B, A;
	};

	vector<TexRGBA> texturedata(256 * 256);//生データ

	for (auto& rgba : texturedata)//texturedataの中のデータ一つに仮でrgbaという名前を付けて回す
	{
		rgba.R = rand() % 255;
		rgba.G = rand() % 255;
		rgba.B = rand() % 255;
		rgba.A = rand() % 255;
	};
	*/

	//こっから本テクスチャ
	TexMetadata metadata = {};//テクスチャリソースの仕様書、Image構造体はテクスチャリソースのサブリソースの仕様書//基本的にまたデータにはミップマップとかしか入ってない。ラフネスマップとか使いたいならもう一つメタデータ構造体を作らないといけない。テクスチャに対する処理の仕方が同じで、画像サイズも一緒なら同じメタデータ構造体にぶち込める。
	ScratchImage scratchImg = {};
	
	result = LoadFromWICFile
	(
		L"erika.png",//読み込みたい画像ファイルのパス
		WIC_FLAGS_NONE, 
		&metadata,//読み込んだ画像のメタデータ（解像度、フォーマット、画像の種類など）を格納するための構造体へのポインタです。
		scratchImg,//読み込まれた実際のピクセルデータが格納される、DirectXTexのメインコンテナオブジェクト（参照渡し）です。
		nullptr
	);

	auto img = scratchImg.GetImage(0, 0, 0);//読み込んだメタデータの中にあるサブリソースの情報を取得するための関数。引数はミップマップレベル、配列スライス、キューブマップの面のインデックスを指定する。
	//得られる値はImg構造体のポインタ


	IDXGIFactory6* _dxgiFactory = nullptr; //グラボを探したり、画面の管理をしたりする大元の窓口
	ID3D12Device* _dev = nullptr;//アダプターを選択したのちにグラボの中に作られる。コマンドアロケーターとかテクスチャバッファなどが必要としてる分を切り出す働き
	ID3D12CommandAllocator* _cmdAllocator = nullptr;//コマンドアロケーターの宣言と初期化、コマンドリストより先に宣言する←コマンドアロケーターに小窓リストは保存されるから
	ID3D12GraphicsCommandList* _cmdList = nullptr;//コマンドリストの宣言と初期化
	ID3D12CommandQueue* _cmdQuene = nullptr;
	IDXGISwapChain4* _swapchain = nullptr;
	ID3D12DescriptorHeap* _descriptorHeap = nullptr;//ディスクリプタヒープの宣言と初期化 バッファーのデータをシェーダーで使う時に必要な仕様書
	ID3D12Fence* _fence = nullptr;
	ID3D12Resource* vertBuff = nullptr;//りそーすを作って保存するとこ
	ID3D12Resource* indexBuff = nullptr;
	ID3D12Resource* texBuff = nullptr;
	ID3D12PipelineState* mainPS = nullptr;
	ID3D12RootSignature* rootSignature = nullptr;
	
	D3D_FEATURE_LEVEL levels[] =
	{
		D3D_FEATURE_LEVEL_12_1,//レイトレーシングにも対応してる
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,//PS4くらいの描画性能
		D3D_FEATURE_LEVEL_11_0,
	};
	//FEATURE_LEVELについて　https://learn.microsoft.com/en-us/windows/win32/api/d3dcommon/ne-d3dcommon-d3d_feature_level

	result = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&_dxgiFactory));
#ifdef _DEBUG
	cout << "\n" << result << "\n";
#endif
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

	D3D_FEATURE_LEVEL featureLevel;

	for (auto lv : levels)//autoは変数の型を自動で保管してくれる	for (int i = 0; i < 4; ++i){D3D_FEATURE_LEVEL lv = levels[i];}
	{
		if (D3D12CreateDevice(nullptr, lv, IID_PPV_ARGS(&_dev)) == S_OK)//ppDeviceがNULLで関数が成功した場合、 S_OKではなくS_FALSEが返されます。今回の場合ちゃんと入っている。４引数は作るデバイスを格納する場所。
		{
			featureLevel = lv;
			break;
		}
	}

	result = _dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_cmdAllocator));//_devというクラス型の変数にCreateCommandAllocator　一つ目の引数はコマンドアロケーターの種類
#ifdef _DEBUG
	cout << result << "\n";
#endif
	result = _dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmdAllocator, nullptr, IID_PPV_ARGS(&_cmdList));//コマンドアロケーターはコマンドリストの命令が乗ってる　コマンドアロケーターを教本ではコマンドリストの招待と記述されている
#ifdef _DEBUG
	cout << result << "\n";
#endif
	//D3D12_COMMAND_QUEUE_DESC構造体について
	/*
	1	[in]	UINT	nodeMask	マルチGPU環境で、どのグラフィックボードでこのリストを作るかを指定するマスク値。単一GPUの場合は 0 を指定します。
	2	[in]	D3D12_COMMAND_LIST_TYPE	type	作成するコマンドリストの種類（DIRECT や BUNDLE など）。アロケータの種類と一致させる必要があります。
	3	[in]	ID3D12CommandAllocator*	pCommandAllocator	先ほど作成したコマンドアロケータのポインタをここに渡します。リストが命令を記録する際のメモリの提供元になります。
	4	[in, optional]	ID3D12PipelineState*	pInitialState	コマンドリストの初期パイプライン状態（PSO）。何も指定しない（既定値のままにする）場合は nullptr で大丈夫です。
	5	[out]	void**	ppCommandList	【出口】 作成されたコマンドリストの受け取り場所です。ここも通常は IID_PPV_ARGS(&myCommandList) の形でお決まりの指定をします。
	*/

	D3D12_COMMAND_QUEUE_DESC cmdQueneDesc = {};//コマンドキュー構造体の初期化
	//D3D12_COMMAND_QUEUE_DESC構造体について
	/*
1. Type (D3D12_COMMAND_LIST_TYPE)
コマンドキューの種類（型）を指定します。GPUにどのような種類の命令を処理させるかを決定する重要な設定です。

主な値:

D3D12_COMMAND_LIST_TYPE_DIRECT: 通常のレンダリング（描画）や計算、コピーなど、すべてのコマンドを実行できる万能なキューです。

D3D12_COMMAND_LIST_TYPE_COMPUTE: 非同期計算（コンピュートシェーダー）専用のキューです。

D3D12_COMMAND_LIST_TYPE_COPY: データの転送（メインメモリ ⇔ VRAM 間など）専用の軽量なキューです。

2. Priority (INT)
コマンドキューの優先順位を指定します。

GPUが複数のキューから命令を受け取っている場合に、どちらを優先して処理するかを制御します。

主に D3D12_COMMAND_QUEUE_PRIORITY 列挙型（NORMAL や HIGH など）の値、またはグローバルリアルタイム優先度（GLOBAL_REALTIME）を指定します。

3. Flags (D3D12_COMMAND_QUEUE_FLAGS)
コマンドキューの追加オプション（挙動のフラグ）を指定します。

主な値:

D3D12_COMMAND_QUEUE_FLAG_NONE: 特別なオプションなし（通常はこれ）。

D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT: GPUのタイムアウト（TDR: 応答停止と回復）を無効化します。ただし、これを使用するには開発者モードなどの特定の権限が必要です。

4. NodeMask (UINT)
マルチGPU（複数のグラフィックボードやアダプター）環境において、どのGPUノードでこのキューを動作させるかをビットマスクで指定します。

単一GPUの場合: 0 を設定します。

マルチGPUの場合: コマンドキューを適用したい物理ノードに対応するビットを1つだけ立てます（例: 1番目のGPUなら 1）。
	*/

	cmdQueneDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;//キューの種類を決める
	cmdQueneDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	cmdQueneDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	cmdQueneDesc.NodeMask = 0;

	result = _dev->CreateCommandQueue(&cmdQueneDesc, IID_PPV_ARGS(&_cmdQuene));
#ifdef _DEBUG
	cout << result << "\n";
#endif

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};

	//DXGI_SWAP_CHAIN_DESC1構造体について
	/*
	Width / Height
バックバッファーの解像度の幅と高さ。HWND用では 0 を指定するとウィンドウサイズから自動取得されます。

Format
画面の表示ピクセルフォーマット（DXGI_FORMAT）を指定します。

Stereo
ステレオ3D表示に対応するかどうか（TRUE / FALSE）。

SampleDesc
マルチサンプリング（MSAA）のパラメータ。フリップモデルでは Count=1, Quality=0 に設定する必要があります。

BufferUsage
バックバッファーの用途（レンダーターゲットやシェーダー入力など）。

BufferCount
スワップチェーンに含めるバッファーの総数（フリップモデルでは2〜16）。

Scaling
ターゲット出力のサイズに合わせた伸縮・サイズ変更の挙動（DXGI_SCALING）。

SwapEffect
画面更新（プレゼンテーション）のモデル（DXGI_SWAP_EFFECT）。モダンなアプリでは通常フリップモデルを指定します。

AlphaMode
バックバッファーの透過（アルファチャネル）の扱い（DXGI_ALPHA_MODE）。

Flags
スワップチェーンの動作オプションを制御するフラグ（DXGI_SWAP_CHAIN_FLAG）の組み合わせ。
	*/

	swapChainDesc.Width = window_width;
	swapChainDesc.Height = window_height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo = false;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	result = _dxgiFactory->CreateSwapChainForHwnd(_cmdQuene, hwnd, &swapChainDesc, nullptr, nullptr, (IDXGISwapChain1**)&_swapchain);
#ifdef _DEBUG
	cout << result << "\n";
#endif

	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};

	//D3D12_DESCRIPTOR_HEAP構造体について
	/*
	データ型： D3D12_DESCRIPTOR_HEAP_TYPE
	意味： このヒープに「何の目的のデータ（記述子）」を格納するかを指定します。
	データ型： UINT
	意味： このヒープの中に記述子を何個分確保するかという「部屋の数」を指定します。
	データ型： D3D12_DESCRIPTOR_HEAP_FLAGS
	意味： ヒープの動作オプションを指定します。
	データ型： UINT
	意味： パソコンに複数のグラフィックボード（GPU）が搭載されている場合（マルチアダプターシステム）、どのGPUに対してこのヒープを作成するかを指定するビットマスクです。
	*/

	descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	descriptorHeapDesc.NodeMask = 0;
	descriptorHeapDesc.NumDescriptors = 2;
	descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	result = _dev->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&_descriptorHeap));
#ifdef _DEBUG
	cout << result << "\n";
#endif

	D3D12_CPU_DESCRIPTOR_HANDLE handle = _descriptorHeap->GetCPUDescriptorHandleForHeapStart();//ヒープの「先頭の住所」を handle に入れる（例：ptr = 0x1000）HANDLEはディスクリプターヒープにあるディスクリプターの境目の位置のことです。
	//スワップチェーン上のバックバッファをディスクリプタに渡してるってこと？
	vector<ID3D12Resource*> _backbuffers(swapChainDesc.BufferCount);//ID3D12Resourceはテクスチャも、ポリゴンの頂点データ、行列のパラメータなど設定によって異なるデータに変わる。可変長配列の数は（）で指定できる
	for (UINT index = 0; index < swapChainDesc.BufferCount; ++index)
	{
		result = _swapchain->GetBuffer(index, IID_PPV_ARGS(&_backbuffers[index]));//IDXGISwapChain::GetBuffer_①何番目のバッファーを、②どの型で、③どこの変数に格納するか
		_dev->CreateRenderTargetView(_backbuffers[index], nullptr, handle);//バックバッファそれぞれに対しレンダーターゲットビューは作らないといけない。バックバッファのデータをレンダーターゲットビュー加工する指示
		handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);//「バックバッファをRTVという仕様書（ディスクリプタ）にして、いま handle が指しているヒープの始点に直接書き込む（ぶち込む）」という作業をこの関数が一瞬で行っています。同じとこにぶち込まないようにポインタをずらしてる
	}

	//レンダーターゲットビュー（RTV）は、ディスクリプター（記述子）の一種です。
	//なぜ「Resource」と「Descriptor」を分けるの？A.元データ（Resource）は1つの使い回しですが、「どう使うか（Descriptor）」によって、いくらでも役割を変えられるように、DX12ではあえて別々に分離しているのです。どう使うかの解釈方法は意外にも少なく４つだけ
	//ビュー＝ディスクリプターのこと。ディスクリプターは「どう使うかの解釈方法」なので、同じ元データでも、ディスクリプターを変えることで、用途を変えられる。例えば、テクスチャを「レンダーターゲットビュー」として使うか、「シェーダーリソースビュー」として使うかで、同じテクスチャでも用途が変わる。
	//_backbufferにバックバッファーが入る。for文で回すたびに配列にぶち込まれていくぅ


	//こっから頂点データをGPUに送って解釈してもらうためのゾーン？
	D3D12_HEAP_PROPERTIES heapProperties = //ヒープ設定構造体を設定
	{
		D3D12_HEAP_TYPE_UPLOAD,//CPUとのアクセスどうする？つなげないなら爆速帯域幅になるけど
		D3D12_CPU_PAGE_PROPERTY_UNKNOWN,//上の設定がカスタムなら使うやつ　UNKNOWNだと自動で適切なやつが選らばあれる
		D3D12_MEMORY_POOL_UNKNOWN,//RAMかVRAMどっちに置く？上の設定がカスタムなら使うやつ　カスタムは自動割り当てじゃないものを使いたいときに使う
		0,
		0,
	};

	D3D12_RESOURCE_DESC setResource = //リソース設定構造体を設定
	{
		D3D12_RESOURCE_DIMENSION_BUFFER,
		0,
		sizeof(vertices),//Unmap（アンマップ）を呼び出し、「データのコピーが終わったので、接続を解除します」とGPUに伝えています
		1,
		1,
		1,
		DXGI_FORMAT_UNKNOWN, // UNKNOWN → DXGI_FORMAT_UNKNOWN
		{1, 0},
		D3D12_TEXTURE_LAYOUT_ROW_MAJOR, // D3D12_TEXTURE_lAYOUT_ROW_MAJOR → D3D12_TEXTURE_LAYOUT_ROW_MAJOR
		D3D12_RESOURCE_FLAG_NONE, // NONE → D3D12_RESOURCE_FLAG_NONE
	};

	_dev->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&setResource,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vertBuff)
	);

	Vertex* vertMap = nullptr;//ポインターを受け取り保存するため

	vertBuff->Map(0, nullptr, (void**)&vertMap);//先ほど用意した vertMap にGPUメモリへ繋がる住所が格納されます。これにより、CPUから直接GPUのメモリへデータを書き込める状態になります。

	std::copy(begin(vertices), end(vertices), vertMap);//何をしているか: std::copy を使って、CPU側のメモリにある頂点配列（vertices）の中身を、先ほど取得したGPU側の住所（vertMap）へごっそりコピーしています。一応ここで頂点バッファが完成

	vertBuff->Unmap(0, nullptr);


	D3D12_VERTEX_BUFFER_VIEW vbView = //構造体
	{
		vertBuff->GetGPUVirtualAddress(),
		sizeof(vertices),
		sizeof(vertices[0]),
	};

	//こっから頂点バッファというただの数列の解釈の方法が描いてある説明書を作る。各頂点をここで区別できるようになる。
	//この時点では１頂点のデータがそれぞれ何を表しているかまでは設定できてない

	//こっからインデックスバッファを作ってく
	setResource.Width = sizeof(indices);
	_dev->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&setResource,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&indexBuff)
	);

	short* mappedIndex = nullptr;
	indexBuff->Map(0, nullptr, (void**)&mappedIndex);
	std::copy(begin(indices), end(indices), mappedIndex);
	indexBuff->Unmap(0, nullptr);

	//こっからインデックスバッファービューつくってく

	D3D12_INDEX_BUFFER_VIEW ibView = {};
	ibView.BufferLocation = indexBuff->GetGPUVirtualAddress();
	ibView.Format = DXGI_FORMAT_R16_UINT;
	ibView.SizeInBytes = sizeof(indices);

	//こっからテクスチャバッファー作っていく

	D3D12_HEAP_PROPERTIES texHeapProperties = {};//メモリの種類を決める構造体

	//テクスチャは普通GPUに置くのでD3D12_HEAP_TYPE_DEFAULTを使うことが多いが、今回はCPUから直接書き込むのでD3D12_HEAP_TYPE_CUSTOMを使う
	texHeapProperties.Type = D3D12_HEAP_TYPE_CUSTOM;
	texHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	texHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
	texHeapProperties.CreationNodeMask = 0;
	texHeapProperties.VisibleNodeMask = 0;

	//テクスチャリソース設定
	D3D12_RESOURCE_DESC texResource = {};
	texResource.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texResource.Width = metadata.width;
	texResource.Height = metadata.height;
	texResource.DepthOrArraySize = metadata.arraySize;//アニメーションのついたものだとこの値が増える。今回は1枚のテクスチャなので1
	texResource.SampleDesc.Count = 1;
	texResource.SampleDesc.Quality = 0;
	texResource.MipLevels = metadata.mipLevels;
	texResource.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);
	texResource.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texResource.Flags = D3D12_RESOURCE_FLAG_NONE;

	result = _dev->CreateCommittedResource(
		&texHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&texResource,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&texBuff)
	);
#ifdef _DEBUG
	cout << result << "\n";
#endif

	result = texBuff->WriteToSubresource
	(
		0,
		nullptr,
		img->pixels,
		img->rowPitch,
		img->slicePitch
		/*0,
		nullptr,
		texturedata.data(),
		sizeof(TexRGBA) * 256,
		sizeof(TexRGBA) * texturedata.size()
		*/
	);

	//ディスクリプターヒープを作る

	ID3D12DescriptorHeap* texDescriptorHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC texDescriptorHeapDesc = {};
	texDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	texDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	texDescriptorHeapDesc.NodeMask = 0;
	texDescriptorHeapDesc.NumDescriptors = 1;
	result = _dev->CreateDescriptorHeap(&texDescriptorHeapDesc, IID_PPV_ARGS(&texDescriptorHeap));

	//ヒープに入れるディスクリプタの設定をする
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 1;

	_dev->CreateShaderResourceView(texBuff, &srvDesc, texDescriptorHeap->GetCPUDescriptorHandleForHeapStart());//ここでディスクリプターとバッファーを結びつける

	//ディスクリプタテーブルを作る
	/*
	ディスクリプタテーブルについて
	ディスクリプタヒープのどこからどこまでが「このシェーダーで使うディスクリプタの範囲ですよ」と指定するのがディスクリプタテーブルです。ディスクリプタテーブルを作ることで、シェーダーは「この範囲のディスクリプタを使ってね」と理解できるようになります。
	*/

#ifdef _DEBUG
	cout << result << "\n";
#endif

	//こっからシェーダーを読み込むための準備
	ID3DBlob* _vsBlob = nullptr;
	ID3DBlob* _psBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;

	D3DCompileFromFile(
		L"VertexShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicVS",
		"vs_5_0",
		D3DCOMPILE_DEBUG,
		0,
		&_vsBlob,
		&errorBlob
	);

	D3DCompileFromFile(
		L"PixelShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicPS",
		"ps_5_0",
		D3DCOMPILE_DEBUG,
		0,
		&_psBlob,
		&errorBlob
	);
	//シェーダをコンパイル
	/*
	ゲーム起動 
  ↓
シェーダーファイルを読み込む
  ↓
コンパイル関数を呼ぶ（D3DCompile など）★ここ！
  ↓
パイプライン状態（PSO）を作る
  ↓
ゲーム本編（メインループ開始）
	*/
	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{ "POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 },
		{"TEXCOORD", 0,DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 }
	};

	//こっからパイプラインステートオブジェクトを作るための準備
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline = {};
	gpipeline.pRootSignature = nullptr;
	gpipeline.VS.pShaderBytecode = _vsBlob->GetBufferPointer();
	gpipeline.VS.BytecodeLength = _vsBlob->GetBufferSize();
	gpipeline.PS.pShaderBytecode = _psBlob->GetBufferPointer();
	gpipeline.PS.BytecodeLength = _psBlob->GetBufferSize();

	gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;//中身は0xffffffff

	//
	gpipeline.BlendState.AlphaToCoverageEnable = false;
	gpipeline.BlendState.IndependentBlendEnable = false;

	D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc = {};

	//ひとまず加算や乗算やαブレンディングは使用しない
	renderTargetBlendDesc.BlendEnable = false;
	renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	//ひとまず論理演算は使用しない
	renderTargetBlendDesc.LogicOpEnable = false;

	gpipeline.BlendState.RenderTarget[0] = renderTargetBlendDesc;


	gpipeline.RasterizerState.MultisampleEnable = false;//まだアンチェリは使わない
	gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;//カリングしない
	gpipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;//中身を塗りつぶす
	gpipeline.RasterizerState.DepthClipEnable = true;//深度方向のクリッピングは有効に

	//残り
	gpipeline.RasterizerState.FrontCounterClockwise = false;
	gpipeline.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	gpipeline.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	gpipeline.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	gpipeline.RasterizerState.AntialiasedLineEnable = false;
	gpipeline.RasterizerState.ForcedSampleCount = 0;
	gpipeline.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;


	gpipeline.DepthStencilState.DepthEnable = false;
	gpipeline.DepthStencilState.StencilEnable = false;

	gpipeline.InputLayout.pInputElementDescs = inputLayout;//レイアウト先頭アドレス
	gpipeline.InputLayout.NumElements = _countof(inputLayout);//レイアウト配列数

	gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;//ストリップ時のカットなし
	gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;//三角形で構成

	gpipeline.NumRenderTargets = 1;//今は１つのみ
	gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;//0～1に正規化されたRGBA
	
	gpipeline.SampleDesc.Count = 1;//サンプリングは1ピクセルにつき１
	gpipeline.SampleDesc.Quality = 0;//クオリティは最低

	D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	samplerDesc.MinLOD = 0.0f;
	samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	samplerDesc.ShaderRegister = 0;

	D3D12_DESCRIPTOR_RANGE descriptorRange = {};
	descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // シェーダーリソースビュー
	descriptorRange.NumDescriptors = 1; // ディスクリプタの数
	descriptorRange.BaseShaderRegister = 0; // シェーダーレジスタのベース
	descriptorRange.RegisterSpace = 0; // レジスタスペース
	descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // ディスクリプタテーブルの先頭からのオフセット

	D3D12_ROOT_DESCRIPTOR_TABLE descriptorTable = {};
	descriptorTable.NumDescriptorRanges = 1;
	descriptorTable.pDescriptorRanges = &descriptorRange;

	D3D12_ROOT_PARAMETER rootParameter = {};
	rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; //ピクセルシェーダーからアクセスできる
	rootParameter.DescriptorTable = descriptorTable;

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootSignatureDesc.pParameters = &rootParameter;
	rootSignatureDesc.NumParameters = 1;
	rootSignatureDesc.pStaticSamplers = &samplerDesc;
	rootSignatureDesc.NumStaticSamplers = 1;

	ID3DBlob* rootSigBlob = nullptr;
	result = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootSigBlob, &errorBlob);
	result = _dev->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	rootSigBlob->Release();

	gpipeline.pRootSignature = rootSignature;
	ID3D12PipelineState* _pipelinestate = nullptr;
	result = _dev->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(&_pipelinestate));

	D3D12_VIEWPORT viewport = {};
	viewport.Width = 2 * window_width / 3;
	viewport.Height = 2 * window_height / 3;
	viewport.TopLeftX = window_width / 6;
	viewport.TopLeftY = window_height / 6;
	viewport.MaxDepth = 1.0;
	viewport.MinDepth = 0.0;

	D3D12_RECT scissorrect = {};
	scissorrect.top = viewport.TopLeftY;
	scissorrect.left = viewport.TopLeftX;
	scissorrect.right = viewport.TopLeftX + viewport.Width;
	scissorrect.bottom = viewport.TopLeftY + viewport.Height;

	ShowWindow(hwnd, SW_SHOW);

	_cmdList->Close();

	MSG msg = {};
	
	UINT fenceVal = 0;
	UINT frame = 0;
	result = _dev->CreateFence(fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence));//fenceはCPUがGPUに送ったコマンドキュー　フェンスの値はGPUが終えた処理のフレーム番号
#ifdef _DEBUG
	cout << result << "\n";
#endif

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
		auto backBufferindex = _swapchain->GetCurrentBackBufferIndex();//表裏ある2つの画面の内、描画計算する裏の画面を特定している？

		auto rtvH = _descriptorHeap->GetCPUDescriptorHandleForHeapStart();//とりあえずディスクリプタヒープの先頭を取得する
		rtvH.ptr += backBufferindex * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV); //バックバッファのメモリの先頭位置を取得するためバックバッファのインデックスが 0 のとき（1枚目の画面）：0 * 1マスのサイズ を足すので、移動量は 0。つまり、マンションの先頭である $S_1$ の住所 になります。バックバッファのインデックスが 1 のとき（2枚目の画面）：1 * 1マスのサイズ を足すので、ちょうど1部屋分後ろにズレます。つまり、$S_2$ の住所 になります。

		D3D12_RESOURCE_BARRIER BarrierDesc = {};
		BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		BarrierDesc.Transition.pResource = _backbuffers[backBufferindex];
		BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		_cmdList->ResourceBarrier(1, &BarrierDesc);

		_cmdList->OMSetRenderTargets(1, &rtvH, false, nullptr);
		//こっから書く内容をコマンドリストに書き込む
		float clearColor[] = { 0.1f, 0.4f,  (float)sin(0.01*frame), 1.0f };//画面の色決め
		frame++;
		_cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);
		_cmdList->SetPipelineState(_pipelinestate);
		_cmdList->RSSetViewports(1, &viewport);
		_cmdList->RSSetScissorRects(1, &scissorrect);
		_cmdList->SetGraphicsRootSignature(rootSignature);
		_cmdList->SetDescriptorHeaps(1, &texDescriptorHeap);
		_cmdList->SetGraphicsRootDescriptorTable(0, texDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

		_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		_cmdList->IASetVertexBuffers(0, 1, &vbView);
		_cmdList->IASetIndexBuffer(&ibView);
		_cmdList->DrawInstanced(4, 1, 0, 0);
		_cmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);

		BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;//ここまでは書き込みよう
		BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;//ここからは画面表示用
		_cmdList->ResourceBarrier(1, &BarrierDesc);

		_cmdList->Close();
		
		ID3D12CommandList* cmdlists[] = { _cmdList };
		_cmdQuene->ExecuteCommandLists(1, cmdlists);//コマンドアロケータを実行しやがれ

		_cmdQuene->Signal(_fence, ++fenceVal);//GPUに対し今やってる描画計算が終わったらfenceの値を１個増やす指示　GPUの仕事の完了をCPUが知るためにコマンドキューの最後にfenceの値を増やす指示書を配置する　こいつはあくまで「コマンドキューの終端にフェンスをいじる指示をするだけの関数？
		if (_fence->GetCompletedValue() != fenceVal)//GetCompletedValueは現在のfenceの値を返す関数
		{
			auto event = CreateEvent(nullptr, false, false, nullptr);//作られるデータは「シグナル状態（ON/OFF）」と「待ち行列（スレッドのリスト）」を記録した小さなデータ構造（構造体）のポインタ

			_fence->SetEventOnCompletion(fenceVal, event);//フェンスの値がfenceValつまり描画計算が終わったらeventが発生する

			WaitForSingleObject(event, INFINITE);//特定の条件（シグナル）が満たされるか、指定した時間が経過するまで、プログラムの処理を一時停止して待つ

			CloseHandle(event);
		}

		//この条件分岐はCPUの指示をまだGPUが終えてないときに行う

		_cmdAllocator->Reset();

		_cmdList->Reset(_cmdAllocator, nullptr);//新しい命令を受け付けるモードに

		_swapchain->Present(1, 0);//フリップ
	}
}

//nullptrで今はない変数をないものとして扱っている