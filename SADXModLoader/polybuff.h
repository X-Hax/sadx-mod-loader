#pragma once

namespace polybuff
{
	// These three variables are the parameters used to initialize the polybuff subsystem.
	// Used to re-initialize when the device resets (window resize etc).
	extern int alignment_probably;
	extern int count;
	extern void* ptr;

	void init();
	void rewrite_init();
}
