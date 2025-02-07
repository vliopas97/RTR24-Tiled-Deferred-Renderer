#pragma once
#include "RenderPass.h"

class ReflectionPass : public RenderPass
{
public:
	ReflectionPass(std::string&& name);
	void Submit(ID3D12GraphicsCommandList4Ptr cmdList, const Scene& scene) override;
protected:
	void Bind(ID3D12GraphicsCommandList4Ptr cmdList) const override;
	void InitResources(ID3D12Device5Ptr device) override;
	void InitRootSignature() override;
	void InitPipelineState() override;
private:
	SharedPtr<ID3D12ResourcePtr> Positions;
	SharedPtr<ID3D12ResourcePtr> Normals;
	SharedPtr<ID3D12ResourcePtr> PixelsColor;

	ID3D12DescriptorHeapPtr RTVHeap{};
	ID3D12DescriptorHeapPtr SRVHeap{};

	D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle;
};

