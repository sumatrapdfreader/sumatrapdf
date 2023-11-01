/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinDynCalls.h"
#include "utils/Dpi.h"
#include "utils/WinUtil.h"

#include "wingui/UIModels.h"

#include "Accelerators.h"
#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "AppColors.h"
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
#include "Menu.h"
#include "SearchAndDDE.h"
#include "Toolbar.h"
#include "Translations.h"
#include "SvgIcons.h"
#include "SumatraConfig.h"
#include "Theme.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "utils/Log.h"

// https://docs.microsoft.com/en-us/windows/win32/controls/toolbar-control-reference

// TODO: experimenting with matching toolbar colors with theme
// Doesn't work, probably have to implement a custom toolbar control
// where we draw everything ourselves.
// #define USE_THEME_COLORS 1

static int kButtonSpacingX = 4;

// distance between label and edit field
constexpr int kTextPaddingRight = 6;

struct ToolbarButtonInfo {
    /* index in the toolbar bitmap (-1 for separators) */
    TbIcon bmpIndex;
    int cmdId;
    const char* toolTip;
};

// thos are not real commands but we have to refer to toolbar buttons
// is by a command. those are just background for area to be
// covered by other HWNDs. They need the right size
constexpr int CmdPageInfo = (int)CmdLast + 16;
constexpr int CmdInfoText = (int)CmdLast + 17;

static ToolbarButtonInfo gToolbarButtons[] = {
    {TbIcon::Open, CmdOpenFile, _TRN("Open")},
    {TbIcon::Print, CmdPrint, _TRN("Print")},
    {TbIcon::None, CmdPageInfo, nullptr}, // text box for page number + show current page / no of pages
    {TbIcon::PagePrev, CmdGoToPrevPage, _TRN("Previous Page")},
    {TbIcon::PageNext, CmdGoToNextPage, _TRN("Next Page")},
    {TbIcon::None, 0, nullptr}, // separator
    {TbIcon::LayoutContinuous, CmdZoomFitWidthAndContinuous, _TRN("Fit Width and Show Pages Continuously")},
    {TbIcon::LayoutSinglePage, CmdZoomFitPageAndSinglePage, _TRN("Fit a Single Page")},
    {TbIcon::RotateLeft, CmdRotateLeft, _TRN("Rotate &Left")},
    {TbIcon::RotateRight, CmdRotateRight, _TRN("Rotate &Right")},
    {TbIcon::ZoomOut, CmdZoomOut, _TRN("Zoom Out")},
    {TbIcon::ZoomIn, CmdZoomIn, _TRN("Zoom In")},
    {TbIcon::None, CmdFindFirst, nullptr},
    {TbIcon::SearchPrev, CmdFindPrev, _TRN("Find Previous")},
    {TbIcon::SearchNext, CmdFindNext, _TRN("Find Next")},
    {TbIcon::MatchCase, CmdFindMatch, _TRN("Match Case")},
    {TbIcon::None, CmdInfoText, nullptr}, // info text
};

constexpr int kButtonsCount = dimof(gToolbarButtons);

static bool TbIsSeparator(ToolbarButtonInfo& tbi) {
    return (int)tbi.bmpIndex < 0;
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
    if (win->AsChm()) {
        return false;
    }
    return true;
}

static bool NeedsInfo(MainWindow* win) {
    char* s = HwndGetTextTemp(win->hwndTbInfoText);
    bool show = str::Len(s) > 0;
    return show;
}

static bool IsVisibleToolbarButton(MainWindow* win, int buttonNo) {
    switch (gToolbarButtons[buttonNo].cmdId) {
        case CmdZoomFitWidthAndContinuous:
        case CmdZoomFitPageAndSinglePage:
            return !win->AsChm();
        case CmdRotateLeft:
        case CmdRotateRight:
            return NeedsRotateUI(win);
        case CmdFindFirst:
        case CmdFindNext:
        case CmdFindPrev:
        case CmdFindMatch:
            return NeedsFindUI(win);
        case CmdInfoText:
            return NeedsInfo(win);
        default:
            return true;
    }
}

