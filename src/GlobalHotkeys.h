/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef GLOBALHOTKEYS_H_
#define GLOBALHOTKEYS_H_

void RegisterGlobalHotkeys(HWND hwnd);
void UnregisterGlobalHotkeys(HWND hwnd);
void ReRegisterGlobalHotkeys();
bool HandleGlobalHotkey(int hotkeyId);

void GlobalHotkeysOnActivate(HWND hwnd);
void GlobalHotkeysOnDestroy(HWND hwnd);
HWND GetGlobalHotkeysHwnd();

#endif
