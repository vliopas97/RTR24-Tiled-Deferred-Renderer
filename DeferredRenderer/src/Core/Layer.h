#pragma once
#include "Core.h"
#include "Events\Event.h"

struct Graphics;

struct ImGuiLayer
{
	ImGuiLayer(ID3D12DescriptorHeapPtr descHeap = nullptr);
	~ImGuiLayer() = default;

	void OnAttach(ID3D12Device5Ptr device);
	void OnDetach();
	void OnEvent(Event& e);

	void Begin();
	void End(ID3D12GraphicsCommandList4Ptr cmdList);

	ID3D12DescriptorHeapPtr DescriptorHeap;
};
