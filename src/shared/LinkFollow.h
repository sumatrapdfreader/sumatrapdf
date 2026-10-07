/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

namespace gpui {
struct PaintCtx;
}

struct MainWindow;
struct Gfx;

extern Kind kNotifLinkFollow;

bool CanFollowLinksWithKeyboard(MainWindow*);
void ToggleKeyboardLinkFollowing(MainWindow*);
bool KeyboardLinkFollowingActive(MainWindow*);
bool StopKeyboardLinkFollowing(MainWindow*);
bool KeyboardLinkFollowingCapturesKey(MainWindow*, int vk);
bool KeyboardLinkFollowingOnChar(MainWindow*, int key);
void KeyboardLinkFollowingViewportChanged(MainWindow*);
void KeyboardLinkFollowingViewportChanged(MainWindow*, int elapsedMs);
void KeyboardLinkFollowingRecompute(MainWindow*);
void PaintKeyboardLinkTargets(MainWindow*, Gfx*);
void PaintKeyboardLinkTargets(MainWindow*, gpui::PaintCtx*);

TempStr KeyboardLinkFollowResultTemp(Str action, Str chars, int* exitCodeOut);
