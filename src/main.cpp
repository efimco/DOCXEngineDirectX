#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include "renderer.hpp"
#include "window.hpp"

// No manifest approach: set process DPI awareness at runtime
// Tries Per-Monitor v2 first, then Per-Monitor, then System-aware as last resort.
static void SetHighDpiAwarenessAtRuntime()
{
	if (const HMODULE user32 = GetModuleHandleW(L"user32.dll"))
	{
		typedef BOOL(WINAPI* SetProcessDpiAwarenessContext_t)(HANDLE);
		const auto pSetProcessDpiAwarenessContext = reinterpret_cast<SetProcessDpiAwarenessContext_t>(
			GetProcAddress(user32, "SetProcessDpiAwarenessContext"));

		// Define constant if headers are older
#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((HANDLE)-4)
#endif

		if (pSetProcessDpiAwarenessContext)
		{
			if (pSetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))
				return; // Success
		}
	}
}

using namespace Microsoft::WRL;

int WINAPI wWinMain(const HINSTANCE hInstance, HINSTANCE, PWSTR, int)
{
// #ifdef _DEBUG
// 	AllocConsole();
// 	FILE* pCout;
// 	freopen_s(&pCout, "CONOUT$", "w", stdout);
// 	freopen_s(&pCout, "CONOUT$", "w", stderr);
// #endif

	// Ensure high-DPI awareness without a manifest, before any window is created
	SetHighDpiAwarenessAtRuntime();

	const Window window(hInstance);
	Renderer renderer(window.getHandle());
	MSG message = {};
	bool running = true;
	while (running)
	{
		if (PeekMessage(&message, NULL, 0, 0, PM_REMOVE))
		{
			if (message.message == WM_QUIT)
			{
				running = false;
				break;
			}

			TranslateMessage(&message);
			DispatchMessage(&message);
		}
		if (running)
		{
			renderer.draw();
		}

	}
	return 0;
}
