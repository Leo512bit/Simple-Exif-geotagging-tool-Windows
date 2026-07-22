//© 2026 Leonard Matthew Teyssier BSD-4 Clause License

#ifndef UNICODE
#define UNICODE
#endif

#define COBJMACROS
#include <windows.h>

//#define memset(dest, c, count) RtlZeroMemory(dest, count)

#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>

#include "main.h"
#include "wic_geotag.h"
#include "select.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")


// Control IDs for our UI Elements
#define IDC_LAT_DEG  101
#define IDC_LAT_MIN  102
#define IDC_LAT_SEC  103
#define IDC_LAT_DIR  104

#define IDC_LON_DEG  105
#define IDC_LON_MIN  106
#define IDC_LON_SEC  107
#define IDC_LON_DIR  108

#define IDC_APPLY    109
#define IDC_SELECT   110



static inline int ScaleDpi(int value, UINT dpi) {
	return MulDiv(value, dpi, 96); // 96 is standard 100% desktop scale
}


// Global safety guard for programmatic UI updates
BOOL g_IsParsingPaste = FALSE;

// Window Handles for our Edit Fields
HWND hLatDeg, hLatMin, hLatSec, hLatDir;
HWND hLonDeg, hLonMin, hLonSec, hLonDir;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wp, LPARAM lp);
LRESULT CALLBACK SharedEditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wp, LPARAM lp, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

// Custom lightweight string-to-integer helper replacing _wtoi
static int CustomWcharToInt(const wchar_t* pszStr)
{
	int result = 0;
	while (*pszStr >= L'0' && *pszStr <= L'9') {
		result = (result * 10) + (*pszStr - L'0');
		pszStr++;
	}
	return result;
}

// Custom locator replacing wcschr
static const wchar_t* CustomWcharFindChar(const wchar_t* pszStr, wchar_t ch)
{
	while (*pszStr) {
		if (*pszStr == ch) return pszStr;
		pszStr++;
	}
	return NULL;
}

// Locates the final backslash in a path string replacing wcsrchr
static const wchar_t* CustomWcharFindLastSlash(const wchar_t* pszStr)
{
	const wchar_t* pszLast = NULL;
	while (*pszStr) {
		if (*pszStr == L'\\') pszLast = pszStr;
		pszStr++;
	}
	return pszLast;
}


static void CheckInitialFileArg(void) {
	int argc = 0;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

	// argv[0] is the executable path, argv[1] is the dropped file
	if (argv && argc > 1) {
		lstrcpynW(g_szSelectedFile, argv[1], MAX_PATH);
	}

	if (argv) {
		LocalFree(argv);
	}
}



int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
	CheckInitialFileArg();
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	HRESULT hr = OleInitialize(NULL);
	BOOL oledInitialized = SUCCEEDED(hr);
	INITCOMMONCONTROLSEX icce;
	icce.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icce.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&icce);

	const wchar_t CLASS_NAME[] = L"ExifGeotagWindowClass";

	WNDCLASS wc = { 0 };
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);

	RegisterClass(&wc);



	UINT systemDpi = GetDpiForSystem();
	int scaledWidth = ScaleDpi(580, systemDpi);
	int scaledHeight = ScaleDpi(200, systemDpi);


	HWND hwnd = CreateWindowEx(0, CLASS_NAME, L"Simple Exif Geotagging Tool", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, scaledWidth, scaledHeight, NULL, NULL, hInstance, NULL);
	//HWND hwnd = CreateWindowEx(0, CLASS_NAME, L"Simple Exif Geotagging Tool", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 580, 200, NULL, NULL, hInstance, NULL);

	if (hwnd == NULL) return 0;


	IDropTarget* pDropTarget = CreateDropTarget(hwnd);
	if (pDropTarget) {
		RegisterDragDrop(hwnd, pDropTarget);
		IDropTarget_Release(pDropTarget);
	}


	/*ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);*/


	//Fix to allow for no runtime dependency on the nCmdShow parameter from WinMain, which can be set to SW_HIDE by certain launchers and would prevent the window from appearing without this workaround
	STARTUPINFOW si = { sizeof(si) };
	GetStartupInfoW(&si);
	int showCmd = (si.dwFlags & STARTF_USESHOWWINDOW) ? si.wShowWindow : SW_SHOWNORMAL;
	ShowWindow(hwnd, showCmd);
	UpdateWindow(hwnd);


