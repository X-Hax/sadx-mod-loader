#include "stdafx.h"
#include <SADXModLoader.h>

BOOL SetProcessDPIAwareWrapper() {
	typedef BOOL(WINAPI* SetProcessDPIAwarePtr)(VOID);
	SetProcessDPIAwarePtr set_process_dpi_aware_func =
		reinterpret_cast<SetProcessDPIAwarePtr>(
			GetProcAddress(GetModuleHandleA("user32.dll"),
				"SetProcessDPIAware"));
	return set_process_dpi_aware_func &&
		set_process_dpi_aware_func();
}

void DPIFix_Init()
{
	SetProcessDPIAwareWrapper();
}