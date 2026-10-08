/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

namespace gpui {
struct PaintCtx;
}

struct MainWindow;
struct Gfx;
enum class TextSelectUnit;

extern Kind kNotifTextSelectMode;

bool CanSelectTextWithKeyboard(MainWindow*);
void ToggleSelectTextWithKeyboard(MainWindow*);
bool SelectTextWithKeyboardActive(MainWindow*);
bool StopSelectTextWithKeyboard(MainWindow*);
bool KeepCaretAfterMarkup(MainWindow*);
void SelectTextWithKeyboardOnKeyUp(MainWindow*, int key);
bool SelectTextWithKeyboardOnKeyDown(MainWindow*, int key);
bool SelectTextWithKeyboardOnKeyDown(MainWindow*, int key, bool ctrl, bool shift, bool alt);
bool SelectTextWithKeyboardOnChar(MainWindow*, int key);
bool CanExtendTextSelection(MainWindow*);
bool ExtendTextSelection(MainWindow*, TextSelectUnit, int dir);
void SelectTextWithKeyboardBlinkCaret(MainWindow*);
void SelectTextWithKeyboardBlinkTick(MainWindow*, int elapsedMs);
void PaintKeyboardTextCaret(MainWindow*, Gfx*);
void PaintKeyboardTextCaret(MainWindow*, gpui::PaintCtx*);

TempStr SelectTextKeyboardResultTemp(int* exitCodeOut);
TempStr SelectTextKeyboardResultTemp(MainWindow* win);
