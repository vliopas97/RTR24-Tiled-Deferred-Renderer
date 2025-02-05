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

class CombinedBlurPass : public RenderPass
{
public:
	CombinedBlurPass(std::string&& name);
	virtual void Submit(ID3D12GraphicsCommandList4Ptr cmdList, const Scene& scene) override;
protected:
	virtual void InitResources(ID3D12Device5Ptr device) override;
	virtual void InitRootSignature() override;
	virtual void InitPipelineState() override;
private:
	void InitStaticResources(ID3D12Device5Ptr device);
protected:
	ID3D12DescriptorHeapPtr SRVHeap;
	ID3D12DescriptorHeapPtr UAVHeap;

	SharedPtr<ID3D12ResourcePtr> SRVToBlur;
	SharedPtr<ID3D12ResourcePtr> BlurOutput;

	static ID3D12DescriptorHeapPtr FiltersHeap;
	static ID3D12ResourcePtr Filters;

public:
	static const uint GroupSize = 16;
};

class CombinedBlurPassGlobal : public CombinedBlurPass
{
public:
	CombinedBlurPassGlobal(std::string&& name, uint radius = 5);
	void Submit(ID3D12GraphicsCommandList4Ptr cmdList, const Scene& scene) override;
	inline void SetRadius(uint radius);
protected:
	void InitResources(ID3D12Device5Ptr device) override;
private:
	ResourceGPU_CBV<uint> FilterRadius;
	ID3D12DescriptorHeapPtr	CBVHeap;
};
