#pragma once
#include "Core/Core.h"
#include "dxDescriptorHandle.h"

namespace DX
{
	struct DescriptorHeap
	{
        class Iterator
        {
        public:
            using IteratorCategory = std::random_access_iterator_tag;
            using DifferenceType = std::ptrdiff_t;

            Iterator(const DescriptorHeap* heap, uint32_t index);

            // Dereference operator
            DescriptorHandle operator*() const;
            SharedPtr<DescriptorHandle> operator->() const;

            // Increment operators
            Iterator& operator++();
            Iterator operator++(int);

            // Decrement operators
            Iterator& operator--();
            Iterator operator--(int);

            // Random access operators
            Iterator operator+(DifferenceType n) const;
            Iterator operator-(DifferenceType n) const;
            DifferenceType operator-(const Iterator& other) const;

            // Comparison operators
            bool operator==(const Iterator& other) const;
            bool operator!=(const Iterator& other) const;
            bool operator<(const Iterator& other) const;
            bool operator>(const Iterator& other) const;
            bool operator<=(const Iterator& other) const;
            bool operator>=(const Iterator& other) const;

        private:
            const DescriptorHeap* heap;
            uint32_t Index;
        };

        DescriptorHeap() = default;
        void Create(ID3D12Device5Ptr device, uint32_t count, D3D12_DESCRIPTOR_HEAP_TYPE type, bool isShaderVisible);

        void Bind(ID3D12GraphicsCommandList4Ptr cmdList, uint32_t slot) const;
        void BindCompute(ID3D12GraphicsCommandList4Ptr cmdList, uint32_t slot) const;

        uint32_t GetNumDescriptors() const { return NumDescriptors; }
        D3D12_DESCRIPTOR_HEAP_DESC GetDesc() const { return Description;}

        Iterator begin() const;
        Iterator end() const;
        DescriptorHandle operator[](uint32_t index) const;

		ID3D12DescriptorHeapPtr Heap;
	private:
		uint32_t NumDescriptors = 0;
		uint32_t IncrementSize = 0;
        D3D12_DESCRIPTOR_HEAP_DESC Description{};
	};
}