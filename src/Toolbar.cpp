/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinDynCalls.h"
#include "utils/Dpi.h"
#include "utils/WinUtil.h"
#include "utils/LogDbg.h"

#include "wingui/TreeModel.h"

#include "Annotation.h"
#include "EngineBase.h"
#include "EngineManager.h"
#include "SettingsStructs.h"
#include "Controller.h"
#include "DisplayModel.h"
#include "AppColors.h"
#include "GlobalPrefs.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "Notifications.h"
#include "SumatraPDF.h"
#include "WindowInfo.h"
#include "TabInfo.h"
#include "resource.h"
#include "AppTools.h"
#include "Menu.h"
#include "SearchAndDDE.h"
#include "Toolbar.h"
#include "Translations.h"
#include "SvgIcons.h"
#
// TODO: experimenting with matching toolbar colors with theme
// Doesn't work, probably have to implement a custom toolbar control
// where we draw everything ourselves.
// #define USE_THEME_COLORS 1

struct ToolbarButtonInfo {
    /* index in the toolbar bitmap (-1 for separators) */
    int bmpIndex;
    int cmdId;
    const char* toolTip;
    int flags;
};

static ToolbarButtonInfo gToolbarButtons[] = {
    {0, IDM_OPEN, _TRN("Open"), MF_REQ_DISK_ACCESS},
    // the Open button is replaced with a Save As button in Plugin mode:
    //  { 12,  IDM_SAVEAS,            _TRN("Save As"),        MF_REQ_DISK_ACCESS },
    {1, IDM_PRINT, _TRN("Print"), MF_REQ_PRINTER_ACCESS},
    {-1, IDM_GOTO_PAGE, nullptr, 0},
    {2, IDM_GOTO_PREV_PAGE, _TRN("Previous Page"), 0},
    {3, IDM_GOTO_NEXT_PAGE, _TRN("Next Page"), 0},
    {-1, 0, nullptr, 0},
    {4, IDT_VIEW_FIT_WIDTH, _TRN("Fit Width and Show Pages Continuously"), 0},
    {5, IDT_VIEW_FIT_PAGE, _TRN("Fit a Single Page"), 0},
    {6, IDT_VIEW_ZOOMOUT, _TRN("Zoom Out"), 0},
    {7, IDT_VIEW_ZOOMIN, _TRN("Zoom In"), 0},
    {-1, IDM_FIND_FIRST, nullptr, 0},
    {8, IDM_FIND_PREV, _TRN("Find Previous"), 0},
    {9, IDM_FIND_NEXT, _TRN("Find Next"), 0},
    {10, IDM_FIND_MATCH, _TRN("Match Case"), 0},
};

#define TOOLBAR_BUTTONS_COUNT dimof(gToolbarButtons)

inline bool TbIsSeparator(ToolbarButtonInfo& tbi) {
    return tbi.bmpIndex < 0;
}

static bool IsVisibleToolbarButton(WindowInfo* win, int buttonNo) {
    switch (gToolbarButtons[buttonNo].cmdId) {
        case IDT_VIEW_FIT_WIDTH:
        case IDT_VIEW_FIT_PAGE:
            return !win->AsChm();

        case IDM_FIND_FIRST:
        case IDM_FIND_NEXT:
        case IDM_FIND_PREV:
        case IDM_FIND_MATCH:
            return NeedsFindUI(win);

        default:
            return true;
    }
}

static bool IsToolbarButtonEnabled(WindowInfo* win, int buttonNo) {
    int cmdId = gToolbarButtons[buttonNo].cmdId;

    // If restricted, disable
    if (!HasPermission(gToolbarButtons[buttonNo].flags >> PERM_FLAG_OFFSET)) {
        return false;
    }

    // If no file open, only enable open button
    if (!win->IsDocLoaded()) {
        return IDM_OPEN == cmdId;
    }

    switch (cmdId) {
        case IDM_OPEN:
            // opening different files isn't allowed in plugin mode
            return !gPluginMode;

#ifndef DISABLE_DOCUMENT_RESTRICTIONS
        case IDM_PRINT:
            return !win->AsFixed() || win->AsFixed()->GetEngine()->AllowsPrinting();
#endif

        case IDM_FIND_NEXT:
        case IDM_FIND_PREV:
            // TODO: Update on whether there's more to find, not just on whether there is text.
            return win::GetTextLen(win->hwndFindBox) > 0;

        case IDM_GOTO_NEXT_PAGE:
            return win->ctrl->CurrentPageNo() < win->ctrl->PageCount();
        case IDM_GOTO_PREV_PAGE:
            return win->ctrl->CurrentPageNo() > 1;

        default:
            return true;
    }
}

