#pragma region Includes
#include "stdafx.h"

#include <cassert>

#include <deque>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include <sstream>
#include "direct3d.h"
#include "bgscale.h"
#include "hudscale.h"
#include "testspawn.h"
#include "json.hpp"
#include "config.h"
#include "UsercallFunctionHandler.h"
#include "video.h"
#include "DisableRoundedCorners.h"

using std::deque;
using std::ifstream;
using std::string;
using std::wstring;
using std::unique_ptr;
using std::vector;

// Win32 headers.
#include <DbgHelp.h>
#include <Shlwapi.h>
#include <GdiPlus.h>
#include "resource.h"

#include "FixFOV.h"
#include "window.h"
#include "SADXModLoader.h"
#pragma endregion

#pragma region Structs
struct windowsize
{
	int x;
	int y;
	int width;
	int height;
};

struct windowdata
{
	int x;
	int y;
	int width;
	int height;
	DWORD style;
	DWORD exStyle;
};
#pragma endregion

#pragma region Enums
enum windowmodes { windowed, fullscreen };

enum screenmodes { window_mode, fullscreen_mode, borderless_mode, custom_mode };
#pragma endregion

#pragma region Variables
// Settings
static unsigned int screenNum = 1;
static bool borderlessWindow = false;
static bool scaleScreen = true;
static bool customWindowSize = false;
static int customWindowWidth = 640;
static int customWindowHeight = 480;
static bool windowResize = false;
static bool textureFilter = true;
static bool pauseWhenInactive;
static bool showMouse = false;
static screenmodes screenMode;

// Used for borderless windowed mode.
// Defines the size of the inner-window on which the game is rendered.
static windowsize innerSizes[2] = {};

// Used for borderless windowed mode.
// Defines the size of the outer-window which wraps the game window and draws the background.
static windowdata outerSizes[] = {
	{ CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, WS_CAPTION | WS_SYSMENU | WS_VISIBLE, 0 }, // windowed
	{ 0, 0, CW_USEDEFAULT, CW_USEDEFAULT, WS_POPUP | WS_VISIBLE, WS_EX_APPWINDOW } // fullscreen
};

static HWND parentWindow = nullptr;
static int windowMode = windowmodes::windowed;
static HACCEL accelTable = nullptr;

static vector<RECT> screenBounds;
static Gdiplus::Bitmap* backgroundImage = nullptr;
static bool switchingWindowMode = false;

static const uint8_t wndpatch[] = { 0xA1, 0x30, 0xFD, 0xD0, 0x03, 0xEB, 0x08 }; // mov eax,[hWnd] / jmp short 0xf
static int currentScreenSize[2];

static RECT   last_rect = {};
static Uint32 last_width = 0;
static Uint32 last_height = 0;
static DWORD  last_style = 0;
static DWORD  last_exStyle = 0;

const auto loc_794566 = (void*)0x00794566;
#pragma endregion

static void enable_fullscreen_mode(HWND handle)
{
	IsWindowed = false;
	last_width = HorizontalResolution;
	last_height = VerticalResolution;

	GetWindowRect(handle, &last_rect);

	last_style = GetWindowLongA(handle, GWL_STYLE);
	last_exStyle = GetWindowLongA(handle, GWL_EXSTYLE);

	SetWindowLongA(handle, GWL_STYLE, WS_POPUP | WS_SYSMENU | WS_VISIBLE);

	while (ShowCursor(FALSE) > 0);
}

static void enable_windowed_mode(HWND handle)
{
	SetWindowLongA(handle, GWL_STYLE, last_style);
	SetWindowLongA(handle, GWL_EXSTYLE, last_exStyle);

	auto width = last_rect.right - last_rect.left;
	auto height = last_rect.bottom - last_rect.top;

	if (width <= 0 || height <= 0)
	{
		last_rect = {};

		last_rect.right = 640;
		last_rect.bottom = 480;

		AdjustWindowRectEx(&last_rect, last_style, false, last_exStyle);

		width = last_rect.right - last_rect.left;
		height = last_rect.bottom - last_rect.top;
	}

	SetWindowPos(handle, HWND_NOTOPMOST, last_rect.left, last_rect.top, width, height, 0);
	IsWindowed = true;

	while (ShowCursor(TRUE) < 0);
}

