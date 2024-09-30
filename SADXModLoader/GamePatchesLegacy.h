#pragma once

struct LegacyGamePatch
{
	std::string PatchName;
	bool DefaultValue;
};

// List of patches that may be using the old system
LegacyGamePatch LegacyGamePatchList[] =
{
	{ "HRTFSound", false },
	{ "KeepCamSettings", true },
	{ "FixVertexColorRendering", true },
	{ "MaterialColorFix", true },
	{ "NodeLimit", true },
	{ "FOVFix", true },
	{ "SkyChaseResolutionFix", true },
	{ "Chaos2CrashFix", true },
	{ "ChunkSpecularFix", true },
	{ "E102NGonFix", true },
	{ "ChaoPanelFix", true },
	{ "PixelOffSetFix", true },
	{ "LightFix", true },
	{ "KillGBIX", false },
	{ "DisableCDCheck", true },
	{ "ExtendedSaveSupport", true },
	{ "CrashGuard", true },
	{ "XInputFix", false },
};