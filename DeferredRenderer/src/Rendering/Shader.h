#pragma once
#include "Core/Core.h"
#include "Buffer.h"
#include "Shaders/HLSLCompat.h"

#include <filesystem>

enum class ShaderType
{
	Vertex = 0,
	Pixel,
	Compute,
	Size
};

constexpr ShaderType Vertex = ShaderType::Vertex;
constexpr ShaderType Pixel = ShaderType::Pixel;
constexpr ShaderType Compute = ShaderType::Compute;

template<ShaderType Type>
struct Shader final
{
	Shader(const std::string& shaderName)
		:Name(shaderName)
	{
		auto path = SetUpPath(Name) + string_2_wstring(Name) + GetTag() + L".cso";
		D3DReadFileToBlob(path.c_str(), &Blob);
	}
	virtual ~Shader() = default;
	const ID3DBlobPtr& GetBlob() const { return Blob; }

private:
	std::wstring SetUpPath(const std::string& shaderName)
	{
		std::string currentDir = std::filesystem::current_path().parent_path().string();
		return std::wstring(currentDir.begin(), currentDir.end()) + L"\\Content\\Shaders-bin\\";
	}

	virtual std::wstring GetTag()
	{
		if constexpr (Type == ShaderType::Vertex) return L"_VS";
		else if constexpr (Type == ShaderType::Pixel) return L"_PS";
		else if constexpr (Type == ShaderType::Compute) return L"_CS";
		else
		{
			assert(false && "Invalid Shader Type");
			return L"_Unknown";
		}
	}

private:
	ID3DBlobPtr Blob;
	std::string Name;

private:
	static const std::wstring Path;
};