#include "window.hpp"
#include <iostream>

int Window::width = 0;
int Window::height = 0;

Window::Window(HINSTANCE hInstance) : m_hWindow(NULL)
{
	LPCSTR CLASS_NAME = "Sample Window Class";
	LPCSTR WINDOW_NAME = "TimeToDX";
	m_windClass = {};
	m_windClass.lpfnWndProc = Window::WindowProc;
	m_windClass.hInstance = hInstance;
	m_windClass.lpszClassName = CLASS_NAME;
	m_windClass.style = CS_HREDRAW | CS_VREDRAW;
	m_windClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	m_windClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

	RegisterClass(&m_windClass);

	m_hWindow = CreateWindowEx(
		0,
		CLASS_NAME,
		WINDOW_NAME,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		NULL,
		NULL,
		hInstance,
		NULL
	);

	if (m_hWindow != NULL)
	{
		ShowWindow(m_hWindow, SW_SHOWDEFAULT);
	}
}

HWND& Window::getHandle()
{
	return m_hWindow;
}


LRESULT CALLBACK Window::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_DESTROY:
	{
		PostQuitMessage(0);
		break;
	}
	case WM_SIZE:
	{
		width = LOWORD(lParam) % 2 == 0 ? LOWORD(lParam) : LOWORD(lParam) + 1;
		height = HIWORD(lParam) % 2 == 0 ? HIWORD(lParam) : HIWORD(lParam) + 1;
		break;
	}

	case WM_PAINT:
	{
		PAINTSTRUCT paintStruct = {};
		HDC hdc = BeginPaint(hwnd, &paintStruct);
		EndPaint(hwnd, &paintStruct);
		break;
	}
	case WM_CLOSE:
	{
		if (MessageBoxW(hwnd, L"Really quit?", L"My application", MB_OKCANCEL) == IDOK)
		{
			DestroyWindow(hwnd);
		}
		// Else: User canceled. Do nothing.
		break;
	}
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}