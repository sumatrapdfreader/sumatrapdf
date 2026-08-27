/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "Settings.h"
#include "Commands.h"
#include "ShortcutParse.h"
#include "Translations.h"
#include "Accelerators.h"

// note: even letter shortcuts like 'k' are marked as FVIRTKEY so that they
// work even on non-english keyboards (cyrillic, hebrew)
// VK_A is 'A' etc. which corresponds to 'a' key.
// To get 'A' need explicitly use FSHIFT.
// https://learn.microsoft.com/en-us/windows/win32/menurc/using-keyboard-accelerators?referrer=grok.com
// https://grok.com/share/bGVnYWN5_d83c2956-4ce2-4c74-ba4d-9794d1760ccb?rid=746312cc-7d0f-4479-abec-25c394652cac
static ACCEL gBuiltInAccelerators[] = {
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
    {FSHIFT | FCONTROL | FVIRTKEY, VK_UP, CmdNavigateFilesInFolder},
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
    {FCONTROL | FVIRTKEY, 'X', CmdCutAnnotation},
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
    {FCONTROL | FVIRTKEY, '4', CmdZoomToSelection},
    {FCONTROL | FVIRTKEY, VK_NUMPAD4, CmdZoomToSelection},
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
    {FVIRTKEY, VK_F7, CmdSelectTextViaKeyboard},
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
    // '?' i.e. Shift + '/'
    {FSHIFT | FVIRTKEY, VK_OEM_2, CmdToggleKeyboardHelp},

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
    {FSHIFT | FVIRTKEY, 'F', CmdToggleKeyboardLinkFollowing},
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

static ACCEL* gAccels = nullptr;
static int gAccelsCount = 0;

// the key cmdId is bound to, appended to a menu string. Parsing shortcut
// strings lives in ShortcutParse.h.
TempStr AppendAccelKeyToMenuStringTemp(TempStr menuStr, int cmdId) {
    ACCEL a;
    for (int i = 0; i < gAccelsCount; i++) {
        a = gAccels[i];
        if (a.cmd == cmdId) {
            TempStr res = AppendAccelKeyToMenuStringTemp(menuStr, a);
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
        ACCEL a = gAccels[i];
        if (a.cmd != cmdId) {
            continue;
        }
        TempStr withTab = AppendAccelKeyToMenuStringTemp(StrL(""), a);
        if (!withTab || withTab.s[0] != '\t') {
            continue;
        }
        TempStr key = Str(withTab.s + 1); // drop the leading '\t'
        if (key.len == 0) {
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
// Command bound to a key+modifiers among the accelerators that are "safe" to
// process while a custom control (edit / tree / WebView2-hosted CHM) has focus.
// Returns the command id, or 0 if none. Used to forward app shortcuts that a
// focused control would otherwise swallow.
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
static Str CurrentLangCode() {
    return trans::GetCurrentLangCode();
}

namespace {
// accumulates accelerators into the three tables, dropping duplicate shortcuts
struct AccelTablesBuilder {
    ACCEL* accels = nullptr;
    ACCEL* editAccels = nullptr;
    ACCEL* treeViewAccels = nullptr;
    int nAccels = 0;
    int nEditAccels = 0;
    int nTreeViewAccels = 0;

    void Add(ACCEL accel);
};
} // namespace

void AccelTablesBuilder::Add(ACCEL accel) {
    for (int i = 0; i < nAccels; i++) {
        if (sameAccelKey(accels[i], accel)) {
            return;
        }
    }
    accels[nAccels++] = accel;
    if (isSafeAccel(accel)) {
        editAccels[nEditAccels++] = accel;
        treeViewAccels[nTreeViewAccels++] = accel;
        return;
    }
    if ((int)accel.cmd == (int)CmdToggleBookmarks) {
        // https://github.com/sumatrapdfreader/sumatrapdf/issues/2832
        treeViewAccels[nTreeViewAccels++] = accel;
    }
}

// custom commands that define a shortcut; an upper bound for the tables
static int CountCustomShortcuts() {
    int n = 0;
    for (auto* curr = gFirstCustomCommand; curr; curr = curr->next) {
        if ((curr->id > 0) && !str::IsEmptyOrWhiteSpace(curr->key)) {
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
        // CmdScreenshot shortcuts are registered as global hotkeys, not accelerators
        if (curr->origId == CmdScreenshot) {
            continue;
        }
        ACCEL accel{};
        accel.cmd = (WORD)curr->id;
        if (ParseShortcutString(curr->key, accel)) {
            b.Add(accel);
        }
    }
}

void CreateSumatraAcceleratorTable() {
    gShortcutLangCode = CurrentLangCode;
    ReportIf(gAccelTables[0] || gAccelTables[1] || gAccelTables[2]);

    // an upper bound for all three tables: Add() appends at most one entry to
    // each per call, and it's called once per built-in and once per custom shortcut
    int nMax = dimofi(gBuiltInAccelerators) + CountCustomShortcuts();

    AccelTablesBuilder b;
    // accels outlives us in gAccels, so it has to be a real allocation. the edit /
    // tree view arrays are only read by CreateAcceleratorTableW below, so they come
    // from the temp arena. They're allocated separately: sizeof(ACCEL) is 6, so
    // carving them out of one block needed the count rounded up to keep the second
    // one aligned (https://github.com/sumatrapdfreader/sumatrapdf/issues/2981)
    b.accels = AllocArray<ACCEL>(nMax);
    b.editAccels = AllocArrayTemp<ACCEL>(nMax);
    b.treeViewAccels = AllocArrayTemp<ACCEL>(nMax);

    AddCustomShortcuts(b);
    // add built-in but only if the shortcut doesn't conflict with custom shortcut
    for (ACCEL accel : gBuiltInAccelerators) {
        b.Add(accel);
    }

    gAccels = b.accels;
    gAccelsCount = b.nAccels;

    gAccelTables[0] = CreateAcceleratorTableW(gAccels, gAccelsCount);
    ReportIf(gAccelTables[0] == nullptr);
    gAccelTables[1] = CreateAcceleratorTableW(b.editAccels, b.nEditAccels);
    ReportIf(gAccelTables[1] == nullptr);
    gAccelTables[2] = CreateAcceleratorTableW(b.treeViewAccels, b.nTreeViewAccels);
    ReportIf(gAccelTables[2] == nullptr);
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
