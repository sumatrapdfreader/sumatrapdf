#ifndef _SAMPLEPROJECT_MAIN_H_
#define _SAMPLEPROJECT_MAIN_H_


#ifdef _WIN32
#include <windows.h>
#include <commctrl.h>
#else
#include "../swell.h"
#endif

#include "../../wdltypes.h"

extern WDL_DLGRET MainDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

extern HINSTANCE g_hInst; 
extern HWND g_hwnd;
extern UINT Scroll_Message; 



#endif//_SAMPLEPROJECT_MAIN_H_

