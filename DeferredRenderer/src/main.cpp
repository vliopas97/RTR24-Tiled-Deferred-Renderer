#include "Window/Window.h"

#include "Application.h"
#include "Core/Exception.h"

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
	EXCEPTION_WRAP(
		Application::Init(1920, 1080, GetModuleHandle(nullptr), "RTR 2024 Deferred Renderer");
		);

	EXCEPTION_WRAP(Application::InitGraphics(););

	int msg{ 0 };
	EXCEPTION_WRAP(
		auto & application = Application::GetApp();
		msg = application.Run();
		);

	Application::Shutdown();
	return msg;
}