/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "SumatraPDF.h"
#include "WindowInfo.h"
#include "SumatraAbout.h"
#include "translations.h"
#include "Version.h"
#include "WinUtil.h"
#include "Scopes.h"

#define ABOUT_LINE_OUTER_SIZE       2
#define ABOUT_LINE_SEP_SIZE         1
#define ABOUT_LEFT_RIGHT_SPACE_DX   8
#define ABOUT_MARGIN_DX            10
#define ABOUT_BOX_MARGIN_DY         6
#define ABOUT_BORDER_COL            RGB(0, 0, 0)
#define ABOUT_TXT_DY                6
#define ABOUT_RECT_PADDING          8
#define ABOUT_INNER_PADDING         6

#define ABOUT_WIN_TITLE         _TR("About SumatraPDF")

#define COL_BLUE_LINK           RGB(0x00, 0x20, 0xa0)

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
    // FreeType's license requires us to advertise that we use it
    { _T("font rendering"), _T("FreeType"),             _T("http://www.freetype.org/") },
    { _T("program icon"),   _T("Zenon"),                _T("http://www.flashvidz.tk/") },
    { _T("toolbar icons"),  _T("Yusuke Kamiyamane"),    _T("http://p.yusukekamiyamane.com/") },
    { _T("translators"),    _T("The Translators"),      _T("http://blog.kowalczyk.info/software/sumatrapdf/translators.html") },
    { _T("translations"),   _T("Contribute translation"), _T("http://blog.kowalczyk.info/software/sumatrapdf/translations.html") },
    // Note: Must be on the last line, as it's dynamically hidden based on enableTeXEnhancements
    { _T("synctex"),        _T("J\xE9rome Laurens"),    _T("http://itexmac.sourceforge.net/SyncTeX.html") },
    { NULL, NULL, NULL }
};

static Vec<StaticLinkInfo> gLinkInfo;

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
    TextOut(hdc, pt.x, pt.y, txt, str::Len(txt));
#else
    // colorful version
    COLORREF cols[] = { COL1, COL2, COL3, COL4, COL5, COL5, COL4, COL3, COL2, COL1 };
    for (size_t i = 0; i < str::Len(txt); i++) {
        SetTextColor(hdc, cols[i % dimof(cols)]);
        TextOut(hdc, pt.x, pt.y, txt + i, 1);

        SIZE txtSize;
        GetTextExtentPoint32(hdc, txt + i, 1, &txtSize);
        pt.x += txtSize.cx;
    }
#endif
}

static SizeI CalcSumatraVersionSize(HDC hdc)
{
    SizeI result;

    ScopedFont fontSumatraTxt(GetSimpleFont(hdc, SUMATRA_TXT_FONT, SUMATRA_TXT_FONT_SIZE));
    ScopedFont fontVersionTxt(GetSimpleFont(hdc, VERSION_TXT_FONT, VERSION_TXT_FONT_SIZE));
    HGDIOBJ oldFont = SelectObject(hdc, fontSumatraTxt);

    SIZE txtSize;
    /* calculate minimal top box size */
    const TCHAR *txt = APP_NAME_STR;
    GetTextExtentPoint32(hdc, txt, (int)str::Len(txt), &txtSize);
    result.dy = txtSize.cy + ABOUT_BOX_MARGIN_DY * 2;
    result.dx = txtSize.cx;

    /* consider version and version-sub strings */
    SelectObject(hdc, fontVersionTxt);
    txt = VERSION_TXT;
    GetTextExtentPoint32(hdc, txt, (int)str::Len(txt), &txtSize);
    int minWidth = txtSize.cx;
    txt = VERSION_SUB_TXT;
    GetTextExtentPoint32(hdc, txt, (int)str::Len(txt), &txtSize);
    txtSize.cx = max(txtSize.cx, minWidth);
    result.dx += 2 * (txtSize.cx + ABOUT_INNER_PADDING);

    SelectObject(hdc, oldFont);

    return result;
}

