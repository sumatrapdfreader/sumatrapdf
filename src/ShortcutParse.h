/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// Parsing and printing keyboard shortcut strings ("Ctrl + Shift + F5"), shared
// by any app that lets shortcuts be configured.

// The app's current language code, used to print key names ("Shift" vs
// "Umschalt"). Null, or returning empty, means English.
extern Str (*gShortcutLangCode)();

bool IsValidShortcutString(Str shortcut);
bool IsGlobalShortcut(Str shortcut);
int TrimGlobalPrefix(Str& shortcut);

#if OS_WIN
bool ParseShortcutString(Str shortcut, ACCEL& accel);
TempStr AppendAccelKeyToMenuStringTemp(TempStr menuStr, const ACCEL& a);
#endif
