/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "VirtKeys.h"
#include "ShortcutParse.h"
#include "Settings.h"
#include "Commands.h"
#include "Accelerators.h"
#include "SumatraLog.h"

// must be last due to assert() over-write
#include "base/tests/UtAssert.h"

constexpr u8 kVirt = KeyShortcut::kVirtKey;
constexpr u8 kShift = KeyShortcut::kShiftKey;
constexpr u8 kCtrl = KeyShortcut::kCtrlKey;

static bool AccelShowsAs(u8 mods, u16 key, Str expected) {
    KeyShortcut a(mods, key);
    TempStr s = AppendAccelKeyToMenuStringTemp(StrL(""), a);
    if (len(s) < 1 || s.s[0] != '\t') {
        return false;
    }
    return str::Eq(Str(s.s + 1, len(s) - 1), expected);
}

bool ShortcutParse_UnitTestShiftedPunct() {
    auto prevLang = gShortcutLangCode;
    gShortcutLangCode = nullptr;

    utassert(AccelShowsAs(kShift | kVirt, VK_OEM_2, StrL("?")));
    utassert(AccelShowsAs(kVirt, VK_OEM_2, StrL("/")));
    utassert(AccelShowsAs(kCtrl | kShift | kVirt, VK_OEM_2, StrL("Ctrl + ?")));
    utassert(AccelShowsAs(kShift | kVirt, VK_OEM_COMMA, StrL("<")));
    utassert(AccelShowsAs(kShift | kVirt, VK_OEM_PERIOD, StrL(">")));
    utassert(AccelShowsAs(kShift | kVirt, VK_OEM_4, StrL("{")));
    utassert(AccelShowsAs(kShift | kVirt, VK_OEM_6, StrL("}")));
    utassert(AccelShowsAs(kShift | kVirt, VK_OEM_5, StrL("|")));
    utassert(AccelShowsAs(kShift | kVirt, VK_OEM_1, StrL(":")));
    utassert(AccelShowsAs(kShift | kVirt, VK_OEM_7, StrL("\"")));
    utassert(AccelShowsAs(kShift | kVirt, VK_OEM_3, StrL("~")));
    utassert(AccelShowsAs(kShift | kVirt, 'A', StrL("Shift + A")));
    // VK codes that equal a punctuation char ('\'' is VK_RIGHT) aren't punctuation
    utassert(AccelShowsAs(kCtrl | kShift | kVirt, VK_RIGHT, StrL("Ctrl + Shift + Right")));
    utassert(AccelShowsAs(kShift | kVirt, VK_DELETE, StrL("Shift + Del")));
    utassert(AccelShowsAs(kShift | kVirt, VK_SNAPSHOT, StrL("Shift + PrtSc")));

    KeyShortcut a{};
    utassert(ParseShortcutString(StrL("<"), a));
    utassert(a.vk == VK_OEM_COMMA && a.Mods() == (kShift | kVirt));
    a = {};
    utassert(ParseShortcutString(StrL("Ctrl + \""), a));
    utassert(a.vk == VK_OEM_7 && a.Mods() == (kCtrl | kShift | kVirt));
    a = {};
    utassert(ParseShortcutString(StrL("Ctrl + `"), a));
    utassert(a.vk == VK_OEM_3 && a.Mods() == (kCtrl | kVirt));
    a = {};
    utassert(ParseShortcutString(StrL("Ctrl + Shift + `"), a));
    utassert(a.vk == VK_OEM_3 && a.Mods() == (kCtrl | kShift | kVirt));
    a = {};
    utassert(ParseShortcutString(StrL("~"), a));
    utassert(a.vk == VK_OEM_3 && a.Mods() == (kShift | kVirt));

    // unshifted punctuation, whose ASCII codes are other VKs ('\'' is VK_RIGHT)
    static const struct {
        Str s;
        u16 vk;
    } unshifted[] = {
        {StrL("'"), VK_OEM_7},    {StrL(","), VK_OEM_COMMA}, {StrL("."), VK_OEM_PERIOD}, {StrL("\\"), VK_OEM_5},
        {StrL("="), VK_OEM_PLUS}, {StrL(";"), VK_OEM_1},     {StrL("["), VK_OEM_4},      {StrL("]"), VK_OEM_6},
    };
    for (auto& u : unshifted) {
        a = {};
        utassert(ParseShortcutString(u.s, a));
        utassert(a.vk == u.vk && a.Mods() == kVirt);
        if (u.vk != VK_OEM_PLUS) { // shown as "+"
            utassert(AccelShowsAs(kVirt, u.vk, u.s));
        }
    }
    a = {};
    utassert(ParseShortcutString(StrL("Ctrl + '"), a));
    utassert(a.vk == VK_OEM_7 && a.Mods() == (kCtrl | kVirt));

    gShortcutLangCode = prevLang;
    return true;
}

