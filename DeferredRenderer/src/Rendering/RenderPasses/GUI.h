#pragma once
#include "RenderPass.h"


class GUIPass final : public RenderPass
{
public:
	GUIPass(std::string&& name);
	~GUIPass();
	void Submit(ID3D12GraphicsCommandList4Ptr cmdList, const Scene& scene) override;
	void PreExecTransitions(ID3D12GraphicsCommandList4Ptr cmdList) override;
	void PostExecTransitions(ID3D12GraphicsCommandList4Ptr cmdList) override;

protected:
	void InitResources(ID3D12Device5Ptr device) override;
	void InitRootSignature() override {}
	void InitPipelineState() override {}
private:
	void GBuffersViewerWindow() const;

private:
	UniquePtr<ImGuiLayer> Layer;

	SharedPtr<DX::Texture> Positions;
	SharedPtr<DX::Texture> Normals;
	SharedPtr<DX::Texture> Diffuse;
	SharedPtr<DX::Texture> Specular;

	SharedPtr<DX::Texture> AmbientOcclusion;

	ID3D12DescriptorHeapPtr SRVHeap{};
	std::array<D3D12_GPU_DESCRIPTOR_HANDLE, 5> GPUHandlesGBuffers;
};

