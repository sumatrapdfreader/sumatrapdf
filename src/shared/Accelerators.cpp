/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#if !OS_WIN
#include "VirtKeys.h"
#endif
#include "Settings.h"
#include "Commands.h"
#include "ShortcutParse.h"
#include "Translations.h"
#include "Accelerators.h"

constexpr u8 kVirt = KeyShortcut::kVirtKey;
constexpr u8 kShift = KeyShortcut::kShiftKey;
constexpr u8 kCtrl = KeyShortcut::kCtrlKey;
constexpr u8 kAlt = KeyShortcut::kAltKey;

// note: even letter shortcuts like 'k' are marked as FVIRTKEY so that they
// work even on non-english keyboards (cyrillic, hebrew)
// VK_A is 'A' etc. which corresponds to 'a' key.
// To get 'A' need explicitly use FSHIFT.
// https://learn.microsoft.com/en-us/windows/win32/menurc/using-keyboard-accelerators?referrer=grok.com
// https://grok.com/share/bGVnYWN5_d83c2956-4ce2-4c74-ba4d-9794d1760ccb?rid=746312cc-7d0f-4479-abec-25c394652cac
// NOLINTBEGIN(modernize-use-designated-initializers)
static Accel gBuiltInAccelerators[] = {
    {{kVirt, 'K'}, CmdScrollUp},
    {{kVirt, 'J'}, CmdScrollDown},
    {{kVirt, 'H'}, CmdScrollLeft},
    {{kVirt, 'L'}, CmdScrollRight},
    {{kVirt, VK_UP}, CmdScrollUp},
    {{kVirt, VK_DOWN}, CmdScrollDown},
    {{kVirt, VK_LEFT}, CmdScrollLeft},
    {{kVirt, VK_RIGHT}, CmdScrollRight},

    {{kShift | kVirt, VK_UP}, CmdScrollUpHalfPage},
    {{kShift | kVirt, VK_DOWN}, CmdScrollDownHalfPage},

    {{kShift | kVirt, VK_LEFT}, CmdScrollLeftPage},
    {{kShift | kVirt, VK_RIGHT}, CmdScrollRightPage},

    {{kVirt, VK_NEXT}, CmdScrollDownPage},
    {{kVirt, VK_PRIOR}, CmdScrollUpPage},

    {{kVirt, VK_SPACE}, CmdScrollDownPage},
    {{kVirt, VK_RETURN}, CmdScrollDownPage},
    {{kCtrl | kVirt, VK_DOWN}, CmdScrollDownPage},

    {{kShift | kVirt, VK_SPACE}, CmdScrollUpPage},
    {{kShift | kVirt, VK_RETURN}, CmdScrollUpPage},
    {{kCtrl | kVirt, VK_UP}, CmdScrollUpPage},

    {{kVirt, 'N'}, CmdGoToNextPage},
    //{{kCtrl | kVirt, VK_NEXT}, CmdGoToNextPage},

    {{kVirt, 'P'}, CmdGoToPrevPage},
    //{{kCtrl | kVirt, VK_PRIOR}, CmdGoToPrevPage},

    {{kVirt, VK_HOME}, CmdGoToFirstPage},
    {{kCtrl | kVirt, VK_HOME}, CmdGoToFirstPage},
    {{kVirt, VK_END}, CmdGoToLastPage},
    {{kCtrl | kVirt, VK_END}, CmdGoToLastPage},

    {{kVirt, VK_BACK}, CmdNavigateBack},
    {{kAlt | kVirt, VK_LEFT}, CmdNavigateBack},
    {{kShift | kVirt, VK_BACK}, CmdNavigateForward},
    {{kAlt | kVirt, VK_RIGHT}, CmdNavigateForward},

    {{kCtrl | kVirt, 'O'}, CmdOpenFile},
    {{kShift | kCtrl | kVirt, VK_RIGHT}, CmdOpenNextFileInFolder},
    {{kShift | kCtrl | kVirt, VK_LEFT}, CmdOpenPrevFileInFolder},
    {{kShift | kCtrl | kVirt, VK_UP}, CmdNavigateFilesInFolder},
    {{kVirt, VK_F2}, CmdRenameFile},
    {{kCtrl | kVirt, 'W'}, CmdClose},
    {{kCtrl | kVirt, 'N'}, CmdNewWindow},
    {{kShift | kCtrl | kVirt, 'N'}, CmdDuplicateInNewWindow},
    {{kCtrl | kVirt, 'S'}, CmdSaveAs},
    //{{kShift | kCtrl | kVirt, 'S'}, CmdCreateShortcutToFile},

    {{kCtrl | kVirt, 'A'}, CmdSelectAll},
    {{kCtrl | kVirt, 'B'}, CmdFavoriteAdd},
    {{kCtrl | kVirt, 'C'}, CmdCopySelection},
    {{kCtrl | kVirt, VK_INSERT}, CmdCopySelection},
    {{kCtrl | kVirt, 'V'}, CmdPasteClipboardImage},
    {{kCtrl | kVirt, 'X'}, CmdCutAnnotation},
    {{kCtrl | kVirt, 'Z'}, CmdUndo},
    {{kShift | kCtrl | kVirt, 'Z'}, CmdRedo},
    {{kCtrl | kVirt, 'D'}, CmdProperties},
    {{kCtrl | kVirt, 'F'}, CmdFindFirst},
    {{kCtrl | kVirt, 'G'}, CmdGoToPage},
    {{kVirt, 'G'}, CmdGoToPage},
    {{kCtrl | kVirt, 'K'}, CmdCommandPalette},
    //{{kAlt | kVirt, 'K'}, CmdCommandPaletteOnlyTabs}, // removed in 3.6
    {{kShift | kCtrl | kVirt, 'S'}, CmdSaveAnnotations},
    {{kCtrl | kVirt, 'P'}, CmdPrint},
    {{kCtrl | kVirt, 'Q'}, CmdExit},
    {{kCtrl | kVirt, 'Y'}, CmdZoomCustom},
    {{kCtrl | kVirt, '0'}, CmdZoomFitPage},
    {{kCtrl | kVirt, VK_NUMPAD0}, CmdZoomFitPage},
    {{kCtrl | kVirt, '1'}, CmdZoomActualSize},
    {{kCtrl | kVirt, VK_NUMPAD1}, CmdZoomActualSize},
    {{kCtrl | kVirt, '2'}, CmdZoomFitWidth},
    {{kCtrl | kVirt, VK_NUMPAD2}, CmdZoomFitWidth},
    {{kCtrl | kVirt, '3'}, CmdZoomFitContent},
    {{kCtrl | kVirt, VK_NUMPAD3}, CmdZoomFitContent},
    {{kCtrl | kVirt, '4'}, CmdZoomToSelection},
    {{kCtrl | kVirt, VK_NUMPAD4}, CmdZoomToSelection},
    {{kCtrl | kVirt, VK_ADD}, CmdZoomIn},
    {{kCtrl | kVirt, VK_SUBTRACT}, CmdZoomOut},
    {{kCtrl | kVirt, VK_OEM_MINUS}, CmdZoomOut},
    {{kCtrl | kVirt, '6'}, CmdSinglePageView},
    {{kCtrl | kVirt, VK_NUMPAD6}, CmdSinglePageView},
    {{kCtrl | kVirt, '7'}, CmdFacingView},
    {{kCtrl | kVirt, VK_NUMPAD7}, CmdFacingView},
    {{kCtrl | kVirt, '8'}, CmdBookView},
    {{kCtrl | kVirt, VK_NUMPAD8}, CmdBookView},
    {{kShift | kCtrl | kVirt, VK_ADD}, CmdRotateRight},
    {{kCtrl | kVirt, VK_OEM_PLUS}, CmdZoomIn},
    {{kShift | kCtrl | kVirt, VK_OEM_PLUS}, CmdRotateRight},
    {{kVirt, VK_F3}, CmdFindNext},
    {{kShift | kVirt, VK_F3}, CmdFindPrev},
    {{kCtrl | kVirt, VK_F3}, CmdFindNextSel},
    {{kShift | kCtrl | kVirt, VK_F3}, CmdFindPrevSel},
    {{kCtrl | kVirt, VK_F4}, CmdClose},
    {{kVirt, VK_F6}, CmdMoveFrameFocus},
    {{kVirt, VK_F7}, CmdSelectTextViaKeyboard},
    {{kVirt, VK_F8}, CmdToggleToolbar},
    {{kVirt, VK_F9}, CmdToggleMenuBar},
    {{kCtrl | kVirt, 'L'}, CmdTogglePresentationMode},
    {{kVirt, VK_F5}, CmdTogglePresentationMode},
    {{kShift | kVirt, VK_F11}, CmdTogglePresentationMode},
    {{kShift | kCtrl | kVirt, 'L'}, CmdToggleFullscreen},
    {{kVirt, VK_F11}, CmdToggleFullscreen},
    {{kShift | kCtrl | kVirt, 'H'}, CmdToggleAutomaticallyScroll},
    {{kVirt, VK_F12}, CmdToggleBookmarks},
    {{kShift | kVirt, VK_F12}, CmdCommandPaletteTOC},
    {{kShift | kCtrl | kVirt, VK_SUBTRACT}, CmdRotateLeft},
    {{kShift | kCtrl | kVirt, VK_OEM_MINUS}, CmdRotateLeft},
    {{kShift | kCtrl | kVirt, 'T'}, CmdReopenLastClosedFile},
    {{kCtrl | kVirt, VK_NEXT}, CmdNextTab},
    {{kCtrl | kVirt, VK_PRIOR}, CmdPrevTab},
    {{kCtrl | kShift | kVirt, VK_NEXT}, CmdMoveTabRight},
    {{kCtrl | kShift | kVirt, VK_PRIOR}, CmdMoveTabLeft},
    {{kCtrl | kVirt, VK_TAB}, CmdNextTabSmart},
    {{kCtrl | kShift | kVirt, VK_TAB}, CmdPrevTabSmart},
    {{kVirt, VK_F1}, CmdHelpOpenManual},
    // '?' i.e. Shift + '/'
    {{kShift | kVirt, VK_OEM_2}, CmdToggleKeyboardHelp},

    {{kVirt, 'A'}, CmdCreateAnnotHighlight},
    {{kVirt, 'U'}, CmdCreateAnnotUnderline},

    {{kVirt | kShift, 'I'}, CmdInvertColors},
    {{kVirt, 'I'}, CmdTogglePageInfo},

    {{kCtrl | kVirt, VK_DELETE}, CmdDeleteAnnotation},

    {{kVirt, 'Q'}, CmdCloseCurrentDocument},
    {{kVirt, 'R'}, CmdReloadDocument},
    {{kVirt, 'Z'}, CmdToggleZoom},
    {{kVirt, 'F'}, CmdToggleFullscreen},
    {{kShift | kVirt, 'F'}, CmdToggleKeyboardLinkFollowing},
    // '['
    {{kVirt, VK_OEM_4}, CmdRotateLeft},
    // ']'
    {{kVirt, VK_OEM_6}, CmdRotateRight},
    {{kVirt, 'M'}, CmdToggleCursorPosition},
    {{kVirt, 'W'}, CmdPresentationWhiteBackground},
    // for Logitech's wireless presenters which target PowerPoint's shortcuts
    // fVirt 0 makes this an ASCII accelerator: it matches the typed '.'
    // on any layout (VK_OEM_PERIOD is only the US-layout virtual key)
    {{0, '.'}, CmdPresentationBlackBackground},
    {{kVirt, 'C'}, CmdToggleContinuousView},

    // Shift + CreateAnnot*: cmd patched to "CmdCreateAnnot* openedit" before
    // the accelerator table is built, so they turn on Edit PDF mode.
    {{kVirt | kShift, 'A'}, CmdCreateAnnotHighlight},
    {{kVirt | kShift, 'U'}, CmdCreateAnnotUnderline},
};
// NOLINTEND(modernize-use-designated-initializers)

