/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;
struct WindowTab;

namespace gpui {
struct PaintCtx;
}

void ReadingBarToggle(MainWindow*);
void ReadingBarToggleInvert(MainWindow*);
void ReadingBarHide(MainWindow*);
void ReadingBarForgetTab(WindowTab*);
void ReadingBarCancelDrag(MainWindow*);
// ng: orig paints through Gfx; here the canvas hands over its PaintCtx
void ReadingBarPaint(MainWindow*, gpui::PaintCtx*);
bool ReadingBarOnLeftDown(MainWindow*, int x, int y);
bool ReadingBarOnMouseMove(MainWindow*, int x, int y);
bool ReadingBarOnLeftUp(MainWindow*);
// ng: orig answers WM_SETCURSOR; here the canvas asks for the cursor it wants
bool ReadingBarOnSetCursor(MainWindow*, int x, int y);
void ReadingBarOnMouseLeave(MainWindow*);
// ng: orig reads the modifiers with GetKeyState in its PreTranslate
bool ReadingBarOnKey(MainWindow*, int key, bool ctrl, bool shift, bool alt);
bool ReadingBarIsOn(MainWindow*);
TempStr ReadingBarStateTemp(int* exitCodeOut);
