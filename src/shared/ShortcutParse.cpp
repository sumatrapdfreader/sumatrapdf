/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// Turning "Ctrl + Shift + F5" into an ACCEL and back into something to show in
// a menu. The key-name tables and the parsing are the same wherever shortcuts
// are configurable, so they live here rather than in any one app's
// accelerator table.

#include "base/Base.h"

#if !OS_WIN
#include "VirtKeys.h"
#endif
#include "ShortcutParse.h"

constexpr u8 kVirt = KeyShortcut::kVirtKey;
constexpr u8 kShift = KeyShortcut::kShiftKey;
constexpr u8 kCtrl = KeyShortcut::kCtrlKey;
constexpr u8 kAlt = KeyShortcut::kAltKey;

u8 KeyShortcut::Mods() const {
    u8 res = isVirt ? kVirt : 0;
    res |= shift ? kShift : 0;
    res |= ctrl ? kCtrl : 0;
    res |= alt ? kAlt : 0;
    return res;
}

bool KeyShortcut::SameKey(const KeyShortcut& o) const {
    return vk == o.vk && Mods() == o.Mods();
}

// Which language key names are printed in. Null means English.
Str (*gShortcutLangCode)() = nullptr;

// http://www.kbdedit.com/manual/low_level_vk_list.html
// https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
// virtual key names: see cmd/gen-code.ts virtKeys (regenerate with bun cmd/gen-code.ts)

// @gen-start virt-keys-num
// clang-format off
static SeqStrNum gVirtKeysNum =
    "numpad0\0" "\xc0\x01" \
    "numpad1\0" "\xc2\x01" \
    "numpad2\0" "\xc4\x01" \
    "numpad3\0" "\xc6\x01" \
    "numpad4\0" "\xc8\x01" \
    "numpad5\0" "\xca\x01" \
    "numpad6\0" "\xcc\x01" \
    "numpad7\0" "\xce\x01" \
    "numpad8\0" "\xd0\x01" \
    "numpad9\0" "\xd2\x01" \
    "Tab\0" "\x12" \
    "End\0" "\x46" \
    "Home\0" "\x48" \
    "Left\0" "\x4a" \
    "Right\0" "\x4e" \
    "Up\0" "\x4c" \
    "Down\0" "\x50" \
    "PageDown\0" "\x44" \
    "PgDown\0" "\x44" \
    "PageUp\0" "\x42" \
    "PgUp\0" "\x42" \
    "Back\0" "\x10" \
    "Backspace\0" "\x10" \
    "Del\0" "\x5c" \
    "Delete\0" "\x5c" \
    "Ins\0" "\x5a" \
    "Insert\0" "\x5a" \
    "Esc\0" "\x36" \
    "Escape\0" "\x36" \
    "Return\0" "\x1a" \
    "Convert\0" "\x38" \
    "NoConvert\0" "\x3a" \
    "Space\0" "\x40" \
    "*\0" "\xd4\x01" \
    "Multiply\0" "\xd4\x01" \
    "Mult\0" "\xd4\x01" \
    "+\0" "\xd6\x01" \
    "+\0" "\xf6\x02" \
    "Add\0" "\xd6\x01" \
    "-\0" "\xfa\x02" \
    "-\0" "\xda\x01" \
    "Subtract\0" "\xda\x01" \
    "Sub\0" "\xda\x01" \
    "/\0" "\xde\x01" \
    "Divide\0" "\xde\x01" \
    "Div\0" "\xde\x01" \
    "Help\0" "\x5e" \
    "Select\0" "\x52" \
    "Volume Down\0" "\xdc\x02" \
    "VolumeDown\0" "\xdc\x02" \
    "Volume Up\0" "\xde\x02" \
    "VolumeUp\0" "\xde\x02" \
    "XButton1\0" "\x0a" \
    "XButton2\0" "\x0c" \
    "F1\0" "\xe0\x01" \
    "F2\0" "\xe2\x01" \
    "F3\0" "\xe4\x01" \
    "F4\0" "\xe6\x01" \
    "F5\0" "\xe8\x01" \
    "F6\0" "\xea\x01" \
    "F7\0" "\xec\x01" \
    "F8\0" "\xee\x01" \
    "F9\0" "\xf0\x01" \
    "F10\0" "\xf2\x01" \
    "F11\0" "\xf4\x01" \
    "F12\0" "\xf6\x01" \
    "F13\0" "\xf8\x01" \
    "F14\0" "\xfa\x01" \
    "F15\0" "\xfc\x01" \
    "F16\0" "\xfe\x01" \
    "F17\0" "\x80\x02" \
    "F18\0" "\x82\x02" \
    "F19\0" "\x84\x02" \
    "F20\0" "\x86\x02" \
    "F21\0" "\x88\x02" \
    "F22\0" "\x8a\x02" \
    "F23\0" "\x8c\x02" \
    "F24\0" "\x8e\x02" \
    "Clear\0" "\x18" \
    "Accept\0" "\x3c" \
    "ModeChange\0" "\x3e" \
    "Print\0" "\x54" \
    "Execute\0" "\x56" \
    "PrtSc\0" "\x58" \
    "PrintScreen\0" "\x58" \
    "Sleep\0" "\xbe\x01" \
    "Separator\0" "\xd8\x01" \
    "Decimal\0" "\xdc\x01" \
    "Scroll\0" "\xa2\x02" \
    ";\0" "\xf4\x02" \
    "/\0" "\xfe\x02" \
    "`\0" "\x80\x03" \
    "[\0" "\xb6\x03" \
    "]\0" "\xba\x03" \
    "=\0" "\xf6\x02" \
    ",\0" "\xf8\x02" \
    ".\0" "\xfc\x02" \
    "\\\0" "\xb8\x03" \
    "'\0" "\xbc\x03" \
    "\0";
