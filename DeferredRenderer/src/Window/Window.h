#pragma once

#include "Core/Core.h"

#include <optional>

struct Window
{
	Window(int width, int height, HINSTANCE& instance, const char* title);

	static std::optional<int> ProcessMessages();
	HWND Handle;

	int Width, Height;
};


