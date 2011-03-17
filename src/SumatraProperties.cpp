/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "SumatraPDF.h"
#include "WindowInfo.h"
#include "DisplayModel.h"
#include "SumatraProperties.h"
#include "AppPrefs.h"
#include "translations.h"
#include "WinUtil.h"
#include "FileUtil.h"
#include "AppTools.h"
#include "vstrlist.h"

#define PROPERTIES_LEFT_RIGHT_SPACE_DX 8
#define PROPERTIES_RECT_PADDING     8
#define PROPERTIES_TXT_DY_PADDING 2
#define PROPERTIES_WIN_TITLE    _TR("Document Properties")

extern HINSTANCE ghinst;
extern SerializableGlobalPrefs gGlobalPrefs;

// See: http://www.verypdf.com/pdfinfoeditor/pdf-date-format.htm
// Format:  "D:YYYYMMDDHHMMSSxxxxxxx"
// Example: "D:20091222171933-05'00'"
static bool PdfDateParse(TCHAR *pdfDate, SYSTEMTIME *timeOut) {
    ZeroMemory(timeOut, sizeof(SYSTEMTIME));
    // "D:" at the beginning is optional
    if (Str::StartsWith(pdfDate, _T("D:")))
        pdfDate += 2;
    return 6 == _stscanf(pdfDate, _T("%4d%2d%2d") _T("%2d%2d%2d"),
        &timeOut->wYear, &timeOut->wMonth, &timeOut->wDay,
        &timeOut->wHour, &timeOut->wMinute, &timeOut->wSecond);
    // don't bother about the day of week, we won't display it anyway
}

// Convert a date in PDF format, e.g. "D:20091222171933-05'00'" to a display
// format e.g. "12/22/2009 5:19:33 PM"
// See: http://www.verypdf.com/pdfinfoeditor/pdf-date-format.htm
// The conversion happens in place
static void PdfDateToDisplay(TCHAR **s) {
    SYSTEMTIME date;

    bool ok = false;
    if (*s) {
        ok = PdfDateParse(*s, &date);
        free(*s);
    }
    *s = NULL;
    if (!ok)
        return;

    TCHAR buf[512];
    int cchBufLen = dimof(buf);
    int ret = GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &date, NULL, buf, cchBufLen);
    if (0 == ret) // GetDateFormat() failed
        ret = 1;

    TCHAR *tmp = buf + ret - 1;
    *tmp++ = _T(' ');
    cchBufLen -= ret;
    ret = GetTimeFormat(LOCALE_USER_DEFAULT, 0, &date, NULL, tmp, cchBufLen);
    if (0 == ret) // GetTimeFormat() failed
        *tmp = '\0';

    *s = Str::Dup(buf);
}

// format a number with a given thousand separator e.g. it turns 1234 into "1,234"
// Caller needs to free() the result.
static TCHAR *FormatNumWithThousandSep(size_t num) {
    TCHAR thousandSep[4];
    GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_STHOUSAND, thousandSep, dimof(thousandSep));
    ScopedMem<TCHAR> buf(Str::Format(_T("%Iu"), num));

    Str::Str<TCHAR> res(32);
    int i = Str::Len(buf) % 3;
    for (TCHAR *src = buf.Get(); *src; src++) {
        res.Append(*src);
        if (*(src + 1) && i == 2)
            res.Append(thousandSep);
        i = (i + 1) % 3;
    }

    return res.StealData();
}

// Format a floating point number with at most two decimal after the point
// Caller needs to free the result.
static TCHAR *FormatFloatWithThousandSep(double number, const TCHAR *unit=NULL) {
    size_t num = (size_t)(number * 100);

    ScopedMem<TCHAR> tmp(FormatNumWithThousandSep(num / 100));
    TCHAR decimal[4];
    GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SDECIMAL, decimal, dimof(decimal));

    // always add between one and two decimals after the point
    ScopedMem<TCHAR> buf(Str::Format(_T("%s%s%02d"), tmp, decimal, num % 100));
    if (Str::EndsWith(buf, _T("0")))
        buf[Str::Len(buf) - 1] = '\0';

    return unit ? Str::Format(_T("%s %s"), buf, unit) : Str::Dup(buf);
}

