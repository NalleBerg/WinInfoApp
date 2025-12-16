#pragma once

#include <windows.h>

// Register the CPU info page class (call once at startup)
BOOL RegisterCpuInfoPageClass(HINSTANCE hInstance);

// Create the CPU info child window under a parent
HWND CreateCpuInfoPage(HWND hParent, HINSTANCE hInstance);

// Show CPU info as a standalone window (centered over parent if provided)
void ShowCpuInfoWindow(HWND hParent);

// Return preferred client height (pixels) for the CPU page based on current DPI and content
int GetCpuInfoPagePreferredHeight(HWND hwnd);
