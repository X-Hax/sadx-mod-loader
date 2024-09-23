#include "stdafx.h"
#include <SADXModLoader.h>
#include "dwmapi.h"

typedef enum {
	DWMWCP_DEFAULT = 0,
	DWMWCP_DONOTROUND = 1,
	DWMWCP_ROUND = 2,
	DWMWCP_ROUNDSMALL = 3
} DWM_WINDOW_CORNER_PREFERENCE;

HRESULT DwmSetWindowAttributeWrapper(HWND handle, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute)
{
	typedef HRESULT(WINAPI* DwmSetWindowAttributePtr)(HWND, DWORD, LPCVOID, DWORD);
	DwmSetWindowAttributePtr DwmSetWindowAttribute_Original;
	HMODULE dwmapiModule = GetModuleHandle(L"dwmapi.dll");
	if (!dwmapiModule)
		return 0;
	DwmSetWindowAttribute_Original = (DwmSetWindowAttributePtr)GetProcAddress(dwmapiModule, "DwmSetWindowAttribute");
	if (!DwmSetWindowAttribute_Original)
		return 0;
	return DwmSetWindowAttribute_Original(handle, dwAttribute, pvAttribute, cbAttribute);
}

static DWM_WINDOW_CORNER_PREFERENCE pref = DWMWCP_DONOTROUND;

void DisableRoundedCorners(HWND handle)
{
	HRESULT result = DwmSetWindowAttributeWrapper(handle, 33, &pref, sizeof(DWM_WINDOW_CORNER_PREFERENCE));
	if (result != 0)
		PrintDebug("Disabling rounded corners: HRESULT %X\n", result);
}