// Format the file size in a short form that rounds to the largest size unit
// e.g. "3.48 GB", "12.38 MB", "23 KB"
// Caller needs to free the result.
static TCHAR *FormatSizeSuccint(size_t size) {
    const TCHAR *unit = NULL;
    double s = (double)size;

    if (size > GB) {
        s /= GB;
        unit = _TR("GB");
    } else if (size > MB) {
        s /= MB;
        unit = _TR("MB");
    } else {
        s /= KB;
        unit = _TR("KB");
    }

    return FormatFloatWithThousandSep(s, unit);
}

// format file size in a readable way e.g. 1348258 is shown
// as "1.29 MB (1,348,258 Bytes)"
// Caller needs to free the result
static TCHAR *FormatPdfSize(size_t size) {
    ScopedMem<TCHAR> n1(FormatSizeSuccint(size));
    ScopedMem<TCHAR> n2(FormatNumWithThousandSep(size));

    return Str::Format(_T("%s (%s %s)"), n1, n2, _TR("Bytes"));
}

// format page size according to locale (e.g. "29.7 x 20.9 cm" or "11.69 x 8.23 in")
// Caller needs to free the result
static TCHAR *FormatPdfPageSize(SizeD size) {
    TCHAR unitSystem[2];
    GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_IMEASURE, unitSystem, dimof(unitSystem));
    bool isMetric = unitSystem[0] == '0';

    double width = size.dx * (isMetric ? 2.54 : 1.0) / PDF_FILE_DPI;
    double height = size.dy * (isMetric ? 2.54 : 1.0) / PDF_FILE_DPI;
    if (((int)(width * 100)) % 100 == 99)
        width += 0.01;
    if (((int)(height * 100)) % 100 == 99)
        height += 0.01;

    ScopedMem<TCHAR> strWidth(FormatFloatWithThousandSep(width));
    ScopedMem<TCHAR> strHeight(FormatFloatWithThousandSep(height, isMetric ? _T("cm") : _T("in")));

    return Str::Format(_T("%s x %s"), strWidth, strHeight);
}

// returns a list of permissions denied by this document
// Caller needs to free the result
static TCHAR *FormatPdfPermissions(PdfEngine *pdfEngine) {
    VStrList denials;

    if (!pdfEngine->hasPermission(PDF_PERM_PRINT))
        denials.Push(Str::Dup(_TR("printing document")));
    if (!pdfEngine->hasPermission(PDF_PERM_COPY))
        denials.Push(Str::Dup(_TR("copying text")));

    return denials.Join(_T(", "));
}

void PdfPropertiesLayout::AddProperty(const TCHAR *key, const TCHAR *value)
{
    // don't display value-less properties
    if (!Str::IsEmpty(value))
        Append(new PdfPropertyEl(key, value));
    else
        delete value;
}