// clang-format on
// @gen-end virt-keys-num

static bool skipVirtKey(Str& s, Str key) {
    if (!str::StartsWithI(s, key)) {
        return false;
    }
    s.s += key.len;
    s.len -= key.len;
    str::TrimAny(s, " \t+-");
    return true;
}

// used in menu shortcuts
static TempStr getVirtTemp(u8 key, bool isEng) {
    // over-rides for non-english languages
    if (!isEng) {
        switch (key) {
            case VK_LEFT:
                return StrL("<-");
            case VK_RIGHT:
                return StrL("->");
        }
    }
    return SeqStrNumStrByNumber(gVirtKeysNum, key);
}

// US layout: Shift + these keys is the glyph people type (Shift+/ is "?").
static const struct {
    u8 vk;
    char unshifted;
    char shifted;
} kPunctKeys[] = {
    {VK_OEM_2, '/', '?'}, {VK_OEM_COMMA, ',', '<'}, {VK_OEM_PERIOD, '.', '>'},
    {VK_OEM_4, '[', '{'}, {VK_OEM_6, ']', '}'},     {VK_OEM_5, '\\', '|'},
    {VK_OEM_1, ';', ':'}, {VK_OEM_7, '\'', '"'},    {VK_OEM_3, '`', '~'},
};

static u8 PunctVk(char unshifted) {
    for (auto& p : kPunctKeys) {
        if (unshifted == p.unshifted) {
            return p.vk;
        }
    }
    return 0;
}

