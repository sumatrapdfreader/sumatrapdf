/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "SumatraPDF.h"
#include "WindowInfo.h"
#include "SumatraAbout.h"
#include "translations.h"
#include "Version.h"
#include "WinUtil.h"

#define ABOUT_LINE_OUTER_SIZE       2
#define ABOUT_LINE_SEP_SIZE         1
#define ABOUT_LEFT_RIGHT_SPACE_DX   8
#define ABOUT_MARGIN_DX            10
#define ABOUT_BOX_MARGIN_DY         6
#define ABOUT_BORDER_COL            RGB(0,0,0)
#define ABOUT_TXT_DY                6
#define ABOUT_RECT_PADDING          8

#define ABOUT_WIN_TITLE         _TR("About SumatraPDF")

#define SUMATRA_TXT_FONT        _T("Arial Black")
#define SUMATRA_TXT_FONT_SIZE   24

#define VERSION_TXT_FONT        _T("Arial Black")
#define VERSION_TXT_FONT_SIZE   12

#define VERSION_TXT             _T("v") CURR_VERSION_STR
#ifdef SVN_PRE_RELEASE_VER
 #define VERSION_SUB_TXT        _T("Pre-release")
#else
 #define VERSION_SUB_TXT        _T("")
#endif

static HWND gHwndAbout;
static HWND gHwndAboutTooltip = NULL;
static const TCHAR *gClickedURL = NULL;

struct AboutLayoutInfoEl {
    /* static data, must be provided */
    const TCHAR *   leftTxt;
    const TCHAR *   rightTxt;
    const TCHAR *   url;

    /* data calculated by the layout */
    RectI           leftPos;
    RectI           rightPos;
};

static AboutLayoutInfoEl gAboutLayoutInfo[] = {
    { _T("website"),        _T("SumatraPDF website"),   _T("http://blog.kowalczyk.info/software/sumatrapdf") },
    { _T("forums"),         _T("SumatraPDF forums"),    _T("http://blog.kowalczyk.info/forum_sumatra") },
    { _T("programming"),    _T("Krzysztof Kowalczyk"),  _T("http://blog.kowalczyk.info") },
    { _T("programming"),    _T("Simon B\xFCnzli"),      _T("http://www.zeniko.ch/#SumatraPDF") },
    { _T("programming"),    _T("William Blum"),         _T("http://william.famille-blum.org/") },
#ifdef SVN_PRE_RELEASE_VER
    { _T("a note"),         _T("Pre-release version, for testing only!"), NULL },
#endif
#ifdef DEBUG
    { _T("a note"),         _T("Debug version, for testing only!"), NULL },
#endif
    { _T("pdf rendering"),  _T("MuPDF"),                _T("http://mupdf.com") },
    { _T("program icon"),   _T("Zenon"),                _T("http://www.flashvidz.tk/") },
    { _T("toolbar icons"),  _T("Yusuke Kamiyamane"),    _T("http://p.yusukekamiyamane.com/") },
    { _T("translators"),    _T("The Translators"),      _T("http://blog.kowalczyk.info/software/sumatrapdf/translators.html") },
    { _T("translations"),   _T("Contribute translation"), _T("http://blog.kowalczyk.info/software/sumatrapdf/translations.html") },
    // Note: Must be on the last line, as it's dynamically hidden based on m_enableTeXEnhancements
    { _T("synctex"),        _T("J\xE9rome Laurens"),    _T("http://itexmac.sourceforge.net/SyncTeX.html") },
    { NULL, NULL, NULL }
};

struct AboutPageLink {
    const TCHAR *url; // usually AboutLayoutInfoEl->url
    RectI rect;       // usually AboutLayoutInfoEl->rightPos
} gLinkInfo[dimof(gAboutLayoutInfo) + 1];

#define COL1 RGB(196, 64, 50)
#define COL2 RGB(227, 107, 35)
#define COL3 RGB(93,  160, 40)
#define COL4 RGB(69, 132, 190)
#define COL5 RGB(112, 115, 207)

