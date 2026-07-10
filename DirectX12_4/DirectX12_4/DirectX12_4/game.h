#include<Windows.h>
#include <tchar.h>
#include <DirectXMath.h>
#include <string>
#include <fbxsdk.h>
#include<map>
#include<wrl/client.h>
#include<DirectXMath.h>

// すべて中身は書かずに、末尾をセミコロン「;」で終わらせる形にします

const UINT BACK_BUFFER_COUNT = 2;

HWND CreateGameWindow(HINSTANCE hInstance, int width, int height, const TCHAR* title);

void EnableDebugLayer();

void CheckResult(HRESULT result, std::string process);

void OnClose(HWND hWnd);

void OnDestroy();

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

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

//描画に必要なインフラをまとめる構造体、モデルクラスとかに渡す
struct RenderContext {
	ID3D12Device* device;
	ID3D12GraphicsCommandList* cmdList;
	ID3D12DescriptorHeap* sevHeap;
};

class Model {
private:	
	Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer = nullptr;
	D3D12_VERTEX_BUFFER_VIEW m_vbView = {};
	//マテリアルマップ
	struct MATERIAL {
		std::vector<float> materials;
		std::string textureFileName;
	};
	std::map<std::string, MATERIAL> m_materialMap;//マテリアル名、マテリアル情報
	struct VERTEX {
		DirectX::XMFLOAT3 Position;
		DirectX::XMFLOAT3 Normal;
		DirectX::XMFLOAT2 UV;
		DirectX::XMFLOAT3 Tangent;
		DirectX::XMFLOAT4 Color;
	};
public:
	bool LoadModel(const RenderContext& context, const std::string& filename);
	void LoadMaterial(fbxsdk::FbxSurfaceMaterial* material);
	bool Draw(const RenderContext& context);
};