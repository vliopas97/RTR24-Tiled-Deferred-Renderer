#include "RenderPass.h"
#include "Core/Exception.h"
#include "Shader.h"
#include "PipelineResources.h"

void RenderPass::Init(ID3D12Device5Ptr device)
{
	Device = device;
	Resources.Setup(Device);
	InitRootSignature();
	InitPipelineState();
}

void RenderPass::Bind(ID3D12GraphicsCommandList4Ptr cmdList)
{
	//RootSignatureData
	cmdList->SetGraphicsRootSignature(RootSignatureData.RootSignaturePtr.GetInterfacePtr());
	Resources.Bind(cmdList);
}

void ForwardRenderPass::InitRootSignature()
{
	std::vector<D3D12_DESCRIPTOR_RANGE> srvRanges;
	std::vector<D3D12_DESCRIPTOR_RANGE> cbvRanges;

	std::vector<D3D12_DESCRIPTOR_RANGE> uavRanges = {
		DescriptorRangeBuilder::CreateRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 2, 0)
	};

	uint32_t userStart = NumGlobalSRVDescriptorRanges - NumUserDescriptorRanges;
	for (uint32_t i = 0; i < NumGlobalSRVDescriptorRanges; ++i)
	{
		UINT registerSpace = (i >= userStart) ? (i - userStart) + 100 : i;
		srvRanges.emplace_back(DescriptorRangeBuilder::CreateRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UINT_MAX, 0, registerSpace));
	}

	for (uint32_t i = 0; i < NumGlobalCBVDescriptorRanges; ++i)
		cbvRanges.emplace_back(DescriptorRangeBuilder::CreateRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, UINT_MAX, 0, i));

	std::vector<D3D12_DESCRIPTOR_RANGE> samplerRanges = {
	DescriptorRangeBuilder::CreateRange(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0)
	};

	std::vector<D3D12_DESCRIPTOR_RANGE> lightRanges = {
	DescriptorRangeBuilder::CreateRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 100)
	};

	RootSignatureData.AddDescriptorTable(srvRanges);
	RootSignatureData.AddDescriptorTable(uavRanges);
	RootSignatureData.AddDescriptorTable(cbvRanges);
	RootSignatureData.AddDescriptorTable(samplerRanges);
	RootSignatureData.AddDescriptorTable(lightRanges);
	RootSignatureData.AddDescriptor(D3D12_ROOT_PARAMETER_TYPE_CBV, D3D12_SHADER_VISIBILITY_ALL, 0, 200);

	RootSignatureData.Build(Device);

}

void ForwardRenderPass::InitPipelineState()
{
	Shader<Vertex> vertexShader("Shader");
	Shader<Pixel> pixelShader("Shader");

	BufferLayout layout{ {"POSITION", DataType::float3},
						{"NORMAL", DataType::float3},
						{"TANGENT", DataType::float3},
						{"BITANGENT", DataType::float3},
						{"TEXCOORD", DataType::float2}, };

	CD3DX12_RASTERIZER_DESC rasterizerDesc(D3D12_DEFAULT);
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;  // No culling

	// Describe and create the graphics pipeline state object (PSO).
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = layout;
	psoDesc.pRootSignature = RootSignatureData.RootSignaturePtr.GetInterfacePtr();
	psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.GetBlob());
	psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.GetBlob());
	psoDesc.RasterizerState = rasterizerDesc;
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDesc.SampleDesc.Count = 1;

	GRAPHICS_ASSERT(Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&PipelineState)));
}
