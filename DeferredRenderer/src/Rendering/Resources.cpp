#include "Resources.h"

#include <algorithm>

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

void GlobalResManager::SetCmdAllocator(ID3D12CommandAllocatorPtr cmdAllocator)
{
	Globals.CmdAllocator = cmdAllocator;
}

static auto isValidName = [](const std::string& name)
{
	return !name.empty() &&
		std::ranges::all_of(name, [](char c) { return std::isalnum(c) || c == '_'; }) &&
		!std::isdigit(name.front());
};

PassInputBase::PassInputBase(std::string&& name, D3D12_RESOURCE_STATES state)
	: Name(std::move(name)), State(state)
{
	if (Name.empty())
		throw std::invalid_argument("Invalid name - Empty pass name");

	if (!isValidName(Name))
		throw std::invalid_argument("Invalid name - unsupported characters");
}

PassOutputBase::PassOutputBase(std::string&& name, D3D12_RESOURCE_STATES state)
	:Name(std::move(name)), State(state)
{}


void PassInputBase::SetTarget(std::string passName, std::string outputName)
{
	if (passName.empty())
		throw std::invalid_argument("Empty pass name");
	if (passName != "$" && !isValidName(passName))
		throw std::invalid_argument("Invalid pass name - unsupported characters");
	if (!isValidName(outputName))
		throw std::invalid_argument("Invalid output name - unsupported characters");

	PassName = std::move(passName);
	OutputName = std::move(outputName);
}