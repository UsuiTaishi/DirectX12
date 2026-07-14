#include<Windows.h>
#include <tchar.h>
#include <DirectXMath.h>
#include <string>
#include <fbxsdk.h>
#include<vector>
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

//描画に必要なインフラをまとめる構造体、モデルクラスとかに渡す
struct RenderContext {
	ID3D12Device* device;
	ID3D12GraphicsCommandList* cmdList;
	ID3D12DescriptorHeap* srvHeap;
};

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
	D3D12_VIEWPORT m_viewport = {};
	D3D12_RECT m_scissorrect = {};
	D3D12_RESOURCE_BARRIER barrier = {};
public:
	HRESULT Init(HWND hWnd, int width, int height);
	HRESULT InitPipeline();
	RenderContext CreateRenderContext();
	void BeginFrame();
	void EndFrame();
	//HRESULT Release();
};

class Model {
private:
	//頂点バッファーの頂点レイアウトの参照に
	struct Vertex {
		float Position[3];
		float Normal[3];
		float UV[2];
		float Tangent[3];
	};
	Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer = nullptr;
	UINT vertexCount = 0;
	D3D12_VERTEX_BUFFER_VIEW m_vbView = {};
	struct MeshData
	{
	std::string materialName;
	std::vector<Vertex> m_mVertexData;
	};
	std::vector<MeshData> m_meshes;
public:
	bool LoadModel(const RenderContext& context, const std::string& filename);
	void LoadMesh(fbxsdk::FbxMesh* mesh);
	void CreateVertexBuffer(const RenderContext& context, std::vector<Vertex>& vertices);
	bool Draw(const RenderContext& context);
};