#pragma once
#include <windows.h>
#include <d3d11.h>
class Window
{
public:
	Window(HINSTANCE hInstance);
	~Window() = default;

	HWND& getHandle();
	static int width;
	static int height;


private:
	HWND m_hWindow;
	WNDCLASS m_windClass;
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};