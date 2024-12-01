#include "PipelineResources.h"
#include "Utils.h"
#include "Core/Exception.h"

ID3D12DescriptorHeapPtr SRVHeap{};
ID3D12DescriptorHeapPtr UAVHeap{};
ID3D12DescriptorHeapPtr CBVHeap{};
ID3D12DescriptorHeapPtr SamplerHeap{};
ID3D12DescriptorHeapPtr LightsHeap{};
ConstantBuffer<PipelineConstants> CBGlobalConstants{};

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

void RootSignature::AddDescriptorTable(const std::vector<D3D12_DESCRIPTOR_RANGE>& ranges, D3D12_SHADER_VISIBILITY visibility)
{
	DescriptorRangeStorage.push_back(ranges);
	RootParameters.push_back(RootParameterBuilder::CreateDescriptorTable(DescriptorRangeStorage.back(), visibility));
}

void RootSignature::AddDescriptor(D3D12_ROOT_PARAMETER_TYPE rootParameterType, D3D12_SHADER_VISIBILITY visibility, UINT shaderRegister, UINT registerSpace)
{
	RootParameters.push_back(RootParameterBuilder::CreateDescriptor(rootParameterType, visibility, shaderRegister, registerSpace));
}

void RootSignature::Build(ID3D12Device5Ptr device, D3D12_ROOT_SIGNATURE_FLAGS flags)
{
	D3D12_ROOT_SIGNATURE_DESC desc{};
	desc.NumParameters = static_cast<UINT>(RootParameters.size());
	desc.pParameters = RootParameters.data();
	desc.Flags = flags;

	RootSignaturePtr = D3D::CreateRootSignature(device, desc);
	Interface = RootSignaturePtr.GetInterfacePtr();
}

D3D12_DESCRIPTOR_RANGE DescriptorRangeBuilder::CreateRange(D3D12_DESCRIPTOR_RANGE_TYPE type, UINT numDescriptors, UINT baseShaderRegister, UINT registerSpace, UINT offsetInDescriptorsFromTableStart)
{
	D3D12_DESCRIPTOR_RANGE range{};
	range.RangeType = type;
	range.BaseShaderRegister = baseShaderRegister;
	range.NumDescriptors = numDescriptors;
	range.RegisterSpace = registerSpace;
	range.OffsetInDescriptorsFromTableStart = offsetInDescriptorsFromTableStart;
	return range;
}

D3D12_ROOT_PARAMETER RootParameterBuilder::CreateDescriptorTable(const std::vector<D3D12_DESCRIPTOR_RANGE>& ranges, D3D12_SHADER_VISIBILITY visibility)
{
	D3D12_ROOT_PARAMETER param{};
	param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	param.ShaderVisibility = visibility;

	D3D12_DESCRIPTOR_RANGE* rangesData = new D3D12_DESCRIPTOR_RANGE[ranges.size()];
	std::copy(ranges.begin(), ranges.end(), rangesData);

	param.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(ranges.size());
	param.DescriptorTable.pDescriptorRanges = rangesData;

	return param;
}

D3D12_ROOT_PARAMETER RootParameterBuilder::CreateDescriptor(D3D12_ROOT_PARAMETER_TYPE rootParameterType, D3D12_SHADER_VISIBILITY visibility, UINT shaderRegister, UINT registerSpace)
{
	D3D12_ROOT_PARAMETER param{};
	param.ParameterType = rootParameterType;
	param.ShaderVisibility = visibility;
	param.Descriptor.ShaderRegister = shaderRegister;
	param.Descriptor.RegisterSpace = registerSpace;
	return param;
}

void RenderPassResources::Bind(ID3D12GraphicsCommandList4Ptr cmdList)
{
	UINT rootParameterIndex = 0;
	for (const auto& heap : Heaps)
	{
		BindDescriptorHeap(cmdList, heap);
		cmdList->SetGraphicsRootDescriptorTable(rootParameterIndex++, heap->GetGPUDescriptorHandleForHeapStart());
	}

}

void RenderPassResources::Setup(ID3D12Device5Ptr device)
{
	Heaps.push_back(SRVHeap);
	Heaps.push_back(UAVHeap);
	Heaps.push_back(CBVHeap);
	Heaps.push_back(SamplerHeap);
	Heaps.push_back(LightsHeap);
}

void RenderPassResources::BindDescriptorHeap(ID3D12GraphicsCommandList4Ptr cmdList, ID3D12DescriptorHeapPtr heap)
{
	std::array<ID3D12DescriptorHeap*, 1> heaps = { heap };
	cmdList->SetDescriptorHeaps(heaps.size(), heaps.data());
}
