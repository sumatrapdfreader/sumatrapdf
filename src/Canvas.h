/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

void UpdateDeltaPerLine();

LRESULT CALLBACK WndProcCanvas(HWND, UINT, WPARAM, LPARAM);
LRESULT WndProcCanvasAbout(WindowInfo*, HWND, UINT, WPARAM, LPARAM);
bool IsDrag(int x1, int x2, int y1, int y2);
void CancelDrag(WindowInfo*);
