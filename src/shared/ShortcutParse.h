/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef SUMATRA_SHORTCUT_PARSE_H
#define SUMATRA_SHORTCUT_PARSE_H

struct KeyShortcut {
    enum : u8 {
        kVirtKey = 1,
        kShiftKey = 4,
        kCtrlKey = 8,
        kAltKey = 16,
    };

    u16 vk = 0;
    bool ctrl = false;
    bool shift = false;
    bool alt = false;
    bool isVirt = false;

    constexpr KeyShortcut() = default;
    constexpr KeyShortcut(u8 mods, u16 key)
        : vk(key),
          ctrl((mods & kCtrlKey) != 0),
          shift((mods & kShiftKey) != 0),
          alt((mods & kAltKey) != 0),
          isVirt((mods & kVirtKey) != 0) {}

    u8 Mods() const;
    bool SameKey(const KeyShortcut&) const;
};

extern Str (*gShortcutLangCode)();

bool IsValidShortcutString(Str shortcut);
bool IsGlobalShortcut(Str shortcut);
int TrimGlobalPrefix(Str& shortcut);
bool ParseShortcutString(Str shortcut, KeyShortcut& sc);
TempStr AppendAccelKeyToMenuStringTemp(TempStr menuStr, const KeyShortcut& sc);
TempStr ShortcutToGpuiStroke(const KeyShortcut& sc);

#if OS_WIN
bool ParseShortcutString(Str shortcut, ACCEL& accel);
TempStr AppendAccelKeyToMenuStringTemp(TempStr menuStr, const ACCEL& a);
#endif

#endif