static Accel* gAccels = nullptr;
static int gAccelsCount = 0;
static AccelStroke* gAccelStrokes = nullptr;
static int gAccelStrokesCount = 0;
#if OS_WIN
static HACCEL gAccelTables[3]{};
#endif

// Custom Shortcuts clone a unique command id, so the accelerator is stored
// under that id. Treat it as the original command when looking up bindings.
static bool AccelIsForCmd(const Accel& a, int cmdId) {
    if (a.cmd == cmdId) {
        return true;
    }
    CustomCommand* cmd = FindCustomCommand(a.cmd);
    return cmd && cmd->origId == cmdId;
}

// the key cmdId is bound to, appended to a menu string. Parsing shortcut
// strings lives in ShortcutParse.h.
TempStr AppendAccelKeyToMenuStringTemp(TempStr menuStr, int cmdId) {
    for (int i = 0; i < gAccelsCount; i++) {
        const Accel& a = gAccels[i];
        if (AccelIsForCmd(a, cmdId)) {
            TempStr res = AppendAccelKeyToMenuStringTemp(menuStr, a.sc);
            return res;
        }
    }
    return menuStr;
}

// All keys bound to cmdId, formatted for display and joined with ", ", e.g.
// "↑, K" or "Ctrl + F". Returns empty when nothing is bound. maxCount caps how
// many bindings are listed (a command can have several, like arrows + hjkl).
// Reads the effective, user-override-aware table, so it shows what actually
// works right now.
TempStr ShortcutsForCmdTemp(int cmdId, int maxCount) {
    TempStr res = str::DupTemp(StrL(""));
    int n = 0;
    for (int i = 0; i < gAccelsCount && n < maxCount; i++) {
        const Accel& a = gAccels[i];
        if (!AccelIsForCmd(a, cmdId)) {
            continue;
        }
        TempStr withTab = AppendAccelKeyToMenuStringTemp(StrL(""), a.sc);
        if (len(withTab) == 0 || withTab.s[0] != '\t') {
            continue;
        }
        TempStr key = Str(withTab.s + 1); // drop the leading '\t'
        if (len(key) == 0) {
            continue;
        }
        // skip a duplicate that formats to the same text (e.g. '+' and numpad '+')
        if (n > 0 && str::Contains(res, key)) {
            continue;
        }
        if (n > 0) {
            res = str::JoinTemp(res, StrL(", "));
        }
        res = str::JoinTemp(res, key);
        n++;
    }
    return res;
}

