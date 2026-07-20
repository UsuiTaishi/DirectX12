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

void Camera::Init(const RenderContext& context)
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
		IID_PPV_ARGS(c_constantBuffer.GetAddressOf())
	);
#ifdef _DEBUG
	CheckResult(result, "CommittedConstResources");
#endif
	void* pMapMatrix = nullptr;
	c_constantBuffer->Map(0, nullptr, &pMapMatrix);
	XMMATRIX* pMatrixData = static_cast<XMMATRIX*>(pMapMatrix);
	*pMatrixData = matrix;

	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;

	context.app->AllocateDescriptor(cpuHandle, gpuHandle);

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = c_constantBuffer->GetGPUVirtualAddress();//リソース本体の仮想アドレスが欲しい
	cbvDesc.SizeInBytes = (sizeof(matrix) + 255) & ~255;
	context.device->CreateConstantBufferView(&cbvDesc, cpuHandle);

	this->c_cbvGpuHandle = gpuHandle;
}

