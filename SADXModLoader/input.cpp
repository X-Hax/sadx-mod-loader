#include "stdafx.h"

#include <SADXModLoader.h>

#include "minmax.h"
#include "typedefs.h"
#include "input.h"
#include "rumble.h"
#include "DreamPad.h"

struct AnalogThing
{
	Angle angle;
	float magnitude;
};

// To be deleted
// This function compares two SDL GUIDs and returns true if they match or if the first GUID is full of 00s.
bool CompareGUID(SDL_GUID guid_stored, SDL_GUID guid_new)
{
	for (int d = 0; d < 16; d++)
	{
		if (guid_stored.data[d] != 0)
		{
			goto check;
			break;
		}
	}
	return true;

	check:
	for (int d = 0; d < 16; d++)
	{
		if (guid_stored.data[d] != guid_new.data[d])
			return false;
	}
	return true;
}

namespace input
{
	ControllerData raw_input[GAMEPAD_COUNT];
	bool legacy_mode = true; // Will remove when the new AppLauncher is finished
	bool controller_enabled[GAMEPAD_COUNT];
	bool debug = false;
	bool disable_mouse = true;
	bool e_held = false;
	bool demo = false;
	KeyboardMapping keys;
	ushort keyboard_player_current; // Only in non-legacy mode
	ushort keyboard_player_last; // Only in non-legacy mode

	inline short get_free_controller()
	{
		for (short i = 0; i < GAMEPAD_COUNT; i++)
		{
			DreamPad& controller = DreamPad::controllers[i];
			if (!controller.connected())
				return i;
		}
		return -1;
	}

	inline ushort get_duplicate_controller(ushort instance)
	{
		for (ushort i = 0; i < GAMEPAD_COUNT; i++)
		{
			DreamPad& controller = DreamPad::controllers[i];
			if (controller.controller_id() == instance)
				return i;
		}
		return -1;
	}

	inline void open_free(int which)
	{
		short free = get_free_controller();
		{
			if (free != -1)
			{
				DreamPad& controller = DreamPad::controllers[free];
				if (input::debug)
					PrintDebug("\tOpening controller slot: %d\n", free);
				controller.open(which);
			}
			else
				if (input::debug)
					PrintDebug("\tOpening controller failed: no free slots\n");
		}
	}

	inline void swap_player(ushort src, ushort dst, int which)
	{
		// Get controllers
		DreamPad& controller_src = DreamPad::controllers[src];
		DreamPad& controller_dst = DreamPad::controllers[dst];
		// Get device index for the other controller
		int id_open_src = controller_src.open_id();
		// Close controllers
		controller_src.close();
		controller_dst.close();
		// Reopen controllers with swapped indices
		controller_src.open(which);
		controller_dst.open(id_open_src);
	}

	inline void open_player(ushort player, int which)
	{
		DreamPad& controller = DreamPad::controllers[player];
		// If it's not connected, just open the controller
		if (!controller.connected())
		{
			if (input::debug)
				PrintDebug("\tOpening controller slot %d\n", player);
			controller.open(which);
		}
		// If not, find another one that isn't connected and swap with it
		else
		{
			short unused = get_free_controller();
			if (unused != -1)
			{
				if (input::debug)
					PrintDebug("\tSwapping controller slots %d and %d: device index %d\n", player, unused, which);
				swap_player(player, unused, which);
			}
		}
	}

