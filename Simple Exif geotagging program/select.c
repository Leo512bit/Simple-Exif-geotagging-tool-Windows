//© 2026 Leonard Matthew Teyssier BSD-4 Clause License

#ifndef UNICODE
#define UNICODE
#endif

#define COBJMACROS
#include <windows.h>
#include <shobjidl.h>
//#include <shlbapi.h> header is not found
#include <shlobj.h>

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



//Drag and drop


typedef struct {
	IDropTargetVtbl* lpVtbl;
	LONG refCount;
	HWND hwnd;
} DropTargetImpl;

// Forward declarations for the Vtbl
HRESULT STDMETHODCALLTYPE DropTarget_QueryInterface(IDropTarget* This, REFIID riid, void** ppvObject);
ULONG STDMETHODCALLTYPE DropTarget_AddRef(IDropTarget* This);
ULONG STDMETHODCALLTYPE DropTarget_Release(IDropTarget* This);
HRESULT STDMETHODCALLTYPE DropTarget_DragEnter(IDropTarget* This, IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect);
HRESULT STDMETHODCALLTYPE DropTarget_DragOver(IDropTarget* This, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect);
HRESULT STDMETHODCALLTYPE DropTarget_DragLeave(IDropTarget* This);
HRESULT STDMETHODCALLTYPE DropTarget_Drop(IDropTarget* This, IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect);

// Instantiate the Vtbl
static IDropTargetVtbl DropTarget_Vtbl = {
	DropTarget_QueryInterface,
	DropTarget_AddRef,
	DropTarget_Release,
	DropTarget_DragEnter,
	DropTarget_DragOver,
	DropTarget_DragLeave,
	DropTarget_Drop
};

// Allocation helper
IDropTarget* CreateDropTarget(HWND hwnd) {
	DropTargetImpl* pImpl = (DropTargetImpl*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(DropTargetImpl));
	if (pImpl) {
		pImpl->lpVtbl = &DropTarget_Vtbl;
		pImpl->refCount = 1;
		pImpl->hwnd = hwnd;
	}
	return (IDropTarget*)pImpl;
}

// --- COM Method Implementations ---

HRESULT STDMETHODCALLTYPE DropTarget_QueryInterface(IDropTarget* This, REFIID riid, void** ppvObject) {
	if (!ppvObject) return E_POINTER;
	if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDropTarget)) {
		IDropTarget_AddRef(This);
		*ppvObject = This;
		return S_OK;
	}
	*ppvObject = NULL;
	return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE DropTarget_AddRef(IDropTarget* This) {
	DropTargetImpl* pImpl = (DropTargetImpl*)This;
	return InterlockedIncrement(&pImpl->refCount);
}

ULONG STDMETHODCALLTYPE DropTarget_Release(IDropTarget* This) {
	DropTargetImpl* pImpl = (DropTargetImpl*)This;
	LONG count = InterlockedDecrement(&pImpl->refCount);
	if (count == 0) {
		HeapFree(GetProcessHeap(), 0, pImpl);
		return 0;
	}
	return count;
}

HRESULT STDMETHODCALLTYPE DropTarget_DragEnter(IDropTarget* This, IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) {
	*pdwEffect &= DROPEFFECT_COPY;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE DropTarget_DragOver(IDropTarget* This, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) {
	*pdwEffect &= DROPEFFECT_COPY;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE DropTarget_DragLeave(IDropTarget* This) {
	return S_OK;
}


HRESULT STDMETHODCALLTYPE DropTarget_Drop(IDropTarget* This, IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) {
	DropTargetImpl* pImpl = (DropTargetImpl*)This;

	FORMATETC fmt = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
	STGMEDIUM med;

	if (SUCCEEDED(IDataObject_GetData(pDataObj, &fmt, &med))) {
		HDROP hDrop = (HDROP)GlobalLock(med.hGlobal);
		if (hDrop) {
			// Grab the first file dropped
			DragQueryFileW(hDrop, 0, g_szSelectedFile, MAX_PATH);
			GlobalUnlock(med.hGlobal);


			HWND hBar = FindWindowExW(pImpl->hwnd, NULL, STATUSCLASSNAMEW, NULL);
			if (hBar) {
				SendMessageW(hBar, SB_SETTEXTW, 1, (LPARAM)g_szSelectedFile);
			}

			// Bring focus to the window and coordinate field
			SetForegroundWindow(pImpl->hwnd);
			HWND hEdit = GetDlgItem(pImpl->hwnd, 101); // IDC_LAT_DEG
			if (hEdit) SetFocus(hEdit);

			*pdwEffect &= DROPEFFECT_COPY;
		}
		ReleaseStgMedium(&med);
	}
	else {
		*pdwEffect = DROPEFFECT_NONE;
	}
	return S_OK;
}