static TBBUTTON TbButtonFromButtonInfo(int i) {
    TBBUTTON tbButton = {0};
    tbButton.idCommand = gToolbarButtons[i].cmdId;
    if (TbIsSeparator(gToolbarButtons[i])) {
        tbButton.fsStyle = TBSTYLE_SEP;
    } else {
        tbButton.iBitmap = gToolbarButtons[i].bmpIndex;
        tbButton.fsState = TBSTATE_ENABLED;
        tbButton.fsStyle = TBSTYLE_BUTTON;
        tbButton.iString = (INT_PTR)trans::GetTranslation(gToolbarButtons[i].toolTip);
    }
    return tbButton;
}

static void BuildTBBUTTONINFO(TBBUTTONINFO& info, const WCHAR* txt) {
    info.cbSize = sizeof(TBBUTTONINFO);
    info.dwMask = TBIF_TEXT | TBIF_BYINDEX;
    info.pszText = (WCHAR*)txt;
}

// Set toolbar button tooltips taking current language into account.
void UpdateToolbarButtonsToolTipsForWindow(WindowInfo* win) {
    TBBUTTONINFO buttonInfo;
    HWND hwnd = win->hwndToolbar;
    LRESULT res;
    for (int i = 0; i < TOOLBAR_BUTTONS_COUNT; i++) {
        WPARAM buttonId = (WPARAM)i;
        const char* txt = gToolbarButtons[i].toolTip;
        if (nullptr == txt) {
            continue;
        }
        const WCHAR* translation = trans::GetTranslation(txt);
        BuildTBBUTTONINFO(buttonInfo, translation);
        res = SendMessage(hwnd, TB_SETBUTTONINFO, buttonId, (LPARAM)&buttonInfo);
        CrashIf(0 == res);
    }
}

void ToolbarUpdateStateForWindow(WindowInfo* win, bool showHide) {
    const LPARAM enabled = (LPARAM)MAKELONG(1, 0);
    const LPARAM disabled = (LPARAM)MAKELONG(0, 0);

    HWND hwnd = win->hwndToolbar;
    for (int i = 0; i < TOOLBAR_BUTTONS_COUNT; i++) {
        auto& tb = gToolbarButtons[i];
        if (showHide) {
            BOOL hide = !IsVisibleToolbarButton(win, i);
            SendMessage(hwnd, TB_HIDEBUTTON, tb.cmdId, hide);
        }
        if (TbIsSeparator(tb)) {
            continue;
        }

        LPARAM buttonState = IsToolbarButtonEnabled(win, i) ? enabled : disabled;
        SendMessage(hwnd, TB_ENABLEBUTTON, tb.cmdId, buttonState);
    }

    // Find labels may have to be repositioned if some
    // toolbar buttons were shown/hidden
    if (showHide && NeedsFindUI(win)) {
        UpdateToolbarFindText(win);
    }
}

void ShowOrHideToolbar(WindowInfo* win) {
    if (win->presentation || win->isFullScreen) {
        return;
    }
    if (gGlobalPrefs->showToolbar && !win->AsEbook()) {
        ShowWindow(win->hwndReBar, SW_SHOW);
    } else {
        // Move the focus out of the toolbar
        if (IsFocused(win->hwndFindBox) || IsFocused(win->hwndPageBox)) {
            SetFocus(win->hwndFrame);
        }
        ShowWindow(win->hwndReBar, SW_HIDE);
    }
    RelayoutWindow(win);
}

void UpdateFindbox(WindowInfo* win) {
    SetWindowStyle(win->hwndFindBg, SS_WHITERECT, win->IsDocLoaded());
    SetWindowStyle(win->hwndPageBg, SS_WHITERECT, win->IsDocLoaded());

    InvalidateRect(win->hwndToolbar, nullptr, TRUE);
    UpdateWindow(win->hwndToolbar);

    if (!win->IsDocLoaded()) { // Avoid focus on Find box
        SetClassLongPtr(win->hwndFindBox, GCLP_HCURSOR, (LONG_PTR)GetCursor(IDC_ARROW));
        HideCaret(nullptr);
    } else {
        SetClassLongPtr(win->hwndFindBox, GCLP_HCURSOR, (LONG_PTR)GetCursor(IDC_IBEAM));
        ShowCaret(nullptr);
    }
}

