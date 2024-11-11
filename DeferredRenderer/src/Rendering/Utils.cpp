#include "Utils.h"
#include "Core/Exception.h"
#include "Shader.h"

#include <codecvt>

std::wstring string_2_wstring(const std::string& s)
{
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0);
	std::wstring ws(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &ws[0], size_needed);
	return ws;
}

std::string wstring_2_string(const std::wstring& ws)
{
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), NULL, 0, NULL, NULL);
	std::string s(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), &s[0], size_needed, NULL, NULL);
	return s;
}

ID3D12DescriptorHeapPtr D3D::CreateDescriptorHeap(ID3D12Device5Ptr device, uint32_t count, D3D12_DESCRIPTOR_HEAP_TYPE type, bool shaderVisible)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc{};
	desc.NumDescriptors = count;
	desc.Type = type;
	desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE :
		D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	ID3D12DescriptorHeapPtr heap;
	GRAPHICS_ASSERT(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap)));
	return heap;
}

ID3D12CommandQueuePtr D3D::CreateCommandQueue(ID3D12Device5Ptr device)
{
	ID3D12CommandQueuePtr queue;
	D3D12_COMMAND_QUEUE_DESC desc{};
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	GRAPHICS_ASSERT(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&queue)));
	return queue;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D::CreateRTV(ID3D12Device5Ptr device, ID3D12ResourcePtr resource, ID3D12DescriptorHeapPtr heap, uint32_t& usedHeapEntries, DXGI_FORMAT format)
{
	D3D12_RENDER_TARGET_VIEW_DESC desc{};
	desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	desc.Format = format;
	desc.Texture2D.MipSlice;
	auto rtvHandle = heap->GetCPUDescriptorHandleForHeapStart();
	rtvHandle.ptr += (usedHeapEntries++) * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	device->CreateRenderTargetView(resource, &desc, rtvHandle);
	return rtvHandle;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D::CreateDSV(ID3D12Device5Ptr device, ID3D12ResourcePtr resource, ID3D12DescriptorHeapPtr heap, uint32_t& usedHeapEntries, DXGI_FORMAT format)
{
	D3D12_DEPTH_STENCIL_VIEW_DESC desc{};
	desc.Format = format;
	desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	desc.Flags = D3D12_DSV_FLAG_NONE;

	auto dsvHandle = heap->GetCPUDescriptorHandleForHeapStart();
	dsvHandle.ptr += (usedHeapEntries++) * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	device->CreateDepthStencilView(resource, &desc, dsvHandle);
	return dsvHandle;
}

void D3D::ResourceBarrier(ID3D12GraphicsCommandList4Ptr cmdList, ID3D12ResourcePtr resource, D3D12_RESOURCE_STATES prevState, D3D12_RESOURCE_STATES nextState)
{
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = resource;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = prevState;
	barrier.Transition.StateAfter = nextState;
	cmdList->ResourceBarrier(1, &barrier);
}

uint64_t D3D::SubmitCommandList(ID3D12GraphicsCommandList4Ptr cmdList, ID3D12CommandQueuePtr cmdQueue, ID3D12FencePtr fence, uint64_t fenceValue)
{
	cmdList->Close();
	ID3D12CommandList* list = cmdList.GetInterfacePtr();
	cmdQueue->ExecuteCommandLists(1, &list);
	cmdQueue->Signal(fence, ++fenceValue);
	return fenceValue;
}

ID3D12RootSignaturePtr D3D::CreateRootSignature(ID3D12Device5Ptr pDevice,
										   const D3D12_ROOT_SIGNATURE_DESC& desc)
{
	ID3DBlobPtr sigBlob;
	ID3DBlobPtr errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errorBlob);
	if (FAILED(hr))
	{
		std::string msg = convertBlobToString(errorBlob.GetInterfacePtr());
		MessageBoxA(NULL, msg.c_str(), "Error", MB_OK);
		return nullptr;
	}
	ID3D12RootSignaturePtr rootSig;
	GRAPHICS_ASSERT(pDevice->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&rootSig)));
	return rootSig;
}

ID3D12RootSignaturePtr D3D::InitializeGlobalRootSignature(ID3D12Device5Ptr device)
{
	std::array<D3D12_DESCRIPTOR_RANGE, 1> uavRanges = {};
	std::array<D3D12_DESCRIPTOR_RANGE, NumGlobalSRVDescriptorRanges> srvRanges = {};
	std::array<D3D12_DESCRIPTOR_RANGE, NumGlobalCBVDescriptorRanges> cbvRanges = {};

	uavRanges[0].BaseShaderRegister = 0;
	uavRanges[0].NumDescriptors = 2;
	uavRanges[0].RegisterSpace = 0;
	uavRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;

	uint32_t userStart = NumGlobalSRVDescriptorRanges - NumUserDescriptorRanges;
	for (uint32_t i = 0; i < NumGlobalSRVDescriptorRanges; ++i)
	{
		auto& descElement = srvRanges[i];
		descElement.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		descElement.NumDescriptors = UINT_MAX;
		descElement.BaseShaderRegister = 0;
		descElement.RegisterSpace = i;
		descElement.OffsetInDescriptorsFromTableStart = 0;
		if (i >= userStart)
			descElement.RegisterSpace = (i - userStart) + 100;
	}

	for (uint32_t i = 0; i < NumGlobalCBVDescriptorRanges; ++i)
	{
		auto& descElement = cbvRanges[i];
		descElement.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		descElement.NumDescriptors = UINT_MAX;
		descElement.BaseShaderRegister = 0;
		descElement.RegisterSpace = i;
		descElement.OffsetInDescriptorsFromTableStart = 0;
	}

	std::array<D3D12_ROOT_PARAMETER, 4> rootParams;

	rootParams[StandardDescriptors].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[StandardDescriptors].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParams[StandardDescriptors].DescriptorTable.NumDescriptorRanges = static_cast<UINT>(srvRanges.size());
	rootParams[StandardDescriptors].DescriptorTable.pDescriptorRanges = srvRanges.data();

	rootParams[UAVDescriptor].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[UAVDescriptor].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParams[UAVDescriptor].DescriptorTable.NumDescriptorRanges = static_cast<UINT>(uavRanges.size());
	rootParams[UAVDescriptor].DescriptorTable.pDescriptorRanges = uavRanges.data();

	rootParams[CBuffer].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[CBuffer].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParams[CBuffer].DescriptorTable.NumDescriptorRanges = static_cast<UINT>(cbvRanges.size());
	rootParams[CBuffer].DescriptorTable.pDescriptorRanges = cbvRanges.data();

	rootParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParams[3].Descriptor.ShaderRegister = 0;
	rootParams[3].Descriptor.RegisterSpace = 200;


	D3D12_ROOT_SIGNATURE_DESC desc{};
	desc.NumParameters = static_cast<UINT>(rootParams.size());
	desc.pParameters = rootParams.data();
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	return D3D::CreateRootSignature(device, desc);
}

ID3D12ResourcePtr D3D::CreateBuffer(ID3D12Device5Ptr device,
							   uint64_t size,
							   D3D12_RESOURCE_FLAGS flags,
							   D3D12_RESOURCE_STATES initState, 
							   const D3D12_HEAP_TYPE& heapType)
{
	ID3D12ResourcePtr buffer;
	GRAPHICS_ASSERT(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(heapType),
													D3D12_HEAP_FLAG_NONE, 
													&CD3DX12_RESOURCE_DESC::Buffer(size, flags),
													initState, 
													nullptr, 
													IID_PPV_ARGS(&buffer)));
	return buffer;
}
