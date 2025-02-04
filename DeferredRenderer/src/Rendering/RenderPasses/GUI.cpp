#include "GUI.h"
#include "Scene.h"

GUIPass::GUIPass(std::string&& name)
	:RenderPass(std::move(name))
{
	Register<PassInput<ID3D12ResourcePtr>>("positions", Positions, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Register<PassInput<ID3D12ResourcePtr>>("normals", Normals, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Register<PassInput<ID3D12ResourcePtr>>("diffuse", Diffuse, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Register<PassInput<ID3D12ResourcePtr>>("specular", Specular, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Register<PassInput<ID3D12ResourcePtr>>("ambientOcclusion", AmbientOcclusion, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Register<PassInput<ID3D12DescriptorHeapPtr>>("srvHeap", SRVHeap);

	Register<PassOutput<ID3D12ResourcePtr>>("positions", Positions, D3D12_RESOURCE_STATE_RENDER_TARGET);
	Register<PassOutput<ID3D12ResourcePtr>>("normals", Normals, D3D12_RESOURCE_STATE_RENDER_TARGET);
	Register<PassOutput<ID3D12ResourcePtr>>("diffuse", Diffuse, D3D12_RESOURCE_STATE_RENDER_TARGET);
	Register<PassOutput<ID3D12ResourcePtr>>("specular", Specular, D3D12_RESOURCE_STATE_RENDER_TARGET);
	Register<PassOutput<ID3D12ResourcePtr>>("ambientOcclusion", AmbientOcclusion, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

GUIPass::~GUIPass()
{
	Layer->OnDetach();
}

void GUIPass::Submit(ID3D12GraphicsCommandList4Ptr cmdList, const Scene& scene)
{
	Layer->Begin();
	Bind(cmdList);
	scene.Bind<GUIPass>(cmdList);

	GBuffersViewerWindow();

	Layer->End(cmdList);
}

void GUIPass::InitResources(ID3D12Device5Ptr device)
{
	auto temp = D3D::CreateDescriptorHeap(device, 1 + (*SRVHeap)->GetDesc().NumDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, false);
	auto dstHandle = temp->GetCPUDescriptorHandleForHeapStart();
	auto srcHandle = (*SRVHeap)->GetCPUDescriptorHandleForHeapStart();
	device->CopyDescriptorsSimple(4, dstHandle,
								  srcHandle,
								  D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	dstHandle.ptr += 4 * (device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	device->CreateShaderResourceView(*AmbientOcclusion, &srvDesc, dstHandle);

	Layer = MakeUnique<ImGuiLayer>(temp);
	Layer->OnAttach(Device);

	auto handle = Layer->DescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	for (UINT i = 0; i < GPUHandlesGBuffers.size(); i++)
	{
		handle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		GPUHandlesGBuffers[i] = handle;
	}
}

void GUIPass::GBuffersViewerWindow() const
{
	ImGui::Begin("G-Buffer Viewer");

	float aspectRatio = 9.0f / 16.0f;
	// Define the min and max sizes for the images
	ImVec2 minSize(360.0f, aspectRatio * 360.0f);
	ImVec2 maxSize(480.0f, aspectRatio * 480.0f);

	ImVec2 availableSize = ImGui::GetContentRegionAvail();

	float width = std::max(minSize.x, std::min(maxSize.x, availableSize.x));
	float height = width * 9.0f / 16.0f;

	// If height exceeds available height, adjust width and height
	if (height > availableSize.y)
	{
		height = std::max(minSize.y, std::min(maxSize.y, availableSize.y));
		width = height * 16.0f / 9.0f;
	}

	ImVec2 imageSize(width, height);

	// Dropdown items
	const char* items[] = {
		"Positions (View Space)",
		"Normals (View Space)",
		"Diffuse",
		"Specular"
	};

	static int selectedItem = 1;

	// Dropdown
	if (ImGui::Combo("Select Texture", &selectedItem, items, IM_ARRAYSIZE(items)))
		std::cout << "Selected: " << items[selectedItem] << std::endl;

	ImGui::Image((ImTextureID)GPUHandlesGBuffers[selectedItem].ptr, imageSize);

	BOOL& ssaoEnabled = Globals.CBGlobalConstants.CPUData.SSAOEnabled;
	bool checkboxState = (ssaoEnabled != 0);

	if (ImGui::Checkbox("Enable SSAO", &checkboxState))
		ssaoEnabled = checkboxState ? 1 : 0;
	ImGui::SliderFloat("SSAO Radius", &Globals.CBGlobalConstants.CPUData.RadiusSSAO, 0.01f, 2.0f, "%.02f");
	ImGui::SliderFloat("SSAO Intensity", &Globals.CBGlobalConstants.CPUData.IntensitySSAO, 0.5f, 4.0f, "%.1f");

	ImGui::Text("Ambient Occlusion: ");
	ImGui::Image((ImTextureID)GPUHandlesGBuffers[4].ptr, imageSize);

	ImGui::End();
}