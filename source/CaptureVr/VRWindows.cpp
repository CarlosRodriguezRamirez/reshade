#include <string>
#include <vector>
#include "runtime.hpp"
#include "captureVR.h"
#include "log.hpp"
#include <thread>         // std::thread
#include <iostream>       // std::cout
#include<mutex>
#include<wtypes.h>
// Moved here stuff for widows initialization

#define SCREEN_WIDTH 300
#define SCREEN_HEIGHT 300

extern HMODULE g_module_handle;

namespace VRAdapter
{
	HWND m_hWnd = 0;  // The companion windows
	HDC m_hDC = 0; // The companion hdc


std::string iGetLastErrorText (DWORD nErrorCode)
{
	char* msg = nullptr;
	// Ask Windows to prepare a standard message for a GetLastError() code:
	DWORD len = FormatMessageA (FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, nErrorCode, MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&msg, 0, NULL);

	// Empty message, exit with unknown.
	if (!msg)
		return("Unknown error");

	// Copy buffer into the result string.
	std::string result = msg;

	// Because we used FORMAT_MESSAGE_ALLOCATE_BUFFER for our buffer, now wee need to release it.
	LocalFree (msg);

	return result;
}
std::string GetLastErrorStr (void)
{
	return iGetLastErrorText (GetLastError ());
}

LRESULT CALLBACK WindowProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_DESTROY)
	{
		PostQuitMessage (0);
		return 0;
	}

	return DefWindowProc (hWnd, message, wParam, lParam);
}

//- Create a dummy windows that we will use for gl initialization and texture resolved
void CreateCompanion ()
{
	WNDCLASSEX wc;

	ZeroMemory (&wc, sizeof (WNDCLASSEX));

	wc.cbSize = sizeof (WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = g_module_handle;//hInstance;
	wc.hCursor = LoadCursor (NULL, IDC_ARROW);
	wc.lpszClassName = L"WindowClass";

	RegisterClassEx (&wc);

	m_hWnd = CreateWindowEx (NULL, L"WindowClass", L"GL - Shared Resource",
		WS_OVERLAPPEDWINDOW, SCREEN_WIDTH + 20, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
		NULL, NULL, g_module_handle, NULL);

	std::string info = GetLastErrorStr ();
	ShowWindow (m_hWnd, SW_HIDE);


	m_hDC = GetDC (m_hWnd);
}
} // End namespace
