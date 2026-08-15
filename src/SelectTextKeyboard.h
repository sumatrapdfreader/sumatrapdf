/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;
struct Gfx;
enum class TextSelectUnit;

extern Kind kNotifTextSelectMode;

bool CanSelectTextWithKeyboard(MainWindow*);
void ToggleSelectTextWithKeyboard(MainWindow*);
bool SelectTextWithKeyboardActive(MainWindow*);
bool StopSelectTextWithKeyboard(MainWindow*);
bool SelectTextWithKeyboardOnKeyDown(MainWindow*, WPARAM key);
bool SelectTextWithKeyboardOnChar(MainWindow*, WPARAM key);
bool CanExtendTextSelection(MainWindow*);
bool ExtendTextSelection(MainWindow*, TextSelectUnit, int dir);
void SelectTextWithKeyboardBlinkCaret(MainWindow*);
void PaintKeyboardTextCaret(MainWindow*, Gfx*);

TempStr SelectTextKeyboardResultTemp(int* exitCodeOut);
