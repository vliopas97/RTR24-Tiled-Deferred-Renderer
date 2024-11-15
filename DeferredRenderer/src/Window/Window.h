#pragma once

#include "Core/Core.h"
#include "Events/Input.h"

#include <functional>
#include <optional>

struct Window
{
	using EventCallbackFn = std::function<void(Event&)>;
	Window(int width, int height, HINSTANCE& instance, const char* title);

	static std::optional<int> ProcessMessages();

	inline uint32_t GetWidth() const { return Width; }
	inline uint32_t GetHeight() const { return Height; }
	inline HWND GetHandle() { return Handle; }

	void OnEvent(Event& event);
	void SetEventCallbackFunction(const EventCallbackFn& fn);

	void ShowCursor();
	void HideCursor();
	bool IsCursorVisible() const;

	void Tick(float delta);

private:
	static LRESULT CALLBACK InitializeWindow(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK WindProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT CALLBACK WindProcImpl(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam);

	void ShowCursorImpl();
	void HideCursorImpl();
	void TrapCursor();
	void FreeCursor();

public:
	InputManager Input;

private:
	HWND Handle;
	uint32_t Width, Height;

	EventCallbackFn EventCallback;
	bool CursorVisibility = false;

};