static void DrawSumatraVersion(HDC hdc, RectI rect)
{
    ScopedFont fontSumatraTxt(GetSimpleFont(hdc, SUMATRA_TXT_FONT, SUMATRA_TXT_FONT_SIZE));
    ScopedFont fontVersionTxt(GetSimpleFont(hdc, VERSION_TXT_FONT, VERSION_TXT_FONT_SIZE));
    HGDIOBJ oldFont = SelectObject(hdc, fontSumatraTxt);

    SetBkMode(hdc, TRANSPARENT);

    SIZE txtSize;
    const TCHAR *txt = APP_NAME_STR;
    GetTextExtentPoint32(hdc, txt, (int)str::Len(txt), &txtSize);
    RectI mainRect(rect.x + (rect.dx - txtSize.cx) / 2,
                   rect.y + (rect.dy - txtSize.cy) / 2, txtSize.cx, txtSize.cy);
    DrawSumatraPDF(hdc, mainRect.TL());

    SetTextColor(hdc, WIN_COL_BLACK);
    SelectObject(hdc, fontVersionTxt);
    PointI pt(mainRect.x + mainRect.dx + ABOUT_INNER_PADDING, mainRect.y);
    txt = VERSION_TXT;
    TextOut(hdc, pt.x, pt.y, txt, (int)str::Len(txt));
    txt = VERSION_SUB_TXT;
    TextOut(hdc, pt.x, pt.y + 16, txt, (int)str::Len(txt));

    SelectObject(hdc, oldFont);
}

static RectI DrawBottomRightLink(HWND hwnd, HDC hdc, const TCHAR *txt)
{
    ScopedFont fontLeftTxt(GetSimpleFont(hdc, _T("MS Shell Dlg"), 14));
    HPEN penLinkLine = CreatePen(PS_SOLID, 1, COL_BLUE_LINK);

    HGDIOBJ origFont = SelectObject(hdc, fontLeftTxt); /* Just to remember the orig font */

    SetTextColor(hdc, COL_BLUE_LINK);
    SetBkMode(hdc, TRANSPARENT);
    ClientRect rc(hwnd);

    SIZE txtSize;
    GetTextExtentPoint32(hdc, txt, (int)str::Len(txt), &txtSize);
    RectI rect(rc.dx - txtSize.cx - ABOUT_INNER_PADDING,
               rc.y + rc.dy - txtSize.cy - ABOUT_INNER_PADDING, txtSize.cx, txtSize.cy);
    if (IsUIRightToLeft())
        rect.x = ABOUT_INNER_PADDING;
    DrawText(hdc, txt, -1, &rect.ToRECT(), IsUIRightToLeft() ? DT_RTLREADING : DT_LEFT);

    SelectObject(hdc, penLinkLine);
    PaintLine(hdc, RectI(rect.x, rect.y + rect.dy, rect.dx, 0));

    SelectObject(hdc, origFont);
    DeleteObject(penLinkLine);

    // make the click target larger
    rect.Inflate(ABOUT_INNER_PADDING, ABOUT_INNER_PADDING);
    return rect;
}

/* Draws the about screen and remembers some state for hyperlinking.
   It transcribes the design I did in graphics software - hopeless
   to understand without seeing the design. */
