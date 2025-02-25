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
	DX::DescriptorHeap SRVHeap;
	DX::DescriptorHeap CBVHeap;

	DX::DescriptorHeap RTVHeap;
	D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle{};

	SharedPtr<DX::Texture> SRVToBlur;
	ResourceGPU_CBV<BlurPassControls> Controls;
};

class HorizontalBlurPass final : public BlurPass
{
public:
	HorizontalBlurPass(std::string&& name);
	void PreExecTransitions(ID3D12GraphicsCommandList4Ptr cmdList) override;
	void PostExecTransitions(ID3D12GraphicsCommandList4Ptr cmdList) override;
protected:
	void InitResources(ID3D12Device5Ptr device) override;
};

class VerticalBlurPass final : public BlurPass
{
public:
	VerticalBlurPass(std::string&& name);
	void PreExecTransitions(ID3D12GraphicsCommandList4Ptr cmdList) override;
	void PostExecTransitions(ID3D12GraphicsCommandList4Ptr cmdList) override;
protected:
	void InitResources(ID3D12Device5Ptr device) override;
};

class CombinedBlurPass : public RenderPass
{
public:
	CombinedBlurPass(std::string&& name, bool flag = true);
	virtual void Submit(ID3D12GraphicsCommandList4Ptr cmdList, const Scene& scene) override;
	void PreExecTransitions(ID3D12GraphicsCommandList4Ptr cmdList) override;
	void PostExecTransitions(ID3D12GraphicsCommandList4Ptr cmdList) override;
protected:
	virtual void InitResources(ID3D12Device5Ptr device) override;
	virtual void InitRootSignature() override;
	virtual void InitPipelineState() override;
private:
	void InitStaticResources(ID3D12Device5Ptr device);
protected:
	DX::DescriptorHeap SRVHeap;
	DX::DescriptorHeap UAVHeap;

	SharedPtr<DX::Texture> SRVToBlur;
	SharedPtr<DX::Texture> BlurOutput;

	static DX::DescriptorHeap FiltersHeap;
	static ID3D12ResourcePtr Filters;

public:
	static const uint GroupSize = 12;
};

class CombinedBlurPassGlobal : public CombinedBlurPass
{
public:
	CombinedBlurPassGlobal(std::string&& name, bool flag = true, uint radius = 5);
	void Submit(ID3D12GraphicsCommandList4Ptr cmdList, const Scene& scene) override;
	inline void SetRadius(uint radius);
protected:
	void InitResources(ID3D12Device5Ptr device) override;
private:
	ResourceGPU_CBV<uint> FilterRadius;
	DX::DescriptorHeap	CBVHeap;
};
