/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// Turning "Ctrl + Shift + F5" into an ACCEL and back into something to show in
// a menu. The key-name tables and the parsing are the same wherever shortcuts
// are configurable, so they live here rather than in any one app's
// accelerator table.

#include "base/Base.h"

#include "ShortcutParse.h"

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
    "\0";
// clang-format on
// @gen-end virt-keys-num

// TOOD: add those as well?
// #define VK_OEM_5          0xDC  //  '\|' for US
// #define VK_OEM_7          0xDE  //  ''"' for US
// #define VK_OEM_102        0xE2  //  "<>" or "\|" on RT 102-key kbd.
// #define VK_OEM_PLUS       0xBB   // '+' any country
// #define VK_OEM_COMMA      0xBC   // ',' any country
// #define VK_OEM_MINUS      0xBD   // '-' any country
// #define VK_OEM_PERIOD     0xBE   // '.' any country
// #define VK_OEM_2          0xBF   // '/?' for US
// #define VK_BROWSER_BACK        0xA6
// #define VK_BROWSER_FORWARD     0xA7
// #define VK_BROWSER_REFRESH     0xA8
// #define VK_BROWSER_STOP        0xA9
// #define VK_BROWSER_SEARCH      0xAA
// #define VK_BROWSER_FAVORITES   0xAB
// #define VK_BROWSER_HOME        0xAC
// #define VK_VOLUME_MUTE         0xAD
// #define VK_MEDIA_NEXT_TRACK    0xB0
// #define VK_MEDIA_PREV_TRACK    0xB1
// #define VK_MEDIA_STOP          0xB2
// #define VK_MEDIA_PLAY_PAUSE    0xB3
// #define VK_LAUNCH_MAIL         0xB4
// #define VK_LAUNCH_MEDIA_SELECT 0xB5
// #define VK_LAUNCH_APP1         0xB6
// #define VK_LAUNCH_APP2         0xB7
// #define VK_OEM_8          0xDF
// #define VK_OEM_AX         0xE1  //  'AX' key on Japanese AX kbd
// #define VK_ICO_HELP       0xE3  //  Help key on ICO
// #define VK_ICO_00         0xE4  //  00 key on ICO
// #define VK_PROCESSKEY     0xE5
// #define VK_OEM_RESET      0xE9
// #define VK_OEM_JUMP       0xEA
// #define VK_OEM_PA1        0xEB
// #define VK_OEM_PA2        0xEC
// #define VK_OEM_PA3        0xED
// #define VK_OEM_WSCTRL     0xEE
// #define VK_OEM_CUSEL      0xEF
// #define VK_OEM_ATTN       0xF0
// #define VK_OEM_FINISH     0xF1
// #define VK_OEM_COPY       0xF2
// #define VK_OEM_AUTO       0xF3
// #define VK_OEM_ENLW       0xF4
// #define VK_OEM_BACKTAB    0xF5
// #define VK_ATTN           0xF6
// #define VK_CRSEL          0xF7
// #define VK_EXSEL          0xF8
// #define VK_EREOF          0xF9
// #define VK_PLAY           0xFA
// #define VK_ZOOM           0xFB
// #define VK_NONAME         0xFC
// #define VK_PA1            0xFD

static void skipWS(Str& s) {
    while (len(s) > 0) {
        if (!str::IsWs(*s.s)) {
            return;
        }
        s.s++;
        s.len--;
    }
}

static void skipPlusOrMinus(Str& s) {
    if (len(s) > 0 && *s.s == '+') {
        s.s++;
        s.len--;
        return;
    }
    if (len(s) > 0 && *s.s == '-') {
        s.s++;
        s.len--;
        return;
    }
}

static bool skipVirtKey(Str& s, Str key) {
    if (!str::StartsWithI(s, key)) {
        return false;
    }
    s.s += key.len;
    s.len -= key.len;
    skipWS(s);
    skipPlusOrMinus(s);
    skipWS(s);
    return true;
}