static void DrawAbout(HWND hwnd, HDC hdc, RectI rect, Vec<StaticLinkInfo>& linkInfo)
{
    HPEN penBorder = CreatePen(PS_SOLID, ABOUT_LINE_OUTER_SIZE, WIN_COL_BLACK);
    HPEN penDivideLine = CreatePen(PS_SOLID, ABOUT_LINE_SEP_SIZE, WIN_COL_BLACK);
    HPEN penLinkLine = CreatePen(PS_SOLID, ABOUT_LINE_SEP_SIZE, COL_BLUE_LINK);

    ScopedFont fontLeftTxt(GetSimpleFont(hdc, LEFT_TXT_FONT, LEFT_TXT_FONT_SIZE));
    ScopedFont fontRightTxt(GetSimpleFont(hdc, RIGHT_TXT_FONT, RIGHT_TXT_FONT_SIZE));

    HGDIOBJ origFont = SelectObject(hdc, fontLeftTxt); /* Just to remember the orig font */

    ClientRect rc(hwnd);
    FillRect(hdc, &rc.ToRECT(), gBrushAboutBg);

    /* render title */
    RectI titleRect(rect.TL(), CalcSumatraVersionSize(hdc));

    SelectObject(hdc, gBrushAboutBg);
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
        TextOut(hdc, el->leftPos.x, el->leftPos.y, el->leftTxt, (int)str::Len(el->leftTxt));

    /* render text on the right */
    SelectObject(hdc, fontRightTxt);
    SelectObject(hdc, penLinkLine);
    linkInfo.Reset();
    for (AboutLayoutInfoEl *el = gAboutLayoutInfo; el->leftTxt; el++) {
        bool hasUrl = HasPermission(Perm_DiskAccess) && el->url;
        SetTextColor(hdc, hasUrl ? COL_BLUE_LINK : ABOUT_BORDER_COL);
        TextOut(hdc, el->rightPos.x, el->rightPos.y, el->rightTxt, (int)str::Len(el->rightTxt));

        if (hasUrl) {
            int underlineY = el->rightPos.y + el->rightPos.dy - 3;
            PaintLine(hdc, RectI(el->rightPos.x, underlineY, el->rightPos.dx, 0));
            linkInfo.Append(StaticLinkInfo(el->rightPos, el->url, el->url));
        }
    }

    SelectObject(hdc, penDivideLine);
    RectI divideLine(gAboutLayoutInfo[0].rightPos.x - ABOUT_LEFT_RIGHT_SPACE_DX,
                     rect.y + titleRect.dy + 4, 0, rect.y + rect.dy - 4 - gAboutLayoutInfo[0].rightPos.y);
    PaintLine(hdc, divideLine);

    SelectObject(hdc, origFont);

    DeleteObject(penBorder);
    DeleteObject(penDivideLine);
    DeleteObject(penLinkLine);
}

static void UpdateAboutLayoutInfo(HWND hwnd, HDC hdc, RectI *rect)
{
    ScopedFont fontLeftTxt(GetSimpleFont(hdc, LEFT_TXT_FONT, LEFT_TXT_FONT_SIZE));
    ScopedFont fontRightTxt(GetSimpleFont(hdc, RIGHT_TXT_FONT, RIGHT_TXT_FONT_SIZE));

    HGDIOBJ origFont = SelectObject(hdc, fontLeftTxt);

    /* show/hide the SyncTeX attribution line */
    assert(!gAboutLayoutInfo[dimof(gAboutLayoutInfo) - 2].leftTxt || str::Eq(gAboutLayoutInfo[dimof(gAboutLayoutInfo) - 2].leftTxt, _T("synctex")));
    if (gGlobalPrefs.enableTeXEnhancements)
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
        GetTextExtentPoint32(hdc, el->leftTxt, (int)str::Len(el->leftTxt), &txtSize);
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
        GetTextExtentPoint32(hdc, el->rightTxt, (int)str::Len(el->rightTxt), &txtSize);
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
}

static void OnPaintAbout(HWND hwnd)
{
    PAINTSTRUCT ps;
    RectI rc;
    HDC hdc = BeginPaint(hwnd, &ps);
    UpdateAboutLayoutInfo(hwnd, hdc, &rc);
    DrawAbout(hwnd, hdc, rc, gLinkInfo);
    EndPaint(hwnd, &ps);
}

