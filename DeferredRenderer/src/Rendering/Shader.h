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

enum RootParamTypes : uint32_t
{
	StandardDescriptors = 0,
	UAVDescriptor,
	CBuffer,
	Sampler,
	LightCBuffer,
	GlobalDescTablesNo
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

struct RootSignatureDesc
{
	D3D12_ROOT_SIGNATURE_DESC Desc = {};
	std::vector<D3D12_DESCRIPTOR_RANGE> Range;
	std::vector<D3D12_ROOT_PARAMETER> RootParams;
};

template <typename Fn, typename... Args>
concept ValidRootSignatureFactory = requires(Fn fn, Args... args)
{
	std::is_invocable_v<Fn, Args...>&&
		std::is_same_v<std::invoke_result_t<Fn, Args...>, ID3D12RootSignaturePtr>;
};

struct RootSignature
{
	RootSignature(ID3D12Device5Ptr device);
	RootSignature(ID3D12Device5Ptr device, ID3D12RootSignaturePtr rootSig);
	template <typename Fn, typename... Args>
		requires ValidRootSignatureFactory<Fn, Args...>
	RootSignature(ID3D12Device5Ptr device, Fn f, Args... args)
	{
		RootSignaturePtr = f(args...);
		Interface = RootSignaturePtr.GetInterfacePtr();
		Subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
		Subobject.pDesc = &Interface;
	}

	ID3D12RootSignaturePtr RootSignaturePtr;
	ID3D12RootSignature* Interface = nullptr;
	D3D12_STATE_SUBOBJECT Subobject = {};
};

struct PipelineStateBindings
{
	using RootSignaturePtr = UniquePtr<RootSignature>;
	PipelineStateBindings() = default;
	~PipelineStateBindings() = default;

	void Initialize(ID3D12Device5Ptr device, RootSignature rootSignature);
	ID3D12RootSignature* GetRootSignaturePtr();

	void Bind(ID3D12GraphicsCommandList4Ptr cmdList);
	void Tick();

private:
	void Setup(ID3D12Device5Ptr device);
	void BindDescriptorHeap(ID3D12GraphicsCommandList4Ptr cmdList, ID3D12DescriptorHeapPtr heap);

public:
	ID3D12DescriptorHeapPtr SRVHeap;
	ID3D12DescriptorHeapPtr UAVHeap;
	ID3D12DescriptorHeapPtr CBVHeap;
	ID3D12DescriptorHeapPtr SamplerHeap;
	ID3D12DescriptorHeapPtr LightsHeap;

	ConstantBuffer<PipelineConstants> CBGlobalConstants;

private:
	RootSignaturePtr RootSignatureData;
};