// ng: the shortcut string -> gpui KeyBinding stroke conversion, which replaces
// the win32 accelerator table (gpui::KeyChordParse parses these back)
static bool StrokeIs(Str shortcut, Str expected) {
    KeyShortcut sc{};
    if (!ParseShortcutString(shortcut, sc)) {
        return false;
    }
    return str::Eq(ShortcutToGpuiStroke(sc), expected);
}

bool ShortcutParse_UnitTestGpuiStroke() {
    utassert(StrokeIs(StrL("Ctrl + Shift + F5"), StrL("ctrl-shift-f5")));
    utassert(StrokeIs(StrL("Ctrl + O"), StrL("ctrl-o")));
    utassert(StrokeIs(StrL("Alt + Left"), StrL("alt-left")));
    utassert(StrokeIs(StrL("PageUp"), StrL("pageup")));
    utassert(StrokeIs(StrL("PgDown"), StrL("pagedown")));
    utassert(StrokeIs(StrL("Esc"), StrL("escape")));
    utassert(StrokeIs(StrL("Space"), StrL("space")));
    utassert(StrokeIs(StrL("Backspace"), StrL("backspace")));
    utassert(StrokeIs(StrL("Return"), StrL("enter")));
    utassert(StrokeIs(StrL("Tab"), StrL("tab")));
    utassert(StrokeIs(StrL("Del"), StrL("delete")));
    utassert(StrokeIs(StrL("F12"), StrL("f12")));
    utassert(StrokeIs(StrL("Ctrl + Add"), StrL("ctrl-add")));
    utassert(StrokeIs(StrL("Ctrl + Numpad0"), StrL("ctrl-numpad0")));
    utassert(StrokeIs(StrL("Numpad9"), StrL("numpad9")));
    utassert(StrokeIs(StrL("Ctrl + ["), StrL("ctrl-[")));
    // the keyboard-help accelerator: Shift + '/' on a US layout
    utassert(str::Eq(ShortcutToGpuiStroke(KeyShortcut(kShift | kVirt, VK_OEM_2)), StrL("shift-/")));
    // a lowercase letter parses as an unshifted virtual key
    utassert(StrokeIs(StrL("k"), StrL("k")));
    utassert(StrokeIs(StrL("K"), StrL("shift-k")));

    // every default shortcut converts: nothing in the built-in table is a key
    // gpui has no name for
    int n = 0;
    const Accel* accels = GetAcceleratorTable(n);
    utassert(n > 0);
    for (int i = 0; i < n; i++) {
        TempStr stroke = ShortcutToGpuiStroke(accels[i].sc);
        utassert(len(stroke) > 0);
    }
    // and the stroke table keeps all of them (no duplicate collapsed a command
    // away): every command in the accelerator table has at least one stroke
    int nStrokes = 0;
    const AccelStroke* strokes = GetAcceleratorStrokes(nStrokes);
    utassert(nStrokes > 0 && strokes != nullptr);
    for (int i = 0; i < n; i++) {
        bool found = false;
        for (int j = 0; j < nStrokes; j++) {
            if (strokes[j].cmd == accels[i].cmd) {
                found = true;
                break;
            }
        }
        utassert(found);
    }
    return true;
}