static void ResumeAllSoundsPause()
{
	if (!IsGamePaused())
	{
		UnpauseAllSounds(0);
	}
	else
		ResumeMusic();
}

static void PauseAllSoundsAndMusic()
{
	PauseAllSounds(0);
}

static BOOL CALLBACK GetMonitorSize(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
	screenBounds.push_back(*lprcMonitor);
	return TRUE;
}


// WndProc wrapper for Borderless mode
static LRESULT CALLBACK WrapperWndProc(HWND wrapper, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_ACTIVATE:
		if ((LOWORD(wParam) != WA_INACTIVE && !lParam) || reinterpret_cast<HWND>(lParam) == WindowHandle)
		{
			SetFocus(WindowHandle);
			return 0;
		}
		break;

	case WM_CLOSE:
		// As this sends the same message to the main window, there's no need to call OnExit here.
		SendMessageA(WindowHandle, WM_CLOSE, wParam, lParam);
		return 0;

	case WM_ERASEBKGND:
	{
		if (backgroundImage == nullptr || windowResize)
		{
			break;
		}

		Gdiplus::Graphics gfx((HDC)wParam);
		gfx.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);

		RECT rect;
		GetClientRect(wrapper, &rect);

		auto w = rect.right - rect.left;
		auto h = rect.bottom - rect.top;

		if (w == innerSizes[windowMode].width && h == innerSizes[windowMode].height)
		{
			break;
		}

		gfx.DrawImage(backgroundImage, 0, 0, w, h);
		return 0;
	}

	case WM_SIZE:
	{
		auto& inner = innerSizes[windowMode];

		if (windowResize)
		{
			inner.x = 0;
			inner.y = 0;
			inner.width = LOWORD(lParam);
			inner.height = HIWORD(lParam);
		}

		// update the inner window (game view)
		SetWindowPos(WindowHandle, HWND_TOP, inner.x, inner.y, inner.width, inner.height, 0);
		break;
	}

	case WM_COMMAND:
	{
		if (wParam != MAKELONG(ID_FULLSCREEN, 1))
			break;

		switchingWindowMode = true;

		if (windowMode == windowed)
		{
			RECT rect;
			GetWindowRect(wrapper, &rect);

			outerSizes[windowMode].x = rect.left;
			outerSizes[windowMode].y = rect.top;
			outerSizes[windowMode].width = rect.right - rect.left;
			outerSizes[windowMode].height = rect.bottom - rect.top;
			outerSizes[windowMode].style = GetWindowLongA(parentWindow, GWL_STYLE);
			outerSizes[windowMode].exStyle = GetWindowLongA(parentWindow, GWL_EXSTYLE);
		}

		windowMode = windowMode == windowed ? fullscreen : windowed;

		// update outer window (draws background)
		const auto& outer = outerSizes[windowMode];
		SetWindowLongA(parentWindow, GWL_STYLE, outer.style);
		SetWindowLongA(parentWindow, GWL_EXSTYLE, outer.exStyle);
		SetWindowPos(parentWindow, HWND_NOTOPMOST, outer.x, outer.y, outer.width, outer.height, SWP_FRAMECHANGED);

		switchingWindowMode = false;
		return 0;
	}

	case WM_ACTIVATEAPP:
		if (!switchingWindowMode)
		{
			if (pauseWhenInactive)
			{
				if (wParam)
				{
					if (!IsGamePaused())
					{
						UnpauseAllSounds(0);
					}
					else
						ResumeMusic();
				}
				else
					PauseAllSounds(0);
			}
			WndProc_B(WindowHandle, uMsg, wParam, lParam);
		}

		//if (screenMode == screenmodes::fullscreen_mode)
		//{
		//	ClipCursor(&screenBounds[0]);
		//}

		if (windowMode == windowed || showMouse)
		{
			while (ShowCursor(TRUE) < 0);
		}
		else
		{
			while (ShowCursor(FALSE) > 0);
		}

	default:
		break;
	}

	// alternatively we can return SendMe
	return DefWindowProcA(wrapper, uMsg, wParam, lParam);
}

