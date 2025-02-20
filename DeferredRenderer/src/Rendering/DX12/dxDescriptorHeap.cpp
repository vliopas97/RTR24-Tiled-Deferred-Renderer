#include "dxDescriptorHeap.h"
#include "Core/Exception.h"

namespace DX
{
    DescriptorHeap::Iterator::Iterator(const DescriptorHeap* heap, uint32_t index)
        : heap(heap), Index(index)
    {}

    DescriptorHandle DescriptorHeap::Iterator::operator*() const
    {
        ASSERT(Index < heap->NumDescriptors || Index < 0, "Dereferencing out-of-bounds iterator");
        return (*heap)[Index];
    }

    SharedPtr<DescriptorHandle> DescriptorHeap::Iterator::operator->() const
    {
        ASSERT(Index < heap->NumDescriptors || Index < 0, "Dereferencing out-of-bounds iterator");
        auto ptr = MakeShared<DescriptorHandle>((*heap)[Index]);
        return ptr;
    }

    DescriptorHeap::Iterator& DescriptorHeap::Iterator::operator++()
    {
        ASSERT(Index < heap->NumDescriptors, "Incrementing out-of-bounds iterator");
        ++Index;
        return *this;
    }

    DescriptorHeap::Iterator DescriptorHeap::Iterator::operator++(int)
    {
        Iterator tmp = *this;
        ++(*this); // Reuse prefix increment
        return tmp;
    }

    DescriptorHeap::Iterator& DescriptorHeap::Iterator::operator--()
    {
        ASSERT(Index > 0, "Decrementing out-of-bounds iterator");
        --Index;
        return *this;
    }

    DescriptorHeap::Iterator DescriptorHeap::Iterator::operator--(int)
    {
        Iterator tmp = *this;
        --(*this); // Reuse prefix decrement
        return tmp;
    }

    DescriptorHeap::Iterator DescriptorHeap::Iterator::operator+(DifferenceType n) const
    {
        ASSERT(Index + n < heap->NumDescriptors, "Adding out-of-bounds iterator");
        return Iterator(heap, Index + n);
    }

    DescriptorHeap::Iterator DescriptorHeap::Iterator::operator-(DifferenceType n) const
    {
        ASSERT(Index >= n, "Subtracting out-of-bounds iterator");
        return Iterator(heap, Index - n);
    }

    DescriptorHeap::Iterator::DifferenceType DescriptorHeap::Iterator::operator-(const Iterator& other) const
    {
        ASSERT((this->heap == other.heap), "Comparing oterators from different heaps");
        return Index - other.Index;
    }

    bool DescriptorHeap::Iterator::operator==(const Iterator& other) const
    {
        ASSERT((this->heap == other.heap), "Comparing oterators from different heaps");
        return Index == other.Index;
    }

    bool DescriptorHeap::Iterator::operator!=(const Iterator& other) const
    {
        ASSERT((this->heap == other.heap), "Comparing oterators from different heaps");
        return Index != other.Index;
    }

    bool DescriptorHeap::Iterator::operator<(const Iterator& other) const
    {
        ASSERT((this->heap == other.heap), "Comparing oterators from different heaps");
        return Index < other.Index;
    }

    bool DescriptorHeap::Iterator::operator>(const Iterator& other) const
    {
        ASSERT((this->heap == other.heap), "Comparing oterators from different heaps");
        return Index > other.Index;
    }

    bool DescriptorHeap::Iterator::operator<=(const Iterator& other) const
    {
        ASSERT((this->heap == other.heap), "Comparing oterators from different heaps");
        return Index <= other.Index;
    }

    bool DescriptorHeap::Iterator::operator>=(const Iterator& other) const
    {
        ASSERT((this->heap == other.heap), "Comparing oterators from different heaps");
        return Index >= other.Index;
    }

    // DescriptorHeap methods
    void DescriptorHeap::Create(ID3D12Device5Ptr device, uint32_t count, D3D12_DESCRIPTOR_HEAP_TYPE type, bool isShaderVisible)
    {
        NumDescriptors = count;
        IncrementSize = device->GetDescriptorHandleIncrementSize(type);

        Description.NumDescriptors = count;
        Description.Type = type;
        Description.Flags = isShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE :
            D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        GRAPHICS_ASSERT(device->CreateDescriptorHeap(&Description, IID_PPV_ARGS(&Heap)));
    }

    void DescriptorHeap::Bind(ID3D12GraphicsCommandList4Ptr cmdList, uint32_t slot) const
    {
        std::array<ID3D12DescriptorHeap*, 1> heaps = { Heap.Get() };
        cmdList->SetDescriptorHeaps(heaps.size(), heaps.data());
        cmdList->SetGraphicsRootDescriptorTable(slot, begin()->GPUHandle);
    }

    void DescriptorHeap::BindCompute(ID3D12GraphicsCommandList4Ptr cmdList, uint32_t slot) const
    {
        std::array<ID3D12DescriptorHeap*, 1> heaps = { Heap.Get() };
        cmdList->SetDescriptorHeaps(heaps.size(), heaps.data());
        cmdList->SetComputeRootDescriptorTable(slot, begin()->GPUHandle);
    }

    DescriptorHeap::Iterator DescriptorHeap::begin() const
    {
        return Iterator{ this, 0 };
    }

    DescriptorHeap::Iterator DescriptorHeap::end() const
    {
        return Iterator{ this, NumDescriptors };
    }

    DescriptorHandle DescriptorHeap::operator[](uint32_t index) const
    {
        ASSERT(index < NumDescriptors, "DescriptorHeap index out of range");

        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = Heap->GetCPUDescriptorHandleForHeapStart();
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = Heap->GetGPUDescriptorHandleForHeapStart();
        cpuHandle.ptr += index * IncrementSize;
        gpuHandle.ptr += index * IncrementSize;
        return { this, cpuHandle, gpuHandle };
    }
}