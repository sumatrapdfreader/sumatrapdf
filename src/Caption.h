/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// factor by how large the non-maximized caption should be in relation to the tabbar
#define CAPTION_TABBAR_HEIGHT_FACTOR 1.25f

void CreateCaption(WindowInfo* win);
void RegisterCaptionWndClass();
LRESULT CustomCaptionFrameProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, bool* callDef, WindowInfo* win);
void PaintParentBackground(HWND hwnd, HDC hdc);
void RelayoutCaption(WindowInfo* win);
void SetCaptionButtonsRtl(CaptionInfo*, bool isRtl);
void CaptionUpdateUI(WindowInfo*, CaptionInfo*);
void DeleteCaption(CaptionInfo*);