static void UpdatePropertiesLayout(HWND hwnd, HDC hdc, RectI *rect) {
    SIZE            txtSize;
    int             totalDx, totalDy;
    int             leftMaxDx, rightMaxDx;
    WindowInfo *    win = FindWindowInfoByHwnd(hwnd);

    PdfPropertiesLayout *layoutData = (PdfPropertiesLayout *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    HFONT fontLeftTxt = Win32_Font_GetSimple(hdc, LEFT_TXT_FONT, LEFT_TXT_FONT_SIZE);
    HFONT fontRightTxt = Win32_Font_GetSimple(hdc, RIGHT_TXT_FONT, RIGHT_TXT_FONT_SIZE);
    HGDIOBJ origFont = SelectObject(hdc, fontLeftTxt);

    /* calculate text dimensions for the left side */
    SelectObject(hdc, fontLeftTxt);
    leftMaxDx = 0;
    for (size_t i = 0; i < layoutData->Count(); i++) {
        PdfPropertyEl *el = layoutData->At(i);
        GetTextExtentPoint32(hdc, el->leftTxt, Str::Len(el->leftTxt), &txtSize);
        el->leftPos.dx = txtSize.cx;
        el->leftPos.dy = txtSize.cy;

        assert(el->leftPos.dy == layoutData->At(0)->leftPos.dy);
        if (el->leftPos.dx > leftMaxDx)
            leftMaxDx = el->leftPos.dx;
    }

    /* calculate text dimensions for the right side */
    SelectObject(hdc, fontRightTxt);
    rightMaxDx = 0;
    int lineCount = 0;
    for (size_t i = 0; i < layoutData->Count(); i++) {
        PdfPropertyEl *el = layoutData->At(i);
        GetTextExtentPoint32(hdc, el->rightTxt, Str::Len(el->rightTxt), &txtSize);
        el->rightPos.dx = txtSize.cx;
        el->rightPos.dy = txtSize.cy;

        assert(el->rightPos.dy == layoutData->At(0)->rightPos.dy);
        if (el->rightPos.dx > rightMaxDx)
            rightMaxDx = el->rightPos.dx;
        lineCount++;
    }

    assert(lineCount > 0);
    int textDy = lineCount > 0 ? layoutData->At(0)->rightPos.dy : 0;
    totalDx = leftMaxDx + PROPERTIES_LEFT_RIGHT_SPACE_DX + rightMaxDx;

    totalDy = 4;
    totalDy += lineCount * (textDy + PROPERTIES_TXT_DY_PADDING);
    totalDy += 4;

    int offset = PROPERTIES_RECT_PADDING;
    if (rect)
        *rect = RectI(0, 0, totalDx + 2 * offset, totalDy + offset);

    int currY = 0;
    for (size_t i = 0; i < layoutData->Count(); i++) {
        PdfPropertyEl *el = layoutData->At(i);
        el->leftPos = RectI(offset, offset + currY, leftMaxDx, el->leftPos.dy);
        el->rightPos.x = offset + leftMaxDx + PROPERTIES_LEFT_RIGHT_SPACE_DX;
        el->rightPos.y = offset + currY;
        currY += (textDy + PROPERTIES_TXT_DY_PADDING);
    }

    SelectObject(hdc, origFont);
    Win32_Font_Delete(fontLeftTxt);
    Win32_Font_Delete(fontRightTxt);
}

static void CreatePropertiesWindow(WindowInfo *win, PdfPropertiesLayout *layoutData) {
    win->hwndPdfProperties = CreateWindow(
           PROPERTIES_CLASS_NAME, PROPERTIES_WIN_TITLE,
           WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
           CW_USEDEFAULT, CW_USEDEFAULT,
           CW_USEDEFAULT, CW_USEDEFAULT,
           NULL, NULL,
           ghinst, NULL);
    if (!win->hwndPdfProperties)
        return;

    assert(!GetWindowLongPtr(win->hwndPdfProperties, GWLP_USERDATA));
    SetWindowLongPtr(win->hwndPdfProperties, GWLP_USERDATA, (LONG_PTR)layoutData);

    // get the dimensions required for the about box's content
    RectI rc;
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(win->hwndPdfProperties, &ps);
    UpdatePropertiesLayout(win->hwndPdfProperties, hdc, &rc);
    EndPaint(win->hwndPdfProperties, &ps);

    // resize the new window to just match these dimensions
    WindowRect wRc(win->hwndPdfProperties);
    ClientRect cRc(win->hwndPdfProperties);
    wRc.dx += rc.dx - cRc.dx;
    wRc.dy += rc.dy - cRc.dy;
    MoveWindow(win->hwndPdfProperties, wRc.x, wRc.y, wRc.dx, wRc.dy, FALSE);

    ShowWindow(win->hwndPdfProperties, SW_SHOW);
}

/*
Example xref->info ("Info") object:
<<
 /Title (javascript performance rocks checklist.graffle)
 /Author (Amy Hoy)
 /Subject <>
 /Creator (OmniGraffle Professional)
 /Producer (Mac OS X 10.5.8 Quartz PDFContext)

 /CreationDate (D:20091017155028Z)
 /ModDate (D:20091019165730+02'00')

 /AAPL:Keywords [ <> ]
 /Keywords <>
>>
*/

// TODO: add missing properties
// TODO: add information about fonts ?
void OnMenuProperties(WindowInfo *win)
{
    if (win->hwndPdfProperties) {
        SetActiveWindow(win->hwndPdfProperties);
        return;
    }

    if (!win->dm || !win->dm->pdfEngine)
        return;
    PdfEngine *pdfEngine = win->dm->pdfEngine;

    PdfPropertiesLayout *layoutData = new PdfPropertiesLayout();
    if (!layoutData)
        return;

    TCHAR *str = Str::Dup(pdfEngine->fileName());
    layoutData->AddProperty(_TR("File:"), str);

    str = pdfEngine->getPdfInfo("Title");
    layoutData->AddProperty(_TR("Title:"), str);

    str = pdfEngine->getPdfInfo("Subject");
    layoutData->AddProperty(_TR("Subject:"), str);

    str = pdfEngine->getPdfInfo("Author");
    layoutData->AddProperty(_TR("Author:"), str);

    str = pdfEngine->getPdfInfo("CreationDate");
    PdfDateToDisplay(&str);
    layoutData->AddProperty(_TR("Created:"), str);

    str = pdfEngine->getPdfInfo("ModDate");
    PdfDateToDisplay(&str);
    layoutData->AddProperty(_TR("Modified:"), str);

    str = pdfEngine->getPdfInfo("Creator");
    layoutData->AddProperty(_TR("Application:"), str);

    str = pdfEngine->getPdfInfo("Producer");
    layoutData->AddProperty(_TR("PDF Producer:"), str);

    int version = pdfEngine->getPdfVersion();
    if (version >= 10000) {
        if (version % 100 > 0)
            str = Str::Format(_T("%d.%d Adobe Extension Level %d"), version / 10000, (version / 100) % 100, version % 100);
        else
            str = Str::Format(_T("%d.%d"), version / 10000, (version / 100) % 100);
        layoutData->AddProperty(_TR("PDF Version:"), str);
    }

    size_t fileSize = File::GetSize(pdfEngine->fileName());
    if (fileSize == INVALID_FILE_SIZE) {
        fz_buffer *data = pdfEngine->getStreamData();
        if (data) {
            fileSize = data->len;
            fz_dropbuffer(data);
        }
    }
    if (fileSize != INVALID_FILE_SIZE) {
        str = FormatPdfSize(fileSize);
        layoutData->AddProperty(_TR("File Size:"), str);
    }

    str = Str::Format(_T("%d"), pdfEngine->pageCount());
    layoutData->AddProperty(_TR("Number of Pages:"), str);

    str = FormatPdfPageSize(pdfEngine->pageSize(win->dm->currentPageNo()));
    layoutData->AddProperty(_TR("Page Size:"), str);

    str = FormatPdfPermissions(pdfEngine);
    layoutData->AddProperty(_TR("Denied Permissions:"), str);

    // TODO: this is about linearlized PDF. Looks like mupdf would
    // have to be extended to detect linearlized PDF. The rules are described
    // in F3.3 of http://www.adobe.com/devnet/acrobat/pdfs/PDF32000_2008.pdf
    // layoutData->AddProperty(_T("Fast Web View:"), Str::Dup(_T("No")));

    // TODO: probably needs to extend mupdf to get this information.
    // Tagged PDF rules are described in 14.8.2 of
    // http://www.adobe.com/devnet/acrobat/pdfs/PDF32000_2008.pdf
    // layoutData->AddProperty(_T("Tagged PDF:"), Str::Dup(_T("No")));

    CreatePropertiesWindow(win, layoutData);
}

static void DrawProperties(HWND hwnd, HDC hdc)
{
    WindowInfo * win = FindWindowInfoByHwnd(hwnd);
    PdfPropertiesLayout *layoutData = (PdfPropertiesLayout *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    HBRUSH brushBg = CreateSolidBrush(gGlobalPrefs.m_bgColor);
#if 0
    HPEN penBorder = CreatePen(PS_SOLID, ABOUT_LINE_OUTER_SIZE, COL_BLACK);
#endif

    HFONT fontLeftTxt = Win32_Font_GetSimple(hdc, LEFT_TXT_FONT, LEFT_TXT_FONT_SIZE);
    HFONT fontRightTxt = Win32_Font_GetSimple(hdc, RIGHT_TXT_FONT, RIGHT_TXT_FONT_SIZE);

    HGDIOBJ origFont = SelectObject(hdc, fontLeftTxt); /* Just to remember the orig font */

    SetBkMode(hdc, TRANSPARENT);

    ClientRect rcClient(hwnd);
    FillRect(hdc, &rcClient.ToRECT(), brushBg);

#if 0
    SelectObject(hdc, brushBg);
    SelectObject(hdc, penBorder);
#endif

    SetTextColor(hdc, WIN_COL_BLACK);

    /* render text on the left*/
    SelectObject(hdc, fontLeftTxt);
    for (size_t i = 0; i < layoutData->Count(); i++) {
        PdfPropertyEl *el = layoutData->At(i);
        DrawText(hdc, el->leftTxt, -1, &el->leftPos.ToRECT(), DT_RIGHT);
    }

    /* render text on the right */
    SelectObject(hdc, fontRightTxt);
    for (size_t i = 0; i < layoutData->Count(); i++) {
        PdfPropertyEl *el = layoutData->At(i);
        RectI rc = el->rightPos;
        if (rc.x + rc.dx > rcClient.x + rcClient.dx - PROPERTIES_RECT_PADDING)
            rc.dx = rcClient.x + rcClient.dx - PROPERTIES_RECT_PADDING - rc.x;
        DrawText(hdc, el->rightTxt, -1, &rc.ToRECT(), DT_LEFT | DT_PATH_ELLIPSIS);
    }

    SelectObject(hdc, origFont);
    Win32_Font_Delete(fontLeftTxt);
    Win32_Font_Delete(fontRightTxt);

    DeleteObject(brushBg);
#if 0
    DeleteObject(penBorder);
#endif
}

static void OnPaintProperties(HWND hwnd)
{
    PAINTSTRUCT ps;
    RectI rc;
    HDC hdc = BeginPaint(hwnd, &ps);
    UpdatePropertiesLayout(hwnd, hdc, &rc);
    DrawProperties(hwnd, hdc);
    EndPaint(hwnd, &ps);
}

void CopyPropertiesToClipboard(HWND hwnd)
{
    if (!OpenClipboard(NULL))
        return;

    // just concatenate all the properties into a multi-line string
    PdfPropertiesLayout *layoutData = (PdfPropertiesLayout *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    VStrList lines;
    for (size_t i = 0; i < layoutData->Count(); i++) {
        PdfPropertyEl *el = layoutData->At(i);
        lines.Append(Str::Format(_T("%s %s\r\n"), el->leftTxt, el->rightTxt));
    }
    ScopedMem<TCHAR> result(lines.Join());

    HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, (Str::Len(result) + 1) * sizeof(TCHAR));
    if (handle) {
        TCHAR *selText = (TCHAR *)GlobalLock(handle);
        lstrcpy(selText, result);
        GlobalUnlock(handle);

        EmptyClipboard();
        SetClipboardData(CF_T_TEXT, handle);
    }
    CloseClipboard();
}

LRESULT CALLBACK WndProcProperties(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WindowInfo * win = FindWindowInfoByHwnd(hwnd);

    switch (message)
    {
        case WM_CREATE:
            break;

        case WM_ERASEBKGND:
            // do nothing, helps to avoid flicker
            return TRUE;

        case WM_PAINT:
            OnPaintProperties(hwnd);
            break;

        case WM_CHAR:
            if (VK_ESCAPE == wParam)
                DestroyWindow(hwnd);
            break;

        case WM_DESTROY:
            delete (PdfPropertiesLayout *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
            assert(win->hwndPdfProperties);
            win->hwndPdfProperties = NULL;
            break;

        /* TODO: handle mouse move/down/up so that links work */
        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}
