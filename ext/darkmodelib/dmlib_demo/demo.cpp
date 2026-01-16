// SPDX-License-Identifier: MPL-2.0

/*
 * Copyright (c) 2025 ozone10
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

// This file is part of darkmodelib demo.

#include "demo.h"

#include <windows.h>

#include <commctrl.h>
#include <richedit.h>

#include <algorithm>
#include <array>
#include <string>

#if !defined(DMLIB_DLL)
#include "DarkModeSubclass.h"
#else
#include "dmlib_dll_helper.h"
#endif

#include "resource.h"

#define DMLIB_FAST_FAIL 0

#if defined(DMLIB_DLL)

// Example of how to define your own dummy function

static HRESULT MyDummyDarkTaskDialogIndirect(
	const TASKDIALOGCONFIG* pTaskConfig,
	int* pnButton,
	int* pnRadioButton,
	BOOL* pfVerificationFlagChecked
)
{
	return ::TaskDialogIndirect(pTaskConfig, pnButton, pnRadioButton, pfVerificationFlagChecked);
}

// Example of how to define your function loader

bool DarkMode::loadDarkModeFunctionsFromDll(const wchar_t* dllName)
{
	wchar_t fullPath[MAX_PATH]{};
	// workaround for SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
	if (!dmlib_module::getModuleFullPath(dllName, fullPath))
	{
		// failed to get correct path (e.g too long)
		::MessageBoxW(nullptr, fullPath, dllName, MB_OK);
#if DMLIB_FAST_FAIL > 0
		return false;
#endif
	}

	HMODULE hDll = ::LoadLibraryExW(fullPath, nullptr, 0);
	if (hDll == nullptr)
	{
		::MessageBoxW(nullptr, L"Cannot load dll.", dllName, MB_OK);
#if DMLIB_FAST_FAIL > 0
		return false;
#endif
	}

#define LOAD_FN(fn, dummy) dmlib_module::LoadFn(hDll, fn, #fn, dummy)

	LOAD_FN(initDarkMode, DummyInitDarkMode);
	LOAD_FN(setDarkModeConfigEx, DummySetDarkModeConfigEx);
	LOAD_FN(setDefaultColors, DummySetDefaultColors);
	LOAD_FN(isExperimentalActive, DummyIsExperimentalActive);
	LOAD_FN(setWindowExStyle, DummySetWindowExStyle);
	LOAD_FN(isEnabled, DummyIsEnabled);
	LOAD_FN(setDarkTitleBarEx, DummySetDarkTitleBarEx);
	LOAD_FN(setChildCtrlsTheme, DummySetChildCtrlsTheme);
	LOAD_FN(disableVisualStyle, DummyDisableVisualStyle);
	LOAD_FN(setDarkTaskDlg, DummySetDarkTaskDlg);
	LOAD_FN(setColorizeTitleBarConfig, DummySetColorizeTitleBarConfig);
	LOAD_FN(setDarkWndNotifySafe, DummySetDarkWndNotifySafe);
	LOAD_FN(setWindowEraseBgSubclass, DummySetWindowEraseBgSubclass);
	LOAD_FN(setWindowMenuBarSubclass, DummySetWindowMenuBarSubclass);
	LOAD_FN(HookDlgProc, DummyHookDlgProc);
	LOAD_FN(darkTaskDialogIndirect, MyDummyDarkTaskDialogIndirect);
	LOAD_FN(setDarkWndNotifySafe, DummySetDarkWndNotifySafe);
	LOAD_FN(getLibInfo, DummyGetLibInfo);

	return true;
}
#endif

// dpi functions

static inline UINT WINAPI MyGetDpiForSystem()
{
	UINT dpi = USER_DEFAULT_SCREEN_DPI;
	HDC hdc = ::GetDC(nullptr);
	if (hdc != nullptr)
	{
		dpi = ::GetDeviceCaps(hdc, LOGPIXELSX);
		::ReleaseDC(nullptr, hdc);
	}
	return dpi;
}

[[nodiscard]] static inline int Scale(int x)
{
	static const UINT dpi = MyGetDpiForSystem();
	return ::MulDiv(x, dpi, USER_DEFAULT_SCREEN_DPI);
}

//

static constexpr size_t MAX_LOADSTRING = 32;

// Global Variables:
HINSTANCE g_hInst = nullptr;                                    // Current instance
std::wstring g_szTitle(MAX_LOADSTRING, L'\0');                  // The title bar text
std::wstring g_szWindowClass(MAX_LOADSTRING, L'\0');            // The main window class name

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	[[maybe_unused]] _In_opt_ HINSTANCE /*hPrevInstance*/,
	[[maybe_unused]] _In_ LPWSTR /*lpCmdLine*/,
	_In_ int nShowCmd)
{
	SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
	SetDllDirectoryW(L"");

#if defined(DMLIB_DLL)
	if (!DarkMode::loadDarkModeFunctionsFromDll(L"darkmode.dll"))
	{
#if DMLIB_FAST_FAIL > 0
		return FALSE;
#endif
	}
#endif

	DarkMode::initDarkMode();
	DarkMode::setDarkModeConfigEx(static_cast<UINT>(DarkMode::DarkModeType::dark));
	DarkMode::setDefaultColors(true);

	// Initialize global strings
	LoadStringW(hInstance, IDS_APP_TITLE, g_szTitle.data(), MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_DEMO, g_szWindowClass.data(), MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	HWND hWnd = nullptr;

	// Perform application initialization:
	if (InitInstance(hInstance, nShowCmd, hWnd) == FALSE)
	{
		return FALSE;
	}

	HACCEL hAccelTable = LoadAcceleratorsW(hInstance, MAKEINTRESOURCE(IDC_DEMO));

	MSG msg{};

	// Main message loop:
	while (GetMessageW(&msg, nullptr, 0, 0) > 0)
	{
		if (IsDialogMessageW(hWnd, &msg) <= 0 && TranslateAcceleratorW(msg.hwnd, hAccelTable, &msg) <= 0)
		{
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}

	return static_cast<int>(msg.wParam);
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex{};

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIconW(hInstance, MAKEINTRESOURCE(IDI_DEMO));
	wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
	wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_DEMO);
	wcex.lpszClassName = g_szWindowClass.c_str();
	wcex.hIconSm = nullptr;

	return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int, HMND&)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow, HWND& hMain)
{
	g_hInst = hInstance; // Store instance handle in our global variable

	hMain = CreateWindowExW(0, g_szWindowClass.c_str(), g_szTitle.c_str(), WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, Scale(970), Scale(550), nullptr, nullptr, hInstance, nullptr);

	if (hMain == nullptr)
	{
		return FALSE;
	}

	ShowWindow(hMain, nCmdShow);
	UpdateWindow(hMain);

	return TRUE;
}

enum class IdCtrl : WORD
{
	none = 0,
	useTabstop = 1000,
	rebar,
	toolbarBtn = 200,
	toolbarMix,
	push = 111,
	pushDisabled,
	pushDef,
	split,
	splitDisabled,
	splitDef,
	checkUnchecked,
	checkUncheckedDisabled,
	checkChecked,
	checkCheckedDisabled,
	triState,
	triStateDisabled,
	progressNormal = 1023,
	progressError,
	progressPaused,
	progressMarquee,
	radioUnset = 127,
	radioUnsetDisabled,
	radioSet,
	radioSetDisabled,
	editNormal,
	editDisabled,
	editPass,
	editReadOnly,
	comboDrop,
	comboDropDisabled,
	comboList,
	comboListDisabled,
	syslink = 1039,
	upDownEdit = 140,
	upDown = 1041,
	trackbar = 142,
	tabcontrol,
	listbox,
	ipAddress,
	listview,
	listviewGrid,
	treeview,
	hotkey,
	richEdit,
	scrollH = 1049,
	scrollV,
	statusbar
};