static bool sameAccelKey(const Accel& a1, const Accel& a2) {
    return a1.sc.SameKey(a2.sc);
}

// clang-format off
static u16 gNotSafeKeys[] = {
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
    // like Home/End: a list or tree with the focus pages through its own items.
    // The find window's results list only got them once they stopped
    // accelerating to CmdScrollUpPage / CmdScrollDownPage (issue #6117)
    VK_PRIOR,
    VK_NEXT,
    VK_OEM_4,
    VK_OEM_6,
    VK_OEM_2 // '?' opens keyboard help, but must still type into edit controls
};
// clang-format on

// a hackish way to determine if we should allow processing a given
// accelerator in custom controls. This is to disable accelerators
// like 'n' or 'left arrow' in e.g. edit control so that they don't
// block regular processing of key events and mess up edit control
// at the same time, we do want most accelerators to be enabed even
// if edit or tree view control has focus
bool IsSafeAccel(const Accel& a) {
    u16 k = a.sc.vk;
    u8 mods = a.sc.Mods();
    if (mods == 0) {
        // regular keys like 'n', without any shift / alt modifier
        return false;
    }

    // regular keys are also coded as FVIRTKEY or FVIRTKEY | FSHIFT
    // so that they work based on virtual keyboard code to support
    // non-english keyboards
    if (k >= 'A' && k <= 'Z') {
        if (mods == kVirt) {
            return false;
        }
        if (mods == (kVirt | kShift)) {
            return false;
        }
    }

    // whitelist Alt + Left, Alt + Right to enable document
    // navigation when focus is in edit or tree control
    // https://github.com/sumatrapdfreader/sumatrapdf/issues/3688#issuecomment-1728271753
    if (mods == (kVirt | kAlt)) {
        if ((k == VK_LEFT) || (k == VK_RIGHT)) {
            return true;
        }
    }

    if ((mods == (kCtrl | kVirt)) && (k == 'V')) {
        // Ctrl+V should work normally in edit controls (paste text)
        return false;
    }

    for (u16 notSafe : gNotSafeKeys) {
        if (notSafe == k) {
            return false;
        }
    }
    return true;
}

