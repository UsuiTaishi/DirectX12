#include<Windows.h>
#include <tchar.h>
#include<string>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <vector>
#include <d3dcompiler.h>
#include"renderer.h"
#include<cassert>
#include<unordered_map>
#include<DirectXTex.h>



#ifdef _DEBUG
#include <iostream>
#endif

#include <fbxsdk.h>
#pragma comment(lib, "libfbxsdk-md.lib")
#pragma comment(lib, "libxml2-md.lib")
#pragma comment(lib, "zlib-md.lib")
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib,"DirectXTex.lib")

using namespace std;
using namespace DirectX;
using namespace Microsoft::WRL;

bool Model::LoadModel(const RenderContext& context, const string& filename)
{
	fbxsdk::FbxManager* fbxManager = fbxsdk::FbxManager::Create();//FBXManager

	FbxIOSettings* ios = FbxIOSettings::Create(fbxManager, IOSROOT);//IO設定第二引数は設定のルートパスを表している。インポートの設定
	fbxManager->SetIOSettings(ios);

	FbxImporter* fbxImporter = FbxImporter::Create(fbxManager, "");

	FbxScene* fbxScene = FbxScene::Create(fbxManager, "My scene");//シーンの作成。このシーンにマテリアルとかメッシュとかを置いていく

	if (!fbxImporter->Initialize(filename.c_str(), -1, NULL))
	{
		OutputDebugStringA("[FAILED] FBXファイルの読み込みに失敗しました。パスを確認してください。\n");
		return false;
	}

	fbxImporter->Initialize(filename.c_str(), -1, NULL);//第三引数はインポート時の挙動（テクスチャやアニメーションを読み込むかなど）を制御する設定オブジェクトへのポインタを指定します。

	fbxImporter->Import(fbxScene);
	fbxImporter->Destroy();//データはシーンにあるのでもういらない

	FbxGeometryConverter fbxConverter(fbxManager);
	fbxConverter.SplitMeshesPerMaterial(fbxScene, true);//マテリアルごとにメッシュを分割する。
	fbxConverter.Triangulate(fbxScene, true, false);//ポリゴンを三角形化する。

	int numMesh = fbxScene->GetSrcObjectCount<FbxMesh>();
	for (int i = 0; i < numMesh; i++)
	{
		FbxMesh* mesh = fbxScene->GetSrcObject<FbxMesh>(i);
		LoadMesh(mesh);
	}
	//メッシュの統合処理
	vector<Vertex> allVertices;
	vector<UINT32> allIndexes;
	uint32_t vertexOffset = 0;
	UINT indexOffset = 0;
	for (MeshData& mesh : m_meshes)
	{
		//マテリアルごとのメッシュのインデックスバッファの開始位置とインデックス数を設定
		mesh.startIndex = indexOffset;//ドロー関数で使います
		mesh.indexCount = static_cast<UINT>(mesh.m_index.size());;//ドロー関数で使います

		allVertices.insert(allVertices.end(), mesh.m_mVertexData.begin(), mesh.m_mVertexData.end());
		for (uint32_t index : mesh.m_index)
		{
			allIndexes.push_back(index + vertexOffset);//取り出した配列内にある頂点インデックスにオフセット分を足している。
		}
		vertexOffset += static_cast<uint32_t>(mesh.m_mVertexData.size());
		indexOffset += mesh.indexCount;
	}

	// 頂点バッファを作成
	CreateVertexBuffer(context, allVertices);
	CreateIndexBuffer(context, allIndexes);
	allVertexCount = static_cast<UINT>(allIndexes.size());

	fbxScene->Destroy();
	fbxManager->Destroy();
	
	return true;
}

