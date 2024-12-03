#include "Resources.h"

GlobalResources Globals{};

void GlobalResManager::SetRTV(ID3D12ResourcePtr res, D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
	Globals.RTVBuffer = res;
	Globals.RTVHandle = handle;
}

void GlobalResManager::SetDSV(ID3D12ResourcePtr res, D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
	Globals.DSVBuffer = res;
	Globals.DSVHandle = handle;
}