// The default shortcuts orig documents in docs/md/Keyboard-shortcuts.md. Every
// one of them must be in the accelerator table, bound to the command the doc
// says. A shortcut the doc gives without a command name (the Ctrl + wheel and
// mouse gestures, the F7 sub-mode keys) is not an accelerator and is left out.
// clang-format off
static const struct {
    Str shortcut;
    int cmd;
} gDocumentedShortcuts[] = {
    {StrL("Ctrl + N"), CmdNewWindow},
    {StrL("Ctrl + O"), CmdOpenFile},
    {StrL("Ctrl + W"), CmdClose},
    {StrL("Ctrl + F4"), CmdClose},
    {StrL("Ctrl + S"), CmdSaveAs},
    {StrL("F2"), CmdRenameFile},
    {StrL("Ctrl + P"), CmdPrint},
    {StrL("Ctrl + D"), CmdProperties},
    {StrL("Ctrl + Q"), CmdExit},
    {StrL("Home"), CmdGoToFirstPage},
    {StrL("End"), CmdGoToLastPage},
    {StrL("Ctrl + G"), CmdGoToPage},
    {StrL("g"), CmdGoToPage},
    {StrL("Alt + Left"), CmdNavigateBack},
    {StrL("Alt + Right"), CmdNavigateForward},
    {StrL("Ctrl + F"), CmdFindFirst},
    {StrL("F3"), CmdFindNext},
    {StrL("Shift + F3"), CmdFindPrev},
    {StrL("Ctrl + B"), CmdFavoriteAdd},
    {StrL("Ctrl + 6"), CmdSinglePageView},
    {StrL("Ctrl + 7"), CmdFacingView},
    {StrL("Ctrl + 8"), CmdBookView},
    // the parser eats a "+" or "-" right after a modifier (orig does too), so
    // the two rotate strokes are spelled with their shifted glyphs here
    {StrL("Ctrl + Shift + _"), CmdRotateLeft},
    {StrL("["), CmdRotateLeft},
    {StrL("Ctrl + Shift + ="), CmdRotateRight},
    {StrL("]"), CmdRotateRight},
    {StrL("F5"), CmdTogglePresentationMode},
    {StrL("Ctrl + L"), CmdTogglePresentationMode},
    {StrL("Shift + F11"), CmdTogglePresentationMode},
    {StrL("F11"), CmdToggleFullscreen},
    {StrL("Ctrl + Shift + L"), CmdToggleFullscreen},
    {StrL("f"), CmdToggleFullscreen},
    {StrL("F12"), CmdToggleBookmarks},
    {StrL("F8"), CmdToggleToolbar},
    {StrL("F9"), CmdToggleMenuBar},
    {StrL("F6"), CmdMoveFrameFocus},
    {StrL("Ctrl + A"), CmdSelectAll},
    {StrL("Ctrl + C"), CmdCopySelection},
    {StrL("F7"), CmdSelectTextViaKeyboard},
    {StrL("Ctrl + 0"), CmdZoomFitPage},
    {StrL("Ctrl + 1"), CmdZoomActualSize},
    {StrL("Ctrl + 2"), CmdZoomFitWidth},
    {StrL("Ctrl + 3"), CmdZoomFitContent},
    {StrL("Ctrl + Y"), CmdZoomCustom},
    {StrL("j"), CmdScrollDown},
    {StrL("k"), CmdScrollUp},
    {StrL("h"), CmdScrollLeft},
    {StrL("l"), CmdScrollRight},
    {StrL("Up"), CmdScrollUp},
    {StrL("Down"), CmdScrollDown},
    {StrL("Shift + Left"), CmdScrollLeftPage},
    {StrL("Shift + Right"), CmdScrollRightPage},
    {StrL("Space"), CmdScrollDownPage},
    {StrL("Shift + Space"), CmdScrollUpPage},
    {StrL("n"), CmdGoToNextPage},
    {StrL("p"), CmdGoToPrevPage},
    {StrL("PageDown"), CmdScrollDownPage},
    {StrL("PageUp"), CmdScrollUpPage},
    {StrL("Ctrl + Down"), CmdScrollDownPage},
    {StrL("Ctrl + Up"), CmdScrollUpPage},
    {StrL("Ctrl + Shift + Right"), CmdOpenNextFileInFolder},
    {StrL("Ctrl + Shift + Left"), CmdOpenPrevFileInFolder},
    {StrL("z"), CmdToggleZoom},
    {StrL("c"), CmdToggleContinuousView},
    {StrL("i"), CmdTogglePageInfo},
    {StrL("Shift + i"), CmdInvertColors},
    {StrL("m"), CmdToggleCursorPosition},
    {StrL("w"), CmdPresentationWhiteBackground},
    {StrL("Ctrl + K"), CmdCommandPalette},
    {StrL("r"), CmdReloadDocument},
    {StrL("q"), CmdCloseCurrentDocument},
    {StrL("F1"), CmdHelpOpenManual},
    {StrL("Ctrl + Shift + N"), CmdDuplicateInNewWindow},
    {StrL("Ctrl + PageDown"), CmdNextTab},
    {StrL("Ctrl + PageUp"), CmdPrevTab},
    {StrL("Ctrl + Tab"), CmdNextTabSmart},
    {StrL("Ctrl + Shift + Tab"), CmdPrevTabSmart},
    {StrL("a"), CmdCreateAnnotHighlight},
    {StrL("A"), CmdCreateAnnotHighlight},
    {StrL("u"), CmdCreateAnnotUnderline},
    {StrL("U"), CmdCreateAnnotUnderline},
    {StrL("Ctrl + X"), CmdCutAnnotation},
    {StrL("Ctrl + V"), CmdPasteClipboardImage},
    {StrL("Ctrl + Z"), CmdUndo},
    {StrL("Ctrl + Shift + Z"), CmdRedo},
    {StrL("Ctrl + Shift + S"), CmdSaveAnnotations},
};
// clang-format on