// Folder browsing (Ctrl+Shift+arrows) is meaningless to a tree view or to a
// document shown in the WebView2, but an edit control uses those keys to select
// by word. So it's safe everywhere except the edit table: after clicking a link
// in a .md / .html file the arrows stopped browsing the folder (issue #6089).
static bool isSafeOutsideEditAccel(const Accel& a) {
    if (IsSafeAccel(a)) {
        return true;
    }
    // a plain arrow rebound to the command still has to move in the tree
    bool isChord = a.sc.ctrl || a.sc.alt;
    if (!isChord) {
        return false;
    }
    switch (a.cmd) {
        case CmdOpenNextFileInFolder:
        case CmdOpenPrevFileInFolder:
        case CmdNavigateFilesInFolder:
            return true;
    }
    return false;
}

// keys the tree uses to move / activate; those stay with the control even
// when a command is bound to them. Ctrl/Alt chords are still accelerators.
// PageUp / PageDown are not here: they scroll the document (issue #1841)
static bool isTreeNavKey(u16 k) {
    switch (k) {
        case VK_LEFT:
        case VK_RIGHT:
        case VK_UP:
        case VK_DOWN:
        case VK_HOME:
        case VK_END:
        case VK_SPACE:
        case VK_RETURN:
        case VK_TAB:
        case VK_ADD:
        case VK_SUBTRACT:
        case VK_MULTIPLY:
            return true;
    }
    return false;
}

