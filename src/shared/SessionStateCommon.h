/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct TabState;
struct WindowTab;

TabState* NewTabStateFromTab(WindowTab* tab);
void SetTabState(WindowTab* tab, TabState* state);
void SaveWindowSessionState();
