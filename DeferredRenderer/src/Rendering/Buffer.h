#pragma once
#include "Core/Core.h"
#include "Core/Exception.h"
#include "Utils.h"

struct VertexElement
{
	glm::float3 position;
	glm::float4 color;
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

struct VertexBuffer
{
	VertexBuffer() = default;
	VertexBuffer(ID3D12Device5Ptr device, const std::vector<VertexElement>& vertices, const BufferLayout& layout);
	VertexBuffer(ID3D12Device5Ptr device, const std::vector<VertexElement>& vertices, std::initializer_list<LayoutElement> layoutElements);

	inline const BufferLayout& GetLayout() const { return Layout; }
	inline const D3D12_VERTEX_BUFFER_VIEW& GetView() const { return BufferView; }

private:
	void Init(ID3D12Device5Ptr device, const std::vector<VertexElement>& vertices);

private:
	BufferLayout Layout;
	ID3D12ResourcePtr Buffer;
	D3D12_VERTEX_BUFFER_VIEW BufferView;
};