// tree: letter shortcuts (and everything else) run the command; only the
// navigation keys above, without Ctrl/Alt, are left for the tree
bool IsSafeTreeAccel(const Accel& a) {
    if (a.sc.ctrl || a.sc.alt) {
        return true;
    }
    return !isTreeNavKey(a.sc.vk);
}

// Command bound to vk + modifiers among the "safe" accelerators (those allowed
// while a custom control has focus). 0 if none. Lets custom controls (e.g. the
// WebView2-hosted CHM) forward app shortcuts they'd otherwise swallow.
// Command bound to a key+modifiers among the accelerators that are "safe" to
// process while a custom control (edit / tree / WebView2-hosted CHM) has focus.
// Returns the command id, or 0 if none. Used to forward app shortcuts that a
// focused control would otherwise swallow.
int SafeAcceleratorCmd(u16 vk, bool ctrl, bool shift, bool alt) {
    u8 mods = kVirt;
    if (ctrl) {
        mods |= kCtrl;
    }
    if (shift) {
        mods |= kShift;
    }
    if (alt) {
        mods |= kAlt;
    }
    for (int i = 0; i < gAccelsCount; i++) {
        const Accel& a = gAccels[i];
        if (a.sc.vk == vk && a.sc.Mods() == mods && isSafeOutsideEditAccel(a)) {
            return a.cmd;
        }
    }
    return 0;
}

static Str CurrentLangCode() {
    return trans::GetCurrentLangCode();
}

namespace {
// ng: orig fills three win32 tables here (all / edit / tree view). We keep one
// and classify at lookup time; gpui decides per key context (step 6).
struct AccelTablesBuilder {
    Accel* accels = nullptr;
    int nAccels = 0;

    void Add(Accel accel);
};
} // namespace