void Model::LoadMesh(FbxMesh* mesh)
{
	MeshData data = {};
	UINT vertexCount = mesh->GetPolygonVertexCount();//全頂点数を調べる(blenderなどで表示される頂点数ではなく。ポリゴン角)
	int* vertexIndex = mesh->GetPolygonVertices();//頂点番号配列の先頭ポインタを取得。インデックスバッファ

	//頂点座標用
	FbxVector4* vertexPos = mesh->GetControlPoints();//x,y,z,w配列を取得（座標は記録されてないあくまで(x1,y1,z1,w1)[0],(x2,y2,z3,w3)[1],()[3],()[4]....）、配列の先頭ポインタが返される。

	//ノーマル用
	FbxArray<FbxVector4> normals;
	mesh->GetPolygonVertexNormals(normals);//引数は結果を収納するところ

	//UV用
	FbxStringList uvset_names;
	mesh->GetUVSetNames(uvset_names);//UVセット名前リストを取得。
	FbxArray<FbxVector2> texcoord;
	mesh->GetPolygonVertexUVs(uvset_names.GetStringAt(0), texcoord);

	//接空間用 専用ヘルパー関数がないのでレイヤー構造から直接取得します。
	if (!mesh->GetElementTangentCount())
	{
		mesh->GenerateTangentsData(0, false, false);
	}
	//レイヤー0から接空間を取得
	FbxGeometryElementTangent* tangentElement = mesh->GetElementTangent(0);
	FbxLayerElementArrayTemplate<FbxVector4>& tangent = tangentElement->GetDirectArray();

	data.m_mVertexData = {};
	data.m_mVertexData.resize(vertexCount);//メモリ確保

	for (int i = 0; i < vertexCount; i++)
	{
		int controlPointIndex = vertexIndex[i];//インデックスバッファ用にインデックスに従って通りに座標を配置していきたいので


		//MESH構造体に格納（float型へ変換）まずは頂点座標から
		data.m_mVertexData[i].Position[0] = static_cast<float>(-vertexPos[controlPointIndex][0]);//0つまりx座標
		data.m_mVertexData[i].Position[1] = static_cast<float>(vertexPos[controlPointIndex][1]);//0つまりy座標
		data.m_mVertexData[i].Position[2] = static_cast<float>(vertexPos[controlPointIndex][2]);//0つまりz座標高さ
		//これでv0(座標),v1(座標),v2(座標),v0(座標),v1(座標),v3(座標)みたいな感じでインデックス順に座標データの配列ができる重複してるとこもあるのでDirectXに渡してインデックスを利用する際は重複を消してインデックスを付与しないといけない

		//ノーマル
		data.m_mVertexData[i].Normal[0] = static_cast<float>(-normals[i][0]);
		data.m_mVertexData[i].Normal[1] = static_cast<float>(normals[i][1]);
		data.m_mVertexData[i].Normal[2] = static_cast<float>(normals[i][2]);

		//UV
		data.m_mVertexData[i].UV[0] = static_cast<float>(texcoord[i][0]);
		data.m_mVertexData[i].UV[1] = static_cast<float>(texcoord[i][1]);

		//接空間
		data.m_mVertexData[i].Tangent[0] = static_cast<float>(-tangent[i][0]);
		data.m_mVertexData[i].Tangent[1] = static_cast<float>(tangent[i][1]);
		data.m_mVertexData[i].Tangent[2] = static_cast<float>(tangent[i][2]);
	}
	
	//重複のない頂点データ配列を作る。
	MeshData optimizedVertexData;//重複を消した頂点データを入れていく配列

	optimizedVertexData.m_mVertexData.clear();
	optimizedVertexData.m_index.clear();

	unordered_map<Vertex, uint32_t, VertexHash> dictionary;//キーは頂点データ、バリューは新たな頂点データ配列の頂点番号
	
	for (const auto& vertex : data.m_mVertexData)
	{
		auto serchingVertexData = dictionary.find(vertex);//このループで扱う頂点データの組み合わせは辞書の中にあるかな？あったらそれがある辞書のアドレスみたいなやつ（イテレータ）が返される。見つかんなかったら最後のイテレータend()が返される

		if (serchingVertexData != dictionary.end())//辞書に保存済みのデータに出会ったら
		{
			optimizedVertexData.m_index.push_back(serchingVertexData->second);//その頂点データ（キー）に対応したインデックス（バリュー）をインデックス配列にぶち込んでいく
		}
		else//辞書に未保存のデータに出会ったら　
		{
			uint32_t newIndex = static_cast<uint32_t>(optimizedVertexData.m_mVertexData.size());//optimizedVertexData.m_mVertexData.size()は重複してない頂点データに遭遇するほど大きくなっていくつまりこの値が頂点データに対する識別番号インデックスとなる

			optimizedVertexData.m_mVertexData.push_back(vertex);//辞書のキーに対応する頂点データを新たに追加
			optimizedVertexData.m_index.push_back(newIndex);//インデックス配列に新たな頂点データのインデックスを追加

			dictionary[vertex] = newIndex;
		}
		//optimizedIVertexdata.Indexは増加し続けるが頂点データは新たな組み合わせが出ないと配列にプッシュされない。
	}
	
	//なんのマテリアルが割り当てられてるか調べるこれも例や構造から直接取得します。
	if (mesh->GetElementMaterialCount() == 0)
	{
		optimizedVertexData.materialName = "";
	}
	else
	{
		FbxLayerElementMaterial* material = mesh->GetElementMaterial(0);
		int index = material->GetIndexArray().GetAt(0);
		FbxNode* node = mesh->GetNode();
		FbxSurfaceMaterial* surface_material = nullptr;
		if (node != nullptr) {
			surface_material = node->GetSrcObject<FbxSurfaceMaterial>(index);//レイヤー構造似ないなら直接取ってきて
		}

		if (surface_material != nullptr) {
			optimizedVertexData.materialName = surface_material->GetName();
		}
		else {
			optimizedVertexData.materialName = "";
		}
	}

	m_meshes.push_back(optimizedVertexData);

}

