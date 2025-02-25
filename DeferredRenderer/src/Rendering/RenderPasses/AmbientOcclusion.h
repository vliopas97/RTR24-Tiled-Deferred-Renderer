#pragma once
#include "RenderPass.h"

class AmbientOcclusionPass final : public RenderPass
{
public:
	AmbientOcclusionPass(std::string&& name);
	void Submit(ID3D12GraphicsCommandList4Ptr cmdList, const Scene& scene) override;
	void PreExecTransitions(ID3D12GraphicsCommandList4Ptr cmdList) override;
	void PostExecTransitions(ID3D12GraphicsCommandList4Ptr cmdList) override;

protected:
	void InitResources(ID3D12Device5Ptr device) override;
	void InitRootSignature() override;
	void InitPipelineState() override;
private:
	SharedPtr<DX::Texture> RandomTexture;
	SharedPtr<ID3D12ResourcePtr> SSAOKernel;
	SharedPtr<DX::Texture> Normals;
	SharedPtr<DX::Texture> Positions;

	DX::DescriptorHeap SRVHeap{};
	DX::DescriptorHeap RTVHeap{};
};