static bool IsToolbarButtonEnabled(MainWindow* win, int buttonNo) {
    int cmdId = gToolbarButtons[buttonNo].cmdId;

    bool isAllowed = true;
    switch (cmdId) {
        case CmdOpenFile:
            isAllowed = HasPermission(Perm::DiskAccess);
            break;
        case CmdPrint:
            isAllowed = HasPermission(Perm::PrinterAccess);
            break;
    }
    if (!isAllowed) {
        return false;
    }

    // If no file open, only enable open button
    if (!win->IsDocLoaded()) {
        return CmdOpenFile == cmdId;
    }

    switch (cmdId) {
        case CmdOpenFile:
            // opening different files isn't allowed in plugin mode
            return !gPluginMode;

#ifndef DISABLE_DOCUMENT_RESTRICTIONS
        case CmdPrint:
            return !win->AsFixed() || win->AsFixed()->GetEngine()->AllowsPrinting();
#endif

        case CmdFindNext:
        case CmdFindPrev:
            // TODO: Update on whether there's more to find, not just on whether there is text.
            return HwndGetTextLen(win->hwndFindEdit) > 0;

        case CmdGoToNextPage:
            return win->ctrl->CurrentPageNo() < win->ctrl->PageCount();
        case CmdGoToPrevPage:
            return win->ctrl->CurrentPageNo() > 1;

        default:
            return true;
    }
}

static TBBUTTON TbButtonFromButtonInfo(int i) {
    auto& btInfo = gToolbarButtons[i];
    TBBUTTON info{};
    info.idCommand = btInfo.cmdId;
    if (TbIsSeparator(btInfo)) {
        info.fsStyle = TBSTYLE_SEP;
    } else {
        info.iBitmap = (int)btInfo.bmpIndex;
        info.fsState = TBSTATE_ENABLED;
        info.fsStyle = TBSTYLE_BUTTON;
        info.iString = (INT_PTR)trans::GetTranslation(btInfo.toolTip);
    }
    return info;
}

// Set toolbar button tooltips taking current language into account.
void UpdateToolbarButtonsToolTipsForWindow(MainWindow* win) {
    TBBUTTONINFO binfo{};
    HWND hwnd = win->hwndToolbar;
    ACCEL accel;
    for (int i = 0; i < kButtonsCount; i++) {
        auto& tb = gToolbarButtons[i];

        if (!tb.toolTip) {
            continue;
        }

        str::Str accelStr;
        if (GetAccelByCmd(tb.cmdId, accel)) {
            AppendAccelKeyToMenuString(accelStr, accel);
        }

        const char* s = trans::GetTranslationA(tb.toolTip);
        if (accelStr.size() > 0) {
            accelStr[0] = '(';
            accelStr.Append(")");
            s = str::JoinTemp(s, "  ", accelStr.Get());
        }

        WCHAR* tmp = ToWStrTemp(s);

        binfo.cbSize = sizeof(TBBUTTONINFO);
        binfo.dwMask = TBIF_TEXT | TBIF_BYINDEX;
        binfo.pszText = tmp;
        WPARAM buttonId = (WPARAM)i;
        TbSetButtonInfo(hwnd, buttonId, &binfo);
    }
}

constexpr LPARAM kStateEnabled = (LPARAM)MAKELONG(1, 0);
constexpr LPARAM kStateDisabled = (LPARAM)MAKELONG(0, 0);

// TODO: this is called too often
void ToolbarUpdateStateForWindow(MainWindow* win, bool setButtonsVisibility) {
    HWND hwnd = win->hwndToolbar;
    for (int i = 0; i < kButtonsCount; i++) {
        auto& tb = gToolbarButtons[i];
        if (setButtonsVisibility) {
            bool hide = !IsVisibleToolbarButton(win, i);
            SendMessageW(hwnd, TB_HIDEBUTTON, tb.cmdId, hide);
        }
        if (TbIsSeparator(tb)) {
            continue;
        }
        LPARAM buttonState = IsToolbarButtonEnabled(win, i) ? kStateEnabled : kStateDisabled;
        SendMessageW(hwnd, TB_ENABLEBUTTON, tb.cmdId, buttonState);
    }

    // Find labels may have to be repositioned if some
    // toolbar buttons were shown/hidden
    if (setButtonsVisibility && NeedsFindUI(win)) {
        UpdateToolbarFindText(win);
    }
    const WCHAR* msg = L"";
    DisplayModel* dm = win->AsFixed();
    if (dm && EngineHasUnsavedAnnotations(dm->GetEngine())) {
        msg = _TR("You have unsaved annotations");
    }
    SetToolbarInfoText(win, msg);
}

