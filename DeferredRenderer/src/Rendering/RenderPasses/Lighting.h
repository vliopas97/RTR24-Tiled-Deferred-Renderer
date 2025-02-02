#pragma once
#include "RenderPass.h"

class LightingPass final : public RenderPass
{
public:
	LightingPass(std::string&& name);
	void Submit(ID3D12GraphicsCommandList4Ptr cmdList, const Scene& scene) override;
protected:
	void Bind(ID3D12GraphicsCommandList4Ptr cmdList) const override;
	void InitResources(ID3D12Device5Ptr device) override;
	void InitRootSignature() override;
	void InitPipelineState() override;
private:
	SharedPtr<ID3D12ResourcePtr> Positions;
	SharedPtr<ID3D12ResourcePtr> Normals;
	SharedPtr<ID3D12ResourcePtr> Diffuse;
	SharedPtr<ID3D12ResourcePtr> Specular;
	SharedPtr<ID3D12ResourcePtr> AmbientOcclusion;

	SharedPtr<ID3D12DescriptorHeapPtr> GBufferHeap{};
	ID3D12DescriptorHeapPtr AOHeap;
};

