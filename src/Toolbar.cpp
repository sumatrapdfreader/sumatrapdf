/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "gui/Dpi.h"
#include "base/Win.h"
#include "base/BitManip.h"
#include "base/Pixmap.h"

#include "gui/UIModels.h"

#include "Accelerators.h"
#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
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

struct ToolbarVirt {
    MainWindow* win = nullptr;
    ILayout* layout = nullptr;
    VirtRoot* vroot = nullptr;
    Vec<VirtCtrl*> items; // not owned; layout owns them
    VirtText* pageLabel = nullptr;
    VirtText* pageTotal = nullptr;
    PlatformFont* platformFont = nullptr;
    int iconSize = 0;
    int rowDy = 0;
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
    if (tb->pageTotal && tb->pageTotal->GetVisibility() == Visibility::Visible &&
        tb->pageTotal->BoundsInWindow().Contains(pt)) {
        return tb->pageTotal;
    }
    return nullptr;
}

static void SetToolbarButtonEnabledByIdx(MainWindow* win, int idx, bool isEnabled) {
    VirtCtrl* w = ToolbarItemAt(win, idx);
    if (!w || w->IsEnabled() == isEnabled) {
        return;
    }
    w->SetIsEnabled(isEnabled);
    w->Invalidate();
}

// hiding the page box hides the whole group (label + edit + " / N")
static void SetToolbarButtonHiddenByIdx(MainWindow* win, int idx, bool isHidden) {
    VirtCtrl* w = ToolbarItemAt(win, idx);
    if (!w) {
        return;
    }
    Visibility want = isHidden ? Visibility::Collapse : Visibility::Visible;
    if (w->GetVisibility() == want) {
        return;
    }
    w->SetVisibility(want);
    ToolbarVirt* tb = win->toolbarVirt;
    if (w->id == PageInfoId && tb) {
        if (tb->pageLabel) {
            tb->pageLabel->SetVisibility(want);
        }
        if (win->pageEdit) {
            win->pageEdit->SetVisibility(want);
        }
        if (tb->pageTotal) {
            tb->pageTotal->SetVisibility(want);
        }
    }
    if (tb && tb->vroot) {
        tb->vroot->RequestLayout();
    }
    RelayoutToolbar(win);
    HwndInvalidate(win->hwndToolbar, true);
}

