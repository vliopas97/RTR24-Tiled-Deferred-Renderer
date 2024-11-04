#include "Application.h"

#include <cassert>

Application* Application::Instance = nullptr;

Timer::Timer()
{
	Start = std::chrono::steady_clock::now();
}

float Timer::Get()
{
	return std::chrono::duration<float>(std::chrono::steady_clock::now() - Start).count();
}

float Timer::GetAndReset()
{
	auto temp = Start;
	Start = std::chrono::steady_clock::now();
	return  std::chrono::duration<float>(std::chrono::steady_clock::now() - temp).count();
}

Application& Application::GetApp()
{
	assert(Instance && "App instance not initialized");
	return *Instance;
}

int Application::Run()
{
	while (true)
	{
		if (const auto ecode = Window::ProcessMessages())
			return *ecode;
		Tick();
	}
}

void Application::Init(int width, int height, HINSTANCE instance, const char* title)
{
	Instance = new Application(width, height, instance, title);
}

void Application::Shutdown()
{
	assert(Instance && "App instance already deleted");
	delete Instance;
}

void Application::Tick()
{
	float delta = Benchmarker.GetAndReset();
	GraphicsInterface->Tick(delta);
}

Application::Application(int width, int height, HINSTANCE instance, const char* title)
	:MainWindow(MakeUnique<Window>(width, height, instance, title)),
	GraphicsInterface(MakeUnique<Graphics>(*MainWindow))
{
	assert(!Instance && "App instance already initialized");
	Instance = this;
	auto cursor = LoadCursor(nullptr, IDC_ARROW);
}