void ShowOrHideToolbar(MainWindow* win) {
    if (win->presentation || win->isFullScreen) {
        return;
    }
    if (gGlobalPrefs->showToolbar) {
        ShowWindow(win->hwndReBar, SW_SHOW);
    } else {
        // Move the focus out of the toolbar
        if (IsFocused(win->hwndFindEdit) || IsFocused(win->hwndPageEdit)) {
            SetFocus(win->hwndFrame);
        }
        ShowWindow(win->hwndReBar, SW_HIDE);
    }
    RelayoutWindow(win);
}

void UpdateFindbox(MainWindow* win) {
    SetWindowStyle(win->hwndFindBg, SS_WHITERECT, win->IsDocLoaded());
    SetWindowStyle(win->hwndPageBg, SS_WHITERECT, win->IsDocLoaded());

    InvalidateRect(win->hwndToolbar, nullptr, TRUE);
    UpdateWindow(win->hwndToolbar);

    if (!win->IsDocLoaded()) { // Avoid focus on Find box
        SetClassLongPtrW(win->hwndFindEdit, GCLP_HCURSOR, (LONG_PTR)GetCachedCursor(IDC_ARROW));
        HideCaret(nullptr);
    } else {
        SetClassLongPtrW(win->hwndFindEdit, GCLP_HCURSOR, (LONG_PTR)GetCachedCursor(IDC_IBEAM));
        ShowCaret(nullptr);
    }
}