	inline void poll_sdl()
	{
		SDL_Event event;

		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			default:
				break;

			case SDL_JOYDEVICEADDED:
			{
				// To be removed
				if (input::legacy_mode)
				{
					const int which = event.cdevice.which;
					for (auto& controller : DreamPad::controllers)
					{
						// The condition is used to prioritize controller GUIDs for specific player IDs.
						// E.g. if Player 1 has a valid stored GUID, a newly connected device with the same GUID will be assigned to Player 1.
						// At the same time, if Player 1 doesn't have a valid stored GUID, any newly added device will be assigned to Player 1.
						// Both of the above will only happen if Player 1 doesn't already have a device assigned to it.
						if (CompareGUID(controller.settings.guid, SDL_JoystickGetDeviceGUID(event.cdevice.which)))
						{
							// Checking for both in cases like the DualShock 4 and (e.g.) DS4Windows where the controller might be
							// "connected" twice with the same ID. DreamPad::open automatically closes if already open.
							if (!controller.connected() || controller.controller_id() == which)
							{
								controller.open(which);
								break;
							}
						}
					}
				}
				else
				{
					// Get connected device info
					const int dev_index = event.cdevice.which;
					const char* devicename = SDL_JoystickNameForIndex(dev_index);
					std::string devicepath = SDL_JoystickPathForIndex(dev_index);
					if (devicepath.empty() || strlen(devicepath.c_str()) < 9)
						break;
					devicepath = devicepath.substr(8); // Remove '\\?\HID#' to avoid having to deal with slashes
					bool hasConfig = config->hasGroup(devicepath);
					// The ID used in all events other than opening is different from the "which" in "device added" events
					// See: https://wiki.libsdl.org/SDL2/SDL_GameControllerOpen#remarks
					const int dev_instance = SDL_JoystickGetDeviceInstanceID(dev_index);
					PrintDebug("[Input] Device: %s (index: %d, instance: %d)\n", devicename, dev_index, dev_instance);
					if (input::debug)
						PrintDebug("\tPath: %s\n", devicepath);
					// Check if a device is already connected
					short duplicate = get_duplicate_controller(dev_instance);
					if (duplicate != -1)
					{
						PrintDebug("\tDevice index %d is already connected as player %d (instance %d)\n", dev_index, duplicate, dev_instance);
						// Checking for cases like the DualShock 4 and (e.g.) DS4Windows where the controller might be
						// "connected" twice with the same ID. DreamPad::open automatically closes if already open.
						DreamPad::controllers[duplicate].open(dev_index);
						break;
					}
					// Check if a device has configuration
					else if (hasConfig)
					{
						// Check if a device is not ignored
						if (!config->getBool(devicepath, "Ignore", false))
						{
							// Check if a device is mapped to a specific player
							int player = config->getInt(devicepath, "Player", -1);
							if (player != -1)
							{
								PrintDebug("\tDevice assigned to slot %d (Player %d)\n", player, player + 1);
								open_player(player, dev_index);
							}
							// If not, find a free slot and connect
							else
							{
								open_free(dev_index);
							}
						}
						else
						{
							PrintDebug("\tDevice is on the ignore list\n");
						}
					}
					// If a device has no configuration, just find a free slot and connect
					else
					{
						if (input::debug)
							PrintDebug("\tDevice has no configuration\n", devicepath);
						open_free(dev_index);
					}
				}
				break;
			}

			case SDL_JOYDEVICEREMOVED:
			{
				// The 'event.cdevice.which' here is joystick instance ID, different from the one in SDL_JOYDEVICEADDED
				const int which = event.cdevice.which;

				for (auto& controller : DreamPad::controllers)
				{
					if (controller.controller_id() == which)
					{
						controller.close();
						break;
					}
				}
				break;
			}
			}
		}

		SDL_GameControllerUpdate();
	}

	void poll_controllers()
	{
		poll_sdl();
		KeyboardMouse::poll();
		
		for (uint i = 0; i < GAMEPAD_COUNT; i++)
		{
			DreamPad& dreampad = DreamPad::controllers[i];

			dreampad.poll();
			raw_input[i] = dreampad.dreamcast_data();

			// Compatibility for mods which use ControllersRaw directly.
			// This will only copy the first four controllers.
			if (i < ControllersRaw.size())
			{
				ControllersRaw[i] = raw_input[i];
			}

#ifdef EXTENDED_BUTTONS
			if (dreampad.e_held_pad()) input::e_held = true;
			if (debug && raw_input[i].HeldButtons & Buttons_C)
			{
				const ControllerData& pad = raw_input[i];
				Motor m = DreamPad::controllers[i].active_motor();

				DisplayDebugStringFormatted(NJM_LOCATION(0, 8 + (3 * i)), "P%d  B: %08X LT/RT: %03d/%03d V: %d%d", (i + 1),
					pad.HeldButtons, pad.LTriggerPressure, pad.RTriggerPressure, (m & Motor::large), (m & Motor::small_) >> 1);
				DisplayDebugStringFormatted(NJM_LOCATION(4, 9 + (3 * i)), "LS: %4d/%4d (%f) RS: %4d/%4d (%f)",
					pad.LeftStickX, pad.LeftStickY, dreampad.normalized_l(), pad.RightStickX, pad.RightStickY,
					dreampad.normalized_r());

				if (pad.HeldButtons & Buttons_Z)
				{
					const int pressed = pad.PressedButtons;
					if (pressed & Buttons_Up)
					{
						dreampad.settings.rumble_factor += 0.125f;
					}
					else if (pressed & Buttons_Down)
					{
						dreampad.settings.rumble_factor -= 0.125f;
					}
					else if (pressed & Buttons_Left)
					{
						rumble::RumbleA_r(i, 0);
					}
					else if (pressed & Buttons_Right)
					{
						rumble::RumbleB_r(i, 7, 59, 6);
					}

					DisplayDebugStringFormatted(NJM_LOCATION(4, 10 + (3 * i)),
						"Rumble factor (U/D): %f (L/R to test)", dreampad.settings.rumble_factor);
				}
			}
#endif
		}
		if (!input::legacy_mode)
		{
			bool fix_keyboard = false;
			// Set keyboard to Player 1 if Player 1 is disconnected
			if (!DreamPad::controllers[0].connected() && keyboard_player_current != 0)
			{
				keyboard_player_last = keyboard_player_current;
				keyboard_player_current = 0;
				fix_keyboard = true;
			}
			// Restore keyboard for the other player if Player 1 reconnects
			else if (DreamPad::controllers[0].connected() && keyboard_player_last != 0 && keyboard_player_current == 0)
			{
				keyboard_player_current = keyboard_player_last;
				fix_keyboard = true;
			}
			if (fix_keyboard)
			{
				KeyboardMouse::set_player(keyboard_player_current);
			}
		}
	}

	// ReSharper disable once CppDeclaratorNeverUsed
	static void WriteAnalogs_c()
	{
		if (!ControlEnabled)
		{
			return;
		}

		for (uint i = 0; i < GAMEPAD_COUNT; i++)
		{
			if (!controller_enabled[i])
			{
				continue;
			}

			const DreamPad& dream_pad = DreamPad::controllers[i];

			if (dream_pad.connected() || (dream_pad.settings.allow_keyboard && !i))
			{
				const ControllerData& pad = dream_pad.dreamcast_data();
				// SADX's internal deadzone is 12 of 127. It doesn't set the relative forward direction
				// unless this is exceeded in WriteAnalogs(), so the analog shouldn't be set otherwise.
				if (abs(pad.LeftStickX) > 12 || abs(pad.LeftStickY) > 12)
				{
					NormalizedAnalogs[i].magnitude = dream_pad.normalized_l();
				}
			}
		}
	}

	void __declspec(naked) WriteAnalogs_hook()
	{
		__asm
		{
			call WriteAnalogs_c
			ret
		}
	}

	// ReSharper disable once CppDeclaratorNeverUsed
	static void redirect_raw_controllers()
	{
		for (uint i = 0; i < GAMEPAD_COUNT; i++)
		{
			ControllerPointers[i] = &raw_input[i];
		}
	}

	void __declspec(naked) InitRawControllers_hook()
	{
		__asm
		{
			call redirect_raw_controllers
			ret
		}
	}

	void EnableController_r(Uint8 index)
	{
		// default behavior 
		if (index > 1)
		{
			controller_enabled[0] = true;
			controller_enabled[1] = true;
		}

		if (index > GAMEPAD_COUNT)
		{
			for (Uint32 i = 0; i < min(index, static_cast<Uint8>(GAMEPAD_COUNT)); i++)
			{
				EnableController_r(i);
			}
		}
		else
		{
			controller_enabled[index] = true;
		}
	}

	void DisableController_r(Uint8 index)
	{
		// default behavior 
		if (index > 1)
		{
			controller_enabled[0] = false;
			controller_enabled[1] = false;
		}

		if (index > GAMEPAD_COUNT)
		{
			for (Uint32 i = 0; i < min(index, static_cast<Uint8>(GAMEPAD_COUNT)); i++)
			{
				DisableController_r(i);
			}
		}
		else
		{
			controller_enabled[index] = false;
		}
	}
}