// This is a single WndProc used for both resizable and non-resizable windows
static LRESULT CALLBACK WndProc_New(HWND handle, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch (Msg)
	{	

	default:
		break;

	case WM_DESTROY:
	case WM_CLOSE:
		OnExit(0, 0, 0);
		PostQuitMessage(0);
		break;

	// Request to change window size: only applicable for resizable window
	case WM_SIZE:
	{
		if (!windowResize || customWindowSize)
		{
			break;
		}

		if (!IsWindowed || Direct3D_Device == nullptr)
		{
			return 0;
		}

		int w = LOWORD(lParam);
		int h = HIWORD(lParam);

		if (!w || !h)
		{
			break;
		}

		direct3d::change_resolution(w, h);
		break;
	}

	// Switching to/from the window, only applicable to exclusive fullscreen
	case WM_ACTIVATE:
	{
		if (screenMode != fullscreen_mode)
			return 0;
			// Alt+TAB from exclusive fullscreen to window
		if (wParam == WA_INACTIVE && !IsWindowed)
		{
			direct3d::change_resolution(last_width, last_height, true);
			enable_windowed_mode(handle);
			SetWindowLongA(handle, GWL_STYLE, WS_CAPTION | WS_SYSMENU | WS_VISIBLE);
			const auto& rect = screenBounds[screenNum == 0 ? 0 : screenNum - 1];
			const auto w = rect.right - rect.left;
			const auto h = rect.bottom - rect.top;
			SetWindowPos(handle, HWND_NOTOPMOST, rect.left+w/ 2-last_width/2, rect.top+h/2-last_height/2, last_width, last_height, 0);
			return 0;
		}
		// Alt+TAB from window to exclusive fullscreen
		else if (wParam != WA_INACTIVE && !switchingWindowMode) // In this WndProc, switchingWindowMode is on when Alt+Enter is used
		{
			direct3d::change_resolution(last_width, last_height, false);
			enable_fullscreen_mode(handle);
			return 0;
		}
		break;
	}

	case WM_ACTIVATEAPP:
	{
		if (pauseWhenInactive)
		{
			if (wParam == FALSE)
			{
				PauseVideo();
			}
			else
			{
				ResumeVideo();
			}
		}
		break;
	}

	// Alt+Enter for exclusive and borderless
	case WM_COMMAND:
	{
		if (wParam != MAKELONG(ID_FULLSCREEN, 1))
		{
			break;
		}

		// Alt+Enter from window to full screen
		if (direct3d::is_windowed() && IsWindowed)
		{
			enable_fullscreen_mode(handle);

			// Only change render resolution to fit the screen in resizable mode
			if (windowResize)
			{
				const auto& rect = screenBounds[screenNum == 0 ? 0 : screenNum - 1];

				const auto w = rect.right - rect.left;
				const auto h = rect.bottom - rect.top;
				direct3d::change_resolution(w, h, false);
			}
			else
				direct3d::change_resolution(last_width, last_height, false);
			
			switchingWindowMode = false;

		}
		// Alt+Enter from fullscreen to window
		else
		{
			direct3d::change_resolution(last_width, last_height, true);
			enable_windowed_mode(handle);
			LONG attrs = WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU | WS_VISIBLE;
			if (windowResize) 
				attrs |= WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SIZEBOX;
			SetWindowLongA(handle, GWL_STYLE, attrs);
			const auto& rect = screenBounds[screenNum == 0 ? 0 : screenNum - 1];
			const auto w = rect.right - rect.left;
			const auto h = rect.bottom - rect.top;
			SetWindowPos(handle, HWND_NOTOPMOST, rect.left + w / 2 - last_width / 2, rect.top + h / 2 - last_height / 2, last_width, last_height, 0);
			switchingWindowMode = true; // This is necessary to distinguish between Alt+Tab and Alt+Enter in exclusive mode
		}

		return 0;
	}
	}

	// The distinction between DefWindowProcA and WndProc must be preserved here, not sure about the exact differences
	return windowResize ? DefWindowProcA(handle, Msg, wParam, lParam) : WndProc(handle, Msg, wParam, lParam);
}

