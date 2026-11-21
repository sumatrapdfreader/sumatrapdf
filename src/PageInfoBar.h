/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// The page info bar: Toggle Page Info's bar along the bottom of the window,
// when PageInfoLocation is bottom or bottom-overlay (#6494).

struct PageInfoBar;
struct Gfx;
struct MainWindow;

bool PageInfoBarTakesRow(MainWindow*);
bool PageInfoBarOverlays(MainWindow*);
int PageInfoBarDy(MainWindow*);
void ProgressBarPaint(MainWindow*, Gfx*);
HWND PageInfoBarEnsure(MainWindow*);
void PageInfoBarSetVisible(MainWindow*, bool visible);
void PageInfoBarUpdate(MainWindow*);
void PageInfoBarPlaceOverlay(MainWindow*);
void DeletePageInfoBar(MainWindow*);
