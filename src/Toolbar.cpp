/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "gui/Dpi.h"
#include "base/Win.h"
#include "base/BitManip.h"
#include "base/Pixmap.h"

extern "C" {
#include <mupdf/fitz.h>
}

#include "gui/UIModels.h"

#include "Accelerators.h"
#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "ImageReader.h"
#include "GlobalPrefs.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "resource.h"
#include "Commands.h"
#include "AppTools.h"
#include "CommandAvailability.h"
#include "Menu.h"
#include "SearchAndDDE.h"
#include "Toolbar.h"
#include "Tabs.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"
#include "gui/win/TabsCtrl.h"
#include "FindBar.h"
#include "Translations.h"
#include "SvgIcons.h"
#include "Theme.h"
#include "DarkModeSubclass.h"
#include "TextToSpeech.h"

// https://docs.microsoft.com/en-us/windows/win32/controls/toolbar-control-reference

static int kButtonSpacingX = 4;

// distance between label and edit field
constexpr int kTextPaddingRight = 6;

struct ToolbarButtonInfo {
    const char* icon = nullptr; // gIcon*, or null for a separator / page box / text
    int cmdId = 0;
    Str toolTip;
    Str svgIcon; // custom SVG from settings
    bool isText = false;
};

// thos are not real commands but we have to refer to toolbar buttons
// is by a command. those are just background for area to be
// covered by other HWNDs. They need the right size
constexpr int PageInfoId = (int)CmdLast + 16;
constexpr int WarningMsgId = (int)CmdLast + 17;

static ToolbarButtonInfo gToolbarButtons[] = {
    {gIconFileOpen, CmdOpenFile, _TRN("Open")},
    {gIconPrint, CmdPrint, _TRN("Print")},
    {nullptr, 0, nullptr},          // separator
    {nullptr, PageInfoId, nullptr}, // text box for page number + show current page / no of pages
    {gIconPagePrev, CmdGoToPrevPage, _TRN("Previous Page")},
    {gIconPageNext, CmdGoToNextPage, _TRN("Next Page")},
    {nullptr, 0, nullptr}, // separator
    {gIconNavigateBack, CmdNavigateBack, _TRN("Back")},
    {gIconNavigateForward, CmdNavigateForward, _TRN("Forward")},
    {nullptr, 0, nullptr}, // separator
    {gIconSpeak, CmdReadAloud, _TRN("Read Aloud")},
    {nullptr, 0, nullptr}, // separator
    {gIconLayoutContinuous, CmdZoomFitWidthAndContinuous, _TRN("Fit Width and Show Pages Continuously")},
    {gIconLayoutSinglePage, CmdZoomFitPageAndSinglePage, _TRN("Fit a Single Page")},
    {gIconRotateLeft, CmdRotateLeft, _TRN("Rotate &Left")},
    {gIconRotateRight, CmdRotateRight, _TRN("Rotate &Right")},
    {gIconZoomOut, CmdZoomOut, _TRN("Zoom Out")},
    {gIconZoomIn, CmdZoomIn, _TRN("Zoom In")},
    {nullptr, 0, nullptr}, // separator
    {gIconSearch, CmdFindFirst, _TRN("Find")},
};
// unicode chars: https://www.compart.com/en/unicode/U+25BC

constexpr int kButtonsCount = dimof(gToolbarButtons);

// The built-in buttons actually on the toolbar, which is gToolbarButtons unless
// ToolbarCustomLayout asks for a different set / order (issue #5095). A layout
// can repeat a button, so allow for more than the default count.
constexpr int kMaxLayoutButtons = 64;
static ToolbarButtonInfo gLayoutButtons[kMaxLayoutButtons];
static int gLayoutButtonsCount = 0;
// the page number box is optional in a custom layout; the controls that float
// over it are hidden when it isn't there
static bool gLayoutHasPageBox = true;
static Str gLayoutParsedFrom;
static bool gLayoutParsed = false;

// 128 should be more than enough
// we use static array so that we don't have to generate
// code for Vec<ToolbarButtonInfo>
constexpr int kMaxCustomButtons = 127;
// +1 to ensure there's always space for WarningsMsgId button
static ToolbarButtonInfo gCustomButtons[kMaxCustomButtons + 1];
static int gCustomButtonsCount = 0;

static HIMAGELIST gTbHiml = nullptr;

HIMAGELIST GetToolbarImageList() {
    return gTbHiml;
}

void DestroyToolbarImageList() {
    if (gTbHiml) {
        ImageList_Destroy(gTbHiml);
        gTbHiml = nullptr;
    }
}

// Light theme ControlBackgroundColor is white, which is what the old themed
// rebar/toolbar painted. Other themes use their control background.
static Color TbBgColor() {
    return ThemeControlBackgroundColor();
}

static Color TbTextColor() {
    if (IsCurrentThemeDefault() && !ThemeColorizeControls()) {
        return GetSysColor(COLOR_BTNTEXT);
    }
    return ThemeWindowTextColor();
}

static Color TbDisabledColor() {
    if (IsCurrentThemeDefault() && !ThemeColorizeControls()) {
        return GetSysColor(COLOR_GRAYTEXT);
    }
    return ThemeWindowTextDisabledColor();
}

static Color TbHoverColor() {
    return ThemeHotBackgroundColor();
}

static Color TbSelectedColor() {
    return AccentColor(TbBgColor(), 28);
}

static Color TbEdgeColor() {
    return ThemeEdgeColor();
}

// Old Win32 toolbar: TBMETRICS.cyPad defaults to 6, then we added DpiScale(2).
// TB_SETBUTTONSIZE cannot go below image + 2*cyPad, so that was the bar height.
static int ToolbarCyPad() {
    return 6 + DpiScale(2);
}

static int ToolbarRowDy(int iconSize) {
    return iconSize + (2 * ToolbarCyPad());
}

static void RelayoutToolbar(MainWindow* win);
static bool IsVirtKind(VirtCtrl* w, const char* k);

struct ToolbarVirt {
    MainWindow* win = nullptr;
    ILayout* layout = nullptr;
    VirtRoot* vroot = nullptr;
    Vec<VirtCtrl*> items; // not owned; layout owns them
    VirtText* pageLabel = nullptr;
    VirtText* pageTotal = nullptr;
    VirtCtrl* pageEditSlot = nullptr;
    HFONT font = nullptr;
    PlatformFont* platformFont = nullptr;
    int iconSize = 0;
    int rowDy = 0;
    Vec<Pixmap*> ownedPixmaps;
};

static const WStr kVirtToolbarClass = WStrL(L"SUMATRA_VIRT_TOOLBAR");

static bool SkipBuiltInButton(const ToolbarButtonInfo& tbi) {
    return !tbi.icon && !tbi.isText && str::IsEmptyOrWhiteSpace(tbi.svgIcon);
}

static VirtCtrl* ToolbarItemAt(MainWindow* win, int idx) {
    ToolbarVirt* tb = win ? win->toolbarVirt : nullptr;
    if (!tb || idx < 0 || idx >= len(tb->items)) {
        return nullptr;
    }
    return tb->items[idx];
}

// Includes disabled items (those are not hit-testable), so a click on a gray
// button is not treated as empty toolbar and does not start a window drag.
static VirtCtrl* ToolbarItemFromPoint(MainWindow* win, Point pt) {
    ToolbarVirt* tb = win ? win->toolbarVirt : nullptr;
    if (!tb) {
        return nullptr;
    }
    for (VirtCtrl* w : tb->items) {
        if (!w || w->GetVisibility() != Visibility::Visible) {
            continue;
        }
        if (w->BoundsInWindow().Contains(pt)) {
            return w;
        }
    }
    if (tb->pageEditSlot && tb->pageEditSlot->GetVisibility() == Visibility::Visible &&
        tb->pageEditSlot->BoundsInWindow().Contains(pt)) {
        return tb->pageEditSlot;
    }
    if (tb->pageTotal && tb->pageTotal->GetVisibility() == Visibility::Visible &&
        tb->pageTotal->BoundsInWindow().Contains(pt)) {
        return tb->pageTotal;
    }
    return nullptr;
}

static void UpdateToolbarButtonStateByIdx(MainWindow* win, int idx, bool set, BYTE flag) {
    VirtCtrl* w = ToolbarItemAt(win, idx);
    if (!w) {
        return;
    }
    if (flag == TBSTATE_ENABLED) {
        if (w->IsEnabled() == set) {
            return;
        }
        w->SetIsEnabled(set);
        w->Invalidate();
        return;
    }
    if (flag == TBSTATE_HIDDEN) {
        Visibility want = set ? Visibility::Collapse : Visibility::Visible;
        if (w->GetVisibility() == want) {
            return;
        }
        w->SetVisibility(want);
        ToolbarVirt* tb = win->toolbarVirt;
        if (w->id == PageInfoId && tb) {
            if (tb->pageLabel) {
                tb->pageLabel->SetVisibility(want);
            }
            if (tb->pageEditSlot) {
                tb->pageEditSlot->SetVisibility(want);
            }
            if (tb->pageTotal) {
                tb->pageTotal->SetVisibility(want);
            }
            if (win->hwndPageEdit) {
                ShowWindow(win->hwndPageEdit, set ? SW_HIDE : SW_SHOW);
            }
        }
        if (tb && tb->vroot) {
            tb->vroot->RequestLayout();
        }
        RelayoutToolbar(win);
        HwndInvalidate(win->hwndToolbar, true);
        return;
    }
    if (flag == TBSTATE_CHECKED) {
        auto* ib = (VirtIconButton*)w;
        if (ib->isSelected == set) {
            return;
        }
        ib->isSelected = set;
        ib->Invalidate();
    }
}

// Work out which built-in buttons the toolbar has, and in which order. Empty
// ToolbarCustomLayout (the default) means the standard layout; otherwise the
// setting lists the buttons the user wants: a command name puts that button
// there, `|` a separator, `PageInfo` the page number box, and leaving a button
// out is how you hide it (issue #5095).
static void PopulateToolbarLayout() {
    Str setting = gGlobalPrefs->toolbarCustomLayout;
    if (gLayoutParsed && str::Eq(setting, gLayoutParsedFrom)) {
        return;
    }
    str::Free(gLayoutParsedFrom);
    gLayoutParsedFrom = str::Dup(setting);
    gLayoutParsed = true;
    gLayoutButtonsCount = 0;
    gLayoutHasPageBox = false;

    auto addButton = [](const ToolbarButtonInfo& tbi) {
        if (gLayoutButtonsCount < kMaxLayoutButtons) {
            gLayoutButtons[gLayoutButtonsCount++] = tbi;
        }
    };

    if (str::IsEmptyOrWhiteSpace(setting)) {
        for (int i = 0; i < kButtonsCount; i++) {
            addButton(gToolbarButtons[i]);
        }
        gLayoutHasPageBox = true;
        return;
    }

    // commas and semicolons are a natural way to write a list, so accept them
    TempStr normalized = str::ReplaceTemp(setting, StrL(","), StrL(" "));
    normalized = str::ReplaceTemp(normalized, StrL(";"), StrL(" "));
    StrVec names;
    Split(&names, normalized, StrL(" "), true);
    for (Str name : names) {
        Str tok = name;
        str::TrimWSInPlace(tok, str::TrimOpt::Both);
        if (!tok) {
            continue;
        }
        if (str::Eq(tok, StrL("|")) || str::EqI(tok, StrL("Separator"))) {
            addButton({nullptr, 0, nullptr});
            continue;
        }
        if (str::EqI(tok, StrL("PageInfo"))) {
            addButton({nullptr, PageInfoId, nullptr});
            gLayoutHasPageBox = true;
            continue;
        }
        int cmdId = GetCommandIdByName(tok);
        const ToolbarButtonInfo* found = nullptr;
        for (int i = 0; i < kButtonsCount && cmdId != CmdNone; i++) {
            if (gToolbarButtons[i].cmdId == cmdId) {
                found = &gToolbarButtons[i];
                break;
            }
        }
        if (!found) {
            logf("ToolbarCustomLayout: no built-in toolbar button for '%s'\n", tok);
            continue;
        }
        addButton(*found);
    }
    if (gLayoutButtonsCount == 0) {
        logf("ToolbarCustomLayout: nothing usable in '%s', using the standard layout\n", setting);
        str::FreePtr(&gLayoutParsedFrom);
        gLayoutParsed = false;
        Str prev = gGlobalPrefs->toolbarCustomLayout;
        gGlobalPrefs->toolbarCustomLayout = nullptr;
        PopulateToolbarLayout();
        gGlobalPrefs->toolbarCustomLayout = prev;
    }
}

static int TotalButtonsCount() {
    return gLayoutButtonsCount + gCustomButtonsCount;
}

static ToolbarButtonInfo& GetToolbarButtonInfoByIdx(int idx) {
    if (idx < gLayoutButtonsCount) return gLayoutButtons[idx];
    return gCustomButtons[idx - gLayoutButtonsCount];
}

// more than one because users can add custom buttons with overlapping ids
static int GetToolbarButtonsByID(int cmdId, int (&buttons)[4]) {
    int nFound = 0;
    int n = TotalButtonsCount();
    for (int idx = 0; idx < n; idx++) {
        ToolbarButtonInfo& tb = GetToolbarButtonInfoByIdx(idx);
        int tbCmdId = tb.cmdId;
        auto* cmd = FindCustomCommand(tbCmdId);
        if (cmd) tbCmdId = cmd->origId;
        cmd = FindCustomCommand(cmdId);
        if (cmd) cmdId = cmd->origId;
        if (cmdId != tbCmdId) continue;
        buttons[nFound++] = idx;
        if (nFound >= 4) {
            return nFound;
        }
    }
    return nFound;
}

