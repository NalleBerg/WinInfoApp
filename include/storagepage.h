#pragma once
#include <windows.h>

// Create the storage page child window. Parent is main window, hInst is module instance.
HWND CreateStoragePage(HWND parent, HINSTANCE hInst);

// Register class (no-op if already registered)
void RegisterStoragePageClass(HINSTANCE hInst);
