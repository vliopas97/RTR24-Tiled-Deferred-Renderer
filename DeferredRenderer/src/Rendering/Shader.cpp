#include "Shader.h"
#include "Utils.h"
#include "Core/Exception.h"


RootSignature::RootSignature(ID3D12Device5Ptr device)
{
	D3D12_ROOT_SIGNATURE_DESC desc{};
	RootSignaturePtr = D3D::CreateRootSignature(device, desc);
	Interface = RootSignaturePtr.GetInterfacePtr();
	Subobject.pDesc = &Interface;
	Subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
}

RootSignature::RootSignature(ID3D12Device5Ptr device, ID3D12RootSignaturePtr rootSig)
{
	RootSignaturePtr = rootSig;
	Interface = RootSignaturePtr.GetInterfacePtr();
	Subobject.pDesc = &Interface;
	Subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
}

void PipelineStateBindings::Initialize(ID3D12Device5Ptr device, RootSignature rootSignature)
{
	RootSignatureData = MakeUnique<RootSignature>(rootSignature);
	Setup(device);
}

ID3D12RootSignature* PipelineStateBindings::GetRootSignaturePtr() { return RootSignatureData->Interface; }

void PipelineStateBindings::Bind(ID3D12GraphicsCommandList4Ptr cmdList)
{
	cmdList->SetGraphicsRootSignature(RootSignatureData->Interface);

	BindDescriptorHeap(cmdList, SRVHeap);
	cmdList->SetGraphicsRootDescriptorTable(StandardDescriptors, SRVHeap->GetGPUDescriptorHandleForHeapStart());

	BindDescriptorHeap(cmdList, UAVHeap);
	cmdList->SetGraphicsRootDescriptorTable(UAVDescriptor, UAVHeap->GetGPUDescriptorHandleForHeapStart());

	BindDescriptorHeap(cmdList, CBVHeap);
	cmdList->SetGraphicsRootDescriptorTable(CBuffer, CBVHeap->GetGPUDescriptorHandleForHeapStart());
}

void PipelineStateBindings::Tick()
{
	std::memcpy(GlobalConstantsDataBridgePtr, &GlobalConstantsData, sizeof(GlobalConstantsData));
}

void PipelineStateBindings::Setup(ID3D12Device5Ptr device)
{
	SRVHeap = D3D::CreateDescriptorHeap(device, NumGlobalSRVDescriptorRanges + 1,
										D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);
	UAVHeap = D3D::CreateDescriptorHeap(device, 1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);
	CBVHeap = D3D::CreateDescriptorHeap(device, 1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);

	//Create constant buffer for temp data
	const UINT constantBufferSize = align_to(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(GlobalConstantsData));    // CB size is required to be 256-byte aligned.

	GRAPHICS_ASSERT(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&GlobalConstants)));

	// Describe and create a constant buffer view.
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = GlobalConstants->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = constantBufferSize;
	device->CreateConstantBufferView(&cbvDesc, CBVHeap->GetCPUDescriptorHandleForHeapStart());

	GRAPHICS_ASSERT(GlobalConstants->Map(0, nullptr, reinterpret_cast<void**>(&GlobalConstantsDataBridgePtr)));
	std::memcpy(GlobalConstantsDataBridgePtr, &GlobalConstantsData, sizeof(GlobalConstantsData));
}

void PipelineStateBindings::BindDescriptorHeap(ID3D12GraphicsCommandList4Ptr cmdList, ID3D12DescriptorHeapPtr heap)
{
	std::array<ID3D12DescriptorHeap*, 1> heaps = { heap };
	cmdList->SetDescriptorHeaps(heaps.size(), heaps.data());
}