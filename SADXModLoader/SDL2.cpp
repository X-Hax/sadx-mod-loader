#include "stdafx.h"
#include <Windows.h>
#include "util.h"
#include <direct.h>	// for _getcwd

#include <string>
#include <sstream>	// because

#include "SDL.h"

#include <SADXModLoader.h>
#include <IniFile.hpp>

#include "typedefs.h"
#include "input.h"
#include "rumble.h"
#include "minmax.h"

bool isInputMod = false;
bool enabledSDL = false;
static void* RumbleA_ptr = reinterpret_cast<void*>(0x004BCBC0);
static void* RumbleB_ptr = reinterpret_cast<void*>(0x004BCC10);
static void* UpdateControllers_ptr = reinterpret_cast<void*>(0x0040F460);
static void* AnalogHook_ptr = reinterpret_cast<void*>(0x0040F343);
static void* InitRawControllers_ptr = reinterpret_cast<void*>(0x0040F451); // End of function (hook)

KeyboardInput SADXKeyboard = { 0, 0, { 0, 0, 0, 0, 0, 0 }, nullptr };

PointerInfo SDL2jumps[] = {
	{ rumble::pdVibMxStop, rumble::pdVibMxStop_r },
	{ RumbleA_ptr, rumble::RumbleA_r },
	{ RumbleB_ptr, rumble::RumbleB_r },
	{ AnalogHook_ptr, input::WriteAnalogs_hook },
	{ InitRawControllers_ptr, input::InitRawControllers_hook },
	{ EnableController, input::EnableController_r },
	{ DisableController, input::DisableController_r },
	{ reinterpret_cast<void*>(0x0042D52D), rumble::default_rumble },
	// Used to skip over the standard controller update function.
	// This has no effect on the OnInput hook.
	{ UpdateControllers_ptr, reinterpret_cast<void*>(0x0040FDB3) }
};

void InitSDL2_Hacks()
{
	for (uint8_t i = 0; i < LengthOfArray(SDL2jumps); i++)
	{
		WriteJump(SDL2jumps[i].address, SDL2jumps[i].data);
	}
}

int GetEKey(int index)
{
	if (input::e_held && IsFreeCameraAllowed())
	{
		input::e_held = false;
		return 1;
	}
	return 0;
}

void CreateSADXKeyboard(KeyboardInput* ptr, int length)
{
	KeyboardInputPointer = &SADXKeyboard;
}

void SDL2_OnInput()
{
	if (isInputMod || !enabledSDL)
		return;

	input::poll_controllers();

	// Soft reset
	if (ControllerPointers[0]->HeldButtons == (Buttons_A | Buttons_B | Buttons_X | Buttons_Y) && ControllerPointers[0]->ReleasedButtons == Buttons_Start && GameMode != 1 && GameMode != 8)
	{
		if (input::debug) 
			PrintDebug("Soft reset\n");

		SoftResetByte = 1;
	}

	// Demo playback
	if (DemoPlaying && Demo_Enabled && CurrentDemoCutsceneID < 0 && !input::demo) input::demo = true; //Ignore everything but the Start button
	if (input::demo)
	{
		DemoControllerData* demo_memory = (DemoControllerData*)HeapThing;
		if (input::debug) PrintDebug("Demo frame:%d\n", Demo_Frame);
		if (GameState == 15)
		{

			if (input::raw_input[0].HeldButtons & Buttons_Start || Demo_Frame > Demo_MaxFrame || demo_memory[Demo_Frame].HeldButtons == -1)
			{
				Demo_Enabled = 0;
				StartLevelCutscene(6);
				input::demo = false;
				Demo_Frame = 0;
			}
			ControllerPointers[0]->HeldButtons = demo_memory[Demo_Frame].HeldButtons;
			ControllerPointers[0]->LTriggerPressure = demo_memory[Demo_Frame].LTrigger;
			ControllerPointers[0]->RTriggerPressure = demo_memory[Demo_Frame].RTrigger;
			ControllerPointers[0]->LeftStickX = demo_memory[Demo_Frame].StickX;
			ControllerPointers[0]->LeftStickY = demo_memory[Demo_Frame].StickY;
			ControllerPointers[0]->RightStickX = 0;
			ControllerPointers[0]->RightStickY = 0;
			ControllerPointers[0]->NotHeldButtons = demo_memory[Demo_Frame].NotHeldButtons;
			ControllerPointers[0]->PressedButtons = demo_memory[Demo_Frame].PressedButtons;
			ControllerPointers[0]->ReleasedButtons = demo_memory[Demo_Frame].ReleasedButtons;
		}
		else
		{
			input::raw_input[0].LeftStickX = 0;
			input::raw_input[0].LeftStickY = 0;
			input::raw_input[0].HeldButtons = 0;
			input::raw_input[0].LTriggerPressure = 0;
			input::raw_input[0].RTriggerPressure = 0;
			input::raw_input[0].LeftStickX = 0;
			input::raw_input[0].LeftStickY = 0;
			input::raw_input[0].RightStickX = 0;
			input::raw_input[0].RightStickY = 0;
			input::raw_input[0].NotHeldButtons = -1;
			input::raw_input[0].PressedButtons = 0;
			input::raw_input[0].ReleasedButtons = 0;
		}
	}
}

