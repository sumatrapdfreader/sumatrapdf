/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

extern "C" {
#include <mupdf/fitz.h>
}

#include "base/Base.h"
#include "base/WinDynCalls.h"
#include "base/Dpi.h"
#include "base/Win.h"
#include "base/BitManip.h"

#include "wingui/UIModels.h"

#include "Accelerators.h"
#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "FzImgReader.h"
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
    Str svgIcon = {};
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
int gCustomButtonsCount = 0;

static bool SkipBuiltInButton(const ToolbarButtonInfo& tbi) {
    return tbi.bmpIndex == TbIcon::None;
}

static void UpdateToolbarButtonStateByIdx(HWND hwnd, int idx, bool set, BYTE flag) {
    TBBUTTONINFOW bi{};
    bi.cbSize = sizeof(bi);
    bi.dwMask = TBIF_BYINDEX | TBIF_STATE;
    SendMessageW(hwnd, TB_GETBUTTONINFOW, idx, (LPARAM)&bi);
    BYTE newState = (BYTE)(set ? bi.fsState | flag : bi.fsState & ~flag);
    if (newState == bi.fsState) {
        // TB_SETBUTTONINFOW repaints the button even when nothing changes, which
        // flickers the toolbar (and the page-number controls floating over it)
        // e.g. on every page change while drag-selecting. Skip the no-op.
        return;
    }
    bi.fsState = newState;
    SendMessageW(hwnd, TB_SETBUTTONINFOW, idx, (LPARAM)&bi);
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
        auto cmd = FindCustomCommand(tbCmdId);
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
    TbSetButtonInfoById(hwndToolbar, cmd, &bi);
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
        case PageInfoId:
            return true;
    }
    auto ctx = NewBuildMenuCtx(win->CurrentTab(), Point{0, 0});
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
    auto ctx = NewBuildMenuCtx(win->CurrentTab(), Point{0, 0});
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
        auto cmd = FindCustomCommand(cmdId);
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
        case CmdFindPrev:
            // TODO: Update on whether there's more to find, not just on whether there is text.
            return win->hwndFindEdit && HwndGetTextLen(win->hwndFindEdit) > 0;

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
        TbSetButtonInfoById(hwnd, buttonId, &binfo);
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
            TbSetButtonInfoById(hwnd, buttonId, &binfo);
        }
    }
#endif
}

static void SetToolbarButtonImageByIdx(HWND hwnd, int idx, TbIcon icon) {
    TBBUTTONINFOW bi{};
    bi.cbSize = sizeof(bi);
    bi.dwMask = TBIF_BYINDEX | TBIF_IMAGE;
    bi.iImage = (int)icon;
    SendMessageW(hwnd, TB_SETBUTTONINFOW, idx, (LPARAM)&bi);
}