// Parses a string like Ctrl+Shift+A into a KeyShortcut
// We accept variants: "Ctrl+A", "Ctrl-A", "Ctrl + A"
static bool ParseShortcut(Str shortcut, KeyShortcut& sc) {
    TempStr shortcutZ = str::DupTemp(shortcut);
    Str cursor = shortcutZ;

    u8 fVirt = 0;

again:
    str::TrimWs(cursor);
    // before "alt": "AltGr + Return" would otherwise match "alt" and leave "Gr"
    if (skipVirtKey(cursor, StrL("altgr")) || skipVirtKey(cursor, StrL("ralt")) ||
        skipVirtKey(cursor, StrL("rightalt"))) {
        // Windows reports Right Alt / AltGr as Ctrl+Alt
        fVirt |= (kCtrl | kAlt | kVirt);
        goto again;
    }
    if (skipVirtKey(cursor, StrL("alt"))) {
        fVirt |= (kAlt | kVirt);
        goto again;
    }
    if (skipVirtKey(cursor, StrL("shift"))) {
        fVirt |= (kShift | kVirt);
        goto again;
    }
    if (skipVirtKey(cursor, StrL("ctrl"))) {
        fVirt |= (kCtrl | kVirt);
        goto again;
    }
    if (skipVirtKey(cursor, StrL("global"))) {
        goto again;
    }

    // when user puts e.g. "~" it's actually "`" but with SHIFT
    static Str shiftKeys = Str("`~,<.>/?;:'\"-_=+[{]}\\|");
    char buf[2] = {};
    Str toFind = cursor;
    bool usedShiftKeyMap = false;
    u16 key = 0;
    if (cursor.len == 1) {
        int idx = str::IndexOfChar(shiftKeys, *cursor.s);
        if ((idx >= 0) && (idx % 2 == 1)) {
            buf[0] = shiftKeys.s[idx - 1];
            toFind = Str(buf, 1);
            key = (u16)(unsigned char)buf[0];
            fVirt |= (kShift | kVirt);
            usedShiftKeyMap = true;
        }
    }

    // check for keys like F1, Del, Backspace etc.
    i64 vk = 0;
    int idx = SeqStrNumIndexIS(gVirtKeysNum, toFind, &vk);
    if (idx >= 0) {
        sc = KeyShortcut(fVirt | kVirt, (u8)vk);
        return true;
    }
    if (usedShiftKeyMap) {
        u8 punctVk = PunctVk(buf[0]);
        if (punctVk) {
            key = punctVk;
        }
        sc = KeyShortcut(fVirt, key);
        return true;
    }

    // now we expect a character like 'a' or 'P'
    TempStr s = cursor;
    if (len(s) > 1) {
        s = str::DupTemp(cursor);
        str::TrimWSInPlace(s, str::TrimOpt::Both);
    }
    if (len(s) > 1) {
        // possibly a unicode character
        TempWStr ws = ToWStrTemp(s);
        if (len(ws) != 1) {
            return false;
        }
#if OS_WIN
        WCHAR wc = *ws.s;
        // https://github.com/sumatrapdfreader/sumatrapdf/issues/4490
        // handle cyrrilic / hebrew keyboards where shortcut character
        // is unicode and needs to be translated to virtual char
        HKL kl = GetKeyboardLayout(0);
        SHORT vkAndShift = VkKeyScanExW(wc, kl);
        if (vkAndShift == -1) {
            logf("can't map char 0x%x\n", (int)wc);
            return false;
        }
        // https://docs.microsoft.com/en-gb/windows/win32/api/winuser/nf-winuser-vkkeyscanexw
        // ... high-order byte contains the shift state,
        // 1 Either SHIFT key is pressed.
        // 2 Either CTRL key is pressed.
        // 4 Either ALT key is pressed.
        BYTE shiftState = HIBYTE(vkAndShift);
        key = (u16)LOBYTE(vkAndShift);
        if (shiftState & 0x1) {
            fVirt |= (kShift | kVirt);
        }
        if (shiftState & 0x2) {
            fVirt |= (kCtrl | kVirt);
        }
        if (shiftState & 0x4) {
            fVirt |= (kAlt | kVirt);
        }
        sc = KeyShortcut(fVirt | kVirt, key);
        return true;
#else
        // ng: POSIX gap. Mapping a unicode character to a virtual key needs
        // the active keyboard layout (win32 VkKeyScanExW); gpui doesn't
        // expose one yet, so non-ASCII shortcut characters don't bind.
        logf("can't map char 0x%x (no keyboard layout)\n", (int)*ws.s);
        return false;
#endif
    }
    if (len(s) == 0) {
        return false;
    }
    char c = *s.s;

    // those correspond to 0...9 keys and require SHIFT
    static Str shift09 = StrL(")!@#$%^&*(");
    idx = str::IndexOfChar(shift09, c);
    if (idx >= 0) {
        sc = KeyShortcut(fVirt | kShift | kVirt, (u16)('0' + idx));
        return true;
    }
    if (fVirt == 0) {
        // in 3.6 we marked our shortcuts as virtual so we need to mark user provided
        // virtual as well
        if (c >= 'a' && c <= 'z') {
            fVirt = kVirt;
            c -= ('a' - 'A');
        } else if (c >= 'A' && c <= 'Z') {
            fVirt = (kVirt | kShift);
        }
    } else {
        // if we have ctrl/alt/shift, convert 'a' - 'z' into 'A' - 'Z'
        if (c >= 'a' && c <= 'z') {
            c -= ('a' - 'A');
        }
    }
    sc = KeyShortcut(fVirt, (u16)(unsigned char)c);
    return true;
}

