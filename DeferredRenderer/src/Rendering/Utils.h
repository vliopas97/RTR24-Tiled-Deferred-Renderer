#pragma once
#include "Core/Core.h"

#include <source_location>
#include <sstream>

struct RootSignature;

#define arraysize(a) (sizeof(a)/sizeof(a[0]))
#define align_to(_alignment, _val) (((_val + _alignment - 1) / _alignment) * _alignment)

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

inline std::vector<std::string> SplitString(const std::string& input, const std::string& delim)
{
	std::vector<std::string> output;
	std::istringstream stream(input);

	std::string part;
	while (std::getline(stream, part, '.'))
		output.push_back(part);

	return output;
}

template<typename T>
concept IsContainer = requires(T container)
{
	{ container.size() } ->std::same_as<std::size_t>;
	container.data();
	typename T::value_type;
};

template <typename Fn, typename... Args>
concept ValidResourceFactory = requires(Fn fn, Args... args)
{
	std::is_invocable_v<Fn, Args...>&& IsContainer<std::invoke_result_t<Fn, Args...>>;
};

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

	D3D12_CPU_DESCRIPTOR_HANDLE CreateDSV(ID3D12Device5Ptr device,
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

	ID3D12ResourcePtr CreateBuffer(ID3D12Device5Ptr device,
								   uint64_t size,
								   D3D12_RESOURCE_FLAGS flags,
								   D3D12_RESOURCE_STATES initState, 
								   const D3D12_HEAP_TYPE& heapType);

	template<typename Container>
	requires IsContainer<Container>
	ID3D12ResourcePtr CreateAndInitializeBuffer(ID3D12Device5Ptr device,
												D3D12_RESOURCE_FLAGS flags,
												D3D12_RESOURCE_STATES initState,
												const D3D12_HEAP_PROPERTIES& heapProperties,
												Container data)
	{
		using Type = decltype(data)::value_type;
		ID3D12ResourcePtr pBuffer = CreateBuffer(device, sizeof(Type) * data.size(), flags, initState, heapProperties);

		uint8_t* pData;
		pBuffer->Map(0, nullptr, (void**)&pData);
		std::memcpy(pData, data.data(), sizeof(Type) * data.size());
		pBuffer->Unmap(0, nullptr);
		return pBuffer;
	}

	template<typename Fn, typename... Args>
	requires ValidResourceFactory<Fn, Args...>
	ID3D12ResourcePtr CreateAndInitializeBuffer(ID3D12Device5Ptr device,
												D3D12_RESOURCE_FLAGS flags,
												D3D12_RESOURCE_STATES initState,
												const D3D12_HEAP_PROPERTIES& heapProperties,
												Fn f, Args... args)
	{
		auto data = f(args...);

		using Type = decltype(data)::value_type;
		ID3D12ResourcePtr pBuffer = CreateBuffer(device, sizeof(Type) * data.size(), flags, initState, heapProperties);

		uint8_t* pData;
		pBuffer->Map(0, nullptr, (void**)&pData);
		std::memcpy(pData, data.data(), sizeof(Type) * data.size());
		pBuffer->Unmap(0, nullptr);
		return pBuffer;
	}
}
