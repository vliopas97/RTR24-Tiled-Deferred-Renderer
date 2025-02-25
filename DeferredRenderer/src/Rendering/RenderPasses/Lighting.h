#pragma once
#include "RenderPass.h"

class LightingPass final : public RenderPass
{
public:
	LightingPass(std::string&& name);
	void Submit(ID3D12GraphicsCommandList4Ptr cmdList, const Scene& scene) override;
	void PreExecTransitions(ID3D12GraphicsCommandList4Ptr cmdList) override;
	void PostExecTransitions(ID3D12GraphicsCommandList4Ptr cmdList) override;

protected:
	void Bind(ID3D12GraphicsCommandList4Ptr cmdList) const override;
	void InitResources(ID3D12Device5Ptr device) override;
	void InitRootSignature() override;
	void InitPipelineState() override;
private:
	SharedPtr<DX::Texture> Positions;
	SharedPtr<DX::Texture> Normals;
	SharedPtr<DX::Texture> Diffuse;
	SharedPtr<DX::Texture> Specular;
	SharedPtr<DX::Texture> AmbientOcclusion;

	SharedPtr<DX::DescriptorHeap> GBufferHeap{};
	DX::DescriptorHeap AOHeap;

	DX::DescriptorHeap RTVHeap{};
	D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle{};
};

