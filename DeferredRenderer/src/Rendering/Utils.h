#pragma once
#include "Core/Core.h"
#include <source_location>

#define arraysize(a) (sizeof(a)/sizeof(a[0]))

std::wstring string_2_wstring(const std::string& s);
std::string wstring_2_string(const std::wstring& ws);

template<class BlotType>
std::string convertBlobToString(BlotType* pBlob)
{
	std::vector<char> infoLog(pBlob->GetBufferSize() + 1);
	memcpy(infoLog.data(), pBlob->GetBufferPointer(), pBlob->GetBufferSize());
	infoLog[pBlob->GetBufferSize()] = 0;
	return std::string(infoLog.data());
}

namespace D3D
{
	ID3D12DescriptorHeapPtr CreateDescriptorHeap(ID3D12Device5Ptr device,
												 uint32_t count,
												 D3D12_DESCRIPTOR_HEAP_TYPE type,
												 bool shaderVisible);

	ID3D12CommandQueuePtr CreateCommandQueue(ID3D12Device5Ptr device);

	D3D12_CPU_DESCRIPTOR_HANDLE CreateRTV(ID3D12Device5Ptr device,
										  ID3D12ResourcePtr resource,
										  ID3D12DescriptorHeapPtr heap,
										  uint32_t& usedHeapEntries,
										  DXGI_FORMAT format);

	void ResourceBarrier(ID3D12GraphicsCommandList4Ptr cmdList,
						 ID3D12ResourcePtr resource,
						 D3D12_RESOURCE_STATES prevState,
						 D3D12_RESOURCE_STATES nextState);

	uint64_t SubmitCommandList(ID3D12GraphicsCommandList4Ptr cmdList,
							   ID3D12CommandQueuePtr cmdQueue,
							   ID3D12FencePtr fence,
							   uint64_t fenceValue);

	ID3D12RootSignaturePtr CreateRootSignature(ID3D12Device5Ptr pDevice,
											const D3D12_ROOT_SIGNATURE_DESC& desc);

	// Shaders!!!!!!!!!!!
	enum class ShaderType
	{
		Vertex = 0,
		Pixel,
		Size
	};

	struct Vertex
	{
		glm::float3 position;
		glm::float4 color;
	};
}

// For Shaders - To be moved in separate file!!!!
std::wstring GetCurrentPath();
ID3DBlobPtr GetShaderBlob(const std::string& shaderName, D3D::ShaderType type);
// !!!!!!!!!!!!!