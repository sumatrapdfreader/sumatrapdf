/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "SumatraPDF.h"
#include "StrUtil.h"
#include "WinUtil.h"
#include "Toolbar.h"
#include "translations.h"
#include "resource.h"
#include "WindowInfo.h"
#include "AppTools.h"
#include "Search.h"
#include "Menu.h"

struct ToolbarButtonInfo {
    /* index in the toolbar bitmap (-1 for separators) */
    int           bmpIndex;
    int           cmdId;
    const char *  toolTip;
    int           flags;
};

static ToolbarButtonInfo gToolbarButtons[] = {
    { 0,   IDM_OPEN,              _TRN("Open"),           MF_REQ_DISK_ACCESS },
// the Open button is replaced with a Save As button in Plugin mode:
//  { 12,  IDM_SAVEAS,            _TRN("Save As"),        MF_REQ_DISK_ACCESS },
    { 1,   IDM_PRINT,             _TRN("Print"),          MF_REQ_PRINTER_ACCESS },
    { -1,  IDM_GOTO_PAGE,         NULL,                   0 },
    { 2,   IDM_GOTO_PREV_PAGE,    _TRN("Previous Page"),  0 },
    { 3,   IDM_GOTO_NEXT_PAGE,    _TRN("Next Page"),      0 },
    { -1,  0,                     NULL,                   0 },
    { 4,   IDT_VIEW_FIT_WIDTH,    _TRN("Fit Width and Show Pages Continuously"), 0 },
    { 5,   IDT_VIEW_FIT_PAGE,     _TRN("Fit a Single Page"), 0 },
    { 6,   IDT_VIEW_ZOOMOUT,      _TRN("Zoom Out"),       0 },
    { 7,   IDT_VIEW_ZOOMIN,       _TRN("Zoom In"),        0 },
    { -1,  IDM_FIND_FIRST,        NULL,                   0 },
    { 8,   IDM_FIND_PREV,         _TRN("Find Previous"),  0 },
    { 9,   IDM_FIND_NEXT,         _TRN("Find Next"),      0 },
    { 10,  IDM_FIND_MATCH,        _TRN("Match Case"),     0 },
};

#define TOOLBAR_BUTTONS_COUNT dimof(gToolbarButtons)

inline bool TbIsSeparator(ToolbarButtonInfo& tbi)
{
    return tbi.bmpIndex < 0;
}

static bool IsVisibleToolbarButton(WindowInfo *win, int buttonNo)
{
    switch (gToolbarButtons[buttonNo].cmdId) {

    case IDT_VIEW_FIT_WIDTH:
    case IDT_VIEW_FIT_PAGE:
        return !win->IsChm();

    case IDM_FIND_FIRST:
    case IDM_FIND_NEXT:
    case IDM_FIND_PREV:
    case IDM_FIND_MATCH:
        return NeedsFindUI(win);

    default:
        return true;
    }
}

static bool IsToolbarButtonEnabled(WindowInfo *win, int buttonNo)
{
    int cmdId = gToolbarButtons[buttonNo].cmdId;

    // If restricted, disable
    if (!HasPermission(gToolbarButtons[buttonNo].flags >> PERM_FLAG_OFFSET))
        return false;

    // If no file open, only enable open button
    if (!win->IsDocLoaded())
        return IDM_OPEN == cmdId;

    switch (cmdId)
    {
    case IDM_OPEN:
        // opening different files isn't allowed in plugin mode
        return !gPluginMode;

    case IDM_PRINT:
        return win->dm->engine && win->dm->engine->IsPrintingAllowed();

    case IDM_FIND_NEXT:
    case IDM_FIND_PREV:
        // TODO: Update on whether there's more to find, not just on whether there is text.
        return win::GetTextLen(win->hwndFindBox) > 0;

    case IDM_GOTO_NEXT_PAGE:
        return win->dm->CurrentPageNo() < win->dm->PageCount();
    case IDM_GOTO_PREV_PAGE:
        return win->dm->CurrentPageNo() > 1;

    default:
        return true;
    }
}

