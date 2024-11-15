#pragma once

#include "Window/Window.h"
#include <chrono>

struct Timer
{
	Timer();
	virtual ~Timer() = default;

	float Get();
	float GetAndReset();
protected:
	std::chrono::steady_clock::time_point Start;
};

#include "Rendering/Graphics.h"

class Application
{
public:
	static Application& GetApp();
	int Run();
	static void Init(int width, int height, HINSTANCE instance, const char* title);
	static void Shutdown();

	const UniquePtr<Window>& GetWindow() { return MainWindow; }
	void OnEvent(Event& e);

private:
	Application(int width, int height, HINSTANCE instance, const char* title);
	~Application();
	Application(const Application&) = delete;
	Application& operator=(const Application&) = delete;

	void Tick();

private:
	UniquePtr<Window> MainWindow;
	UniquePtr<Graphics> GraphicsInterface;

	static Application* Instance;
	Timer Benchmarker;
};


