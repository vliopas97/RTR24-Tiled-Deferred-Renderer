#pragma once

#include "Core/Core.h"
#include "Buffer.h"
#include "Shaders/HLSLCompat.h"

enum RootParamTypes : uint32_t
{
	StandardDescriptors = 0,
	UAVDescriptor,
	CBuffer,
	Sampler,
	LightCBuffer,
	GlobalDescTablesNo
};

struct RootSignatureDesc
{
	D3D12_ROOT_SIGNATURE_DESC Desc = {};
	std::vector<D3D12_DESCRIPTOR_RANGE> Range;
	std::vector<D3D12_ROOT_PARAMETER> RootParams;
};

template <typename Fn, typename... Args>
concept ValidRootSignatureFactory = requires(Fn fn, Args... args)
{
	std::is_invocable_v<Fn, Args...>&&
		std::is_same_v<std::invoke_result_t<Fn, Args...>, ID3D12RootSignaturePtr>;
};

class DescriptorRangeBuilder
{
public:
	static D3D12_DESCRIPTOR_RANGE CreateRange(D3D12_DESCRIPTOR_RANGE_TYPE type, 
											  UINT numDescriptors, 
											  UINT baseShaderRegister, 
											  UINT registerSpace = 0,
											  UINT offsetInDescriptorsFromTableStart = 0)
	{
		D3D12_DESCRIPTOR_RANGE range{};
		range.RangeType = type;
		range.BaseShaderRegister = baseShaderRegister;
		range.NumDescriptors = numDescriptors;
		range.RegisterSpace = registerSpace;
		range.OffsetInDescriptorsFromTableStart = offsetInDescriptorsFromTableStart;
		return range;
	}
};

class RootParameterBuilder
{
public:
	static D3D12_ROOT_PARAMETER CreateDescriptorTable(
		const std::vector<D3D12_DESCRIPTOR_RANGE>& ranges,
		D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL)
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

	static D3D12_ROOT_PARAMETER CreateConstantBufferView(
		D3D12_ROOT_PARAMETER_TYPE rootParameterType,
		D3D12_SHADER_VISIBILITY visibility,
		UINT shaderRegister,
		UINT registerSpace = 0)
	{
		D3D12_ROOT_PARAMETER param{};
		param.ParameterType = rootParameterType;
		param.ShaderVisibility = visibility;
		param.Descriptor.ShaderRegister = shaderRegister;
		param.Descriptor.RegisterSpace = registerSpace;
		return param;
	}

};

struct RootSignature
{
	RootSignature(ID3D12Device5Ptr device);
	RootSignature(ID3D12Device5Ptr device, ID3D12RootSignaturePtr rootSig);
	template <typename Fn, typename... Args>
		requires ValidRootSignatureFactory<Fn, Args...>
	RootSignature(ID3D12Device5Ptr device, Fn f, Args... args)
	{
		RootSignaturePtr = f(args...);
		Interface = RootSignaturePtr.GetInterfacePtr();
		Subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
		Subobject.pDesc = &Interface;
	}

	ID3D12RootSignaturePtr RootSignaturePtr;
	ID3D12RootSignature* Interface = nullptr;
	D3D12_STATE_SUBOBJECT Subobject = {};
};

struct PipelineStateBindings
{
	using RootSignaturePtr = UniquePtr<RootSignature>;
	PipelineStateBindings() = default;
	~PipelineStateBindings() = default;

	void Initialize(ID3D12Device5Ptr device, RootSignature rootSignature);
	ID3D12RootSignature* GetRootSignaturePtr();

	void Bind(ID3D12GraphicsCommandList4Ptr cmdList);
	void Tick();

private:
	void Setup(ID3D12Device5Ptr device);
	void BindDescriptorHeap(ID3D12GraphicsCommandList4Ptr cmdList, ID3D12DescriptorHeapPtr heap);

public:
	ID3D12DescriptorHeapPtr SRVHeap;
	ID3D12DescriptorHeapPtr UAVHeap;
	ID3D12DescriptorHeapPtr CBVHeap;
	ID3D12DescriptorHeapPtr SamplerHeap;
	ID3D12DescriptorHeapPtr LightsHeap;

	ConstantBuffer<PipelineConstants> CBGlobalConstants;

private:
	RootSignaturePtr RootSignatureData;
};