LRESULT CALLBACK BgSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass,
                                DWORD_PTR dwRefData) {
    if (WM_ERASEBKGND == uMsg && ThemeColorizeControls()) {
        HDC hdc = (HDC)wParam;
        RECT rect;
        GetClientRect(hWnd, &rect);
        SetTextColor(hdc, ThemeWindowTextColor());
        SetBkColor(hdc, GetControlBackgroundColor());
        auto bg = CreateSolidBrush(GetControlBackgroundColor());
        FillRect(hdc, &rect, bg);
        DeleteObject(bg);
        return 1;
    }
    if (WM_NCDESTROY == uMsg) {
        RemoveWindowSubclass(hWnd, BgSubclassProc, uIdSubclass);
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

static WNDPROC DefWndProcToolbar = nullptr;
static LRESULT CALLBACK WndProcToolbar(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (WM_CTLCOLORSTATIC == msg || WM_CTLCOLOREDIT == msg) {
        HWND hStatic = (HWND)lp;
        HDC hdc = (HDC)wp;
        MainWindow* win = FindMainWindowByHwnd(hStatic);
        if (!win) {
            return CallWindowProc(DefWndProcToolbar, hwnd, msg, wp, lp);
        }
        if (win->hwndTbInfoText == hStatic) {
            COLORREF col = RGB(0xff, 0x00, 0x00);
            SetTextColor(hdc, col);
            SetBkMode(hdc, TRANSPARENT);
            auto br = GetStockBrush(NULL_BRUSH);
            return (LRESULT)br;
        }
        if ((win->hwndFindBg != hStatic && win->hwndPageBg != hStatic) || theme::IsAppThemed()) {
            // Set color used in "Page:" and "Find:" labels
            auto col = RGB(0x00, 0x00, 0x00);
            SetTextColor(hdc, ThemeWindowTextColor());
            SetBkMode(hdc, TRANSPARENT);
            return (LRESULT)win->brControlBgColor;
        }
    }

    if (WM_COMMAND == msg) {
        HWND hEdit = (HWND)lp;
        MainWindow* win = FindMainWindowByHwnd(hEdit);
        // "find as you type"
        if (EN_UPDATE == HIWORD(wp) && hEdit == win->hwndFindEdit && gGlobalPrefs->showToolbar) {
            FindTextOnThread(win, TextSearchDirection::Forward, false);
        }
    }
    return CallWindowProc(DefWndProcToolbar, hwnd, msg, wp, lp);
}

static WNDPROC DefWndProcEditSearch = nullptr;
static LRESULT CALLBACK WndProcEditSearch(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    MainWindow* win = FindMainWindowByHwnd(hwnd);
    if (!win || !win->IsDocLoaded()) {
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    if (ExtendedEditWndProc(hwnd, msg, wp, lp)) {
        // select the whole find box on a non-selecting click
    } else if (WM_CHAR == msg) {
        switch (wp) {
            case VK_ESCAPE:
                if (win->findThread) {
                    AbortFinding(win, true);
                } else {
                    SetFocus(win->hwndFrame);
                }
                return 1;

            case VK_RETURN: {
                if (IsShiftPressed()) {
                    FindPrev(win);
                } else {
                    FindNext(win);
                }
                return 1;
            }

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
        // TODO: if user re-binds F3 it'll not be picked up
        // we would have to either run accelerators after
        if (wp == VK_F3) {
            auto searchDir = IsShiftPressed() ? TextSearchDirection::Backward : TextSearchDirection::Forward;
            FindTextOnThread(win, searchDir, true);
            // Note: we don't return but let default processing take place
        }
        // TODO: here we used to call FrameOnKeydown() to make keys
        // like pageup etc. work even when focus is in text field
        // that no longer works because we moved most keys handling
        // to accelerators and we don't want to process acceleratos
        // while in edit control.
        // We could try to manually run accelerators but only if they
        // are virtual and don't prevent edit control from working
        // or maybe explicitly forword built-in accelerator for
        // white-listed shortucts but only if they were not modified by the user
    }

    LRESULT ret = CallWindowProc(DefWndProcEditSearch, hwnd, msg, wp, lp);

    // TOOD: why do we do it? re-eneable when we notice what breaks
    // the intent seems to be "after content of edit box changed"
    // but how does that afect state of the toolbar?
#if 0
    switch (msg) {
        case WM_CHAR:
        case WM_PASTE:
        case WM_CUT:
        case WM_CLEAR:
        case WM_UNDO:
        case WM_KEYUP:
            ToolbarUpdateStateForWindow(win, false);
            break;
    }
#endif

    return ret;
}

void UpdateToolbarFindText(MainWindow* win) {
    bool showUI = NeedsFindUI(win);
    HwndSetVisibility(win->hwndFindLabel, showUI);
    HwndSetVisibility(win->hwndFindBg, showUI);
    HwndSetVisibility(win->hwndFindEdit, showUI);
    if (!showUI) {
        return;
    }

    const WCHAR* text = _TR("Find:");
    HwndSetText(win->hwndFindLabel, text);

    Rect findWndRect = WindowRect(win->hwndFindBg);

    RECT r{};
    TbGetRect(win->hwndToolbar, CmdZoomIn, &r);
    int currX = r.right + DpiScale(win->hwndToolbar, 10);
    int currY = (r.bottom - findWndRect.dy) / 2;

    Size size = TextSizeInHwnd(win->hwndFindLabel, text);
    size.dx += DpiScale(win->hwndFrame, kTextPaddingRight);
    size.dx += DpiScale(win->hwndFrame, kButtonSpacingX);

    int padding = GetSystemMetrics(SM_CXEDGE);
    int x = currX;
    int y = (findWndRect.dy - size.dy + 1) / 2 + currY;
    MoveWindow(win->hwndFindLabel, x, y, size.dx, size.dy, TRUE);
    x = currX + size.dx;
    y = currY;
    MoveWindow(win->hwndFindBg, x, y, findWndRect.dx, findWndRect.dy, FALSE);
    x = currX + size.dx + padding;
    y = (findWndRect.dy - size.dy + 1) / 2 + currY;
    int dx = findWndRect.dx - 2 * padding;
    MoveWindow(win->hwndFindEdit, x, y, dx, size.dy, FALSE);

    dx = size.dx + findWndRect.dx + 12;
    TbSetButtonDx(win->hwndToolbar, CmdFindFirst, dx);
}

void SetToolbarInfoText(MainWindow* win, const WCHAR* s) {
    HWND hwnd = win->hwndTbInfoText;
    HwndSetText(hwnd, s);
    Size size = TextSizeInHwnd(hwnd, s);

    bool hide = size.dx == 0;
    SendMessageW(hwnd, TB_HIDEBUTTON, CmdInfoText, hide);
    if (hide) {
        MoveWindow(hwnd, 0, 0, 0, 0, TRUE);
        return;
    }

    TbSetButtonDx(win->hwndToolbar, CmdInfoText, size.dx);
    RECT r{};
    TbGetRect(win->hwndToolbar, CmdFindMatch, &r);
    int x = r.right + DpiScale(win->hwndToolbar, 10);
    int y = (r.bottom - size.dy) / 2;
    MoveWindow(hwnd, x, y, size.dx, size.dy, TRUE);
}

void UpdateToolbarState(MainWindow* win) {
    if (!win->IsDocLoaded()) {
        return;
    }
    HWND hwnd = win->hwndToolbar;
    WORD state = (WORD)SendMessageW(hwnd, TB_GETSTATE, CmdZoomFitWidthAndContinuous, 0);
    DisplayMode dm = win->ctrl->GetDisplayMode();
    float zoomVirtual = win->ctrl->GetZoomVirtual();
    if (dm == DisplayMode::Continuous && zoomVirtual == kZoomFitWidth) {
        state |= TBSTATE_CHECKED;
    } else {
        state &= ~TBSTATE_CHECKED;
    }
    SendMessageW(hwnd, TB_SETSTATE, CmdZoomFitWidthAndContinuous, state);

    bool isChecked = (state & TBSTATE_CHECKED);

    state = (WORD)SendMessageW(hwnd, TB_GETSTATE, CmdZoomFitPageAndSinglePage, 0);
    if (dm == DisplayMode::SinglePage && zoomVirtual == kZoomFitPage) {
        state |= TBSTATE_CHECKED;
    } else {
        state &= ~TBSTATE_CHECKED;
    }
    SendMessageW(hwnd, TB_SETSTATE, CmdZoomFitPageAndSinglePage, state);

    isChecked |= (state & TBSTATE_CHECKED);
    if (!isChecked) {
        win->CurrentTab()->prevZoomVirtual = kInvalidZoom;
    }
}

static void CreateFindBox(MainWindow* win, HFONT hfont, int iconDy) {
    int findBoxDx = HwndMeasureText(win->hwndFrame, "this is a story of my", hfont).dx;
    HMODULE hmod = GetModuleHandleW(nullptr);
    HWND p = win->hwndToolbar;
    DWORD style = WS_VISIBLE | WS_CHILD | WS_BORDER;
    DWORD exStyle = 0;
    int dy = iconDy + 2;
    // Size textSize = HwndMeasureText(win->hwndFrame, L"M", hfont);
    HWND findBg =
        CreateWindowEx(exStyle, WC_STATIC, L"", style, 0, 1, findBoxDx, dy, p, (HMENU) nullptr, hmod, nullptr);

    style = WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL;
    // dy = iconDy + DpiScale(win->hwndFrame, 2);
    dy = iconDy;
    exStyle = 0;
    HWND find = CreateWindowExW(exStyle, WC_EDIT, L"", style, 0, 1, findBoxDx, dy, p, (HMENU) nullptr, hmod, nullptr);

    style = WS_VISIBLE | WS_CHILD;
    HWND label = CreateWindowExW(0, WC_STATIC, L"", style, 0, 1, 0, 0, p, (HMENU) nullptr, hmod, nullptr);

    SetWindowFont(label, hfont, FALSE);
    SetWindowFont(find, hfont, FALSE);

    if (!DefWndProcToolbar) {
        DefWndProcToolbar = (WNDPROC)GetWindowLongPtr(win->hwndToolbar, GWLP_WNDPROC);
    }
    SetWindowLongPtr(win->hwndToolbar, GWLP_WNDPROC, (LONG_PTR)WndProcToolbar);

    if (!DefWndProcEditSearch) {
        DefWndProcEditSearch = (WNDPROC)GetWindowLongPtr(find, GWLP_WNDPROC);
    }
    SetWindowLongPtr(find, GWLP_WNDPROC, (LONG_PTR)WndProcEditSearch);

    win->hwndFindLabel = label;
    win->hwndFindEdit = find;
    win->hwndFindBg = findBg;
}

static void CreateInfoText(MainWindow* win, HFONT font) {
    HMODULE hmod = GetModuleHandleW(nullptr);
    DWORD style = WS_VISIBLE | WS_CHILD;
    HWND labelInfo =
        CreateWindowExW(0, WC_STATIC, L"", style, 0, 1, 0, 0, win->hwndToolbar, (HMENU) nullptr, hmod, nullptr);
    SetWindowFont(labelInfo, font, FALSE);

    win->hwndTbInfoText = labelInfo;
    SetToolbarInfoText(win, L"");
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
                char* s = HwndGetTextTemp(win->hwndPageEdit);
                int newPageNo = win->ctrl->GetPageByLabel(s);
                if (win->ctrl->ValidPageNo(newPageNo)) {
                    win->ctrl->GoToPage(newPageNo, true);
                    SetFocus(win->hwndFrame);
                }
                return 1;
            }
            case VK_ESCAPE:
                SetFocus(win->hwndFrame);
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
    const WCHAR* text = _TR("Page:");
    if (!updateOnly) {
        HwndSetText(win->hwndPageLabel, text);
    }
    Size size = TextSizeInHwnd(win->hwndPageLabel, text);
    size.dx += DpiScale(win->hwndFrame, kTextPaddingRight);
    size.dx += DpiScale(win->hwndFrame, kButtonSpacingX);

    Rect pageWndRect = WindowRect(win->hwndPageBg);

    RECT r{};
    SendMessageW(win->hwndToolbar, TB_GETRECT, CmdPrint, (LPARAM)&r);
    int currX = r.right + DpiScale(win->hwndFrame, 10);
    int currY = (r.bottom - pageWndRect.dy) / 2;

    TempStr txt = nullptr;
    Size size2;
    if (-1 == pageCount) {
        // preserve hwndPageTotal's text and size
        txt = HwndGetTextTemp(win->hwndPageTotal);
        size2 = ClientRect(win->hwndPageTotal).Size();
        size2.dx -= DpiScale(win->hwndFrame, kTextPaddingRight);
        size2.dx -= DpiScale(win->hwndFrame, kButtonSpacingX);
    } else if (!pageCount) {
        txt = (TempStr) "";
    } else if (!win->ctrl || !win->ctrl->HasPageLabels()) {
        txt = str::FormatTemp(" / %d", pageCount);
    } else {
        txt = str::FormatTemp(" (%d / %d)", win->ctrl->CurrentPageNo(), pageCount);
        TempStr txt2 = str::FormatTemp(" (%d / %d)", pageCount, pageCount);
        size2 = TextSizeInHwnd(win->hwndPageTotal, txt2);
    }

    HwndSetText(win->hwndPageTotal, txt);
    if (0 == size2.dx) {
        size2 = TextSizeInHwnd(win->hwndPageTotal, txt);
    }
    size2.dx += DpiScale(win->hwndFrame, kTextPaddingRight);
    size2.dx += DpiScale(win->hwndFrame, kButtonSpacingX);

    int padding = GetSystemMetrics(SM_CXEDGE);
    int x = currX;
    int y = (pageWndRect.dy - size.dy + 1) / 2 + currY;
    MoveWindow(win->hwndPageLabel, x, y, size.dx, size.dy, FALSE);
    if (IsUIRightToLeft()) {
        currX += size2.dx;
        currX -= DpiScale(win->hwndFrame, kTextPaddingRight);
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
    if (IsUIRightToLeft()) {
        currX -= size2.dx;
        x = currX + size.dx;
        y = (pageWndRect.dy - size.dy + 1) / 2 + currY;
        MoveWindow(win->hwndPageTotal, x, y, size2.dx, size.dy, FALSE);
    } else {
        x = currX + size.dx + pageWndRect.dx;
        y = (pageWndRect.dy - size.dy + 1) / 2 + currY;
        MoveWindow(win->hwndPageTotal, x, y, size2.dx, size.dy, FALSE);
    }

    TBBUTTONINFOW bi{};
    bi.cbSize = sizeof(bi);
    bi.dwMask = TBIF_SIZE;
    SendMessageW(win->hwndToolbar, TB_GETBUTTONINFO, CmdPageInfo, (LPARAM)&bi);
    size2.dx += size.dx + pageWndRect.dx + 12;
    if (bi.cx != size2.dx || !updateOnly) {
        TbSetButtonDx(win->hwndToolbar, CmdPageInfo, size2.dx);
    } else {
        // TODO: we don't always refresh page numbers correctly (can be seen in stress test)
        // maybe just InvalidateRect(win->hwndToolbar, nullptr, TRUE);
        // at the end?
        Rect rc = ClientRect(win->hwndPageTotal);
        rc = MapRectToWindow(rc, win->hwndPageTotal, win->hwndToolbar);
        RECT rTmp = ToRECT(rc);
        InvalidateRect(win->hwndToolbar, &rTmp, TRUE);
    }
    InvalidateRect(win->hwndToolbar, nullptr, TRUE);
}

static void CreatePageBox(MainWindow* win, HFONT font, int iconDy) {
    auto hwndFrame = win->hwndFrame;
    auto hwndToolbar = win->hwndToolbar;
    int boxWidth = HwndMeasureText(hwndFrame, "99999", font).dx;
    DWORD style = WS_VISIBLE | WS_CHILD;
    auto h = GetModuleHandle(nullptr);
    int dx = boxWidth;
    int dy = iconDy + 2;
    DWORD exStyle = 0;
    HWND pageBg = CreateWindowExW(exStyle, WC_STATICW, L"", style | WS_BORDER, 0, 1, dx, dy, hwndToolbar,
                                  (HMENU) nullptr, h, nullptr);
    HWND label = CreateWindowExW(0, WC_STATICW, L"", style, 0, 1, 0, 0, hwndToolbar, (HMENU) nullptr, h, nullptr);
    HWND total = CreateWindowExW(0, WC_STATICW, L"", style, 0, 1, 0, 0, hwndToolbar, (HMENU) nullptr, h, nullptr);

    style = WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL | ES_NUMBER | ES_RIGHT;
    dx = boxWidth - DpiScale(hwndFrame, 4); // 4 pixels padding on the right side of the text box
    dy = iconDy;
    exStyle = 0;
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

constexpr int kDefaultIconSize = 18;

// https://docs.microsoft.com/en-us/windows/win32/controls/toolbar-control-reference
void CreateToolbar(MainWindow* win) {
    kButtonSpacingX = 0;
    HINSTANCE hinst = GetModuleHandle(nullptr);
    HWND hwndParent = win->hwndFrame;
    DWORD style = WS_CHILD | WS_CLIPSIBLINGS | TBSTYLE_TOOLTIPS | TBSTYLE_FLAT;
    style |= TBSTYLE_LIST | CCS_NODIVIDER | CCS_NOPARENTALIGN;
    const WCHAR* cls = TOOLBARCLASSNAME;
    HMENU cmd = (HMENU)IDC_TOOLBAR;
    HWND hwndToolbar = CreateWindowExW(0, cls, nullptr, style, 0, 0, 0, 0, hwndParent, cmd, hinst, nullptr);
    win->hwndToolbar = hwndToolbar;
    SendMessageW(hwndToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);

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

    // assume square icons
    HIMAGELIST himl = ImageList_Create(dx, dx, ILC_COLORDDB | ILC_MASK, kButtonsCount, 0);
    COLORREF mask = RGB(0xff, 0xff, 0xff);
    HBITMAP hbmp = BuildIconsBitmap(dx, dx);
    if (true) {
        ImageList_AddMasked(himl, hbmp, mask);
    } else {
        int amres = ImageList_AddMasked(himl, hbmp, mask);
        int nImages = ImageList_GetImageCount(himl);
        logf("res: %d, nImages: %d\n", amres, nImages);
        LogBitmapInfo(hbmp);
    }
    DeleteObject(hbmp);
    SendMessageW(hwndToolbar, TB_SETIMAGELIST, 0, (LPARAM)himl);

    TBMETRICS tbMetrics{};
    tbMetrics.cbSize = sizeof(tbMetrics);
    // tbMetrics.dwMask = TBMF_PAD;
    tbMetrics.dwMask = TBMF_BUTTONSPACING;
    TbGetMetrics(hwndToolbar, &tbMetrics);
    tbMetrics.cxPad += DpiScale(win->hwndFrame, 14);
    tbMetrics.cyPad += DpiScale(win->hwndFrame, 2);
    tbMetrics.cxButtonSpacing += DpiScale(win->hwndFrame, kButtonSpacingX);
    // tbMetrics.cyButtonSpacing += DpiScale(win->hwndFrame, 4);
    TbSetMetrics(hwndToolbar, &tbMetrics);

    LRESULT exstyle = SendMessageW(hwndToolbar, TB_GETEXTENDEDSTYLE, 0, 0);
    exstyle |= TBSTYLE_EX_MIXEDBUTTONS;
    SendMessageW(hwndToolbar, TB_SETEXTENDEDSTYLE, 0, exstyle);

    TBBUTTON tbButtons[kButtonsCount];
    for (int i = 0; i < kButtonsCount; i++) {
        tbButtons[i] = TbButtonFromButtonInfo(i);
        if (gToolbarButtons[i].cmdId == CmdFindMatch) {
            tbButtons[i].fsStyle = BTNS_CHECK;
        }
    }
    BOOL ok = SendMessageW(hwndToolbar, TB_ADDBUTTONS, kButtonsCount, (LPARAM)tbButtons);
    CrashIf(!ok);

    SendMessageW(hwndToolbar, TB_SETBUTTONSIZE, 0, MAKELONG(dx, dx));

    RECT rc;
    LRESULT res = SendMessageW(hwndToolbar, TB_GETITEMRECT, 0, (LPARAM)&rc);
    if (!res) {
        rc.left = rc.right = rc.top = rc.bottom = 0;
    }

    ShowWindow(hwndToolbar, SW_SHOW);
    DWORD dwStyle = WS_CHILD | WS_CLIPCHILDREN | WS_BORDER | RBS_VARHEIGHT | RBS_BANDBORDERS;
    dwStyle |= CCS_NODIVIDER | CCS_NOPARENTALIGN | WS_VISIBLE;
    win->hwndReBar = CreateWindowExW(WS_EX_TOOLWINDOW, REBARCLASSNAME, nullptr, dwStyle, 0, 0, 0, 0, hwndParent,
                                     (HMENU)IDC_REBAR, hinst, nullptr);
    SetWindowSubclass(win->hwndReBar, BgSubclassProc, 0, 0);

    REBARINFO rbi{};
    rbi.cbSize = sizeof(REBARINFO);
    rbi.fMask = 0;
    rbi.himl = (HIMAGELIST) nullptr;
    SendMessageW(win->hwndReBar, RB_SETBARINFO, 0, (LPARAM)&rbi);

    REBARBANDINFOW rbBand{};
    rbBand.cbSize = sizeof(REBARBANDINFOW);
    rbBand.fMask = /*RBBIM_COLORS | RBBIM_TEXT | RBBIM_BACKGROUND | */
        RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE /*| RBBIM_SIZE*/;
    rbBand.fStyle = /*RBBS_CHILDEDGE |*/ /* RBBS_BREAK |*/ RBBS_FIXEDSIZE /*| RBBS_GRIPPERALWAYS*/;
    if (theme::IsAppThemed()) {
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

    int defFontSize = GetSizeOfDefaultGuiFont();
    // 18 was the default toolbar size, we want to scale the fonts in proportion
    int newSize = (defFontSize * gGlobalPrefs->toolbarSize) / kDefaultIconSize;
    auto font = GetDefaultGuiFontOfSize(newSize);

    CreatePageBox(win, font, iconSize);
    CreateFindBox(win, font, iconSize);
    CreateInfoText(win, font);

    UpdateToolbarPageText(win, -1);
    UpdateToolbarFindText(win);
}