void Model::CreateVertexBuffer(const RenderContext& context, std::vector<Vertex>& vertices)
{
	HRESULT result;
	D3D12_RESOURCE_DESC vertexResourceDesc = {};
	vertexResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	vertexResourceDesc.Width = sizeof(Vertex) * vertices.size();
	vertexResourceDesc.Height = 1;
	vertexResourceDesc.SampleDesc.Count = 1;
	vertexResourceDesc.SampleDesc.Quality = 0;
	vertexResourceDesc.DepthOrArraySize = 1;
	vertexResourceDesc.MipLevels = 1;
	vertexResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	vertexResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	vertexResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12_HEAP_PROPERTIES vertexHeap = {};
	vertexHeap.Type = D3D12_HEAP_TYPE_UPLOAD;//CPUからアクセス可能
	vertexHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;//カスタムのとき使うやつ
	vertexHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;//カスタムのとき使うやつ

	result = context.device->CreateCommittedResource
	(
		&vertexHeap,
		D3D12_HEAP_FLAG_NONE,
		&vertexResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,//Gpuからは読み取り専用
		nullptr,
		IID_PPV_ARGS(m_vertexBuffer.GetAddressOf())
	);
#ifdef _DEBUG
	CheckResult(result, "CommittedVertexResources");
#endif

	void* pMappedData = nullptr;
	m_vertexBuffer->Map(0, nullptr, &pMappedData);
	Vertex* pVertexData = static_cast<Vertex*>(pMappedData);

	size_t offset = 0;
	memcpy(pMappedData, vertices.data(), sizeof(Vertex) * vertices.size());
	m_vertexBuffer->Unmap(0, nullptr);

	m_vbView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
	m_vbView.SizeInBytes = sizeof(Vertex) * vertices.size();
	m_vbView.StrideInBytes = sizeof(Vertex);
}