// used in menu shortcuts
static TempStr getVirtTemp(BYTE key, bool isEng) {
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

// Parses a string like Ctrl+Shift+A into ACCEL structure
// We accept variants: "Ctrl+A", "Ctrl-A", "Ctrl + A"
static bool ParseShortcut(Str shortcut, ACCEL& accel) {
    TempStr shortcutZ = str::DupTemp(shortcut);
    Str cursor = shortcutZ;

    BYTE fVirt = 0;

again:
    skipWS(cursor);
    if (skipVirtKey(cursor, "alt")) {
        fVirt |= (FALT | FVIRTKEY);
        goto again;
    }
    if (skipVirtKey(cursor, "shift")) {
        fVirt |= (FSHIFT | FVIRTKEY);
        goto again;
    }
    if (skipVirtKey(cursor, "ctrl")) {
        fVirt |= (FCONTROL | FVIRTKEY);
        goto again;
    }
    accel.fVirt = fVirt;

    // when user puts e.g. "~" it's actually "`" but with SHIFT
    static Str shiftKeys = Str("~`,<.>/?;:'\"-_=+[{]}\\|");
    char buf[2] = {};
    Str toFind = cursor;
    bool usedShiftKeyMap = false;
    if (cursor.len == 1) {
        int idx = str::IndexOfChar(shiftKeys, *cursor.s);
        if ((idx >= 0) && (idx % 2 == 1)) {
            buf[0] = shiftKeys.s[idx - 1];
            toFind = Str(buf, 1);
            accel.key = (WORD)(unsigned char)buf[0];
            accel.fVirt |= (FSHIFT | FVIRTKEY);
            usedShiftKeyMap = true;
        }
    }

    // check for keys like F1, Del, Backspace etc.
    i64 vk = 0;
    int idx = SeqStrNumIndexIS(gVirtKeysNum, toFind, &vk);
    if (idx >= 0) {
        accel.key = (BYTE)vk;
        accel.fVirt |= FVIRTKEY;
        return true;
    }
    if (usedShiftKeyMap) {
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
        WCHAR wc = *ws.s;
        // https://github.com/sumatrapdfreader/sumatrapdf/issues/4490
        // handle cyrrilic / hebrew keyboards where shortcut character
        // is unicode and needs to be translated to virtual char
        HKL kl = GetKeyboardLayout(0);
        SHORT key = VkKeyScanExW(wc, kl);
        if (key == -1) {
            logf("can't map char 0x%x\n", (int)wc);
            return false;
        }
        // https://docs.microsoft.com/en-gb/windows/win32/api/winuser/nf-winuser-vkkeyscanexw
        // ... high-order byte contains the shift state,
        // 1 Either SHIFT key is pressed.
        // 2 Either CTRL key is pressed.
        // 4 Either ALT key is pressed.
        BYTE shiftState = HIBYTE(key);
        BYTE k = LOBYTE(key);
        // logf("mapped char 0x%x as %d (0x%x), shift state: %d\n", (int)wc, (int)k, (int)k, (int)shiftState);
        key = (SHORT)k;
        if (shiftState & 0x1) {
            accel.fVirt |= (FSHIFT | FVIRTKEY);
        }
        if (shiftState & 0x2) {
            accel.fVirt |= (FCONTROL | FVIRTKEY);
        }
        if (shiftState & 0x4) {
            accel.fVirt |= (FALT | FVIRTKEY);
        }
        accel.fVirt |= FVIRTKEY;
        accel.key = (WORD)key;
        return true;
    }
    if (len(s) == 0) {
        return false;
    }
    char c = *s.s;

    // those correspond to 0...9 keys and require SHIFT
    static Str shift09 = StrL(")!@#$%^&*(");
    idx = str::IndexOfChar(shift09, c);
    if (idx >= 0) {
        accel.key = (WORD)('0' + idx);
        accel.fVirt |= (FSHIFT | FVIRTKEY);
        return true;
    }
    if (accel.fVirt == 0) {
        // in 3.6 we marked our shortcuts as virtual so we need to mark user provided
        // virtual as well
        if (c >= 'a' && c <= 'z') {
            accel.fVirt = FVIRTKEY;
            c -= ('a' - 'A');
        } else if (c >= 'A' && c <= 'Z') {
            accel.fVirt = (FVIRTKEY | FSHIFT);
        }
    } else {
        // if we have ctrl/alt/shift, convert 'a' - 'z' into 'A' - 'Z'
        if (c >= 'a' && c <= 'z') {
            c -= ('a' - 'A');
        }
    }
    accel.key = (WORD)(unsigned char)c;
    return true;
}

// true if shortcut names a key we can bind
bool IsValidShortcutString(Str shortcut) {
    ACCEL accel = {};
    accel.cmd = (WORD)-1; // for debugging
    return ParseShortcut(shortcut, accel);
}

// Fills accel with the key and modifiers shortcut names. Returns false if it
// doesn't name a key; accel.cmd is left alone, the caller owns that.
bool ParseShortcutString(Str shortcut, ACCEL& accel) {
    return ParseShortcut(shortcut, accel);
}

// Appends " \tCtrl + O" to a menu string, for the key a is bound to.
TempStr AppendAccelKeyToMenuStringTemp(TempStr menuStr, const ACCEL& a) {
    Str lang = gShortcutLangCode ? gShortcutLangCode() : Str();
    bool isEng = len(lang) == 0 || str::Eq(lang, StrL("en"));
    bool isGerman = str::Eq(lang, StrL("de"));
    bool isAscii = false;

    // "\tCtrl + Shift + Alt + F24" / localized variants fit in ~64 bytes.
    char strScratch[64]{};
    str::Builder str(Str(strScratch, sizeofi(strScratch)));
    str.Append("\t"); // marks start of an accelerator in menu item
    BYTE virt = a.fVirt;
    if (virt & FALT) {
        Str s = "Alt + ";
        if (isGerman) {
            s = "Größe + ";
        }
        str.Append(s);
    }
    if (virt & FCONTROL) {
        Str s = "Ctrl + ";
        if (isGerman) {
            s = "Strg + ";
        }
        str.Append(s);
    }
    if (virt & FSHIFT) {
        Str s = "Shift + ";
        if (isGerman) {
            s = "Umschalt + ";
        }
        str.Append(s);
    }
    bool isVirt = virt & FVIRTKEY;
    BYTE key = (BYTE)a.key;

    if (isVirt) {
        if (key >= VK_NUMPAD0 && key <= VK_NUMPAD9) {
            WCHAR c = (WCHAR)key - VK_NUMPAD0 + '0';
            str.AppendChar((char)c);
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

    logf("Unknown key: 0x%x, virt: 0x%x\n", key, virt);
    ReportIf(true);
    return menuStr;
Exit:
    TempStr res = str::JoinTemp(menuStr, ToStr(str));
    return res;
}
