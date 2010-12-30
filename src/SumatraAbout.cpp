/* Copyright 2006-2010 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "SumatraPDF.h"
#include "WindowInfo.h"
#include "SumatraAbout.h"
#include "translations.h"
#include "Version.h"
#include "AppPrefs.h"

#define ABOUT_LINE_OUTER_SIZE       2
#define ABOUT_LINE_SEP_SIZE         1
#define ABOUT_LEFT_RIGHT_SPACE_DX   8
#define ABOUT_MARGIN_DX            10
#define ABOUT_BOX_MARGIN_DY         6
#define ABOUT_BORDER_COL            RGB(0,0,0)
#define ABOUT_TXT_DY                6
#define ABOUT_RECT_PADDING          8

#define ABOUT_WIN_TITLE         _TR("About SumatraPDF")

#ifndef SUMATRA_TXT
#define SUMATRA_TXT             _T("SumatraPDF")
#endif
#define SUMATRA_TXT_FONT        _T("Arial Black")
#define SUMATRA_TXT_FONT_SIZE   24

#define VERSION_TXT_FONT        _T("Arial Black")
#define VERSION_TXT_FONT_SIZE   12

#define VERSION_TXT             _T("v") CURR_VERSION_STR
#ifdef SVN_PRE_RELEASE_VER
 #define VERSION_SUB_TXT        _T("Pre-release")
#else
 #if defined(DEBUG) || !defined(BUILD_RM_VERSION)
  #define VERSION_SUB_TXT       _T("")
 #else
  #define VERSION_SUB_TXT       _T("Adapted by RM")
 #endif
#endif

static HWND gHwndAbout;

static AboutLayoutInfoEl gAboutLayoutInfo[] = {
    { _T("website"),        _T("SumatraPDF website"),   _T("http://blog.kowalczyk.info/software/sumatrapdf"), 0 },
    { _T("forums"),         _T("SumatraPDF forums"),    _T("http://blog.kowalczyk.info/forum_sumatra"), 0 },
    { _T("programming"),    _T("Krzysztof Kowalczyk"),  _T("http://blog.kowalczyk.info"), 0 },
    { _T("programming"),    _T("Simon B\xFCnzli"),      _T("http://www.zeniko.ch/#SumatraPDF"), 0 },
    { _T("programming"),    _T("William Blum"),         _T("http://william.famille-blum.org/"), 0 },
#ifdef _TEX_ENHANCEMENT
    { _T("note"),           _T("TeX build"),            _T("http://william.famille-blum.org/software/sumatra/index.html"), 0 },
#endif 
#ifdef SVN_PRE_RELEASE_VER
    { _T("a note"),         _T("Pre-release version, for testing only!"), NULL, 0 },
#endif
#ifdef DEBUG
    { _T("a note"),         _T("Debug version, for testing only!"), NULL, 0 },
#endif
    { _T("pdf rendering"),  _T("MuPDF"),                _T("http://mupdf.com"), 0 },
    { _T("program icon"),   _T("Zenon"),                _T("http://www.flashvidz.tk/"), 0 },
    { _T("toolbar icons"),  _T("Mark James"),           _T("http://www.famfamfam.com/lab/icons/silk/"), 0 },
    { _T("translators"),    _T("The Translators"),      _T("http://blog.kowalczyk.info/software/sumatrapdf/translators.html"), 0 },
    { _T("translations"),   _T("Contribute translation"), _T("http://blog.kowalczyk.info/software/sumatrapdf/translations.html"), 0 },
#ifdef _TEX_ENHANCEMENT
    { _T("SyncTeX"),        _T("J\xE9rome Laurens"),    _T("http://itexmac.sourceforge.net/SyncTeX.html"), 0 },
#endif 
    { NULL, NULL, NULL, 0 }
};

#define COL1 RGB(196, 64, 50)
#define COL2 RGB(227, 107, 35)
#define COL3 RGB(93,  160, 40)
#define COL4 RGB(69, 132, 190)
#define COL5 RGB(112, 115, 207)

void DrawSumatraPDF(HDC hdc, int x, int y)
{
    const TCHAR *txt = SUMATRA_TXT;
#ifdef BLACK_ON_YELLOW
    // simple black version
    SetTextColor(hdc, ABOUT_BORDER_COL);
    TextOut(hdc, x, y, txt, lstrlen(txt));
#else
    // colorful version
    COLORREF cols[] = { COL1, COL2, COL3, COL4, COL5, COL5, COL4, COL3, COL2, COL1 };
    for (int i = 0; i < lstrlen(txt); i++) {
        SetTextColor(hdc, cols[i % dimof(cols)]);
        TextOut(hdc, x, y, txt + i, 1);

        SIZE txtSize;
        GetTextExtentPoint32(hdc, txt + i, 1, &txtSize);
        x += txtSize.cx;
    }
#endif
}

/* Draws the about screen a remember some state for hyperlinking.
   It transcribes the design I did in graphics software - hopeless
   to understand without seeing the design. */
