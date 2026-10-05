/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// Parsing and printing keyboard shortcut strings ("Ctrl + Shift + F5"), shared
// by any app that lets shortcuts be configured.

// ng: orig parses into a win32 ACCEL. KeyShortcut is the same thing without
// <windows.h>; the virtual key codes stay the win32 ones (VirtKeys.h defines
// them off Windows) because that is what the settings file and the key-name
// tables are written in.
struct KeyShortcut {
    // same bits as win32 FVIRTKEY / FSHIFT / FCONTROL / FALT
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
    // orig's FVIRTKEY. Without it the shortcut matches the typed character
    // rather than the virtual key (one built-in accelerator uses that).
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

// The app's current language code, used to print key names ("Shift" vs
// "Umschalt"). Null, or returning empty, means English.
extern Str (*gShortcutLangCode)();

bool IsValidShortcutString(Str shortcut);
bool IsGlobalShortcut(Str shortcut);
int TrimGlobalPrefix(Str& shortcut);

bool ParseShortcutString(Str shortcut, KeyShortcut& sc);
TempStr AppendAccelKeyToMenuStringTemp(TempStr menuStr, const KeyShortcut& sc);

// gpui KeyBinding stroke ("ctrl-shift-f5"), for gpui::KeymapBind().
// Empty when gpui has no name for the key.
TempStr ShortcutToGpuiStroke(const KeyShortcut& sc);
