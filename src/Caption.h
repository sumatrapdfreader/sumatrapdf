/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// factor by how large the non-maximized caption should be in relation to the tabbar
#define kCaptionTabBarDyFactor 1.25f

void CreateCaption(MainWindow* win);
void RegisterCaptionWndClass();
LRESULT CustomCaptionFrameProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, bool* callDef, MainWindow* win);
void PaintParentBackground(HWND hwnd, HDC hdc);
void RelayoutCaption(MainWindow* win);
void SetCaptionButtonsRtl(CaptionInfo*, bool isRtl);
void CaptionUpdateUI(MainWindow*, CaptionInfo*);
void DeleteCaption(CaptionInfo*);
void OpenSystemMenu(MainWindow* win);
