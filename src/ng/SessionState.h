/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;
struct WindowTab;
class EngineBase;

// makes SaveSettings() snapshot the open windows (orig does it inline)
void InstallSessionStateHook();

// orig's WinMain: keep the session we started with alive and give gSettings a
// fresh (empty) one to fill
void TakeInitialSessionData();
bool SettingsRestoreSession();
// restores the tabs of the last session into `win`; false if there was nothing
bool RestoreSession(MainWindow* win, MainWindow** startWinOut);
void LoadLazyTabIfNeeded(WindowTab* tab);
void FinishLazyTabLoad(WindowTab* tab, EngineBase* engine);