static void DrawSumatraPDF(HDC hdc, PointI pt)
{
    const TCHAR *txt = APP_NAME_STR;
#ifdef BLACK_ON_YELLOW
    // simple black version
    SetTextColor(hdc, ABOUT_BORDER_COL);
    TextOut(hdc, pt.x, pt.y, txt, Str::Len(txt));
#else
    // colorful version
    COLORREF cols[] = { COL1, COL2, COL3, COL4, COL5, COL5, COL4, COL3, COL2, COL1 };
    for (size_t i = 0; i < Str::Len(txt); i++) {
        SetTextColor(hdc, cols[i % dimof(cols)]);
        TextOut(hdc, pt.x, pt.y, txt + i, 1);

        SIZE txtSize;
        GetTextExtentPoint32(hdc, txt + i, 1, &txtSize);
        pt.x += txtSize.cx;
    }
#endif
}

SizeI CalcSumatraVersionSize(HDC hdc)
{
    SizeI result;

    Win::Font::ScopedFont fontSumatraTxt(hdc, SUMATRA_TXT_FONT, SUMATRA_TXT_FONT_SIZE);
    Win::Font::ScopedFont fontVersionTxt(hdc, VERSION_TXT_FONT, VERSION_TXT_FONT_SIZE);
    HGDIOBJ oldFont = SelectObject(hdc, fontSumatraTxt);

    SIZE txtSize;
    /* calculate minimal top box size */
    const TCHAR *txt = APP_NAME_STR;
    GetTextExtentPoint32(hdc, txt, Str::Len(txt), &txtSize);
    result.dy = txtSize.cy + ABOUT_BOX_MARGIN_DY * 2;
    result.dx = txtSize.cx;

    /* consider version and version-sub strings */
    SelectObject(hdc, fontVersionTxt);
    txt = VERSION_TXT;
    GetTextExtentPoint32(hdc, txt, Str::Len(txt), &txtSize);
    int minWidth = txtSize.cx;
    txt = VERSION_SUB_TXT;
    GetTextExtentPoint32(hdc, txt, Str::Len(txt), &txtSize);
    txtSize.cx = max(txtSize.cx, minWidth);
    result.dx += 2 * (txtSize.cx + 6);

    SelectObject(hdc, oldFont);

    return result;
}

void DrawSumatraVersion(HDC hdc, RectI rect)
{
    Win::Font::ScopedFont fontSumatraTxt(hdc, SUMATRA_TXT_FONT, SUMATRA_TXT_FONT_SIZE);
    Win::Font::ScopedFont fontVersionTxt(hdc, VERSION_TXT_FONT, VERSION_TXT_FONT_SIZE);
    HGDIOBJ oldFont = SelectObject(hdc, fontSumatraTxt);

    SetBkMode(hdc, TRANSPARENT);

    SIZE txtSize;
    const TCHAR *txt = APP_NAME_STR;
    GetTextExtentPoint32(hdc, txt, Str::Len(txt), &txtSize);
    RectI mainRect(rect.x + (rect.dx - txtSize.cx) / 2,
                   rect.y + (rect.dy - txtSize.cy) / 2, txtSize.cx, txtSize.cy);
    DrawSumatraPDF(hdc, mainRect.TL());

    SetTextColor(hdc, WIN_COL_BLACK);
    SelectObject(hdc, fontVersionTxt);
    PointI pt(mainRect.x + mainRect.dx + 6, mainRect.y);
    txt = VERSION_TXT;
    TextOut(hdc, pt.x, pt.y, txt, Str::Len(txt));
    txt = VERSION_SUB_TXT;
    TextOut(hdc, pt.x, pt.y + 16, txt, Str::Len(txt));

    SelectObject(hdc, oldFont);
}