MSG msg = {0};
	while (GetMessage(&msg, NULL, 0, 0) > 0) {
		if (!IsDialogMessage(hwnd, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	RevokeDragDrop(hwnd);

	if (oledInitialized) OleUninitialize();
	ExitProcess(0);
	//return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wp, LPARAM lp) {
	static HWND hLatLabel, hLonLabel;
	static HWND hSymD1, hSymM1, hSymS1;
	static HWND hSymD2, hSymM2, hSymS2;
	static HWND hBtnApply, hBtnSelect;
	static HFONT hModernFont;
	static HWND hStatusBar;


	switch (uMsg) {
	case WM_CREATE: {

		UINT dpi = GetDpiForWindow(hwnd);

		


		NONCLIENTMETRICS ncm = { 0 };
		ncm.cbSize = sizeof(NONCLIENTMETRICS) - sizeof(ncm.iPaddedBorderWidth);
		SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0);
		hModernFont = CreateFontIndirect(&ncm.lfMessageFont);

		// --- LATITUDE ROW ---
		hLatLabel = CreateWindowEx(0, L"STATIC", L"Latitude (DMS):", WS_CHILD | WS_VISIBLE | SS_LEFT, ScaleDpi(20, dpi), ScaleDpi(15, dpi), ScaleDpi(200, dpi), ScaleDpi(18, dpi), hwnd, NULL, NULL, NULL);
		hLatDeg = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_CENTER | ES_UPPERCASE | ES_NUMBER | WS_TABSTOP, ScaleDpi(20, dpi), ScaleDpi(40, dpi), ScaleDpi(45, dpi), ScaleDpi(25, dpi), hwnd, (HMENU)IDC_LAT_DEG, NULL, NULL);
		hSymD1 = CreateWindowEx(0, L"STATIC", L"°", WS_CHILD | WS_VISIBLE | SS_CENTER, ScaleDpi(67, dpi), ScaleDpi(43, dpi), ScaleDpi(15, dpi), ScaleDpi(25, dpi), hwnd, NULL, NULL, NULL);
		hLatMin = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_CENTER | ES_UPPERCASE | ES_NUMBER | WS_TABSTOP, ScaleDpi(85, dpi), ScaleDpi(40, dpi), ScaleDpi(40, dpi), ScaleDpi(25, dpi), hwnd, (HMENU)IDC_LAT_MIN, NULL, NULL);
		hSymM1 = CreateWindowEx(0, L"STATIC", L"'", WS_CHILD | WS_VISIBLE | SS_CENTER, ScaleDpi(127, dpi), ScaleDpi(43, dpi), ScaleDpi(15, dpi), ScaleDpi(25, dpi), hwnd, NULL, NULL, NULL);
		hLatSec = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_CENTER | ES_UPPERCASE | WS_TABSTOP, ScaleDpi(145, dpi), ScaleDpi(40, dpi), ScaleDpi(65, dpi), ScaleDpi(25, dpi), hwnd, (HMENU)IDC_LAT_SEC, NULL, NULL);
		hSymS1 = CreateWindowEx(0, L"STATIC", L"\"", WS_CHILD | WS_VISIBLE | SS_CENTER, ScaleDpi(212, dpi), ScaleDpi(43, dpi), ScaleDpi(15, dpi), ScaleDpi(25, dpi), hwnd, NULL, NULL, NULL);
		hLatDir = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_CENTER | ES_UPPERCASE | WS_TABSTOP, ScaleDpi(230, dpi), ScaleDpi(40, dpi), ScaleDpi(30, dpi), ScaleDpi(25, dpi), hwnd, (HMENU)IDC_LAT_DIR, NULL, NULL);

		// --- LONGITUDE ROW ---
		hLonLabel = CreateWindowEx(0, L"STATIC", L"Longitude (DMS):", WS_CHILD | WS_VISIBLE | SS_LEFT, ScaleDpi(300, dpi), ScaleDpi(15, dpi), ScaleDpi(200, dpi), ScaleDpi(18, dpi), hwnd, NULL, NULL, NULL);
		hLonDeg = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_CENTER | ES_UPPERCASE | ES_NUMBER | WS_TABSTOP, ScaleDpi(300, dpi), ScaleDpi(40, dpi), ScaleDpi(45, dpi), ScaleDpi(25, dpi), hwnd, (HMENU)IDC_LON_DEG, NULL, NULL);
		hSymD2 = CreateWindowEx(0, L"STATIC", L"°", WS_CHILD | WS_VISIBLE | SS_CENTER, ScaleDpi(347, dpi), ScaleDpi(43, dpi), ScaleDpi(15, dpi), ScaleDpi(25, dpi), hwnd, NULL, NULL, NULL);
		hLonMin = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_CENTER | ES_UPPERCASE | ES_NUMBER | WS_TABSTOP, ScaleDpi(365, dpi), ScaleDpi(40, dpi), ScaleDpi(40, dpi), ScaleDpi(25, dpi), hwnd, (HMENU)IDC_LON_MIN, NULL, NULL);
		hSymM2 = CreateWindowEx(0, L"STATIC", L"'", WS_CHILD | WS_VISIBLE | SS_CENTER, ScaleDpi(407, dpi), ScaleDpi(43, dpi), ScaleDpi(15, dpi), ScaleDpi(25, dpi), hwnd, NULL, NULL, NULL);
		hLonSec = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_CENTER | ES_UPPERCASE | WS_TABSTOP, ScaleDpi(425, dpi), ScaleDpi(40, dpi), ScaleDpi(65, dpi), ScaleDpi(25, dpi), hwnd, (HMENU)IDC_LON_SEC, NULL, NULL);
		hSymS2 = CreateWindowEx(0, L"STATIC", L"\"", WS_CHILD | WS_VISIBLE | SS_CENTER, ScaleDpi(492, dpi), ScaleDpi(43, dpi), ScaleDpi(15, dpi), ScaleDpi(25, dpi), hwnd, NULL, NULL, NULL);
		hLonDir = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_CENTER | ES_UPPERCASE | WS_TABSTOP, ScaleDpi(510, dpi), ScaleDpi(40, dpi), ScaleDpi(30, dpi), ScaleDpi(25, dpi), hwnd, (HMENU)IDC_LON_DIR, NULL, NULL);

		//hStatusBar = CreateWindowEx(0, STATUSCLASSNAME, NULL, WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, (HMENU)111, NULL, NULL);
		hStatusBar = CreateWindowEx(0, STATUSCLASSNAME, NULL, WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, (HMENU)111, NULL, NULL);



		// --- BUTTONS ---
		hBtnApply = CreateWindowEx(0, L"BUTTON", L"Apply Tag", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP, ScaleDpi(20, dpi), ScaleDpi(95, dpi), ScaleDpi(110, dpi), ScaleDpi(32, dpi), hwnd, (HMENU)IDC_APPLY, NULL, NULL);
		hBtnSelect = CreateWindowEx(0, L"BUTTON", L"Select File…", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP, ScaleDpi(145, dpi), ScaleDpi(95, dpi), ScaleDpi(110, dpi), ScaleDpi(32, dpi), hwnd, (HMENU)IDC_SELECT, NULL, NULL);//BS_DEFPUSHBUTTON

		HWND controls[] = { hLatLabel, hLatDeg, hSymD1, hLatMin, hSymM1, hLatSec, hSymS1, hLatDir, hLonLabel, hLonDeg, hSymD2, hLonMin, hSymM2, hLonSec, hSymS2, hLonDir, hBtnApply, hBtnSelect };
		for (int i = 0; i < sizeof(controls) / sizeof(HWND); i++) {
			SendMessage(controls[i], WM_SETFONT, (WPARAM)hModernFont, TRUE);
		}

		// Hard Character Limits
		SendMessage(hLatDeg, EM_LIMITTEXT, 2, 0); SendMessage(hLonDeg, EM_LIMITTEXT, 3, 0);
		SendMessage(hLatMin, EM_LIMITTEXT, 2, 0); SendMessage(hLonMin, EM_LIMITTEXT, 2, 0);
		SendMessage(hLatSec, EM_LIMITTEXT, 10, 0); SendMessage(hLonSec, EM_LIMITTEXT, 10, 0);
		SendMessage(hLatDir, EM_LIMITTEXT, 1, 0); SendMessage(hLonDir, EM_LIMITTEXT, 1, 0);

		HWND textBoxes[] = { hLatDeg, hLatMin, hLatSec, hLatDir, hLonDeg, hLonMin, hLonSec, hLonDir };
		for (int i = 0; i < 8; i++) {
			SetWindowSubclass(textBoxes[i], SharedEditSubclassProc, i, 0);
		}
		if (g_szSelectedFile[0] != L'\0') {
			SendMessage(hStatusBar, SB_SETTEXTW, 1, (LPARAM)g_szSelectedFile);
			SetFocus(hLatDeg);
		}
		else {
			SetFocus(hBtnSelect);
		}
		break;
	}

	case WM_SIZE: {
		SendMessage(hStatusBar, WM_SIZE, wp, lp);

		UINT dpi = GetDpiForWindow(hwnd);
		int parts[2] = { ScaleDpi(80, dpi), -1 };
		SendMessage(hStatusBar, SB_SETPARTS, 2, (LPARAM)parts);

		SendMessage(hStatusBar, SB_SETTEXTW, 0, (LPARAM)L"Selected file:");
		if (g_szSelectedFile[0] != L'\0') {
			SendMessage(hStatusBar, SB_SETTEXTW, 1, (LPARAM)g_szSelectedFile);
		}
		break;
	}


	case WM_DPICHANGED: {
		UINT newDpi = LOWORD(wp);
		RECT* const prcNewWindow = (RECT*)lp;

		// Apply the suggested new window size calculated by the OS
		SetWindowPos(hwnd, NULL,
			prcNewWindow->left, prcNewWindow->top,
			prcNewWindow->right - prcNewWindow->left,
			prcNewWindow->bottom - prcNewWindow->top,
			SWP_NOZORDER | SWP_NOACTIVATE);

		// --- LATITUDE ROW ---
		MoveWindow(hLatLabel, ScaleDpi(20, newDpi), ScaleDpi(15, newDpi), ScaleDpi(200, newDpi), ScaleDpi(18, newDpi), TRUE);
		MoveWindow(hLatDeg, ScaleDpi(20, newDpi), ScaleDpi(40, newDpi), ScaleDpi(45, newDpi), ScaleDpi(25, newDpi), TRUE);
		MoveWindow(hSymD1, ScaleDpi(67, newDpi), ScaleDpi(43, newDpi), ScaleDpi(15, newDpi), ScaleDpi(25, newDpi), TRUE);
		MoveWindow(hLatMin, ScaleDpi(85, newDpi), ScaleDpi(40, newDpi), ScaleDpi(40, newDpi), ScaleDpi(25, newDpi), TRUE);
		MoveWindow(hSymM1, ScaleDpi(127, newDpi), ScaleDpi(43, newDpi), ScaleDpi(15, newDpi), ScaleDpi(25, newDpi), TRUE);
		MoveWindow(hLatSec, ScaleDpi(145, newDpi), ScaleDpi(40, newDpi), ScaleDpi(65, newDpi), ScaleDpi(25, newDpi), TRUE);
		MoveWindow(hSymS1, ScaleDpi(212, newDpi), ScaleDpi(43, newDpi), ScaleDpi(15, newDpi), ScaleDpi(25, newDpi), TRUE);
		MoveWindow(hLatDir, ScaleDpi(230, newDpi), ScaleDpi(40, newDpi), ScaleDpi(30, newDpi), ScaleDpi(25, newDpi), TRUE);

		// --- LONGITUDE ROW ---
		MoveWindow(hLonLabel, ScaleDpi(300, newDpi), ScaleDpi(15, newDpi), ScaleDpi(200, newDpi), ScaleDpi(18, newDpi), TRUE);
		MoveWindow(hLonDeg, ScaleDpi(300, newDpi), ScaleDpi(40, newDpi), ScaleDpi(45, newDpi), ScaleDpi(25, newDpi), TRUE);
		MoveWindow(hSymD2, ScaleDpi(347, newDpi), ScaleDpi(43, newDpi), ScaleDpi(15, newDpi), ScaleDpi(25, newDpi), TRUE);
		MoveWindow(hLonMin, ScaleDpi(365, newDpi), ScaleDpi(40, newDpi), ScaleDpi(40, newDpi), ScaleDpi(25, newDpi), TRUE);
		MoveWindow(hSymM2, ScaleDpi(407, newDpi), ScaleDpi(43, newDpi), ScaleDpi(15, newDpi), ScaleDpi(25, newDpi), TRUE);
		MoveWindow(hLonSec, ScaleDpi(425, newDpi), ScaleDpi(40, newDpi), ScaleDpi(65, newDpi), ScaleDpi(25, newDpi), TRUE);
		MoveWindow(hSymS2, ScaleDpi(492, newDpi), ScaleDpi(43, newDpi), ScaleDpi(15, newDpi), ScaleDpi(25, newDpi), TRUE);
		MoveWindow(hLonDir, ScaleDpi(510, newDpi), ScaleDpi(40, newDpi), ScaleDpi(30, newDpi), ScaleDpi(25, newDpi), TRUE);

		// --- BUTTONS ---
		MoveWindow(hBtnApply, ScaleDpi(20, newDpi), ScaleDpi(95, newDpi), ScaleDpi(110, newDpi), ScaleDpi(32, newDpi), TRUE);
		MoveWindow(hBtnSelect, ScaleDpi(145, newDpi), ScaleDpi(95, newDpi), ScaleDpi(110, newDpi), ScaleDpi(32, newDpi), TRUE);

		// --- STATUS BAR ---
		// Force the status bar to recalculate its width relative to the parent window dimensions
		SendMessageW(hStatusBar, WM_SIZE, 0, 0);
		int parts[2] = { ScaleDpi(80, newDpi), -1 };
		SendMessageW(hStatusBar, SB_SETPARTS, 2, (LPARAM)parts);
		break;
	}


	case WM_CTLCOLORSTATIC: {
		HDC hdcStatic = (HDC)wp;
		SetBkMode(hdcStatic, TRANSPARENT);
		return (LRESULT)GetSysColorBrush(COLOR_3DFACE);
	}
	//Stupid button fix. DO NOT REMOVE OTHERWISE YOU GET WHITE BOXES/HALOS AROUND THE BUTTONS!!!!!!!!!!!!!!!
	case WM_CTLCOLORBTN: {
		return (LRESULT)GetSysColorBrush(COLOR_3DFACE);
	}
	case WM_COMMAND: {
		int controlId = LOWORD(wp);
		int notificationCode = HIWORD(wp);

		if (g_IsParsingPaste) break;

		if (notificationCode == BN_CLICKED) {
			if (controlId == IDC_SELECT) {
				if (SelectImageFile(hwnd)) {//SelectImageFile is defined in select.c "entery point"
					SendMessage(hStatusBar, SB_SETTEXTW, 1, (LPARAM)g_szSelectedFile);
					SetFocus(hLatDeg);
				}
			}
			else if (controlId == IDC_APPLY) {
				if (lstrlenW(g_szSelectedFile) == 0) {
					MessageBoxW(hwnd, L"Please select an image file first!", L"No File Selected", MB_OK | MB_ICONWARNING);
				}
				else {
					//wic_geotag.c "entry point"
					ApplyGeotag(hwnd, g_szSelectedFile);
				}
			}
		}


		//Auto-jump to the next field when the current one is filled
		if (notificationCode == EN_CHANGE) {
			if (controlId == IDC_LAT_DEG && GetWindowTextLength(hLatDeg) == 2) { SetFocus(hLatMin); SendMessage(hLatMin, EM_SETSEL, 0, -1); }
			if (controlId == IDC_LAT_MIN && GetWindowTextLength(hLatMin) == 2) { SetFocus(hLatSec); SendMessage(hLatSec, EM_SETSEL, 0, -1); }
			if (controlId == IDC_LAT_DIR && GetWindowTextLength(hLatDir) == 1) { SetFocus(hLonDeg); SendMessage(hLonDeg, EM_SETSEL, 0, -1); }
			if (controlId == IDC_LON_DEG && GetWindowTextLength(hLonDeg) == 3) { SetFocus(hLonMin); SendMessage(hLonMin, EM_SETSEL, 0, -1); }
			if (controlId == IDC_LON_MIN && GetWindowTextLength(hLonMin) == 2) { SetFocus(hLonSec); SendMessage(hLonSec, EM_SETSEL, 0, -1); }
			
			if (controlId == IDC_LON_DIR && GetWindowTextLength(hLonDir) == 1) {
				//Have hBtnSelect be a regular button again
				SendMessage(hBtnSelect, BM_SETSTYLE, BS_PUSHBUTTON, TRUE);

				//Have hBtnApply be the default button so that pressing Enter will trigger it
				SendMessage(hBtnApply, BM_SETSTYLE, BS_DEFPUSHBUTTON, TRUE);
				SendMessage(hwnd, DM_SETDEFID, IDC_APPLY, 0);

				SetFocus(hBtnApply);
			}
		}
		break;
	}

	case WM_DESTROY:
		DeleteObject(hModernFont);
		PostQuitMessage(0);
		return 0;

	default:
		return DefWindowProc(hwnd, uMsg, wp, lp);
	}
	return 0;
}


