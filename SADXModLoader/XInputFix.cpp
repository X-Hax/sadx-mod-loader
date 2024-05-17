#include "stdafx.h"

Uint32 ProcessButtons(Uint32 orig)
{
	Uint32 result = 0;
	if (orig & 0x04)
		result |= Buttons_A;
	if (orig & 0x02)
		result |= Buttons_B;
	if (orig & 0x20)
		result |= Buttons_X;
	if (orig & 0x400)
		result |= Buttons_Y;
	if (orig & 0x200)
		result |= Buttons_L;
	if (orig & 0x10)
		result |= Buttons_R;
	if (orig & 0x10000)
		result |= Buttons_Start;
	if (orig & 0x20000)
		result |= Buttons_Z;
	if (orig & 0x8)
		result |= Buttons_C;
	return result;
}

static Trampoline* GetControllerData_t;
static ControllerData* GetControllerData_r(int a1)
{
	Sint16 newx = 0;
	Uint16 deadzone = 24; // To prevent 1st person camera from getting stuck at near-center values of the stick

	ControllerData* orig = TARGET_DYNAMIC(GetControllerData)(a1);

	Uint16 rawx = (Uint16)orig->RightStickX; // X360 controller reports 0 to 65535 for this axis in DInput mode
	Sint16 newy = orig->RightStickY; // X360 controller reports -128 to 128 for this axis in DInput mode
	Uint32 rawbtn_held = orig->HeldButtons;
	Uint32 rawbtn_notheld = orig->NotHeldButtons;
	Uint32 rawbtn_pressed = orig->PressedButtons;
	// Convert 0~65535 to -128~128 for the X axis
	if (rawx != 0)
	{
		if (rawx < 32768)
		{
			newx = int(-128.0f + (float)rawx / 256.0f);
		}
		else
		{
			newx = int((float)(rawx - 32768) / 256.0f);
		}

	}

	// Apply deadzones
	if (abs(newx) < deadzone)
		newx = 0;
	if (abs(newy) < deadzone)
		newy = 0;

	orig->RightStickX = newx;
	orig->RightStickY = newy;

	/*
	if (a1 == 0)
	{
		PrintDebug("X2: %d / %d\n", rawx, orig->RightStickX);
		PrintDebug("Y2: %d / %d\n", newy, orig->RightStickY);
		PrintDebug("Buttons: %X\n", orig->HeldButtons);
	}
	*/

	// Process buttons
	orig->HeldButtons = ProcessButtons(rawbtn_held);
	orig->PressedButtons = ProcessButtons(rawbtn_pressed);
	orig->NotHeldButtons = ProcessButtons(rawbtn_notheld);

	return orig;
}

void XInputFix_Init()
{
	GetControllerData_t = new Trampoline(0x0077F120, 0x0077F129, GetControllerData_r);
}