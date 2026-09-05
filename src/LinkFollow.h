/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;
struct Gfx;

extern Kind kNotifLinkFollow;

bool CanFollowLinksWithKeyboard(MainWindow*);
void ToggleKeyboardLinkFollowing(MainWindow*);
bool KeyboardLinkFollowingActive(MainWindow*);
bool StopKeyboardLinkFollowing(MainWindow*);
bool KeyboardLinkFollowingCapturesKey(MainWindow*, WPARAM vk);
bool KeyboardLinkFollowingOnChar(MainWindow*, WPARAM key);
void KeyboardLinkFollowingViewportChanged(MainWindow*);
void KeyboardLinkFollowingRecompute(MainWindow*);
void PaintKeyboardLinkTargets(MainWindow*, Gfx*);

TempStr KeyboardLinkFollowResultTemp(Str action, Str chars, int* exitCodeOut);
