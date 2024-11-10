#pragma once
#include "Core/Core.h"
#include "Rendering/Buffer.h"

class Actor
{
public:
	Actor(ID3D12Device5Ptr device);
	void Bind(ID3D12GraphicsCommandList4Ptr cmdList) const;

private:
	VertexBuffer VBuffer;
	IndexBuffer IBuffer;
};