static TBBUTTON TbButtonFromButtonInfo(int i) {
    TBBUTTON tbButton = { 0 };
    tbButton.idCommand = gToolbarButtons[i].cmdId;
    if (TbIsSeparator(gToolbarButtons[i])) {
        tbButton.fsStyle = TBSTYLE_SEP;
    } else {
        tbButton.iBitmap = gToolbarButtons[i].bmpIndex;
        tbButton.fsState = TBSTATE_ENABLED;
        tbButton.fsStyle = TBSTYLE_BUTTON;
        tbButton.iString = (INT_PTR)Trans::GetTranslation(gToolbarButtons[i].toolTip);
    }
    return tbButton;
}

#define WS_TOOLBAR (WS_CHILD | WS_CLIPSIBLINGS | \
                    TBSTYLE_TOOLTIPS | TBSTYLE_FLAT | \
                    TBSTYLE_LIST | CCS_NODIVIDER | CCS_NOPARENTALIGN)

static void BuildTBBUTTONINFO(TBBUTTONINFO& info, TCHAR *txt)
{
    info.cbSize = sizeof(TBBUTTONINFO);
    info.dwMask = TBIF_TEXT | TBIF_BYINDEX;
    info.pszText = txt;
}

// Set toolbar button tooltips taking current language into account.
void UpdateToolbarButtonsToolTipsForWindow(WindowInfo *win)
{
    TBBUTTONINFO buttonInfo;
    HWND hwnd = win->hwndToolbar;
    LRESULT res;
    for (int i = 0; i < TOOLBAR_BUTTONS_COUNT; i++) {
        WPARAM buttonId = (WPARAM)i;
        const char *txt = gToolbarButtons[i].toolTip;
        if (NULL == txt)
            continue;
        const TCHAR *translation = Trans::GetTranslation(txt);
        BuildTBBUTTONINFO(buttonInfo, (TCHAR *)translation);
        res = SendMessage(hwnd, TB_SETBUTTONINFO, buttonId, (LPARAM)&buttonInfo);
        assert(0 != res);
    }
}

void ToolbarUpdateStateForWindow(WindowInfo *win, bool showHide)
{
    const LPARAM enabled = (LPARAM)MAKELONG(1, 0);
    const LPARAM disabled = (LPARAM)MAKELONG(0, 0);

    for (int i = 0; i < TOOLBAR_BUTTONS_COUNT; i++) {
        if (showHide) {
            BOOL hide = !IsVisibleToolbarButton(win, i);
            SendMessage(win->hwndToolbar, TB_HIDEBUTTON, gToolbarButtons[i].cmdId, hide);
        }
        if (TbIsSeparator(gToolbarButtons[i]))
            continue;

        LPARAM buttonState = IsToolbarButtonEnabled(win, i) ? enabled : disabled;
        SendMessage(win->hwndToolbar, TB_ENABLEBUTTON, gToolbarButtons[i].cmdId, buttonState);
    }

    // Find labels may have to be repositioned if some
    // toolbar buttons were shown/hidden
    if (showHide && NeedsFindUI(win))
        UpdateToolbarFindText(win);
}

void ShowOrHideToolbarGlobally()
{
    for (size_t i = 0; i < gWindows.Count(); i++) {
        WindowInfo *win = gWindows.At(i);
        if (gGlobalPrefs.toolbarVisible) {
            ShowWindow(win->hwndReBar, SW_SHOW);
        } else {
            // Move the focus out of the toolbar
            if (win->hwndFindBox == GetFocus() || win->hwndPageBox == GetFocus())
                SetFocus(win->hwndFrame);
            ShowWindow(win->hwndReBar, SW_HIDE);
        }
        ClientRect rect(win->hwndFrame);
        SendMessage(win->hwndFrame, WM_SIZE, 0, MAKELONG(rect.dx, rect.dy));
    }
}

void UpdateFindbox(WindowInfo* win)
{
    ToggleWindowStyle(win->hwndFindBg, SS_WHITERECT, win->IsDocLoaded());
    ToggleWindowStyle(win->hwndPageBg, SS_WHITERECT, win->IsDocLoaded());

    InvalidateRect(win->hwndToolbar, NULL, TRUE);
    UpdateWindow(win->hwndToolbar);

    if (!win->IsDocLoaded()) {  // Avoid focus on Find box
        SetClassLongPtr(win->hwndFindBox, GCLP_HCURSOR, (LONG_PTR)gCursorArrow);
        HideCaret(NULL);
    } else {
        SetClassLongPtr(win->hwndFindBox, GCLP_HCURSOR, (LONG_PTR)gCursorIBeam);
        ShowCaret(NULL);
    }
}

