#include<Windows.h>
#include <tchar.h>
#include<string>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <vector>
#include <d3dcompiler.h>
#include"game.h"
#include<cassert>

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

using namespace std;
using namespace DirectX;

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

	/*マテリアル読み込み
	int numMaterials = fbxScene->GetMaterialCount();//このFBXシーン全体に登録されているマテリアルの総数を取得する
	for (int i = 0; i < numMaterials; i++)
	{
		//マテリアル名のみ抽出する。
		LoadMateial(fbxScene->GetMaterial(i));
	}*/

	int numMesh = fbxScene->GetSrcObjectCount<FbxMesh>();
	for (int i = 0; i < numMesh; i++)
	{
		FbxMesh* mesh = fbxScene->GetSrcObject<FbxMesh>(i);
		LoadMesh(mesh);
		UINT meshVertexCount = mesh->GetPolygonVertexCount();
		allVertexCount += meshVertexCount;
	}
	std::vector<Vertex> allVertices;
	for (const auto& mesh : m_meshes)
	{
		allVertices.insert(allVertices.end(), mesh.m_mVertexData.begin(), mesh.m_mVertexData.end());
	}

	// 頂点バッファを作成
	CreateVertexBuffer(context, allVertices);
}

void Model::LoadMesh(FbxMesh* mesh)
{
	MeshData data = {};
	UINT vertexCount = mesh->GetPolygonVertexCount();//全頂点数を調べる(blenderなどで表示される頂点数ではなく。頂点インデックスで並べたときの要素数)
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

	//なんのマテリアルが割り当てられてるか調べるこれも例や構造から直接取得します。
	if (mesh->GetElementMaterialCount() == 0)
	{
		data.materialName = "";
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
			data.materialName = surface_material->GetName();
		}
		else {
			data.materialName = "";
		}
	}

	data.m_mVertexData = {};
	data.m_mVertexData.resize(vertexCount);//メモリ確保

	for (int i = 0; i < vertexCount; i++)
	{
		int controlPointIndex = vertexIndex[i];//インデックスバッファ用にインデックスに従って通りに座標を配置していきたいので


		//MESH構造体に格納（float型へ変換）まずは頂点座標から
		data.m_mVertexData[i].Position[0] = static_cast<float>(vertexPos[controlPointIndex][0]);//0つまりx座標
		data.m_mVertexData[i].Position[1] = static_cast<float>(vertexPos[controlPointIndex][1]);//0つまりy座標
		data.m_mVertexData[i].Position[2] = static_cast<float>(vertexPos[controlPointIndex][2]);//0つまりz座標
		//これでv0(座標),v1(座標),v2(座標),v0(座標),v1(座標),v3(座標)みたいな感じでインデックス順に座標データの配列ができる重複してるとこもあるのでDirectXに渡してインデックスを利用する際は重複を消してインデックスを付与しないといけない

		//ノーマル
		data.m_mVertexData[i].Normal[0] = static_cast<float>(normals[i][0]);
		data.m_mVertexData[i].Normal[1] = static_cast<float>(normals[i][1]);
		data.m_mVertexData[i].Normal[2] = static_cast<float>(normals[i][2]);

		//UV
		data.m_mVertexData[i].UV[0] = static_cast<float>(texcoord[i][0]);
		data.m_mVertexData[i].UV[1] = static_cast<float>(texcoord[i][1]);

		//接空間
		data.m_mVertexData[i].Tangent[0] = static_cast<float>(tangent[i][0]);
		data.m_mVertexData[i].Tangent[1] = static_cast<float>(tangent[i][1]);
		data.m_mVertexData[i].Tangent[2] = static_cast<float>(tangent[i][2]);
	}
	m_meshes.push_back(data);
}

void Model::CreateVertexBuffer(const RenderContext& context, std::vector<Vertex>& vertices)
{
	HRESULT result;
	D3D12_RESOURCE_DESC vertexResouceDesc = {};
	vertexResouceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	vertexResouceDesc.Width = sizeof(Vertex) * vertices.size();
	vertexResouceDesc.Height = 1;
	vertexResouceDesc.SampleDesc.Count = 1;
	vertexResouceDesc.SampleDesc.Quality = 0;
	vertexResouceDesc.DepthOrArraySize = 1;
	vertexResouceDesc.MipLevels = 1;
	vertexResouceDesc.Format = DXGI_FORMAT_UNKNOWN;
	vertexResouceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	vertexResouceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	D3D12_HEAP_PROPERTIES vertexheap = {};
	vertexheap.Type = D3D12_HEAP_TYPE_UPLOAD;//CPUからアクセス可能
	vertexheap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;//カスタムのとき使うやつ
	vertexheap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;//カスタムのとき使うやつ

	result = context.device->CreateCommittedResource
	(
		&vertexheap,
		D3D12_HEAP_FLAG_NONE,
		&vertexResouceDesc,
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
	for (size_t i = 0; i < m_meshes.size(); i++)
	{
		MeshData& mesh = m_meshes[i];
		copy(mesh.m_mVertexData.begin(), mesh.m_mVertexData.end(), pVertexData + offset);//第３引数は配置するアドレス
		offset += mesh.m_mVertexData.size();//MeshDataの頂点デーや分だけオフセットをずらす。
	}
	m_vertexBuffer->Unmap(0, nullptr);

	m_vbView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
	m_vbView.SizeInBytes = sizeof(Vertex) * vertices.size();
	m_vbView.StrideInBytes = sizeof(Vertex);
}

bool Model::Draw(const RenderContext& context)
{
	context.cmdList->IASetVertexBuffers(0, 1, &m_vbView);
	context.cmdList->SetGraphicsRootDescriptorTable(0, m_cbvGpuHandle);
	context.cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context.cmdList->DrawInstanced(allVertexCount, 1, 0, 0);

	return true;
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
	constResourceDesc.Width = (sizeof(matrix) + 255) & ~255;//定数バッファーはメモリサイズが256のサイズでないといけない
	constResourceDesc.Height = 1;
	constResourceDesc.DepthOrArraySize = 1;
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
	CheckResult(result, "CommittedConstResources");
#endif
	void* pMapMatrix = nullptr;
	m_constantBuffer->Map(0, nullptr, &pMapMatrix);
	XMMATRIX* pMatrixData = static_cast<XMMATRIX*>(pMapMatrix);
	*pMatrixData = matrix;

	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;

	context.app->AllocateDescriptor(cpuHandle, gpuHandle);

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = m_constantBuffer->GetGPUVirtualAddress();//リソース本体の仮想アドレスが欲しい
	cbvDesc.SizeInBytes = (sizeof(matrix) + 255) & ~255;
	context.device->CreateConstantBufferView(&cbvDesc, cpuHandle);

	this->m_cbvGpuHandle = gpuHandle;
}