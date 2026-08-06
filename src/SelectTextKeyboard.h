/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;

extern Kind kNotifTextSelectMode;

bool CanSelectTextWithKeyboard(MainWindow*);
void ToggleSelectTextWithKeyboard(MainWindow*);
bool SelectTextWithKeyboardActive(MainWindow*);
bool StopSelectTextWithKeyboard(MainWindow*);
bool SelectTextWithKeyboardOnKeyDown(MainWindow*, WPARAM key);
bool SelectTextWithKeyboardOnChar(MainWindow*, WPARAM key);
void SelectTextWithKeyboardBlinkCaret(MainWindow*);
void PaintKeyboardTextCaret(MainWindow*, HDC);

TempStr SelectTextKeyboardResultTemp(int* exitCodeOut);
