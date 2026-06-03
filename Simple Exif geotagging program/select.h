//© 2024 Leonard Matthew Teyssier BSD-4 Clause License
#pragma once

#include <windows.h>

// Global storage for the absolute file path, accessible by main.c and wic_geotag.c
extern wchar_t g_szSelectedFile[MAX_PATH];

// Launches the native Windows file picker. Returns TRUE if a file was selected.
BOOL SelectImageFile(HWND hwndOwner);
