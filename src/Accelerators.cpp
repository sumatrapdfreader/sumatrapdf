/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "Settings.h"
#include "Translations.h"
#include "Commands.h"

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

// note: even letter shortcuts like 'k' are marked as FVIRTKEY so that they
// work even on non-english keyboards (cyrillic, hebrew)
// VK_A is 'A' etc. which corresponds to 'a' key.
// To get 'A' need explicitly use FSHIFT.
// https://learn.microsoft.com/en-us/windows/win32/menurc/using-keyboard-accelerators?referrer=grok.com
// https://grok.com/share/bGVnYWN5_d83c2956-4ce2-4c74-ba4d-9794d1760ccb?rid=746312cc-7d0f-4479-abec-25c394652cac
ACCEL gBuiltInAccelerators[] = {
    {FVIRTKEY, 'K', CmdScrollUp},
    {FVIRTKEY, 'J', CmdScrollDown},
    {FVIRTKEY, 'H', CmdScrollLeft},
    {FVIRTKEY, 'L', CmdScrollRight},
    {FVIRTKEY, VK_UP, CmdScrollUp},
    {FVIRTKEY, VK_DOWN, CmdScrollDown},
    {FVIRTKEY, VK_LEFT, CmdScrollLeft},
    {FVIRTKEY, VK_RIGHT, CmdScrollRight},

    {FSHIFT | FVIRTKEY, VK_UP, CmdScrollUpHalfPage},
    {FSHIFT | FVIRTKEY, VK_DOWN, CmdScrollDownHalfPage},

    {FSHIFT | FVIRTKEY, VK_LEFT, CmdScrollLeftPage},
    {FSHIFT | FVIRTKEY, VK_RIGHT, CmdScrollRightPage},

    // TODO: maybe CmdGoToNextPage / CmdGoToPrevPage is better
    {FVIRTKEY, VK_NEXT, CmdScrollDownPage},
    {FVIRTKEY, VK_PRIOR, CmdScrollUpPage},

    {FVIRTKEY, VK_SPACE, CmdScrollDownPage},
    {FVIRTKEY, VK_RETURN, CmdScrollDownPage},
    {FCONTROL | FVIRTKEY, VK_DOWN, CmdScrollDownPage},

    {FSHIFT | FVIRTKEY, VK_SPACE, CmdScrollUpPage},
    {FSHIFT | FVIRTKEY, VK_RETURN, CmdScrollUpPage},
    {FCONTROL | FVIRTKEY, VK_UP, CmdScrollUpPage},

    {FVIRTKEY, 'N', CmdGoToNextPage},
    //{FCONTROL | FVIRTKEY, VK_NEXT, CmdGoToNextPage},

    {FVIRTKEY, 'P', CmdGoToPrevPage},
    //{FCONTROL | FVIRTKEY, VK_PRIOR, CmdGoToPrevPage},

    {FVIRTKEY, VK_HOME, CmdGoToFirstPage},
    {FCONTROL | FVIRTKEY, VK_HOME, CmdGoToFirstPage},
    {FVIRTKEY, VK_END, CmdGoToLastPage},
    {FCONTROL | FVIRTKEY, VK_END, CmdGoToLastPage},

    {FVIRTKEY, VK_BACK, CmdNavigateBack},
    {FALT | FVIRTKEY, VK_LEFT, CmdNavigateBack},
    {FSHIFT | FVIRTKEY, VK_BACK, CmdNavigateForward},
    {FALT | FVIRTKEY, VK_RIGHT, CmdNavigateForward},

    {FCONTROL | FVIRTKEY, 'O', CmdOpenFile},
    {FSHIFT | FCONTROL | FVIRTKEY, VK_RIGHT, CmdOpenNextFileInFolder},
    {FSHIFT | FCONTROL | FVIRTKEY, VK_LEFT, CmdOpenPrevFileInFolder},
    {FVIRTKEY, VK_F2, CmdRenameFile},
    {FCONTROL | FVIRTKEY, 'W', CmdClose},
    {FCONTROL | FVIRTKEY, 'N', CmdNewWindow},
    {FSHIFT | FCONTROL | FVIRTKEY, 'N', CmdDuplicateInNewWindow},
    {FCONTROL | FVIRTKEY, 'S', CmdSaveAs},
    //{FSHIFT | FCONTROL | FVIRTKEY, 'S', CmdCreateShortcutToFile},

    {FCONTROL | FVIRTKEY, 'A', CmdSelectAll},
    {FCONTROL | FVIRTKEY, 'B', CmdFavoriteAdd},
    {FCONTROL | FVIRTKEY, 'C', CmdCopySelection},
    {FCONTROL | FVIRTKEY, VK_INSERT, CmdCopySelection},
    {FCONTROL | FVIRTKEY, 'V', CmdPasteClipboardImage},
    {FCONTROL | FVIRTKEY, 'D', CmdProperties},
    {FCONTROL | FVIRTKEY, 'F', CmdFindFirst},
    {FCONTROL | FVIRTKEY, 'G', CmdGoToPage},
    {FVIRTKEY, 'G', CmdGoToPage},
    {FCONTROL | FVIRTKEY, 'K', CmdCommandPalette},
    //{FALT | FVIRTKEY, 'K', CmdCommandPaletteOnlyTabs}, // removed in 3.6
    {FSHIFT | FCONTROL | FVIRTKEY, 'S', CmdSaveAnnotations},
    {FCONTROL | FVIRTKEY, 'P', CmdPrint},
    {FCONTROL | FVIRTKEY, 'Q', CmdExit},
    {FCONTROL | FVIRTKEY, 'Y', CmdZoomCustom},
    {FCONTROL | FVIRTKEY, '0', CmdZoomFitPage},
    {FCONTROL | FVIRTKEY, VK_NUMPAD0, CmdZoomFitPage},
    {FCONTROL | FVIRTKEY, '1', CmdZoomActualSize},
    {FCONTROL | FVIRTKEY, VK_NUMPAD1, CmdZoomActualSize},
    {FCONTROL | FVIRTKEY, '2', CmdZoomFitWidth},
    {FCONTROL | FVIRTKEY, VK_NUMPAD2, CmdZoomFitWidth},
    {FCONTROL | FVIRTKEY, '3', CmdZoomFitContent},
    {FCONTROL | FVIRTKEY, VK_NUMPAD3, CmdZoomFitContent},
    {FCONTROL | FVIRTKEY, VK_ADD, CmdZoomIn},
    {FCONTROL | FVIRTKEY, VK_SUBTRACT, CmdZoomOut},
    {FCONTROL | FVIRTKEY, VK_OEM_MINUS, CmdZoomOut},
    {FCONTROL | FVIRTKEY, '6', CmdSinglePageView},
    {FCONTROL | FVIRTKEY, VK_NUMPAD6, CmdSinglePageView},
    {FCONTROL | FVIRTKEY, '7', CmdFacingView},
    {FCONTROL | FVIRTKEY, VK_NUMPAD7, CmdFacingView},
    {FCONTROL | FVIRTKEY, '8', CmdBookView},
    {FCONTROL | FVIRTKEY, VK_NUMPAD8, CmdBookView},
    {FSHIFT | FCONTROL | FVIRTKEY, VK_ADD, CmdRotateRight},
    {FCONTROL | FVIRTKEY, VK_OEM_PLUS, CmdZoomIn},
    {FSHIFT | FCONTROL | FVIRTKEY, VK_OEM_PLUS, CmdRotateRight},
    {FVIRTKEY, VK_F3, CmdFindNext},
    {FSHIFT | FVIRTKEY, VK_F3, CmdFindPrev},
    {FCONTROL | FVIRTKEY, VK_F3, CmdFindNextSel},
    {FSHIFT | FCONTROL | FVIRTKEY, VK_F3, CmdFindPrevSel},
    {FCONTROL | FVIRTKEY, VK_F4, CmdClose},
    {FVIRTKEY, VK_F6, CmdMoveFrameFocus},
    {FVIRTKEY, VK_F8, CmdToggleToolbar},
    {FVIRTKEY, VK_F9, CmdToggleMenuBar},
    {FCONTROL | FVIRTKEY, 'L', CmdTogglePresentationMode},
    {FVIRTKEY, VK_F5, CmdTogglePresentationMode},
    {FSHIFT | FVIRTKEY, VK_F11, CmdTogglePresentationMode},
    {FSHIFT | FCONTROL | FVIRTKEY, 'L', CmdToggleFullscreen},
    {FVIRTKEY, VK_F11, CmdToggleFullscreen},
    {FVIRTKEY, VK_F12, CmdToggleBookmarks},
    {FSHIFT | FVIRTKEY, VK_F12, CmdCommandPaletteTOC},
    {FSHIFT | FCONTROL | FVIRTKEY, VK_SUBTRACT, CmdRotateLeft},
    {FSHIFT | FCONTROL | FVIRTKEY, VK_OEM_MINUS, CmdRotateLeft},
    {FSHIFT | FCONTROL | FVIRTKEY, 'T', CmdReopenLastClosedFile},
    {FCONTROL | FVIRTKEY, VK_NEXT, CmdNextTab},
    {FCONTROL | FVIRTKEY, VK_PRIOR, CmdPrevTab},
    {FCONTROL | FSHIFT | FVIRTKEY, VK_NEXT, CmdMoveTabRight},
    {FCONTROL | FSHIFT | FVIRTKEY, VK_PRIOR, CmdMoveTabLeft},
    {FCONTROL | FVIRTKEY, VK_TAB, CmdNextTabSmart},
    {FCONTROL | FSHIFT | FVIRTKEY, VK_TAB, CmdPrevTabSmart},
    {FVIRTKEY, VK_F1, CmdHelpOpenManual},

    // need 2 entries for 'a' and 'Shift + a'
    // TODO: maybe add CmdCreateAnnotHighlightAndOpenWindow (kind of clumsy)
    {FVIRTKEY, 'A', CmdCreateAnnotHighlight},
    {FVIRTKEY | FSHIFT, 'A', CmdCreateAnnotHighlight},

    {FVIRTKEY, 'U', CmdCreateAnnotUnderline},
    {FVIRTKEY | FSHIFT, 'U', CmdCreateAnnotUnderline},

    {FVIRTKEY | FSHIFT, 'I', CmdInvertColors},
    {FVIRTKEY, 'I', CmdTogglePageInfo},

    {FCONTROL | FVIRTKEY, VK_DELETE, CmdDeleteAnnotation},

    {FVIRTKEY, 'Q', CmdCloseCurrentDocument},
    {FVIRTKEY, 'R', CmdReloadDocument},
    {FVIRTKEY, 'Z', CmdToggleZoom},
    {FVIRTKEY, 'F', CmdToggleFullscreen},
    // '['
    {FVIRTKEY, VK_OEM_4, CmdRotateLeft},
    // ']'
    {FVIRTKEY, VK_OEM_6, CmdRotateRight},
    {FVIRTKEY, 'M', CmdToggleCursorPosition},
    {FVIRTKEY, 'W', CmdPresentationWhiteBackground},
    // for Logitech's wireless presenters which target PowerPoint's shortcuts
    // TODO: don't know what VK_ is this
    {0, '.', CmdPresentationBlackBackground},
    {FVIRTKEY, 'C', CmdToggleContinuousView},
};