void DrawAbout(HWND hwnd, HDC hdc, RECT *rect)
{
    SIZE            txtSize;
    int             totalDx, totalDy;
    int             leftLargestDx;
    int             sumatraPdfTxtDx, sumatraPdfTxtDy;
    int             linePosX, linePosY, lineDy;
    int             offX, offY;
    int             x, y;
    int             boxDy;

    HBRUSH brushBg = CreateSolidBrush(gGlobalPrefs.m_bgColor);

    HPEN penBorder = CreatePen(PS_SOLID, ABOUT_LINE_OUTER_SIZE, WIN_COL_BLACK);
    HPEN penDivideLine = CreatePen(PS_SOLID, ABOUT_LINE_SEP_SIZE, WIN_COL_BLACK);
    HPEN penLinkLine = CreatePen(PS_SOLID, ABOUT_LINE_SEP_SIZE, COL_BLUE_LINK);

    HFONT fontSumatraTxt = Win32_Font_GetSimple(hdc, SUMATRA_TXT_FONT, SUMATRA_TXT_FONT_SIZE);
    HFONT fontVersionTxt = Win32_Font_GetSimple(hdc, VERSION_TXT_FONT, VERSION_TXT_FONT_SIZE);
    HFONT fontLeftTxt = Win32_Font_GetSimple(hdc, LEFT_TXT_FONT, LEFT_TXT_FONT_SIZE);
    HFONT fontRightTxt = Win32_Font_GetSimple(hdc, RIGHT_TXT_FONT, RIGHT_TXT_FONT_SIZE);

    HGDIOBJ origFont = SelectObject(hdc, fontSumatraTxt); /* Just to remember the orig font */

    SetBkMode(hdc, TRANSPARENT);

    RECT rc;
    GetClientRect(hwnd, &rc);
    FillRect(hdc, &rc, brushBg);

    SelectObject(hdc, brushBg);
    SelectObject(hdc, penBorder);

    offX = rect->left;
    offY = rect->top;
    totalDx = RectDx(rect);
    totalDy = RectDy(rect);

    /* render title */
    const TCHAR *txt = SUMATRA_TXT;
    GetTextExtentPoint32(hdc, txt, lstrlen(txt), &txtSize);
    sumatraPdfTxtDx = txtSize.cx;
    sumatraPdfTxtDy = txtSize.cy;

    boxDy = sumatraPdfTxtDy + ABOUT_BOX_MARGIN_DY * 2;

    Rectangle(hdc, offX, offY + ABOUT_LINE_OUTER_SIZE, offX + totalDx, offY + boxDy + ABOUT_LINE_OUTER_SIZE);

    SelectObject(hdc, fontSumatraTxt);
    x = offX + (totalDx - sumatraPdfTxtDx) / 2;
    y = offY + (boxDy - sumatraPdfTxtDy) / 2;
    DrawSumatraPDF(hdc, x, y);

    SetTextColor(hdc, ABOUT_BORDER_COL);
    SelectObject(hdc, fontVersionTxt);
    x = offX + (totalDx - sumatraPdfTxtDx) / 2 + sumatraPdfTxtDx + 6;
    y = offY + (boxDy - sumatraPdfTxtDy) / 2;
    txt = VERSION_TXT;
    TextOut(hdc, x, y, txt, lstrlen(txt));
    txt = VERSION_SUB_TXT;
    TextOut(hdc, x, y + 16, txt, lstrlen(txt));

    SetTextColor(hdc, ABOUT_BORDER_COL);

    offY += boxDy;
    Rectangle(hdc, offX, offY, offX + totalDx, offY + totalDy - boxDy);

    /* render text on the left*/
    leftLargestDx = 0;
    SelectObject(hdc, fontLeftTxt);
    for (AboutLayoutInfoEl *el = gAboutLayoutInfo; el->leftTxt; el++) {
        TextOut(hdc, el->leftPos.x, el->leftPos.y, el->leftTxt, lstrlen(el->leftTxt));
        if (leftLargestDx < el->leftPos.dx)
            leftLargestDx = el->leftPos.dx;
    }

    /* render text on the right */
    SelectObject(hdc, fontRightTxt);
    for (AboutLayoutInfoEl *el = gAboutLayoutInfo; el->leftTxt; el++) {
        bool hasUrl = !gRestrictedUse && el->url;
        SetTextColor(hdc, hasUrl ? COL_BLUE_LINK : ABOUT_BORDER_COL);

        TextOut(hdc, el->rightPos.x, el->rightPos.y, el->rightTxt, lstrlen(el->rightTxt));

        if (!hasUrl)
            continue;

        int underlineY = el->rightPos.y + el->rightPos.dy - 3;
        SelectObject(hdc, penLinkLine);
        MoveToEx(hdc, el->rightPos.x, underlineY, NULL);
        LineTo(hdc, el->rightPos.x + el->rightPos.dx, underlineY);    
    }

    linePosX = ABOUT_LINE_OUTER_SIZE + ABOUT_MARGIN_DX + leftLargestDx + ABOUT_LEFT_RIGHT_SPACE_DX;
    linePosY = 4;
    lineDy = (dimof(gAboutLayoutInfo)-1) * (gAboutLayoutInfo[0].rightPos.dy + ABOUT_TXT_DY);

    SelectObject(hdc, penDivideLine);
    MoveToEx(hdc, linePosX + offX, linePosY + offY, NULL);
    LineTo(hdc, linePosX + offX, linePosY + lineDy + offY);

    SelectObject(hdc, origFont);
    Win32_Font_Delete(fontSumatraTxt);
    Win32_Font_Delete(fontVersionTxt);
    Win32_Font_Delete(fontLeftTxt);
    Win32_Font_Delete(fontRightTxt);

    DeleteObject(brushBg);
    DeleteObject(penBorder);
    DeleteObject(penDivideLine);
    DeleteObject(penLinkLine);
}

