#pragma once
#include "wrl/client.h"

using Microsoft::WRL::ComPtr;

template<typename T>
class ComPtrWrapper
{
public:
	ComPtrWrapper() = default;
	explicit ComPtrWrapper(const ComPtr<T>& other)
		:Ptr(other)
	{}

	ComPtrWrapper(std::nullptr_t) noexcept
		:Ptr(nullptr)
	{}

	operator bool() const noexcept { return Ptr != nullptr; }
	T& operator*() const noexcept { return *Ptr.Get(); }
	T** operator&() noexcept { return &Ptr; }
	T* const* operator&() const noexcept { return &Ptr; }
	operator T* () const noexcept { return Ptr.Get(); }

    T* operator->() const noexcept { return Ptr.Get(); }
	T* Get() const noexcept { return Ptr.Get(); }
    T* const* GetAddressOf() const noexcept { return Ptr.GetAddressOf(); }

	T* Detach() noexcept { return Ptr.Detach(); }

protected:
	ComPtr<T> Ptr;
};