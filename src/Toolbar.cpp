/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Dpi.h"
#include "base/Win.h"
#include "base/BitManip.h"

extern "C" {
#include <mupdf/fitz.h>
}

#include "wingui/UIModels.h"

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
#include "FindBar.h"
#include "Translations.h"
#include "SvgIcons.h"
#include "Theme.h"
#include "DarkModeSubclass.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"
#include "TextToSpeech.h"

// https://docs.microsoft.com/en-us/windows/win32/controls/toolbar-control-reference

static int kButtonSpacingX = 4;

// distance between label and edit field
constexpr int kTextPaddingRight = 6;

struct ToolbarButtonInfo {
    /* index in the toolbar bitmap (-1 for separators) */
    TbIcon bmpIndex;
    int cmdId;
    Str toolTip;
    Str svgIcon;
};

// thos are not real commands but we have to refer to toolbar buttons
// is by a command. those are just background for area to be
// covered by other HWNDs. They need the right size
constexpr int PageInfoId = (int)CmdLast + 16;
constexpr int WarningMsgId = (int)CmdLast + 17;

static ToolbarButtonInfo gToolbarButtons[] = {
    {TbIcon::Open, CmdOpenFile, _TRN("Open")},
    {TbIcon::Print, CmdPrint, _TRN("Print")},
    {TbIcon::None, PageInfoId, nullptr}, // text box for page number + show current page / no of pages
    {TbIcon::PagePrev, CmdGoToPrevPage, _TRN("Previous Page")},
    {TbIcon::PageNext, CmdGoToNextPage, _TRN("Next Page")},
    {TbIcon::None, 0, nullptr}, // separator
    {TbIcon::NavigateBack, CmdNavigateBack, _TRN("Back")},
    {TbIcon::NavigateForward, CmdNavigateForward, _TRN("Forward")},
    {TbIcon::None, 0, nullptr}, // separator
    {TbIcon::Speak, CmdReadAloud, _TRN("Read Aloud")},
    {TbIcon::None, 0, nullptr}, // separator
    {TbIcon::LayoutContinuous, CmdZoomFitWidthAndContinuous, _TRN("Fit Width and Show Pages Continuously")},
    {TbIcon::LayoutSinglePage, CmdZoomFitPageAndSinglePage, _TRN("Fit a Single Page")},
    {TbIcon::RotateLeft, CmdRotateLeft, _TRN("Rotate &Left")},
    {TbIcon::RotateRight, CmdRotateRight, _TRN("Rotate &Right")},
    {TbIcon::ZoomOut, CmdZoomOut, _TRN("Zoom Out")},
    {TbIcon::ZoomIn, CmdZoomIn, _TRN("Zoom In")},
    {TbIcon::None, 0, nullptr}, // separator
    {TbIcon::Search, CmdFindFirst, _TRN("Find")},
};
// unicode chars: https://www.compart.com/en/unicode/U+25BC

constexpr int kButtonsCount = dimof(gToolbarButtons);

// 128 should be more than enough
// we use static array so that we don't have to generate
// code for Vec<ToolbarButtonInfo>
constexpr int kMaxCustomButtons = 127;
// +1 to ensure there's always space for WarningsMsgId button
static ToolbarButtonInfo gCustomButtons[kMaxCustomButtons + 1];
static int gCustomButtonsCount = 0;

static bool SkipBuiltInButton(const ToolbarButtonInfo& tbi) {
    return tbi.bmpIndex == TbIcon::None;
}

static void UpdateToolbarButtonStateByIdx(HWND hwnd, int idx, bool set, BYTE flag) {
    TBBUTTONINFOW bi{};
    bi.cbSize = sizeof(bi);
    bi.dwMask = TBIF_BYINDEX | TBIF_STATE;
    TbGetButtonInfo(hwnd, idx, &bi);
    BYTE newState = (BYTE)(set ? bi.fsState | flag : bi.fsState & ~flag);
    if (newState == bi.fsState) {
        // TB_SETBUTTONINFOW repaints the button even when nothing changes, which
        // flickers the toolbar (and the page-number controls floating over it)
        // e.g. on every page change while drag-selecting. Skip the no-op.
        return;
    }
    bi.fsState = newState;
    TbSetButtonInfo(hwnd, idx, &bi);
}

static int TotalButtonsCount() {
    return kButtonsCount + gCustomButtonsCount;
}

static ToolbarButtonInfo& GetToolbarButtonInfoByIdx(int idx) {
    if (idx < kButtonsCount) return gToolbarButtons[idx];
    return gCustomButtons[idx - kButtonsCount];
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
        UpdateToolbarButtonStateByIdx(win->hwndToolbar, idx, isChecked, TBSTATE_CHECKED);
    }
}

