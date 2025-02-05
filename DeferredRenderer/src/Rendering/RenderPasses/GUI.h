#pragma once
#include "RenderPass.h"


class GUIPass final : public RenderPass
{
public:
	GUIPass(std::string&& name);
	~GUIPass();
	void Submit(ID3D12GraphicsCommandList4Ptr cmdList, const Scene& scene) override;
protected:
	void InitResources(ID3D12Device5Ptr device) override;
	void InitRootSignature() override {}
	void InitPipelineState() override {}
private:
	void GBuffersViewerWindow() const;

private:
	UniquePtr<ImGuiLayer> Layer;

	SharedPtr<ID3D12ResourcePtr> Positions;
	SharedPtr<ID3D12ResourcePtr> Normals;
	SharedPtr<ID3D12ResourcePtr> Diffuse;
	SharedPtr<ID3D12ResourcePtr> Specular;

	SharedPtr<ID3D12ResourcePtr> AmbientOcclusion;

	ID3D12DescriptorHeapPtr SRVHeap{};
	std::array<D3D12_GPU_DESCRIPTOR_HANDLE, 5> GPUHandlesGBuffers;
};