RectI DrawBottomRightLink(HWND hwnd, HDC hdc, const TCHAR *txt)
{
    Win::Font::ScopedFont fontLeftTxt(hdc, _T("MS Shell Dlg"), 14);
    HPEN penLinkLine = CreatePen(PS_SOLID, 1, COL_BLUE_LINK);

    HGDIOBJ origFont = SelectObject(hdc, fontLeftTxt); /* Just to remember the orig font */

    SetTextColor(hdc, COL_BLUE_LINK);
    SetBkMode(hdc, TRANSPARENT);
    ClientRect rc(hwnd);

    SIZE txtSize;
    GetTextExtentPoint32(hdc, txt, Str::Len(txt), &txtSize);
    RectI rect(rc.dx - txtSize.cx - 6, rc.y + rc.dy - txtSize.cy - 6, txtSize.cx, txtSize.cy);
    DrawText(hdc, txt, -1, &rect.ToRECT(), DT_LEFT);

    SelectObject(hdc, penLinkLine);
    PaintLine(hdc, RectI(rect.x, rect.y + rect.dy, rect.dx, 0));

    SelectObject(hdc, origFont);
    DeleteObject(penLinkLine);

    // make the click target larger
    rect.Inflate(6, 6);
    return rect;
}

/* Draws the about screen and remembers some state for hyperlinking.
   It transcribes the design I did in graphics software - hopeless
   to understand without seeing the design. */
static void DrawAbout(HWND hwnd, HDC hdc, RectI rect)
{
    HBRUSH brushBg = CreateSolidBrush(gGlobalPrefs.m_bgColor);

    HPEN penBorder = CreatePen(PS_SOLID, ABOUT_LINE_OUTER_SIZE, WIN_COL_BLACK);
    HPEN penDivideLine = CreatePen(PS_SOLID, ABOUT_LINE_SEP_SIZE, WIN_COL_BLACK);
    HPEN penLinkLine = CreatePen(PS_SOLID, ABOUT_LINE_SEP_SIZE, COL_BLUE_LINK);

    Win::Font::ScopedFont fontLeftTxt(hdc, LEFT_TXT_FONT, LEFT_TXT_FONT_SIZE);
    Win::Font::ScopedFont fontRightTxt(hdc, RIGHT_TXT_FONT, RIGHT_TXT_FONT_SIZE);

    HGDIOBJ origFont = SelectObject(hdc, fontLeftTxt); /* Just to remember the orig font */

    ClientRect rc(hwnd);
    FillRect(hdc, &rc.ToRECT(), brushBg);

    /* render title */
    RectI titleRect(rect.TL(), CalcSumatraVersionSize(hdc));

    SelectObject(hdc, brushBg);
    SelectObject(hdc, penBorder);
    Rectangle(hdc, rect.x, rect.y + ABOUT_LINE_OUTER_SIZE, rect.x + rect.dx, rect.y + titleRect.dy + ABOUT_LINE_OUTER_SIZE);

    titleRect.Offset((rect.dx - titleRect.dx) / 2, 0);
    DrawSumatraVersion(hdc, titleRect);

    /* render attribution box */
    SetTextColor(hdc, ABOUT_BORDER_COL);
    SetBkMode(hdc, TRANSPARENT);

    Rectangle(hdc, rect.x, rect.y + titleRect.dy, rect.x + rect.dx, rect.y + rect.dy);

    /* render text on the left*/
    SelectObject(hdc, fontLeftTxt);
    for (AboutLayoutInfoEl *el = gAboutLayoutInfo; el->leftTxt; el++)
        TextOut(hdc, el->leftPos.x, el->leftPos.y, el->leftTxt, Str::Len(el->leftTxt));

    /* render text on the right */
    SelectObject(hdc, fontRightTxt);
    SelectObject(hdc, penLinkLine);
    ZeroMemory(&gLinkInfo, sizeof(gLinkInfo));
    for (AboutLayoutInfoEl *el = gAboutLayoutInfo; el->leftTxt; el++) {
        bool hasUrl = !gRestrictedUse && el->url;
        SetTextColor(hdc, hasUrl ? COL_BLUE_LINK : ABOUT_BORDER_COL);
        TextOut(hdc, el->rightPos.x, el->rightPos.y, el->rightTxt, Str::Len(el->rightTxt));

        if (hasUrl) {
            int underlineY = el->rightPos.y + el->rightPos.dy - 3;
            PaintLine(hdc, RectI(el->rightPos.x, underlineY, el->rightPos.dx, 0));
            gLinkInfo[el - gAboutLayoutInfo].url = el->url;
            gLinkInfo[el - gAboutLayoutInfo].rect = el->rightPos;
        }
    }

    SelectObject(hdc, penDivideLine);
    RectI divideLine(gAboutLayoutInfo[0].rightPos.x - ABOUT_LEFT_RIGHT_SPACE_DX,
                     rect.y + titleRect.dy + 4, 0, rect.y + rect.dy - 4 - gAboutLayoutInfo[0].rightPos.y);
    PaintLine(hdc, divideLine);

    SelectObject(hdc, origFont);

    DeleteObject(brushBg);
    DeleteObject(penBorder);
    DeleteObject(penDivideLine);
    DeleteObject(penLinkLine);
}