const TCHAR *GetStaticLink(Vec<StaticLinkInfo>& linkInfo, int x, int y, StaticLinkInfo *info)
{
    if (!HasPermission(Perm_DiskAccess))
        return NULL;

    PointI pt(x, y);
    for (size_t i = 0; i < linkInfo.Count(); i++) {
        if (linkInfo.At(i).rect.Inside(pt)) {
            if (info)
                *info = linkInfo.At(i);
            return linkInfo.At(i).target;
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

static void CreateInfotipForLink(StaticLinkInfo& linkInfo)
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
    ti.lpszText = (TCHAR *)linkInfo.infotip;
    ti.rect = linkInfo.rect.ToRECT();

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
                StaticLinkInfo linkInfo;
                if (GetStaticLink(gLinkInfo, pt.x, pt.y, &linkInfo)) {
                    CreateInfotipForLink(linkInfo);
                    SetCursor(gCursorHand);
                    return TRUE;
                }
            }
            ClearInfotip();
            return DefWindowProc(hwnd, message, wParam, lParam);

        case WM_LBUTTONDOWN:
            gClickedURL = GetStaticLink(gLinkInfo, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            break;

        case WM_LBUTTONUP:
            url = GetStaticLink(gLinkInfo, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
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

void DrawAboutPage(WindowInfo& win, HDC hdc)
{
    ClientRect rc(win.hwndCanvas);
    UpdateAboutLayoutInfo(win.hwndCanvas, hdc, &rc);
    DrawAbout(win.hwndCanvas, hdc, rc, win.staticLinks);
    if (HasPermission(Perm_SavePreferences | Perm_DiskAccess) && gGlobalPrefs.rememberOpenedFiles) {
        RectI rect = DrawBottomRightLink(win.hwndCanvas, hdc, _TR("Show frequently read"));
        win.staticLinks.Append(StaticLinkInfo(rect, SLINK_LIST_SHOW));
    }
}

/* alternate static page to display when no document is loaded */

#include "FileUtil.h"
#include "AppTools.h"
#include "FileHistory.h"

#define DOCLIST_SEPARATOR_DY        2
#define DOCLIST_THUMBNAIL_BORDER_W  1
#define DOCLIST_MARGIN_LEFT        40
#define DOCLIST_MARGIN_BETWEEN_X   30
#define DOCLIST_MARGIN_RIGHT       40
#define DOCLIST_MARGIN_TOP         60
#define DOCLIST_MARGIN_BETWEEN_Y   50
#define DOCLIST_MARGIN_BOTTOM      40
#define DOCLIST_MAX_THUMBNAILS_X    5
#define DOCLIST_BOTTOM_BOX_DY      50

static bool LoadThumbnail(DisplayState& state);

void DrawStartPage(WindowInfo& win, HDC hdc, FileHistory& fileHistory, bool invertColors)
{
    HPEN penBorder = CreatePen(PS_SOLID, DOCLIST_SEPARATOR_DY, WIN_COL_BLACK);
    HPEN penThumbBorder = CreatePen(PS_SOLID, DOCLIST_THUMBNAIL_BORDER_W, WIN_COL_BLACK);
    HPEN penLinkLine = CreatePen(PS_SOLID, 1, COL_BLUE_LINK);

    ScopedFont fontSumatraTxt(GetSimpleFont(hdc, _T("MS Shell Dlg"), 24));
    ScopedFont fontLeftTxt(GetSimpleFont(hdc, _T("MS Shell Dlg"), 14));

    HGDIOBJ origFont = SelectObject(hdc, fontSumatraTxt); /* Just to remember the orig font */

    ClientRect rc(win.hwndCanvas);
    FillRect(hdc, &rc.ToRECT(), gBrushAboutBg);

    SelectObject(hdc, gBrushAboutBg);
    SelectObject(hdc, penBorder);

    bool isRtl = IsUIRightToLeft();

    /* render title */
    RectI titleBox = RectI(PointI(0, 0), CalcSumatraVersionSize(hdc));
    titleBox.x = rc.dx - titleBox.dx - 3;
    DrawSumatraVersion(hdc, titleBox);
    PaintLine(hdc, RectI(0, titleBox.dy, rc.dx, 0));

    /* render recent files list */
    SelectObject(hdc, penThumbBorder);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, WIN_COL_BLACK);

    rc.y += titleBox.dy;
    rc.dy -= titleBox.dy;
#ifndef BLACK_ON_YELLOW
    FillRect(hdc, &rc.ToRECT(), gBrushAboutBg);
#else
    FillRect(hdc, &rc.ToRECT(), gBrushNoDocBg);
#endif
    rc.dy -= DOCLIST_BOTTOM_BOX_DY;

    Vec<DisplayState *> *list = fileHistory.GetFrequencyOrder();

    int width = limitValue((rc.dx - DOCLIST_MARGIN_LEFT - DOCLIST_MARGIN_RIGHT + DOCLIST_MARGIN_BETWEEN_X) / (THUMBNAIL_DX + DOCLIST_MARGIN_BETWEEN_X), 1, DOCLIST_MAX_THUMBNAILS_X);
    int height = min((rc.dy - DOCLIST_MARGIN_TOP - DOCLIST_MARGIN_BOTTOM + DOCLIST_MARGIN_BETWEEN_Y) / (THUMBNAIL_DY + DOCLIST_MARGIN_BETWEEN_Y), FILE_HISTORY_MAX_FREQUENT / width);
    PointI offset(rc.x + DOCLIST_MARGIN_LEFT + (rc.dx - width * THUMBNAIL_DX - (width - 1) * DOCLIST_MARGIN_BETWEEN_X - DOCLIST_MARGIN_LEFT - DOCLIST_MARGIN_RIGHT) / 2, rc.y + DOCLIST_MARGIN_TOP);
    if (offset.x < ABOUT_INNER_PADDING)
        offset.x = ABOUT_INNER_PADDING;
    else if (list->Count() == 0)
        offset.x = DOCLIST_MARGIN_LEFT;

    SelectObject(hdc, fontSumatraTxt);
    SIZE txtSize;
    const TCHAR *txt = _TR("Frequently Read");
    GetTextExtentPoint32(hdc, txt, (int)str::Len(txt), &txtSize);
    RectI headerRect(offset.x, rc.y + (DOCLIST_MARGIN_TOP - txtSize.cy) / 2, txtSize.cx, txtSize.cy);
    if (isRtl)
        headerRect.x = rc.dx - offset.x - headerRect.dx;
    DrawText(hdc, txt, -1, &headerRect.ToRECT(), (isRtl ? DT_RTLREADING : DT_LEFT) | DT_NOPREFIX);

    SelectObject(hdc, fontLeftTxt);
    SelectObject(hdc, GetStockBrush(NULL_BRUSH));

    win.staticLinks.Reset();
    for (int h = 0; h < height; h++) {
        for (int w = 0; w < width; w++) {
            if (h * width + w >= (int)list->Count()) {
                // display the "Open a document" link right below the last row
                height = w > 0 ? h + 1 : h;
                break;
            }
            DisplayState *state = list->At(h * width + w);

            RectI page(offset.x + w * (int)(THUMBNAIL_DX + DOCLIST_MARGIN_BETWEEN_X * win.uiDPIFactor),
                       offset.y + h * (int)(THUMBNAIL_DY + DOCLIST_MARGIN_BETWEEN_Y * win.uiDPIFactor),
                       THUMBNAIL_DX, THUMBNAIL_DY);
            if (isRtl)
                page.x = rc.dx - page.x - page.dx;
            if (state->thumbnail || LoadThumbnail(*state)) {
                HRGN clip = CreateRoundRectRgn(page.x, page.y, page.x + page.dx, page.y + page.dy, 10, 10);
                SelectClipRgn(hdc, clip);
                RectI thumb(page.TL(), state->thumbnail->Size());
                assert(thumb.dx == page.dx);
                if (invertColors)
                    InvertBitmapColors(state->thumbnail->GetBitmap());
                state->thumbnail->StretchDIBits(hdc, thumb);
                if (invertColors)
                    InvertBitmapColors(state->thumbnail->GetBitmap());
                SelectClipRgn(hdc, NULL);
                DeleteObject(clip);
            }
            RoundRect(hdc, page.x, page.y, page.x + page.dx, page.y + page.dy, 10, 10);

            int iconSpace = (int)(20 * win.uiDPIFactor);
            RectI rect(page.x + iconSpace, page.y + THUMBNAIL_DY + 3, THUMBNAIL_DX - iconSpace, iconSpace);
            if (isRtl)
                rect.x -= iconSpace;
            DrawText(hdc, path::GetBaseName(state->filePath), -1, &rect.ToRECT(), DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX | (isRtl ? DT_RIGHT : DT_LEFT));

            SHFILEINFO sfi;
            HIMAGELIST himl = (HIMAGELIST)SHGetFileInfo(state->filePath, 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
            ImageList_Draw(himl, sfi.iIcon, hdc,
                           isRtl ? page.x + page.dx - (int)(16 * win.uiDPIFactor) : page.x,
                           rect.y, ILD_TRANSPARENT);

            win.staticLinks.Append(StaticLinkInfo(rect.Union(page), state->filePath, state->filePath));
        }
    }
    delete list;

    /* render bottom links */
    rc.y += DOCLIST_MARGIN_TOP + height * THUMBNAIL_DY + (height - 1) * DOCLIST_MARGIN_BETWEEN_Y + DOCLIST_MARGIN_BOTTOM;
    rc.dy = DOCLIST_BOTTOM_BOX_DY;

    SetTextColor(hdc, COL_BLUE_LINK);
    SelectObject(hdc, penLinkLine);

    HIMAGELIST himl = (HIMAGELIST)SendMessage(win.hwndToolbar, TB_GETIMAGELIST, 0, 0);
    RectI rectIcon(offset.x, rc.y, 0, 0);
    ImageList_GetIconSize(himl, &rectIcon.dx, &rectIcon.dy);
    rectIcon.y += (rc.dy - rectIcon.dy) / 2;
    if (isRtl)
        rectIcon.x = rc.dx - offset.x - rectIcon.dx;
    ImageList_Draw(himl, 0 /* index of Open icon */, hdc, rectIcon.x, rectIcon.y, ILD_NORMAL);

    txt = _TR("Open a document...");
    GetTextExtentPoint32(hdc, txt, (int)str::Len(txt), &txtSize);
    RectI rect(offset.x + rectIcon.dx + 3, rc.y + (rc.dy - txtSize.cy) / 2, txtSize.cx, txtSize.cy);
    if (isRtl)
        rect.x = rectIcon.x - rect.dx - 3;
    DrawText(hdc, txt, -1, &rect.ToRECT(), isRtl ? DT_RTLREADING : DT_LEFT);
    PaintLine(hdc, RectI(rect.x, rect.y + rect.dy, rect.dx, 0));
    // make the click target larger
    rect = rect.Union(rectIcon);
    rect.Inflate(10, 10);
    win.staticLinks.Append(StaticLinkInfo(rect, SLINK_OPEN_FILE));

    rect = DrawBottomRightLink(win.hwndCanvas, hdc, _TR("Hide frequently read"));
    win.staticLinks.Append(StaticLinkInfo(rect, SLINK_LIST_HIDE));

    SelectObject(hdc, origFont);

    DeleteObject(penBorder);
    DeleteObject(penThumbBorder);
    DeleteObject(penLinkLine);
}

// TODO: create in TEMP directory instead?
static TCHAR *GetThumbnailPath(const TCHAR *filePath)
{
    // create a fingerprint of a (normalized) path for the file name
    // I'd have liked to also include the file's last modification time
    // in the fingerprint (much quicker than hashing the entire file's
    // content), but that's too expensive for files on slow drives
    unsigned char digest[16];
    ScopedMem<char> pathU(str::conv::ToUtf8(filePath));
    if (path::IsOnRemovableDrive(filePath))
        pathU[0] = '?'; // ignore the drive letter, if it might change
    CalcMD5Digest((unsigned char *)pathU.Get(), str::Len(pathU), digest);
    ScopedMem<char> fingerPrint(str::MemToHex(digest, 16));

    ScopedMem<TCHAR> thumbsPath(AppGenDataFilename(THUMBNAILS_DIR_NAME));
    if (!thumbsPath)
        return NULL;
    ScopedMem<TCHAR> fname(str::conv::FromAnsi(fingerPrint));

    return str::Format(_T("%s\\%s.png"), thumbsPath, fname);
}

// removes thumbnails that don't belong to any frequently used item in file history
void CleanUpThumbnailCache(FileHistory& fileHistory)
{
    ScopedMem<TCHAR> thumbsPath(AppGenDataFilename(THUMBNAILS_DIR_NAME));
    if (!thumbsPath)
        return;
    ScopedMem<TCHAR> pattern(path::Join(thumbsPath, _T("*.png")));

    StrVec files;
    WIN32_FIND_DATA fdata;

    HANDLE hfind = FindFirstFile(pattern, &fdata);
    if (INVALID_HANDLE_VALUE == hfind)
        return;
    do {
        if (!(fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            files.Append(str::Dup(fdata.cFileName));
    } while (FindNextFile(hfind, &fdata));
    FindClose(hfind);

    Vec<DisplayState *> *list = fileHistory.GetFrequencyOrder();
    for (size_t i = 0; i < list->Count() && i < FILE_HISTORY_MAX_FREQUENT * 2; i++) {
        ScopedMem<TCHAR> bmpPath(GetThumbnailPath(list->At(i)->filePath));
        if (!bmpPath)
            continue;
        int idx = files.Find(path::GetBaseName(bmpPath));
        if (idx != -1) {
            free(files.At(idx));
            files.RemoveAt(idx);
        }
    }
    delete list;

    for (size_t i = 0; i < files.Count(); i++) {
        ScopedMem<TCHAR> bmpPath(path::Join(thumbsPath, files.At(i)));
        file::Delete(bmpPath);
    }
}

static bool LoadThumbnail(DisplayState& ds)
{
    if (ds.thumbnail)
        delete ds.thumbnail;
    ds.thumbnail = NULL;

    ScopedMem<TCHAR> bmpPath(GetThumbnailPath(ds.filePath));
    if (!bmpPath)
        return false;

    ds.thumbnail = LoadRenderedBitmap(bmpPath);
    return ds.thumbnail != NULL;
}

bool HasThumbnail(DisplayState& ds)
{
    if (!ds.thumbnail && !LoadThumbnail(ds))
        return false;

    ScopedMem<TCHAR> bmpPath(GetThumbnailPath(ds.filePath));
    if (!bmpPath)
        return true;
    FILETIME bmpTime = file::GetModificationTime(bmpPath);
    FILETIME fileTime = file::GetModificationTime(ds.filePath);
    // delete the thumbnail if the file is newer than the thumbnail
    if (FileTimeDiffInSecs(fileTime, bmpTime) > 0) {
        delete ds.thumbnail;
        ds.thumbnail = NULL;
    }

    return ds.thumbnail != NULL;
}

void SaveThumbnail(DisplayState& ds)
{
    if (!ds.thumbnail)
        return;

    ScopedMem<TCHAR> bmpPath(GetThumbnailPath(ds.filePath));
    if (!bmpPath)
        return;
    ScopedMem<TCHAR> thumbsPath(path::GetDir(bmpPath));
    if (dir::Create(thumbsPath))
        SaveRenderedBitmap(ds.thumbnail, bmpPath);
}