static BOOL validation(HWND hWnd, UINT uMsg, WPARAM wp) {
	if (uMsg == WM_CHAR && (hWnd == hLatDeg || hWnd == hLonDeg)) {
		wchar_t ch = (wchar_t)wp;
		if (ch >= L'0' && ch <= L'9') {
			wchar_t currentText[4] = { 0 };
			wchar_t predictedText[5] = { 0 };
			GetWindowTextW(hWnd, currentText, 4);

			DWORD startSel = 0, endSel = 0;
			SendMessage(hWnd, EM_GETSEL, (WPARAM)&startSel, (LPARAM)&endSel);

			int len = lstrlenW(currentText);
			int pIdx = 0;
			for (int i = 0; i < len; i++) {
				if (i == (int)startSel) predictedText[pIdx++] = ch;
				if (i < (int)startSel || i >= (int)endSel) predictedText[pIdx++] = currentText[i];
			}
			if ((int)startSel == len) predictedText[pIdx++] = ch;
			predictedText[pIdx] = L'\0';

			int predictedVal = CustomWcharToInt(predictedText);
			int maxLimit = (hWnd == hLatDeg) ? 90 : 180;

			if (predictedVal >= maxLimit) {
				EDITBALLOONTIP ebt = { 0 };
				ebt.cbStruct = sizeof(EDITBALLOONTIP);
				ebt.pszTitle = L"Value Out of Range";
				ebt.pszText = (hWnd == hLatDeg) ? L"Latitude degrees must be strictly less than 90°." : L"Longitude degrees must be strictly less than 180°.";
				ebt.ttiIcon = TTI_ERROR;
				SendMessage(hWnd, EM_SHOWBALLOONTIP, 0, (LPARAM)&ebt);
				MessageBeep(MB_ICONINFORMATION);
				return TRUE;
			}
		}
	}
	if (uMsg == WM_CHAR && (hWnd == hLatMin || hWnd == hLonMin)) {
		wchar_t ch = (wchar_t)wp;
		if (ch >= L'0' && ch <= L'9') {
			wchar_t currentText[4] = { 0 };
			wchar_t predictedText[5] = { 0 };
			GetWindowTextW(hWnd, currentText, 4);

			DWORD startSel = 0, endSel = 0;
			SendMessage(hWnd, EM_GETSEL, (WPARAM)&startSel, (LPARAM)&endSel);

			int len = lstrlenW(currentText);
			int pIdx = 0;
			for (int i = 0; i < len; i++) {
				if (i == (int)startSel) predictedText[pIdx++] = ch;
				if (i < (int)startSel || i >= (int)endSel) predictedText[pIdx++] = currentText[i];
			}
			if ((int)startSel == len) predictedText[pIdx++] = ch;
			predictedText[pIdx] = L'\0';

			int predictedVal = CustomWcharToInt(predictedText);
			if (predictedVal >= 60) {
				EDITBALLOONTIP ebt = { 0 };
				ebt.cbStruct = sizeof(EDITBALLOONTIP);
				ebt.pszTitle = L"Value Out of Range";
				ebt.pszText = L"Minutes must be strictly less than 60'.";
				ebt.ttiIcon = TTI_ERROR;
				SendMessage(hWnd, EM_SHOWBALLOONTIP, 0, (LPARAM)&ebt);
				MessageBeep(MB_ICONINFORMATION);
				return TRUE;
			}
		}
	}
	if (uMsg == WM_CHAR && (hWnd == hLatSec || hWnd == hLonSec)) {
		wchar_t ch = (wchar_t)wp;
		if (ch != VK_BACK && ch != L'.' && (ch < L'0' || ch > L'9')) {
			EDITBALLOONTIP ebt = { 0 };
			ebt.cbStruct = sizeof(EDITBALLOONTIP);
			ebt.pszTitle = L"Unacceptable Character";
			ebt.pszText = L"You can only type a decimal here.";
			ebt.ttiIcon = TTI_ERROR;

			SendMessage(hWnd, EM_SHOWBALLOONTIP, 0, (LPARAM)&ebt);
			MessageBeep(MB_ICONINFORMATION);
			return TRUE;
		}

		wchar_t currentText[12] = { 0 };
		wchar_t predictedText[13] = { 0 };
		GetWindowTextW(hWnd, currentText, 12);
		if (ch == L'.' && CustomWcharFindChar(currentText, L'.') != NULL) {
			EDITBALLOONTIP ebt = { 0 };
			ebt.cbStruct = sizeof(EDITBALLOONTIP);
			ebt.pszTitle = L"Invalid Format";
			ebt.pszText = L"The seconds field can only contain one decimal point.";
			ebt.ttiIcon = TTI_ERROR;
			SendMessage(hWnd, EM_SHOWBALLOONTIP, 0, (LPARAM)&ebt);
			return TRUE;
		}

		DWORD startSel = 0, endSel = 0;
		SendMessage(hWnd, EM_GETSEL, (WPARAM)&startSel, (LPARAM)&endSel);

		int len = lstrlenW(currentText);
		int pIdx = 0;

		for (int i = 0; i < len; i++) {
			if (i == (int)startSel) predictedText[pIdx++] = ch;
			if (i < (int)startSel || i >= (int)endSel) predictedText[pIdx++] = currentText[i];
		}
		if ((int)startSel == len) predictedText[pIdx++] = ch;
		predictedText[pIdx] = L'\0';

		// Float-Free String Validation: Evaluate string via integer lengths to check boundary limitations (< 60.0)
		int wholeSeconds = 0;
		const wchar_t* pDot = CustomWcharFindChar(predictedText, L'.');
		if (pDot) {
			wchar_t szWholeTemp[16] = { 0 };
			int wholeLen = (int)(pDot - predictedText);
			if (wholeLen < 16) {
				for (int k = 0; k < wholeLen; k++) szWholeTemp[k] = predictedText[k];
				wholeSeconds = CustomWcharToInt(szWholeTemp);
			}
		}
		else {
			wholeSeconds = CustomWcharToInt(predictedText);
		}

		if (wholeSeconds >= 60) {
			EDITBALLOONTIP ebt = { 0 };
			ebt.cbStruct = sizeof(EDITBALLOONTIP);
			ebt.pszTitle = L"Value Out of Range";
			ebt.pszText = L"Seconds must be strictly less than 60\".";
			ebt.ttiIcon = TTI_ERROR;
			SendMessage(hWnd, EM_SHOWBALLOONTIP, 0, (LPARAM)&ebt);
			MessageBeep(MB_ICONINFORMATION);
			return TRUE;// Reject the keystroke completely
		}
	}
	// 3. STRICT DIRECTION LOCK: Only allow N/S for Latitude Direction
	if (uMsg == WM_CHAR && hWnd == hLatDir) {
		wchar_t ch = (wchar_t)wp;
		if (ch != VK_BACK && ch != L'N' && ch != L'n' && ch != L'S' && ch != L's') {
			EDITBALLOONTIP ebt = { 0 };
			ebt.cbStruct = sizeof(EDITBALLOONTIP);
			ebt.pszTitle = L"Value Out of Range";
			ebt.pszText = L"Latitude direction must be N or S";
			ebt.ttiIcon = TTI_ERROR;
			SendMessage(hWnd, EM_SHOWBALLOONTIP, 0, (LPARAM)&ebt);
			MessageBeep(MB_ICONINFORMATION);
			return TRUE;// Reject the keystroke completely
		}
	}
	// 4. STRICT DIRECTION LOCK: Only allow E/W for Longitude Direction
	if (uMsg == WM_CHAR && hWnd == hLonDir) {
		wchar_t ch = (wchar_t)wp;
		if (ch != VK_BACK && ch != L'E' && ch != L'e' && ch != L'W' && ch != L'w') {
			EDITBALLOONTIP ebt = { 0 };
			ebt.cbStruct = sizeof(EDITBALLOONTIP);
			ebt.pszTitle = L"Value Out of Range";
			ebt.pszText = L"Longitude direction must be E or W";
			ebt.ttiIcon = TTI_ERROR;
			SendMessage(hWnd, EM_SHOWBALLOONTIP, 0, (LPARAM)&ebt);
			MessageBeep(MB_ICONINFORMATION);
			return TRUE;// Reject the keystroke completely
		}
	}
	return FALSE;
}