// true if shortcut names a key we can bind
bool IsValidShortcutString(Str shortcut) {
    KeyShortcut sc;
    return ParseShortcut(shortcut, sc);
}

int TrimGlobalPrefix(Str& shortcut) {
    Str s = shortcut;
    str::TrimWs(s);
    if (!str::TrimPrefixI(s, StrL("global"))) {
        return 0;
    }
    if (!str::TrimAny(s, " \t+-")) {
        return 0;
    }
    if (len(s) == 0) {
        return 0;
    }
    int trimmed = len(shortcut) - len(s);
    shortcut = s;
    return trimmed;
}

bool IsGlobalShortcut(Str shortcut) {
    return TrimGlobalPrefix(shortcut);
}

// Fills sc with the key and modifiers shortcut names. Returns false if it
// doesn't name a key.
bool ParseShortcutString(Str shortcut, KeyShortcut& sc) {
    return ParseShortcut(shortcut, sc);
}

// only a VK_OEM code: VK_RIGHT is 0x27, same as '\''
static char ShiftedPunctGlyph(u8 key) {
    for (auto& p : kPunctKeys) {
        if (key == p.vk) {
            return p.shifted;
        }
    }
    return 0;
}

// Appends " \tCtrl + O" to a menu string, for the key sc is bound to.
TempStr AppendAccelKeyToMenuStringTemp(TempStr menuStr, const KeyShortcut& sc) {
    Str lang = gShortcutLangCode ? gShortcutLangCode() : Str();
    bool isEng = len(lang) == 0 || str::Eq(lang, StrL("en"));
    bool isGerman = str::Eq(lang, StrL("de"));
    bool isAscii = false;

    // "\tCtrl + Shift + Alt + F24" / localized variants fit in ~64 bytes.
    char strScratch[64]{};
    str::Builder str;
    str::BuilderUseExternalBuffer(str, Str(strScratch, sizeofi(strScratch)));
    str.Append(StrL("\t")); // marks start of an accelerator in menu item
    u8 key = (u8)sc.vk;
    bool isVirt = sc.isVirt;
    char shiftedPunct = sc.shift ? ShiftedPunctGlyph(key) : 0;
    if (sc.alt && sc.ctrl) {
        // same bits as AltGr on Windows; keep the name the user would type
        str.Append(StrL("AltGr + "));
    } else if (sc.alt) {
        Str s = StrL("Alt + ");
        if (isGerman) {
            s = StrL("Größe + ");
        }
        str.Append(s);
    } else if (sc.ctrl) {
        Str s = StrL("Ctrl + ");
        if (isGerman) {
            s = StrL("Strg + ");
        }
        str.Append(s);
    }
    if (sc.shift && !shiftedPunct) {
        Str s = StrL("Shift + ");
        if (isGerman) {
            s = StrL("Umschalt + ");
        }
        str.Append(s);
    }
    if (shiftedPunct) {
        str.AppendChar(shiftedPunct);
        goto Exit;
    }

    if (isVirt) {
        if (key >= VK_NUMPAD0 && key <= VK_NUMPAD9) {
            char c = (char)(key - VK_NUMPAD0 + '0');
            str.AppendChar(c);
            goto Exit;
        }
        if (key >= VK_F1 && key <= VK_F24) {
            int n = key - VK_F1 + 1;
            str.Append(fmt("F%d", n));
            goto Exit;
        }
        TempStr s = getVirtTemp(key, isEng);
        if (s) {
            str.Append(s);
            goto Exit;
        }
    }

    // virtual codes overlap with some ascii chars like '-' is VK_INSERT
    // so for non-virtual assume it's a single char
    isAscii = (key >= 'A' && key <= 'Z') || (key >= 'a' && key <= 'z') || (key >= '0' && key <= '9');
    static Str otherAscii = Str("[]'`~@#$%^&*(){}/\\|?<>!,.+-=_;:\"");
    if (str::ContainsChar(otherAscii, (char)key)) {
        isAscii = true;
    }
    if (isAscii) {
        str.AppendChar((char)key);
        goto Exit;
    }

    logf("Unknown key: 0x%x, virt: 0x%x\n", key, sc.Mods());
    ReportIf(true);
    return menuStr;
Exit:
    TempStr res = str::JoinTemp(menuStr, ToStr(str));
    return res;
}