LRESULT __stdcall WndProc_hook(HWND handle, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch (Msg)
	{
	case WM_ACTIVATEAPP:
		if (pauseWhenInactive)
		{
			if (wParam == FALSE)
			{
				PauseVideo();
			}
			else
			{
				ResumeVideo();
			}
		}
		break;
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
		if (wParam != VK_F4 && wParam != VK_F2 && wParam != VK_RETURN)
			return 0;
		break;

	default:
		break;
	}
	
	return DefWindowProcA(handle, Msg, wParam, lParam);
}

static void __cdecl HandleWindowMessages_r()
{
	MSG Msg; // [sp+4h] [bp-1Ch]@1

	if (PeekMessageA(&Msg, nullptr, 0, 0, 1u))
	{
		do
		{
			if (!TranslateAccelerator(parentWindow, accelTable, &Msg))
			{
				TranslateMessage(&Msg);
				DispatchMessageA(&Msg);
			}
		} while (PeekMessageA(&Msg, nullptr, 0, 0, 1u));
		wParam = Msg.wParam;
	}
	else
	{
		wParam = Msg.wParam;
	}
}

static void CreateSADXWindow_r(HINSTANCE hInstance, int nCmdShow)
{
	// Primary window class name.
	const char* const lpszClassName = GetWindowClassName();

	// Hook default return of SADX's window procedure to force it to return DefWindowProc
	WriteJump(reinterpret_cast<void*>(0x00789E48), WndProc_hook);

	// Primary window class for SADX.
	WNDCLASSA window{}; // [sp+4h] [bp-28h]@1

	window.style = 0;
	window.lpfnWndProc = WndProc_New;
	window.cbClsExtra = 0;
	window.cbWndExtra = 0;
	window.hInstance = hInstance;
	window.hIcon = LoadIconA(hInstance, MAKEINTRESOURCEA(101));
	window.hCursor = LoadCursor(nullptr, IDC_ARROW);
	window.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	window.lpszMenuName = nullptr;
	window.lpszClassName = lpszClassName;

	if (!RegisterClassA(&window))
	{
		MessageBox(nullptr, L"Failed to register class A to create SADX Window, game won't work.", L"Failed to create SADX Window", MB_OK | MB_ICONERROR);
		return;
	}

	RECT windowRect;

	windowRect.top = 0;
	windowRect.left = 0;

	if (customWindowSize)
	{
		windowRect.right = customWindowWidth;
		windowRect.bottom = customWindowHeight;
	}
	else
	{
		windowRect.right = HorizontalResolution;
		windowRect.bottom = VerticalResolution;
	}

	if (borderlessWindow || IsWindowed)
	{
		currentScreenSize[0] = GetSystemMetrics(SM_CXSCREEN);
		currentScreenSize[1] = GetSystemMetrics(SM_CYSCREEN);
		WriteData((int**)0x79426E, &currentScreenSize[0]);
		WriteData((int**)0x79427A, &currentScreenSize[1]);
	}

	EnumDisplayMonitors(nullptr, nullptr, GetMonitorSize, 0);

	int screenX, screenY, screenW, screenH, wsX, wsY, wsW, wsH;
	if (screenNum > 0)
	{
		if (screenBounds.size() < screenNum)
		{
			screenNum = 1;
		}

		RECT screenSize = screenBounds[screenNum - 1];

		wsX = screenX = screenSize.left;
		wsY = screenY = screenSize.top;
		wsW = screenW = screenSize.right - screenSize.left;
		wsH = screenH = screenSize.bottom - screenSize.top;
	}
	else
	{
		screenX = GetSystemMetrics(SM_XVIRTUALSCREEN);
		screenY = GetSystemMetrics(SM_YVIRTUALSCREEN);
		screenW = GetSystemMetrics(SM_CXVIRTUALSCREEN);
		screenH = GetSystemMetrics(SM_CYVIRTUALSCREEN);

		wsX = 0;
		wsY = 0;
		wsW = GetSystemMetrics(SM_CXSCREEN);
		wsH = GetSystemMetrics(SM_CYSCREEN);
	}

	accelTable = LoadAcceleratorsA(g_hinstDll, MAKEINTRESOURCEA(IDR_ACCEL_WRAPPER_WINDOW));

	if (screenMode != fullscreen_mode)
	{
		PrintDebug("Creating SADX Window in borderless mode...\n");

		if (windowResize)
		{
			outerSizes[windowed].style |= WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SIZEBOX;
		}

		AdjustWindowRectEx(&windowRect, outerSizes[windowed].style, false, 0);

		outerSizes[windowed].width = windowRect.right - windowRect.left;
		outerSizes[windowed].height = windowRect.bottom - windowRect.top;

		outerSizes[windowed].x = wsX + ((wsW - outerSizes[windowed].width) / 2);
		outerSizes[windowed].y = wsY + ((wsH - outerSizes[windowed].height) / 2);

		outerSizes[fullscreen].x = screenX;
		outerSizes[fullscreen].y = screenY;
		outerSizes[fullscreen].width = screenW;
		outerSizes[fullscreen].height = screenH;

		if (customWindowSize)
		{
			float num = min((float)customWindowWidth / (float)HorizontalResolution, (float)customWindowHeight / (float)VerticalResolution);

			innerSizes[windowed].width = (int)((float)HorizontalResolution * num);
			innerSizes[windowed].height = (int)((float)VerticalResolution * num);
			innerSizes[windowed].x = (customWindowWidth - innerSizes[windowed].width) / 2;
			innerSizes[windowed].y = (customWindowHeight - innerSizes[windowed].height) / 2;
		}
		else
		{
			innerSizes[windowed].width = HorizontalResolution;
			innerSizes[windowed].height = VerticalResolution;
			innerSizes[windowed].x = 0;
			innerSizes[windowed].y = 0;
		}

		if (scaleScreen)
		{
			float num = min((float)screenW / (float)HorizontalResolution, (float)screenH / (float)VerticalResolution);

			innerSizes[fullscreen].width = (int)((float)HorizontalResolution * num);
			innerSizes[fullscreen].height = (int)((float)VerticalResolution * num);
		}
		else
		{
			innerSizes[fullscreen].width = HorizontalResolution;
			innerSizes[fullscreen].height = VerticalResolution;
		}

		innerSizes[fullscreen].x = (screenW - innerSizes[fullscreen].width) / 2;
		innerSizes[fullscreen].y = (screenH - innerSizes[fullscreen].height) / 2;

		windowMode = IsWindowed ? windowed : fullscreen;

		// Register a window class for the wrapper window.
		WNDCLASSA wrapper;

		wrapper.style = 0;
		wrapper.lpfnWndProc = WrapperWndProc;
		wrapper.cbClsExtra = 0;
		wrapper.cbWndExtra = 0;
		wrapper.hInstance = hInstance;
		if (iconPathName.empty())
			wrapper.hIcon = LoadIconA(hInstance, MAKEINTRESOURCEA(101));
		else
		{
			HICON iconHandle = (HICON)LoadImage(NULL, iconPathName.c_str(), IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
			wrapper.hIcon = iconHandle; // Window
			HWND hConsole = GetConsoleWindow();
			SendMessage(hConsole, WM_SETICON, ICON_SMALL, (LPARAM)iconHandle); // Console
			SendMessage(hConsole, WM_SETICON, ICON_BIG, (LPARAM)iconHandle); // Console
		}
		wrapper.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wrapper.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
		wrapper.lpszMenuName = nullptr;
		wrapper.lpszClassName = "WrapperWindow";

		if (!RegisterClassA(&wrapper))
		{
			MessageBox(nullptr, L"Failed to register class A to create SADX Window with Borderless settings, game won't work.", L"Failed to create SADX Window", MB_OK | MB_ICONERROR);
			return;
		}

		// Parent Window Setup
		const auto& outerSize = outerSizes[windowMode];
		parentWindow = CreateWindowExA(
			outerSize.exStyle,					// Extended Style
			"WrapperWindow",					// Class
			lpszClassName,						// Name
			outerSize.style,					// Style
			outerSize.x, outerSize.y,			// Position
			outerSize.width, outerSize.height,	// Size
			nullptr,							// Parent Window (This is the parent window)
			nullptr,							// Menu Handle (Does not need one)
			hInstance,							// Instance Handle
			nullptr								// No Other Parameters
		);

		if (parentWindow == nullptr)
		{
			MessageBox(nullptr, L"Failed to Create parentWindow with Borderless settings, game won't work.", L"Failed to create SADX Window", MB_OK | MB_ICONERROR);
			return;
		}

		DisableRoundedCorners(parentWindow);

		// Game Render Window Setup
		const auto& innerSize = innerSizes[windowMode];
		WindowHandle = CreateWindowExA(
			0,									// Extended Style (Doesn't need one).
			lpszClassName,						// Window Class
			lpszClassName,						// Name
			WS_CHILD | WS_VISIBLE,				// Style (Child Winodw and Visible)
			innerSize.x, innerSize.y,			// Position
			innerSize.width, innerSize.height,	// Size
			parentWindow,						// Parent Window
			nullptr,							// Menu Handle (Doesn't need one).
			hInstance,							// Instance Handle
			nullptr								// No Other Paramters
		);

		std::wstring failed = L"Failed to Create SADX Window with Borderless settings, game won't work.";
		if (WindowHandle == nullptr)
		{
			MessageBox(nullptr, failed.c_str(), L"Failed to create SADX Window", MB_OK | MB_ICONERROR);
			return;
		}

		SetFocus(WindowHandle);
		if (ShowWindow(parentWindow, nCmdShow) && UpdateWindow(parentWindow) && SetForegroundWindow(parentWindow))
			PrintDebug("Successfully created SADX Borderless Window: outer %dx%d, inner %dx%d\n", outerSize.width, outerSize.height, innerSize.width, innerSize.height);
		else
			PrintDebug("SADX Borderless Window got created, but a few functions call failed. Warning, the game may crash. Outer %dx%d, inner %dx%d\n", outerSize.width, outerSize.height, innerSize.width, innerSize.height);

		IsWindowed = true;
		WriteData((void*)0x402C61, wndpatch);
	}
	else
	{
		PrintDebug("Creating SADX Window...\n");

		DWORD dwStyle = WS_CAPTION | WS_SYSMENU | WS_VISIBLE;
		DWORD dwExStyle = 0;

		if (windowResize)
		{
			dwStyle |= WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SIZEBOX;
		}

		AdjustWindowRectEx(&windowRect, dwStyle, false, dwExStyle);

		int w = windowRect.right - windowRect.left;
		int h = windowRect.bottom - windowRect.top;
		int x = wsX + ((wsW - w) / 2);
		int y = wsY + ((wsH - h) / 2);

		WindowHandle = CreateWindowExA(
			dwExStyle,
			lpszClassName,
			lpszClassName,
			dwStyle,
			x, y, w, h,
			nullptr, 
			nullptr, 
			hInstance, 
			nullptr
		);

		if (WindowHandle == nullptr)
		{
			MessageBox(nullptr, L"Failed to Create SADX Window, game won't work.", L"Failed to create SADX Window", MB_OK | MB_ICONERROR);
			return;
		}

		if (!IsWindowed)
		{
			enable_fullscreen_mode(WindowHandle);
		}

		if (ShowWindow(WindowHandle, nCmdShow) && UpdateWindow(WindowHandle))
			PrintDebug("Successfully created SADX Window: %dx%d\n", w, h);
		else
			PrintDebug("SADX Window got created, but a few functions call failed. Warning, the game may crash. Res: %dx%d\n", w, h);

		parentWindow = WindowHandle;
		WriteCall((void*)0x00401920, ResumeAllSoundsPause);
		WriteCall((void*)0x00401939, PauseAllSoundsAndMusic);	
	}

	// Hook the window message handler.
	WriteJump((void*)HandleWindowMessages, (void*)HandleWindowMessages_r);
}

static __declspec(naked) void CreateSADXWindow_asm()
{
	__asm
	{
		mov ebx, [esp + 4]
		push ebx
		push eax
		call CreateSADXWindow_r
		add esp, 8
		retn
	}
}

// Patches the window handler and several other graphical options related to the window's settings.
void PatchWindow(const LoaderSettings& settings)
{
	screenMode = (screenmodes)settings.ScreenMode;
	screenNum = settings.ScreenNum;
	windowResize = settings.ResizableWindow;
	pauseWhenInactive = settings.PauseWhenInactive;
	customWindowHeight = settings.WindowHeight;
	customWindowWidth = settings.WindowWidth;
	showMouse = settings.ShowMouseInFullscreen;

	bool dynamicBuffers = true; // Always enabled since the main performance impact has been mitigated

	// If Screen Mode is not window mode, then it needs the below settings otherwise shit just breaks.
	// This whole window handler needs a rewrite.
	switch (screenMode)
	{
	case custom_mode:
		customWindowSize = true;
		borderlessWindow = true;
		scaleScreen = true;
		break;
	case borderless_mode:
		customWindowSize = false;
		borderlessWindow = true;
		scaleScreen = true;
		break;
	case fullscreen_mode:
		customWindowSize = false;
		scaleScreen = true;
		break;
	case window_mode:
	default:
		customWindowSize = false;
		borderlessWindow = true;
		scaleScreen = true;
		break;
	}

	WriteJump((void*)0x789E50, CreateSADXWindow_asm); // override window creation function

	// Custom resolution.
	WriteJump((void*)0x40297A, (void*)0x402A90);

	int hres = settings.HorizontalResolution;
	if (hres > 0)
	{
		HorizontalResolution = hres;
		HorizontalStretch = static_cast<float>(HorizontalResolution) / 640.0f;
	}

	int vres = settings.VerticalResolution;
	if (vres > 0)
	{
		VerticalResolution = vres;
		VerticalStretch = static_cast<float>(VerticalResolution) / 480.0f;
	}

	if (settings.FovFix)
		fov::initialize();

	if (!borderlessWindow)
	{
		vector<uint8_t> nop(5, 0x90);
		WriteData((void*)0x007943D0, nop.data(), nop.size());

		// SADX automatically corrects values greater than the number of adapters available.
		// DisplayAdapter is unsigned, so -1 will be greater than the number of adapters, and it will reset.
		DisplayAdapter = screenNum - 1;
	}

	// Dynamic buffers, needed for window resize and fulscreen alttabbing (causes slowdown)
	if (windowResize || dynamicBuffers)
	{
		// MeshSetBuffer_CreateVertexBuffer: Change D3DPOOL_DEFAULT to D3DPOOL_MANAGED
		WriteData((char*)0x007853F3, (char)D3DPOOL_MANAGED);
		// MeshSetBuffer_CreateVertexBuffer: Remove D3DUSAGE_DYNAMIC
		WriteData((short*)0x007853F6, (short)D3DUSAGE_WRITEONLY);
		PrintDebug("Dynamic buffers initialized\n");
	}

	if (!pauseWhenInactive)
	{
		WriteData((uint8_t*)0x00401914, (uint8_t)0xEBu);
		// Don't pause music and sounds when the window is inactive
		WriteData<5>(reinterpret_cast<void*>(0x00401939), 0x90u);
		WriteData<5>(reinterpret_cast<void*>(0x00401920), 0x90u);
	}
}

void SetBorderImage(std::wstring path)
{
	if (screenMode == fullscreen_mode)
	{
		return;
	}

	if (backgroundImage)
	{
		delete backgroundImage;
		backgroundImage = nullptr;
	}

	if (FileExists(path))
	{
		Gdiplus::GdiplusStartupInput gdiplusStartupInput;
		ULONG_PTR gdiplusToken;
		Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);
		backgroundImage = Gdiplus::Bitmap::FromFile(path.c_str());
	}
}