#pragma once

void LoadModLoaderSettings(LoaderSettings* loaderSettings, std::wstring appPath, std::wstring gamePath);
void DisplaySettingsLoadError(std::wstring gamePath, std::wstring appPath, std::wstring errorFile);
unsigned int GetModCount();
std::string GetModName(int index);
bool IsGamePatchEnabled(const char* patchName);
void ListGamePatches();