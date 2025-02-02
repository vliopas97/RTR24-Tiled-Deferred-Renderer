#pragma once
#include "RenderPass.h"

class ForwardRenderPass final : public RenderPass
{
public:
	ForwardRenderPass(std::string&& name);
	void Submit(ID3D12GraphicsCommandList4Ptr cmdList, const Scene& scene) override;
protected:
	void InitResources(ID3D12Device5Ptr device) override;
	void InitRootSignature() override;
	void InitPipelineState() override;
};

