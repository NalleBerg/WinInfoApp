#pragma once
#include <windows.h>

// Show the custom menu panel at screen coordinates (x,y).
// The panel will post WM_COMMAND messages to `parent` when an item is chosen
// using the same IDs as the app (1001..).
HWND ShowMenuPanel(HWND parent, int x, int y);

// Close any open panel (if needed)
void CloseMenuPanel();