void SetToolbarButtonCheckedState(MainWindow* win, int cmdId, bool isChecked) {
    int buttons[4];
    int n = GetToolbarButtonsByID(cmdId, buttons);
    if (n == 0) return;
    for (int i = 0; i < n; i++) {
        int idx = buttons[i];
        UpdateToolbarButtonStateByIdx(win, idx, isChecked, TBSTATE_CHECKED);
    }
}

// which documents support rotation
static bool NeedsRotateUI(MainWindow* win) {
    if (IsBrowserDocController(win->ctrl)) {
        return false;
    }
    return true;
}

// some commands are only avialble in certain contexts
// we remove toolbar buttons for un-availalbe commands
static bool IsCmdAvailable(MainWindow* win, int cmdId) {
    switch (cmdId) {
        case CmdZoomFitWidthAndContinuous:
        case CmdZoomFitPageAndSinglePage:
            return !IsBrowserDocController(win->ctrl);
        case CmdRotateLeft:
        case CmdRotateRight:
            return NeedsRotateUI(win);
        case CmdFindFirst:
            // CHM has its own (WebView2/IE) find bar even though NeedsFindUI()
            // is false for it; show the Search button so it's reachable
            return NeedsFindUI(win) || IsBrowserDocController(win->ctrl);
        case CmdFindNext:
        case CmdFindPrev:
        case CmdFindToggleMatchCase:
        case CmdFindToggleMatchWholeWord:
            return NeedsFindUI(win);
        case CmdReadAloud:
            // opt-in: the button and its drop-down only show if asked for
            return gGlobalPrefs->toolbarShowReadAloud;
        case PageInfoId:
            return true;
    }
    auto* ctx = NewBuildMenuCtx(win->CurrentTab(), Point{0, 0});
    AutoCall delCtx(DeleteBuildMenuCtx, ctx);
    // Toolbar buttons stay visible (but disabled) when no document is open, so
    // decide visibility as if a document were loaded; otherwise the no-document
    // gate in GetCommandVisibility would remove them. Document-type-specific
    // removals (e.g. for CHM/image collections) still apply when a real document
    // is loaded, and the enabled state is handled separately in IsCmdEnabled.
    ctx->isDocLoaded = true;
    bool remove, disable;
    GetCommandIdState(ctx, cmdId, &remove, &disable);
    return !remove;
}

static bool IsCmdEnabled(MainWindow* win, int cmdId) {
    auto* ctx = NewBuildMenuCtx(win->CurrentTab(), Point{0, 0});
    AutoCall delCtx(DeleteBuildMenuCtx, ctx);

    switch (cmdId) {
        case CmdNextTab:
        case CmdPrevTab:
        case CmdNextTabSmart:
        case CmdPrevTabSmart:
            return SettingsUseTabs();
        case PageInfoId:
            return true;
    }

    bool remove, disable;
    GetCommandIdState(ctx, cmdId, &remove, &disable);
    if (remove || disable) {
        return false;
    }
    bool isAllowed = true;
    switch (cmdId) {
        case CmdOpenFile:
            isAllowed = CanAccessDisk();
            break;
        case CmdPrint:
            isAllowed = HasPermission(Perm::PrinterAccess);
            break;
    }
    if (!isAllowed) {
        return false;
    }

    // if no file is open, only enable buttons for commands that don't require a document
    // (custom toolbar buttons use a custom command id, the original command decides)
    // https://github.com/sumatrapdfreader/sumatrapdf/issues/5657
    if (!win->IsDocLoaded()) {
        int realCmdId = cmdId;
        auto* cmd = FindCustomCommand(cmdId);
        if (cmd) {
            realCmdId = cmd->origId;
        }
        return CmdWorksWithoutDocument(realCmdId);
    }

    switch (cmdId) {
        case CmdOpenFile:
            // opening different files isn't allowed in plugin mode
            return !gPluginMode;

#ifndef DISABLE_DOCUMENT_RESTRICTIONS
        case CmdPrint:
            return !win->AsFixed() || win->AsFixed()->GetEngine()->AllowsPrinting();
#endif

        case CmdFindFirst:
            return NeedsFindUI(win) || IsBrowserDocController(win->ctrl);

        case CmdFindNext:
        case CmdFindPrev: {
            // Need non-empty find text (hwndFindEdit is the active bar or floating window edit).
            if (!win->hwndFindEdit || HwndGetTextLen(win->hwndFindEdit) == 0) {
                return false;
            }
            // When we already know there are zero matches, disable next/prev.
            // Unknown count (scan pending / not started) still allows searching.
            if (win->ctrl && win->ctrl->CanFindInPage()) {
                if (win->browserFindTotal == 0) {
                    return false;
                }
                return true;
            }
            if (win->findCountValid && len(win->findCountPositions) == 0) {
                return false;
            }
            return true;
        }

        case CmdGoToNextPage:
            return win->ctrl->CurrentPageNo() < win->ctrl->PageCount();
        case CmdGoToPrevPage:
            return win->ctrl->CurrentPageNo() > 1;

        case CmdNavigateBack:
            return win->ctrl->CanNavigate(-1);
        case CmdNavigateForward:
            return win->ctrl->CanNavigate(1);

        default:
            return true;
    }
}

static TempStr ToolbarTipTemp(int cmdId, Str tip, bool translate) {
    TempStr s = translate ? trans::GetTranslation(tip) : TempStr(tip);
    TempStr accelStr = AppendAccelKeyToMenuStringTemp(nullptr, cmdId);
    if (accelStr) {
        Str accel = accelStr.len > 1 ? Str(accelStr.s + 1, accelStr.len - 1) : accelStr;
        s = str::JoinTemp(s, fmt(" (%s)", accel));
    }
    return s;
}

void UpdateToolbarButtonsToolTipsForWindow(MainWindow* win) {
    ToolbarVirt* tb = win ? win->toolbarVirt : nullptr;
    if (!tb) {
        return;
    }
    for (int i = 0; i < gLayoutButtonsCount; i++) {
        const ToolbarButtonInfo& bi = gLayoutButtons[i];
        if (!bi.toolTip || bi.isText) {
            continue;
        }
        VirtCtrl* w = ToolbarItemAt(win, i);
        if (w) {
            w->SetTooltip(ToolbarTipTemp(bi.cmdId, bi.toolTip, true));
        }
    }
}

static void SetToolbarButtonImageByIdx(MainWindow* win, int idx, const char* icon) {
    VirtCtrl* w = ToolbarItemAt(win, idx);
    if (!w) {
        return;
    }
    auto* ib = (VirtIconButton*)w;
    ToolbarVirt* tb = win->toolbarVirt;
    int sz = tb ? tb->iconSize : DpiScale(gGlobalPrefs->toolbarSize);
    Pixmap* px = GetPixmapForIcon(icon, sz, sz, TbTextColor());
    Pixmap* pxOff = GetPixmapForIcon(icon, sz, sz, TbDisabledColor());
    if (ib->pixmap == px && ib->pixmapDisabled == pxOff) {
        return;
    }
    ib->pixmap = px;
    ib->pixmapDisabled = pxOff;
    ib->Invalidate();
}

static void SetToolbarButtonToolTipByIdx(MainWindow* win, int idx, int cmdId, Str s) {
    VirtCtrl* w = ToolbarItemAt(win, idx);
    if (!w) {
        return;
    }
    w->SetTooltip(ToolbarTipTemp(cmdId, s, false));
}

// TODO: this is called too often
// TODO: also set checked state instead of calling SetToolbarButtonCheckedState() all over
void ToolbarUpdateStateForWindow(MainWindow* win, bool setButtonsVisibility) {
    int n = TotalButtonsCount();
    for (int i = 0; i < n; i++) {
        auto& tb = GetToolbarButtonInfoByIdx(i);
        int cmdId = tb.cmdId;
        // cmdId 0 is a separator; GetCommandVisibility treats 0 as Hide, but
        // the old Win32 toolbar still painted the etched line (TBSTATE_HIDDEN
        // on a BTNS_SEP is a no-op / all-seps share id 0).
        if (setButtonsVisibility && cmdId != WarningMsgId && cmdId != 0) {
            bool hide = !IsCmdAvailable(win, cmdId);
            UpdateToolbarButtonStateByIdx(win, i, hide, TBSTATE_HIDDEN);
        }
        if (SkipBuiltInButton(tb)) {
            continue;
        }
        bool isEnabled = IsCmdEnabled(win, cmdId);
        UpdateToolbarButtonStateByIdx(win, i, isEnabled, TBSTATE_ENABLED);

        if (cmdId == CmdReadAloud || cmdId == CmdPauseReadAloud) {
            bool speaking = TtsIsSpeaking();
            SetToolbarButtonImageByIdx(win, i, speaking ? gIconPauseSpeaking : gIconSpeak);
            // tooltip reflects what clicking the button will do
            Str tip = _TRA("Read Aloud");
            if (speaking) {
                tip = _TRA("Pause Reading");
            } else if (CanContinueReadAloud(win->CurrentTab())) {
                tip = _TRA("Continue Reading");
            }
            SetToolbarButtonToolTipByIdx(win, i, cmdId, tip);
        }
    }

    if (setButtonsVisibility) {
        // drop a separator that would sit next to another, or at either end
        // (Read Aloud is hidden by default, which would otherwise leave ||)
        bool prevVisibleNonSep = false;
        int lastSep = -1;
        for (int i = 0; i < n; i++) {
            const ToolbarButtonInfo& bi = GetToolbarButtonInfoByIdx(i);
            VirtCtrl* w = ToolbarItemAt(win, i);
            if (!w) {
                continue;
            }
            if (bi.cmdId == 0) {
                bool hide = !prevVisibleNonSep;
                UpdateToolbarButtonStateByIdx(win, i, hide, TBSTATE_HIDDEN);
                prevVisibleNonSep = false;
                if (!hide) {
                    lastSep = i;
                }
                continue;
            }
            if (w->GetVisibility() == Visibility::Visible && bi.cmdId != PageInfoId) {
                prevVisibleNonSep = true;
                lastSep = -1;
            }
        }
        if (lastSep >= 0) {
            UpdateToolbarButtonStateByIdx(win, lastSep, true, TBSTATE_HIDDEN);
        }
    }

    // reposition the floating find bar over the search icon (and hide it if the
    // current document doesn't support find) when toolbar buttons change
    if (setButtonsVisibility) {
        UpdateToolbarFindText(win);
    }

    // update dirty (unsaved annotations) flag and tooltip on each tab
    if (win->tabsCtrl) {
        int nTabs = win->TabCount();
        for (int i = 0; i < nTabs; i++) {
            WindowTab* tab = win->GetTab(i);
            bool dirty = false;
            if (tab && tab->AsFixed()) {
                dirty = EngineHasUnsavedAnnotations(tab->AsFixed()->GetEngine());
            }
            // update tooltip before SetTabDirty (which rebuilds tooltips via LayoutTabs).
            // Must use MakeTabTooltipTemp (path+size); path-only overwrote size here.
            TabInfo* ti = win->tabsCtrl->GetTab(i);
            if (ti && tab && tab->filePath) {
                TempStr tooltip = MakeTabTooltipTemp(tab->filePath, dirty);
                str::ReplaceWithCopy(&ti->tooltip, tooltip);
            }
            win->tabsCtrl->SetTabDirty(i, dirty);
        }
    }
}

void SetToolbarButtonEnableState(MainWindow* win, int cmdId, bool isEnabled) {
    int buttons[4];
    int n = GetToolbarButtonsByID(cmdId, buttons);
    if (n == 0) return;
    for (int i = 0; i < n; i++) {
        int idx = buttons[i];
        UpdateToolbarButtonStateByIdx(win, idx, isEnabled, TBSTATE_ENABLED);
    }
}
// whether the current window context (presentation, about page) permits a
// toolbar at all, independent of the show/hide/overlay mode
static bool ToolbarContextAllows(MainWindow* win) {
    if (win->presentation) {
        return false;
    }
    return true;
}

// toolbar mode for this window: Fullscreen.Toolbar in fullscreen, else Toolbar
static int ToolbarModeForWindow(MainWindow* win) {
    if (win->isFullScreen) {
        return FullscreenToolbarModeFromPrefs();
    }
    return ToolbarModeFromPrefs();
}

bool ShouldShowToolbar(MainWindow* win) {
    if (!ToolbarContextAllows(win)) {
        return false;
    }
    int mode = ToolbarModeForWindow(win);
    return mode == kToolbarShow;
}

bool ShouldOverlayToolbar(MainWindow* win) {
    if (!ToolbarContextAllows(win)) {
        return false;
    }
    if (ToolbarModeForWindow(win) != kToolbarOverlay) {
        return false;
    }
    // don't float the overlay toolbar over the home / about page (only the
    // pinned "show" mode shows a toolbar there)
    if (win->IsCurrentTabAbout()) {
        return false;
    }
    return true;
}