void AccelTablesBuilder::Add(Accel accel) {
    for (int i = 0; i < nAccels; i++) {
        if (sameAccelKey(accels[i], accel)) {
            return;
        }
    }
    accels[nAccels++] = accel;
}

// custom commands that define a shortcut; an upper bound for the tables
static int CountCustomShortcuts() {
    int n = 0;
    for (auto* curr = gFirstCustomCommand; curr; curr = curr->next) {
        if ((curr->id > 0) && !str::IsEmptyOrWhiteSpace(curr->key) && !IsGlobalShortcut(curr->key) &&
            curr->origId != CmdScreenshot) {
            n++;
        }
    }
    return n;
}

static void AddCustomShortcuts(AccelTablesBuilder& b) {
    for (auto* curr = gFirstCustomCommand; curr; curr = curr->next) {
        if ((curr->id <= 0) || str::IsEmptyOrWhiteSpace(curr->key)) {
            continue;
        }
        // global shortcuts are registered with the OS, not accelerators
        if (IsGlobalShortcut(curr->key) || curr->origId == CmdScreenshot) {
            continue;
        }
        Accel accel{};
        accel.cmd = curr->id;
        if (ParseShortcutString(curr->key, accel.sc)) {
            b.Add(accel);
        }
    }
}

// Replace Shift+CreateAnnot* built-ins with a "CmdCreateAnnot* openedit"
// custom command so those shortcuts turn on Edit PDF mode. orig cmd ids are
// snapshotted once: LoadSettings frees custom commands and this runs again
// with new ids.
static void PatchCreateAnnotEditAccelerators() {
    static int origCmds[dimofi(gBuiltInAccelerators)];
    static bool didInit = false;
    if (!didInit) {
        for (int i = 0; i < dimofi(gBuiltInAccelerators); i++) {
            origCmds[i] = gBuiltInAccelerators[i].cmd;
        }
        didInit = true;
    }
    for (int i = 0; i < dimofi(gBuiltInAccelerators); i++) {
        int origId = origCmds[i];
        const KeyShortcut& sc = gBuiltInAccelerators[i].sc;
        if (!sc.shift) {
            continue;
        }
        if (sc.ctrl || sc.alt) {
            continue;
        }
        if (origId < CmdCreateAnnotFirst || origId > CmdCreateAnnotLast) {
            continue;
        }
        Str name = GetCommandName(origId);
        if (len(name) == 0) {
            continue;
        }
        CustomCommand* cmd = CreateCommandFromDefinition(fmt("%s openedit", name));
        if (cmd) {
            gBuiltInAccelerators[i].cmd = cmd->id;
        }
    }
}

// The gpui view of the table: one stroke per accelerator, duplicates and
// shortcuts gpui has no name for dropped.
static bool StrokeDup(int cmd, Str stroke) {
    for (int j = 0; j < gAccelStrokesCount; j++) {
        if (gAccelStrokes[j].cmd == cmd && str::Eq(gAccelStrokes[j].stroke, stroke)) {
            return true;
        }
    }
    return false;
}

static void AddAccelStroke(int cmd, Str stroke) {
    if (len(stroke) == 0 || StrokeDup(cmd, stroke)) {
        return;
    }
    gAccelStrokes[gAccelStrokesCount].stroke = str::Dup(stroke);
    gAccelStrokes[gAccelStrokesCount].cmd = cmd;
    gAccelStrokesCount++;
}

static void BuildAcceleratorStrokes() {
#if OS_DARWIN
    // Command stands in for Ctrl: Command+K opens the command palette
    int nSlots = gAccelsCount * 2;
#else
    int nSlots = gAccelsCount;
#endif
    gAccelStrokes = AllocArray<AccelStroke>(nSlots);
    gAccelStrokesCount = 0;
    for (int i = 0; i < gAccelsCount; i++) {
        TempStr stroke = ShortcutToGpuiStroke(gAccels[i].sc);
        if (len(stroke) == 0) {
            logf("BuildAcceleratorStrokes: no gpui key name for vk 0x%x\n", gAccels[i].sc.vk);
            continue;
        }
        AddAccelStroke(gAccels[i].cmd, stroke);
#if OS_DARWIN
        if (str::StartsWith(stroke, StrL("ctrl-"))) {
            TempStr asCmd = str::JoinTemp(StrL("cmd-"), Str(stroke.s + 5, stroke.len - 5));
            AddAccelStroke(gAccels[i].cmd, asCmd);
        }
#endif
    }
}

