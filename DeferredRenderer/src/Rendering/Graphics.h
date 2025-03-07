#pragma once

#include "Buffer.h"
#include "Camera.h"
#include "Core/Core.h"
#include "Shader.h"
#include "Window/Window.h"
#include "Actors/Actor.h"
#include "Scene.h"
#include "Rendering/RootSignature.h"
#include "Rendering/RenderPasses/RenderPass.h"
#include "Rendering/Resources.h"
#include "Rendering/RenderGraph.h"

struct ImGuiLayer;

struct Graphics
{
	Graphics(Window& window);
	~Graphics();

	void Tick(float delta);

    ID3D12Device5Ptr GetDevice() { return Device; }
    ID3D12CommandAllocatorPtr GetCommandAllocator() { return FrameObjects[SwapChain->GetCurrentBackBufferIndex()].CmdAllocator; }

private:
	void Init();
    void InitGlobals();
    void InitScene();
	void Shutdown();

	void CreateDevice();
	void CreateSwapChain();

    void CreateShaderResources();

    void UpdateGlobals(UINT frameIndex, float delta);
    void EndFrame(UINT frameIndex);

private:
    HWND WinHandle{ nullptr };

	IDXGIFactory4Ptr Factory;
	ID3D12DebugPtr Debug;

	ID3D12Device5Ptr Device;
	IDXGISwapChain3Ptr SwapChain;
    glm::uvec2 SwapChainSize;

        UniquePtr<RenderGraph> Graph;
    UniquePtr<Scene> MainScene;

    struct
    {
        ID3D12CommandAllocatorPtr CmdAllocator;
        
        ID3D12ResourcePtr SwapChainBuffer;
        ID3D12ResourcePtr DepthStencilBuffer;

        D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle;
        D3D12_CPU_DESCRIPTOR_HANDLE DSVHandle;
    } FrameObjects[DefaultSwapChainBuffers];

    struct HeapData
    {
        ID3D12DescriptorHeapPtr Heap;
        uint32_t UsedEntries = 0;
    };

    HeapData RTVHeap;
    HeapData DSVHeap;
    static const uint32_t RTVHeapSize = 3;
    static const uint32_t DSVHeapSize = 3;

    Camera SceneCamera;
};