// natural width of the toolbar content (buttons + page box); the find bar
// floats separately so the page-total label is the rightmost element
static int ToolbarNaturalWidth(MainWindow* win) {
    ToolbarVirt* tb = win->toolbarVirt;
    if (!tb || !tb->layout) {
        return 0;
    }
    Size sz = tb->layout->MinIntrinsicWidth(tb->rowDy) > 0 ? Size{tb->layout->MinIntrinsicWidth(tb->rowDy), tb->rowDy}
                                                           : Size{tb->rowDy * 8, tb->rowDy};
    return sz.dx + DpiScale(12);
}

// canvas rectangle in frame-client coordinates
static Rect CanvasRectInFrame(MainWindow* win) {
    Rect rc = HwndWindowRect(win->hwndCanvas);
    Point tl = HwndScreenToClient(win->hwndFrame, rc.TL());
    return {tl, rc.Size()};
}

// when the overlay toolbar sits at the bottom, lift it above the horizontal
// scrollbar so it doesn't cover it. The height is reserved even when the
// scrollbar isn't currently visible, so the toolbar's position is stable.
static int OverlayToolbarBottomScrollbarOffset(MainWindow* win) {
    if (ScrollbarsAreHidden()) {
        return 0;
    }
    if (ScrollbarsUseOverlay()) {
        // smart/overlay: the thick overlay scrollbar height (see OverlayScrollbarCreate)
        return DpiScale(16);
    }
    // windows native horizontal scrollbar
    return DpiGetSystemMetrics(SM_CYHSCROLL);
}

// rectangle (frame-client coords) the overlay toolbar occupies when shown
static Rect OverlayToolbarRect(MainWindow* win) {
    Rect canvas = CanvasRectInFrame(win);
    int natW = ToolbarNaturalWidth(win);
    if (natW <= 0 || natW > canvas.dx) {
        natW = canvas.dx;
    }
    int h = HwndWindowRect(win->hwndReBar).dy;
    int x = canvas.x + ((canvas.dx - natW) / 2);
    int y = canvas.y;
    if (ToolbarAtBottom()) {
        y = canvas.y + canvas.dy - h - OverlayToolbarBottomScrollbarOffset(win);
    }
    return {x, y, natW, h};
}

// position/show the floating overlay toolbar; called on relayout and mouse move
void PositionOverlayToolbar(MainWindow* win) {
    if (!win->isToolbarOverlay || !win->hwndReBar) {
        return;
    }
    Rect r = OverlayToolbarRect(win);
    UINT flags = SWP_NOACTIVATE;
    flags |= win->toolbarOverlayShown ? SWP_SHOWWINDOW : SWP_HIDEWINDOW;
    SetWindowPos(win->hwndReBar, HWND_TOP, r.x, r.y, r.dx, r.dy, flags);
    if (!win->toolbarOverlayShown) {
        // repaint the canvas area the toolbar was covering
        HwndInvalidate(win->hwndCanvas);
        HwndInvalidateRect(win->hwndFrame, r, false);
    }
}

// whether the cursor is currently in the reveal band or over the toolbar
static bool OverlayToolbarShouldShowForCursor(MainWindow* win) {
    Point pt = GetCursorPosition();
    Point ptFrame = HwndScreenToClient(win->hwndFrame, pt);

    Rect tb = OverlayToolbarRect(win);
    // reveal band: spans the full canvas width so the toolbar also appears when
    // the mouse is to the left or right of it, and extends a bit past the
    // toolbar (toward the page) so it shows before the cursor reaches it
    Rect canvas = CanvasRectInFrame(win);
    int my = DpiScale(16);
    int bandY = ToolbarAtBottom() ? (tb.y - my) : tb.y;
    Rect band(canvas.x, bandY, canvas.dx, tb.dy + my);
    bool inBand = band.Contains(Point(ptFrame.x, ptFrame.y));

    // also keep shown while the cursor is over the toolbar window itself
    HWND hwndUnder = HwndWindowFromPoint(pt);
    bool overToolbar = hwndUnder && (hwndUnder == win->hwndReBar || hwndUnder == win->hwndToolbar ||
                                     IsChild(win->hwndReBar, hwndUnder));
    return inBand || overToolbar;
}

// the overlay toolbar must not vanish while it owns the keyboard focus (e.g.
// the user is typing a page number into the page box after Ctrl+G)
static bool OverlayToolbarHasFocus(MainWindow* win) {
    if (!win->hwndReBar) {
        return false;
    }
    HWND focus = GetFocus();
    if (!focus) {
        return false;
    }
    return focus == win->hwndReBar || focus == win->hwndToolbar || IsChild(win->hwndReBar, focus);
}

static void CancelOverlayHide(MainWindow* win) {
    if (win->toolbarOverlayHidePending) {
        KillTimer(win->hwndFrame, kHideOverlayToolbarTimerId);
        win->toolbarOverlayHidePending = false;
    }
}

static void ScheduleOverlayHide(MainWindow* win) {
    if (win->toolbarOverlayHidePending) {
        return; // already scheduled; don't keep pushing it out on every move
    }
    win->toolbarOverlayHidePending = true;
    SetTimer(win->hwndFrame, kHideOverlayToolbarTimerId, kDelayToolbarHide, nullptr);
}

static void SetOverlayShown(MainWindow* win, bool shown) {
    if (shown == win->toolbarOverlayShown) {
        return;
    }
    win->toolbarOverlayShown = shown;
    PositionOverlayToolbar(win);
}

// re-evaluate overlay toolbar visibility based on the cursor's screen position
void UpdateOverlayToolbarForMouse(MainWindow* win) {
    if (!win->isToolbarOverlay || !win->hwndReBar) {
        return;
    }
    bool show = OverlayToolbarShouldShowForCursor(win) || OverlayToolbarHasFocus(win);
    if (show) {
        CancelOverlayHide(win);
        SetOverlayShown(win, true);
    } else if (win->toolbarOverlayShown) {
        // don't hide immediately; give the user kDelayToolbarHide to come back
        ScheduleOverlayHide(win);
    }
}

// reveal the overlay toolbar right now, without waiting for the cursor to enter
// the reveal band. Used by commands that drive the toolbar from the keyboard
// (Ctrl+G): the toolbar stays up while it has the focus and auto-hides once the
// focus and the cursor are away from it.
void RevealOverlayToolbar(MainWindow* win) {
    if (!win->isToolbarOverlay || !win->hwndReBar) {
        return;
    }
    CancelOverlayHide(win);
    SetOverlayShown(win, true);
}

// handle the delayed-hide timer firing (kHideOverlayToolbarTimerId)
void OverlayToolbarHideTimerFired(MainWindow* win) {
    win->toolbarOverlayHidePending = false;
    KillTimer(win->hwndFrame, kHideOverlayToolbarTimerId);
    if (!win->isToolbarOverlay) {
        return;
    }
    // if the cursor came back near the top while the timer was pending, keep
    // the toolbar shown; otherwise hide it now
    if (OverlayToolbarShouldShowForCursor(win) || OverlayToolbarHasFocus(win)) {
        SetOverlayShown(win, true);
    } else {
        SetOverlayShown(win, false);
    }
}

void ShowOrHideToolbar(MainWindow* win) {
    bool show = ShouldShowToolbar(win);
    bool overlay = ShouldOverlayToolbar(win);
    if (show == win->isToolbarVisible && overlay == win->isToolbarOverlay) {
        return;
    }
    bool enteredOverlay = overlay && !win->isToolbarOverlay;
    win->isToolbarVisible = show;
    win->isToolbarOverlay = overlay;
    if (!overlay) {
        CancelOverlayHide(win);
        win->toolbarOverlayShown = false;
    }
    if (enteredOverlay) {
        // reveal immediately on entering overlay mode (e.g. via F8) so the
        // change is visible; it auto-hides after kDelayToolbarHide
        win->toolbarOverlayShown = true;
    }
    if (!show && !overlay) {
        // Move the focus out of the toolbar
        if (HwndIsFocused(win->hwndFindEdit) || HwndIsFocused(win->hwndPageEdit)) {
            HwndSetFocus(win->hwndFrame);
        }
    }
    ScheduleUiUpdate(win);
    if (enteredOverlay) {
        ScheduleOverlayHide(win);
    }
}

void UpdateFindbox(MainWindow* win) {
    HwndInvalidate(win->hwndToolbar, true);
    if (HwndIsVisible(win->hwndFrame)) {
        UpdateWindow(win->hwndToolbar);
    }

    auto* cursorId = win->IsDocLoaded() ? IDC_IBEAM : IDC_ARROW;
    if (win->hwndFindEdit) {
        SetClassLongPtrW(win->hwndFindEdit, GCLP_HCURSOR, (LONG_PTR)GetCachedCursor(cursorId));
    }
}

