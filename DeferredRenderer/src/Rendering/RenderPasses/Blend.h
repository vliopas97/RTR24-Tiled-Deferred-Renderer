#pragma once
#include "RenderPass.h"

class BlendPass : public RenderPass
{
public:
	BlendPass(std::string&& name);
	void Submit(ID3D12GraphicsCommandList4Ptr cmdList, const Scene& scene) override;
protected:
	void InitResources(ID3D12Device5Ptr device) override;
	void InitRootSignature() override;
	void InitPipelineState() override;
private:
	SharedPtr<ID3D12ResourcePtr> OriginalColor;
	SharedPtr<ID3D12ResourcePtr> ReflectionColor;

	ID3D12DescriptorHeapPtr SRVHeap;
};
