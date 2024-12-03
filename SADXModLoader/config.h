#pragma once

void LoadModLoaderSettings(LoaderSettings* loaderSettings, std::wstring appPath, std::wstring gamePath);
unsigned int GetModCount();
std::string GetModName(int index);
bool IsGamePatchEnabled(const char* patchName);
void ListGamePatches();