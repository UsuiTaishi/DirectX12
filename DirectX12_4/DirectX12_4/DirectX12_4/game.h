#include<Windows.h>
#include <tchar.h>
#include <DirectXMath.h>
#include <string>

using namespace DirectX;

// すべて中身は書かずに、末尾をセミコロン「;」で終わらせる形にします

const UINT BACK_BUFFER_COUNT = 2;

HWND CreateGameWindow(HINSTANCE hInstance, int width, int height, const TCHAR* title);

void EnableDebugLayer();

void CheckResult(HRESULT result, std::string process);

void OnClose(HWND hWnd);

void OnDestroy();

class DX12App {
private:
	IDXGIFactory6* m_dxgiFactory = nullptr;
	ID3D12Device* m_dev = nullptr;
	ID3D12CommandAllocator* m_cmdAllocators[BACK_BUFFER_COUNT] = {};
	ID3D12GraphicsCommandList* m_cmdList = nullptr;
	ID3D12CommandQueue* m_cmdQueue = nullptr;
	IDXGISwapChain4* m_swapChain = nullptr;
	ID3D12DescriptorHeap* m_rtvHeap = nullptr;
	ID3D12Fence* m_fence = nullptr;
	UINT64 m_fenceVal;
	ID3D12Resource* m_rtvResources[BACK_BUFFER_COUNT] = {};
	ID3D12RootSignature* m_rootSignature = nullptr;
	ID3D12PipelineState* m_pipelineState = nullptr;
public:
	HRESULT Init(HWND hWnd, int width, int height);
	HRESULT InitPipeline();
	void Render();
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