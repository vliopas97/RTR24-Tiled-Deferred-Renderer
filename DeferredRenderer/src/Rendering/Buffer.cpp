#include "Buffer.h"


LayoutElement::LayoutElement(const std::string& name, DataType type)
	:Name(name), Type(type), Size(CalcSize(type))
{}

LayoutElement::operator D3D12_INPUT_ELEMENT_DESC() const
{
	return { Name.c_str(), 0, DataTypeToDXGI(Type), 0, Offset, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
}

DXGI_FORMAT LayoutElement::DataTypeToDXGI(DataType type)
{
	switch (type)
	{
	case DataType::float1: return DXGI_FORMAT_R32_FLOAT;
	case DataType::float2: return DXGI_FORMAT_R32G32_FLOAT;
	case DataType::float3: return DXGI_FORMAT_R32G32B32_FLOAT;
	case DataType::float4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case DataType::int1: return DXGI_FORMAT_R32_SINT;
	case DataType::int2: return DXGI_FORMAT_R32G32_SINT;
	case DataType::int3: return DXGI_FORMAT_R32G32B32_SINT;
	case DataType::int4: return DXGI_FORMAT_R32G32B32A32_SINT;
	default:
		assert(false && "Unsupported data type");
		return DXGI_FORMAT_UNKNOWN;
	}
}

uint32_t LayoutElement::CalcSize(DataType type)
{
	switch (type)
	{
	case DataType::float1: return sizeof(float);
	case DataType::float2: return sizeof(float) * 2;
	case DataType::float3: return sizeof(float) * 3;
	case DataType::float4: return sizeof(float) * 4;
	case DataType::int1: return  sizeof(int);
	case DataType::int2: return sizeof(int) * 2;
	case DataType::int3: return sizeof(int) * 3;
	case DataType::int4: return sizeof(int) * 4;
	default:
		assert(false && "Unsupported data type");
		return DXGI_FORMAT_UNKNOWN;
	}
}

BufferLayout::BufferLayout(std::initializer_list<LayoutElement> elements)
	:Elements(elements)
{
	InputElementDesc.reserve(Elements.size());

	uint32_t offset = 0;
	for (auto& element : Elements)
	{
		element.Offset = offset;
		offset += element.Size;
		Stride += element.Size;
		InputElementDesc.emplace_back(element);
	}
}

uint32_t BufferLayout::GetStride() const
{
	return Stride;
}

size_t BufferLayout::GetSize() const
{
	return Elements.size();
}

BufferLayout::operator D3D12_INPUT_LAYOUT_DESC() const
{

	return { InputElementDesc.data(), (uint32_t)InputElementDesc.size() };
}

VertexBuffer::VertexBuffer(ID3D12Device5Ptr device, const std::vector<VertexElement>& vertices, const BufferLayout& layout)
	:Layout(layout)
{
	Init(device, vertices);
}

VertexBuffer::VertexBuffer(ID3D12Device5Ptr device, const std::vector<VertexElement>& vertices, std::initializer_list<LayoutElement> layoutElements)
	: Layout(layoutElements)
{
	Init(device, vertices);
}

void VertexBuffer::Init(ID3D12Device5Ptr device, const std::vector<VertexElement>& vertices)
{
	const UINT vertexBufferSize = sizeof(VertexElement) * vertices.size();

	GRAPHICS_ASSERT(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&Buffer)));

	UINT8* pVertexDataBegin;
	CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
	GRAPHICS_ASSERT(Buffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
	std::memcpy(pVertexDataBegin, vertices.data(), vertexBufferSize);
	Buffer->Unmap(0, nullptr);

	BufferView.BufferLocation = Buffer->GetGPUVirtualAddress();
	BufferView.StrideInBytes = sizeof(VertexElement);
	BufferView.SizeInBytes = vertexBufferSize;
}