#if OS_WIN
static ACCEL ToWinAccel(const Accel& a) {
    ACCEL res{};
    res.fVirt = a.sc.Mods();
    res.key = a.sc.vk;
    res.cmd = (WORD)a.cmd;
    return res;
}

static void BuildWinAcceleratorTables() {
    ACCEL* all = AllocArrayTemp<ACCEL>(gAccelsCount);
    ACCEL* edit = AllocArrayTemp<ACCEL>(gAccelsCount);
    ACCEL* tree = AllocArrayTemp<ACCEL>(gAccelsCount);
    int nEdit = 0;
    int nTree = 0;

    for (int i = 0; i < gAccelsCount; i++) {
        const Accel& a = gAccels[i];
        all[i] = ToWinAccel(a);
        if (IsSafeAccel(a)) {
            edit[nEdit++] = all[i];
        }
        if (IsSafeTreeAccel(a)) {
            tree[nTree++] = all[i];
        }
    }

    gAccelTables[0] = CreateAcceleratorTableW(all, gAccelsCount);
    gAccelTables[1] = CreateAcceleratorTableW(edit, nEdit);
    gAccelTables[2] = CreateAcceleratorTableW(tree, nTree);
    ReportIf(!gAccelTables[0] || !gAccelTables[1] || !gAccelTables[2]);
}
#endif

void CreateSumatraAcceleratorTable() {
    gShortcutLangCode = CurrentLangCode;
    ReportIf(gAccels);

    PatchCreateAnnotEditAccelerators();

    // an upper bound: Add() appends at most one entry per call, and it's called
    // once per built-in and once per custom shortcut
    int nMax = dimofi(gBuiltInAccelerators) + CountCustomShortcuts();

    AccelTablesBuilder b;
    // accels outlives us in gAccels, so it has to be a real allocation
    b.accels = AllocArray<Accel>(nMax);

    AddCustomShortcuts(b);
    // add built-in but only if the shortcut doesn't conflict with custom shortcut
    for (Accel accel : gBuiltInAccelerators) {
        b.Add(accel);
    }

    gAccels = b.accels;
    gAccelsCount = b.nAccels;
}

void FreeAcceleratorTables() {
#if OS_WIN
    for (HACCEL& table : gAccelTables) {
        DestroyAcceleratorTable(table);
        table = nullptr;
    }
#endif
    for (int i = 0; i < gAccelStrokesCount; i++) {
        str::FreePtr(&gAccelStrokes[i].stroke);
    }
    free(gAccelStrokes);
    gAccelStrokes = nullptr;
    gAccelStrokesCount = 0;
    free(gAccels);
    gAccels = nullptr;
    gAccelsCount = 0;
}

const Accel* GetAcceleratorTable(int& nOut) {
    if (!gAccels) {
        CreateSumatraAcceleratorTable();
    }
    nOut = gAccelsCount;
    return gAccels;
}

const AccelStroke* GetAcceleratorStrokes(int& nOut) {
    if (!gAccels) {
        CreateSumatraAcceleratorTable();
    }
    if (!gAccelStrokes) {
        BuildAcceleratorStrokes();
    }
    nOut = gAccelStrokesCount;
    return gAccelStrokes;
}

#if OS_WIN
HACCEL* GetAcceleratorTables() {
    if (!gAccels) {
        CreateSumatraAcceleratorTable();
    }
    if (!gAccelTables[0]) {
        BuildWinAcceleratorTables();
    }
    return gAccelTables;
}
#endif

