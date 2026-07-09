#include<Windows.h>
#include <tchar.h>
#include <DirectXMath.h>
#include <string>
#include<wrl/client.h>

// すべて中身は書かずに、末尾をセミコロン「;」で終わらせる形にします

const UINT BACK_BUFFER_COUNT = 2;

HWND CreateGameWindow(HINSTANCE hInstance, int width, int height, const TCHAR* title);

void EnableDebugLayer();

void CheckResult(HRESULT result, std::string process);

void OnClose(HWND hWnd);

void OnDestroy();

class DX12App {
private:
	Microsoft::WRL::ComPtr<IDXGIFactory6> m_dxgiFactory = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Device> m_dev = nullptr;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_cmdAllocators[BACK_BUFFER_COUNT] = {};
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_cmdList = nullptr;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_cmdQueue = nullptr;
	Microsoft::WRL::ComPtr<IDXGISwapChain4> m_swapChain = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap = nullptr;//ゲームプレイ中、新しい3Dモデル（FBX）や画像をロードするたびに、このヒープの「空いている場所」に新しいビュー（SRV）を書き込みます。テクスチャ、定数バッファ、マテリアルデータなど。
	UINT MAX_SRV_COUNT = 4096;
	UINT m_nextSrvIndex = 0;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_smpHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_depthBuffer = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Fence> m_fence = nullptr;
	UINT64 m_fenceVal;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_buckBuffer[BACK_BUFFER_COUNT] = {};
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState = nullptr;
public:
	HRESULT Init(HWND hWnd, int width, int height);
	HRESULT InitPipeline();
	void Render();
	//HRESULT Release();
};

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

/*
HRESULT InitDX3D
(
	HWND _hwnd,
	IDXGIFactory6*& _dxgiFactory,
	ID3D12Device*& _dev,
	ID3D12CommandAllocator* _GraphicsCmdAllocators[2],
	ID3D12GraphicsCommandList*& _GraphicsCmdList,
	ID3D12CommandQueue*& _GraphicsCmdQuene,
	DXGI_SWAP_CHAIN_DESC1& _swapChainDesc,
	IDXGISwapChain4*& _swapChain,
	ID3D12DescriptorHeap*& _rtvHeap,
	ID3D12Fence*& _fence
);
*/