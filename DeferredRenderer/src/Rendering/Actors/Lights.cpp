#include "Lights.h"

DirectionalLight::DirectionalLight()
	:Position(10.0f, 50.0f, 10.0f)
{}

void DirectionalLight::Bind(ID3D12GraphicsCommandList4Ptr cmdList) const
{
	//cmdList->SetGraphicsRootConstantBufferView(4, Info.GetGPUVirtualAddress());// ATTENTION WITH THE SLOT!!!!
}

void DirectionalLight::GUI()
{
	ImGui::Begin("Light Position");
	auto& x = Position.x;
	auto& y = Position.y;
	auto& z = Position.z;
	ImGui::Text("Position");
	ImGui::SliderFloat("X", &x, -50.0f, 50.0f, "%.1f");
	ImGui::SliderFloat("Y", &y, 40.0f, 100.0f, "%.1f");
	ImGui::SliderFloat("Z", &z, -30.0f, 30.0f, "%.1f");
	ImGui::End();
}

void DirectionalLight::Tick()
{
	Info.CPUData.Model = glm::translate(glm::mat4(1.0f), Position);
	Info.CPUData.Position = Position;
	Info.CPUData.Direction = glm::normalize(-Position);
	Info.Tick();
}

void DirectionalLight::SetUpGPUResources(ID3D12Device5Ptr device, D3D12_CPU_DESCRIPTOR_HANDLE destDescriptor)
{
	Info.Init(device, destDescriptor);
}