// sets button text, which the toolbar shows as its tooltip
static void SetToolbarButtonToolTipByIdx(HWND hwnd, int idx, int cmdId, Str s) {
    TempStr accelStr = AppendAccelKeyToMenuStringTemp(nullptr, cmdId);
    if (accelStr) {
        Str accel = accelStr.len > 1 ? Str(accelStr.s + 1, accelStr.len - 1) : accelStr;
        TempStr s2 = fmt(" (%s)", accel);
        s = str::JoinTemp(s, s2);
    }
    TBBUTTONINFOW bi{};
    bi.cbSize = sizeof(bi);
    bi.dwMask = TBIF_BYINDEX | TBIF_TEXT;
    bi.pszText = CWStrTemp(s);
    SendMessageW(hwnd, TB_SETBUTTONINFOW, idx, (LPARAM)&bi);
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
            // update tooltip before SetTabDirty (which rebuilds tooltips via LayoutTabs)
            TabInfo* ti = win->tabsCtrl->GetTab(i);
            if (ti && tab && tab->filePath) {
                Str path = tab->filePath;
                if (dirty) {
                    TempStr tooltip = str::JoinTemp(path, StrL(" "), _TRA("(unsaved annotations)"));
                    str::ReplaceWithCopy(&ti->tooltip, tooltip);
                } else {
                    str::ReplaceWithCopy(&ti->tooltip, path);
                }
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
// whether the current window context (presentation, fullscreen, about page)
// permits showing the toolbar at all, independent of the show/hide/overlay mode
static bool ToolbarContextAllows(MainWindow* win) {
    if (win->presentation) {
        return false;
    }
    if (win->isFullScreen) {
        return gGlobalPrefs->fullscreen.showToolbar;
    }
    return true;
}

bool ShouldShowToolbar(MainWindow* win) {
    if (win->isFullScreen) {
        // fullscreen has its own pinned toggle (fullscreen.showToolbar)
        return ToolbarContextAllows(win);
    }
    if (ToolbarModeIsHidden() || ToolbarModeIsOverlay()) {
        return false;
    }
    return ToolbarContextAllows(win);
}

bool ShouldOverlayToolbar(MainWindow* win) {
    if (win->isFullScreen) {
        return false;
    }
    if (!ToolbarModeIsOverlay()) {
        return false;
    }
    // don't float the overlay toolbar over the home / about page (only the
    // pinned "show" mode shows a toolbar there)
    if (win->IsCurrentTabAbout()) {
        return false;
    }
    return ToolbarContextAllows(win);
}

// natural width of the toolbar content (buttons + page box); the find bar
// floats separately so the page-total label is the rightmost element
static int ToolbarNaturalWidth(MainWindow* win) {
    if (!win->hwndReBar || !win->hwndToolbar) {
        return 0;
    }
    Rect rRebar = WindowRect(win->hwndReBar);
    Rect rTb = WindowRect(win->hwndToolbar);
    int contentRight = rTb.x; // screen x of the rightmost content edge
    SIZE tbSz{};
    SendMessageW(win->hwndToolbar, TB_GETMAXSIZE, 0, (LPARAM)&tbSz);
    contentRight = std::max(contentRight, rTb.x + (int)tbSz.cx);
    if (win->hwndPageTotal && IsWindowVisible(win->hwndPageTotal)) {
        Rect rpt = WindowRect(win->hwndPageTotal);
        contentRight = std::max(contentRight, rpt.x + rpt.dx);
    }
    int natW = (contentRight - rRebar.x) + DpiScale(win->hwndFrame, 12);
    return natW;
}

// canvas rectangle in frame-client coordinates
static Rect CanvasRectInFrame(MainWindow* win) {
    RECT rc{};
    GetWindowRect(win->hwndCanvas, &rc);
    POINT tl{rc.left, rc.top};
    ScreenToClient(win->hwndFrame, &tl);
    return Rect(tl.x, tl.y, rc.right - rc.left, rc.bottom - rc.top);
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
    return GetSystemMetrics(SM_CYHSCROLL);
}

// rectangle (frame-client coords) the overlay toolbar occupies when shown
static Rect OverlayToolbarRect(MainWindow* win) {
    Rect canvas = CanvasRectInFrame(win);
    int natW = ToolbarNaturalWidth(win);
    if (natW <= 0 || natW > canvas.dx) {
        natW = canvas.dx;
    }
    int h = WindowRect(win->hwndReBar).dy;
    int x = canvas.x + (canvas.dx - natW) / 2;
    int y = canvas.y;
    if (ToolbarAtBottom()) {
        y = canvas.y + canvas.dy - h - OverlayToolbarBottomScrollbarOffset(win);
    }
    return Rect(x, y, natW, h);
}

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
        RECT rc = ToRECT(r);
        InvalidateRect(win->hwndCanvas, nullptr, FALSE);
        InvalidateRect(win->hwndFrame, &rc, FALSE);
    }
}