static void CheckModeMenu(HWND hWnd, UINT checkID)
{
	HMENU hMenuBar = GetMenu(hWnd);
	HMENU hModeSubMenu = GetSubMenu(hMenuBar, 1);
	CheckMenuRadioItem(hModeSubMenu, IDM_DARK, IDM_CLASSIC, checkID, MF_BYCOMMAND | MF_CHECKED);
}

static void SelectAndRefreshMode(HWND hWnd, UINT checkID)
{
	CheckModeMenu(hWnd, checkID);

	UINT dmType = 1;
	switch (checkID)
	{
		case IDM_LIGHT:
		{
			if (!DarkMode::isExperimentalActive() && DarkMode::isEnabled())
			{
				return;
			}

			dmType = 0;
			break;
		}

		case IDM_CLASSIC:
		{
			if (!DarkMode::isEnabled())
			{
				return;
			}

			dmType = 3;
			break;
		}

		case IDM_DARK:
		{
			if (DarkMode::isExperimentalActive())
			{
				return;
			}

			[[fallthrough]];
		}
		default:
		{
			dmType = 1;
			break;
		}
	}

	DarkMode::setDarkModeConfigEx(dmType);
	DarkMode::setDefaultColors(true);
	DarkMode::setDarkTitleBarEx(hWnd, true);
	DarkMode::setChildCtrlsTheme(hWnd);

	if (checkID == IDM_CLASSIC)
	{
		DarkMode::disableVisualStyle(GetDlgItem(hWnd, static_cast<int>(IdCtrl::treeview)), false);
	}

	RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW | RDW_FRAME);
}

enum TaskDlgBtnID
{
	radio_1 = 100,
	radio_2,
	radio_3,
	radio_4,
	cmd_1,
	cmd_2,
	cmd_3,
	cmd_4,
	cmd_5
};

static HRESULT CALLBACK TaskDlgCallback(
	HWND hWnd,
	UINT msg,
	[[maybe_unused]] WPARAM wParam,
	[[maybe_unused]] LPARAM lParam,
	[[maybe_unused]] LONG_PTR lpRefData
)
{
	switch (msg)
	{
		case TDN_CREATED:
		{
			SendMessageW(hWnd, TDM_ENABLE_RADIO_BUTTON, static_cast<WPARAM>(radio_2), FALSE);
			SendMessageW(hWnd, TDM_SET_PROGRESS_BAR_MARQUEE, TRUE, 0);

			SendMessageW(hWnd, TDM_ENABLE_BUTTON, static_cast<WPARAM>(cmd_2), FALSE);
			SendMessageW(hWnd, TDM_SET_BUTTON_ELEVATION_REQUIRED_STATE, static_cast<WPARAM>(cmd_3), TRUE);
			SendMessageW(hWnd, TDM_ENABLE_BUTTON, static_cast<WPARAM>(cmd_4), FALSE);
			SendMessageW(hWnd, TDM_SET_BUTTON_ELEVATION_REQUIRED_STATE, static_cast<WPARAM>(cmd_4), TRUE);
			break;
		}

		case TDN_DIALOG_CONSTRUCTED:
		{
			DarkMode::setDarkTaskDlg(hWnd);
			break;
		}

		default:
		{
			break;
		}
	}
	return S_OK;
}

