/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;

extern Kind kNotifLinkFollow;

constexpr int kMaxKeyboardLinkTargets = 9;

bool CanFollowLinksWithKeyboard(MainWindow*);
void ToggleKeyboardLinkFollowing(MainWindow*);
bool KeyboardLinkFollowingActive(MainWindow*);
bool StopKeyboardLinkFollowing(MainWindow*);
bool KeyboardLinkFollowingOnChar(MainWindow*, WPARAM key);
void KeyboardLinkFollowingViewportChanged(MainWindow*);
void KeyboardLinkFollowingRecompute(MainWindow*);
void PaintKeyboardLinkTargets(MainWindow*, HDC);

TempStr KeyboardLinkFollowResultTemp(int* exitCodeOut);
