#pragma once

#include "Core/Core.h"
#include "Rendering/RootSignature.h"

class RenderPass
{
public:
	RenderPass() = default;
	virtual ~RenderPass() = default;
	
	void Init(ID3D12Device5Ptr device);
	void Bind(ID3D12GraphicsCommandList4Ptr cmdList);

	inline ID3D12PipelineStatePtr GetPSO() { return PipelineState; }

protected:
	virtual void InitRootSignature() = 0;
	virtual void InitPipelineState() = 0;

protected:
	ID3D12Device5Ptr Device;

	ID3D12PipelineStatePtr PipelineState;
	RootSignature RootSignatureData;
	RenderPassResources Resources;
};

class ForwardRenderPass final : public RenderPass
{

protected:
	void InitRootSignature() override;
	void InitPipelineState() override;
};