void Model::CreateIndexBuffer(const RenderContext& context, std::vector<UINT32> index)
{
	HRESULT result;
	D3D12_RESOURCE_DESC indexResourceDesc = {};
	indexResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	indexResourceDesc.Width = sizeof(UINT32) * index.size();
	indexResourceDesc.Height = 1;
	indexResourceDesc.DepthOrArraySize = 1;
	indexResourceDesc.MipLevels = 1;
	indexResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	indexResourceDesc.SampleDesc.Count = 1;
	indexResourceDesc.SampleDesc.Quality = 0;
	indexResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	indexResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12_HEAP_PROPERTIES indexHeap = {};
	indexHeap.Type = D3D12_HEAP_TYPE_UPLOAD;//CPUからアクセス可能
	indexHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;//カスタムのとき使うやつ
	indexHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;//カスタムのとき使うやつ

	result = context.device->CreateCommittedResource
	(
		&indexHeap,
		D3D12_HEAP_FLAG_NONE,
		&indexResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,//Gpuからは読み取り専用
		nullptr,
		IID_PPV_ARGS(m_indexBuffer.GetAddressOf())
	);
#ifdef _DEBUG
	CheckResult(result, "CommittedIndexResources");
#endif
	void* pMappedData = nullptr;
	m_indexBuffer->Map(0, nullptr, &pMappedData);
	uint32_t* pIndexData = static_cast<uint32_t*>(pMappedData);
	memcpy(pIndexData, index.data(), sizeof(uint32_t) * index.size());
	m_indexBuffer->Unmap(0, nullptr);

	m_ibView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
	m_ibView.SizeInBytes = static_cast<UINT>(sizeof(uint32_t) * index.size());
	m_ibView.Format = DXGI_FORMAT_R32_UINT;
}

void Model::CreateTextureBuffer(const RenderContext& context, const TextureSet& textures)
{
	HRESULT result;

	result = CoInitializeEx(0, COINIT_MULTITHREADED);

	TexMetadata metaData = {};
	ScratchImage scratchImg = {};

	vector<string> texPaths = textures.GetPathsArray();

	UINT incrementSIze = context.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	for (string& texture : texPaths)
	{
		result = LoadFromWICFile(ConvertWString(texture).c_str(), WIC_FLAGS_NONE, &metaData, scratchImg);

		const Image* img = scratchImg.GetImage(0, 0, 0);
		ComPtr<ID3D12Resource> tempUploadBuffer;
		ComPtr<ID3D12Resource> tempTextureBuffer;
		//アップロードリソース、ヒープ設定
		D3D12_RESOURCE_DESC textureUploadResourceDesc = {};
		textureUploadResourceDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metaData.dimension);
		textureUploadResourceDesc.Alignment = 0;
		textureUploadResourceDesc.Width = metaData.width;
		textureUploadResourceDesc.Height = metaData.height;
		textureUploadResourceDesc.DepthOrArraySize - 1;
		textureUploadResourceDesc.MipLevels = metaData.mipLevels;
		textureUploadResourceDesc.SampleDesc.Count = 1;
		textureUploadResourceDesc.SampleDesc.Quality = 0;
		textureUploadResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		textureUploadResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		D3D12_HEAP_PROPERTIES textureUploadHeap = {};
		textureUploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;//CPUからアクセス可能
		textureUploadHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;//カスタムのとき使うやつ
		textureUploadHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;//カスタムのとき使うやつ

		//デフォルトヒープ用の設定
		D3D12_RESOURCE_DESC textureResourceDesc = {};
		textureResourceDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metaData.dimension);
		textureResourceDesc.Alignment = 0;
		textureResourceDesc.Width = metaData.width;
		textureResourceDesc.Height = metaData.height;
		textureResourceDesc.DepthOrArraySize - 1;
		textureResourceDesc.MipLevels = metaData.mipLevels;
		textureResourceDesc.SampleDesc.Count = 1;
		textureResourceDesc.SampleDesc.Quality = 0;
		textureResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		textureResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		D3D12_HEAP_PROPERTIES textureHeap = {};
		textureHeap.Type = D3D12_HEAP_TYPE_DEFAULT;//広帯域メモリ
		textureHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;//カスタムのとき使うやつ
		textureHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;//カスタムのとき使うやつ

		result = context.device->CreateCommittedResource
		(
			&textureUploadHeap,
			D3D12_HEAP_FLAG_NONE,
			&textureUploadResourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,//Gpuからは読み取り専用
			nullptr,
			IID_PPV_ARGS(tempUploadBuffer.GetAddressOf())
		);
#ifdef _DEBUG
		CheckResult(result, "CommittedTextureResourcesToUploadHeaqp");
