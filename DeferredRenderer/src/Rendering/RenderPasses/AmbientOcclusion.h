#pragma once
#include "RenderPass.h"

class AmbientOcclusionPass final : public RenderPass
{
public:
	AmbientOcclusionPass(std::string&& name);
	void Submit(ID3D12GraphicsCommandList4Ptr cmdList, const Scene& scene) override;
protected:
	void InitResources(ID3D12Device5Ptr device) override;
	void InitRootSignature() override;
	void InitPipelineState() override;
private:
	SharedPtr<ID3D12ResourcePtr> RandomTexture;
	SharedPtr<ID3D12ResourcePtr> SSAOKernel;
	SharedPtr<ID3D12ResourcePtr> Normals;
	SharedPtr<ID3D12ResourcePtr> Positions;

	SharedPtr<ID3D12DescriptorHeapPtr> SRVHeap{};
	SharedPtr<ID3D12DescriptorHeapPtr> RTVHeap{};
	D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle{};
};