ACCEL* gAccels = nullptr;
int gAccelsCount = 0;

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
static bool parseShortcut(Str shortcut, ACCEL& accel) {
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
            accel.key = buf[0];
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
        } else {
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
        }
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
    accel.key = c;
    return true;
}

bool IsValidShortcutString(Str shortcut) {
    ACCEL accel = {};
    accel.cmd = (WORD)-1; // for debugging
    return parseShortcut(shortcut, accel);
}

bool ParseShortcutString(Str shortcut, ACCEL& accel) {
    return parseShortcut(shortcut, accel);
}

static TempStr appendAccelKeyToMenuStringTemp(TempStr menuStr, const ACCEL& a) {
    auto lang = trans::GetCurrentLangCode();
    bool isEng = len(lang) == 0 || str::Eq(lang, "en");
    bool isGerman = str::Eq(lang, "de");
    bool isAscii = false;

    str::Builder str;
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

TempStr AppendAccelKeyToMenuStringTemp(TempStr menuStr, int cmdId) {
    ACCEL a;
    for (int i = 0; i < gAccelsCount; i++) {
        a = gAccels[i];
        if (a.cmd == cmdId) {
            TempStr res = appendAccelKeyToMenuStringTemp(menuStr, a);
            return res;
        }
    }
    return menuStr;
}

static bool sameAccelKey(const ACCEL& a1, const ACCEL& a2) {
    if (a1.fVirt != a2.fVirt) {
        return false;
    }
    if (a1.key != a2.key) {
        return false;
    }
    return true;
}

// clang-format off
static WORD gNotSafeKeys[] = {
    VK_LEFT,
    VK_RIGHT,
    VK_UP,
    VK_DOWN,
    VK_SPACE,
    VK_RETURN,
    VK_INSERT,
    VK_DELETE,
    VK_BACK,
    VK_HOME,
    VK_END,
    VK_OEM_4,
    VK_OEM_6
};
// clang-format on

// a hackish way to determine if we should allow processing a given
// accelerator in custom controls. This is to disable accelerators
// like 'n' or 'left arrow' in e.g. edit control so that they don't
// block regular processing of key events and mess up edit control
// at the same time, we do want most accelerators to be enabed even
// if edit or tree view control has focus
static bool isSafeAccel(const ACCEL& a) {
    WORD k = a.key;
    if (a.fVirt == 0) {
        // regular keys like 'n', without any shift / alt modifier
        return false;
    }

    // regular keys are also coded as FVIRTKEY or FVIRTKEY | FSHIFT
    // so that they work based on virtual keyboard code to support
    // non-english keyboards
    if (k >= 'A' && k <= 'Z') {
        if (a.fVirt == FVIRTKEY) {
            return false;
        }
        if (a.fVirt == (FVIRTKEY | FSHIFT)) {
            return false;
        }
    }

    // whitelist Alt + Left, Alt + Right to enable document
    // navigation when focus is in edit or tree control
    // https://github.com/sumatrapdfreader/sumatrapdf/issues/3688#issuecomment-1728271753
    if (a.fVirt == (FVIRTKEY | FALT)) {
        if ((k == VK_LEFT) || (k == VK_RIGHT)) {
            return true;
        }
    }

    if ((a.fVirt == (FCONTROL | FVIRTKEY)) && (k == 'V')) {
        // Ctrl+V should work normally in edit controls (paste text)
        return false;
    }

    for (WORD notSafe : gNotSafeKeys) {
        if (notSafe == k) {
            return false;
        }
    }
    return true;
}

// Command bound to vk + modifiers among the "safe" accelerators (those allowed
// while a custom control has focus). 0 if none. Lets custom controls (e.g. the
// WebView2-hosted CHM) forward app shortcuts they'd otherwise swallow.
int SafeAcceleratorCmd(u16 vk, bool ctrl, bool shift, bool alt) {
    BYTE fVirt = FVIRTKEY;
    if (ctrl) {
        fVirt |= FCONTROL;
    }
    if (shift) {
        fVirt |= FSHIFT;
    }
    if (alt) {
        fVirt |= FALT;
    }
    for (int i = 0; i < gAccelsCount; i++) {
        const ACCEL& a = gAccels[i];
        if (a.key == vk && a.fVirt == fVirt && isSafeAccel(a)) {
            return a.cmd;
        }
    }
    return 0;
}

static HACCEL gAccelTables[3] = {
    nullptr, // for all but edit and tree view
    nullptr, // for edit
    nullptr, // for tree view
};

/* returns a pointer to HACCEL so that we can update it and message loop will use
the latest version */
void CreateSumatraAcceleratorTable() {
    ReportIf(gAccelTables[0] || gAccelTables[1] || gAccelTables[2]);

    int nMax = (int)dimof(gBuiltInAccelerators);
    auto curr = gFirstCustomCommand;
    while (curr) {
        if ((curr->id > 0) && !str::IsEmptyOrWhiteSpace(curr->key)) {
            nMax++;
        }
        curr = curr->next;
    }

    // https://github.com/sumatrapdfreader/sumatrapdf/issues/2981
    // sizeof(ACCEL) is 6 so odd number will cause treeViewAccels to
    // be mis-aligined. Rounding to 2 should be enoug, do 4 for extra safety
    nMax = RoundUp(nMax, 4);
    ACCEL* accels = AllocArray<ACCEL>(nMax);
    // perf: only 1 allocation for 2 arrays
    ACCEL* editAccels = AllocArray<ACCEL>(nMax * 2);
    ACCEL* treeViewAccels = editAccels + nMax;

    int nAccels = 0;
    int nEditAccels = 0;
    int nTreeViewAccels = 0;

    auto addShortcutIfNotExists = [&](ACCEL accel) -> void {
        bool shortcutExists = false;
        for (int i = 0; !shortcutExists && i < nAccels; i++) {
            shortcutExists = sameAccelKey(accels[i], accel);
        }
        if (shortcutExists) {
            return;
        }
        accels[nAccels++] = accel;
        if (isSafeAccel(accel)) {
            editAccels[nEditAccels++] = accel;
            treeViewAccels[nTreeViewAccels++] = accel;
        }
        if (((int)accel.cmd == (int)CmdToggleBookmarks) && !isSafeAccel(accel)) {
            // https://github.com/sumatrapdfreader/sumatrapdf/issues/2832
            treeViewAccels[nTreeViewAccels++] = accel;
        }
    };

    curr = gFirstCustomCommand;
    while (curr) {
        if ((curr->id > 0) && !str::IsEmptyOrWhiteSpace(curr->key)) {
            // CmdScreenshot shortcuts are registered as global hotkeys, not accelerators
            if (curr->origId == CmdScreenshot) {
                curr = curr->next;
                continue;
            }
            ACCEL accel{};
            accel.cmd = (WORD)curr->id;
            if (parseShortcut(curr->key, accel)) {
                addShortcutIfNotExists(accel);
            }
        }
        curr = curr->next;
    }

    // add built-in but only if the shortcut doesn't conflict with custom shortcut
    for (ACCEL accel : gBuiltInAccelerators) {
        addShortcutIfNotExists(accel);
    }

    gAccels = accels;
    gAccelsCount = nAccels;

    gAccelTables[0] = CreateAcceleratorTableW(gAccels, gAccelsCount);
    ReportIf(gAccelTables[0] == nullptr);
    gAccelTables[1] = CreateAcceleratorTableW(editAccels, nEditAccels);
    ReportIf(gAccelTables[1] == nullptr);
    gAccelTables[2] = CreateAcceleratorTableW(treeViewAccels, nTreeViewAccels);
    ReportIf(gAccelTables[2] == nullptr);

    free(editAccels);
}

void FreeAcceleratorTables() {
    DestroyAcceleratorTable(gAccelTables[0]);
    DestroyAcceleratorTable(gAccelTables[1]);
    DestroyAcceleratorTable(gAccelTables[2]);
    gAccelTables[0] = nullptr;
    gAccelTables[1] = nullptr;
    gAccelTables[2] = nullptr;
    free(gAccels);
    gAccels = nullptr;
}

HACCEL* GetAcceleratorTables() {
    if (gAccelTables[0] == nullptr) {
        CreateSumatraAcceleratorTable();
    }
    return gAccelTables;
}
