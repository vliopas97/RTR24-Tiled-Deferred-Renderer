#pragma once

#include "Window/Window.h"
#include <memory>

#include "Rendering/Graphics.h"

class Application
{
public:
	static Application& GetApp();
	int Run();
	static void Init(int width, int height, HINSTANCE instance, const char* title);

private:
	Application(int width, int height, HINSTANCE instance, const char* title);
	Application(const Application&) = delete;
	Application& operator=(const Application&) = delete;

	void Tick();

private:
	UniquePtr<Window> MainWindow;
	UniquePtr<Graphics> GraphicsInterface;

	static Application* Instance;
};