static void TbSetButtonDx(HWND hwndToolbar, int cmd, int dx) {
    TBBUTTONINFOW bi{};
    bi.cbSize = sizeof(bi);
    bi.dwMask = TBIF_SIZE;
    bi.cx = (WORD)dx;
    TbSetButtonInfo(hwndToolbar, cmd, &bi);
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

static TBBUTTON TbButtonFromButtonInfo(const ToolbarButtonInfo& bi, bool noTranslate = false) {
    TBBUTTON b{};
    b.idCommand = bi.cmdId;
    if (SkipBuiltInButton(bi)) {
        b.fsStyle = BTNS_SEP;
        return b;
    }
    b.iBitmap = (int)bi.bmpIndex;
    b.fsState = TBSTATE_ENABLED;
    b.fsStyle = BTNS_BUTTON;

    if (bi.cmdId == CmdReadAloud) {
        b.fsStyle |= BTNS_DROPDOWN;
    }

    if (bi.cmdId == CmdFindToggleMatchCase || bi.cmdId == CmdFindToggleMatchWholeWord) {
        b.fsStyle = BTNS_CHECK;
    }
    if (bi.bmpIndex == TbIcon::Text) {
        // b.fsStyle = BTNS_DROPDOWN;
        b.fsStyle |= BTNS_SHOWTEXT;
        b.fsStyle |= BTNS_AUTOSIZE;
    }
    Str s = noTranslate ? Str(bi.toolTip) : trans::GetTranslation(bi.toolTip);
    b.iString = (INT_PTR)CWStrTemp(s);
    return b;
}

// Set toolbar button tooltips taking current language into account.
void UpdateToolbarButtonsToolTipsForWindow(MainWindow* win) {
    TBBUTTONINFO binfo{};
    HWND hwnd = win->hwndToolbar;
    for (int i = 0; i < kButtonsCount; i++) {
        const ToolbarButtonInfo& bi = gToolbarButtons[i];
        if (!bi.toolTip) {
            continue;
        }
        if (bi.bmpIndex == TbIcon::Text) {
            continue;
        }
        TempStr accelStr = AppendAccelKeyToMenuStringTemp(nullptr, bi.cmdId);
        TempStr s = trans::GetTranslation(bi.toolTip);
        if (accelStr) {
            Str accel = accelStr.len > 1 ? Str(accelStr.s + 1, accelStr.len - 1) : accelStr;
            TempStr s2 = fmt(" (%s)", accel);
            s = str::JoinTemp(s, s2);
        }

        binfo.cbSize = sizeof(TBBUTTONINFO);
        binfo.dwMask = TBIF_TEXT | TBIF_BYINDEX;
        binfo.pszText = CWStrTemp(s);
        WPARAM buttonId = (WPARAM)i;
        TbSetButtonInfo(hwnd, (int)buttonId, &binfo);
    }
    // TODO: need an explicit tooltip window https://chatgpt.com/c/18fb77c8-761c-4314-a1ac-e55b93edfeef
#if 0
    if (gCustomToolbarButtons) {
        int n = gCustomToolbarButtons->Size();
        for (int i = 0; i < n; i++) {
            const ToolbarButtonInfo& bi = (*gCustomToolbarButtons)[i];
            TempStr accelStr = AppendAccelKeyToMenuStringTemp(nullptr, bi.cmdId);
            TempStr s = bi.toolTip;
            if (accelStr) {
                Str accel = accelStr.len > 1 ? Str(accelStr.s + 1, accelStr.len - 1) : accelStr;
                TempStr s2 = fmt(" (%s)", accel);
                s = str::JoinTemp(s, s2);
            }

            binfo.cbSize = sizeof(TBBUTTONINFO);
            binfo.dwMask = TBIF_TEXT | TBIF_BYINDEX;
            binfo.pszText = CWStrTemp(s);
            WPARAM buttonId = (WPARAM)(kButtonsCount + i);
            TbSetButtonInfo(hwnd, buttonId, &binfo);
        }
    }
#endif
}

static void SetToolbarButtonImageByIdx(HWND hwnd, int idx, TbIcon icon) {
    TBBUTTONINFOW bi{};
    bi.cbSize = sizeof(bi);
    bi.dwMask = TBIF_BYINDEX | TBIF_IMAGE;
    TbGetButtonInfo(hwnd, idx, &bi);
    if (bi.iImage == (int)icon) {
        return; // TB_SETBUTTONINFOW always repaints; skip no-ops
    }
    bi.iImage = (int)icon;
    TbSetButtonInfo(hwnd, idx, &bi);
}

// sets button text, which the toolbar shows as its tooltip
static void SetToolbarButtonToolTipByIdx(HWND hwnd, int idx, int cmdId, Str s) {
    TempStr accelStr = AppendAccelKeyToMenuStringTemp(nullptr, cmdId);
    if (accelStr) {
        Str accel = accelStr.len > 1 ? Str(accelStr.s + 1, accelStr.len - 1) : accelStr;
        TempStr s2 = fmt(" (%s)", accel);
        s = str::JoinTemp(s, s2);
    }
    // TB_GETBUTTONINFO with TBIF_TEXT needs a buffer; skip SET when equal
    WCHAR prevBuf[256]{};
    TBBUTTONINFOW bi{};
    bi.cbSize = sizeof(bi);
    bi.dwMask = TBIF_BYINDEX | TBIF_TEXT;
    bi.pszText = prevBuf;
    bi.cchText = dimof(prevBuf);
    TbGetButtonInfo(hwnd, idx, &bi);
    if (str::Eq(ToUtf8Temp(prevBuf), s)) {
        return;
    }
    bi.pszText = CWStrTemp(s);
    bi.cchText = 0;
    TbSetButtonInfo(hwnd, idx, &bi);
}

// TODO: this is called too often
// TODO: also set checked state instead of calling SetToolbarButtonCheckedState() all over
void ToolbarUpdateStateForWindow(MainWindow* win, bool setButtonsVisibility) {
    HWND hwnd = win->hwndToolbar;
    int n = TotalButtonsCount();
    for (int i = 0; i < n; i++) {
        auto& tb = GetToolbarButtonInfoByIdx(i);
        int cmdId = tb.cmdId;
        if (setButtonsVisibility && cmdId != WarningMsgId) {
            bool hide = !IsCmdAvailable(win, cmdId);
            UpdateToolbarButtonStateByIdx(hwnd, i, hide, TBSTATE_HIDDEN);
        }
        if (SkipBuiltInButton(tb)) {
            continue;
        }
        bool isEnabled = IsCmdEnabled(win, cmdId);
        UpdateToolbarButtonStateByIdx(hwnd, i, isEnabled, TBSTATE_ENABLED);

        if (cmdId == CmdReadAloud || cmdId == CmdPauseReadAloud) {
            bool speaking = TtsIsSpeaking();
            SetToolbarButtonImageByIdx(hwnd, i, speaking ? TbIcon::PauseSpeaking : TbIcon::Speak);
            // tooltip reflects what clicking the button will do
            Str tip = _TRA("Read Aloud");
            if (speaking) {
                tip = _TRA("Pause Reading");
            } else if (CanContinueReadAloud(win->CurrentTab())) {
                tip = _TRA("Continue Reading");
            }
            SetToolbarButtonToolTipByIdx(hwnd, i, cmdId, tip);
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
        UpdateToolbarButtonStateByIdx(win->hwndToolbar, idx, isEnabled, TBSTATE_ENABLED);
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
    if (!win->hwndReBar || !win->hwndToolbar) {
        return 0;
    }
    Rect rRebar = HwndWindowRect(win->hwndReBar);
    Rect rTb = HwndWindowRect(win->hwndToolbar);
    int contentRight = rTb.x; // screen x of the rightmost content edge
    Size tbSz = TbGetMaxSize(win->hwndToolbar);
    contentRight = std::max(contentRight, rTb.x + tbSz.dx);
    if (win->hwndPageTotal && HwndIsVisible(win->hwndPageTotal)) {
        Rect rpt = HwndWindowRect(win->hwndPageTotal);
        contentRight = std::max(contentRight, rpt.x + rpt.dx);
    }
    int natW = (contentRight - rRebar.x) + DpiScale(win->hwndFrame, 12);
    return natW;
}

// canvas rectangle in frame-client coordinates
static Rect CanvasRectInFrame(MainWindow* win) {
    Rect rc = HwndWindowRect(win->hwndCanvas);
    Point tl = HwndScreenToClient(win->hwndFrame, rc.TL());
    return Rect(tl, rc.Size());
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
        return DpiScale(win->hwndFrame, 16);
    }
    // windows native horizontal scrollbar
    return DpiGetSystemMetrics(win->hwndFrame, SM_CYHSCROLL);
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
    return Rect(x, y, natW, h);
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
    int my = DpiScale(win->hwndFrame, 16);
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
    // remove SS_WHITERECT so WM_CTLCOLORSTATIC controls the background color
    HwndSetWindowStyle(win->hwndPageBg, SS_WHITERECT, false);

    HwndInvalidate(win->hwndToolbar, true);
    if (HwndIsVisible(win->hwndFrame)) {
        UpdateWindow(win->hwndToolbar);
    }

    auto* cursorId = win->IsDocLoaded() ? IDC_IBEAM : IDC_ARROW;
    if (win->hwndFindEdit) {
        SetClassLongPtrW(win->hwndFindEdit, GCLP_HCURSOR, (LONG_PTR)GetCachedCursor(cursorId));
    }
}

static LRESULT CALLBACK ReBarWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass,
                                     DWORD_PTR /*dwRefData*/) {
    if (WM_ERASEBKGND == uMsg && ThemeColorizeControls()) {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, ThemeWindowTextColor());
        COLORREF bgCol = ThemeControlBackgroundColor();
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
        COLORREF bgCol2 = ThemeControlBackgroundColor();
        COLORREF col = AccentColor(bgCol2, 40);
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
                SetBkColor(hdc, RGB(0xff, 0xff, 0xff));
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

// subclass the toolbar so we can handle WM_CTLCOLOR* for the page box and
// allow dragging the window from empty toolbar areas
static void SubclassToolbar(MainWindow* win) {
    if (!DefWndProcToolbar) {
        DefWndProcToolbar = (WNDPROC)GetWindowLongPtr(win->hwndToolbar, GWLP_WNDPROC);
    }
    SetWindowLongPtr(win->hwndToolbar, GWLP_WNDPROC, (LONG_PTR)WndProcToolbar);
}

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
    if (!win->hwndToolbar) {
        return;
    }
    Str text = _TRA("Page:");
    if (!updateOnly) {
        HwndSetText(win->hwndPageLabel, text);
    }
    int padX = DpiScale(win->hwndFrame, kTextPaddingRight);
    Size size = HwndMeasureText(win->hwndPageLabel, text);
    size.dx += padX;
    size.dx += DpiScale(win->hwndFrame, kButtonSpacingX);

    Rect pageWndRect = HwndWindowRect(win->hwndPageBg);

    // TB_GETRECT fails for hidden buttons, so anchor on a button that's still
    // visible. CmdPrint is hidden when PrinterAccess is revoked via
    // sumatrapdfrestrict.ini (issue #5563); fall back to CmdOpenFile in that case.
    int anchorCmd = HasPermission(Perm::PrinterAccess) ? CmdPrint : CmdOpenFile;
    Rect r = TbGetRect(win->hwndToolbar, anchorCmd);
    int currX = r.x + r.dx + DpiScale(win->hwndFrame, 10);
    int currY = (r.y + r.dy - pageWndRect.dy) / 2;

    TempStr txt = nullptr;
    Size size2;
    Size minSize = HwndMeasureText(win->hwndPageTotal, "999 / 999");
    minSize.dx += padX;
    int labelDx = 0;
    if (-1 == pageCount || !pageCount) {
#if 0
        // for pageCount == -1: preserve hwndPageTotal's text and size
        txt = HwndGetTextTemp(win->hwndPageTotal);
        size2 = HwndClientRect(win->hwndPageTotal).Size();
        size2.dx -= padX;
        size2.dx -= DpiScale(win->hwndFrame, kButtonSpacingX);
#endif
        // hack: https://github.com/sumatrapdfreader/sumatrapdf/issues/4475
        txt = " ";
        minSize.dx = 0;
        size2.dx = 0;
    } else if (!win->ctrl || !win->ctrl->HasPageLabels()) {
        txt = fmt(" / %d", pageCount);
        size2 = HwndMeasureText(win->hwndPageTotal, txt);
        minSize.dx = size2.dx;
    } else {
        txt = fmt("%d / %d", win->ctrl->CurrentPageNo(), pageCount);
        // TempStr txt2 = fmt(" (%d / %d)", pageCount, pageCount);
        size2 = HwndMeasureText(win->hwndPageTotal, txt);
    }
    labelDx = size2.dx;
    size2.dx = std::max(size2.dx, minSize.dx);

    // Skip layout/repaint when an update-only refresh would not change anything
    // (page-label docs used to invalidate the whole toolbar on every call).
    TempStr prevTotal = HwndGetTextTemp(win->hwndPageTotal);
    bool textSame = prevTotal && txt && str::Eq(prevTotal, txt);
    if (updateOnly && textSame) {
        TBBUTTONINFOW bi0{};
        bi0.cbSize = sizeof(bi0);
        bi0.dwMask = TBIF_SIZE;
        TbGetButtonInfo(win->hwndToolbar, PageInfoId, &bi0);
        int wantCx = size2.dx + size.dx + pageWndRect.dx + 12;
        if (bi0.cx == wantCx) {
            return;
        }
    }

    HwndSetText(win->hwndPageTotal, txt);
    if (0 == size2.dx) {
        size2 = HwndMeasureText(win->hwndPageTotal, txt);
    }
    size2.dx += padX;
    size2.dx += DpiScale(win->hwndFrame, kButtonSpacingX);

    int padding = DpiGetSystemMetrics(win->hwndFrame, SM_CXEDGE);
    int x = currX - 1;
    int y = ((pageWndRect.dy - size.dy + 1) / 2) + currY;
    MoveWindow(win->hwndPageLabel, x, y, size.dx, size.dy, FALSE);
    if (IsUIRtl()) {
        currX += size2.dx;
        currX -= padX;
        currX -= DpiScale(win->hwndFrame, kButtonSpacingX);
    }
    x = currX + size.dx;
    y = currY;
    MoveWindow(win->hwndPageBg, x, y, pageWndRect.dx, pageWndRect.dy, FALSE);
    x = currX + size.dx + padding;
    y = ((pageWndRect.dy - size.dy + 1) / 2) + currY;
    int dx = pageWndRect.dx - (2 * padding);
    MoveWindow(win->hwndPageEdit, x, y, dx, size.dy, FALSE);
    // in right-to-left layout, the total comes "before" the current page number
    if (IsUIRtl()) {
        currX -= size2.dx;
        x = currX + size.dx;
        y = ((pageWndRect.dy - size.dy + 1) / 2) + currY;
        MoveWindow(win->hwndPageTotal, x, y, size2.dx, size.dy, FALSE);
    } else {
        x = currX + size.dx + pageWndRect.dx;
        int midX = (size2.dx - labelDx) / 2;
        y = ((pageWndRect.dy - size.dy + 1) / 2) + currY;
        MoveWindow(win->hwndPageTotal, x + midX, y, labelDx, size.dy, FALSE);
    }

    TBBUTTONINFOW bi{};
    bi.cbSize = sizeof(bi);
    bi.dwMask = TBIF_SIZE;
    TbGetButtonInfo(win->hwndToolbar, PageInfoId, &bi);
    size2.dx += size.dx + pageWndRect.dx + 12;
    if (bi.cx != size2.dx || !updateOnly) {
        TbSetButtonDx(win->hwndToolbar, PageInfoId, size2.dx);
    }
    HwndInvalidate(win->hwndToolbar, true);
}

static void CreatePageBox(MainWindow* win, HFONT font, int iconDy) {
    bool isRtl = IsUIRtl();

    auto* hwndFrame = win->hwndFrame;
    auto* hwndToolbar = win->hwndToolbar;
    // Measure a full page number plus edit-control padding; plain measure is
    // too tight for the right-aligned ES_NUMBER box (esp. under high DPI).
    int boxWidth = HwndMeasureText(hwndFrame, "999999", font).dx;
    boxWidth += 2 * DpiGetSystemMetrics(hwndFrame, SM_CXEDGE);
    boxWidth += DpiScale(hwndFrame, 12);
    DWORD style = WS_VISIBLE | WS_CHILD;
    auto* h = GetModuleHandle(nullptr);
    int dx = boxWidth;
    int dy = iconDy + 2;
    DWORD exStyle = 0;
    if (isRtl) exStyle |= WS_EX_LAYOUTRTL;

    HWND pageBg =
        CreateWindowExW(exStyle, WC_STATICW, L"", style, 0, 1, dx, dy, hwndToolbar, (HMENU) nullptr, h, nullptr);
    // capture the original static wndproc so WndProcEditBg can chain to it (it
    // does the actual WM_PAINT BeginPaint/EndPaint that validates the window).
    // This used to be captured in CreateFindBox; that box is gone, so do it here.
    if (!DefWndProcEditBg) {
        DefWndProcEditBg = (WNDPROC)GetWindowLongPtr(pageBg, GWLP_WNDPROC);
    }
    SetWindowLongPtr(pageBg, GWLP_WNDPROC, (LONG_PTR)WndProcEditBg);
    HWND label = CreateWindowExW(0, WC_STATICW, L"", style, 0, 1, 0, 0, hwndToolbar, (HMENU) nullptr, h, nullptr);
    HWND total = CreateWindowExW(0, WC_STATICW, L"", style, 0, 1, 0, 0, hwndToolbar, (HMENU) nullptr, h, nullptr);

    style = WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL | ES_NUMBER | ES_RIGHT;
    dx = boxWidth - DpiScale(hwndFrame, 4); // 4 pixels padding on the right side of the text box
    dy = iconDy;
    exStyle = 0;
    if (isRtl) exStyle |= WS_EX_LAYOUTRTL;
    HWND page = CreateWindowExW(exStyle, WC_EDIT, L"0", style, 0, 1, dx, dy, hwndToolbar, (HMENU) nullptr, h, nullptr);

    SetWindowFont(label, font, FALSE);
    SetWindowFont(page, font, FALSE);
    SetWindowFont(total, font, FALSE);

    if (!DefWndProcPageBox) {
        DefWndProcPageBox = (WNDPROC)GetWindowLongPtr(page, GWLP_WNDPROC);
    }
    SetWindowLongPtr(page, GWLP_WNDPROC, (LONG_PTR)WndProcPageBox);

    win->hwndPageLabel = label;
    win->hwndPageEdit = page;
    win->hwndPageBg = pageBg;
    win->hwndPageTotal = total;
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
            tbi.bmpIndex = TbIcon::None;
            tbi.cmdId = shortcut->cmdId;
            tbi.svgIcon = shortcut->toolbarSvgIcon;
            tbi.toolTip = ShortcutToolbarToolTipTemp(shortcut);
            gCustomButtons[gCustomButtonsCount++] = tbi;
            continue;
        }
        if (!str::IsEmptyOrWhiteSpace(shortcut->toolbarText)) {
            ToolbarButtonInfo tbi;
            tbi.bmpIndex = TbIcon::Text;
            tbi.cmdId = shortcut->cmdId;
            tbi.toolTip = shortcut->toolbarText;
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
            tbi.bmpIndex = TbIcon::None;
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
        tbi.bmpIndex = TbIcon::Text;
        tbi.cmdId = cc->id;
        tbi.toolTip = tbText;
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
static fz_pixmap* RenderSvgIconPixmap(fz_context* ctx, Str svgData, int dx, int dy, COLORREF fgCol, COLORREF bgCol) {
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

static void BlitPixmap(u8* dstSamples, ptrdiff_t dstStride, fz_pixmap* src, int dstX, int dstY, COLORREF bgCol) {
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
static void ClearIconSlot(u8* dstSamples, ptrdiff_t dstStride, int dx, int dy, int dstX, COLORREF bgCol) {
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

static HBITMAP BuildIconsBitmap(int dx, int dy, Str* customSvgs, int customCount) {
    fz_context* ctx = fz_new_context_windows();
    int nBuiltIn = (int)TbIcon::kMax;
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

    COLORREF fgCol = ThemeWindowTextColor();
    COLORREF bgCol = ThemeControlBackgroundColor();
    for (int i = 0; i < nBuiltIn; i++) {
        Str svgData = GetSvgIcon((TbIcon)i);
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

static int SetToolbarIconsImageList(MainWindow* win) {
    HWND hwndToolbar = win->hwndToolbar;
    HWND hwndParent = GetParent(hwndToolbar);

    // we call it ToolbarSize for users, but it's really size of the icon
    // toolbar size is iconSize + padding (seems to be 6)
    // the setting is a size at 100% scaling, so it's scaled to the window's dpi
    int iconSize = DpiScale(hwndParent, gGlobalPrefs->toolbarSize);
    // icon sizes must be multiple of 4 or else they are sheared
    // TODO: I must be doing something wrong, any size should be ok
    // it might be about size of buttons / bitmaps
    iconSize = RoundUp(iconSize, 4);
    int dx = iconSize;
    // this doesn't seem to be required and doesn't help with weird sizes like 22
    // but the docs say to do it
    TbSetBitmapSize(hwndToolbar, Size(dx, dx));

    Str customSvgs[kMaxCustomButtons];
    int customCount = 0;
    int nBuiltIn = (int)TbIcon::kMax;
    for (int i = 0; i < gCustomButtonsCount; i++) {
        Str svg = gCustomButtons[i].svgIcon;
        if (str::IsEmptyOrWhiteSpace(svg)) {
            continue;
        }
        gCustomButtons[i].bmpIndex = (TbIcon)(nBuiltIn + customCount);
        customSvgs[customCount++] = svg;
    }

    // assume square icons
    HIMAGELIST himl = ImageList_Create(dx, dx, ILC_COLOR32, nBuiltIn + customCount, 0);
    HBITMAP hbmp = BuildIconsBitmap(dx, dx, customSvgs, customCount);
    ImageList_Add(himl, hbmp, nullptr);
    DeleteObject(hbmp);
    // Replace (and free) the previous list so theme / size changes do not leak
    HIMAGELIST oldHiml = TbSetImageList(hwndToolbar, himl);
    if (oldHiml) {
        ImageList_Destroy(oldHiml);
    }
    return iconSize;
}

void UpdateToolbarAfterThemeChange(MainWindow* win) {
    SetToolbarIconsImageList(win);
    HwndScheduleRepaint(win->hwndToolbar);
}

// build an image list with all the standard toolbar icons; the FindBar uses
// this for its own small toolbar (chevrons, match-case, close). Caller owns
// the returned HIMAGELIST.
HIMAGELIST BuildStdToolbarImageList(int dx) {
    HIMAGELIST himl = ImageList_Create(dx, dx, ILC_COLOR32, (int)TbIcon::kMax, 0);
    HBITMAP hbmp = BuildIconsBitmap(dx, dx, nullptr, 0);
    ImageList_Add(himl, hbmp, nullptr);
    DeleteObject(hbmp);
    return himl;
}

// screen-coordinates rect of a toolbar button, used to position the FindBar.
// returns an empty rect when the toolbar isn't visible (e.g. fullscreen /
// presentation) so the caller can fall back to a different anchor.
Rect GetToolbarButtonScreenRect(MainWindow* win, int cmdId) {
    if (!win->hwndToolbar || !HwndIsVisible(win->hwndToolbar)) {
        return {};
    }
    Rect r = TbGetRect(win->hwndToolbar, cmdId);
    return HwndMapRectToWindow(r, win->hwndToolbar, HWND_DESKTOP);
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
    HWND hwnd = win->hwndToolbar;
    int n = (int)SendMessageW(hwnd, TB_BUTTONCOUNT, 0, 0);
    HWND hwndTt = (HWND)SendMessageW(hwnd, TB_GETTOOLTIPS, 0, 0);
    int nTools = hwndTt ? (int)SendMessageW(hwndTt, TTM_GETTOOLCOUNT, 0, 0) : -1;
    out.Append(fmt("buttons=%d tooltipTools=%d\n", n, nTools));
    for (int i = 0; i < n; i++) {
        WCHAR buf[512]{};
        TBBUTTONINFOW bi{};
        bi.cbSize = sizeof(bi);
        bi.dwMask = TBIF_BYINDEX | TBIF_COMMAND | TBIF_TEXT | TBIF_STATE;
        bi.pszText = buf;
        bi.cchText = dimof(buf);
        TbGetButtonInfo(hwnd, i, &bi);
        RECT r{};
        SendMessageW(hwnd, TB_GETITEMRECT, (WPARAM)i, (LPARAM)&r);
        bool hidden = (bi.fsState & TBSTATE_HIDDEN) != 0;
        out.Append(fmt("idx=%d cmd=%d hidden=%d rect=%d,%d,%d,%d text=%s\n", i, (int)bi.idCommand, hidden ? 1 : 0,
                       (int)r.left, (int)r.top, (int)r.right, (int)r.bottom, ToUtf8Temp(buf)));
    }
    for (int i = 0; i < nTools; i++) {
        TTTOOLINFOW ti{};
        ti.cbSize = sizeof(ti);
        if (!SendMessageW(hwndTt, TTM_ENUMTOOLSW, (WPARAM)i, (LPARAM)&ti)) {
            continue;
        }
        out.Append(fmt("tool=%d uid=%d rect=%d,%d,%d,%d\n", i, (int)ti.uId, (int)ti.rect.left, (int)ti.rect.top,
                       (int)ti.rect.right, (int)ti.rect.bottom));
    }
    *exitCodeOut = 0;
    return ToStrTemp(out);
}

// https://docs.microsoft.com/en-us/windows/win32/controls/toolbar-control-reference
void CreateToolbar(MainWindow* win) {
    bool isRtl = IsUIRtl();

    kButtonSpacingX = 0;
    HINSTANCE hinst = GetModuleHandle(nullptr);
    HWND hwndParent = win->hwndFrame;

    // WS_CLIPSIBLINGS so that in overlay mode the canvas (a lower-Z sibling)
    // doesn't paint over the floating toolbar
    DWORD style = WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | RBS_VARHEIGHT;
    if (IsCurrentThemeDefault()) {
        style |= WS_BORDER | RBS_BANDBORDERS;
    }
    style |= CCS_NODIVIDER | CCS_NOPARENTALIGN | WS_VISIBLE;
    DWORD exStyle = WS_EX_TOOLWINDOW;
    if (isRtl) exStyle |= WS_EX_LAYOUTRTL;

    win->hwndReBar = CreateWindowExW(exStyle, REBARCLASSNAME, nullptr, style, 0, 0, 0, 0, hwndParent, (HMENU)IDC_REBAR,
                                     hinst, nullptr);
    SetWindowSubclass(win->hwndReBar, ReBarWndProc, 0, 0);

    REBARINFO rbi{};
    rbi.cbSize = sizeof(REBARINFO);
    rbi.fMask = 0;
    rbi.himl = (HIMAGELIST) nullptr;
    SendMessageW(win->hwndReBar, RB_SETBARINFO, 0, (LPARAM)&rbi);
    if (!IsCurrentThemeDefault()) {
        SendMessageW(win->hwndReBar, RB_SETBKCOLOR, 0, ThemeControlBackgroundColor());
    }

    style = WS_CHILD | WS_CLIPSIBLINGS | TBSTYLE_TOOLTIPS | TBSTYLE_FLAT;
    style |= TBSTYLE_LIST | CCS_NODIVIDER | CCS_NOPARENTALIGN;
    exStyle = 0;
    if (isRtl) exStyle |= WS_EX_LAYOUTRTL;
    HMENU cmd = (HMENU)IDC_TOOLBAR;
    HWND hwndToolbar =
        CreateWindowExW(exStyle, TOOLBARCLASSNAME, nullptr, style, 0, 0, 0, 0, win->hwndReBar, cmd, hinst, nullptr);
    win->hwndToolbar = hwndToolbar;
    TbSetButtonStructSize(hwndToolbar, sizeofi(TBBUTTON));

    if (!UseDarkModeLib() || !DarkMode::isEnabled()) {
        if (!IsCurrentThemeDefault()) {
            // without this custom draw code doesn't work
            SetWindowTheme(hwndToolbar, L"", L"");
        }
    }

    if (UseDarkModeLib()) {
        DarkMode::setWindowNotifyCustomDrawSubclass(win->hwndReBar);
    }

    PopulateCustomToolbarButtons();
    int iconSize = SetToolbarIconsImageList(win);

    TBMETRICS tbMetrics{};
    tbMetrics.cbSize = sizeof(tbMetrics);
    // tbMetrics.dwMask = TBMF_PAD;
    tbMetrics.dwMask = TBMF_BUTTONSPACING;
    TbGetMetrics(hwndToolbar, &tbMetrics);
    int yPad = DpiScale(win->hwndFrame, 2);
    tbMetrics.cxPad += DpiScale(win->hwndFrame, 14);
    tbMetrics.cyPad += yPad;
    tbMetrics.cxButtonSpacing += DpiScale(win->hwndFrame, kButtonSpacingX);
    // tbMetrics.cyButtonSpacing += DpiScale(win->hwndFrame, 4);
    TbSetMetrics(hwndToolbar, &tbMetrics);

    DWORD exstyle = TbGetExtendedStyle(hwndToolbar);
    exstyle |= TBSTYLE_EX_MIXEDBUTTONS;
    exstyle |= TBSTYLE_EX_DRAWDDARROWS;
    TbSetExtendedStyle(hwndToolbar, exstyle);

    TBBUTTON tbButtons[kButtonsCount];
    for (int i = 0; i < kButtonsCount; i++) {
        const ToolbarButtonInfo& bi = gToolbarButtons[i];
        tbButtons[i] = TbButtonFromButtonInfo(bi);
    }
    TbAddButtons(hwndToolbar, kButtonsCount, tbButtons);

    TBBUTTON* buttons = AllocArrayTemp<TBBUTTON>(gCustomButtonsCount);
    for (int i = 0; i < gCustomButtonsCount; i++) {
        ToolbarButtonInfo& tbi = gCustomButtons[i];
        buttons[i] = TbButtonFromButtonInfo(tbi, true);
    }
    TbAddButtons(hwndToolbar, gCustomButtonsCount, buttons);
    TbSetButtonSize(hwndToolbar, Size(iconSize, iconSize));

    Rect rc = TbGetItemRect(hwndToolbar, 0);

    ShowWindow(hwndToolbar, SW_SHOW);

    REBARBANDINFOW rbBand{};
    rbBand.cbSize = sizeof(REBARBANDINFOW);
    rbBand.fMask = RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE;
    rbBand.fStyle = RBBS_FIXEDSIZE;
    if (IsAppThemed() && IsCurrentThemeDefault()) {
        rbBand.fStyle |= RBBS_CHILDEDGE;
    }
    rbBand.hbmBack = nullptr;
    rbBand.lpText = (WCHAR*)L"Toolbar"; // NOLINT
    rbBand.hwndChild = hwndToolbar;
    rbBand.cxMinChild = rc.dx * kButtonsCount;
    rbBand.cyMinChild = rc.dy + (2 * rc.y);
    rbBand.cx = 0;
    SendMessageW(win->hwndReBar, RB_INSERTBAND, (WPARAM)-1, (LPARAM)&rbBand);

    SetWindowPos(win->hwndReBar, nullptr, 0, 0, 0, 0, SWP_NOZORDER);

    int defFontSize = GetAppFontSize(win->hwndFrame);
    // ToolbarSize scales icons only; UI font size comes from UIFontSize (GetAppFontSize).
    int newSize = defFontSize;
    int maxFontSize = iconSize - (yPad * 2) - 2; // -2 determined empirically
    if (newSize > maxFontSize) {
        logfa("CreateToolbar: setting toolbar font size to %d (scaled was %d, default size: %d)\n", maxFontSize,
              newSize, defFontSize);
        newSize = maxFontSize;
    } else {
        logfa("CreateToolbar: setting toolbar font size to %d (default size: %d)\n", newSize, defFontSize);
    }
    auto* font = GetDefaultGuiFontOfSize(newSize);
    HwndSetFont(hwndToolbar, font);

    CreatePageBox(win, font, iconSize);
    SubclassToolbar(win);

    // a document can already be loaded when we're re-creating the toolbar
    // (settings reload, DPI or RTL change), so restore the page box instead
    // of leaving it blank until the next page change
    DocController* ctrl = win->ctrl;
    UpdateToolbarPageText(win, ctrl ? ctrl->PageCount() : -1);
    if (ctrl) {
        TempStr label = ctrl->GetPageLabeTemp(ctrl->CurrentPageNo());
        HwndSetText(win->hwndPageEdit, label);
        // the box is created with ES_NUMBER; docs with page labels need it off
        HwndSetWindowStyle(win->hwndPageEdit, ES_NUMBER, !ctrl->HasPageLabels());
    }
    UpdateToolbarFindText(win);
}

void ReCreateToolbar(MainWindow* win) {
    if (win->hwndReBar) {
        HwndDestroyWindowSafe(&win->hwndPageLabel);
        HwndDestroyWindowSafe(&win->hwndPageEdit);
        HwndDestroyWindowSafe(&win->hwndPageBg);
        HwndDestroyWindowSafe(&win->hwndPageTotal);
        HwndDestroyWindowSafe(&win->hwndToolbar);
        HwndDestroyWindowSafe(&win->hwndReBar);
    }
    CreateToolbar(win);
}

static int MenuBarToolbarIdealDy(MainWindow* win) {
    HFONT font = GetAppMenuFont(win->hwndFrame);
    int dy = FontDyPx(win->hwndFrame, font) + DpiScale(win->hwndFrame, 4);
    int minDy = DpiScale(win->hwndFrame, kTabBarDy);
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
        COLORREF bgCol = ThemeControlBackgroundColor();
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

    HFONT font = GetAppMenuFont(win->hwndFrame);
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
