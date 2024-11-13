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

void IndexBuffer::Init(ID3D12Device5Ptr device, const std::vector<uint32_t>& indices)
{
	assert(!Buffer && "Constant Buffer already initialized");
	InitImpl(device, indices);
}

void IndexBuffer::InitImpl(ID3D12Device5Ptr device, const std::vector<uint32_t>& indices)
{
	IndexCount = indices.size();
	const UINT bufferSize = static_cast<UINT>(sizeof(uint32_t) * IndexCount);

	Buffer = D3D::CreateBuffer(device, bufferSize, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_HEAP_TYPE_UPLOAD);

	uint32_t* pBufferDataBegin = nullptr;
	CD3DX12_RANGE readRange(0, 0);  // We do not intend to read from this resource on the CPU.
	Buffer->Map(0, &readRange, reinterpret_cast<void**>(&pBufferDataBegin));
	std::memcpy(pBufferDataBegin, indices.data(), bufferSize);
	Buffer->Unmap(0, nullptr);

	BufferView.BufferLocation = Buffer->GetGPUVirtualAddress();
	BufferView.SizeInBytes = bufferSize;
	BufferView.Format = DXGI_FORMAT_R32_UINT;
}
