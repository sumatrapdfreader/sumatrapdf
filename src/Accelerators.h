/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

void FreeAcceleratorTables();
void CreateSumatraAcceleratorTable();
HACCEL* GetAcceleratorTables();
// the key cmdId is bound to, appended to a menu string. Parsing shortcut
// strings lives in ShortcutParse.h.
TempStr AppendAccelKeyToMenuStringTemp(TempStr str, int cmdId);

// Command bound to a key+modifiers among the accelerators that are "safe" to
// process while a custom control (edit / tree / WebView2-hosted CHM) has focus.
// Returns the command id, or 0 if none. Used to forward app shortcuts that a
// focused control would otherwise swallow.
int SafeAcceleratorCmd(u16 vk, bool ctrl, bool shift, bool alt);