#if IS_DEBUG
// Folder browsing has to keep working when focus is inside the document (the
// WebView2 that shows .md / .html) or the bookmarks tree (issue #6089)
bool Accelerators_UnitTestFolderNavIsSafe() {
    int n = 0;
    GetAcceleratorTable(n); // builds gAccels if it isn't built yet
    if (SafeAcceleratorCmd(VK_RIGHT, true, true, false) != CmdOpenNextFileInFolder) {
        return false;
    }
    if (SafeAcceleratorCmd(VK_LEFT, true, true, false) != CmdOpenPrevFileInFolder) {
        return false;
    }
    if (SafeAcceleratorCmd(VK_UP, true, true, false) != CmdNavigateFilesInFolder) {
        return false;
    }
    // a bare arrow still belongs to the control, so it can scroll / move the selection
    if (SafeAcceleratorCmd(VK_RIGHT, false, false, false) != 0) {
        return false;
    }
    return true;
}

// bookmarks / favorites tree: letter shortcuts run the command (e.g. t bound
// to CmdToggleBookmarks); arrows stay on the tree
bool Accelerators_UnitTestTreeTakesLetters() {
    Accel letter{{kVirt, 'T'}, CmdToggleBookmarks};
    if (!IsSafeTreeAccel(letter)) {
        return false;
    }
    Accel up{{kVirt, VK_UP}, CmdScrollUp};
    if (IsSafeTreeAccel(up)) {
        return false;
    }
    Accel pgDn{{kVirt, VK_NEXT}, CmdScrollDownPage};
    if (!IsSafeTreeAccel(pgDn)) {
        return false;
    }
    Accel enter{{kVirt, VK_RETURN}, CmdScrollDownPage};
    if (IsSafeTreeAccel(enter)) {
        return false;
    }
    Accel ctrlUp{{kCtrl | kVirt, VK_UP}, CmdScrollUpPage};
    if (!IsSafeTreeAccel(ctrlUp)) {
        return false;
    }
    return true;
}

bool Accelerators_UnitTestCreateAnnotEdit() {
    int nAccels = 0;
    GetAcceleratorTable(nAccels);
    bool plainA = false;
    bool shiftA = false;
    bool plainU = false;
    bool shiftU = false;
    for (int i = 0; i < gAccelsCount; i++) {
        const Accel& a = gAccels[i];
        if (a.sc.vk != 'A' && a.sc.vk != 'U') {
            continue;
        }
        if (a.sc.Mods() == kVirt) {
            if (a.sc.vk == 'A') {
                plainA = a.cmd == CmdCreateAnnotHighlight;
            } else {
                plainU = a.cmd == CmdCreateAnnotUnderline;
            }
            continue;
        }
        if (a.sc.Mods() != (kVirt | kShift)) {
            continue;
        }
        CustomCommand* cmd = FindCustomCommand(a.cmd);
        if (!cmd || !GetCommandBoolArg(cmd, kCmdArgOpenEdit, false)) {
            continue;
        }
        if (a.sc.vk == 'A') {
            shiftA = cmd->origId == CmdCreateAnnotHighlight;
        } else {
            shiftU = cmd->origId == CmdCreateAnnotUnderline;
        }
    }
    return plainA && shiftA && plainU && shiftU;
}

// Shortcuts entries clone a unique command id; help/menus must still list
// that key on the original command.
bool Accelerators_UnitTestCustomShortcutShown() {
    int nAccels = 0;
    GetAcceleratorTable(nAccels);
    auto* base = CreateCommandFromDefinition(StrL("CmdOpenFile"));
    if (!base) {
        return false;
    }
    auto* cmd = CloneCustomCommand(base, {}, StrL("Ctrl + Shift + F24"));
    if (!cmd || cmd->id == CmdOpenFile) {
        return false;
    }
    FreeAcceleratorTables();
    CreateSumatraAcceleratorTable();
    TempStr keys = ShortcutsForCmdTemp(CmdOpenFile, 8);
    return str::Contains(keys, StrL("Ctrl + Shift + F24"));
}
#endif
