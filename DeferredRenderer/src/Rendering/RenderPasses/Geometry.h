#pragma once
#include "RenderPass.h"


class GeometryPass final : public RenderPass
{
public:
	GeometryPass(std::string&& name);
	void Submit(ID3D12GraphicsCommandList4Ptr cmdList, const Scene& scene) override;
protected:
	void InitResources(ID3D12Device5Ptr device) override;
	void InitRootSignature() override;
	void InitPipelineState() override;
private:
	SharedPtr<ID3D12ResourcePtr> Positions;
	SharedPtr<ID3D12ResourcePtr> Normals;
	SharedPtr<ID3D12ResourcePtr> Diffuse;
	SharedPtr<ID3D12ResourcePtr> Specular;

	// GBuffers
	DX::DescriptorHeap RTVHeap{}; // to be used in this pass as RTVs
	SharedPtr<DX::DescriptorHeap> SRVHeap{}; // to be used in lighting pass as SRVs
};