static HBITMAP LoadExternalBitmap(HINSTANCE hInst, WCHAR* fileName, INT resourceId, bool useDibSection) {
    AutoFreeWstr path(AppGenDataFilename(fileName));
    UINT flags = useDibSection ? LR_CREATEDIBSECTION : 0;
    if (path) {
        HBITMAP hBmp = (HBITMAP)LoadImageW(nullptr, path, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | flags);
        if (hBmp) {
            return hBmp;
        }
    }
    return (HBITMAP)LoadImageW(hInst, MAKEINTRESOURCE(resourceId), IMAGE_BITMAP, 0, 0, flags);
}

static WNDPROC DefWndProcToolbar = nullptr;
static LRESULT CALLBACK WndProcToolbar(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (WM_CTLCOLORSTATIC == msg) {
        HWND hStatic = (HWND)lParam;
        WindowInfo* win = FindWindowInfoByHwnd(hStatic);
        if ((win && win->hwndFindBg != hStatic && win->hwndPageBg != hStatic) || theme::IsAppThemed()) {
            HDC hdc = (HDC)wParam;
#if defined(USE_THEME_COLORS)
            SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
            SetBkColor(hdc, GetCurrentTheme()->mainWindow.backgroundColor);
            // SetBkMode(hdc, TRANSPARENT);
            auto br = CreateSolidBrush(GetCurrentTheme()->mainWindow.backgroundColor);
#else
            auto col = GetAppColor(AppColor::DocumentText);
            SetTextColor(hdc, col);
            SetBkMode(hdc, TRANSPARENT);
            auto br = GetStockBrush(NULL_BRUSH);
#endif
            return (LRESULT)br;
        }
    }
    if (WM_COMMAND == msg) {
        HWND hEdit = (HWND)lParam;
        WindowInfo* win = FindWindowInfoByHwnd(hEdit);
        // "find as you type"
        if (EN_UPDATE == HIWORD(wParam) && hEdit == win->hwndFindBox && gGlobalPrefs->showToolbar) {
            FindTextOnThread(win, TextSearchDirection::Forward, false);
        }
    }
    return CallWindowProc(DefWndProcToolbar, hwnd, msg, wParam, lParam);
}

