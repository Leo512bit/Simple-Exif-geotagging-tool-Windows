//© 2024 Leonard Matthew Teyssier BSD-4 Clause License

#ifndef UNICODE
#define UNICODE
#endif

#define COBJMACROS
#include <windows.h>
#include <shobjidl.h>

#include "select.h"

// Define the global variable declared in the header
wchar_t g_szSelectedFile[MAX_PATH] = { 0 };

BOOL SelectImageFile(HWND hwndOwner) {
    IFileOpenDialog* pFileOpen = NULL;
    
    HRESULT hr = CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, &IID_IFileOpenDialog, (void**)&pFileOpen);
    if (FAILED(hr)) return FALSE;

    // Define allowed JPEG file types
    COMDLG_FILTERSPEC fileTypes[] = {
        { L"JPEG Images (*.jpg; *.jpeg)", L"*.jpg;*.jpeg" },
        { L"All Files (*.*)", L"*.*" }
    };
    
    // Pure-C COM macro style using COBJMACROS (much cleaner than manually writing ->lpVtbl)
    IFileOpenDialog_SetFileTypes(pFileOpen, 2, fileTypes);

    // Display the dialog box
    hr = IFileOpenDialog_Show(pFileOpen, hwndOwner);
    if (SUCCEEDED(hr)) {
        IShellItem* pItem = NULL;
        hr = IFileOpenDialog_GetResult(pFileOpen, &pItem);
        if (SUCCEEDED(hr)) {
            wchar_t* pszFilePath = NULL;
            hr = IShellItem_GetDisplayName(pItem, SIGDN_FILESYSPATH, &pszFilePath);
            
            if (SUCCEEDED(hr)) {
                // Safely copy the string buffer over to our global path variable
				lstrcpynW(g_szSelectedFile, pszFilePath, MAX_PATH);
                CoTaskMemFree(pszFilePath);
            }
            IShellItem_Release(pItem);
        }
    }
    
    IFileOpenDialog_Release(pFileOpen);
    return SUCCEEDED(hr);
}