static HBITMAP LoadExternalBitmap(HINSTANCE hInst, TCHAR * filename, INT resourceId)
{
    ScopedMem<TCHAR> path(AppGenDataFilename(filename));

    if (path) {
        HBITMAP hBmp = (HBITMAP)LoadImage(NULL, path, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
        if (hBmp)
            return hBmp;
    }
    return LoadBitmap(hInst, MAKEINTRESOURCE(resourceId));
}

static WNDPROC DefWndProcToolbar = NULL;
static LRESULT CALLBACK WndProcToolbar(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (WM_CTLCOLORSTATIC == message) {
        HWND hStatic = (HWND)lParam;
        WindowInfo *win = FindWindowInfoByHwnd(hStatic);
        if ((win && win->hwndFindBg != hStatic && win->hwndPageBg != hStatic) || IsAppThemed())
        {
            SetBkMode((HDC)wParam, TRANSPARENT);
            SelectBrush((HDC)wParam, GetStockBrush(NULL_BRUSH));
            return 0;
        }
    }
    return CallWindowProc(DefWndProcToolbar, hwnd, message, wParam, lParam);
}

static WNDPROC DefWndProcFindBox = NULL;
static LRESULT CALLBACK WndProcFindBox(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WindowInfo *win = FindWindowInfoByHwnd(hwnd);
    if (!win || !win->IsDocLoaded())
        return DefWindowProc(hwnd, message, wParam, lParam);

    if (ExtendedEditWndProc(hwnd, message, wParam, lParam)) {
        // select the whole find box on a non-selecting click
    } else if (WM_CHAR == message) {
        switch (wParam) {
        case VK_ESCAPE:
            if (win->findThread)
                AbortFinding(win);
            else
                SetFocus(win->hwndFrame);
            return 1;

        case VK_RETURN:
            FindTextOnThread(win);
            return 1;

        case VK_TAB:
            AdvanceFocus(win);
            return 1;
        }
    }
    else if (WM_ERASEBKGND == message) {
        RECT r;
        Edit_GetRect(hwnd, &r);
        if (r.left == 0 && r.top == 0) { // virgin box
            r.left += 4;
            r.top += 3;
            r.bottom += 3;
            r.right -= 2;
            Edit_SetRectNoPaint(hwnd, &r);
        }
    }
    else if (WM_KEYDOWN == message) {
        if (FrameOnKeydown(win, wParam, lParam, true))
            return 0;
    }

    LRESULT ret = CallWindowProc(DefWndProcFindBox, hwnd, message, wParam, lParam);

    if (WM_CHAR  == message ||
        WM_PASTE == message ||
        WM_CUT   == message ||
        WM_CLEAR == message ||
        WM_UNDO  == message) {
        ToolbarUpdateStateForWindow(win, false);
    }

    return ret;
}

#define TB_TEXT_PADDING_RIGHT 6

void UpdateToolbarFindText(WindowInfo *win)
{
    bool showUI = NeedsFindUI(win);
    ShowWindow(win->hwndFindText, showUI ? SW_SHOW : SW_HIDE);
    ShowWindow(win->hwndFindBg, showUI ? SW_SHOW : SW_HIDE);
    ShowWindow(win->hwndFindBox, showUI ? SW_SHOW : SW_HIDE);
    if (!showUI)
        return;

    const TCHAR *text = _TR("Find:");
    win::SetText(win->hwndFindText, text);

    WindowRect findWndRect(win->hwndFindBg);

    RECT r;
    SendMessage(win->hwndToolbar, TB_GETRECT, IDT_VIEW_ZOOMIN, (LPARAM)&r);
    int pos_x = r.right + 10;
    int pos_y = (r.bottom - findWndRect.dy) / 2;

    SizeI size = TextSizeInHwnd(win->hwndFindText, text);
    size.dx += TB_TEXT_PADDING_RIGHT;

    int padding = GetSystemMetrics(SM_CXEDGE);
    MoveWindow(win->hwndFindText, pos_x, (findWndRect.dy - size.dy + 1) / 2 + pos_y, size.dx, size.dy, TRUE);
    MoveWindow(win->hwndFindBg, pos_x + size.dx, pos_y, findWndRect.dx, findWndRect.dy, FALSE);
    MoveWindow(win->hwndFindBox, pos_x + size.dx + padding, (findWndRect.dy - size.dy + 1) / 2 + pos_y,
        findWndRect.dx - 2 * padding, size.dy, FALSE);

    TBBUTTONINFO bi;
    bi.cbSize = sizeof(bi);
    bi.dwMask = TBIF_SIZE;
    bi.cx = (WORD)(size.dx + findWndRect.dx + 12);
    SendMessage(win->hwndToolbar, TB_SETBUTTONINFO, IDM_FIND_FIRST, (LPARAM)&bi);
}

void UpdateToolbarState(WindowInfo *win)
{
    if (!win->IsDocLoaded())
        return;

    WORD state = (WORD)SendMessage(win->hwndToolbar, TB_GETSTATE, IDT_VIEW_FIT_WIDTH, 0);
    if (win->dm->displayMode() == DM_CONTINUOUS && win->dm->ZoomVirtual() == ZOOM_FIT_WIDTH)
        state |= TBSTATE_CHECKED;
    else
        state &= ~TBSTATE_CHECKED;
    SendMessage(win->hwndToolbar, TB_SETSTATE, IDT_VIEW_FIT_WIDTH, state);

    bool isChecked = (state & TBSTATE_CHECKED);

    state = (WORD)SendMessage(win->hwndToolbar, TB_GETSTATE, IDT_VIEW_FIT_PAGE, 0);
    if (win->dm->displayMode() == DM_SINGLE_PAGE && win->dm->ZoomVirtual() == ZOOM_FIT_PAGE)
        state |= TBSTATE_CHECKED;
    else
        state &= ~TBSTATE_CHECKED;
    SendMessage(win->hwndToolbar, TB_SETSTATE, IDT_VIEW_FIT_PAGE, state);

    isChecked &= (state & TBSTATE_CHECKED);
    if (!isChecked)
        win->prevZoomVirtual = INVALID_ZOOM;
}


#define TOOLBAR_MIN_ICON_SIZE 16
#define FIND_BOX_WIDTH 160

static void CreateFindBox(WindowInfo& win)
{
    HWND findBg = CreateWindowEx(WS_EX_STATICEDGE, WC_STATIC, _T(""), WS_VISIBLE | WS_CHILD,
                            0, 1, (int)(FIND_BOX_WIDTH * win.uiDPIFactor), (int)(TOOLBAR_MIN_ICON_SIZE * win.uiDPIFactor + 4),
                            win.hwndToolbar, (HMENU)0, ghinst, NULL);

    HWND find = CreateWindowEx(0, WC_EDIT, _T(""), WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL,
                            0, 1, (int)(FIND_BOX_WIDTH * win.uiDPIFactor - 2 * GetSystemMetrics(SM_CXEDGE)), (int)(TOOLBAR_MIN_ICON_SIZE * win.uiDPIFactor + 2),
                            win.hwndToolbar, (HMENU)0, ghinst, NULL);

    HWND label = CreateWindowEx(0, WC_STATIC, _T(""), WS_VISIBLE | WS_CHILD,
                            0, 1, 0, 0, win.hwndToolbar, (HMENU)0, ghinst, NULL);

    SetWindowFont(label, gDefaultGuiFont, FALSE);
    SetWindowFont(find, gDefaultGuiFont, FALSE);

    if (!DefWndProcToolbar)
        DefWndProcToolbar = (WNDPROC)GetWindowLongPtr(win.hwndToolbar, GWLP_WNDPROC);
    SetWindowLongPtr(win.hwndToolbar, GWLP_WNDPROC, (LONG_PTR)WndProcToolbar);

    if (!DefWndProcFindBox)
        DefWndProcFindBox = (WNDPROC)GetWindowLongPtr(find, GWLP_WNDPROC);
    SetWindowLongPtr(find, GWLP_WNDPROC, (LONG_PTR)WndProcFindBox);

    win.hwndFindText = label;
    win.hwndFindBox = find;
    win.hwndFindBg = findBg;

    UpdateToolbarFindText(&win);
}

static WNDPROC DefWndProcPageBox = NULL;
static LRESULT CALLBACK WndProcPageBox(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WindowInfo *win = FindWindowInfoByHwnd(hwnd);
    if (!win || !win->IsDocLoaded())
        return DefWindowProc(hwnd, message, wParam, lParam);

    if (ExtendedEditWndProc(hwnd, message, wParam, lParam)) {
        // select the whole page box on a non-selecting click
    } else if (WM_CHAR == message) {
        switch (wParam) {
        case VK_RETURN: {
            ScopedMem<TCHAR> buf(win::GetText(win->hwndPageBox));
            int newPageNo = win->dm->engine->GetPageByLabel(buf);
            if (win->dm->ValidPageNo(newPageNo)) {
                win->dm->GoToPage(newPageNo, 0, true);
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
    } else if (WM_ERASEBKGND == message) {
        RECT r;
        Edit_GetRect(hwnd, &r);
        if (r.left == 0 && r.top == 0) { // virgin box
            r.left += 4;
            r.top += 3;
            r.bottom += 3;
            r.right -= 2;
            Edit_SetRectNoPaint(hwnd, &r);
        }
    } else if (WM_KEYDOWN == message) {
        if (FrameOnKeydown(win, wParam, lParam, true))
            return 0;
    }

    return CallWindowProc(DefWndProcPageBox, hwnd, message, wParam, lParam);
}

#define PAGE_BOX_WIDTH 40

void UpdateToolbarPageText(WindowInfo *win, int pageCount, bool updateOnly)
{
    const TCHAR *text = _TR("Page:");
    if (!updateOnly)
        win::SetText(win->hwndPageText, text);
    SizeI size = TextSizeInHwnd(win->hwndPageText, text);
    size.dx += TB_TEXT_PADDING_RIGHT;

    WindowRect pageWndRect(win->hwndPageBg);

    RECT r;
    SendMessage(win->hwndToolbar, TB_GETRECT, IDM_PRINT, (LPARAM)&r);
    int pos_x = r.right + 10;
    int pos_y = (r.bottom - pageWndRect.dy) / 2;

    TCHAR *buf;
    SizeI size2;
    if (-1 == pageCount) {
        // preserve hwndPageTotal's text and size
        buf = win::GetText(win->hwndPageTotal);
        size2 = ClientRect(win->hwndPageTotal).Size();
        size2.dx -= TB_TEXT_PADDING_RIGHT;
    } else if (!pageCount)
        buf = str::Dup(_T(""));
    else if (!win->dm || !win->dm->engine || !win->dm->engine->HasPageLabels())
        buf = str::Format(_T(" / %d"), pageCount);
    else {
        buf = str::Format(_T(" (%d / %d)"), win->dm->CurrentPageNo(), pageCount);
        ScopedMem<TCHAR> buf2(str::Format(_T(" (%d / %d)"), pageCount, pageCount));
        size2 = TextSizeInHwnd(win->hwndPageTotal, buf2);
    }

    win::SetText(win->hwndPageTotal, buf);
    if (0 == size2.dx)
        size2 = TextSizeInHwnd(win->hwndPageTotal, buf);
    size2.dx += TB_TEXT_PADDING_RIGHT;
    free(buf);

    int padding = GetSystemMetrics(SM_CXEDGE);
    MoveWindow(win->hwndPageText, pos_x, (pageWndRect.dy - size.dy + 1) / 2 + pos_y, size.dx, size.dy, FALSE);
    if (IsUIRightToLeft())
        pos_x += size2.dx - TB_TEXT_PADDING_RIGHT;
    MoveWindow(win->hwndPageBg, pos_x + size.dx, pos_y, pageWndRect.dx, pageWndRect.dy, FALSE);
    MoveWindow(win->hwndPageBox, pos_x + size.dx + padding, (pageWndRect.dy - size.dy + 1) / 2 + pos_y,
               pageWndRect.dx - 2 * padding, size.dy, FALSE);
    // in right-to-left layout, the total comes "before" the current page number
    if (IsUIRightToLeft()) {
        pos_x -= size2.dx;
        MoveWindow(win->hwndPageTotal, pos_x + size.dx, (pageWndRect.dy - size.dy + 1) / 2 + pos_y, size2.dx, size.dy, FALSE);
    }
    else
        MoveWindow(win->hwndPageTotal, pos_x + size.dx + pageWndRect.dx, (pageWndRect.dy - size.dy + 1) / 2 + pos_y, size2.dx, size.dy, FALSE);

    TBBUTTONINFO bi;
    bi.cbSize = sizeof(bi);
    bi.dwMask = TBIF_SIZE;
    SendMessage(win->hwndToolbar, TB_GETBUTTONINFO, IDM_GOTO_PAGE, (LPARAM)&bi);
    size2.dx += size.dx + pageWndRect.dx + 12;
    if (bi.cx != size2.dx || !updateOnly) {
        bi.cx = (WORD)size2.dx;
        SendMessage(win->hwndToolbar, TB_SETBUTTONINFO, IDM_GOTO_PAGE, (LPARAM)&bi);
    } else {
        RectI rc = ClientRect(win->hwndPageTotal);
        rc = MapRectToWindow(rc, win->hwndPageTotal, win->hwndToolbar);
        InvalidateRect(win->hwndToolbar, &rc.ToRECT(), TRUE);
    }
}

static void CreatePageBox(WindowInfo& win)
{
    HWND pageBg = CreateWindowEx(WS_EX_STATICEDGE, WC_STATIC, _T(""), WS_VISIBLE | WS_CHILD,
                            0, 1, (int)(PAGE_BOX_WIDTH * win.uiDPIFactor), (int)(TOOLBAR_MIN_ICON_SIZE * win.uiDPIFactor + 4),
                            win.hwndToolbar, (HMENU)0, ghinst, NULL);

    HWND page = CreateWindowEx(0, WC_EDIT, _T("0"), WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL | ES_NUMBER | ES_RIGHT,
                            0, 1, (int)(PAGE_BOX_WIDTH * win.uiDPIFactor - 2 * GetSystemMetrics(SM_CXEDGE)), (int)(TOOLBAR_MIN_ICON_SIZE * win.uiDPIFactor + 2),
                            win.hwndToolbar, (HMENU)0, ghinst, NULL);

    HWND label = CreateWindowEx(0, WC_STATIC, _T(""), WS_VISIBLE | WS_CHILD,
                            0, 1, 0, 0, win.hwndToolbar, (HMENU)0, ghinst, NULL);

    HWND total = CreateWindowEx(0, WC_STATIC, _T(""), WS_VISIBLE | WS_CHILD,
                            0, 1, 0, 0, win.hwndToolbar, (HMENU)0, ghinst, NULL);

    SetWindowFont(label, gDefaultGuiFont, FALSE);
    SetWindowFont(page, gDefaultGuiFont, FALSE);
    SetWindowFont(total, gDefaultGuiFont, FALSE);

    if (!DefWndProcPageBox)
        DefWndProcPageBox = (WNDPROC)GetWindowLongPtr(page, GWLP_WNDPROC);
    SetWindowLongPtr(page, GWLP_WNDPROC, (LONG_PTR)WndProcPageBox);

    win.hwndPageText = label;
    win.hwndPageBox = page;
    win.hwndPageBg = pageBg;
    win.hwndPageTotal = total;

    UpdateToolbarPageText(&win, -1);
}

#define WS_REBAR (WS_CHILD | WS_CLIPCHILDREN | WS_BORDER | RBS_VARHEIGHT | \
                  RBS_BANDBORDERS | CCS_NODIVIDER | CCS_NOPARENTALIGN)

void CreateToolbar(WindowInfo *win)
{
    HWND hwndToolbar = CreateWindowEx(0, TOOLBARCLASSNAME, NULL, WS_TOOLBAR,
                                      0, 0, 0, 0, win->hwndFrame,(HMENU)IDC_TOOLBAR, ghinst, NULL);
    win->hwndToolbar = hwndToolbar;
    LRESULT lres = SendMessage(hwndToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);

    ShowWindow(hwndToolbar, SW_SHOW);
    TBBUTTON tbButtons[TOOLBAR_BUTTONS_COUNT];

    // the name of the bitmap contains the number of icons so that after adding/removing
    // icons a complete default toolbar is used rather than an incomplete customized one
    HBITMAP hbmp = LoadExternalBitmap(ghinst, _T("toolbar_11.bmp"), IDB_TOOLBAR);
    SizeI size = GetBitmapSize(hbmp);
    // stretch the toolbar bitmaps for higher DPI settings
    // TODO: get nicely interpolated versions of the toolbar icons for higher resolutions
    if (size.dy < TOOLBAR_MIN_ICON_SIZE * win->uiDPIFactor) {
        size.dx *= (int)(win->uiDPIFactor + 0.5f);
        size.dy *= (int)(win->uiDPIFactor + 0.5f);
        hbmp = (HBITMAP)CopyImage(hbmp, IMAGE_BITMAP, size.dx, size.dy, LR_COPYDELETEORG);
    }
    // Assume square icons
    HIMAGELIST himl = ImageList_Create(size.dy, size.dy, ILC_COLORDDB | ILC_MASK, 0, 0);
    ImageList_AddMasked(himl, hbmp, RGB(0xFF, 0, 0xFF));
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
        if (gToolbarButtons[i].cmdId == IDM_FIND_MATCH)
            tbButtons[i].fsStyle = BTNS_CHECK;
    }
    lres = SendMessage(hwndToolbar, TB_SETIMAGELIST, 0, (LPARAM)himl);

    LRESULT exstyle = SendMessage(hwndToolbar, TB_GETEXTENDEDSTYLE, 0, 0);
    exstyle |= TBSTYLE_EX_MIXEDBUTTONS;
    lres = SendMessage(hwndToolbar, TB_SETEXTENDEDSTYLE, 0, exstyle);

    lres = SendMessage(hwndToolbar, TB_ADDBUTTONS, TOOLBAR_BUTTONS_COUNT, (LPARAM)tbButtons);

    RECT rc;
    lres = SendMessage(hwndToolbar, TB_GETITEMRECT, 0, (LPARAM)&rc);

    DWORD  reBarStyle = WS_REBAR | WS_VISIBLE;
    win->hwndReBar = CreateWindowEx(WS_EX_TOOLWINDOW, REBARCLASSNAME, NULL, reBarStyle,
                                    0, 0, 0, 0, win->hwndFrame, (HMENU)IDC_REBAR, ghinst, NULL);
    if (!win->hwndReBar)
        SeeLastError();

    REBARINFO rbi;
    rbi.cbSize = sizeof(REBARINFO);
    rbi.fMask  = 0;
    rbi.himl   = (HIMAGELIST)NULL;
    lres = SendMessage(win->hwndReBar, RB_SETBARINFO, 0, (LPARAM)&rbi);

    REBARBANDINFO rbBand;
    rbBand.cbSize  = sizeof(REBARBANDINFO);
    rbBand.fMask   = /*RBBIM_COLORS | RBBIM_TEXT | RBBIM_BACKGROUND | */
                   RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE /*| RBBIM_SIZE*/;
    rbBand.fStyle  = /*RBBS_CHILDEDGE |*//* RBBS_BREAK |*/ RBBS_FIXEDSIZE /*| RBBS_GRIPPERALWAYS*/;
    if (IsAppThemed())
        rbBand.fStyle |= RBBS_CHILDEDGE;
    rbBand.hbmBack = NULL;
    rbBand.lpText     = _T("Toolbar");
    rbBand.hwndChild  = hwndToolbar;
    rbBand.cxMinChild = (rc.right - rc.left) * TOOLBAR_BUTTONS_COUNT;
    rbBand.cyMinChild = (rc.bottom - rc.top) + 2 * rc.top;
    rbBand.cx         = 0;
    lres = SendMessage(win->hwndReBar, RB_INSERTBAND, (WPARAM)-1, (LPARAM)&rbBand);

    SetWindowPos(win->hwndReBar, NULL, 0, 0, 0, 0, SWP_NOZORDER);

    CreatePageBox(*win);
    CreateFindBox(*win);
}
