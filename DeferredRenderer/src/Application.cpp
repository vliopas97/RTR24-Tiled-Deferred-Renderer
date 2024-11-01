#include "Application.h"

#include <cassert>

Application* Application::Instance = nullptr;

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
	GraphicsInterface->Tick();
}

Application::Application(int width, int height, HINSTANCE instance, const char* title)
	:MainWindow(MakeUnique<Window>(width, height, instance, title)),
	GraphicsInterface(MakeUnique<Graphics>(*MainWindow))
{
	assert(!Instance && "App instance already initialized");
	Instance = this;
	auto cursor = LoadCursor(nullptr, IDC_ARROW);
}