#pragma once
#include "RenderPass.h"


class ClearPass final : public RenderPass
{
public:
	ClearPass(std::string&& name);
	void Submit(ID3D12GraphicsCommandList4Ptr cmdList, const Scene& scene) override;
protected:
	void Bind(ID3D12GraphicsCommandList4Ptr cmdList) const override;
	inline void InitResources(ID3D12Device5Ptr device) override {}
	void InitRootSignature() override {}
	void InitPipelineState() override {}
};
