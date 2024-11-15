#include "Lights.h"

DirectionalLight::DirectionalLight()
	:Position(10.0f, 50.0f, 10.0f)
{
	Info.CPUData.Ambient = glm::vec3(0.15, 0.15f, 0.15f);
	Info.CPUData.DiffuseColor = glm::vec3(1.0f, 1.0f, 1.0f);
	Info.CPUData.DiffuseIntensity = 1.0f;
}

void DirectionalLight::Bind(ID3D12GraphicsCommandList4Ptr cmdList) const
{
	//cmdList->SetGraphicsRootConstantBufferView(LightCBuffer + 1, Info.GetGPUVirtualAddress());// ATTENTION WITH THE SLOT!!!!
}

void DirectionalLight::GUI()
{
	ImGui::Begin("Light Properties");
	auto& x = Position.x;
	auto& y = Position.y;
	auto& z = Position.z;
	ImGui::Text("Position");
	ImGui::SliderFloat("X", &x, -50.0f, 50.0f, "%.1f");
	ImGui::SliderFloat("Y", &y, 40.0f, 100.0f, "%.1f");
	ImGui::SliderFloat("Z", &z, -30.0f, 30.0f, "%.1f");

	float ambient = Info.CPUData.Ambient.x;
	ImGui::Text("Ambient");
	ImGui::SliderFloat("Ambient", &ambient, 0.0f, 1.0f, "%.05f");
	Info.CPUData.Ambient = glm::vec3(ambient);

	ImGui::End();
}

void DirectionalLight::Tick()
{
	Info.CPUData.Position = Position;
	Info.CPUData.Direction = glm::normalize(-Position);
	Info.Tick();
}

void DirectionalLight::SetUpGPUResources(ID3D12Device5Ptr device, D3D12_CPU_DESCRIPTOR_HANDLE destDescriptor)
{
	Info.Init(device, destDescriptor);
}