#endif

		result = context.device->CreateCommittedResource
		(
			&textureHeap,
			D3D12_HEAP_FLAG_NONE,
			&textureResourceDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,//コピー先として使う
			nullptr,
			IID_PPV_ARGS(tempTextureBuffer.GetAddressOf())
		);
#ifdef _DEBUG
		CheckResult(result, "CommittedTextureResources");
#endif
		//アップロードバッファーへテクスチャのピクセルデータを描きこむ
		m_uploadBuffer.push_back(tempUploadBuffer);
		m_textureBuffer.push_back(tempTextureBuffer);

		void* pTexCPUVirtualAddress = nullptr;
		m_uploadBuffer.back()->Map(0, nullptr, &pTexCPUVirtualAddress);
		memcpy(pTexCPUVirtualAddress, img->pixels, img->rowPitch * img->height);
		m_uploadBuffer.back()->Unmap(0, nullptr);

		//デフォルトヒープへのコピーはコマンドリストで行うので、コマンドリストを取得してコピーする
		
		context.cmdList->CopyTextureRegion();

	}

}

void Model::InitTransform(const RenderContext& context)
{
	HRESULT result;
	D3D12_HEAP_PROPERTIES constHeapProp = {};
	constHeapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
	constHeapProp.CreationNodeMask = 1;
	constHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	constHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	constHeapProp.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC constResourceDesc = {};

	constResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	constResourceDesc.Alignment = 0;
	constResourceDesc.Width = (sizeof(worldMatrix) + 255) & ~255;//定数バッファーはメモリサイズが256のサイズでないといけない
	constResourceDesc.Height = 1;
	constResourceDesc.DepthOrArraySize = 1;
	constResourceDesc.MipLevels = 1;
	constResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	constResourceDesc.SampleDesc.Count = 1;
	constResourceDesc.SampleDesc.Quality = 0;
	constResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	constResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	result = context.device->CreateCommittedResource(
		&constHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&constResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(m_constantBuffer.GetAddressOf())
	);
#ifdef _DEBUG
	CheckResult(result, "CommittedConstResourcesV");
#endif
	void* pMapMatrix = nullptr;
	m_constantBuffer->Map(0, nullptr, &pMapMatrix);
	XMMATRIX* pMatrixData = static_cast<XMMATRIX*>(pMapMatrix);
	*pMatrixData = worldMatrix;

	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;

	context.app->AllocateDescriptor(cpuHandle, gpuHandle);//空いてるメモリの先っぽを取得する
	//ディスクリプターつくって更新したハンドルを起点にしてぶち込む
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = m_constantBuffer->GetGPUVirtualAddress();//リソース本体の仮想アドレスが欲しい
	cbvDesc.SizeInBytes = (sizeof(worldMatrix) + 255) & ~255;
	context.device->CreateConstantBufferView(&cbvDesc, cpuHandle);

	this->m_cbvGpuHandle = gpuHandle;
}

void Model::UpdateTransform()
{
	static float angle = 0.0f;
	
	XMMATRIX transformMatrix = XMMatrixRotationX(XM_PIDIV2)* XMMatrixRotationZ(XM_PI);
	transformMatrix *= XMMatrixTranslation(0.0f, -1.0f, 0.0f);
	worldMatrix = transformMatrix;
	angle += 0.01;

	void* pMapMatrix = nullptr;
	m_constantBuffer->Map(0, nullptr, &pMapMatrix);
	XMMATRIX* pMatrixData = static_cast<XMMATRIX*>(pMapMatrix);
	*pMatrixData = worldMatrix;
	m_constantBuffer->Unmap(0, nullptr);
}

bool Model::Draw(const RenderContext& context)
{
	context.cmdList->IASetVertexBuffers(0, 1, &m_vbView);
	context.cmdList->IASetIndexBuffer(&m_ibView);
	context.cmdList->SetGraphicsRootConstantBufferView(1, m_constantBuffer->GetGPUVirtualAddress());//レジスター1に登録
	context.cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	for (const MeshData& mesh : m_meshes)
	{
		context.cmdList->DrawIndexedInstanced(mesh.indexCount, 1, mesh.startIndex, 0, 0);
	}
	
	return true;
}