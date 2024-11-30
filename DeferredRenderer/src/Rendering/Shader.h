#pragma once
#include "Core/Core.h"
#include "Buffer.h"
#include "Shaders/HLSLCompat.h"

enum class ShaderType
{
	Vertex = 0,
	Pixel,
	Size
};

constexpr ShaderType Vertex = ShaderType::Vertex;
constexpr ShaderType Pixel = ShaderType::Pixel;

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
		std::string currentDir = std::string(std::source_location::current().file_name());
		currentDir = currentDir.substr(0, currentDir.find_last_of("\\/"));
		return std::wstring(currentDir.begin(), currentDir.end()) + L"\\Shaders\\build\\";
	}

	virtual std::wstring GetTag()
	{
		if constexpr (Type == ShaderType::Vertex) return L"_VS";
		else if constexpr (Type == ShaderType::Pixel) return L"_PS";
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