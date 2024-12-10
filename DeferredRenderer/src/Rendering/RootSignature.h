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

namespace DescriptorRangeBuilder
{

	D3D12_DESCRIPTOR_RANGE CreateRange(D3D12_DESCRIPTOR_RANGE_TYPE type,
									   UINT numDescriptors, 
									   UINT baseShaderRegister, 
									   UINT registerSpace = 0,
									   UINT offsetInDescriptorsFromTableStart = 0);
};

namespace RootParameterBuilder
{

	D3D12_ROOT_PARAMETER CreateDescriptorTable(const std::vector<D3D12_DESCRIPTOR_RANGE>& ranges,
											   D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);

	D3D12_ROOT_PARAMETER CreateDescriptor(D3D12_ROOT_PARAMETER_TYPE rootParameterType, 
										  D3D12_SHADER_VISIBILITY visibility, 
										  UINT shaderRegister, 
										  UINT registerSpace = 0);

};

struct RootSignature
{
	RootSignature() = default;
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

	void AddDescriptorTable(
		const std::vector<D3D12_DESCRIPTOR_RANGE>& ranges,
		D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);

	void AddDescriptor(
		D3D12_ROOT_PARAMETER_TYPE rootParameterType,
		D3D12_SHADER_VISIBILITY visibility,
		UINT shaderRegister, 
		UINT registerSpace = 0);

	void Build(ID3D12Device5Ptr device, D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ID3D12RootSignaturePtr RootSignaturePtr;
	ID3D12RootSignature* Interface = nullptr;
	D3D12_STATE_SUBOBJECT Subobject = {};

private:
	std::vector<D3D12_ROOT_PARAMETER> RootParameters;
	std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> DescriptorRangeStorage; // To manage descriptor range memory
};