#if 0
static LRESULT CALLBACK ReBarWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass,
                                     DWORD_PTR /*dwRefData*/) {
    if (WM_ERASEBKGND == uMsg && ThemeColorizeControls()) {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, ThemeWindowTextColor());
        Color bgCol = ThemeControlBackgroundColor();
        SetBkColor(hdc, bgCol);
        auto* bgBrush = CreateSolidBrush(bgCol);
        HdcFillRect(hdc, HwndClientRect(hWnd), bgBrush);
        DeleteObject(bgBrush);
        return 1;
    }
    if (WM_NOTIFY == uMsg) {
        auto* win = FindMainWindowByHwnd(hWnd);
        NMHDR* hdr = (NMHDR*)lParam;
        HWND chwnd = hdr->hwndFrom;
        if (hdr->code == NM_CUSTOMDRAW) {
            if (win && win->hwndToolbar == chwnd) {
                NMTBCUSTOMDRAW* custDraw = (NMTBCUSTOMDRAW*)hdr;
                switch (custDraw->nmcd.dwDrawStage) {
                    case CDDS_PREPAINT:
                        return CDRF_NOTIFYITEMDRAW;

                    case CDDS_ITEMPREPAINT: {
                        auto col = ThemeWindowTextColor();
                        UINT itemState = custDraw->nmcd.uItemState;
                        if (itemState & (CDIS_DISABLED | CDIS_GRAYED)) {
                            col = ThemeWindowTextDisabledColor();
                        }
                        // Toolbar honors text color from the DC (and clrText) when
                        // CDRF_NEWFONT is returned; setting only clrText is ignored
                        // under some common-control versions / themes.
                        custDraw->clrText = col;
                        SetTextColor(custDraw->nmcd.hdc, col);
                        return CDRF_NEWFONT;
                    }
                }
            }
        }
    }
    // allow window dragging from empty rebar area (main toolbar)
    if (WM_LBUTTONDOWN == uMsg) {
        auto* win = FindMainWindowByHwnd(hWnd);
        if (win && win->tabsInTitlebar) {
            HWND hwndFrame = GetAncestor(hWnd, GA_ROOT);
            ReleaseCapture();
            SendMessageW(hwndFrame, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            return 0;
        }
    }
    if (WM_LBUTTONDBLCLK == uMsg) {
        auto* win = FindMainWindowByHwnd(hWnd);
        if (win && win->tabsInTitlebar) {
            HWND hwndFrame = GetAncestor(hWnd, GA_ROOT);
            WPARAM cmd = IsZoomed(hwndFrame) ? SC_RESTORE : SC_MAXIMIZE;
            PostMessageW(hwndFrame, WM_SYSCOMMAND, cmd, 0);
            return 0;
        }
    }
    // keep the overlay toolbar visible while the mouse is over it, and re-evaluate
    // (likely hiding it) once the mouse leaves
    if (WM_MOUSEMOVE == uMsg || WM_MOUSELEAVE == uMsg) {
        auto* win = FindMainWindowByHwnd(hWnd);
        if (win && win->isToolbarOverlay) {
            if (WM_MOUSEMOVE == uMsg) {
                TRACKMOUSEEVENT tme{sizeof(TRACKMOUSEEVENT), TME_LEAVE, hWnd, 0};
                TrackMouseEvent(&tme);
            }
            UpdateOverlayToolbarForMouse(win);
        }
    }
    if (WM_NCDESTROY == uMsg) {
        RemoveWindowSubclass(hWnd, ReBarWndProc, uIdSubclass);
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

static WNDPROC DefWndProcEditBg = nullptr;
static LRESULT CALLBACK WndProcEditBg(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    LRESULT res = CallWindowProc(DefWndProcEditBg, hwnd, msg, wp, lp);
    if (msg == WM_PAINT) {
        HDC hdc = GetDC(hwnd);
        RECT rc = ToRECT(HwndClientRect(hwnd));
        Color bgCol2 = ThemeControlBackgroundColor();
        Color col = AccentColor(bgCol2, 40);
        HBRUSH br = CreateSolidBrush(col);
        FrameRect(hdc, &rc, br);
        DeleteObject(br);
        ReleaseDC(hwnd, hdc);
    }
    return res;
}

static WNDPROC DefWndProcToolbar = nullptr;
static LRESULT CALLBACK WndProcToolbar(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (WM_CTLCOLORSTATIC == msg || WM_CTLCOLOREDIT == msg) {
        HWND hwndCtrl = (HWND)lp;
        HDC hdc = (HDC)wp;
        MainWindow* win = FindMainWindowByHwnd(hwndCtrl);
        if (!win) {
            return CallWindowProc(DefWndProcToolbar, hwnd, msg, wp, lp);
        }
        {
            bool isBgCtrl = (win->hwndPageBg == hwndCtrl);
            bool isEditCtrl = (win->hwndPageEdit == hwndCtrl);
            SetTextColor(hdc, ThemeWindowTextColor());
            SetBkMode(hdc, TRANSPARENT);
            // the page box is a white field on the light theme's gray toolbar.
            // In high contrast the system palette decides what a field looks
            // like, and a white one would put white text on white (#2124)
            if ((isBgCtrl || isEditCtrl) && !ThemeColorizeControls() && !ThemeUsesHighContrastColors()) {
                SetBkColor(hdc, kColWhite);
                return (LRESULT)GetStockObject(WHITE_BRUSH);
            }
            return (LRESULT)win->brControlBgColor;
        }
    }

    // allow window dragging from empty toolbar areas
    if (WM_LBUTTONDOWN == msg || WM_LBUTTONDBLCLK == msg) {
        MainWindow* win = FindMainWindowByHwnd(hwnd);
        if (win && win->tabsInTitlebar) {
            Point pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            int idx = TbHitTest(hwnd, pt);
            if (idx < 0) {
                // also check we're not over a child control (find box, page box)
                HWND childAtPoint = ChildWindowFromPoint(hwnd, ToPOINT(pt));
                if (!childAtPoint || childAtPoint == hwnd) {
                    HWND hwndFrame = GetAncestor(hwnd, GA_ROOT);
                    if (WM_LBUTTONDBLCLK == msg) {
                        WPARAM cmd = IsZoomed(hwndFrame) ? SC_RESTORE : SC_MAXIMIZE;
                        PostMessageW(hwndFrame, WM_SYSCOMMAND, cmd, 0);
                    } else {
                        ReleaseCapture();
                        SendMessageW(hwndFrame, WM_NCLBUTTONDOWN, HTCAPTION, 0);
                    }
                    return 0;
                }
            }
        }
    }

    return CallWindowProc(DefWndProcToolbar, hwnd, msg, wp, lp);
}
#endif

// the find UI is now a floating Chrome-style bar (see FindBar.cpp). When the
// toolbar moves/resizes we keep the bar centered over the search icon.
void UpdateToolbarFindText(MainWindow* win) {
    FindBarReposition(win);
}

void UpdateToolbarState(MainWindow* win) {
    if (!win->IsDocLoaded()) {
        return;
    }
    DisplayMode dm = win->ctrl->GetDisplayMode();
    float zoomVirtual = win->ctrl->GetZoomVirtual();
    {
        bool isChecked = dm == DisplayMode::Continuous && zoomVirtual == kZoomFitWidth;
        SetToolbarButtonCheckedState(win, CmdZoomFitWidthAndContinuous, isChecked);
    }
    {
        bool isChecked = dm == DisplayMode::SinglePage && zoomVirtual == kZoomFitPage;
        SetToolbarButtonCheckedState(win, CmdZoomFitPageAndSinglePage, isChecked);
        if (!isChecked) {
            win->CurrentTab()->prevZoomVirtual = kInvalidZoom;
        }
    }
}

// (old Win32 toolbar subclass removed; WndProcVirtToolbar handles this)

static WNDPROC DefWndProcPageBox = nullptr;
static LRESULT CALLBACK WndProcPageBox(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    MainWindow* win = FindMainWindowByHwnd(hwnd);
    if (!win || !win->IsDocLoaded()) {
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    // NOLINTNEXTLINE(bugprone-branch-clone): both empty branches are handled elsewhere, for different reasons
    if (ExtendedEditWndProc(hwnd, msg, wp, lp)) {
        // select the whole page box on a non-selecting click
    } else if (WM_CHAR == msg) {
        switch (wp) {
            case VK_RETURN: {
                TempStr s = HwndGetTextTemp(win->hwndPageEdit);
                int newPageNo = win->ctrl->GetPageByLabel(s);
                if (win->ctrl->ValidPageNo(newPageNo)) {
                    win->ctrl->GoToPage(newPageNo, true);
                    HwndSetFocus(win->hwndFrame);
                    // the overlay toolbar was kept up by the focus; now that
                    // it's gone, let it hide again
                    UpdateOverlayToolbarForMouse(win);
                }
                return 1;
            }
            case VK_ESCAPE:
                HwndSetFocus(win->hwndFrame);
                UpdateOverlayToolbarForMouse(win);
                return 1;

            case VK_TAB:
                AdvanceFocus(win);
                return 1;
        }
    } else if (WM_ERASEBKGND == msg) {
        RECT r;
        Edit_GetRect(hwnd, &r);
        if (r.left == 0 && r.top == 0) { // virgin box
            r.left += 4;
            r.top += 3;
            r.bottom += 3;
            r.right -= 2;
            Edit_SetRectNoPaint(hwnd, &r);
        }
    } else if (WM_KEYDOWN == msg) {
        // TODO: see WndProcEditSearch for note on enabling accelerators here as well
    }

    return CallWindowProc(DefWndProcPageBox, hwnd, msg, wp, lp);
}

void UpdateToolbarPageText(MainWindow* win, int pageCount, bool updateOnly) {
    if (!win->hwndToolbar || !gLayoutHasPageBox) {
        return;
    }
    ToolbarVirt* tb = win->toolbarVirt;
    if (!tb || !tb->pageTotal) {
        return;
    }
    if (tb->pageLabel) {
        tb->pageLabel->SetText(_TRA("Page:"));
    }
    TempStr txt = nullptr;
    if (-1 == pageCount || !pageCount) {
        txt = " ";
    } else if (!win->ctrl || !win->ctrl->HasPageLabels()) {
        txt = fmt(" / %d", pageCount);
    } else {
        txt = fmt("%d / %d", win->ctrl->CurrentPageNo(), pageCount);
    }
    if (updateOnly && tb->pageTotal->s && txt && str::Eq(tb->pageTotal->s, txt)) {
        return;
    }
    tb->pageTotal->SetText(txt);
    RelayoutToolbar(win);
    HwndInvalidate(win->hwndToolbar, true);
}

static void CreatePageBox(MainWindow* win, HFONT font, int iconDy) {
    if (!gLayoutHasPageBox) {
        return;
    }
    bool isRtl = IsUIRtl();
    int boxWidth = HwndMeasureText(win->hwndFrame, "999999", font).dx;
    boxWidth += 2 * DpiGetSystemMetrics(SM_CXEDGE);
    boxWidth += DpiScale(12);
    // no WS_EX_CLIENTEDGE: a themed edit draws a blue bottom accent (Win11).
    // The old toolbar used a borderless edit sitting on a static that we
    // framed ourselves with a 1px gray line.
    DWORD style = WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL | ES_NUMBER | ES_RIGHT | WS_TABSTOP;
    DWORD exStyle = 0;
    if (isRtl) {
        exStyle |= WS_EX_LAYOUTRTL;
    }
    HWND page = CreateWindowExW(exStyle, WC_EDIT, L"0", style, 0, 1, boxWidth, iconDy, win->hwndToolbar,
                                (HMENU) nullptr, GetModuleHandle(nullptr), nullptr);
    SetWindowTheme(page, L"", L"");
    SetWindowFont(page, font, FALSE);
    if (!DefWndProcPageBox) {
        DefWndProcPageBox = (WNDPROC)GetWindowLongPtr(page, GWLP_WNDPROC);
    }
    SetWindowLongPtr(page, GWLP_WNDPROC, (LONG_PTR)WndProcPageBox);
    win->hwndPageEdit = page;
}

__unused static void LogBitmapInfo(HBITMAP hbmp) {
    BITMAP bmpInfo;
    GetObject(hbmp, sizeof(BITMAP), &bmpInfo);
    logf("dx: %d, dy: %d, stride: %d, bitsPerPixel: %d\n", (int)bmpInfo.bmWidth, (int)bmpInfo.bmHeight,
         (int)bmpInfo.bmWidthBytes, (int)bmpInfo.bmBitsPixel);
    u8* bits = (u8*)bmpInfo.bmBits;
    u8* d;
    for (int y = 0; y < 5; y++) {
        d = bits + ((size_t)bmpInfo.bmWidthBytes * y);
        logf("y: %d, d: 0x%p\n", y, d);
    }
}

static TempStr ShortcutToolbarToolTipTemp(Shortcut* shortcut) {
    if (!str::IsEmptyOrWhiteSpace(shortcut->name)) {
        return shortcut->name;
    }
    CustomCommand* cmd = FindCustomCommand(shortcut->cmdId);
    if (cmd && cmd->name) {
        return cmd->name;
    }
    int origId = cmd ? cmd->origId : shortcut->cmdId;
    if (origId > 0 && origId < CmdLast) {
        Str desc = SeqStrByIndex(gCommandDescriptions, origId);
        if (desc) {
            return desc;
        }
    }
    return shortcut->cmd;
}

static TempStr CustomCommandToolbarToolTipTemp(CustomCommand* cmd, Str fallback) {
    if (cmd && !str::IsEmptyOrWhiteSpace(cmd->name)) {
        return cmd->name;
    }
    if (!str::IsEmptyOrWhiteSpace(fallback)) {
        return fallback;
    }
    return "External Viewer";
}

static void PopulateCustomToolbarButtons() {
    gCustomButtonsCount = 0;
    for (Shortcut* shortcut : *gGlobalPrefs->shortcuts) {
        if (gCustomButtonsCount >= kMaxCustomButtons) {
            break;
        }
        if (!str::IsEmptyOrWhiteSpace(shortcut->toolbarSvgIcon)) {
            ToolbarButtonInfo tbi;
            tbi.cmdId = shortcut->cmdId;
            tbi.svgIcon = shortcut->toolbarSvgIcon;
            tbi.toolTip = ShortcutToolbarToolTipTemp(shortcut);
            gCustomButtons[gCustomButtonsCount++] = tbi;
            continue;
        }
        if (!str::IsEmptyOrWhiteSpace(shortcut->toolbarText)) {
            ToolbarButtonInfo tbi;
            tbi.cmdId = shortcut->cmdId;
            tbi.toolTip = shortcut->toolbarText;
            tbi.isText = true;
            gCustomButtons[gCustomButtonsCount++] = tbi;
        }
    }

    // add toolbar buttons from custom commands with toolbar settings (e.g. ExternalViewers).
    // gFirstCustomCommand is a prepend-only list, so walking it directly yields
    // the commands in reverse creation order and the buttons would show up in
    // the reverse of the order the user listed them in (#5869)
    Vec<CustomCommand*> customCmds;
    for (auto* cc = gFirstCustomCommand; cc; cc = cc->next) {
        customCmds.Append(cc);
    }
    VecReverse(customCmds);
    for (CustomCommand* cc : customCmds) {
        if (gCustomButtonsCount >= kMaxCustomButtons) {
            break;
        }
        Str svgIcon = GetCommandStringArg(cc, kCmdArgToolbarSvgIcon, nullptr);
        Str tbText = GetCommandStringArg(cc, kCmdArgToolbarText, nullptr);
        if (!str::IsEmptyOrWhiteSpace(svgIcon)) {
            ToolbarButtonInfo tbi;
            tbi.cmdId = cc->id;
            tbi.svgIcon = svgIcon;
            tbi.toolTip = CustomCommandToolbarToolTipTemp(cc, tbText);
            gCustomButtons[gCustomButtonsCount++] = tbi;
            continue;
        }
        if (str::IsEmptyOrWhiteSpace(tbText)) {
            continue;
        }
        ToolbarButtonInfo tbi;
        tbi.cmdId = cc->id;
        tbi.toolTip = tbText;
        tbi.isText = true;
        gCustomButtons[gCustomButtonsCount++] = tbi;
    }
}

// Returns nullptr if mupdf can't render svgData; the caller then leaves that
// icon's part of the bitmap empty.
//
// A custom ToolbarSvgIcon comes from the settings file, so it can be malformed:
// a typo, or the file caught half-written by the settings watcher while the user
// is editing it. mupdf signals that by throwing, and an uncaught mupdf exception
// aborts the whole process, so everything here has to be inside fz_try.
static fz_pixmap* RenderSvgIconPixmap(fz_context* ctx, Str svgData, int dx, int dy, Color fgCol, Color bgCol) {
    TempStr strokeCol = SerializeColorTemp(fgCol);
    TempStr fillCol = SerializeColorTemp(bgCol);
    TempStr fillColRepl = str::JoinTemp(StrL("fill=\""), fillCol, StrL("\""));
    TempStr svg = str::ReplaceTemp(svgData, StrL("currentColor"), strokeCol);
    svg = str::ReplaceTemp(svg, StrL(R"(fill="none")"), fillColRepl);

    fz_buffer* buf = nullptr;
    fz_image* image = nullptr;
    fz_pixmap* pixmap = nullptr;
    fz_var(buf);
    fz_var(image);
    fz_var(pixmap);
    fz_try(ctx) {
        buf = fz_new_buffer_from_copied_data(ctx, (u8*)svg.s, svg.len);
        image = fz_new_image_from_svg(ctx, buf, nullptr, nullptr);
        image->w = dx;
        image->h = dy;
        pixmap = fz_get_pixmap_from_image(ctx, image, nullptr, nullptr, nullptr, nullptr);
    }
    fz_always(ctx) {
        fz_drop_image(ctx, image);
        fz_drop_buffer(ctx, buf);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        logf("RenderSvgIconPixmap: rendering svg icon failed with: '%s'\n", Str(fz_caught_message(ctx)));
        return nullptr;
    }
    return pixmap;
}

static void BlitPixmap(u8* dstSamples, ptrdiff_t dstStride, fz_pixmap* src, int dstX, int dstY, Color bgCol) {
    int dx = src->w;
    int dy = src->h;
    int srcN = src->n;
    int dstN = 4;
    auto srcStride = src->stride;
    u8 r, g, b;
    UnpackColor(bgCol, r, g, b);
    for (size_t y = 0; y < (size_t)dy; y++) {
        u8* s = src->samples + (srcStride * y);
        size_t atY = y + (size_t)dstY;
        u8* d = dstSamples + (dstStride * atY) + ((size_t)dstX * dstN);
        for (int x = 0; x < dx; x++) {
            bool isTransparent = (s[0] == r) && (s[1] == g) && (s[2] == b);
            // note: we're swapping red and green channel because src is rgb
            // and we want bgr for Toolbar's IMAGELIST
            d[0] = s[2];
            d[1] = s[1];
            d[2] = s[0];
            if (isTransparent) {
                d[3] = 0;
            } else {
                d[3] = 0xff;
            }
            d += dstN;
            s += srcN;
        }
    }
}

// leaves an icon-sized hole in the bitmap: background color, fully transparent.
// Used for an icon we couldn't render, so the button shows up empty instead of
// as a black square (the zero-filled DIB).
static void ClearIconSlot(u8* dstSamples, ptrdiff_t dstStride, int dx, int dy, int dstX, Color bgCol) {
    u8 r, g, b;
    UnpackColor(bgCol, r, g, b);
    for (size_t y = 0; y < (size_t)dy; y++) {
        u8* d = dstSamples + (dstStride * y) + ((size_t)dstX * 4);
        for (int x = 0; x < dx; x++) {
            d[0] = b;
            d[1] = g;
            d[2] = r;
            d[3] = 0;
            d += 4;
        }
    }
}

// same rendering the toolbar's image list uses, into a standalone Pixmap.
// bgCol is what the icon will be drawn on: pixels that come out as that color
// are the svg's background and become transparent
Pixmap* RenderSvgIconToPixmap(Str svgData, int dx, int dy, Color fgCol, Color bgCol) {
    if (str::IsEmptyOrWhiteSpace(svgData) || dx <= 0 || dy <= 0) {
        return nullptr;
    }
    fz_context* ctx = fz_new_context_windows();
    fz_pixmap* pixmap = RenderSvgIconPixmap(ctx, svgData, dx, dy, fgCol, bgCol);
    Pixmap* res = nullptr;
    if (pixmap) {
        res = AllocPixmap(dx, dy);
        if (res) {
            BlitPixmap(res->data, res->stride, pixmap, 0, 0, bgCol);
        }
        fz_drop_pixmap(ctx, pixmap);
    }
    fz_drop_context_windows(ctx);
    return res;
}

static HBITMAP BuildIconsBitmap(int dx, int dy, Str* customSvgs, int customCount) {
    fz_context* ctx = fz_new_context_windows();
    int nBuiltIn = SvgIconsCount();
    int nIcons = nBuiltIn + customCount;
    int destDx = dx * nIcons;
    ptrdiff_t dstStride;

    u8* hbmpData = nullptr;
    HBITMAP hbmp;
    {
        int w = destDx;
        int h = dy;
        int n = 4;
        dstStride = (ptrdiff_t)destDx * n;
        int imgSize = (int)dstStride * h;
        int bitsCount = n * 8;

        int bmiSize = (int)(sizeof(BITMAPINFO) + (255 * sizeof(RGBQUAD)));
        auto* bmi = (BITMAPINFO*)AllocArrayTemp<u8>(bmiSize);
        BITMAPINFOHEADER* bmih = &bmi->bmiHeader;
        bmih->biSize = sizeof(*bmih);
        bmih->biWidth = w;
        bmih->biHeight = -h;
        bmih->biPlanes = 1;
        bmih->biCompression = BI_RGB;
        bmih->biBitCount = bitsCount;
        bmih->biSizeImage = imgSize;
        bmih->biClrUsed = 0;
        uint usage = DIB_RGB_COLORS;
        // no file mapping: nothing shares the section and the bitmap is
        // deleted right after ImageList_Add, so let CreateDIBSection
        // allocate (a mapping handle here was leaked)
        hbmp = CreateDIBSection(nullptr, bmi, usage, (void**)&hbmpData, nullptr, 0);
    }

    Color fgCol = ThemeWindowTextColor();
    Color bgCol = ThemeControlBackgroundColor();
    for (int i = 0; i < nBuiltIn; i++) {
        Str svgData = SvgIconAt(i);
        fz_pixmap* pixmap = RenderSvgIconPixmap(ctx, svgData, dx, dy, fgCol, bgCol);
        if (!pixmap) {
            ClearIconSlot(hbmpData, dstStride, dx, dy, dx * i, bgCol);
            continue;
        }
        BlitPixmap(hbmpData, dstStride, pixmap, dx * i, 0, bgCol);
        fz_drop_pixmap(ctx, pixmap);
    }
    for (int i = 0; i < customCount; i++) {
        fz_pixmap* pixmap = RenderSvgIconPixmap(ctx, customSvgs[i], dx, dy, fgCol, bgCol);
        if (!pixmap) {
            ClearIconSlot(hbmpData, dstStride, dx, dy, dx * (nBuiltIn + i), bgCol);
            continue;
        }
        BlitPixmap(hbmpData, dstStride, pixmap, dx * (nBuiltIn + i), 0, bgCol);
        fz_drop_pixmap(ctx, pixmap);
    }

    fz_drop_context_windows(ctx);
    return hbmp;
}

// One icon rendered into a Pixmap, with the pixels the VirtCtrl controls want:
// BGRA, alpha-premultiplied, transparent where the SVG left the background
// (which is what BlitPixmap() marks with alpha 0 for the image list)
static Pixmap* RenderIconPixmap(const char* svgData, int dx, int dy, Color fgCol) {
    if (!svgData || !*svgData) {
        return nullptr;
    }
    Pixmap* px = AllocPixmapDIB(dx, dy);
    if (!px) {
        return nullptr;
    }
    memset(px->data, 0, (size_t)px->stride * (size_t)dy);
    px->premultiplied = true;

    Color bgCol = ThemeControlBackgroundColor();
    fz_context* ctx = fz_new_context_windows();
    fz_pixmap* pixmap = RenderSvgIconPixmap(ctx, Str(svgData), dx, dy, fgCol, bgCol);
    if (pixmap) {
        BlitPixmap(px->data, px->stride, pixmap, 0, 0, bgCol);
        // BlitPixmap leaves the background color in the transparent pixels;
        // premultiplied alpha wants them at 0 or AlphaBlend paints a box of it
        u8* row = px->data;
        for (int y = 0; y < dy; y++) {
            u8* d = row;
            for (int x = 0; x < dx; x++) {
                if (d[3] == 0) {
                    d[0] = d[1] = d[2] = 0;
                }
                d += 4;
            }
            row += px->stride;
        }
        fz_drop_pixmap(ctx, pixmap);
    }
    fz_drop_context_windows(ctx);
    return px;
}

// A handful of icons are ever cached (a few sizes of a few icons), so an
// intrusive list walked linearly is as good as anything
struct CachedPixmapIcon {
    CachedPixmapIcon* next;
    const char* svg; // gIcon* pointer, not owned
    Color fg;
    // the size is the pixmap's own width / height
    Pixmap* pixmap; // owned
};

static CachedPixmapIcon* gIconPixmaps = nullptr;

Pixmap* GetPixmapForIcon(const char* svg, int dx, int dy, Color fg) {
    if (!svg || !*svg || dx <= 0 || dy <= 0) {
        return nullptr;
    }
    if (fg == kColorUnset) {
        fg = ThemeWindowTextColor();
    }
    for (CachedPixmapIcon* e = gIconPixmaps; e; e = e->next) {
        if (e->svg == svg && e->fg == fg && e->pixmap->width == dx && e->pixmap->height == dy) {
            return e->pixmap;
        }
    }
    Pixmap* px = RenderIconPixmap(svg, dx, dy, fg);
    if (!px) {
        return nullptr;
    }
    auto* e = new CachedPixmapIcon{gIconPixmaps, svg, fg, px};
    gIconPixmaps = e;
    return px;
}

// the icons are drawn in the theme's colors, so this is also what a theme
// change calls to drop them
void DestroyIconPixmaps() {
    CachedPixmapIcon* e = gIconPixmaps;
    gIconPixmaps = nullptr;
    while (e) {
        CachedPixmapIcon* next = e->next;
        FreePixmap(e->pixmap);
        delete e;
        e = next;
    }
}

static int SetToolbarIconsImageList(MainWindow* win) {
    // we call it ToolbarSize for users, but it's really size of the icon
    int iconSize = DpiScale(gGlobalPrefs->toolbarSize);
    iconSize = RoundUp(iconSize, 4);
    int dx = iconSize;

    Str customSvgs[kMaxCustomButtons];
    int customCount = 0;
    int nBuiltIn = SvgIconsCount();
    for (int i = 0; i < gCustomButtonsCount; i++) {
        Str svg = gCustomButtons[i].svgIcon;
        if (str::IsEmptyOrWhiteSpace(svg)) {
            continue;
        }
        customSvgs[customCount++] = svg;
    }

    HIMAGELIST himl = ImageList_Create(dx, dx, ILC_COLOR32, nBuiltIn + customCount, 0);
    HBITMAP hbmp = BuildIconsBitmap(dx, dx, customSvgs, customCount);
    ImageList_Add(himl, hbmp, nullptr);
    DeleteObject(hbmp);
    if (gTbHiml) {
        ImageList_Destroy(gTbHiml);
    }
    gTbHiml = himl;
    return iconSize;
}

static void ApplyToolbarItemColors(VirtCtrl* w) {
    Color hover = TbHoverColor();
    Color sel = TbSelectedColor();
    if (auto* ib = (w->GetKind() && str::Eq(w->GetKind(), "virtCtrlIconButton")) ? (VirtIconButton*)w : nullptr) {
        ib->bgColorHover = hover;
        ib->bgColorSelected = sel;
        ib->chevronColor = TbTextColor();
        return;
    }
    if (auto* b = (w->GetKind() && str::Eq(w->GetKind(), "virtCtrlButton")) ? (VirtButton*)w : nullptr) {
        b->bgColorHover = hover;
        b->textColor = TbTextColor();
        b->textColorDisabled = TbDisabledColor();
        return;
    }
    if (auto* t = (w->GetKind() && str::Eq(w->GetKind(), "virtCtrlText")) ? (VirtText*)w : nullptr) {
        t->textColor = TbTextColor();
        return;
    }
    if (auto* line = (w->GetKind() && str::Eq(w->GetKind(), "virtCtrlLine")) ? (VirtLine*)w : nullptr) {
        line->color = TbEdgeColor();
    }
}

static void RefreshToolbarIcons(MainWindow* win) {
    ToolbarVirt* tb = win->toolbarVirt;
    if (!tb) {
        return;
    }
    int sz = tb->iconSize;
    Color fg = TbTextColor();
    Color dis = TbDisabledColor();
    for (int i = 0; i < len(tb->items); i++) {
        VirtCtrl* w = tb->items[i];
        ApplyToolbarItemColors(w);
        if (!str::Eq(w->GetKind(), "virtCtrlIconButton")) {
            continue;
        }
        const ToolbarButtonInfo& bi = GetToolbarButtonInfoByIdx(i);
        if (SkipBuiltInButton(bi) && !bi.svgIcon) {
            continue;
        }
        auto* ib = (VirtIconButton*)w;
        if (bi.svgIcon) {
            ib->pixmap = RenderSvgIconToPixmap(bi.svgIcon, sz, sz, fg, TbBgColor());
            ib->pixmapDisabled = RenderSvgIconToPixmap(bi.svgIcon, sz, sz, dis, TbBgColor());
            if (ib->pixmap) {
                tb->ownedPixmaps.Append(ib->pixmap);
            }
            if (ib->pixmapDisabled) {
                tb->ownedPixmaps.Append(ib->pixmapDisabled);
            }
        } else if (bi.icon) {
            ib->pixmap = GetPixmapForIcon(bi.icon, sz, sz, fg);
            ib->pixmapDisabled = GetPixmapForIcon(bi.icon, sz, sz, dis);
        }
    }
    if (tb->pageLabel) {
        tb->pageLabel->textColor = TbTextColor();
    }
    if (tb->pageTotal) {
        tb->pageTotal->textColor = TbTextColor();
    }
}

void UpdateToolbarAfterThemeChange(MainWindow* win) {
    SetToolbarIconsImageList(win);
    DestroyIconPixmaps();
    RefreshToolbarIcons(win);
    HwndScheduleRepaint(win->hwndToolbar);
}

// screen-coordinates rect of a toolbar button, used to position the FindBar.
// returns an empty rect when the toolbar isn't visible (e.g. fullscreen /
// presentation) so the caller can fall back to a different anchor.
Rect GetToolbarButtonScreenRect(MainWindow* win, int cmdId) {
    if (!win->hwndToolbar || !HwndIsVisible(win->hwndToolbar) || !win->toolbarVirt) {
        return {};
    }
    for (VirtCtrl* w : win->toolbarVirt->items) {
        if (w && w->id == cmdId && w->GetVisibility() == Visibility::Visible) {
            Rect r = w->BoundsInWindow();
            return HwndMapRectToWindow(r, win->hwndToolbar, HWND_DESKTOP);
        }
    }
    return {};
}

// Dump of the toolbar's buttons for -dbg-control tests (tests/issue-5869.ts).
// One line per button: its command id, its rect and the string the toolbar
// shows as its tooltip. Also reports how many tools the toolbar's tooltip
// control ended up with: the toolbar registers one tool per button keyed by
// command id, so duplicate command ids silently collapse into one tooltip.
TempStr ToolbarButtonsResultTemp(int* exitCodeOut) {
    str::Builder out;
    MainWindow* win = len(gWindows) == 0 ? nullptr : gWindows[0];
    if (!win || !win->hwndToolbar) {
        *exitCodeOut = 1;
        out.Append("ERROR no-toolbar\n");
        return ToStrTemp(out);
    }
    ToolbarVirt* tb = win->toolbarVirt;
    int n = tb ? len(tb->items) : 0;
    int nTools = 0;
    for (int i = 0; i < n; i++) {
        if (tb->items[i] && tb->items[i]->tooltip) {
            nTools++;
        }
    }
    out.Append(fmt("buttons=%d tooltipTools=%d\n", n, nTools));
    int toolIdx = 0;
    for (int i = 0; i < n; i++) {
        VirtCtrl* w = tb->items[i];
        Rect r = w ? w->BoundsInWindow() : Rect{};
        bool hidden = !w || w->GetVisibility() != Visibility::Visible;
        // Match the old Win32 dump: TBIF_TEXT. Built-in buttons store the
        // tooltip (with accelerator); custom ones stored the raw name.
        Str text{};
        if (IsVirtKind(w, "virtCtrlButton")) {
            text = ((VirtButton*)w)->s;
        } else if (i >= gLayoutButtonsCount) {
            text = GetToolbarButtonInfoByIdx(i).toolTip;
        } else if (w && w->tooltip) {
            text = w->tooltip;
        }
        out.Append(fmt("idx=%d cmd=%d hidden=%d rect=%d,%d,%d,%d text=%s\n", i, w ? w->id : 0, hidden ? 1 : 0, r.x, r.y,
                       r.x + r.dx, r.y + r.dy, text));
        if (w && w->tooltip) {
            out.Append(fmt("tool=%d uid=%d rect=%d,%d,%d,%d\n", toolIdx, w->id, r.x, r.y, r.x + r.dx, r.y + r.dy));
            toolIdx++;
        }
    }
    *exitCodeOut = 0;
    return ToStrTemp(out);
}

static bool IsVirtKind(VirtCtrl* w, const char* k) {
    return w && w->GetKind() && str::Eq(w->GetKind(), k);
}

// TrackPopupMenu is modal: a click on the split button dismisses the menu and
// is then delivered as a new WM_LBUTTONDOWN, which would open it again.
static u64 gToolbarDropdownClosedAt = 0;

static bool ToolbarDropdownJustClosed() {
    return GetTickCount64() - gToolbarDropdownClosedAt < 200;
}

static bool PeekRemoveClickOnRect(HWND hwnd, UINT msgId, Rect btn) {
    MSG msg{};
    if (!PeekMessageW(&msg, hwnd, msgId, msgId, PM_NOREMOVE)) {
        return false;
    }
    Point pt = {GET_X_LPARAM(msg.lParam), GET_Y_LPARAM(msg.lParam)};
    if (!btn.Contains(pt)) {
        return false;
    }
    PeekMessageW(&msg, hwnd, msgId, msgId, PM_REMOVE);
    return true;
}

// After the popup returns, drop a pending click that landed on this button.
void ToolbarEatMenuDismissClick(MainWindow* win, int cmdId) {
    gToolbarDropdownClosedAt = GetTickCount64();
    if (!win || !win->hwndToolbar || !win->toolbarVirt) {
        return;
    }
    Rect btn{};
    for (VirtCtrl* w : win->toolbarVirt->items) {
        if (w && w->id == cmdId && w->GetVisibility() == Visibility::Visible) {
            btn = w->BoundsInWindow();
            break;
        }
    }
    if (btn.IsEmpty()) {
        return;
    }
    HWND hwnd = win->hwndToolbar;
    while (PeekRemoveClickOnRect(hwnd, WM_LBUTTONDOWN, btn) || PeekRemoveClickOnRect(hwnd, WM_LBUTTONDBLCLK, btn)) {
        MSG up{};
        PeekMessageW(&up, hwnd, WM_LBUTTONUP, WM_LBUTTONUP, PM_REMOVE);
    }
}

static void OnToolbarButtonClicked(MainWindow* win, VirtMouseEvent* ev) {
    VirtCtrl* w = ev->target;
    if (!w || !win || !w->IsEnabled()) {
        return;
    }
    int cmdId = w->id;
    if (cmdId == PageInfoId || cmdId == 0) {
        return;
    }
    if (ToolbarDropdownJustClosed() && (cmdId == CmdReadAloud || cmdId == CmdPauseReadAloud)) {
        ev->didHandle = true;
        return;
    }
    if (IsVirtKind(w, "virtCtrlIconButton")) {
        auto* ib = (VirtIconButton*)w;
        if (ib->hasDropdown) {
            int dropDx = ib->DropdownDx();
            if (dropDx > 0 && ev->pt.x >= w->bounds.dx - dropDx) {
                NMTOOLBARW nmtb{};
                nmtb.hdr.hwndFrom = win->hwndToolbar;
                nmtb.hdr.idFrom = IDC_TOOLBAR;
                nmtb.hdr.code = TBN_DROPDOWN;
                nmtb.iItem = cmdId;
                SendMessageW(win->hwndFrame, WM_NOTIFY, IDC_TOOLBAR, (LPARAM)&nmtb);
                ev->didHandle = true;
                return;
            }
        }
    }
    HwndPostCommand(win->hwndFrame, cmdId, 0);
    ev->didHandle = true;
}

static void PositionPageEdit(MainWindow* win) {
    ToolbarVirt* tb = win->toolbarVirt;
    if (!tb || !tb->pageEditSlot || !win->hwndPageEdit) {
        return;
    }
    Rect r = tb->pageEditSlot->BoundsInWindow();
    if (r.IsEmpty()) {
        return;
    }
    int pad = DpiScale(2);
    MoveWindow(win->hwndPageEdit, r.x + pad, r.y + pad, std::max(1, r.dx - (2 * pad)), std::max(1, r.dy - (2 * pad)),
               TRUE);
}

static void RelayoutToolbar(MainWindow* win) {
    ToolbarVirt* tb = win->toolbarVirt;
    if (!tb || !tb->layout || !win->hwndToolbar) {
        return;
    }
    Rect rc = HwndClientRect(win->hwndToolbar);
    if (rc.dx <= 0 || rc.dy <= 0) {
        return;
    }
    LayoutTreeToSize(win->hwndToolbar, tb->layout, rc.Size(), &tb->vroot);
    PositionPageEdit(win);
}

static void PaintToolbarSeparator(VirtCustom*, VirtPaintCtx* ctx) {
    Rect r = ctx->bounds;
    int inset = DpiScale(6);
    int dy = r.dy - (2 * inset);
    if (dy <= 0) {
        return;
    }
    int x = r.x + (r.dx / 2);
    ctx->gfx->FillRect({x, r.y + inset, 1, dy}, ThemeEdgeColor());
}

static VirtCtrl* MakeToolbarSeparator(int rowDy) {
    auto* sep = new VirtCustom();
    sep->idealSize = {DpiScale(8), rowDy};
    sep->onPaint = MkFunc1(PaintToolbarSeparator, sep);
    sep->SetFlag(vwfNoHitTest, true);
    return sep;
}

static void BuildToolbarLayout(MainWindow* win) {
    ToolbarVirt* tb = win->toolbarVirt;
    delete tb->layout;
    tb->layout = nullptr;
    tb->items.Reset();
    for (Pixmap* px : tb->ownedPixmaps) {
        FreePixmap(px);
    }
    tb->ownedPixmaps.Reset();
    tb->pageLabel = nullptr;
    tb->pageTotal = nullptr;
    tb->pageEditSlot = nullptr;

    int cyPad = ToolbarCyPad();
    int iconPad = DpiScale(6);
    tb->rowDy = ToolbarRowDy(tb->iconSize);
    Color fg = TbTextColor();
    Color dis = TbDisabledColor();
    Color hover = TbHoverColor();
    Color sel = TbSelectedColor();

    auto* box = new HBox();
    box->alignCross = CrossAxisAlign::CrossCenter;
    box->rtl = IsUIRtl();

    int n = TotalButtonsCount();
    for (int i = 0; i < n; i++) {
        const ToolbarButtonInfo& bi = GetToolbarButtonInfoByIdx(i);
        VirtCtrl* w = nullptr;
        bool noTranslate = i >= gLayoutButtonsCount;
        if (bi.cmdId == PageInfoId) {
            auto* label = new VirtText(_TRA("Page:"), tb->platformFont);
            label->textColor = fg;
            label->padding = {0, DpiScale(kTextPaddingRight), 0, DpiScale(4)};
            label->id = PageInfoId;
            tb->pageLabel = label;
            box->AddChild(label);

            int editDx = HwndMeasureText(win->hwndFrame, "999999", tb->font).dx;
            editDx += DpiScale(16);
            auto* slot = new VirtFill();
            slot->color = ThemeWindowControlBackgroundColor();
            slot->idealSize = {editDx, tb->iconSize + DpiScale(2)};
            slot->id = PageInfoId;
            tb->pageEditSlot = slot;
            box->AddChild(slot);

            auto* total = new VirtText(StrL(" "), tb->platformFont);
            total->textColor = fg;
            total->padding = {0, DpiScale(4), 0, DpiScale(4)};
            total->id = PageInfoId;
            tb->pageTotal = total;
            box->AddChild(total);
            tb->items.Append(label);
            continue;
        } else if (bi.cmdId == 0 || (SkipBuiltInButton(bi) && !bi.svgIcon && !bi.isText)) {
            w = MakeToolbarSeparator(tb->rowDy);
        } else if (bi.isText) {
            auto* b = new VirtButton(noTranslate ? bi.toolTip : trans::GetTranslation(bi.toolTip), tb->platformFont);
            b->textColor = fg;
            b->textColorDisabled = dis;
            b->bgColorHover = hover;
            b->textPadding = {cyPad, iconPad, cyPad, iconPad};
            w = b;
        } else {
            auto* ib = new VirtIconButton();
            ib->padding = {cyPad, iconPad, cyPad, iconPad};
            ib->bgColorHover = hover;
            ib->bgColorSelected = sel;
            ib->chevronColor = fg;
            ib->hasDropdown = (bi.cmdId == CmdReadAloud);
            if (bi.svgIcon) {
                ib->pixmap = RenderSvgIconToPixmap(bi.svgIcon, tb->iconSize, tb->iconSize, fg, TbBgColor());
                ib->pixmapDisabled = RenderSvgIconToPixmap(bi.svgIcon, tb->iconSize, tb->iconSize, dis, TbBgColor());
                if (ib->pixmap) {
                    tb->ownedPixmaps.Append(ib->pixmap);
                }
                if (ib->pixmapDisabled) {
                    tb->ownedPixmaps.Append(ib->pixmapDisabled);
                }
            } else {
                ib->pixmap = GetPixmapForIcon(bi.icon, tb->iconSize, tb->iconSize, fg);
                ib->pixmapDisabled = GetPixmapForIcon(bi.icon, tb->iconSize, tb->iconSize, dis);
            }
            w = ib;
        }
        w->id = bi.cmdId;
        if (bi.toolTip && !bi.isText) {
            w->SetTooltip(ToolbarTipTemp(bi.cmdId, bi.toolTip, !noTranslate));
        } else if (bi.isText && bi.toolTip) {
            w->SetTooltip(ToolbarTipTemp(bi.cmdId, bi.toolTip, false));
        }
        if (bi.cmdId != 0 && bi.cmdId != PageInfoId) {
            w->onClick = MkFunc1(OnToolbarButtonClicked, win);
        }
        tb->items.Append(w);
        box->AddChild(w);
    }

    tb->layout = new Padding(box, Insets{0, DpiScale(4), 0, DpiScale(4)});
    RelayoutToolbar(win);
}

static void FreeToolbarVirt(MainWindow* win) {
    ToolbarVirt* tb = win->toolbarVirt;
    if (!tb) {
        return;
    }
    delete tb->layout;
    delete tb->vroot;
    for (Pixmap* px : tb->ownedPixmaps) {
        FreePixmap(px);
    }
    delete tb;
    win->toolbarVirt = nullptr;
}

static LRESULT CALLBACK WndProcVirtToolbar(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    MainWindow* win = FindMainWindowByHwnd(hwnd);
    ToolbarVirt* tb = win ? win->toolbarVirt : nullptr;
    Color bg = TbBgColor();

    if (msg == WM_MOUSEACTIVATE) {
        // the old Win32 toolbar did not take keyboard focus; a generic child
        // would, and then accelerators (Ctrl+W, …) never reached the frame
        HWND frame = GetAncestor(hwnd, GA_ROOT);
        if (frame && GetForegroundWindow() == frame) {
            return MA_NOACTIVATE;
        }
    }
    if (msg == WM_SIZE) {
        if (tb) {
            RelayoutToolbar(win);
        }
        return 0;
    }
    if (msg == WM_ERASEBKGND) {
        return 1;
    }
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        Rect rc = HwndClientRect(hwnd);
        DoubleBuffer buffer(hwnd, rc);
        HDC memDC = buffer.GetDC();
        HdcFillRect(memDC, rc, bg);
        if (tb && tb->vroot) {
            SetBkMode(memDC, TRANSPARENT);
            GfxHdc gfx(memDC);
            tb->vroot->Paint(&gfx, rc);
        }
        if (tb && tb->pageEditSlot && tb->pageEditSlot->GetVisibility() == Visibility::Visible) {
            Rect r = tb->pageEditSlot->BoundsInWindow();
            if (!r.IsEmpty()) {
                Color col = AccentColor(TbBgColor(), 40);
                HBRUSH br = CreateSolidBrush(col);
                RECT wr = ToRECT(r);
                FrameRect(memDC, &wr, br);
                DeleteObject(br);
            }
        }
        if (IsCurrentThemeDefault() && !ThemeColorizeControls()) {
            HdcFillRect(memDC, {rc.x, rc.Bottom() - 1, rc.dx, 1}, TbEdgeColor());
        }
        buffer.Flush(hdc);
        EndPaint(hwnd, &ps);
        PositionPageEdit(win);
        return 0;
    }
    if ((msg == WM_CTLCOLOREDIT || msg == WM_CTLCOLORSTATIC) && win) {
        HDC hdc = (HDC)wp;
        SetTextColor(hdc, TbTextColor());
        SetBkColor(hdc, ThemeWindowControlBackgroundColor());
        if (IsCurrentThemeDefault() && !ThemeColorizeControls() && !ThemeUsesHighContrastColors()) {
            return (LRESULT)GetStockObject(WHITE_BRUSH);
        }
        return (LRESULT)win->brControlBgColor;
    }
    if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK) {
        if (win && win->tabsInTitlebar) {
            Point pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            HWND childAtPoint = ChildWindowFromPoint(hwnd, ToPOINT(pt));
            bool overChild = childAtPoint && childAtPoint != hwnd;
            VirtCtrl* hit = ToolbarItemFromPoint(win, pt);
            if (!overChild && (!hit || hit->id == 0 || hit->id == PageInfoId)) {
                HWND hwndFrame = GetAncestor(hwnd, GA_ROOT);
                if (msg == WM_LBUTTONDBLCLK) {
                    WPARAM cmd = IsZoomed(hwndFrame) ? SC_RESTORE : SC_MAXIMIZE;
                    PostMessageW(hwndFrame, WM_SYSCOMMAND, cmd, 0);
                } else {
                    ReleaseCapture();
                    SendMessageW(hwndFrame, WM_NCLBUTTONDOWN, HTCAPTION, 0);
                }
                return 0;
            }
        }
    }
    if (msg == WM_MOUSEMOVE || msg == WM_MOUSELEAVE) {
        if (win && win->isToolbarOverlay) {
            if (msg == WM_MOUSEMOVE) {
                TRACKMOUSEEVENT tme{sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0};
                TrackMouseEvent(&tme);
            }
            UpdateOverlayToolbarForMouse(win);
        }
    }
    if (tb && tb->vroot) {
        LRESULT res = 0;
        if (VirtTreeOnMessage(hwnd, tb->vroot, msg, wp, lp, res)) {
            return res;
        }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void RegisterVirtToolbarClass() {
    static ATOM atom = 0;
    if (atom) {
        return;
    }
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = WndProcVirtToolbar;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = GetCachedCursor(IDC_ARROW);
    wc.lpszClassName = kVirtToolbarClass.s;
    atom = RegisterClassExW(&wc);
}

void CreateToolbar(MainWindow* win) {
    bool isRtl = IsUIRtl();
    RegisterVirtToolbarClass();

    HINSTANCE hinst = GetModuleHandle(nullptr);
    HWND hwndParent = win->hwndFrame;

    // WS_CLIPSIBLINGS so that in overlay mode the canvas (a lower-Z sibling)
    // doesn't paint over the floating toolbar
    DWORD style = WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE;
    DWORD exStyle = WS_EX_TOOLWINDOW;
    if (isRtl) {
        exStyle |= WS_EX_LAYOUTRTL;
    }

    PopulateToolbarLayout();
    PopulateCustomToolbarButtons();
    int iconSize = SetToolbarIconsImageList(win);
    int yPad = DpiScale(2);
    int rowDy = ToolbarRowDy(iconSize);

    HWND hwnd = CreateWindowExW(exStyle, kVirtToolbarClass.s, nullptr, style, 0, 0, 100, rowDy, hwndParent,
                                (HMENU)IDC_REBAR, hinst, nullptr);
    win->hwndReBar = hwnd;
    win->hwndToolbar = hwnd;

    auto* tb = new ToolbarVirt();
    tb->win = win;
    tb->iconSize = iconSize;
    tb->rowDy = rowDy;
    int defFontSize = GetAppFontSize();
    int newSize = defFontSize;
    int maxFontSize = iconSize - (yPad * 2) - 2;
    if (newSize > maxFontSize) {
        newSize = maxFontSize;
    }
    tb->font = GetDefaultGuiFontOfSize(newSize);
    tb->platformFont = GetPlatformFont(tb->font);
    win->toolbarVirt = tb;
    HwndSetFont(hwnd, tb->font);

    CreatePageBox(win, tb->font, iconSize);
    BuildToolbarLayout(win);

    DocController* ctrl = win->ctrl;
    UpdateToolbarPageText(win, ctrl ? ctrl->PageCount() : -1);
    if (ctrl && win->hwndPageEdit) {
        TempStr label = ctrl->GetPageLabeTemp(ctrl->CurrentPageNo());
        HwndSetText(win->hwndPageEdit, label);
        HwndSetWindowStyle(win->hwndPageEdit, ES_NUMBER, !ctrl->HasPageLabels());
    }
    UpdateToolbarFindText(win);
    ToolbarUpdateStateForWindow(win, true);
}

void DestroyToolbar(MainWindow* win) {
    if (!win->hwndReBar && !win->toolbarVirt) {
        return;
    }
    HwndDestroyWindowSafe(&win->hwndPageEdit);
    HwndDestroyWindowSafe(&win->hwndPageLabel);
    HwndDestroyWindowSafe(&win->hwndPageBg);
    HwndDestroyWindowSafe(&win->hwndPageTotal);
    FreeToolbarVirt(win);
    HwndDestroyWindowSafe(&win->hwndToolbar);
    win->hwndReBar = nullptr;
}

void ReCreateToolbar(MainWindow* win) {
    DestroyToolbar(win);
    CreateToolbar(win);
}

static int MenuBarToolbarIdealDy(MainWindow* win) {
    HFONT font = GetAppMenuFont();
    int dy = FontDyPx(win->hwndFrame, font) + DpiScale(4);
    int minDy = DpiScale(kTabBarDy);
    return std::max(dy, minDy);
}

int GetMenuBarRebarHeight(MainWindow* win) {
    HWND hwnd = win ? win->hwndMenuReBar : nullptr;
    if (!hwnd || !::IsWindow(hwnd)) {
        return 0;
    }
    // RB_GETBARHEIGHT underreports by 1px without WS_BORDER
    int dy = (int)SendMessageW(hwnd, RB_GETBARHEIGHT, 0, 0) + 1;
    if (dy > 1) {
        if (IsRunningOnWine()) {
            logf("GetMenuBarRebarHeight: rebar=%p RB_GETBARHEIGHT=%d\n", win->hwndMenuReBar, dy);
        }
        return dy;
    }
    int ideal = MenuBarToolbarIdealDy(win);
    if (IsRunningOnWine()) {
        logf("GetMenuBarRebarHeight: rebar=%p RB_GETBARHEIGHT=%d fallbackIdeal=%d\n", win->hwndMenuReBar, dy, ideal);
    }
    return ideal;
}

// --- Menu bar as rebar control (used when tabs are in titlebar) ---

static LRESULT CALLBACK MenuBarReBarWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass,
                                            DWORD_PTR /*dwRefData*/) {
    if (WM_ERASEBKGND == uMsg) {
        // always paint background with theme color to avoid gray strips in light theme
        HDC hdc = (HDC)wParam;
        Color bgCol = ThemeControlBackgroundColor();
        auto* bgBrush = CreateSolidBrush(bgCol);
        HdcFillRect(hdc, HwndClientRect(hWnd), bgBrush);
        DeleteObject(bgBrush);
        return 1;
    }
    if (WM_NOTIFY == uMsg) {
        auto* win = FindMainWindowByHwnd(hWnd);
        NMHDR* hdr = (NMHDR*)lParam;
        if (win && hdr->code == NM_CUSTOMDRAW && hdr->hwndFrom == win->hwndMenuToolbar) {
            NMTBCUSTOMDRAW* custDraw = (NMTBCUSTOMDRAW*)hdr;
            switch (custDraw->nmcd.dwDrawStage) {
                case CDDS_PREPAINT:
                    return CDRF_NOTIFYITEMDRAW;
                case CDDS_ITEMPREPAINT: {
                    auto col = ThemeWindowTextColor();
                    UINT itemState = custDraw->nmcd.uItemState;
                    if (itemState & CDIS_DISABLED) {
                        col = ThemeWindowTextDisabledColor();
                    }
                    custDraw->clrText = col;
                    return CDRF_DODEFAULT;
                }
            }
        }
    }
    if (WM_NCDESTROY == uMsg) {
        RemoveWindowSubclass(hWnd, MenuBarReBarWndProc, uIdSubclass);
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

static LRESULT CALLBACK MenuBarToolbarWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass,
                                              DWORD_PTR /*dwRefData*/) {
    if (WM_ERASEBKGND == uMsg) {
        // don't erase background here; toolbar paints its own background during WM_PAINT
        // filling here causes visible flicker (erase then paint) during window resize
        return 1;
    }
    if (WM_NCDESTROY == uMsg) {
        RemoveWindowSubclass(hWnd, MenuBarToolbarWndProc, uIdSubclass);
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

constexpr int kMenuBarCmdFirst = 50000;
constexpr int kMenuBarCmdLast = 50020;

struct MenuBarPopupNav {
    MainWindow* win = nullptr;
    HMENU rootMenu = nullptr;
    HMENU currentMenu = nullptr;
    UINT currentFlags = 0;
    int nextMenuIdx = -1;
};

static MenuBarPopupNav gMenuBarPopupNav;

// track when a menu popup was last dismissed so a second click on the same
// menu bar button closes the popup instead of immediately reopening it
static int gMenuBarLastDismissedIdx = -1;
static u64 gMenuBarLastDismissedTick = 0;

static bool ShouldSwitchCustomMenuBarPopup(UINT vk) {
    if (!gMenuBarPopupNav.win || !gMenuBarPopupNav.rootMenu) {
        return false;
    }
    if (!gMenuBarPopupNav.currentMenu || gMenuBarPopupNav.currentMenu != gMenuBarPopupNav.rootMenu) {
        return false;
    }
    if (bit::IsMaskSet(gMenuBarPopupNav.currentFlags, (UINT)MF_POPUP)) {
        return false;
    }

    int menuCount = GetMenuItemCount(gMenuBarPopupNav.win->menu);
    if (menuCount <= 1) {
        return false;
    }

    int step = 0;
    if (vk == VK_LEFT) {
        step = -1;
    } else if (vk == VK_RIGHT) {
        step = 1;
    }
    if (step == 0) {
        return false;
    }

    gMenuBarPopupNav.nextMenuIdx += step;
    if (gMenuBarPopupNav.nextMenuIdx < 0) {
        gMenuBarPopupNav.nextMenuIdx = menuCount - 1;
    } else if (gMenuBarPopupNav.nextMenuIdx >= menuCount) {
        gMenuBarPopupNav.nextMenuIdx = 0;
    }
    return true;
}

// check if mouse is over a different toolbar button and switch to it
static bool ShouldSwitchMenuBarOnMouseMove() {
    if (!gMenuBarPopupNav.win || !gMenuBarPopupNav.win->hwndMenuToolbar) {
        return false;
    }
    HWND hwndTb = gMenuBarPopupNav.win->hwndMenuToolbar;

    Point pt = HwndGetCursorPos(hwndTb);

    // hit-test the toolbar
    int btnCount = TbGetButtonCount(hwndTb);
    for (int i = 0; i < btnCount; i++) {
        Rect rc = TbGetItemRect(hwndTb, i);
        if (rc.Contains(pt.x, pt.y)) {
            TBBUTTON tb{};
            SendMessageW(hwndTb, TB_GETBUTTON, i, (LPARAM)&tb);
            int menuIdx = tb.idCommand - kMenuBarCmdFirst;
            if (menuIdx != gMenuBarPopupNav.nextMenuIdx) {
                gMenuBarPopupNav.nextMenuIdx = menuIdx;
                return true;
            }
            return false;
        }
    }
    return false;
}

static LRESULT CALLBACK MenuBarMsgFilterHook(int code, WPARAM wParam, LPARAM lParam) {
    if (code == MSGF_MENU && gMenuBarPopupNav.win) {
        MSG* msg = (MSG*)lParam;
        if ((msg->message == WM_KEYDOWN || msg->message == WM_SYSKEYDOWN) &&
            ShouldSwitchCustomMenuBarPopup((UINT)msg->wParam)) {
            EndMenu();
            return 1;
        }
        if (msg->message == WM_MOUSEMOVE && ShouldSwitchMenuBarOnMouseMove()) {
            EndMenu();
            return 1;
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

void UpdateCustomMenuBarMenuSelect(MainWindow* win, WPARAM wp, LPARAM lp) {
    if (gMenuBarPopupNav.win != win) {
        return;
    }

    UINT flags = HIWORD(wp);
    HMENU menu = (HMENU)lp;
    if (flags == 0xFFFF && !menu) {
        gMenuBarPopupNav.currentMenu = nullptr;
        gMenuBarPopupNav.currentFlags = 0;
        return;
    }

    gMenuBarPopupNav.currentMenu = menu;
    gMenuBarPopupNav.currentFlags = flags;
}

void RebuildMenuBarButtons(MainWindow* win) {
    HWND hwndMb = win->hwndMenuToolbar;
    if (!hwndMb) {
        return;
    }

    // remove existing buttons
    while (SendMessageW(hwndMb, TB_DELETEBUTTON, 0, 0)) {
    }

    HMENU menu = win->menu;
    int count = GetMenuItemCount(menu);
    if (count <= 0) {
        return;
    }

    MENUITEMINFOW mii{};
    mii.cbSize = sizeof(MENUITEMINFOW);
    mii.fMask = MIIM_SUBMENU | MIIM_STRING;

    for (int i = 0; i < count && i < (kMenuBarCmdLast - kMenuBarCmdFirst); i++) {
        mii.dwTypeData = nullptr;
        mii.cch = 0;
        GetMenuItemInfoW(menu, i, TRUE, &mii);
        if (!mii.hSubMenu || !mii.cch) {
            continue;
        }
        mii.cch++;
        WCHAR* name = AllocArrayTemp<WCHAR>((int)mii.cch);
        mii.dwTypeData = name;
        GetMenuItemInfoW(menu, i, TRUE, &mii);

        TBBUTTON b{};
        b.iBitmap = I_IMAGENONE;
        b.idCommand = kMenuBarCmdFirst + i;
        b.fsState = TBSTATE_ENABLED;
        b.fsStyle = BTNS_AUTOSIZE | BTNS_SHOWTEXT;
        b.iString = (INT_PTR)name;
        TbAddButtons(hwndMb, 1, &b);
    }

    TbAutosIZE(hwndMb);

    if (win->hwndMenuReBar) {
        Rect rc = TbGetItemRect(hwndMb, 0);
        int menuBarDy = MenuBarToolbarIdealDy(win);
        if (rc.dy > 0) {
            menuBarDy = rc.dy + (2 * rc.y);
        }
        REBARBANDINFOW rbBand{};
        rbBand.cbSize = sizeof(REBARBANDINFOW);
        rbBand.fMask = RBBIM_CHILDSIZE;
        rbBand.cyChild = menuBarDy;
        rbBand.cyMinChild = menuBarDy;
        SendMessageW(win->hwndMenuReBar, RB_SETBANDINFO, 0, (LPARAM)&rbBand);
    }
}

void CreateMenuBarRebar(MainWindow* win) {
    if (!win || win->hwndMenuReBar) {
        return;
    }
    // embedded hosts (TC lister) must not get titlebar menu rebar chrome
    if (gMyWindowWasEmbedded) {
        return;
    }
    HWND hwndParent = win->hwndFrame;
    if (!hwndParent || !::IsWindow(hwndParent)) {
        return;
    }

    bool isRtl = IsUIRtl();
    HINSTANCE hinst = GetModuleHandle(nullptr);

    // create hidden; caller shows after the scheduled relayout positions it
    // no WS_BORDER (avoids 1px gap) and no RBS_BANDBORDERS (avoids gray band separators)
    DWORD style = WS_CHILD | WS_CLIPCHILDREN | RBS_VARHEIGHT;
    style |= CCS_NODIVIDER | CCS_NOPARENTALIGN;
    DWORD exStyle = WS_EX_TOOLWINDOW;
    if (isRtl) {
        exStyle |= WS_EX_LAYOUTRTL;
    }

    win->hwndMenuReBar = CreateWindowExW(exStyle, REBARCLASSNAME, nullptr, style, 0, 0, 0, 0, hwndParent,
                                         (HMENU)IDC_MENUBAR_REBAR, hinst, nullptr);
    SetWindowSubclass(win->hwndMenuReBar, MenuBarReBarWndProc, 0, 0);

    REBARINFO rbi{};
    rbi.cbSize = sizeof(REBARINFO);
    SendMessageW(win->hwndMenuReBar, RB_SETBARINFO, 0, (LPARAM)&rbi);
    SendMessageW(win->hwndMenuReBar, RB_SETBKCOLOR, 0, ThemeControlBackgroundColor());

    style = WS_CHILD | WS_CLIPSIBLINGS | TBSTYLE_FLAT | TBSTYLE_LIST;
    style |= CCS_NODIVIDER | CCS_NOPARENTALIGN;
    exStyle = 0;
    if (isRtl) {
        exStyle |= WS_EX_LAYOUTRTL;
    }

    win->hwndMenuToolbar = CreateWindowExW(exStyle, TOOLBARCLASSNAME, nullptr, style, 0, 0, 0, 0, win->hwndMenuReBar,
                                           (HMENU)IDC_MENUBAR, hinst, nullptr);
    SetWindowSubclass(win->hwndMenuToolbar, MenuBarToolbarWndProc, 0, 0);
    TbSetButtonStructSize(win->hwndMenuToolbar, sizeofi(TBBUTTON));

    if (!UseDarkModeLib() || !DarkMode::isEnabled()) {
        if (!IsCurrentThemeDefault()) {
            SetWindowTheme(win->hwndMenuToolbar, L"", L"");
        }
    }

    HFONT font = GetAppMenuFont();
    HwndSetFont(win->hwndMenuToolbar, font);

    DWORD tbExStyle = TbGetExtendedStyle(win->hwndMenuToolbar);
    tbExStyle |= TBSTYLE_EX_MIXEDBUTTONS;
    TbSetExtendedStyle(win->hwndMenuToolbar, tbExStyle);

    RebuildMenuBarButtons(win);

    Rect rc = TbGetItemRect(win->hwndMenuToolbar, 0);
    int menuBarDy = rc.dy + (2 * rc.y);
    if (menuBarDy <= 0) {
        menuBarDy = MenuBarToolbarIdealDy(win);
    }

    ShowWindow(win->hwndMenuToolbar, SW_SHOW);

    REBARBANDINFOW rbBand{};
    rbBand.cbSize = sizeof(REBARBANDINFOW);
    rbBand.fMask = RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE;
    rbBand.fStyle = RBBS_FIXEDSIZE;
    rbBand.hwndChild = win->hwndMenuToolbar;
    rbBand.cxMinChild = 0;
    rbBand.cyMinChild = menuBarDy;
    rbBand.cx = 0;
    SendMessageW(win->hwndMenuReBar, RB_INSERTBAND, (WPARAM)-1, (LPARAM)&rbBand);

    if (UseDarkModeLib()) {
        DarkMode::setWindowNotifyCustomDrawSubclass(win->hwndMenuReBar);
        DarkMode::setChildCtrlsSubclassAndTheme(win->hwndMenuReBar);
    }
}

void ShowMenuBarRebar(MainWindow* win) {
    HWND hwnd = win ? win->hwndMenuReBar : nullptr;
    if (hwnd && ::IsWindow(hwnd)) {
        ShowWindow(hwnd, SW_SHOW);
    }
}

void DestroyMenuBarRebar(MainWindow* win) {
    if (!win) {
        return;
    }
    // clear fields first so re-entrant layout/paint cannot SetWindowPos them
    HWND hwndTb = win->hwndMenuToolbar;
    HWND hwndRb = win->hwndMenuReBar;
    win->hwndMenuToolbar = nullptr;
    win->hwndMenuReBar = nullptr;
    // hide before destroy so nested paint is less likely to walk half-torn
    // rebar/toolbar scroll-arrow state (comctl32!DrawScrollBar AV)
    if (hwndRb && ::IsWindow(hwndRb)) {
        ShowWindow(hwndRb, SW_HIDE);
    }
    if (hwndTb && ::IsWindow(hwndTb)) {
        ShowWindow(hwndTb, SW_HIDE);
        DestroyWindow(hwndTb);
    }
    if (hwndRb && ::IsWindow(hwndRb)) {
        DestroyWindow(hwndRb);
    }
}

bool IsShowingMenuBarRebar(MainWindow* win) {
    if (!win) {
        return false;
    }
    HWND hwnd = win->hwndMenuReBar;
    if (!hwnd || !::IsWindow(hwnd)) {
        return false;
    }
    // host reparented us as WS_CHILD: menu rebar is being (or about to be)
    // torn down; treat as not showing so layout does not SetWindowPos it
    if (gMyWindowWasEmbedded) {
        return false;
    }
    if (win->presentation) {
        return false;
    }
    return true;
}

bool HandleMenuBarCommand(MainWindow* win, int cmdId) {
    if (cmdId < kMenuBarCmdFirst || cmdId >= kMenuBarCmdLast) {
        return false;
    }
    if (!win->hwndMenuToolbar) {
        return false;
    }

    int menuCount = GetMenuItemCount(win->menu);
    int menuIdx = cmdId - kMenuBarCmdFirst;

    // if same button was clicked shortly after dismissing its popup, treat as toggle-close
    u64 now = GetTickCount64();
    if (menuIdx == gMenuBarLastDismissedIdx && (now - gMenuBarLastDismissedTick) < 500) {
        gMenuBarLastDismissedIdx = -1;
        return true;
    }

    UINT flags = TPM_LEFTALIGN | TPM_TOPALIGN;
    if (IsUIRtl()) {
        flags = TPM_RIGHTALIGN | TPM_TOPALIGN;
    }

    for (;;) {
        HMENU subMenu = GetSubMenu(win->menu, menuIdx);
        if (!subMenu) {
            return true;
        }

        // get button rect in screen coordinates
        int btnCmdId = kMenuBarCmdFirst + menuIdx;
        int btnIdx = (int)SendMessageW(win->hwndMenuToolbar, TB_COMMANDTOINDEX, btnCmdId, 0);
        Rect btnRect = TbGetItemRect(win->hwndMenuToolbar, btnIdx);
        btnRect = HwndMapRectToWindow(btnRect, win->hwndMenuToolbar, HWND_DESKTOP);

        gMenuBarPopupNav.win = win;
        gMenuBarPopupNav.rootMenu = subMenu;
        gMenuBarPopupNav.currentMenu = subMenu;
        gMenuBarPopupNav.currentFlags = 0;
        gMenuBarPopupNav.nextMenuIdx = menuIdx;

        HHOOK hook = SetWindowsHookExW(WH_MSGFILTER, MenuBarMsgFilterHook, nullptr, GetCurrentThreadId());
        TrackPopupMenu(subMenu, flags, btnRect.x, btnRect.y + btnRect.dy, 0, win->hwndFrame, nullptr);
        if (hook) {
            UnhookWindowsHookEx(hook);
        }

        int nextMenuIdx = gMenuBarPopupNav.nextMenuIdx;
        gMenuBarPopupNav = {};
        if (nextMenuIdx == menuIdx || menuCount <= 1) {
            gMenuBarLastDismissedIdx = menuIdx;
            gMenuBarLastDismissedTick = GetTickCount64();
            break;
        }
        menuIdx = nextMenuIdx;
    }

    return true;
}

// Activate a menu bar button by accelerator key (Alt+letter).
// If accel is 0, activate the first menu item.
// Returns true if handled.
bool ActivateMenuBarByAccel(MainWindow* win, WCHAR accel) {
    if (!win->hwndMenuToolbar || !win->menu) {
        return false;
    }

    int count = GetMenuItemCount(win->menu);
    if (count <= 0) {
        return false;
    }

    // if accel is 0 (bare Alt press), open the first menu
    if (accel == 0) {
        return HandleMenuBarCommand(win, kMenuBarCmdFirst);
    }

    // normalize to uppercase for matching
    if (accel >= 'a' && accel <= 'z') {
        accel -= 'a' - 'A';
    }

    // find the menu item whose text has &<accel>
    MENUITEMINFOW mii{};
    mii.cbSize = sizeof(MENUITEMINFOW);
    mii.fMask = MIIM_STRING;

    for (int i = 0; i < count && i < (kMenuBarCmdLast - kMenuBarCmdFirst); i++) {
        mii.dwTypeData = nullptr;
        mii.cch = 0;
        GetMenuItemInfoW(win->menu, i, TRUE, &mii);
        if (!mii.cch) {
            continue;
        }
        mii.cch++;
        WCHAR* name = AllocArrayTemp<WCHAR>((int)mii.cch);
        mii.dwTypeData = name;
        GetMenuItemInfoW(win->menu, i, TRUE, &mii);

        // look for &X where X matches accel
        WStr menuName(name, len(WStr(name)));
        for (int off = 0; off < menuName.len; off++) {
            if (menuName.s[off] == L'&' && off + 1 < menuName.len) {
                WCHAR ch = menuName.s[off + 1];
                if (ch >= 'a' && ch <= 'z') {
                    ch -= 'a' - 'A';
                }
                if (ch == accel) {
                    return HandleMenuBarCommand(win, kMenuBarCmdFirst + i);
                }
                break;
            }
        }
    }

    return false;
}
