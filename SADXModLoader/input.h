#pragma once
#include "DreamPad.h"
#include <IniFile.hpp>

namespace input
{
	void poll_controllers();
	void WriteAnalogs_hook();
	void InitRawControllers_hook();
	void __cdecl EnableController_r(Uint8 index);
	void __cdecl DisableController_r(Uint8 index);

	extern ControllerData raw_input[GAMEPAD_COUNT];
	extern bool controller_enabled[GAMEPAD_COUNT];
	extern bool debug;
	extern bool disable_mouse;
	extern bool e_held;
	extern bool demo;
	extern KeyboardMapping keys;

	extern ushort keyboard_player_current;
}

extern IniFile* config;
extern bool enabledSmoothCam;

void SDL2_Init(std::wstring extLibPath);
void SDL2_OnExit();
void SDL2_OnInput();