void SDL2_Init()
{
	enabledSDL = true;
	if (GetModuleHandle(L"sadx-input-mod") != nullptr)
	{
		isInputMod = true;
		MessageBox(nullptr, L"The Input Mod should be disabled when Better Input (SDL2) is enabled in the Mod Manager. "
			"Disable the Input Mod and try again.\n\n"
			"If you would like to continue using the old Input Mod, disable Better Input in the Mod Manager (not recommended).",
			L"SDL Load Error", MB_OK | MB_ICONERROR);
		return;
	}

	std::wstring sdlFolderPath = appPath + L"extlib\\SDL2\\";

	//if path doesn't exist, assume the dll is in the game folder directly
	if (!FileExists(sdlFolderPath + L"SDL2.dll"))
		sdlFolderPath = L"";

	std::wstring dll = sdlFolderPath + L"SDL2.dll";

	const auto handle = LoadLibrary(dll.c_str());

	if (handle == nullptr)
	{
		MessageBox(nullptr, L"Error loading SDL.\n\n"
			L"Make sure the Mod Loader is installed properly.",
			L"SDL Load Error", MB_OK | MB_ICONERROR);
		enabledSDL = false;
		return;
	}

	int init;
	if ((init = SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC | SDL_INIT_EVENTS)) != 0)
	{
		PrintDebug("[Input] Unable to initialize SDL. Error code: %i\n", init);
		MessageBoxA(nullptr, "Error initializing SDL. See debug message for details.",
			"SDL Init Error", MB_OK | MB_ICONERROR);
		return;
	}

	InitSDL2_Hacks();

	// Replace function to get the E key for centering camera on character
	WriteCall(reinterpret_cast<void*>(0x00437547), GetEKey);
	// Replace call to CreateKeyboardDevice to set up SADX keyboard pointer
	WriteCall(reinterpret_cast<void*>(0x0077F0D7), CreateSADXKeyboard);
	// Disable call to CreateMouseDevice
	WriteData<5>(reinterpret_cast<void*>(0x0077F03E), 0x90i8);
	// Disable call to DirectInput_Init
	WriteData<5>(reinterpret_cast<void*>(0x0077F205), 0x90i8);

	// EnableControl
	WriteData(reinterpret_cast<bool**>(0x40EF80), &input::controller_enabled[0]);
	WriteData(reinterpret_cast<bool**>(0x40EF86), &input::controller_enabled[1]);
	WriteData(reinterpret_cast<bool**>(0x40EF90), input::controller_enabled);

	// DisableControl
	WriteData(reinterpret_cast<bool**>(0x40EFB0), &input::controller_enabled[0]);
	WriteData(reinterpret_cast<bool**>(0x40EFB6), &input::controller_enabled[1]);
	WriteData(reinterpret_cast<bool**>(0x40EFC0), input::controller_enabled);

	// IsControllerEnabled
	WriteData(reinterpret_cast<bool**>(0x40EFD8), input::controller_enabled);

	// Control
	WriteData(reinterpret_cast<bool**>(0x40FE0D), input::controller_enabled);
	WriteData(reinterpret_cast<bool**>(0x40FE2F), &input::controller_enabled[1]);

	// WriteAnalogs
	WriteData(reinterpret_cast<bool**>(0x40F30C), input::controller_enabled);

	input::controller_enabled[0] = true;
	input::controller_enabled[1] = true;

	std::wstring dbpath = sdlFolderPath + L"gamecontrollerdb.txt";

	if (FileExists(dbpath))
	{
		char pathbuf[MAX_PATH];
		sprintf(pathbuf, "%ws", dbpath.c_str());

		int result = SDL_GameControllerAddMappingsFromFile(pathbuf);

		if (result == -1)
		{
			PrintDebug("[Input] Error loading gamecontrollerdb: %s\n", SDL_GetError());
		}
		else
		{
			PrintDebug("[Input] Controller mappings loaded: %i\n", result);
		}
	}

	const std::wstring config_path = sdlFolderPath + L"SDLconfig.ini";

#ifdef _DEBUG
	const bool debug_default = true;
#else
	const bool debug_default = false;