// whether the cursor is currently in the reveal band or over the toolbar
static bool OverlayToolbarShouldShowForCursor(MainWindow* win) {
    POINT pt{};
    GetCursorPos(&pt);
    POINT ptFrame = pt;
    ScreenToClient(win->hwndFrame, &ptFrame);

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
    HWND hwndUnder = WindowFromPoint(pt);
    bool overToolbar = hwndUnder && (hwndUnder == win->hwndReBar || hwndUnder == win->hwndToolbar ||
                                     IsChild(win->hwndReBar, hwndUnder));
    return inBand || overToolbar;
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

void UpdateOverlayToolbarForMouse(MainWindow* win) {
    if (!win->isToolbarOverlay || !win->hwndReBar) {
        return;
    }
    bool show = OverlayToolbarShouldShowForCursor(win);
    if (show) {
        CancelOverlayHide(win);
        SetOverlayShown(win, true);
    } else if (win->toolbarOverlayShown) {
        // don't hide immediately; give the user kDelayToolbarHide to come back
        ScheduleOverlayHide(win);
    }
}

void OverlayToolbarHideTimerFired(MainWindow* win) {
    win->toolbarOverlayHidePending = false;
    KillTimer(win->hwndFrame, kHideOverlayToolbarTimerId);
    if (!win->isToolbarOverlay) {
        return;
    }
    // if the cursor came back near the top while the timer was pending, keep
    // the toolbar shown; otherwise hide it now
    if (OverlayToolbarShouldShowForCursor(win)) {
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
    RelayoutWindow(win);
    if (enteredOverlay) {
        ScheduleOverlayHide(win);
    }
}

void UpdateFindbox(MainWindow* win) {
    // remove SS_WHITERECT so WM_CTLCOLORSTATIC controls the background color
    SetWindowStyle(win->hwndPageBg, SS_WHITERECT, false);

    InvalidateRect(win->hwndToolbar, nullptr, TRUE);
    if (IsWindowVisible(win->hwndFrame)) {
        UpdateWindow(win->hwndToolbar);
    }

    auto cursorId = win->IsDocLoaded() ? IDC_IBEAM : IDC_ARROW;
    if (win->hwndFindEdit) {
        SetClassLongPtrW(win->hwndFindEdit, GCLP_HCURSOR, (LONG_PTR)GetCachedCursor(cursorId));
    }
}

LRESULT CALLBACK ReBarWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass,
                              DWORD_PTR dwRefData) {
    if (WM_ERASEBKGND == uMsg && ThemeColorizeControls()) {
        HDC hdc = (HDC)wParam;
        RECT rect;
        GetClientRect(hWnd, &rect);
        SetTextColor(hdc, ThemeWindowTextColor());
        COLORREF bgCol = ThemeControlBackgroundColor();
        SetBkColor(hdc, bgCol);
        auto bgBrush = CreateSolidBrush(bgCol);
        FillRect(hdc, &rect, bgBrush);
        DeleteObject(bgBrush);
        return 1;
    }
    if (WM_NOTIFY == uMsg) {
        auto win = FindMainWindowByHwnd(hWnd);
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
                        // col = RGB(255, 0, 0);
                        // SetTextColor(custDraw->nmcd.hdc, col);
                        UINT itemState = custDraw->nmcd.uItemState;
                        if (itemState & CDIS_DISABLED) {
                            // TODO: this doesn't work
                            col = ThemeWindowTextDisabledColor();
                            // col = RGB(255, 0, 0);
                            custDraw->clrText = col;
                        } else if (false && itemState & CDIS_SELECTED) {
                            custDraw->clrText = RGB(0, 255, 0);
                        } else if (false && itemState & CDIS_GRAYED) {
                            custDraw->clrText = RGB(0, 0, 255);
                        } else {
                            custDraw->clrText = col;
                        }
                        return CDRF_DODEFAULT;
                        // return CDRF_NEWFONT;
                    }
                }
            }
        }
    }
    // allow window dragging from empty rebar area (main toolbar)
    if (WM_LBUTTONDOWN == uMsg) {
        auto win = FindMainWindowByHwnd(hWnd);
        if (win && win->tabsInTitlebar) {
            HWND hwndFrame = GetAncestor(hWnd, GA_ROOT);
            ReleaseCapture();
            SendMessageW(hwndFrame, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            return 0;
        }
    }
    if (WM_LBUTTONDBLCLK == uMsg) {
        auto win = FindMainWindowByHwnd(hWnd);
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
        auto win = FindMainWindowByHwnd(hWnd);
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
        RECT rc;
        GetClientRect(hwnd, &rc);
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
            if ((isBgCtrl || isEditCtrl) && !ThemeColorizeControls()) {
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
            POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            int idx = (int)SendMessageW(hwnd, TB_HITTEST, 0, (LPARAM)&pt);
            if (idx < 0) {
                // also check we're not over a child control (find box, page box)
                POINT ptScreen = pt;
                ClientToScreen(hwnd, &ptScreen);
                HWND childAtPoint = ChildWindowFromPoint(hwnd, pt);
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
    HWND hwnd = win->hwndToolbar;
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
                }
                return 1;
            }
            case VK_ESCAPE:
                HwndSetFocus(win->hwndFrame);
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

    Rect pageWndRect = WindowRect(win->hwndPageBg);

    // TB_GETRECT fails for hidden buttons, so anchor on a button that's still
    // visible. CmdPrint is hidden when PrinterAccess is revoked via
    // sumatrapdfrestrict.ini (issue #5563); fall back to CmdOpenFile in that case.
    RECT r{};
    int anchorCmd = HasPermission(Perm::PrinterAccess) ? CmdPrint : CmdOpenFile;
    SendMessageW(win->hwndToolbar, TB_GETRECT, anchorCmd, (LPARAM)&r);
    int currX = r.right + DpiScale(win->hwndFrame, 10);
    int currY = (r.bottom - pageWndRect.dy) / 2;

    TempStr txt = nullptr;
    Size size2;
    Size minSize = HwndMeasureText(win->hwndPageTotal, "999 / 999");
    minSize.dx += padX;
    int labelDx = 0;
    if (-1 == pageCount) {
#if 0
        // preserve hwndPageTotal's text and size
        txt = HwndGetTextTemp(win->hwndPageTotal);
        size2 = ClientRect(win->hwndPageTotal).Size();
        size2.dx -= padX;
        size2.dx -= DpiScale(win->hwndFrame, kButtonSpacingX);
#endif
        // hack: https://github.com/sumatrapdfreader/sumatrapdf/issues/4475
        txt = " ";
        minSize.dx = 0;
        size2.dx = 0;
    } else if (!pageCount) {
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

    HwndSetText(win->hwndPageTotal, txt);
    if (0 == size2.dx) {
        size2 = HwndMeasureText(win->hwndPageTotal, txt);
    }
    size2.dx += padX;
    size2.dx += DpiScale(win->hwndFrame, kButtonSpacingX);

    int padding = GetSystemMetrics(SM_CXEDGE);
    int x = currX - 1;
    int y = (pageWndRect.dy - size.dy + 1) / 2 + currY;
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
    y = (pageWndRect.dy - size.dy + 1) / 2 + currY;
    int dx = pageWndRect.dx - 2 * padding;
    MoveWindow(win->hwndPageEdit, x, y, dx, size.dy, FALSE);
    // in right-to-left layout, the total comes "before" the current page number
    if (IsUIRtl()) {
        currX -= size2.dx;
        x = currX + size.dx;
        y = (pageWndRect.dy - size.dy + 1) / 2 + currY;
        MoveWindow(win->hwndPageTotal, x, y, size2.dx, size.dy, FALSE);
    } else {
        x = currX + size.dx + pageWndRect.dx;
        int midX = (size2.dx - labelDx) / 2;
        y = (pageWndRect.dy - size.dy + 1) / 2 + currY;
        MoveWindow(win->hwndPageTotal, x + midX, y, labelDx, size.dy, FALSE);
    }

    TBBUTTONINFOW bi{};
    bi.cbSize = sizeof(bi);
    bi.dwMask = TBIF_SIZE;
    SendMessageW(win->hwndToolbar, TB_GETBUTTONINFO, PageInfoId, (LPARAM)&bi);
    size2.dx += size.dx + pageWndRect.dx + 12;
    if (bi.cx != size2.dx || !updateOnly) {
        TbSetButtonDx(win->hwndToolbar, PageInfoId, size2.dx);
    }
    InvalidateRect(win->hwndToolbar, nullptr, TRUE);
}

static void CreatePageBox(MainWindow* win, HFONT font, int iconDy) {
    bool isRtl = IsUIRtl();

    auto hwndFrame = win->hwndFrame;
    auto hwndToolbar = win->hwndToolbar;
    // TODO: this is broken, result is way too small
    int boxWidth = HwndMeasureText(hwndFrame, "999999", font).dx;
    DWORD style = WS_VISIBLE | WS_CHILD;
    auto h = GetModuleHandle(nullptr);
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

void LogBitmapInfo(HBITMAP hbmp) {
    BITMAP bmpInfo;
    GetObject(hbmp, sizeof(BITMAP), &bmpInfo);
    logf("dx: %d, dy: %d, stride: %d, bitsPerPixel: %d\n", (int)bmpInfo.bmWidth, (int)bmpInfo.bmHeight,
         (int)bmpInfo.bmWidthBytes, (int)bmpInfo.bmBitsPixel);
    u8* bits = (u8*)bmpInfo.bmBits;
    u8* d;
    for (int y = 0; y < 5; y++) {
        d = bits + (size_t)bmpInfo.bmWidthBytes * y;
        logf("y: %d, d: 0x%p\n", y, d);
    }
}

static const TempStr ShortcutToolbarToolTipTemp(Shortcut* shortcut) {
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

    // add toolbar buttons from custom commands with toolbar settings (e.g. ExternalViewers)
    for (auto cc = gFirstCustomCommand; cc; cc = cc->next) {
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

static fz_pixmap* RenderSvgIconPixmap(fz_context* ctx, Str svgData, int dx, int dy, COLORREF fgCol, COLORREF bgCol) {
    TempStr strokeCol = SerializeColorTemp(fgCol);
    TempStr fillCol = SerializeColorTemp(bgCol);
    TempStr fillColRepl = str::JoinTemp(StrL("fill=\""), fillCol, StrL("\""));
    TempStr svg = str::ReplaceTemp(svgData, StrL("currentColor"), strokeCol);
    svg = str::ReplaceTemp(svg, StrL(R"(fill="none")"), fillColRepl);
    fz_buffer* buf = fz_new_buffer_from_copied_data(ctx, (u8*)svg.s, svg.len);
    fz_image* image = fz_new_image_from_svg(ctx, buf, nullptr, nullptr);
    image->w = dx;
    image->h = dy;
    fz_pixmap* pixmap = fz_get_pixmap_from_image(ctx, image, nullptr, nullptr, nullptr, nullptr);
    fz_drop_image(ctx, image);
    fz_drop_buffer(ctx, buf);
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
        dstStride = destDx * n;
        int imgSize = (int)dstStride * h;
        int bitsCount = n * 8;

        int bmiSize = (int)(sizeof(BITMAPINFO) + 255 * sizeof(RGBQUAD));
        auto bmi = (BITMAPINFO*)AllocArrayTemp<u8>(bmiSize);
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
        BlitPixmap(hbmpData, dstStride, pixmap, dx * i, 0, bgCol);
        fz_drop_pixmap(ctx, pixmap);
    }
    for (int i = 0; i < customCount; i++) {
        fz_pixmap* pixmap = RenderSvgIconPixmap(ctx, customSvgs[i], dx, dy, fgCol, bgCol);
        BlitPixmap(hbmpData, dstStride, pixmap, dx * (nBuiltIn + i), 0, bgCol);
        fz_drop_pixmap(ctx, pixmap);
    }

    fz_drop_context_windows(ctx);
    return hbmp;
}

constexpr int kDefaultIconSize = 18;

static int SetToolbarIconsImageList(MainWindow* win) {
    HWND hwndToolbar = win->hwndToolbar;
    HWND hwndParent = GetParent(hwndToolbar);

    // we call it ToolbarSize for users, but it's really size of the icon
    // toolbar size is iconSize + padding (seems to be 6)
    int iconSize = gGlobalPrefs->toolbarSize;
    if (iconSize == kDefaultIconSize) {
        // scale if default size
        iconSize = DpiScale(hwndParent, iconSize);
    }
    // icon sizes must be multiple of 4 or else they are sheared
    // TODO: I must be doing something wrong, any size should be ok
    // it might be about size of buttons / bitmaps
    iconSize = RoundUp(iconSize, 4);
    int dx = iconSize;
    // this doesn't seem to be required and doesn't help with weird sizes like 22
    // but the docs say to do it
    SendMessage(hwndToolbar, TB_SETBITMAPSIZE, 0, (LPARAM)MAKELONG(dx, dx));

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
    SendMessageW(hwndToolbar, TB_SETIMAGELIST, 0, (LPARAM)himl);
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
    RECT r{};
    if (!win->hwndToolbar || !IsWindowVisible(win->hwndToolbar)) {
        return {};
    }
    TbGetRectById(win->hwndToolbar, cmdId, &r);
    MapWindowPoints(win->hwndToolbar, HWND_DESKTOP, (POINT*)&r, 2);
    return Rect{r.left, r.top, r.right - r.left, r.bottom - r.top};
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
    SendMessageW(hwndToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);

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

    LRESULT exstyle = SendMessageW(hwndToolbar, TB_GETEXTENDEDSTYLE, 0, 0);
    exstyle |= TBSTYLE_EX_MIXEDBUTTONS;
    exstyle |= TBSTYLE_EX_DRAWDDARROWS;
    SendMessageW(hwndToolbar, TB_SETEXTENDEDSTYLE, 0, exstyle);

    TBBUTTON tbButtons[kButtonsCount];
    for (int i = 0; i < kButtonsCount; i++) {
        const ToolbarButtonInfo& bi = gToolbarButtons[i];
        tbButtons[i] = TbButtonFromButtonInfo(bi);
    }
    SendMessageW(hwndToolbar, TB_ADDBUTTONS, kButtonsCount, (LPARAM)tbButtons);

    TBBUTTON* buttons = AllocArrayTemp<TBBUTTON>(gCustomButtonsCount);
    for (int i = 0; i < gCustomButtonsCount; i++) {
        ToolbarButtonInfo& tbi = gCustomButtons[i];
        buttons[i] = TbButtonFromButtonInfo(tbi, true);
    }
    SendMessageW(hwndToolbar, TB_ADDBUTTONS, gCustomButtonsCount, (LPARAM)buttons);
    SendMessageW(hwndToolbar, TB_SETBUTTONSIZE, 0, MAKELONG(iconSize, iconSize));

    RECT rc;
    LRESULT res = SendMessageW(hwndToolbar, TB_GETITEMRECT, 0, (LPARAM)&rc);
    if (!res) {
        rc.left = rc.right = rc.top = rc.bottom = 0;
    }

    ShowWindow(hwndToolbar, SW_SHOW);

    REBARBANDINFOW rbBand{};
    rbBand.cbSize = sizeof(REBARBANDINFOW);
    rbBand.fMask = RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE;
    rbBand.fStyle = RBBS_FIXEDSIZE;
    if (theme::IsAppThemed() && IsCurrentThemeDefault()) {
        rbBand.fStyle |= RBBS_CHILDEDGE;
    }
    rbBand.hbmBack = nullptr;
    rbBand.lpText = (WCHAR*)L"Toolbar"; // NOLINT
    rbBand.hwndChild = hwndToolbar;
    rbBand.cxMinChild = (rc.right - rc.left) * kButtonsCount;
    rbBand.cyMinChild = (rc.bottom - rc.top) + 2 * rc.top;
    rbBand.cx = 0;
    SendMessageW(win->hwndReBar, RB_INSERTBAND, (WPARAM)-1, (LPARAM)&rbBand);

    SetWindowPos(win->hwndReBar, nullptr, 0, 0, 0, 0, SWP_NOZORDER);

    int defFontSize = GetAppFontSize();
    // ToolbarSize scales icons only; UI font size comes from UIFontSize (GetAppFontSize).
    int newSize = defFontSize;
    int maxFontSize = iconSize - yPad * 2 - 2; // -2 determined empirically
    if (newSize > maxFontSize) {
        logfa("CreateToolbar: setting toolbar font size to %d (scaled was %d, default size: %d)\n", maxFontSize,
              newSize, defFontSize);
        newSize = maxFontSize;
    } else {
        logfa("CreateToolbar: setting toolbar font size to %d (default size: %d)\n", newSize, defFontSize);
    }
    auto font = GetDefaultGuiFontOfSize(newSize);
    HwndSetFont(hwndToolbar, font);

    CreatePageBox(win, font, iconSize);
    SubclassToolbar(win);

    UpdateToolbarPageText(win, -1);
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
    HFONT font = GetAppMenuFont();
    int dy = FontDyPx(win->hwndFrame, font) + DpiScale(win->hwndFrame, 4);
    int minDy = DpiScale(win->hwndFrame, kTabBarDy);
    return std::max(dy, minDy);
}

int GetMenuBarRebarHeight(MainWindow* win) {
    if (!win || !win->hwndMenuReBar) {
        return 0;
    }
    // RB_GETBARHEIGHT underreports by 1px without WS_BORDER
    int dy = (int)SendMessageW(win->hwndMenuReBar, RB_GETBARHEIGHT, 0, 0) + 1;
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
                                            DWORD_PTR dwRefData) {
    if (WM_ERASEBKGND == uMsg) {
        // always paint background with theme color to avoid gray strips in light theme
        HDC hdc = (HDC)wParam;
        RECT rect;
        GetClientRect(hWnd, &rect);
        COLORREF bgCol = ThemeControlBackgroundColor();
        auto bgBrush = CreateSolidBrush(bgCol);
        FillRect(hdc, &rect, bgBrush);
        DeleteObject(bgBrush);
        return 1;
    }
    if (WM_NOTIFY == uMsg) {
        auto win = FindMainWindowByHwnd(hWnd);
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
                                              DWORD_PTR dwRefData) {
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
static DWORD gMenuBarLastDismissedTick = 0;

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

    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(hwndTb, &pt);

    // hit-test the toolbar
    int btnCount = (int)SendMessageW(hwndTb, TB_BUTTONCOUNT, 0, 0);
    for (int i = 0; i < btnCount; i++) {
        RECT rc;
        SendMessageW(hwndTb, TB_GETITEMRECT, i, (LPARAM)&rc);
        if (PtInRect(&rc, pt)) {
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
        WCHAR* name = AllocArrayTemp<WCHAR>(mii.cch);
        mii.dwTypeData = name;
        GetMenuItemInfoW(menu, i, TRUE, &mii);

        TBBUTTON b{};
        b.iBitmap = I_IMAGENONE;
        b.idCommand = kMenuBarCmdFirst + i;
        b.fsState = TBSTATE_ENABLED;
        b.fsStyle = BTNS_AUTOSIZE | BTNS_SHOWTEXT;
        b.iString = (INT_PTR)name;
        SendMessageW(hwndMb, TB_ADDBUTTONS, 1, (LPARAM)&b);
    }

    SendMessageW(hwndMb, TB_AUTOSIZE, 0, 0);

    if (win->hwndMenuReBar) {
        RECT rc;
        LRESULT res = SendMessageW(hwndMb, TB_GETITEMRECT, 0, (LPARAM)&rc);
        int menuBarDy = MenuBarToolbarIdealDy(win);
        if (res && rc.bottom > rc.top) {
            menuBarDy = (rc.bottom - rc.top) + 2 * rc.top;
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
    if (win->hwndMenuReBar) {
        return;
    }

    bool isRtl = IsUIRtl();
    HINSTANCE hinst = GetModuleHandle(nullptr);
    HWND hwndParent = win->hwndFrame;

    // create hidden; caller shows after RelayoutWindow positions it
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
    SendMessageW(win->hwndMenuToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);

    if (!UseDarkModeLib() || !DarkMode::isEnabled()) {
        if (!IsCurrentThemeDefault()) {
            SetWindowTheme(win->hwndMenuToolbar, L"", L"");
        }
    }

    HFONT font = GetAppMenuFont();
    HwndSetFont(win->hwndMenuToolbar, font);

    LRESULT tbExStyle = SendMessageW(win->hwndMenuToolbar, TB_GETEXTENDEDSTYLE, 0, 0);
    tbExStyle |= TBSTYLE_EX_MIXEDBUTTONS;
    SendMessageW(win->hwndMenuToolbar, TB_SETEXTENDEDSTYLE, 0, tbExStyle);

    RebuildMenuBarButtons(win);

    RECT rc;
    LRESULT res = SendMessageW(win->hwndMenuToolbar, TB_GETITEMRECT, 0, (LPARAM)&rc);
    int menuBarDy = (rc.bottom - rc.top) + 2 * rc.top;
    if (!res || menuBarDy <= 0) {
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
    if (win->hwndMenuReBar) {
        ShowWindow(win->hwndMenuReBar, SW_SHOW);
    }
}

void DestroyMenuBarRebar(MainWindow* win) {
    HwndDestroyWindowSafe(&win->hwndMenuToolbar);
    HwndDestroyWindowSafe(&win->hwndMenuReBar);
}

bool IsShowingMenuBarRebar(MainWindow* win) {
    if (!win->hwndMenuReBar) {
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
    DWORD now = GetTickCount();
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
        RECT btnRect;
        int btnCmdId = kMenuBarCmdFirst + menuIdx;
        int btnIdx = (int)SendMessageW(win->hwndMenuToolbar, TB_COMMANDTOINDEX, btnCmdId, 0);
        SendMessageW(win->hwndMenuToolbar, TB_GETITEMRECT, btnIdx, (LPARAM)&btnRect);
        MapWindowPoints(win->hwndMenuToolbar, HWND_DESKTOP, (POINT*)&btnRect, 2);

        gMenuBarPopupNav.win = win;
        gMenuBarPopupNav.rootMenu = subMenu;
        gMenuBarPopupNav.currentMenu = subMenu;
        gMenuBarPopupNav.currentFlags = 0;
        gMenuBarPopupNav.nextMenuIdx = menuIdx;

        HHOOK hook = SetWindowsHookExW(WH_MSGFILTER, MenuBarMsgFilterHook, nullptr, GetCurrentThreadId());
        TrackPopupMenu(subMenu, flags, btnRect.left, btnRect.bottom, 0, win->hwndFrame, nullptr);
        if (hook) {
            UnhookWindowsHookEx(hook);
        }

        int nextMenuIdx = gMenuBarPopupNav.nextMenuIdx;
        gMenuBarPopupNav = {};
        if (nextMenuIdx == menuIdx || menuCount <= 1) {
            gMenuBarLastDismissedIdx = menuIdx;
            gMenuBarLastDismissedTick = GetTickCount();
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
        WCHAR* name = AllocArrayTemp<WCHAR>(mii.cch);
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