static void UpdateAboutLayoutInfo(HWND hwnd, HDC hdc, RectI *rect)
{
    HFONT fontLeftTxt = Win::Font::GetSimple(hdc, LEFT_TXT_FONT, LEFT_TXT_FONT_SIZE);
    HFONT fontRightTxt = Win::Font::GetSimple(hdc, RIGHT_TXT_FONT, RIGHT_TXT_FONT_SIZE);

    HGDIOBJ origFont = SelectObject(hdc, fontLeftTxt);

    /* show/hide the SyncTeX attribution line */
    assert(!gAboutLayoutInfo[dimof(gAboutLayoutInfo) - 2].leftTxt || Str::Eq(gAboutLayoutInfo[dimof(gAboutLayoutInfo) - 2].leftTxt, _T("synctex")));
    if (gGlobalPrefs.m_enableTeXEnhancements)
        gAboutLayoutInfo[dimof(gAboutLayoutInfo) - 2].leftTxt = _T("synctex");
    else
        gAboutLayoutInfo[dimof(gAboutLayoutInfo) - 2].leftTxt = NULL;

    /* calculate minimal top box size */
    SizeI headerSize = CalcSumatraVersionSize(hdc);

    /* calculate left text dimensions */
    SelectObject(hdc, fontLeftTxt);
    int leftLargestDx = 0;
    int leftDy = 0;
    for (AboutLayoutInfoEl *el = gAboutLayoutInfo; el->leftTxt; el++) {
        SIZE txtSize;
        GetTextExtentPoint32(hdc, el->leftTxt, Str::Len(el->leftTxt), &txtSize);
        el->leftPos.dx = txtSize.cx;
        el->leftPos.dy = txtSize.cy;

        if (el == &gAboutLayoutInfo[0])
            leftDy = el->leftPos.dy;
        else
            assert(leftDy == el->leftPos.dy);
        if (leftLargestDx < el->leftPos.dx)
            leftLargestDx = el->leftPos.dx;
    }

    /* calculate right text dimensions */
    SelectObject(hdc, fontRightTxt);
    int rightLargestDx = 0;
    int rightDy = 0;
    for (AboutLayoutInfoEl *el = gAboutLayoutInfo; el->leftTxt; el++) {
        SIZE txtSize;
        GetTextExtentPoint32(hdc, el->rightTxt, Str::Len(el->rightTxt), &txtSize);
        el->rightPos.dx = txtSize.cx;
        el->rightPos.dy = txtSize.cy;

        if (el == &gAboutLayoutInfo[0])
            rightDy = el->rightPos.dy;
        else
            assert(rightDy == el->rightPos.dy);
        if (rightLargestDx < el->rightPos.dx)
            rightLargestDx = el->rightPos.dx;
    }

    /* calculate total dimension and position */
    RectI minRect;
    minRect.dx = ABOUT_LEFT_RIGHT_SPACE_DX + leftLargestDx + ABOUT_LINE_SEP_SIZE + rightLargestDx + ABOUT_LEFT_RIGHT_SPACE_DX;
    if (minRect.dx < headerSize.dx)
        minRect.dx = headerSize.dx;
    minRect.dx += 2 * ABOUT_LINE_OUTER_SIZE + 2 * ABOUT_MARGIN_DX;

    minRect.dy = headerSize.dy;
    for (AboutLayoutInfoEl *el = gAboutLayoutInfo; el->leftTxt; el++)
        minRect.dy += rightDy + ABOUT_TXT_DY;
    minRect.dy += 2 * ABOUT_LINE_OUTER_SIZE + 4;

    ClientRect rc(hwnd);
    minRect.x = (rc.dx - minRect.dx) / 2;
    minRect.y = (rc.dy - minRect.dy) / 2;

    if (rect)
        *rect = minRect;

    /* calculate text positions */
    int linePosX = ABOUT_LINE_OUTER_SIZE + ABOUT_MARGIN_DX + leftLargestDx + ABOUT_LEFT_RIGHT_SPACE_DX;
    int currY = minRect.y + headerSize.dy + 4;
    for (AboutLayoutInfoEl *el = gAboutLayoutInfo; el->leftTxt; el++) {
        el->leftPos.x = minRect.x + linePosX - ABOUT_LEFT_RIGHT_SPACE_DX - el->leftPos.dx;
        el->leftPos.y = currY + (rightDy - leftDy) / 2;
        el->rightPos.x = minRect.x + linePosX + ABOUT_LEFT_RIGHT_SPACE_DX;
        el->rightPos.y = currY;
        currY += rightDy + ABOUT_TXT_DY;
    }

    SelectObject(hdc, origFont);
    Win::Font::Delete(fontLeftTxt);
    Win::Font::Delete(fontRightTxt);
}

