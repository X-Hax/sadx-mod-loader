#pragma once

extern HINSTANCE g_hinstDll;
extern wstring iconPathName;

void PatchWindow(const LoaderSettings& settings);
void SetBorderImage(std::wstring path);