void UpdateAboutLayoutInfo(HWND hwnd, HDC hdc, RECT * rect)
{
    SIZE            txtSize;
    int             totalDx, totalDy;
    int             leftDy, rightDy;
    int             leftLargestDx, rightLargestDx, titleLargestDx;
    int             linePosX, linePosY;
    int             currY;
    int             offX, offY;
    int             boxDy;

    HFONT fontSumatraTxt = Win32_Font_GetSimple(hdc, SUMATRA_TXT_FONT, SUMATRA_TXT_FONT_SIZE);
    HFONT fontVersionTxt = Win32_Font_GetSimple(hdc, VERSION_TXT_FONT, VERSION_TXT_FONT_SIZE);
    HFONT fontLeftTxt = Win32_Font_GetSimple(hdc, LEFT_TXT_FONT, LEFT_TXT_FONT_SIZE);
    HFONT fontRightTxt = Win32_Font_GetSimple(hdc, RIGHT_TXT_FONT, RIGHT_TXT_FONT_SIZE);
    HGDIOBJ origFont = SelectObject(hdc, fontSumatraTxt);

    /* calculate minimal top box size */
    const TCHAR *txt = SUMATRA_TXT;
    GetTextExtentPoint32(hdc, txt, lstrlen(txt), &txtSize);
    boxDy = txtSize.cy + ABOUT_BOX_MARGIN_DY * 2;
    titleLargestDx = txtSize.cx;

    /* consider version and version-sub strings */
    SelectObject(hdc, fontVersionTxt);
    txt = VERSION_TXT;
    GetTextExtentPoint32(hdc, txt, lstrlen(txt), &txtSize);
    offX = txtSize.cx;
    txt = VERSION_SUB_TXT;
    GetTextExtentPoint32(hdc, txt, lstrlen(txt), &txtSize);
    txtSize.cx = max(txtSize.cx, offX);
    titleLargestDx += 2 * (txtSize.cx + 6);

    /* calculate left text dimensions */
    SelectObject(hdc, fontLeftTxt);
    leftLargestDx = 0;
    leftDy = 0;
    for (AboutLayoutInfoEl *el = gAboutLayoutInfo; el->leftTxt; el++) {
        GetTextExtentPoint32(hdc, el->leftTxt, lstrlen(el->leftTxt), &txtSize);
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
    rightLargestDx = 0;
    rightDy = 0;
    for (AboutLayoutInfoEl *el = gAboutLayoutInfo; el->leftTxt; el++) {
        GetTextExtentPoint32(hdc, el->rightTxt, lstrlen(el->rightTxt), &txtSize);
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
    totalDx  = leftLargestDx + rightLargestDx;
    totalDx += ABOUT_LEFT_RIGHT_SPACE_DX + ABOUT_LINE_SEP_SIZE + ABOUT_LEFT_RIGHT_SPACE_DX;
    if (totalDx < titleLargestDx)
        totalDx = titleLargestDx;
    totalDx += 2 * ABOUT_LINE_OUTER_SIZE + 2 * ABOUT_MARGIN_DX;

    totalDy  = boxDy;
    totalDy += ABOUT_LINE_OUTER_SIZE;
    totalDy += (dimof(gAboutLayoutInfo)-1) * (rightDy + ABOUT_TXT_DY);
    totalDy += ABOUT_LINE_OUTER_SIZE + 4;

    RECT rc;
    GetClientRect(hwnd, &rc);
    offX = (RectDx(&rc) - totalDx) / 2;
    offY = (RectDy(&rc) - totalDy) / 2;

    if (rect) {
        rect->left = offX;
        rect->top = offY;
        rect->right = offX + totalDx;
        rect->bottom = offY + totalDy;
    }

    /* calculate text positions */
    linePosX = ABOUT_LINE_OUTER_SIZE + ABOUT_MARGIN_DX + leftLargestDx + ABOUT_LEFT_RIGHT_SPACE_DX;
    linePosY = 4;

    currY = offY + boxDy + linePosY;
    for (AboutLayoutInfoEl *el = gAboutLayoutInfo; el->leftTxt; el++) {
        el->leftPos.x = offX + linePosX - ABOUT_LEFT_RIGHT_SPACE_DX - el->leftPos.dx;
        el->leftPos.y = currY + (rightDy - leftDy) / 2;
        el->rightPos.x = offX + linePosX + ABOUT_LEFT_RIGHT_SPACE_DX;
        el->rightPos.y = currY;
        currY += rightDy + ABOUT_TXT_DY;
    }

    SelectObject(hdc, origFont);
    Win32_Font_Delete(fontSumatraTxt);
    Win32_Font_Delete(fontVersionTxt);
    Win32_Font_Delete(fontLeftTxt);
    Win32_Font_Delete(fontRightTxt);
}

void OnPaintAbout(HWND hwnd)
{
    PAINTSTRUCT ps;
    RECT rc;
    HDC hdc = BeginPaint(hwnd, &ps);
    UpdateAboutLayoutInfo(hwnd, hdc, &rc);
    DrawAbout(hwnd, hdc, &rc);
    EndPaint(hwnd, &ps);
}

const TCHAR *AboutGetLink(WindowInfo *win, int x, int y, AboutLayoutInfoEl **el_out)
{
    if (gRestrictedUse) return NULL;

    // Update the link location information
    if (win)
        UpdateAboutLayoutInfo(win->hwndCanvas, win->hdcToDraw, NULL);
    else
        OnPaintAbout(gHwndAbout);

    for (AboutLayoutInfoEl *el = gAboutLayoutInfo; el->leftTxt; el++) {
        if ((x < el->rightPos.x) ||
            (x > el->rightPos.x + el->rightPos.dx))
            continue;
        if ((y < el->rightPos.y) ||
            (y > el->rightPos.y + el->rightPos.dy))
            continue;
        if (el_out)
            *el_out = el;
        return el->url;
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
    RECT rc;
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(gHwndAbout, &ps);
    UpdateAboutLayoutInfo(gHwndAbout, hdc, &rc);
    EndPaint(gHwndAbout, &ps);
    InflateRect(&rc, ABOUT_RECT_PADDING, ABOUT_RECT_PADDING);

    // resize the new window to just match these dimensions
    RECT wRc, cRc;
    GetWindowRect(gHwndAbout, &wRc);
    GetClientRect(gHwndAbout, &cRc);
    wRc.right += RectDx(&rc) - RectDx(&cRc);
    wRc.bottom += RectDy(&rc) - RectDy(&cRc);
    MoveWindow(gHwndAbout, wRc.left, wRc.top, RectDx(&wRc), RectDy(&wRc), FALSE);

    ShowWindow(gHwndAbout, SW_SHOW);
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
                if (AboutGetLink(NULL, pt.x, pt.y)) {
                    SetCursor(gCursorHand);
                    return TRUE;
                }
            }
            return DefWindowProc(hwnd, message, wParam, lParam);

        case WM_LBUTTONDOWN:
            url = AboutGetLink(NULL, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)url);
            break;

        case WM_LBUTTONUP:
            url = AboutGetLink(NULL, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            if (url && url == (const TCHAR *)GetWindowLongPtr(hwnd, GWLP_USERDATA))
                LaunchBrowser(url);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
            break;

        case WM_CHAR:
            if (VK_ESCAPE == wParam)
                DestroyWindow(hwnd);
            break;

        case WM_DESTROY:
            assert(gHwndAbout);
            gHwndAbout = NULL;
            break;

        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}