void OnPaintAbout(HWND hwnd)
{
    PAINTSTRUCT ps;
    RectI rc;
    HDC hdc = BeginPaint(hwnd, &ps);
    UpdateAboutLayoutInfo(hwnd, hdc, &rc);
    DrawAbout(hwnd, hdc, rc);
    EndPaint(hwnd, &ps);
}

static const TCHAR *GetAboutLink(WindowInfo *win, int x, int y, RectI *rect=NULL)
{
    if (gRestrictedUse) return NULL;

    PointI cursor(x, y);
    for (int i = 0; i < dimof(gLinkInfo); i++) {
        if (gLinkInfo[i].rect.Inside(cursor)) {
            if (rect)
                *rect = gLinkInfo[i].rect;
            return gLinkInfo[i].url;
        }
    }
    
    return NULL;
}

void OnMenuAbout() {
    if (gHwndAbout) {
        SetActiveWindow(gHwndAbout);
        return;
    }

    gHwndAbout = CreateWindow(
            ABOUT_CLASS_NAME, ABOUT_WIN_TITLE,
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
            CW_USEDEFAULT, CW_USEDEFAULT,
            CW_USEDEFAULT, CW_USEDEFAULT,
            NULL, NULL,
            ghinst, NULL);
    if (!gHwndAbout)
        return;

    // get the dimensions required for the about box's content
    RectI rc;
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(gHwndAbout, &ps);
    UpdateAboutLayoutInfo(gHwndAbout, hdc, &rc);
    EndPaint(gHwndAbout, &ps);
    rc.Inflate(ABOUT_RECT_PADDING, ABOUT_RECT_PADDING);

    // resize the new window to just match these dimensions
    WindowRect wRc(gHwndAbout);
    ClientRect cRc(gHwndAbout);
    wRc.dx += rc.dx - cRc.dx;
    wRc.dy += rc.dy - cRc.dy;
    MoveWindow(gHwndAbout, wRc.x, wRc.y, wRc.dx, wRc.dy, FALSE);

    ShowWindow(gHwndAbout, SW_SHOW);
}

static void CreateInfotipForLink(const TCHAR *text, RectI& rc)
{
    if (gHwndAboutTooltip)
        return;

    gHwndAboutTooltip = CreateWindowEx(WS_EX_TOPMOST,
        TOOLTIPS_CLASS, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        gHwndAbout, NULL, ghinst, NULL);

    TOOLINFO ti = { 0 };
    ti.cbSize = sizeof(ti);
    ti.hwnd = gHwndAbout;
    ti.uFlags = TTF_SUBCLASS;
    ti.lpszText = (TCHAR *)text;
    ti.rect = rc.ToRECT();

    SendMessage(gHwndAboutTooltip, TTM_ADDTOOL, 0, (LPARAM)&ti);
}

