/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

void UpdateDeltaPerLine();

LRESULT CALLBACK WndProcCanvas(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WndProcCanvasAbout(WindowInfo* win, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
bool IsDragY(int y1, int y2);
bool IsDragX(int x1, int x2);