static void SetToolbarButtonCheckedByIdx(MainWindow* win, int idx, bool isChecked) {
    // a custom button with ToolbarText is a VirtButton, which has no
    // checked state (and is not a VirtIconButton)
    auto* ib = AsVirtIconButton(ToolbarItemAt(win, idx));
    if (!ib || ib->isSelected == isChecked) {
        return;
    }
    ib->isSelected = isChecked;
    ib->Invalidate();
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
        for (const ToolbarButtonInfo& tbi : gToolbarButtons) {
            addButton(tbi);
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
        SetToolbarButtonCheckedByIdx(win, idx, isChecked);
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
            // Need non-empty find text (findEdit is the active bar or floating window edit).
            if (!win->findEdit || win->findEdit->GetTextLen() == 0) {
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
    auto* ib = AsVirtIconButton(w);
    if (!ib) {
        return;
    }
    ToolbarVirt* tb = win->toolbarVirt;
    int sz = tb ? tb->iconSize : DpiScale(gGlobalPrefs->toolbarSize);
    Pixmap* px = GetCachedPixmapForSvg(icon, sz, sz, TbTextColor());
    Pixmap* pxOff = GetCachedPixmapForSvg(icon, sz, sz, TbDisabledColor());
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
        // separators are always drawn. Which ones to drop is decided below,
        // by position, not by command availability.
        if (setButtonsVisibility && cmdId != WarningMsgId && cmdId != 0) {
            bool hide = !IsCmdAvailable(win, cmdId);
            SetToolbarButtonHiddenByIdx(win, i, hide);
        }
        if (SkipBuiltInButton(tb)) {
            continue;
        }
        bool isEnabled = IsCmdEnabled(win, cmdId);
        SetToolbarButtonEnabledByIdx(win, i, isEnabled);

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
                SetToolbarButtonHiddenByIdx(win, i, hide);
                prevVisibleNonSep = false;
                if (!hide) {
                    lastSep = i;
                }
                continue;
            }
            // the page box counts as visible content: a separator right after
            // it is not a leading one (ToolbarCustomLayout = PageInfo | ...)
            if (w->GetVisibility() == Visibility::Visible) {
                prevVisibleNonSep = true;
                lastSep = -1;
            }
        }
        if (lastSep >= 0) {
            SetToolbarButtonHiddenByIdx(win, lastSep, true);
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
        SetToolbarButtonEnabledByIdx(win, idx, isEnabled);
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
static int OverlayToolbarBottomScrollbarOffset() {
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
    int h = HwndWindowRect(win->hwndToolbar).dy;
    int x = canvas.x + ((canvas.dx - natW) / 2);
    int y = canvas.y;
    if (ToolbarAtBottom()) {
        y = canvas.y + canvas.dy - h - OverlayToolbarBottomScrollbarOffset();
    }
    return {x, y, natW, h};
}

// position/show the floating overlay toolbar; called on relayout and mouse move
void PositionOverlayToolbar(MainWindow* win) {
    if (!win->isToolbarOverlay || !win->hwndToolbar) {
        return;
    }
    Rect r = OverlayToolbarRect(win);
    UINT flags = SWP_NOACTIVATE;
    flags |= win->toolbarOverlayShown ? SWP_SHOWWINDOW : SWP_HIDEWINDOW;
    SetWindowPos(win->hwndToolbar, HWND_TOP, r.x, r.y, r.dx, r.dy, flags);
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
    bool overToolbar = hwndUnder && (hwndUnder == win->hwndToolbar || IsChild(win->hwndToolbar, hwndUnder));
    return inBand || overToolbar;
}

// the overlay toolbar must not vanish while it owns the keyboard focus (e.g.
// the user is typing a page number into the page box after Ctrl+G)
static bool OverlayToolbarHasFocus(MainWindow* win) {
    if (!win->hwndToolbar) {
        return false;
    }
    HWND focus = GetFocus();
    if (!focus) {
        return false;
    }
    return focus == win->hwndToolbar || IsChild(win->hwndToolbar, focus);
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
    if (!win->isToolbarOverlay || !win->hwndToolbar) {
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
    if (!win->isToolbarOverlay || !win->hwndToolbar) {
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
        if ((win->findEdit && win->findEdit->IsFocused()) || (win->pageEdit && win->pageEdit->IsFocused())) {
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

    // no document: the find edit does nothing, so don't offer a text cursor
    LPWSTR cursorId = win->IsDocLoaded() ? nullptr : IDC_ARROW;
    if (win->findEdit) {
        win->findEdit->SetCursorId(cursorId);
    }
}

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

static void OnPageEditChar(MainWindow* win, Edit::CharEvent* ev) {
    if (!win || !win->IsDocLoaded() || !win->pageEdit) {
        return;
    }
    switch (ev->c) {
        case VK_RETURN: {
            TempStr s = win->pageEdit->GetTextTemp();
            int newPageNo = win->ctrl->GetPageByLabel(s);
            if (win->ctrl->ValidPageNo(newPageNo)) {
                win->ctrl->GoToPage(newPageNo, true);
                HwndSetFocus(win->hwndFrame);
                // the overlay toolbar was kept up by the focus; now that
                // it's gone, let it hide again
                UpdateOverlayToolbarForMouse(win);
            }
            ev->didHandle = true;
            return;
        }
        case VK_ESCAPE:
            HwndSetFocus(win->hwndFrame);
            UpdateOverlayToolbarForMouse(win);
            ev->didHandle = true;
            return;
        case VK_TAB:
            AdvanceFocus(win);
            ev->didHandle = true;
            return;
    }
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

static int PageEditPadL() {
    return DpiGetSystemMetrics(SM_CXEDGE);
}

static int PageEditPadR() {
    return PageEditPadL() + DpiScale(4);
}

static Edit* CreatePageEdit(MainWindow* win, HFONT font, int iconDy) {
    Edit::CreateArgs args;
    args.parent = win->hwndToolbar;
    args.font = font;
    args.isRtl = IsUIRtl();
    // no WS_EX_CLIENTEDGE: a themed edit draws a blue bottom accent (Win11)
    args.withFrame = true;
    args.noTheme = true;
    args.numbersOnly = true;
    args.alignRight = true;
    args.selectAllOnFocus = true;
    // the box is as tall as the icons, so without this the digits would sit at
    // its top instead of on the same line as "Page:" and "/ N"
    args.centerTextVert = true;
    args.text = StrL("0");
    args.marginLeft = PageEditPadL();
    args.marginRight = PageEditPadR();
    auto* e = new Edit();
    e->SetColors(TbTextColor(), ThemeWindowControlBackgroundColor());
    e->Create(args);
    e->SetIdealWidthFromText(StrL("999999"), DpiScale(24));
    e->idealDy = iconDy;
    e->onChar = MkFunc1(OnPageEditChar, win);
    return e;
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

static int ToolbarIconSize() {
    return RoundUp(DpiScale(gGlobalPrefs->toolbarSize), 4);
}

static void ApplyToolbarItemColors(VirtCtrl* w) {
    Color hover = TbHoverColor();
    Color sel = TbSelectedColor();
    if (auto* ib = AsVirtIconButton(w)) {
        ib->bgColorHover = hover;
        ib->bgColorSelected = sel;
        ib->chevronColor = TbTextColor();
        return;
    }
    if (auto* b = AsVirtButton(w)) {
        b->bgColorHover = hover;
        b->textColor = TbTextColor();
        b->textColorDisabled = TbDisabledColor();
        return;
    }
    if (auto* t = AsVirtText(w)) {
        t->textColor = TbTextColor();
        return;
    }
    if (auto* line = AsVirtLine(w)) {
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
        auto* ib = AsVirtIconButton(w);
        if (!ib) {
            continue;
        }
        const ToolbarButtonInfo& bi = GetToolbarButtonInfoByIdx(i);
        if (SkipBuiltInButton(bi) && !bi.svgIcon) {
            continue;
        }
        Str svg = bi.svgIcon ? bi.svgIcon : Str(bi.icon);
        ib->pixmap = GetCachedPixmapForSvg(svg, sz, sz, fg, TbBgColor());
        ib->pixmapDisabled = GetCachedPixmapForSvg(svg, sz, sz, dis, TbBgColor());
    }
    if (tb->pageLabel) {
        tb->pageLabel->textColor = TbTextColor();
    }
    if (tb->pageTotal) {
        tb->pageTotal->textColor = TbTextColor();
    }
    if (win->pageEdit) {
        win->pageEdit->SetColors(TbTextColor(), ThemeWindowControlBackgroundColor());
    }
}

void UpdateToolbarAfterThemeChange(MainWindow* win) {
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
        if (auto* b = AsVirtButton(w)) {
            text = b->s;
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
    if (auto* ib = AsVirtIconButton(w)) {
        if (ib->hasDropdown) {
            int dropDx = ib->DropdownDx();
            if (dropDx > 0 && ev->pt.x >= w->bounds.dx - dropDx) {
                ShowTtsVoiceMenu(win, GetToolbarButtonScreenRect(win, cmdId));
                ev->didHandle = true;
                return;
            }
        }
    }
    HwndPostCommand(win->hwndFrame, cmdId, 0);
    ev->didHandle = true;
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
    tb->pageLabel = nullptr;
    tb->pageTotal = nullptr;
    win->pageEdit = nullptr;

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
            // Old toolbar: label HWND was text + kTextPaddingRight + kButtonSpacingX
            // (10dpi) so "Page:" and "/ N" were not flush against the edit.
            int pageGap = DpiScale(kTextPaddingRight) + DpiScale(kButtonSpacingX);
            auto* label = new VirtText(_TRA("Page:"), tb->platformFont);
            label->textColor = fg;
            label->padding = {0, pageGap, 0, DpiScale(4)};
            label->id = PageInfoId;
            tb->pageLabel = label;
            box->AddChild(label);

            Edit* pageEdit = CreatePageEdit(win, tb->platformFont->GetHFont(), tb->iconSize);
            win->pageEdit = pageEdit;
            box->AddChild(pageEdit);

            auto* total = new VirtText(StrL(" "), tb->platformFont);
            total->textColor = fg;
            total->padding = {0, DpiScale(4), 0, pageGap};
            total->id = PageInfoId;
            tb->pageTotal = total;
            box->AddChild(total);
            tb->items.Append(label);
            continue;
        }
        if (bi.cmdId == 0 || (SkipBuiltInButton(bi) && !bi.svgIcon && !bi.isText)) {
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
            Str svg = bi.svgIcon ? bi.svgIcon : Str(bi.icon);
            ib->pixmap = GetCachedPixmapForSvg(svg, tb->iconSize, tb->iconSize, fg, TbBgColor());
            ib->pixmapDisabled = GetCachedPixmapForSvg(svg, tb->iconSize, tb->iconSize, dis, TbBgColor());
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
        if (IsCurrentThemeDefault() && !ThemeColorizeControls()) {
            HdcFillRect(memDC, {rc.x, rc.Bottom() - 1, rc.dx, 1}, TbEdgeColor());
        }
        buffer.Flush(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    if (msg == WM_COMMAND || msg == WM_CTLCOLOREDIT || msg == WM_CTLCOLORSTATIC) {
        LRESULT reflected = TryReflectMessages(hwnd, msg, wp, lp);
        if (reflected) {
            return reflected;
        }
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
    int iconSize = ToolbarIconSize();
    int yPad = DpiScale(2);
    int rowDy = ToolbarRowDy(iconSize);

    HWND hwnd = CreateWindowExW(exStyle, kVirtToolbarClass.s, nullptr, style, 0, 0, 100, rowDy, hwndParent, nullptr,
                                hinst, nullptr);
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
    tb->platformFont = GetPlatformFont(GetDefaultGuiFontOfSize(newSize));
    win->toolbarVirt = tb;
    HwndSetFont(hwnd, tb->platformFont->GetHFont());

    BuildToolbarLayout(win);

    DocController* ctrl = win->ctrl;
    UpdateToolbarPageText(win, ctrl ? ctrl->PageCount() : -1);
    if (ctrl && win->pageEdit) {
        TempStr label = ctrl->GetPageLabeTemp(ctrl->CurrentPageNo());
        win->pageEdit->SetText(label);
        win->pageEdit->SetNumbersOnly(!ctrl->HasPageLabels());
    }
    UpdateToolbarFindText(win);
    ToolbarUpdateStateForWindow(win, true);
}

void DestroyToolbar(MainWindow* win) {
    if (!win->hwndToolbar && !win->toolbarVirt) {
        return;
    }
    win->pageEdit = nullptr;
    FreeToolbarVirt(win);
    HwndDestroyWindowSafe(&win->hwndToolbar);
    win->hwndToolbar = nullptr;
}

void ReCreateToolbar(MainWindow* win) {
    DestroyToolbar(win);
    CreateToolbar(win);
}
