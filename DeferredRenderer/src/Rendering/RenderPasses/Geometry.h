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
	SharedPtr<ID3D12DescriptorHeapPtr> RTVHeap{}; // to be used in this pass as RTVs
	SharedPtr<ID3D12DescriptorHeapPtr> SRVHeap{}; // to be used in lighting pass as SRVs
	SharedPtr<ID3D12DescriptorHeapPtr> SRVHeapRO{}; // read only alternative of SRVHeap - to be used for GUI displaying
	std::array<D3D12_CPU_DESCRIPTOR_HANDLE, 4> RTVHandles{};
};