/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Timer for mouse wheel smooth scrolling
constexpr UINT_PTR kSmoothScrollTimerID = 6;

void UpdateDeltaPerLine();

LRESULT CALLBACK WndProcCanvas(HWND, UINT, WPARAM, LPARAM);
LRESULT WndProcCanvasAbout(MainWindow*, HWND, UINT, WPARAM, LPARAM);
bool IsDragDistance(int x1, int x2, int y1, int y2);
void CancelDrag(MainWindow*);

extern Kind kNotifAnnotation;
