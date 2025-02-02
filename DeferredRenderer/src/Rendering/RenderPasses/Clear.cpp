#include "Clear.h"
#include "Scene.h"

ClearPass::ClearPass(std::string&& name)
	:RenderPass(std::move(name))
{
	Register<PassInput<ID3D12ResourcePtr>>("renderTarget", RTVBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
	Register<PassInput<ID3D12ResourcePtr>>("depthBuffer", DSVBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	Register<PassOutput<ID3D12ResourcePtr>>("renderTarget", RTVBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
	Register<PassOutput<ID3D12ResourcePtr>>("depthBuffer", DSVBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);
}

void ClearPass::Submit(ID3D12GraphicsCommandList4Ptr cmdList, const Scene& scene)
{
	Bind(cmdList);
	scene.Bind<ClearPass>(cmdList);
}

void ClearPass::Bind(ID3D12GraphicsCommandList4Ptr cmdList) const
{
	RenderPass::Bind(cmdList);

	const float clearColor[4] = { 0.4f, 0.6f, 0.2f, 1.0f };
	cmdList->ClearRenderTargetView(Globals.RTVHandle, clearColor, 0, nullptr);
	cmdList->ClearDepthStencilView(Globals.DSVHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0.0f, 0, nullptr);
}