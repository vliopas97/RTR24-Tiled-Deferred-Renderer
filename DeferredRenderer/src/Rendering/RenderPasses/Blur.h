#pragma once
#include "RenderPass.h"


class BlurPass : public RenderPass
{
public:
	virtual void Submit(ID3D12GraphicsCommandList4Ptr cmdList, const Scene& scene) override;
	virtual ~BlurPass() = default;
protected:
	BlurPass(std::string&& name);
	virtual void InitResources(ID3D12Device5Ptr device) override;
	virtual void InitRootSignature() override;
	virtual void InitPipelineState() override;
protected:
	ID3D12DescriptorHeapPtr SRVHeap;
	ID3D12DescriptorHeapPtr CBVHeap;

	SharedPtr<ID3D12DescriptorHeapPtr> RTVHeap{};
	D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle{};

	SharedPtr<ID3D12ResourcePtr> SRVToBlur;
	ResourceGPU_CBV<BlurPassControls> Controls;

};

class HorizontalBlurPass final : public BlurPass
{
public:
	HorizontalBlurPass(std::string&& name);
protected:
	void InitResources(ID3D12Device5Ptr device) override;
};

class VerticalBlurPass final : public BlurPass
{
public:
	VerticalBlurPass(std::string&& name);
protected:
	void InitResources(ID3D12Device5Ptr device) override;
};

