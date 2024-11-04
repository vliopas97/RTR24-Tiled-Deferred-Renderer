#pragma once

#include "Core/Core.h"
#include "Window/Window.h"
#include "Buffer.h"
#include "Shader.h"

struct Graphics
{
	Graphics(Window& window);
	~Graphics();

	void Tick(float delta);

private:
	void Init();
    void InitScene();
	void Shutdown();
	void PopulateCommandList(UINT frameIndex);

	void CreateDevice();
	void CreateSwapChain();

    void CreateShaderResources();

private:
    HWND WinHandle{ nullptr };

	IDXGIFactory4Ptr Factory;
	ID3D12DebugPtr Debug;

	ID3D12Device5Ptr Device;
	IDXGISwapChain3Ptr SwapChain;
    glm::uvec2 SwapChainSize;

	ID3D12CommandQueuePtr CmdQueue;
    ID3D12GraphicsCommandList4Ptr CmdList;

    PipelineStateBindings PipelineBindings;
    ID3D12PipelineStatePtr PipelineState;
    
    VertexBuffer VBuffer;

    ID3D12FencePtr Fence;
    HANDLE FenceEvent;
    uint64_t FenceValue = 0;

    struct
    {
        ID3D12CommandAllocatorPtr CmdAllocator;
        ID3D12ResourcePtr SwapChainBuffer;
        D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle;
    } FrameObjects[DefaultSwapChainBuffers];

    struct HeapData
    {
        ID3D12DescriptorHeapPtr Heap;
        uint32_t UsedEntries = 0;
    };

    HeapData RTVHeap;
    static const uint32_t RTVHeapSize = 3;
};

