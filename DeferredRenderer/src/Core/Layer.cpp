#include "Application.h"
#include "Core/Exception.h"
#include "Layer.h"
#include "Rendering/Utils.h"

void ImGui_ImplWin32_InitPlatformInterface();

ImGuiLayer::ImGuiLayer()
{
}

void ImGuiLayer::OnAttach(ID3D12Device5Ptr device)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.WantCaptureKeyboard = true;
	io.WantCaptureMouse = true;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	ImGui::StyleColorsDark();

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
	heapDesc.NumDescriptors = 1;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	GRAPHICS_ASSERT(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&DescriptorHeap)));

	ImGui_ImplWin32_Init(Application::GetApp().GetWindow()->GetHandle());
	ImGui_ImplDX12_Init(device, 
						DefaultSwapChainBuffers, 
						DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
						DescriptorHeap,
						DescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
						DescriptorHeap->GetGPUDescriptorHandleForHeapStart());
}

void ImGuiLayer::OnDetach()
{
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void ImGuiLayer::OnEvent(Event& e)
{
	ImGuiIO& io = ImGui::GetIO();
	e.Handled = e.GetCategory() == EventCategory::MouseEvents && io.WantCaptureMouse;
	e.Handled = e.GetCategory() == EventCategory::KeyEvents && io.WantCaptureKeyboard;
}

void ImGuiLayer::Begin()
{
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void ImGuiLayer::End(ID3D12GraphicsCommandList4Ptr cmdList)
{
	ImGuiIO& io = ImGui::GetIO();
	Application& app = Application::GetApp();
	io.DisplaySize = ImVec2((float)app.GetWindow()->GetWidth(), (float)app.GetWindow()->GetHeight());

	static bool showDemoWindow = true;
	if (showDemoWindow)
		ImGui::ShowDemoWindow(&showDemoWindow);

	ImGui::Render();

	ID3D12DescriptorHeap* descriptorHeaps[] = { DescriptorHeap.GetInterfacePtr() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);

	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}
}