static void ClearInfotip()
{
    if (!gHwndAboutTooltip)
        return;

    TOOLINFO ti = { 0 };
    ti.cbSize = sizeof(ti);
    ti.hwnd = gHwndAbout;

    SendMessage(gHwndAboutTooltip, TTM_DELTOOL, 0, (LPARAM)&ti);
    DestroyWindow(gHwndAboutTooltip);
    gHwndAboutTooltip = NULL;
}

LRESULT CALLBACK WndProcAbout(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    const TCHAR * url;
    POINT pt;

    switch (message)
    {
        case WM_CREATE:
            assert(!gHwndAbout);
            break;

        case WM_ERASEBKGND:
            // do nothing, helps to avoid flicker
            return TRUE;

        case WM_PAINT:
            OnPaintAbout(hwnd);
            break;

        case WM_SETCURSOR:
            if (GetCursorPos(&pt) && ScreenToClient(hwnd, &pt)) {
                RectI rect;
                const TCHAR *url = GetAboutLink(NULL, pt.x, pt.y, &rect);
                if (url) {
                    CreateInfotipForLink(url, rect);
                    SetCursor(gCursorHand);
                    return TRUE;
                }
            }
            ClearInfotip();
            return DefWindowProc(hwnd, message, wParam, lParam);

        case WM_LBUTTONDOWN:
            gClickedURL = GetAboutLink(NULL, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            break;

        case WM_LBUTTONUP:
            url = GetAboutLink(NULL, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            if (url && url == gClickedURL)
                LaunchBrowser(url);
            break;

        case WM_CHAR:
            if (VK_ESCAPE == wParam)
                DestroyWindow(hwnd);
            break;

        case WM_DESTROY:
            ClearInfotip();
            assert(gHwndAbout);
            gHwndAbout = NULL;
            break;

        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}

static void OnPaint(WindowInfo *win)
{
    ClientRect rc(win->hwndCanvas);

    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(win->hwndCanvas, &ps);
    UpdateAboutLayoutInfo(win->hwndCanvas, win->buffer->GetDC(), &rc);
    DrawAbout(win->hwndCanvas, win->buffer->GetDC(), rc);
#ifdef NEW_START_PAGE
    // add "Show frequently read" link
    if (!gRestrictedUse && gGlobalPrefs.m_rememberOpenedFiles) {
        // TODO: translate
        RectI rect = DrawBottomRightLink(win->hwndCanvas, win->buffer->GetDC(), _T("Show frequently read"));
        gLinkInfo[dimof(gLinkInfo) - 1].url = _T("<View,ShowList>");
        gLinkInfo[dimof(gLinkInfo) - 1].rect = rect;
    }
#endif
    win->buffer->Flush(hdc);
    EndPaint(win->hwndCanvas, &ps);
}

LRESULT HandleWindowAboutMsg(WindowInfo *win, HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, bool& handled)
{
    POINT        pt;

    assert(win->IsAboutWindow());
    handled = false;

    switch (message)
    {
        case WM_PAINT:
            OnPaint(win);
            handled = true;
            return 0;

        case WM_SETCURSOR:
            if (GetCursorPos(&pt) && ScreenToClient(hwnd, &pt)) {
                RectI rect;
                const TCHAR *url = GetAboutLink(NULL, pt.x, pt.y, &rect);
                if (url) {
                    if (*url != '<')
                        win->CreateInfotip(url, rect);
                    SetCursor(gCursorHand);
                    handled = true;
                    return TRUE;
                }
            }
            break;

        case WM_LBUTTONDOWN:
            // remember a link under so that on mouse up we only activate
            // link if mouse up is on the same link as mouse down
            win->url = GetAboutLink(win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            handled = true;
            break;

        case WM_LBUTTONUP:
            const TCHAR *url = GetAboutLink(win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            if (url && url == win->url) {
#ifdef NEW_START_PAGE
                if (Str::Eq(url, _T("<View,ShowList>"))) {
                    gGlobalPrefs.m_showStartPage = true;
                    win->RedrawAll(true);
                } else
#endif
                    LaunchBrowser(url);
            }
            win->url = NULL;
            handled = true;
            break;            
    }

    return 0;
}