static BOOL IsValOutOfRange(const wchar_t* str, int maxLimit) {
	if (!str || str[0] == L'\0') return FALSE;
	int val = CustomWcharToInt(str);
	return (val >= maxLimit);
}

// Helper for seconds (handles decimal points)
static BOOL IsSecOutOfRange(const wchar_t* str) {
	if (!str || str[0] == L'\0') return FALSE;
	int wholeSeconds = 0;
	const wchar_t* pDot = CustomWcharFindChar(str, L'.');
	if (pDot) {
		wchar_t szWholeTemp[16] = { 0 };
		int wholeLen = (int)(pDot - str);
		if (wholeLen < 16) {
			for (int k = 0; k < wholeLen; k++) szWholeTemp[k] = str[k];
			wholeSeconds = CustomWcharToInt(szWholeTemp);
		}
	}
	else {
		wholeSeconds = CustomWcharToInt(str);
	}
	return (wholeSeconds >= 60);
}



LRESULT CALLBACK SharedEditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wp, LPARAM lp, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
	/*(void)uIdSubclass;
	(void)dwRefData;*/

	//Backsapce autojump
	if (uMsg == WM_CHAR && wp == VK_BACK) {
		if (GetWindowTextLength(hWnd) == 0) {
			HWND hTarget = NULL;

			if (hWnd == hLatMin)      hTarget = hLatDeg;
			else if (hWnd == hLatSec) hTarget = hLatMin;
			else if (hWnd == hLatDir) hTarget = hLatSec;
			else if (hWnd == hLonDeg) hTarget = hLatDir;
			else if (hWnd == hLonMin) hTarget = hLonDeg;
			else if (hWnd == hLonSec) hTarget = hLonMin;
			else if (hWnd == hLonDir) hTarget = hLonSec;

			if (hTarget == hLatDir) {
				SetFocus(hTarget);
				SendMessage(hTarget, EM_SETSEL, 0, -1);//Selects the entire box during backspace so you can just change the letter without deleting it if you want
				return 0;
			}
			else if (hTarget != NULL) {
				SetFocus(hTarget);
				int len = GetWindowTextLength(hTarget);
				SendMessage(hTarget, EM_SETSEL, len, len);
				return 0;
			}
		}
	}

	if (validation(hWnd, uMsg, wp)) {
		return TRUE; // Block character processing
	}

	//Custom clipboard paste handling for our coordinate format: "DD°MM'SS.SSS\"H, DDD°MM'SS.SSS\"H" (H = Hemisphere)
	if (uMsg == WM_PASTE) {
		if (OpenClipboard(NULL)) {
			HANDLE hData = GetClipboardData(CF_UNICODETEXT);
			if (hData != NULL) {
				wchar_t* pszText = (wchar_t*)GlobalLock(hData);
				if (pszText != NULL) {
					g_IsParsingPaste = TRUE;

					wchar_t latD[4] = { 0 }, latM[3] = { 0 }, latS[8] = { 0 }, latRef[2] = { 0 };
					wchar_t lonD[4] = { 0 }, lonM[3] = { 0 }, lonS[8] = { 0 }, lonRef[2] = { 0 };

					int i = 0;

					// --- Parse Latitude ---
					int idx = 0;
					while (pszText[i] && pszText[i] != L'°' && idx < 3) { if (pszText[i] >= '0' && pszText[i] <= '9') latD[idx++] = pszText[i]; i++; }
					if (pszText[i] == L'°') i++;

					idx = 0;
					while (pszText[i] && pszText[i] != L'\'' && idx < 2) { if (pszText[i] >= '0' && pszText[i] <= '9') latM[idx++] = pszText[i]; i++; }
					if (pszText[i] == L'\'') i++;

					idx = 0;
					while (pszText[i] && pszText[i] != L'\"' && idx < 7) {
						if ((pszText[i] >= '0' && pszText[i] <= '9') || pszText[i] == L'.') latS[idx++] = pszText[i];
						i++;
					}
					if (pszText[i] == L'\"') i++;

					while (pszText[i] && pszText[i] != L' ') {
						if (pszText[i] == 'N' || pszText[i] == 'n' || pszText[i] == 'S' || pszText[i] == 's') { latRef[0] = pszText[i]; break; }
						i++;
					}

					// --- Skip Spaces to find Longitude start ---
					while (pszText[i] && (pszText[i] == L' ' || pszText[i] == L',')) i++;

					// --- Parse Longitude ---
					idx = 0;
					while (pszText[i] && pszText[i] != L'°' && idx < 3) { if (pszText[i] >= '0' && pszText[i] <= '9') lonD[idx++] = pszText[i]; i++; }
					if (pszText[i] == L'°') i++;

					idx = 0;
					while (pszText[i] && pszText[i] != L'\'' && idx < 2) { if (pszText[i] >= '0' && pszText[i] <= '9') lonM[idx++] = pszText[i]; i++; }
					if (pszText[i] == L'\'') i++;

					idx = 0;
					while (pszText[i] && pszText[i] != L'\"' && idx < 7) {
						if ((pszText[i] >= '0' && pszText[i] <= '9') || pszText[i] == L'.') lonS[idx++] = pszText[i];
						i++;
					}
					if (pszText[i] == L'\"') i++;

					while (pszText[i]) {
						if (pszText[i] == 'E' || pszText[i] == 'e' || pszText[i] == 'W' || pszText[i] == 'w') { lonRef[0] = pszText[i]; break; }
						i++;
					}

					if (IsValOutOfRange(latD, 90) || IsValOutOfRange(lonD, 180) || IsValOutOfRange(latM, 60) || IsValOutOfRange(lonM, 60) || IsSecOutOfRange(latS) || IsSecOutOfRange(lonS)){
						EDITBALLOONTIP ebt = { 0 };
						ebt.cbStruct = sizeof(EDITBALLOONTIP);
						ebt.pszTitle = L"Pasted Value Out of Range";
						ebt.pszText = L"The coordinates in your clipboard contain out-of-range values.";
						ebt.ttiIcon = TTI_ERROR;

						SendMessage(hWnd, EM_SHOWBALLOONTIP, 0, (LPARAM)&ebt);
						MessageBeep(MB_ICONINFORMATION);

						GlobalUnlock(hData);
						CloseClipboard();
						return 0; // Abort paste execution
					}

					SetWindowTextW(hLatDeg, latD); SetWindowTextW(hLatMin, latM); SetWindowTextW(hLatSec, latS); SetWindowTextW(hLatDir, latRef);
					SetWindowTextW(hLonDeg, lonD); SetWindowTextW(hLonMin, lonM); SetWindowTextW(hLonSec, lonS); SetWindowTextW(hLonDir, lonRef);
					
					SetFocus(hLonDir);
					SendMessage(hLonDir, EM_SETSEL, 0, -1);//Actually bring the cursor to the end. We also want it highlighted so you can just change direction without backspace.
					g_IsParsingPaste = FALSE;
					GlobalUnlock(hData);
				}
			}
			CloseClipboard();
		}
		return 0;
	}
	return DefSubclassProc(hWnd, uMsg, wp, lp);
}