static HMODULE hModRich = nullptr;

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_CREATE:
		{
			DarkMode::setWindowExStyle(hWnd, true, WS_EX_COMPOSITED);

			CheckModeMenu(hWnd, IDM_DARK);

			static HFONT hFont = nullptr;

			NONCLIENTMETRICS ncm{};
			ncm.cbSize = sizeof(NONCLIENTMETRICS);
			if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0) > 0)
			{
				hFont = CreateFontIndirectW(&ncm.lfMessageFont);
			}

			RECT rcClient{};
			GetClientRect(hWnd, &rcClient);

			auto createCtrl = [&](LPCWSTR lpClassName, LPCWSTR lpWindowName, DWORD dwStyle,
				int x, int y, int w, int h,
				IdCtrl id = IdCtrl::none, DWORD dwExStyle = 0, HWND hParent = nullptr) -> HWND
			{
				HWND hCtrl = CreateWindowExW(
					dwExStyle, lpClassName, lpWindowName,
					((id > IdCtrl::none && id < IdCtrl::useTabstop) ? WS_TABSTOP : 0) | WS_VISIBLE | WS_CHILDWINDOW | dwStyle,
					x, y, w, h,
					(hParent == nullptr) ? hWnd : hParent,
					(id != IdCtrl::none) ? reinterpret_cast<HMENU>(id) : nullptr,
					nullptr, nullptr
				);

				SendMessageW(hCtrl, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);

				return hCtrl;
			};

			static const int yRow = Scale(40);

			static const int xPosCtrl = Scale(10);
			static const int yPosCtrl = Scale(20);
			static const int yPosCtrlEnd = Scale(6);
			static const int xGap = Scale(10);
			static const int yGap = Scale(4);

			static const int heightCtrl = Scale(20);
			static const int heightCtrlGap = Scale(2);
			static const int heightProgressGap = Scale(5);
			static const int heightPush = Scale(23);
			static const int heightPushGap = Scale(4);
			static const int heightEdit = Scale(22);
			static const int heightEditGap = Scale(6);
			static const int heightTrackbar = Scale(30);
			static const int heightTabCtrl = Scale(76);
			static const int heightListBox = Scale(100);
			static const int heightListView = Scale(98);
			static const int heightTreeView = Scale(90);
			static const int heightRichEdit = Scale(98);

			static const int xPos1stCol = 10;
			static const int xPos1stColCtrl = xPos1stCol + xPosCtrl;

			static const int wGroup1stCol = Scale(230);
			static const int wGroup2ndCol = Scale(160);
			static const int wGroup3rdCol = Scale(160);
			static const int wGroup4thCol = Scale(160);
			static const int wGroup5thCol = Scale(160);

			static const int wBtn = (wGroup1stCol - (2 * xPosCtrl) - xGap) / 2;
			static const int wCheck = wBtn * 2;

			static const int wProgressLabel = (wGroup1stCol - (2 * xPosCtrl) - xGap) / 4;
			static const int wProgress = ((wGroup1stCol - (2 * xPosCtrl) - xGap) / 4) * 3;

			static const int xProgress = xPosCtrl + wProgressLabel + (xGap * 2);

			static const int wEditCombo = wGroup2ndCol - (2 * xPosCtrl);

			static const int wCtrl3rdCol = wGroup3rdCol - (2 * xPosCtrl);
			static const int wCtrl4thCol = wGroup4thCol - (2 * xPosCtrl);
			static const int wCtrl5thCol = wGroup5thCol - (2 * xPosCtrl);

			static const int heightGBPush = yPosCtrl + ((heightPush + heightPushGap) * 3) + yPosCtrlEnd;
			static const int heightGBCheck = yPosCtrl + ((heightCtrl + heightCtrlGap) * 6) + yPosCtrlEnd;
			static const int heightGBProgress = yPosCtrl + ((heightCtrl + heightProgressGap) * 4) + yPosCtrlEnd;

			static const int heightGBRadio = yPosCtrl + ((heightCtrl + heightCtrlGap) * 4) + yPosCtrlEnd;
			static const int heightGBEdit = yPosCtrl + ((heightEdit + heightEditGap) * 4) + yPosCtrlEnd;
			static const int heightGBCombo = yPosCtrl + ((heightEdit + heightEditGap) * 4) + yPosCtrlEnd;

			static const int heightGBLink = yPosCtrl + ((heightCtrl + heightCtrlGap) * 1) + yPosCtrlEnd;
			static const int heightGBUpDown = yPosCtrl + ((heightEdit + heightEditGap) * 1) + yPosCtrlEnd;
			static const int heightGBTrackbar = yPosCtrl + ((heightTrackbar + heightEditGap) * 1) + yPosCtrlEnd;

			static const int heightGBTabCtrl = yPosCtrl + ((heightTabCtrl + heightEditGap) * 1) + yPosCtrlEnd;

			static const int heightGBIP = yPosCtrl + ((heightCtrl + heightEditGap) * 1) + yPosCtrlEnd;

			static const int heightGBHotKey = yPosCtrl + ((heightCtrl + heightEditGap) * 1) + yPosCtrlEnd;
			static const int heightGBRichEdit = yPosCtrl + ((heightRichEdit + heightEditGap) * 1) + yPosCtrlEnd;

			static const int yGBCheck = yRow + heightGBPush + yGap;
			static const int yGBProgress = yGBCheck + heightGBCheck + yGap;

			static const int yGBEdit = yRow + heightGBRadio + yGap;
			static const int yGBCombo = yGBEdit + heightGBEdit + yGap;

			static const int yGBUpDown = yRow + heightGBLink + yGap;
			static const int yGBTrackbar = yGBUpDown + heightGBUpDown + yGap;

			static const int yGBTabCtrl = yGBTrackbar + heightGBTrackbar + yGap;
			static const int ySTListBox = yGBTabCtrl + heightGBTabCtrl + yGap;

			static const int ySTListView = yRow + heightGBIP + yGap;

			static const int ySTTreeView = ySTListView + yPosCtrl + ((heightListView + yPosCtrlEnd) * 2) + yGap;

			static const int yGBRichEdit = yRow + heightGBHotKey + yGap;

			static const int xPosSplit = xPos1stColCtrl + xGap + wBtn;

			static const int xPos2ndCol = xPos1stCol + xGap + wGroup1stCol;
			static const int xPos2ndColCtrl = xPos2ndCol + xPosCtrl;

			static const int xPos3rdCol = xPos2ndCol + xGap + wGroup2ndCol;
			static const int xPos3rdColCtrl = xPos3rdCol + xPosCtrl;

			static const int xPos4thCol = xPos3rdCol + xGap + wGroup3rdCol;
			static const int xPos4thColCtrl = xPos4thCol + xPosCtrl;

			static const int xPos5thCol = xPos4thCol + xGap + wGroup4thCol;
			static const int xPos5thColCtrl = xPos5thCol + xPosCtrl;

			static const int yPush1 = yRow + yPosCtrl + ((heightPush + heightPushGap) * 0);
			static const int yPush2 = yRow + yPosCtrl + ((heightPush + heightPushGap) * 1);
			static const int yPush3 = yRow + yPosCtrl + ((heightPush + heightPushGap) * 2);

			static const int yCheck1 = yGBCheck + yPosCtrl + ((heightCtrl + heightCtrlGap) * 0);
			static const int yCheck2 = yGBCheck + yPosCtrl + ((heightCtrl + heightCtrlGap) * 1);
			static const int yCheck3 = yGBCheck + yPosCtrl + ((heightCtrl + heightCtrlGap) * 2);
			static const int yCheck4 = yGBCheck + yPosCtrl + ((heightCtrl + heightCtrlGap) * 3);
			static const int yCheck5 = yGBCheck + yPosCtrl + ((heightCtrl + heightCtrlGap) * 4);
			static const int yCheck6 = yGBCheck + yPosCtrl + ((heightCtrl + heightCtrlGap) * 5);

			static const int yProgress1 = yGBProgress + yPosCtrl + ((heightCtrl + heightProgressGap) * 0);
			static const int yProgress2 = yGBProgress + yPosCtrl + ((heightCtrl + heightProgressGap) * 1);
			static const int yProgress3 = yGBProgress + yPosCtrl + ((heightCtrl + heightProgressGap) * 2);
			static const int yProgress4 = yGBProgress + yPosCtrl + ((heightCtrl + heightProgressGap) * 3);

			static const int yRadio1 = yRow + yPosCtrl + ((heightCtrl + heightCtrlGap) * 0);
			static const int yRadio2 = yRow + yPosCtrl + ((heightCtrl + heightCtrlGap) * 1);
			static const int yRadio3 = yRow + yPosCtrl + ((heightCtrl + heightCtrlGap) * 2);
			static const int yRadio4 = yRow + yPosCtrl + ((heightCtrl + heightCtrlGap) * 3);

			static const int yEdit1 = yGBEdit + yPosCtrl + ((heightEdit + heightEditGap) * 0);
			static const int yEdit2 = yGBEdit + yPosCtrl + ((heightEdit + heightEditGap) * 1);
			static const int yEdit3 = yGBEdit + yPosCtrl + ((heightEdit + heightEditGap) * 2);
			static const int yEdit4 = yGBEdit + yPosCtrl + ((heightEdit + heightEditGap) * 3);

			static const int yCombo1 = yGBCombo + yPosCtrl + ((heightEdit + heightEditGap) * 0);
			static const int yCombo2 = yGBCombo + yPosCtrl + ((heightEdit + heightEditGap) * 1);
			static const int yCombo3 = yGBCombo + yPosCtrl + ((heightEdit + heightEditGap) * 2);
			static const int yCombo4 = yGBCombo + yPosCtrl + ((heightEdit + heightEditGap) * 3);

			static const int yLink = yRow + yPosCtrl + ((heightCtrl + heightCtrlGap) * 0);
			static const int yUpDown = yGBUpDown + yPosCtrl + ((heightCtrl + heightCtrlGap) * 0);
			static const int yTrackbar = yGBTrackbar + yPosCtrl + ((heightTrackbar + heightEditGap) * 0);

			static const int yTabCtrl = yGBTabCtrl + yPosCtrl + ((heightTabCtrl + heightEditGap) * 0);
			static const int yListBox = ySTListBox + yPosCtrl;

			static const int yIP = yRow + yPosCtrl + ((heightCtrl + heightEditGap) * 0);

			static const int yListView1 = ySTListView + yPosCtrl;
			static const int yListView2 = yListView1 + ((heightListView + yPosCtrlEnd) * 1) + yGap + 1;

			static const int yTreeView = ySTTreeView + yPosCtrl;

			static const int yHotKey = yRow + yPosCtrl + ((heightCtrl + heightEditGap) * 0);
			static const int yRichEdit = yGBRichEdit + yPosCtrl + ((heightRichEdit + heightEditGap) * 0);

			static const int xToolbar = Scale(100);
			static const int wToolbar = xToolbar;

			static const int offsetScroll = Scale(43);

			// --- Rebar And Toolbars ---
			HWND hRebar = createCtrl(REBARCLASSNAMEW, nullptr,
				WS_CLIPSIBLINGS | WS_CLIPCHILDREN | RBS_VARHEIGHT | RBS_BANDBORDERS | CCS_NODIVIDER | CCS_NOMOVEY,
				0, 0, 0, 0, IdCtrl::rebar);

			static const int imgSize = Scale(16);
			static auto hImageList = ImageList_Create(imgSize, imgSize, ILC_COLOR32 | ILC_MASK, 1, 1);
			auto hIcon = static_cast<HICON>(
				LoadImageW(g_hInst, MAKEINTRESOURCEW(IDI_DEMO), IMAGE_ICON, imgSize, imgSize, LR_DEFAULTCOLOR));
			ImageList_AddIcon(hImageList, hIcon);
			DestroyIcon(hIcon);

			static constexpr size_t nBtn = 5;
			std::array<TBBUTTON, nBtn> tbb{};
			for (size_t i = 0; i < nBtn; ++i)
			{
				tbb.at(i).iBitmap = 0;
				tbb.at(i).idCommand = 10000 + static_cast<int>(i);
				tbb.at(i).fsState = TBSTATE_ENABLED;
				tbb.at(i).fsStyle = BTNS_BUTTON;
			}

			auto createToolbar = [&](IdCtrl id) -> HWND
			{
				HWND hToolbar = createCtrl(TOOLBARCLASSNAMEW, nullptr,
					TBSTYLE_FLAT | TBSTYLE_TOOLTIPS | TBSTYLE_LIST | CCS_NORESIZE,
					xToolbar, 0, wToolbar, 0, id, 0, hRebar);

				SendMessageW(hToolbar, TB_BUTTONSTRUCTSIZE, static_cast<WPARAM>(sizeof(TBBUTTON)), 0);
				SendMessageW(hToolbar, TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>(hImageList));
				SendMessageW(hToolbar, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_DRAWDDARROWS | TBSTYLE_EX_MIXEDBUTTONS | TBSTYLE_EX_HIDECLIPPEDBUTTONS);

				SendMessageW(hToolbar, TB_ADDBUTTONS, static_cast<WPARAM>(tbb.size()), reinterpret_cast<LPARAM>(tbb.data()));
				SendMessageW(hToolbar, TB_AUTOSIZE, 0, 0);

				return hToolbar;
			};

			tbb.at(0).iString = reinterpret_cast<INT_PTR>(L"Normal");

			tbb.at(1).fsState |= TBSTATE_CHECKED;
			tbb.at(1).iString = reinterpret_cast<INT_PTR>(L"Checked");

			tbb.at(2).fsState &= ~TBSTATE_ENABLED;
			tbb.at(2).iString = reinterpret_cast<INT_PTR>(L"Disabled");

			tbb.at(3).fsStyle |= BTNS_DROPDOWN;
			tbb.at(3).iString = reinterpret_cast<INT_PTR>(L"Drop down");

			tbb.at(4).fsStyle |= BTNS_WHOLEDROPDOWN;
			tbb.at(4).iString = reinterpret_cast<INT_PTR>(L"Drop down whole");

			HWND hToolbarBtn = createToolbar(IdCtrl::toolbarBtn);

			for (size_t i = 0; i < nBtn; ++i)
			{
				tbb.at(i).fsStyle |= BTNS_SHOWTEXT;
			}

			HWND hToolbarMix = createToolbar(IdCtrl::toolbarMix);

			SIZE szTb{};
			SendMessageW(hToolbarBtn, TB_GETIDEALSIZE, FALSE, reinterpret_cast<LPARAM>(&szTb));

			REBARBANDINFOW rbBand{};
			rbBand.cbSize = sizeof(REBARBANDINFOW);
			rbBand.fMask = RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_STYLE | RBBIM_SIZE | RBBIM_IDEALSIZE;
			rbBand.fStyle = RBBS_CHILDEDGE | RBBS_GRIPPERALWAYS | RBBS_USECHEVRON | RBBS_TOPALIGN;
			rbBand.hwndChild = hToolbarBtn;
			rbBand.cxMinChild = imgSize * static_cast<UINT>(tbb.size());
			rbBand.cxIdeal = szTb.cx;
			rbBand.cyMinChild = imgSize * 2;
			rbBand.cx = rbBand.cxIdeal;
			const auto tmp = szTb.cx;
			SendMessageW(hRebar, RB_INSERTBAND, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(&rbBand));

			rbBand.hwndChild = hToolbarMix;
			SendMessageW(hToolbarMix, TB_GETIDEALSIZE, FALSE, reinterpret_cast<LPARAM>(&szTb));
			rbBand.cxIdeal = szTb.cx;
			SendMessageW(hRebar, RB_INSERTBAND, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(&rbBand));
			SendMessageW(hRebar, RB_MAXIMIZEBAND, 1, 0);
			SendMessageW(hRebar, RB_SETBANDWIDTH, 0, static_cast<LPARAM>(tmp));

			// --- Push And Split Buttons ---
			createCtrl(WC_BUTTON, L"Push And Split Buttons", BS_GROUPBOX,
				xPos1stCol, yRow, wGroup1stCol, heightGBPush);

			createCtrl(WC_BUTTON, L"Push", BS_PUSHBUTTON,
				xPos1stColCtrl, yPush1, wBtn, heightPush, IdCtrl::push);
			createCtrl(WC_BUTTON, L"Disabled", BS_PUSHBUTTON | WS_DISABLED,
				xPos1stColCtrl, yPush2, wBtn, heightPush, IdCtrl::pushDisabled);
			createCtrl(WC_BUTTON, L"Def Push", BS_DEFPUSHBUTTON,
				xPos1stColCtrl, yPush3, wBtn, heightPush, IdCtrl::pushDef);

			createCtrl(WC_BUTTON, L"Split", BS_PUSHBUTTON | BS_SPLITBUTTON,
				xPosSplit, yPush1, wBtn, heightPush, IdCtrl::split);
			createCtrl(WC_BUTTON, L"Disabled", BS_PUSHBUTTON | BS_SPLITBUTTON | WS_DISABLED,
				xPosSplit, yPush2, wBtn, heightPush, IdCtrl::splitDisabled);
			createCtrl(WC_BUTTON, L"Def Split", BS_DEFPUSHBUTTON | BS_SPLITBUTTON,
				xPosSplit, yPush3, wBtn, heightPush, IdCtrl::splitDef);

			// --- Checkboxes ---
			createCtrl(WC_BUTTON, L"Check Boxes", BS_GROUPBOX,
				xPos1stCol, yGBCheck, wGroup1stCol, heightGBCheck);

			createCtrl(WC_BUTTON, L"Unchecked", BS_AUTOCHECKBOX,
				xPos1stColCtrl, yCheck1, wCheck, heightCtrl, IdCtrl::checkUnchecked);
			createCtrl(WC_BUTTON, L"Unchecked Disabled", BS_AUTOCHECKBOX | WS_DISABLED,
				xPos1stColCtrl, yCheck2, wCheck, heightCtrl, IdCtrl::checkUncheckedDisabled);

			HWND hChecked = createCtrl(WC_BUTTON, L"Checked", BS_AUTOCHECKBOX,
				xPos1stColCtrl, yCheck3, wCheck, heightCtrl, IdCtrl::checkChecked);
			SendMessageW(hChecked, BM_SETCHECK, BST_CHECKED, 0);
			HWND hCheckedDisabled = createCtrl(WC_BUTTON, L"Checked Disabled",
				BS_AUTOCHECKBOX | WS_DISABLED,
				xPos1stColCtrl, yCheck4, wCheck, heightCtrl, IdCtrl::checkCheckedDisabled);
			SendMessageW(hCheckedDisabled, BM_SETCHECK, BST_CHECKED, 0);

			HWND hTriState = createCtrl(WC_BUTTON, L"Tri-state", BS_AUTO3STATE,
				xPos1stColCtrl, yCheck5, wCheck, heightCtrl, IdCtrl::triState);
			SendMessageW(hTriState, BM_SETCHECK, BST_INDETERMINATE, 0);
			HWND hTriStateDisabled = createCtrl(WC_BUTTON, L"Tri-state Disabled", BS_AUTO3STATE | WS_DISABLED,
				xPos1stColCtrl, yCheck6, wCheck, heightCtrl, IdCtrl::triStateDisabled);
			SendMessageW(hTriStateDisabled, BM_SETCHECK, BST_INDETERMINATE, 0);

			// --- Progress Bars ---
			createCtrl(WC_BUTTON, L"Progress Bars", BS_GROUPBOX,
				xPos1stCol, yGBProgress, wGroup1stCol, heightGBProgress);

			createCtrl(WC_STATIC, L"Normal:", SS_RIGHT,
				xPos1stColCtrl, yProgress1, wProgressLabel, heightCtrl);
			HWND hProgNormal = createCtrl(PROGRESS_CLASSW, nullptr, 0,
				xProgress, yProgress1, wProgress, heightCtrl, IdCtrl::progressNormal);
			SendMessageW(hProgNormal, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
			SendMessageW(hProgNormal, PBM_SETPOS, 60, 0);
			SendMessageW(hProgNormal, PBM_SETSTATE, PBST_NORMAL, 0);

			createCtrl(WC_STATIC, L"Error:", SS_RIGHT,
				xPos1stColCtrl, yProgress2, wProgressLabel, heightCtrl);
			HWND hProgError = createCtrl(PROGRESS_CLASSW, nullptr, 0,
				xProgress, yProgress2, wProgress, heightCtrl, IdCtrl::progressError);
			SendMessageW(hProgError, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
			SendMessageW(hProgError, PBM_SETPOS, 40, 0);
			SendMessageW(hProgError, PBM_SETSTATE, PBST_ERROR, 0);

			createCtrl(WC_STATIC, L"Paused:", SS_RIGHT,
				xPos1stColCtrl, yProgress3, wProgressLabel, heightCtrl);
			HWND hProgPaused = createCtrl(PROGRESS_CLASSW, nullptr, 0,
				xProgress, yProgress3, wProgress, heightCtrl, IdCtrl::progressPaused);
			SendMessageW(hProgPaused, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
			SendMessageW(hProgPaused, PBM_SETPOS, 80, 0);
			SendMessageW(hProgPaused, PBM_SETSTATE, PBST_PAUSED, 0);

			createCtrl(WC_STATIC, L"Marquee:", SS_RIGHT,
				xPos1stColCtrl, yProgress4, wProgressLabel, heightCtrl);
			HWND hProgMarquee = createCtrl(PROGRESS_CLASSW, nullptr, PBS_MARQUEE,
				xProgress, yProgress4, wProgress, heightCtrl, IdCtrl::progressMarquee);
			SendMessageW(hProgMarquee, PBM_SETMARQUEE, TRUE, 0);

			// --- Radio Buttons ---
			createCtrl(WC_BUTTON, L"Radio Buttons", BS_GROUPBOX,
				xPos2ndCol, yRow, wGroup2ndCol, heightGBRadio);

			createCtrl(WC_BUTTON, L"Unset", BS_AUTORADIOBUTTON,
				xPos2ndColCtrl, yRadio1, wBtn, heightCtrl, IdCtrl::radioUnset);
			createCtrl(WC_BUTTON, L"Unset Disabled", BS_AUTORADIOBUTTON | WS_DISABLED,
				xPos2ndColCtrl, yRadio2, wBtn, heightCtrl, IdCtrl::radioUnsetDisabled);

			HWND hCheckedRadio = createCtrl(WC_BUTTON, L"Set", BS_AUTORADIOBUTTON,
				xPos2ndColCtrl, yRadio3, wBtn, heightCtrl, IdCtrl::radioSet);
			SendMessageW(hCheckedRadio, BM_SETCHECK, BST_CHECKED, 0);
			HWND hCheckedRadioDisabled = createCtrl(WC_BUTTON, L"Set Disabled", BS_AUTORADIOBUTTON | WS_DISABLED,
				xPos2ndColCtrl, yRadio4, wBtn, heightCtrl, IdCtrl::radioSetDisabled);
			SendMessageW(hCheckedRadioDisabled, BM_SETCHECK, BST_CHECKED, 0);

			// --- Edit Controls ---
			createCtrl(WC_BUTTON, L"Edit Controls", BS_GROUPBOX,
				xPos2ndCol, yGBEdit, wGroup2ndCol, heightGBEdit);

			createCtrl(WC_EDIT, L"Normal", ES_LEFT,
				xPos2ndColCtrl, yEdit1, wEditCombo, heightEdit, IdCtrl::editNormal, WS_EX_CLIENTEDGE);
			createCtrl(WC_EDIT, L"Disabled", WS_DISABLED,
				xPos2ndColCtrl, yEdit2, wEditCombo, heightEdit, IdCtrl::editDisabled, WS_EX_CLIENTEDGE);
			createCtrl(WC_EDIT, L"Secret", ES_PASSWORD,
				xPos2ndColCtrl, yEdit3, wEditCombo, heightEdit, IdCtrl::editPass, WS_EX_CLIENTEDGE);
			createCtrl(WC_EDIT, L"Read-only", ES_READONLY,
				xPos2ndColCtrl, yEdit4, wEditCombo, heightEdit, IdCtrl::editReadOnly, WS_EX_CLIENTEDGE);

			// --- Combo Boxes ---
			createCtrl(WC_BUTTON, L"Combo Boxes", BS_GROUPBOX,
				xPos2ndCol, yGBCombo, wGroup2ndCol, heightGBCombo + 1);

			HWND hCombo = createCtrl(WC_COMBOBOX, nullptr, CBS_DROPDOWN | WS_VSCROLL,
				xPos2ndColCtrl, yCombo1, wEditCombo, 100, IdCtrl::comboDrop);
			for (int i = 0; i < 4; ++i)
			{
				const std::wstring itemText = L"Item " + std::to_wstring(i + 1);
				SendMessageW(hCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(itemText.c_str()));
			}
			SetWindowTextW(hCombo, L"Item 1");

			HWND hComboDisabled = createCtrl(WC_COMBOBOX, nullptr, CBS_DROPDOWN | WS_VSCROLL | WS_DISABLED,
				xPos2ndColCtrl, yCombo2, wEditCombo, 100, IdCtrl::comboDropDisabled);
			SendMessageW(hComboDisabled, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Item 1"));
			SetWindowTextW(hComboDisabled, L"Item 1");
			SendMessageW(hComboDisabled, CB_SETEDITSEL, 0, 0);

			HWND hComboList = createCtrl(WC_COMBOBOX, nullptr, CBS_DROPDOWNLIST | WS_VSCROLL,
				xPos2ndColCtrl, yCombo3, wEditCombo, 100, IdCtrl::comboList);
			for (int i = 0; i < 4; ++i)
			{
				const std::wstring itemText = L"Item " + std::to_wstring(i + 1);
				SendMessageW(hComboList, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(itemText.c_str()));
			}
			SendMessageW(hComboList, CB_SETCURSEL, 0, 0);

			HWND hComboListDisabled = createCtrl(WC_COMBOBOX, nullptr, CBS_DROPDOWNLIST | WS_VSCROLL | WS_DISABLED,
				xPos2ndColCtrl, yCombo4, wEditCombo, 100, IdCtrl::comboListDisabled);
			SendMessageW(hComboListDisabled, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Item 1"));
			SendMessageW(hComboListDisabled, CB_SETCURSEL, 0, 0);

			// --- SysLink ---
			createCtrl(WC_BUTTON, L"SysLink", BS_GROUPBOX,
				xPos3rdCol, yRow, wGroup3rdCol, heightGBLink);

			createCtrl(WC_LINK, L"<a href=\"https://example.com\">https://example.com</a>", 0,
				xPos3rdColCtrl, yLink, wCtrl3rdCol, heightCtrl, IdCtrl::syslink);

			// --- UpDown (Spinner) Control ---
			createCtrl(WC_BUTTON, L"UpDown (Spinner)", BS_GROUPBOX,
				xPos3rdCol, yGBUpDown, wGroup3rdCol, heightGBUpDown);

			// Buddy Edit control
			HWND hEditUpdDown = createCtrl(WC_EDIT, L"0", ES_RIGHT | WS_BORDER,
				xPos3rdColCtrl, yUpDown, wCtrl3rdCol + Scale(15), heightEdit, IdCtrl::upDownEdit);

			// UpDown control
			HWND hUpDown = createCtrl(UPDOWN_CLASSW, L"UpDown (Spinner)", UDS_ALIGNRIGHT | UDS_AUTOBUDDY | UDS_SETBUDDYINT | UDS_ARROWKEYS,
				0, 0, 0, 0, IdCtrl::upDown);
			SendMessageW(hUpDown, UDM_SETBUDDY, reinterpret_cast<WPARAM>(hEditUpdDown), 0);
			SendMessageW(hUpDown, UDM_SETRANGE, 0, MAKELPARAM(9999, 0));

			// --- Trackbar ---
			createCtrl(WC_BUTTON, L"Trackbar", BS_GROUPBOX,
				xPos3rdCol, yGBTrackbar, wGroup3rdCol, heightGBTrackbar);

			HWND hTrackbar = createCtrl(TRACKBAR_CLASSW, nullptr, TBS_AUTOTICKS | TBS_TOOLTIPS,
				xPos3rdColCtrl, yTrackbar, wCtrl3rdCol, heightTrackbar, IdCtrl::trackbar);
			SendMessageW(hTrackbar, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
			SendMessageW(hTrackbar, TBM_SETPOS, TRUE, 50);
			SendMessageW(hTrackbar, TBM_SETTICFREQ, 10, 0);

			// --- Tab Control ---
			HWND hTabCtrl = createCtrl(WC_TABCONTROLW, nullptr, TCS_TABS,
				xPos3rdColCtrl, yTabCtrl, wCtrl3rdCol, heightTabCtrl, IdCtrl::tabcontrol);

			createCtrl(WC_BUTTON, L"Tab Control", BS_GROUPBOX,
				xPos3rdCol, yGBTabCtrl, wGroup3rdCol, heightGBTabCtrl);

			// Define tabs
			TCITEMW tie{};
			tie.mask = TCIF_TEXT;
			for (int i = 0; i < 4; ++i)
			{
				std::wstring label = L"Tab " + std::to_wstring(i + 1);
				tie.pszText = label.data();

				SendMessageW(hTabCtrl, TCM_INSERTITEM, i, reinterpret_cast<LPARAM>(&tie));
			}

			// --- List Box ---
			createCtrl(WC_STATIC, L"List Box:", SS_LEFT,
				xPos3rdCol, ySTListBox, wGroup3rdCol, heightCtrl);

			HWND hListBox = createCtrl(WC_LISTBOX, nullptr, WS_VSCROLL,
				xPos3rdCol, yListBox, wGroup3rdCol, heightListBox, IdCtrl::listbox, WS_EX_CLIENTEDGE);

			// Add list box items
			for (int i = 1; i <= 7; ++i)
			{
				const std::wstring label = L"Item " + std::to_wstring(i);
				SendMessageW(hListBox, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
			}

			// --- IP Address ---
			createCtrl(WC_BUTTON, L"IP Address", BS_GROUPBOX,
				xPos4thCol, yRow, wGroup4thCol, heightGBIP);

			HWND hIP = createCtrl(WC_IPADDRESS, nullptr, 0,
				xPos4thColCtrl, yIP, wCtrl4thCol, heightCtrl, IdCtrl::ipAddress, 0, hWnd);

			SendMessageW(hIP, IPM_SETADDRESS, 0, MAKEIPADDRESS(255, 255, 255, 255));

			// --- List Views ---
			createCtrl(WC_STATIC, L"List Views:", SS_LEFT,
				xPos4thCol, ySTListView, wGroup4thCol, heightCtrl);

			HWND hListView = createCtrl(WC_LISTVIEWW, nullptr, LVS_REPORT,
				xPos4thCol, yListView1, wGroup4thCol, heightListView, IdCtrl::listview, WS_EX_CLIENTEDGE);
			HWND hListViewGrid = createCtrl(WC_LISTVIEWW, nullptr, LVS_REPORT,
				xPos4thCol, yListView2, wGroup4thCol, heightListView, IdCtrl::listviewGrid, WS_EX_CLIENTEDGE);

			ListView_SetExtendedListViewStyle(hListView, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);
			ListView_SetExtendedListViewStyle(hListViewGrid, LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);

			// Set up columns
			LVCOLUMNW col{};
			col.mask = LVCF_TEXT | LVCF_WIDTH;

			std::wstring colText1 = L"Col 1";
			col.pszText = colText1.data();
			col.cx = Scale(70);
			ListView_InsertColumn(hListView, 0, &col);
			ListView_InsertColumn(hListViewGrid, 0, &col);

			std::wstring colText2 = L"Col 2";
			col.pszText = colText2.data();
			ListView_InsertColumn(hListViewGrid, 1, &col);
			col.cx = Scale(100);
			ListView_InsertColumn(hListView, 1, &col);

			// Add 5 items
			LVITEMW item{};
			item.mask = LVIF_TEXT;
			for (int i = 0; i < 3; ++i)
			{
				std::wstring name = L"Item " + std::to_wstring(i + 1);
				item.iItem = i;
				item.iSubItem = 0;
				item.pszText = name.data();
				ListView_InsertItem(hListView, &item);
				ListView_InsertItem(hListViewGrid, &item);

				item.iSubItem = 1;
				item.pszText = name.data();
				ListView_SetItem(hListView, &item);
				ListView_SetItem(hListViewGrid, &item);

				ListView_SetCheckState(hListView, i, ((i % 2) == 0) ? TRUE : FALSE);
			}

			// --- Tree View ---
			createCtrl(WC_STATIC, L"Tree View:", SS_LEFT,
				xPos4thCol, ySTTreeView, wGroup4thCol, heightCtrl);

			HWND hTree = createCtrl(WC_TREEVIEWW, nullptr, TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS,
				xPos4thCol, yTreeView, wGroup4thCol, heightTreeView, IdCtrl::treeview, WS_EX_CLIENTEDGE);

			// Add items and subitems
			for (int i = 0; i < 4; ++i)
			{
				std::wstring parentText = L"Item " + std::to_wstring(i + 1);

				TVINSERTSTRUCTW parentInsert{};
				parentInsert.hParent = TVI_ROOT;
				parentInsert.hInsertAfter = TVI_LAST;
				parentInsert.item.mask = TVIF_TEXT;
				parentInsert.item.pszText = parentText.data();

				const auto hTvItemParent = TreeView_InsertItem(hTree, &parentInsert);

				const int childCount = 3 - i;
				for (int j = 0; j < childCount; ++j)
				{
					std::wstring childText = L"Subitem " + std::to_wstring(j + 1);

					TVINSERTSTRUCTW childInsert{};
					childInsert.hParent = hTvItemParent;
					childInsert.hInsertAfter = TVI_LAST;
					childInsert.item.mask = TVIF_TEXT;
					childInsert.item.pszText = childText.data();

					TreeView_InsertItem(hTree, &childInsert);
				}
			}

			// --- Hot Key ---
			createCtrl(WC_BUTTON, L"Hot Key", BS_GROUPBOX,
				xPos5thCol, yRow, wGroup5thCol, heightGBHotKey);

			createCtrl(HOTKEY_CLASS, nullptr, 0,
				xPos5thColCtrl, yHotKey, wCtrl5thCol, heightCtrl, IdCtrl::hotkey, 0, hWnd);

			// --- Rich Edit ---
			createCtrl(WC_BUTTON, L"Rich Edit", BS_GROUPBOX,
				xPos5thCol, yGBRichEdit, wGroup5thCol, heightGBRichEdit);

			hModRich = ::LoadLibraryExW(L"Msftedit.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
			if (hModRich != nullptr)
			{
				static const std::wstring richEditText = L"{\\rtf1\\ansi\n"
					L"\\line\n"
					L"Darkmodelib\\line\n"
					L"\\line\n"
					L"by\\line\n"
					L"\\line\n"
					L"ozone10\\line\n";

				createCtrl(MSFTEDIT_CLASS, richEditText.c_str(), ES_CENTER | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_VSCROLL | WS_BORDER,
					xPos5thColCtrl, yRichEdit, wCtrl5thCol, heightRichEdit, IdCtrl::richEdit, WS_EX_CLIENTEDGE);
			}

			// --- Scroll Bars ---
			// Horizontal scroll bar
			HWND hScrollH = createCtrl(WC_SCROLLBAR, nullptr, SBS_HORZ,
				0, rcClient.bottom - offsetScroll, rcClient.right - heightCtrl, heightCtrl, IdCtrl::scrollH);

			// Vertical scroll bar
			RECT rcRebar{};
			GetClientRect(hRebar, &rcRebar);
			const LONG heightRebar = rcRebar.bottom - rcRebar.top;

			HWND hScrollV = createCtrl(WC_SCROLLBAR, nullptr, SBS_VERT,
				rcClient.right - heightCtrl, heightRebar, heightCtrl, rcClient.bottom - offsetScroll - heightRebar, IdCtrl::scrollV);

			SCROLLINFO si{};
			si.cbSize = sizeof(SCROLLINFO);
			si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
			si.nMin = 0;
			si.nMax = 50;
			si.nPage = 10;
			si.nPos = 0;
			SetScrollInfo(hScrollH, SB_CTL, &si, TRUE);
			SetScrollInfo(hScrollV, SB_CTL, &si, TRUE);

			// --- Status Bar ---
			HWND hStatus = createCtrl(STATUSCLASSNAMEW, nullptr, SBARS_SIZEGRIP,
				0, 0, 0, 0, IdCtrl::statusbar);

			static const std::array<int, 3> widths{
				Scale(120),
				Scale(240),
				-1 // Extend to full width
			};
			SendMessageW(hStatus, SB_SETPARTS, static_cast<WPARAM>(widths.size()), reinterpret_cast<LPARAM>(widths.data()));

			// Set text in each parts
			SendMessageW(hStatus, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(L"Status Bar"));
			SendMessageW(hStatus, SB_SETTEXT, 1, reinterpret_cast<LPARAM>(L"Panel 1"));
			SendMessageW(hStatus, SB_SETTEXT, 2, reinterpret_cast<LPARAM>(L"Panel 2"));

			// --- Dark Mode ---
			DarkMode::setColorizeTitleBarConfig(true);
			DarkMode::setDarkWndNotifySafe(hWnd);
			DarkMode::setWindowEraseBgSubclass(hWnd);
			DarkMode::setWindowMenuBarSubclass(hWnd);

			DarkMode::setWindowExStyle(hWnd, false, WS_EX_COMPOSITED);

			break;
		}

		case WM_COMMAND:
		{
			const int wmId = LOWORD(wParam);
			switch (wmId)
			{
				case IDM_ABOUT:
				{
					DialogBoxParamW(g_hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About, 0L);
					break;
				}

				case IDM_CHOOSECOLOR:
				{
					static constexpr COLORREF white = 0xFFFFFF;

					static constinit std::array<COLORREF, 16> customColors{
						white, white, white, white,
						white, white, white, white,
						white, white, white, white,
						white, white, white, white
					};

					CHOOSECOLORW cc{};
					cc.lStructSize = sizeof(cc);
					cc.hwndOwner = hWnd;
					cc.lpCustColors = customColors.data();
					cc.rgbResult = RGB(0, 120, 215);
					cc.Flags = CC_FULLOPEN | CC_RGBINIT | CC_ENABLEHOOK;
					cc.lpfnHook = static_cast<LPCCHOOKPROC>(DarkMode::HookDlgProc);

					ChooseColorW(&cc);
					break;
				}

				case IDM_CHOOSEFONT:
				{
					static LOGFONTW lf = []{
						NONCLIENTMETRICS ncm{};
						ncm.cbSize = sizeof(NONCLIENTMETRICS);
						if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0))
						{
							return ncm.lfMessageFont;
						}
						return LOGFONTW{};
					}();

					CHOOSEFONTW cf{};
					cf.lStructSize = sizeof(cf);
					cf.hwndOwner = hWnd;
					cf.lpLogFont = &lf;
					cf.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT | CF_EFFECTS;
					cf.Flags |= CF_ENABLEHOOK | CF_ENABLETEMPLATE;
					cf.lpfnHook = static_cast<LPCFHOOKPROC>(DarkMode::HookDlgProc);
					cf.hInstance = GetModuleHandleW(nullptr);
					cf.lpTemplateName = MAKEINTRESOURCE(IDD_DARK_FONT_DIALOG);

					ChooseFontW(&cf);
					break;
				}

				case IDM_TASKDIALOG:
				{
					static constexpr std::array<TASKDIALOG_BUTTON, 4> radioBtn{ {
						{radio_1, L"Radio button 1"},
						{radio_2, L"Radio button 2 (disabled)"},
						{radio_3, L"Radio button 3"},
						{radio_4, L"Radio button 4\nwith\nmultiple\nlines"}
					} };

					static constexpr std::array<TASKDIALOG_BUTTON, 5> commandBtn{ {
						{cmd_1, L"Command link 1"},
						{cmd_2, L"Command link 2 (disabled)"},
						{cmd_3, L"Command link 3 with shield"},
						{cmd_4, L"Command link 4 with shield (disabled)"},
						{cmd_5, L"Command link 5\nwith\nmultiple\nlines"}
					} };

					TASKDIALOGCONFIG taskDlgCfg{};
					taskDlgCfg.cbSize = sizeof(TASKDIALOGCONFIG);
					taskDlgCfg.hwndParent = hWnd;
					taskDlgCfg.hInstance = nullptr;
					taskDlgCfg.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_ALLOW_DIALOG_CANCELLATION | TDF_USE_COMMAND_LINKS | TDF_EXPAND_FOOTER_AREA | TDF_SHOW_MARQUEE_PROGRESS_BAR | TDF_CAN_BE_MINIMIZED | TDF_SIZE_TO_CONTENT;
					taskDlgCfg.dwCommonButtons = TDCBF_OK_BUTTON | TDCBF_YES_BUTTON | TDCBF_NO_BUTTON | TDCBF_CANCEL_BUTTON | TDCBF_RETRY_BUTTON | TDCBF_CLOSE_BUTTON;
					taskDlgCfg.pszWindowTitle = L"Dark Task Dialog";
					taskDlgCfg.pszMainIcon = TD_ERROR_ICON;
					taskDlgCfg.pszMainInstruction = L"Simple Dark Task Dialog";
					taskDlgCfg.pszContent = L"Example of task dialog with basic dark mode support.\nMight/might not support every task dialog configuration.\nCurrently works only on Windows 11.";
					taskDlgCfg.cButtons = static_cast<UINT>(commandBtn.size());
					taskDlgCfg.pButtons = commandBtn.data();
					taskDlgCfg.cRadioButtons = static_cast<UINT>(radioBtn.size());
					taskDlgCfg.pRadioButtons = radioBtn.data();
					taskDlgCfg.nDefaultButton = IDCLOSE;
					taskDlgCfg.pszVerificationText = L"&Verification text";
					taskDlgCfg.pszExpandedInformation = L"Expanded Information in footer.\nThis can also go in the top part of the dialog.";
					taskDlgCfg.pszExpandedControlText = L"Expanded Control Text\non two lines";
					taskDlgCfg.pszCollapsedControlText = L"Collapsed Control Text";
					taskDlgCfg.pszFooterIcon = TD_INFORMATION_ICON;
					taskDlgCfg.pszFooter = L"Footer with <a href=\"https://example.com\">hyperlink</a>";
					taskDlgCfg.pfCallback = TaskDlgCallback;
					taskDlgCfg.lpCallbackData = 0;
					taskDlgCfg.cxWidth = 0;

					BOOL checkFlag = FALSE;
					DarkMode::darkTaskDialogIndirect(&taskDlgCfg, nullptr, nullptr, &checkFlag);
					break;
				}

				case IDM_DARK:
				case IDM_LIGHT:
				case IDM_CLASSIC:
				{
					SelectAndRefreshMode(hWnd, wmId);
					break;
				}

				case IDM_EXIT:
				{
					DestroyWindow(hWnd);
					break;
				}

				default:
				{
					return DefWindowProcW(hWnd, message, wParam, lParam);
				}
			}
			break;
		}

		case WM_SIZE:
		{
			SendMessageW(GetDlgItem(hWnd, static_cast<int>(IdCtrl::statusbar)), WM_SIZE, 0, 0); // Auto-reposition
			SendMessageW(GetDlgItem(hWnd, static_cast<int>(IdCtrl::rebar)), WM_SIZE, 0, 0); // Auto-reposition

			RECT rcClient{};
			GetClientRect(hWnd, &rcClient);

			// Reposition scroll bars
			RECT rcRebar{};
			GetClientRect(GetDlgItem(hWnd, static_cast<int>(IdCtrl::rebar)), &rcRebar);
			const LONG heightRebar = rcRebar.bottom - rcRebar.top;

			static const int heightStatus = Scale(20);
			static const int offsetScroll = Scale(43);

			SetWindowPos(GetDlgItem(hWnd, static_cast<int>(IdCtrl::scrollH)), nullptr,
				0, rcClient.bottom - offsetScroll, rcClient.right - heightStatus, heightStatus,
				SWP_NOZORDER);
			SetWindowPos(GetDlgItem(hWnd, static_cast<int>(IdCtrl::scrollV)), nullptr,
				rcClient.right - heightStatus, heightRebar, heightStatus, rcClient.bottom - offsetScroll - heightRebar,
				SWP_NOZORDER);

			return 0;
		}

		case WM_HSCROLL:
		{
			HWND hTrackbar = GetDlgItem(hWnd, static_cast<int>(IdCtrl::trackbar));
			if (hTrackbar == reinterpret_cast<HWND>(lParam))
			{
				break;
			}
			[[fallthrough]];
		}
		case WM_VSCROLL:
		{
			HWND hScroll = reinterpret_cast<HWND>(lParam);
			const int code = LOWORD(wParam);

			SCROLLINFO si{};
			si.cbSize = sizeof(SCROLLINFO);
			si.fMask = SIF_ALL;
			GetScrollInfo(hScroll, SB_CTL, &si);

			switch (code)
			{
				case SB_LINELEFT:
				{
					si.nPos -= 1;
					break;
				}

				case SB_LINERIGHT:
				{
					si.nPos += 1;
					break;
				}

				case SB_PAGELEFT:
				{
					si.nPos -= static_cast<int>(si.nPage);
					break;
				}

				case SB_PAGERIGHT:
				{
					si.nPos += static_cast<int>(si.nPage);
					break;
				}

				case SB_THUMBTRACK:
				case SB_THUMBPOSITION:
				{
					si.nPos = HIWORD(wParam); //si.nTrackPos;
					break;
				}

				default:
				{
					return 0;
				}
			}

			si.fMask = SIF_POS;
			si.nPos = std::max<int>(si.nMin, std::min<int>(si.nMax - static_cast<int>(si.nPage) + 1, si.nPos));
			SetScrollInfo(hScroll, SB_CTL, &si, TRUE);

			return 0;
		}

		case WM_DESTROY:
		{
			PostQuitMessage(0);
			break;
		}

		case WM_NCDESTROY:
		{
			HWND hStatus = GetDlgItem(hWnd, static_cast<int>(IdCtrl::statusbar));
			auto hFont = reinterpret_cast<HFONT>(SendMessageW(hStatus, WM_GETFONT, 0, 0));
			if (hFont != nullptr)
			{
				DeleteObject(hFont);
				hFont = nullptr;
			}

			HWND hToolbar = GetDlgItem(hWnd, static_cast<int>(IdCtrl::toolbarBtn));
			if (hToolbar)
			{
				auto hImgList = reinterpret_cast<HIMAGELIST>(SendMessageW(hToolbar, TB_GETIMAGELIST, 0, 0));
				if (hImgList != nullptr)
				{
					ImageList_Destroy(hImgList);
					hImgList = nullptr;
				}
			}

			if (hModRich != nullptr)
			{
				::FreeLibrary(hModRich);
				hModRich = nullptr;
			}

			return DefWindowProcW(hWnd, message, wParam, lParam);
		}

		default:
		{
			return DefWindowProcW(hWnd, message, wParam, lParam);
		}
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, [[maybe_unused]] LPARAM /*lParam*/)
{
	switch (message)
	{
		case WM_INITDIALOG:
		{
			DarkMode::setDarkWndNotifySafe(hDlg);
			std::wstring dmlVer = L"Darkmodelib demo ";
#if defined(DMLIB_DLL)
			dmlVer += L"DLL";
#endif
			dmlVer += L" v";
			dmlVer += std::to_wstring(DarkMode::getLibInfo(static_cast<int>(DarkMode::LibInfo::verMajor)));
			dmlVer += L'.';
			dmlVer += std::to_wstring(DarkMode::getLibInfo(static_cast<int>(DarkMode::LibInfo::verMinor)));
			dmlVer += L'.';
			dmlVer += std::to_wstring(DarkMode::getLibInfo(static_cast<int>(DarkMode::LibInfo::verRevision)));

			SetDlgItemTextW(hDlg, IDC_ABOUT_VERSION, dmlVer.c_str());
			return TRUE;
		}

		case WM_COMMAND:
		{
			if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
			{
				EndDialog(hDlg, LOWORD(wParam));
				return TRUE;
			}
			break;
		}

		default:
		{
			break;
		}
	}
	return FALSE;
}
