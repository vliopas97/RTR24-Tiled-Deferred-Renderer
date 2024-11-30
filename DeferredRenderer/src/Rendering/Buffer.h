#pragma once
#include "Core/Core.h"
#include "Core/Exception.h"
#include "Utils.h"

struct VertexElement
{
	glm::float3 Position;
};

enum class DataType
{
	float1, float2, float3, float4,
	int1, int2, int3, int4
};

struct LayoutElement
{
public:
	LayoutElement(const std::string& name, DataType type);

	operator D3D12_INPUT_ELEMENT_DESC() const;

private:
	static DXGI_FORMAT DataTypeToDXGI(DataType type);
	static uint32_t CalcSize(DataType type);

public:
	std::string Name;
	DataType Type;
	uint32_t Offset;
	uint32_t Size;

};

struct BufferLayout
{
	BufferLayout() = default;
	BufferLayout(std::initializer_list<LayoutElement> elements);

	uint32_t GetStride() const;
	size_t GetSize() const;

	operator D3D12_INPUT_LAYOUT_DESC() const;
private:

	std::vector<LayoutElement> Elements;
	std::vector< D3D12_INPUT_ELEMENT_DESC> InputElementDesc;
	uint32_t Stride = 0;
};

template<typename T>
concept IsVertexElement = std::is_base_of_v<VertexElement, T>;

struct VertexBuffer
{
	VertexBuffer() = default;

	template<IsVertexElement Vertex>
	void Init(ID3D12Device5Ptr device, const std::vector<Vertex>& vertices, const BufferLayout& layout)
	{
		ASSERT(!Buffer, "Constant Buffer already initialized");
		Layout = layout;
		InitImpl(device, vertices);
	}
	
	template<IsVertexElement Vertex>
	void Init(ID3D12Device5Ptr device, const std::vector<Vertex>& vertices, std::initializer_list<LayoutElement> layoutElements)
	{
		ASSERT(!Buffer, "Constant Buffer already initialized");
		Layout = { layoutElements };
		InitImpl(device, vertices);
	}

	inline const BufferLayout& GetLayout() const { return Layout; }
	inline const D3D12_VERTEX_BUFFER_VIEW& GetView() const { return BufferView; }
	inline uint32_t GetCountPerInstance() const { return CountPerInstance; }

private:
	template<IsVertexElement Vertex>
	void InitImpl(ID3D12Device5Ptr device, const std::vector<Vertex>& vertices)
	{
		CountPerInstance = vertices.size();
		const UINT bufferSize = sizeof(Vertex) * CountPerInstance;

		Buffer = D3D::CreateBuffer(device, bufferSize, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_HEAP_TYPE_UPLOAD);

		UINT8* pBufferDataBegin;
		CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
		GRAPHICS_ASSERT(Buffer->Map(0, &readRange, reinterpret_cast<void**>(&pBufferDataBegin)));
		std::memcpy(pBufferDataBegin, vertices.data(), bufferSize);
		Buffer->Unmap(0, nullptr);

		BufferView.BufferLocation = Buffer->GetGPUVirtualAddress();
		BufferView.StrideInBytes = sizeof(Vertex);
		BufferView.SizeInBytes = bufferSize;
	}

private:
	BufferLayout Layout;
	ID3D12ResourcePtr Buffer;
	D3D12_VERTEX_BUFFER_VIEW BufferView;

	uint32_t CountPerInstance;
};

struct IndexBuffer
{
    IndexBuffer() = default;
	void Init(ID3D12Device5Ptr device, const std::vector<uint32_t>& indices);

    inline const D3D12_INDEX_BUFFER_VIEW& GetView() const { return BufferView; }
	inline uint32_t GetIndexCount() const { return IndexCount; }

private:
	void InitImpl(ID3D12Device5Ptr device, const std::vector<uint32_t>& indices);

private:
    ID3D12ResourcePtr Buffer;
    D3D12_INDEX_BUFFER_VIEW BufferView;
	uint32_t IndexCount;
};

template<typename T>
struct ConstantBuffer
{
	ConstantBuffer() = default;
	void Init(ID3D12Device5Ptr device, D3D12_CPU_DESCRIPTOR_HANDLE destDescriptor)
	{
		ASSERT(!Buffer, "Constant Buffer already initialized");
		InitImpl(device, destDescriptor);
	}

	void Tick()
	{
		std::memcpy(CPUDataBridgePtr, &CPUData, sizeof(CPUData));
	}

	D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const
	{
		return Buffer->GetGPUVirtualAddress();
	}

private:
	void InitImpl(ID3D12Device5Ptr device, D3D12_CPU_DESCRIPTOR_HANDLE destDescriptor)
	{
		const UINT bufferSize = align_to(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(CPUData));

		Buffer = D3D::CreateBuffer(device, bufferSize, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_HEAP_TYPE_UPLOAD);
		
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = Buffer->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = bufferSize;
		device->CreateConstantBufferView(&cbvDesc, destDescriptor);

		GRAPHICS_ASSERT(Buffer->Map(0, nullptr, reinterpret_cast<void**>(&CPUDataBridgePtr)));
		std::memcpy(CPUDataBridgePtr, &CPUData, sizeof(CPUData));
	}

public:
	T CPUData;

private:
	ID3D12ResourcePtr Buffer;
	uint8_t* CPUDataBridgePtr;
};
