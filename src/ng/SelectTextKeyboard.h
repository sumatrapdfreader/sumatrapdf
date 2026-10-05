/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

namespace gpui {
struct PaintCtx;
}

struct MainWindow;
enum class TextSelectUnit;

extern Kind kNotifTextSelectMode;

bool CanSelectTextWithKeyboard(MainWindow*);
void ToggleSelectTextWithKeyboard(MainWindow*);
bool SelectTextWithKeyboardActive(MainWindow*);
bool StopSelectTextWithKeyboard(MainWindow*);
// ng: orig reads the modifiers with GetKeyState(); gpui reports them with the
// key event, so they are passed in
bool SelectTextWithKeyboardOnKeyDown(MainWindow*, int key, bool ctrl, bool shift, bool alt);
bool SelectTextWithKeyboardOnChar(MainWindow*, int key);
bool CanExtendTextSelection(MainWindow*);
bool ExtendTextSelection(MainWindow*, TextSelectUnit, int dir);
// ng: orig blinks the caret on a WM_TIMER; the shell's tick calls this
void SelectTextWithKeyboardBlinkTick(MainWindow*, int elapsedMs);
void PaintKeyboardTextCaret(MainWindow*, gpui::PaintCtx*);

TempStr SelectTextKeyboardResultTemp(MainWindow* win);
