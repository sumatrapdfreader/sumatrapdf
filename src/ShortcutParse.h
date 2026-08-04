/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// Parsing and printing keyboard shortcut strings ("Ctrl + Shift + F5"), shared
// by any app that lets shortcuts be configured.

// The app's current language code, used to print key names ("Shift" vs
// "Umschalt"). Null, or returning empty, means English.
extern Str (*gShortcutLangCode)();

// true if shortcut names a key we can bind
bool IsValidShortcutString(Str shortcut);

// Fills accel with the key and modifiers shortcut names. Returns false if it
// doesn't name a key; accel.cmd is left alone, the caller owns that.
bool ParseShortcutString(Str shortcut, ACCEL& accel);

// Appends " \tCtrl + O" to a menu string, for the key a is bound to.
TempStr AppendAccelKeyToMenuStringTemp(TempStr menuStr, const ACCEL& a);