static const struct {
    u8 vk;
    Str name;
} kGpuiKeyNames[] = {
    {VK_BACK, StrL("backspace")},      {VK_TAB, StrL("tab")},       {VK_RETURN, StrL("enter")},
    {VK_ESCAPE, StrL("escape")},       {VK_SPACE, StrL("space")},   {VK_PRIOR, StrL("pageup")},
    {VK_NEXT, StrL("pagedown")},       {VK_END, StrL("end")},       {VK_HOME, StrL("home")},
    {VK_LEFT, StrL("left")},           {VK_UP, StrL("up")},         {VK_RIGHT, StrL("right")},
    {VK_DOWN, StrL("down")},           {VK_INSERT, StrL("insert")}, {VK_DELETE, StrL("delete")},
    {VK_APPS, StrL("menu")},           {VK_ADD, StrL("add")},       {VK_SUBTRACT, StrL("subtract")},
    {VK_MULTIPLY, StrL("multiply")},   {VK_DIVIDE, StrL("divide")}, {VK_DECIMAL, StrL("decimal")},
    {VK_SEPARATOR, StrL("separator")},
};

static Str GpuiKeyName(u16 vk) {
    for (const auto& key : kGpuiKeyNames) {
        if (key.vk == vk) {
            return key.name;
        }
    }
    for (const auto& key : kPunctKeys) {
        if (key.vk == vk) {
            return Str(&key.unshifted, 1);
        }
    }
    if (vk == VK_OEM_PLUS) {
        return StrL("=");
    }
    if (vk == VK_OEM_MINUS) {
        return StrL("-");
    }
    if (vk >= VK_F1 && vk <= VK_F24) {
        return fmt("f%d", vk - VK_F1 + 1);
    }
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        return fmt("numpad%d", vk - VK_NUMPAD0);
    }
    if (vk >= '0' && vk <= '9') {
        return fmt("%c", (char)vk);
    }
    if (vk >= 'A' && vk <= 'Z') {
        return fmt("%c", (char)(vk + ('a' - 'A')));
    }
    return {};
}

// A gpui KeyBinding stroke, e.g. "ctrl-shift-f5". Empty when gpui has no name
// for the key, so the caller can skip the binding.
TempStr ShortcutToGpuiStroke(const KeyShortcut& sc) {
    Str key;
    if (sc.isVirt) {
        key = GpuiKeyName(sc.vk);
    } else if (sc.vk >= ' ' && sc.vk < 127) {
        // not a virtual key: vk is the character to match
        char c = (char)sc.vk;
        if (c >= 'A' && c <= 'Z') {
            c += ('a' - 'A');
        }
        key = fmt("%c", c);
    }
    if (len(key) == 0) {
        return {};
    }
    TempStr res = str::DupTemp(StrL(""));
    if (sc.ctrl) {
        res = str::JoinTemp(res, StrL("ctrl-"));
    }
    if (sc.alt) {
        res = str::JoinTemp(res, StrL("alt-"));
    }
    if (sc.shift) {
        res = str::JoinTemp(res, StrL("shift-"));
    }
    return str::JoinTemp(res, key);
}

#if OS_WIN
bool ParseShortcutString(Str shortcut, ACCEL& accel) {
    KeyShortcut sc;
    if (!ParseShortcutString(shortcut, sc)) {
        return false;
    }
    accel.fVirt = sc.Mods();
    accel.key = sc.vk;
    return true;
}

TempStr AppendAccelKeyToMenuStringTemp(TempStr menuStr, const ACCEL& accel) {
    KeyShortcut sc(accel.fVirt, accel.key);
    return AppendAccelKeyToMenuStringTemp(menuStr, sc);
}
#endif
