/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;
struct WindowTab;
struct Gfx;

void ReadingBarToggle(MainWindow*);
void ReadingBarToggleInvert(MainWindow*);
void ReadingBarHide(MainWindow*);
void ReadingBarForgetTab(WindowTab*);
void ReadingBarCancelDrag(MainWindow*);
void ReadingBarPaint(MainWindow*, Gfx*);
bool ReadingBarOnLeftDown(MainWindow*, int x, int y);
bool ReadingBarOnMouseMove(MainWindow*, int x, int y);
bool ReadingBarOnLeftUp(MainWindow*);
bool ReadingBarOnSetCursor(MainWindow*);
void ReadingBarOnMouseLeave(MainWindow*);
bool ReadingBarOnKey(MainWindow*, WPARAM key);
bool ReadingBarIsOn(MainWindow*);
TempStr ReadingBarStateTemp(int* exitCodeOut);
