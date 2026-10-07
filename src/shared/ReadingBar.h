/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

namespace gpui {
struct PaintCtx;
}

struct MainWindow;
struct WindowTab;
struct Gfx;

void ReadingBarToggle(MainWindow*);
void ReadingBarToggleInvert(MainWindow*);
void ReadingBarHide(MainWindow*);
void ReadingBarForgetTab(WindowTab*);
void ReadingBarCancelDrag(MainWindow*);
void ReadingBarPaint(MainWindow*, Gfx*);
void ReadingBarPaint(MainWindow*, gpui::PaintCtx*);
bool ReadingBarOnLeftDown(MainWindow*, int x, int y);
bool ReadingBarOnMouseMove(MainWindow*, int x, int y);
bool ReadingBarOnLeftUp(MainWindow*);
bool ReadingBarOnSetCursor(MainWindow*);
bool ReadingBarOnSetCursor(MainWindow*, int x, int y);
void ReadingBarOnMouseLeave(MainWindow*);
bool ReadingBarOnKey(MainWindow*, WPARAM key);
bool ReadingBarOnKey(MainWindow*, int key, bool ctrl, bool shift, bool alt);
bool ReadingBarIsOn(MainWindow*);
TempStr ReadingBarStateTemp(int* exitCodeOut);