#endif

	IniFile config(config_path);
	ushort KeyboardPlayer = max(0, min(7, config.getInt("Config", "KeyboardPlayer", 0)));
	input::debug = config.getBool("Config", "Debug", debug_default);
	input::disable_mouse = config.getBool("Config", "DisableMouse", true);
	// Keyboard mappings
	const char* keyb[] = { "Keyboard", "Keyboard 2", "Keyboard 3" };
	for (int n = 0; n < 3; n++)
	{
		input::keys.Analog1_Up[n] = config.getInt(keyb[n], "-lefty", 38);
		input::keys.Analog1_Down[n] = config.getInt(keyb[n], "+lefty", 40);
		input::keys.Analog1_Left[n] = config.getInt(keyb[n], "-leftx", 37);
		input::keys.Analog1_Right[n] = config.getInt(keyb[n], "+leftx", 39);
		input::keys.Analog2_Up[n] = config.getInt(keyb[n], "-righty", 73);
		input::keys.Analog2_Down[n] = config.getInt(keyb[n], "+righty", 77);
		input::keys.Analog2_Left[n] = config.getInt(keyb[n], "-rightx", 74);
		input::keys.Analog2_Right[n] = config.getInt(keyb[n], "+rightx", 76);
		input::keys.Button_A[n] = config.getInt(keyb[n], "a", 88);
		input::keys.Button_B[n] = config.getInt(keyb[n], "b", 90);
		input::keys.Button_X[n] = config.getInt(keyb[n], "x", 65);
		input::keys.Button_Y[n] = config.getInt(keyb[n], "y", 83);
		input::keys.Button_Start[n] = config.getInt(keyb[n], "start", 13);
		input::keys.Button_Back[n] = config.getInt(keyb[n], "back", 86);
		input::keys.LT[n] = config.getInt(keyb[n], "lefttrigger", 81);
		input::keys.RT[n] = config.getInt(keyb[n], "righttrigger", 87);
		input::keys.Button_LeftShoulder[n] = config.getInt(keyb[n], "leftshoulder", 67);
		input::keys.Button_RightShoulder[n] = config.getInt(keyb[n], "rightshoulder", 66);
		input::keys.Button_LeftStick[n] = config.getInt(keyb[n], "leftstick", 69);
		input::keys.Button_RightStick[n] = config.getInt(keyb[n], "rightstick", 160);
		input::keys.DPad_Up[n] = config.getInt(keyb[n], "dpup", 104);
		input::keys.DPad_Down[n] = config.getInt(keyb[n], "dpdown", 98);
		input::keys.DPad_Left[n] = config.getInt(keyb[n], "dpleft", 100);
		input::keys.DPad_Right[n] = config.getInt(keyb[n], "dpright", 102);
	}
	// This defaults RadialR to enabled if smooth-cam is detected.
	const bool smooth_cam = GetModuleHandle(L"smooth-cam.dll") != nullptr;

	for (ushort i = 0; i < GAMEPAD_COUNT; i++)
	{
		DreamPad::Settings& settings = DreamPad::controllers[i].settings;

		const std::string section = "Controller " + std::to_string(i + 1);

		const int deadzone_l = config.getInt(section, "DeadzoneL", GAMEPAD_LEFT_THUMB_DEADZONE);
		const int deadzone_r = config.getInt(section, "DeadzoneR", GAMEPAD_RIGHT_THUMB_DEADZONE);

		settings.set_deadzone_l(deadzone_l);
		settings.set_deadzone_r(deadzone_r);

		settings.radial_l = config.getBool(section, "RadialL", true);
		settings.radial_r = config.getBool(section, "RadialR", smooth_cam);

		settings.trigger_threshold = config.getInt(section, "TriggerThreshold", GAMEPAD_TRIGGER_THRESHOLD);

		settings.rumble_factor = clamp(config.getFloat(section, "RumbleFactor", 1.0f), 0.0f, 1.0f);

		settings.mega_rumble = config.getBool(section, "MegaRumble", false);
		settings.rumble_min_time = static_cast<ushort>(config.getInt(section, "RumbleMinTime", 0));

		settings.allow_keyboard = (i == KeyboardPlayer);

		settings.guid = SDL_GUIDFromString(config.getString(section, "GUID", "00000000000000000000000000000000").c_str());

		if (input::debug)
		{
			PrintDebug("[Input] Deadzones for P%d (L/R/T): %05d / %05d / %05d\n", (i + 1),
				settings.deadzone_l, settings.deadzone_r, settings.trigger_threshold);
		}
	}

	PrintDebug("[Input] Initialization complete.\n");
}

void SDL2_OnExit()
{
	if (isInputMod || !enabledSDL)
		return;

	for (auto& i : DreamPad::controllers)
	{
		i.close();
	}

	SDL_Quit();
}
