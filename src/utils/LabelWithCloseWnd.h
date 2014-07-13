/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef LabelWithCloseWnd_h
#define LabelWithCloseWnd_h

struct LabelWithCloseWnd;

void                RegisterLabelWithCloseWnd();
LabelWithCloseWnd * CreateLabelWithCloseWnd(HWND parent, int cmd);
HWND                GetHwnd(LabelWithCloseWnd*);
void                SetLabel(LabelWithCloseWnd*, const WCHAR*);
void                SetFont(LabelWithCloseWnd*, HFONT);
void                SetPaddingXY(LabelWithCloseWnd*, int x, int y);
SizeI               GetIdealSize(LabelWithCloseWnd*);
void                Free(LabelWithCloseWnd*);

#endif