static WNDPROC DefWndProcFindBox = nullptr;
static LRESULT CALLBACK WndProcFindBox(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    WindowInfo* win = FindWindowInfoByHwnd(hwnd);
    if (!win || !win->IsDocLoaded()) {
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    if (ExtendedEditWndProc(hwnd, msg, wParam, lParam)) {
        // select the whole find box on a non-selecting click
    } else if (WM_CHAR == msg) {
        switch (wParam) {
            case VK_ESCAPE:
                if (win->findThread) {
                    AbortFinding(win, false);
                } else {
                    SetFocus(win->hwndFrame);
                }
                return 1;

            case VK_RETURN: {
                auto searchDir = IsShiftPressed() ? TextSearchDirection::Backward : TextSearchDirection::Forward;
                FindTextOnThread(win, searchDir, true);
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
        if (FrameOnKeydown(win, wParam, lParam, true)) {
            return 0;
        }
    }

    LRESULT ret = CallWindowProc(DefWndProcFindBox, hwnd, msg, wParam, lParam);

    if (WM_CHAR == msg || WM_PASTE == msg || WM_CUT == msg || WM_CLEAR == msg || WM_UNDO == msg ||
        WM_KEYUP == msg) {
        ToolbarUpdateStateForWindow(win, false);
    }

    return ret;
}

// distance between label and edit field
constexpr int TB_TEXT_PADDING_RIGHT = 6;

void UpdateToolbarFindText(WindowInfo* win) {
    bool showUI = NeedsFindUI(win);
    win::SetVisibility(win->hwndFindText, showUI);
    win::SetVisibility(win->hwndFindBg, showUI);
    win::SetVisibility(win->hwndFindBox, showUI);
    if (!showUI) {
        return;
    }

    const WCHAR* text = _TR("Find:");
    win::SetText(win->hwndFindText, text);

    Rect findWndRect = WindowRect(win->hwndFindBg);

    RECT r{};
    SendMessage(win->hwndToolbar, TB_GETRECT, IDT_VIEW_ZOOMIN, (LPARAM)&r);
    int currX = r.right + 10;
    int currY = (r.bottom - findWndRect.dy) / 2;

    Size size = TextSizeInHwnd(win->hwndFindText, text);
    size.dx += DpiScale(win->hwndFrame, TB_TEXT_PADDING_RIGHT);

    int padding = GetSystemMetrics(SM_CXEDGE);
    int x = currX;
    int y = (findWndRect.dy - size.dy + 1) / 2 + currY;
    MoveWindow(win->hwndFindText, x, y, size.dx, size.dy, TRUE);
    x = currX + size.dx;
    y = currY;
    MoveWindow(win->hwndFindBg, x, y, findWndRect.dx, findWndRect.dy, FALSE);
    x = currX + size.dx + padding;
    y = (findWndRect.dy - size.dy + 1) / 2 + currY;
    int dx = findWndRect.dx - 2 * padding;
    MoveWindow(win->hwndFindBox, x, y, dx, size.dy, FALSE);

    TBBUTTONINFOW bi{};
    bi.cbSize = sizeof(bi);
    bi.dwMask = TBIF_SIZE;
    bi.cx = (WORD)(size.dx + findWndRect.dx + 12);
    SendMessage(win->hwndToolbar, TB_SETBUTTONINFO, IDM_FIND_FIRST, (LPARAM)&bi);
}

void UpdateToolbarState(WindowInfo* win) {
    if (!win->IsDocLoaded()) {
        return;
    }
    HWND hwnd = win->hwndToolbar;
    WORD state = (WORD)SendMessage(hwnd, TB_GETSTATE, IDT_VIEW_FIT_WIDTH, 0);
    DisplayMode dm = win->ctrl->GetDisplayMode();
    float zoomVirtual = win->ctrl->GetZoomVirtual();
    if (dm == DM_CONTINUOUS && zoomVirtual == ZOOM_FIT_WIDTH) {
        state |= TBSTATE_CHECKED;
    } else {
        state &= ~TBSTATE_CHECKED;
    }
    SendMessage(hwnd, TB_SETSTATE, IDT_VIEW_FIT_WIDTH, state);

    bool isChecked = (state & TBSTATE_CHECKED);

    state = (WORD)SendMessage(hwnd, TB_GETSTATE, IDT_VIEW_FIT_PAGE, 0);
    if (dm == DM_SINGLE_PAGE && zoomVirtual == ZOOM_FIT_PAGE) {
        state |= TBSTATE_CHECKED;
    } else {
        state &= ~TBSTATE_CHECKED;
    }
    SendMessage(hwnd, TB_SETSTATE, IDT_VIEW_FIT_PAGE, state);

    isChecked |= (state & TBSTATE_CHECKED);
    if (!isChecked) {
        win->currentTab->prevZoomVirtual = INVALID_ZOOM;
    }
}

#define TOOLBAR_MIN_ICON_SIZE 16
#define FIND_BOX_WIDTH 160

static void CreateFindBox(WindowInfo* win) {
    int boxWidth = DpiScale(win->hwndFrame, FIND_BOX_WIDTH);
    int minIconSize = DpiScale(win->hwndFrame, TOOLBAR_MIN_ICON_SIZE);
    HWND findBg = CreateWindowEx(WS_EX_STATICEDGE, WC_STATIC, L"", WS_VISIBLE | WS_CHILD, 0, 1, boxWidth,
                                 minIconSize + 4, win->hwndToolbar, (HMENU)0, GetModuleHandle(nullptr), nullptr);

    HWND find = CreateWindowEx(0, WC_EDIT, L"", WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL, 0, 1,
                               boxWidth - 2 * GetSystemMetrics(SM_CXEDGE), minIconSize + 2, win->hwndToolbar, (HMENU)0,
                               GetModuleHandle(nullptr), nullptr);

    HWND label = CreateWindowEx(0, WC_STATIC, L"", WS_VISIBLE | WS_CHILD, 0, 1, 0, 0, win->hwndToolbar, (HMENU)0,
                                GetModuleHandle(nullptr), nullptr);

    SetWindowFont(label, GetDefaultGuiFont(), FALSE);
    SetWindowFont(find, GetDefaultGuiFont(), FALSE);

    if (!DefWndProcToolbar) {
        DefWndProcToolbar = (WNDPROC)GetWindowLongPtr(win->hwndToolbar, GWLP_WNDPROC);
    }
    SetWindowLongPtr(win->hwndToolbar, GWLP_WNDPROC, (LONG_PTR)WndProcToolbar);

    if (!DefWndProcFindBox)
        DefWndProcFindBox = (WNDPROC)GetWindowLongPtr(find, GWLP_WNDPROC);
    SetWindowLongPtr(find, GWLP_WNDPROC, (LONG_PTR)WndProcFindBox);

    win->hwndFindText = label;
    win->hwndFindBox = find;
    win->hwndFindBg = findBg;

    UpdateToolbarFindText(win);
}

static WNDPROC DefWndProcPageBox = nullptr;
static LRESULT CALLBACK WndProcPageBox(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    WindowInfo* win = FindWindowInfoByHwnd(hwnd);
    if (!win || !win->IsDocLoaded())
        return DefWindowProc(hwnd, msg, wParam, lParam);

    if (ExtendedEditWndProc(hwnd, msg, wParam, lParam)) {
        // select the whole page box on a non-selecting click
    } else if (WM_CHAR == msg) {
        switch (wParam) {
            case VK_RETURN: {
                AutoFreeWstr buf(win::GetText(win->hwndPageBox));
                int newPageNo = win->ctrl->GetPageByLabel(buf);
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
        if (FrameOnKeydown(win, wParam, lParam, true))
            return 0;
    }

    return CallWindowProc(DefWndProcPageBox, hwnd, msg, wParam, lParam);
}

#define PAGE_BOX_WIDTH 40

void UpdateToolbarPageText(WindowInfo* win, int pageCount, bool updateOnly) {
    const WCHAR* text = _TR("Page:");
    if (!updateOnly) {
        win::SetText(win->hwndPageText, text);
    }
    Size size = TextSizeInHwnd(win->hwndPageText, text);
    size.dx += DpiScale(win->hwndFrame, TB_TEXT_PADDING_RIGHT);

    Rect pageWndRect = WindowRect(win->hwndPageBg);

    RECT r{};
    SendMessage(win->hwndToolbar, TB_GETRECT, IDM_PRINT, (LPARAM)&r);
    int pos_x = r.right + 10;
    int pos_y = (r.bottom - pageWndRect.dy) / 2;

    WCHAR* buf;
    Size size2;
    if (-1 == pageCount) {
        // preserve hwndPageTotal's text and size
        buf = win::GetText(win->hwndPageTotal);
        size2 = ClientRect(win->hwndPageTotal).Size();
        size2.dx -= DpiScale(win->hwndFrame, TB_TEXT_PADDING_RIGHT);
    } else if (!pageCount) {
        buf = str::Dup(L"");
    } else if (!win->ctrl || !win->ctrl->HasPageLabels()) {
        buf = str::Format(L" / %d", pageCount);
    } else {
        buf = str::Format(L" (%d / %d)", win->ctrl->CurrentPageNo(), pageCount);
        AutoFreeWstr buf2(str::Format(L" (%d / %d)", pageCount, pageCount));
        size2 = TextSizeInHwnd(win->hwndPageTotal, buf2);
    }

    win::SetText(win->hwndPageTotal, buf);
    if (0 == size2.dx) {
        size2 = TextSizeInHwnd(win->hwndPageTotal, buf);
    }
    size2.dx += DpiScale(win->hwndFrame, TB_TEXT_PADDING_RIGHT);
    free(buf);

    int padding = GetSystemMetrics(SM_CXEDGE);
    MoveWindow(win->hwndPageText, pos_x, (pageWndRect.dy - size.dy + 1) / 2 + pos_y, size.dx, size.dy, FALSE);
    if (IsUIRightToLeft()) {
        pos_x += size2.dx - DpiScale(win->hwndFrame, TB_TEXT_PADDING_RIGHT);
    }
    MoveWindow(win->hwndPageBg, pos_x + size.dx, pos_y, pageWndRect.dx, pageWndRect.dy, FALSE);
    MoveWindow(win->hwndPageBox, pos_x + size.dx + padding, (pageWndRect.dy - size.dy + 1) / 2 + pos_y,
               pageWndRect.dx - 2 * padding, size.dy, FALSE);
    // in right-to-left layout, the total comes "before" the current page number
    if (IsUIRightToLeft()) {
        pos_x -= size2.dx;
        int x = pos_x + size.dx;
        int y = (pageWndRect.dy - size.dy + 1) / 2 + pos_y;
        MoveWindow(win->hwndPageTotal, x, y, size2.dx, size.dy, FALSE);
    } else {
        int x = pos_x + size.dx + pageWndRect.dx;
        int y = (pageWndRect.dy - size.dy + 1) / 2 + pos_y;
        MoveWindow(win->hwndPageTotal, x, y, size2.dx, size.dy, FALSE);
    }

    TBBUTTONINFOW bi{};
    bi.cbSize = sizeof(bi);
    bi.dwMask = TBIF_SIZE;
    SendMessage(win->hwndToolbar, TB_GETBUTTONINFO, IDM_GOTO_PAGE, (LPARAM)&bi);
    size2.dx += size.dx + pageWndRect.dx + 12;
    if (bi.cx != size2.dx || !updateOnly) {
        bi.cx = (WORD)size2.dx;
        SendMessage(win->hwndToolbar, TB_SETBUTTONINFO, IDM_GOTO_PAGE, (LPARAM)&bi);
    } else {
        Rect rc = ClientRect(win->hwndPageTotal);
        rc = MapRectToWindow(rc, win->hwndPageTotal, win->hwndToolbar);
        RECT rTmp = rc.ToRECT();
        InvalidateRect(win->hwndToolbar, &rTmp, TRUE);
    }
}

static void CreatePageBox(WindowInfo* win) {
    auto hwndFrame = win->hwndFrame;
    auto hwndToolbar = win->hwndToolbar;
    int boxWidth = DpiScale(hwndFrame, PAGE_BOX_WIDTH);
    int minIconSize = DpiScale(hwndFrame, TOOLBAR_MIN_ICON_SIZE);
    DWORD style = WS_VISIBLE | WS_CHILD;
    auto h = GetModuleHandle(nullptr);
    int dx = boxWidth;
    int dy = minIconSize + 4;
    DWORD exStyle = WS_EX_STATICEDGE;
    HWND pageBg = CreateWindowExW(exStyle, WC_STATIC, L"", style, 0, 1, dx, dy, hwndToolbar, (HMENU)0, h, nullptr);
    HWND label = CreateWindowExW(0, WC_STATIC, L"", style, 0, 1, 0, 0, hwndToolbar, (HMENU)0, h, nullptr);
    HWND total = CreateWindowExW(0, WC_STATIC, L"", style, 0, 1, 0, 0, hwndToolbar, (HMENU)0, h, nullptr);

    style = WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL | ES_NUMBER | ES_RIGHT;
    dx = boxWidth - 2 * GetSystemMetrics(SM_CXEDGE);
    dy = minIconSize + 2;
    HWND page = CreateWindowExW(0, WC_EDIT, L"0", style, 0, 1, dx, dy, hwndToolbar, (HMENU)0, h, nullptr);

    auto font = GetDefaultGuiFont();
    SetWindowFont(label, font, FALSE);
    SetWindowFont(page, font, FALSE);
    SetWindowFont(total, font, FALSE);

    if (!DefWndProcPageBox) {
        DefWndProcPageBox = (WNDPROC)GetWindowLongPtr(page, GWLP_WNDPROC);
    }
    SetWindowLongPtr(page, GWLP_WNDPROC, (LONG_PTR)WndProcPageBox);

    win->hwndPageText = label;
    win->hwndPageBox = page;
    win->hwndPageBg = pageBg;
    win->hwndPageTotal = total;

    UpdateToolbarPageText(win, -1);
}

#define WS_REBAR \
    (WS_CHILD | WS_CLIPCHILDREN | WS_BORDER | RBS_VARHEIGHT | RBS_BANDBORDERS | CCS_NODIVIDER | CCS_NOPARENTALIGN)

// Sometimes scaled icons show up with purple background. Here's what I was able to piece together.
// When icons not scaled, we don't ask for DIB section (the original behavior of the code)
// Win 7 : purple if DIB section (tested by me)
// Win 10 :
//  build 14383 : purple if no DIB section (tested by me)
//  build 10586 : purple if DIB section (reported in
//  https://github.com/sumatrapdfreader/sumatrapdf/issues/569#issuecomment-231508990)
// Other builds not tested, will default to no DIB section. Might need to update it if more reports come in.
static bool UseDibSection(bool needsScaling) {
    if (!needsScaling) {
        return false;
    }
    OSVERSIONINFOEX ver;
    GetOsVersion(ver);
    // everything other than win 10: no DIB section
    if (ver.dwMajorVersion != 10) {
        return false;
    }
    // win 10 seems to behave differently depending on the build
    // I assume that up to 10586 we don't want dib
    if (ver.dwBuildNumber <= 10586) {
        return false;
    }
    // builds > 10586, including 14383
    return true;
}

void LogBitmapInfo(HBITMAP hbmp) {
    BITMAP bmpInfo;
    GetObject(hbmp, sizeof(BITMAP), &bmpInfo);
    dbglogf("dx: %d, dy: %d, stride: %d, bitsPerPixel: %d\n", (int)bmpInfo.bmWidth, (int)bmpInfo.bmHeight,
            (int)bmpInfo.bmWidthBytes, (int)bmpInfo.bmBitsPixel);
    u8* bits = (u8*)bmpInfo.bmBits;
    u8* d;
    for (int y = 0; y < 5; y++) {
        d = bits + (size_t)bmpInfo.bmWidthBytes * y;
        dbglogf("y: %d, d: 0x%p\n", y, d);
    }
}

void CreateToolbar(WindowInfo* win) {
    HINSTANCE hinst = GetModuleHandle(nullptr);
    HWND hwndParent = win->hwndFrame;
    DWORD style = WS_CHILD | WS_CLIPSIBLINGS | TBSTYLE_TOOLTIPS | TBSTYLE_FLAT;
    style |= TBSTYLE_LIST | CCS_NODIVIDER | CCS_NOPARENTALIGN;
    const WCHAR* cls = TOOLBARCLASSNAME;
    HMENU cmd = (HMENU)IDC_TOOLBAR;
    HWND hwndToolbar = CreateWindowExW(0, cls, nullptr, style, 0, 0, 0, 0, hwndParent, cmd, hinst, nullptr);
    win->hwndToolbar = hwndToolbar;
    SendMessage(hwndToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);

    ShowWindow(hwndToolbar, SW_SHOW);
    TBBUTTON tbButtons[TOOLBAR_BUTTONS_COUNT];

    int dpi = DpiGet(win->hwndFrame);

    HBITMAP hbmp = nullptr;
    bool useSvg = true;

    Size size{-1, -1};
    if (useSvg) {
        // TODO: bitmap is skewed for dxDpi of 20, 24 etc.
        int dxDpi = 16;
        int scale = (int)ceilf((float)dpi / 96.f);
        int dx = dxDpi * scale;
        size.dx = dx;
        size.dy = dx;
        hbmp = BuildIconsBitmap(dx, dx);
    } else {
        // stretch the toolbar bitmaps for higher DPI settings
        // TODO: get nicely interpolated versions of the toolbar icons for higher resolutions

        // scale toolbar images only by integral sizes (2, 3 etc.)
        int scaleX = (int)ceilf((float)dpi / 96.f);
        int scaleY = (int)ceilf((float)dpi / 96.f);
        bool needsScaling = (scaleX > 1) || (scaleY > 1);
        bool useDibSection = UseDibSection(needsScaling);

        // the name of the bitmap contains the number of icons so that after adding/removing
        // icons a complete default toolbar is used rather than an incomplete customized one
        hbmp = LoadExternalBitmap(GetModuleHandle(nullptr), L"toolbar_11.bmp", IDB_TOOLBAR, useDibSection);
        size = GetBitmapSize(hbmp);

        if (needsScaling) {
            size.dx *= scaleX;
            size.dy *= scaleY;

            UINT flags = LR_COPYDELETEORG;
            if (useDibSection) {
                flags |= LR_CREATEDIBSECTION;
            }
            hbmp = (HBITMAP)CopyImage(hbmp, IMAGE_BITMAP, size.dx, size.dy, flags);
        }
    }
    // assume square icons
    HIMAGELIST himl = ImageList_Create(size.dy, size.dy, ILC_COLORDDB | ILC_MASK, 0, 0);
    COLORREF mask = RGB(0xFF, 0, 0xFF);
    if (useSvg) {
        mask = RGB(0xff, 0xff, 0xff);
    }
    int amres = ImageList_AddMasked(himl, hbmp, mask);
    int nImages = ImageList_GetImageCount(himl);
#if 0
    dbglogf("res: %d, nImages: %d\n", amres, nImages);
    LogBitmapInfo(hbmp);
#endif
    DeleteObject(hbmp);

    // in Plugin mode, replace the Open with a Save As button
    if (gPluginMode && size.dx / size.dy == 13) {
        gToolbarButtons[0].bmpIndex = 12;
        gToolbarButtons[0].cmdId = IDM_SAVEAS;
        gToolbarButtons[0].toolTip = _TRN("Save As");
        gToolbarButtons[0].flags = MF_REQ_DISK_ACCESS;
    }
    for (int i = 0; i < TOOLBAR_BUTTONS_COUNT; i++) {
        tbButtons[i] = TbButtonFromButtonInfo(i);
        if (gToolbarButtons[i].cmdId == IDM_FIND_MATCH) {
            tbButtons[i].fsStyle = BTNS_CHECK;
        }
    }
    SendMessageW(hwndToolbar, TB_SETIMAGELIST, 0, (LPARAM)himl);

    LRESULT exstyle = SendMessage(hwndToolbar, TB_GETEXTENDEDSTYLE, 0, 0);
    exstyle |= TBSTYLE_EX_MIXEDBUTTONS;
    SendMessageW(hwndToolbar, TB_SETEXTENDEDSTYLE, 0, exstyle);
    bool ok = SendMessageW(hwndToolbar, TB_ADDBUTTONS, TOOLBAR_BUTTONS_COUNT, (LPARAM)tbButtons);
    if (!ok) {
        DbgOutLastError();
    }

    RECT rc;
    LRESULT res = SendMessageW(hwndToolbar, TB_GETITEMRECT, 0, (LPARAM)&rc);
    if (!res) {
        rc.left = rc.right = rc.top = rc.bottom = 0;
    }

    DWORD dwStyle = WS_REBAR | WS_VISIBLE;
    win->hwndReBar = CreateWindowExW(WS_EX_TOOLWINDOW, REBARCLASSNAME, nullptr, dwStyle, 0, 0, 0, 0, hwndParent,
                                     (HMENU)IDC_REBAR, hinst, nullptr);

    REBARINFO rbi;
    rbi.cbSize = sizeof(REBARINFO);
    rbi.fMask = 0;
    rbi.himl = (HIMAGELIST) nullptr;
    SendMessage(win->hwndReBar, RB_SETBARINFO, 0, (LPARAM)&rbi);

    REBARBANDINFO rbBand;
    rbBand.cbSize = sizeof(REBARBANDINFO);
    rbBand.fMask = /*RBBIM_COLORS | RBBIM_TEXT | RBBIM_BACKGROUND | */
        RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE /*| RBBIM_SIZE*/;
    rbBand.fStyle = /*RBBS_CHILDEDGE |*/ /* RBBS_BREAK |*/ RBBS_FIXEDSIZE /*| RBBS_GRIPPERALWAYS*/;
    if (theme::IsAppThemed()) {
        rbBand.fStyle |= RBBS_CHILDEDGE;
    }
    rbBand.hbmBack = nullptr;
    rbBand.lpText = L"Toolbar";
    rbBand.hwndChild = hwndToolbar;
    rbBand.cxMinChild = (rc.right - rc.left) * TOOLBAR_BUTTONS_COUNT;
    rbBand.cyMinChild = (rc.bottom - rc.top) + 2 * rc.top;
    rbBand.cx = 0;
    SendMessage(win->hwndReBar, RB_INSERTBAND, (WPARAM)-1, (LPARAM)&rbBand);

    SetWindowPos(win->hwndReBar, nullptr, 0, 0, 0, 0, SWP_NOZORDER);

    CreatePageBox(win);
    CreateFindBox(win);
}