static bool SameShortcut(const KeyShortcut& a, const KeyShortcut& b) {
    return a.vk == b.vk && a.Mods() == b.Mods();
}

// ng: orig has no such test. The port builds one table where orig builds three
// win32 HACCELs, so a stroke bound twice, or a documented default that fell out
// while porting, would go unnoticed.
bool ShortcutParse_UnitTestAccelTable() {
    auto prevLang = gShortcutLangCode;
    gShortcutLangCode = nullptr;
    // what is wrong with the table matters more than that something is
    bool prevLogToConsole = gLogToConsole;
    gLogToConsole = true;

    int n = 0;
    const Accel* accels = GetAcceleratorTable(n);
    utassert(n > 0 && accels != nullptr);

    // no stroke is bound to two different commands, and every entry names a
    // command that exists
    for (int i = 0; i < n; i++) {
        CustomCommand* custom = FindCustomCommand(accels[i].cmd);
        utassert(len(GetCommandName(custom ? custom->origId : accels[i].cmd)) > 0);
        for (int j = i + 1; j < n; j++) {
            if (!SameShortcut(accels[i].sc, accels[j].sc)) {
                continue;
            }
            if (accels[i].cmd != accels[j].cmd) {
                logf("stroke shared by %s and %s\n", GetCommandName(accels[i].cmd), GetCommandName(accels[j].cmd));
            }
            utassert(accels[i].cmd == accels[j].cmd);
        }
    }

    // every default shortcut orig documents is bound to the documented command
    for (auto& d : gDocumentedShortcuts) {
        KeyShortcut sc{};
        if (!ParseShortcutString(d.shortcut, sc)) {
            logf("can't parse '%s'\n", d.shortcut);
        }
        utassert(ParseShortcutString(d.shortcut, sc));
        bool found = false;
        for (int i = 0; i < n; i++) {
            if (!SameShortcut(accels[i].sc, sc)) {
                continue;
            }
            // Shift + a / Shift + u are clones that add "openedit"
            CustomCommand* custom = FindCustomCommand(accels[i].cmd);
            int cmdId = custom ? custom->origId : accels[i].cmd;
            if (cmdId == d.cmd) {
                found = true;
                break;
            }
        }
        if (!found) {
            logf("'%s' is not bound to %s\n", d.shortcut, GetCommandName(d.cmd));
        }
        utassert(found);
    }

    gLogToConsole = prevLogToConsole;
    gShortcutLangCode = prevLang;
    return true;
}
