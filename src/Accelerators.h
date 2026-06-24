/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

void FreeAcceleratorTables();
void CreateSumatraAcceleratorTable();
HACCEL* GetAcceleratorTables();
TempStr AppendAccelKeyToMenuStringTemp(TempStr str, int cmdId);
bool IsValidShortcutString(const char* shortcut);
bool ParseShortcutString(const char* shortcut, ACCEL& accel);

// Command bound to a key+modifiers among the accelerators that are "safe" to
// process while a custom control (edit / tree / WebView2-hosted CHM) has focus.
// Returns the command id, or 0 if none. Used to forward app shortcuts that a
// focused control would otherwise swallow.
int SafeAcceleratorCmd(u16 vk, bool ctrl, bool shift, bool alt);
