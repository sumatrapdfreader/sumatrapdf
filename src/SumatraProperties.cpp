/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "SumatraPDF.h"
#include "WindowInfo.h"
#include "DisplayModel.h"
#include "SumatraProperties.h"
#include "AppPrefs.h"
#include "translations.h"
#include "win_util.h"
#include "WinUtil.hpp"
#include "AppTools.h"

#define PROPERTIES_LEFT_RIGHT_SPACE_DX 8
#define PROPERTIES_RECT_PADDING     8
#define PROPERTIES_TXT_DY_PADDING 2
#define PROPERTIES_WIN_TITLE    _TR("Document Properties")

extern HINSTANCE ghinst;

static uint64_t WinFileSizeGet(const TCHAR *file_path)
{
    int                         ok;
    WIN32_FILE_ATTRIBUTE_DATA   fileInfo;
    uint64_t                    res;

    if (NULL == file_path)
        return INVALID_FILE_SIZE;

    ok = GetFileAttributesEx(file_path, GetFileExInfoStandard, (void*)&fileInfo);
    if (!ok)
        return (uint64_t)INVALID_FILE_SIZE;

    res = fileInfo.nFileSizeHigh;
    res = (res << 32) + fileInfo.nFileSizeLow;

    return res;
}

// Note: returns NULL instead of an empty string
static TCHAR *PdfToString(fz_obj *obj) {
    if (fz_tostrlen(obj) == 0)
        return NULL;
    return pdf_to_tstr(obj);
}

// See: http://www.verypdf.com/pdfinfoeditor/pdf-date-format.htm
// Format:  "D:YYYYMMDDHHMMSSxxxxxxx"
// Example: "D:20091222171933-05'00'"
static bool PdfDateParse(TCHAR *pdfDate, SYSTEMTIME *timeOut) {
    ZeroMemory(timeOut, sizeof(SYSTEMTIME));
    // "D:" at the beginning is optional
    if (tstr_startswith(pdfDate, _T("D:")))
        pdfDate += 2;
    return 6 == _stscanf(pdfDate, _T("%4d%2d%2d") _T("%2d%2d%2d"),
        &timeOut->wYear, &timeOut->wMonth, &timeOut->wDay,
        &timeOut->wHour, &timeOut->wMinute, &timeOut->wSecond);
    // don't bother about the day of week, we won't display it anyway
}

// Convert a date in PDF format, e.g. "D:20091222171933-05'00'" to a display
// format e.g. "12/22/2009 5:19:33 PM"
// See: http://www.verypdf.com/pdfinfoeditor/pdf-date-format.htm
// Caller needs to free this string
static TCHAR *PdfDateToDisplay(fz_obj *dateObj) {
    SYSTEMTIME date;

    bool ok = false;
    TCHAR *s = PdfToString(dateObj);
    if (s) {
        ok = PdfDateParse(s, &date);
        free(s);
    }
    if (!ok) {
        return NULL;
    }

    TCHAR buf[512];
    int cchBufLen = dimof(buf);
    int ret = GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &date, NULL, buf, cchBufLen);
    if (0 == ret) {
        // GetDateFormat() failed
        return NULL;
    }

    TCHAR *tmp = buf + ret - 1;
    *tmp++ = _T(' ');
    cchBufLen -= ret;
    ret = GetTimeFormat(LOCALE_USER_DEFAULT, 0, &date, NULL, tmp, cchBufLen);
    if (0 == ret) {
        // GetTimeFormat() failed
        return NULL;
    }
    return tstr_dup(buf);
}

// format a number with a given thousand separator e.g. it turns 1234 into "1,234"
// Caller needs to free() the result.
static TCHAR *FormatNumWithThousandSep(uint64_t num) {
    TCHAR buf[32], buf2[64], thousandSep[4];

    tstr_printf_s(buf, dimof(buf), _T("%I64d"), num);
    GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_STHOUSAND, thousandSep, dimof(thousandSep));

    TCHAR *src = buf, *dst = buf2;
    int len = lstrlen(src);
    while (*src) {
        *dst++ = *src++;
        if (*src && (len - (src - buf)) % 3 == 0) {
            lstrcpy(dst, thousandSep);
            dst += lstrlen(thousandSep);
        }
    }
    *dst = '\0';

    return tstr_dup(buf2);
}

// Format a floating point number with at most two decimal after the point
// Caller needs to free the result.
static TCHAR *FormatFloatWithThousandSep(double number, const TCHAR *unit=NULL) {
    TCHAR buf[64];
    uint64_t num = (uint64_t)(number * 100);

    TCHAR *tmp = FormatNumWithThousandSep(num / 100);
    TCHAR decimal[4];
    GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SDECIMAL, decimal, dimof(decimal));

    // always add between one and two decimals after the point
    wsprintf(buf, _T("%s%s%02d"), tmp, decimal, num % 100);
    if (buf[lstrlen(buf) - 1] == '0')
        buf[lstrlen(buf) - 1] = '\0';
    free(tmp);

    return unit ? tstr_printf(_T("%s %s"), buf, unit) : tstr_dup(buf);
}

// Format the file size in a short form that rounds to the largest size unit
// e.g. "3.48 GB", "12.38 MB", "23 KB"
// Caller needs to free the result.
static TCHAR *FormatSizeSuccint(uint64_t size) {
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
static TCHAR *FormatPdfSize(uint64_t size) {
    TCHAR *n1, *n2, *result;

    n1 = FormatSizeSuccint(size);
    n2 = FormatNumWithThousandSep(size);
    result = tstr_printf(_T("%s (%s %s)"), n1, n2, _TR("Bytes"));

    free(n1);
    free(n2);

    return result;
}

// format page size according to locale (e.g. "29.7 x 20.9 cm" or "11.69 x 8.23 in")
// Caller needs to free the result
static TCHAR *FormatPdfPageSize(SizeD size) {
    TCHAR unitSystem[2];
    GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_IMEASURE, unitSystem, dimof(unitSystem));
    bool isMetric = unitSystem[0] == '0';

    double width = size.dx() * (isMetric ? 2.54 : 1.0) / PDF_FILE_DPI;
    double height = size.dy() * (isMetric ? 2.54 : 1.0) / PDF_FILE_DPI;
    if (((int)(width * 100)) % 100 == 99)
        width += 0.01;
    if (((int)(height * 100)) % 100 == 99)
        height += 0.01;

    TCHAR *strWidth = FormatFloatWithThousandSep(width);
    TCHAR *strHeight = FormatFloatWithThousandSep(height, isMetric ? _T("cm") : _T("in"));
    TCHAR *result = tstr_printf(_T("%s x %s"), strWidth, strHeight);
    free(strWidth);
    free(strHeight);

    return result;
}

// returns a list of permissions denied by this document (NULL if everything's permitted)
// Caller needs to free the result
static TCHAR *FormatPdfPermissions(PdfEngine *pdfEngine) {
    VStrList denials;

    if (!pdfEngine->hasPermission(PDF_PERM_PRINT))
        denials.push_back(tstr_dup(_TR("printing document")));
    if (!pdfEngine->hasPermission(PDF_PERM_COPY))
        denials.push_back(tstr_dup(_TR("copying text")));

    TCHAR *denialList = denials.join(_T(", "));
    if (tstr_empty(denialList)) {
        free(denialList);
        denialList = NULL;
    }

    return denialList;
}

static void AddPdfProperty(PdfPropertiesLayout *layoutData, const TCHAR *left, const TCHAR *right) {
    // don't display value-less properties
    if (!right)
        return;

    PdfPropertyEl *el = SA(PdfPropertyEl);
    el->leftTxt = left;
    el->rightTxt = tstr_dup(right);
    el->next = NULL;

    if (!layoutData->last) {
        layoutData->first = layoutData->last = el;
    }
    else {
        layoutData->last->next = el;
        layoutData->last = layoutData->last->next;
    }
}

static void FreePdfProperties(HWND hwnd)
{
    // free the text on the right. Text on left is static, so doesn't need to be freed
    PdfPropertiesLayout *layoutData = (PdfPropertiesLayout *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    assert(layoutData);
    for (PdfPropertyEl *el = layoutData->first; el; ) {
        PdfPropertyEl *tofree = el;
        el = el->next;
        free((void *)tofree->rightTxt);
        free(tofree);
    }
    free(layoutData);
}

static void UpdatePropertiesLayout(HWND hwnd, HDC hdc, RECT *rect) {
    SIZE            txtSize;
    int             totalDx, totalDy;
    int             leftMaxDx, rightMaxDx;
    WindowInfo *    win = WindowInfoList::Find(hwnd);

    PdfPropertiesLayout *layoutData = (PdfPropertiesLayout *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    HFONT fontLeftTxt = Win32_Font_GetSimple(hdc, LEFT_TXT_FONT, LEFT_TXT_FONT_SIZE);
    HFONT fontRightTxt = Win32_Font_GetSimple(hdc, RIGHT_TXT_FONT, RIGHT_TXT_FONT_SIZE);
    HFONT origFont = (HFONT)SelectObject(hdc, fontLeftTxt);

    /* calculate text dimensions for the left side */
    (HFONT)SelectObject(hdc, fontLeftTxt);
    leftMaxDx = 0;
    for (PdfPropertyEl *el = layoutData->first; el; el = el->next) {
        GetTextExtentPoint32(hdc, el->leftTxt, lstrlen(el->leftTxt), &txtSize);
        el->leftPos.dx = txtSize.cx;
        el->leftPos.dy = txtSize.cy;

        assert(el->leftPos.dy == layoutData->first->leftPos.dy);
        if (el->leftPos.dx > leftMaxDx)
            leftMaxDx = el->leftPos.dx;
    }

    /* calculate text dimensions for the right side */
    (HFONT)SelectObject(hdc, fontRightTxt);
    rightMaxDx = 0;
    int lineCount = 0;
    for (PdfPropertyEl *el = layoutData->first; el; el = el->next) {
        GetTextExtentPoint32(hdc, el->rightTxt, lstrlen(el->rightTxt), &txtSize);
        el->rightPos.dx = txtSize.cx;
        el->rightPos.dy = txtSize.cy;

        assert(el->rightPos.dy == layoutData->first->rightPos.dy);
        if (el->rightPos.dx > rightMaxDx)
            rightMaxDx = el->rightPos.dx;
        lineCount++;
    }

    assert(lineCount > 0);
    int textDy = lineCount > 0 ? layoutData->first->rightPos.dy : 0;
    totalDx = leftMaxDx + PROPERTIES_LEFT_RIGHT_SPACE_DX + rightMaxDx;

    totalDy = 4;
    totalDy += lineCount * (textDy + PROPERTIES_TXT_DY_PADDING);
    totalDy += 4;

    RECT rc;
    GetClientRect(hwnd, &rc);

    int offset = PROPERTIES_RECT_PADDING;
    if (rect) {
        rect->left = 0;
        rect->top = 0;
        rect->right = totalDx + 2 * offset;
        rect->bottom = totalDy + offset;
    }

    int currY = 0;
    for (PdfPropertyEl *el = layoutData->first; el; el = el->next) {
        el->leftPos.x = offset;
        el->leftPos.dx = leftMaxDx;
        el->leftPos.y = offset + currY;
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
    RECT rc;
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(win->hwndPdfProperties, &ps);
    UpdatePropertiesLayout(win->hwndPdfProperties, hdc, &rc);
    EndPaint(win->hwndPdfProperties, &ps);

    // resize the new window to just match these dimensions
    RECT wRc, cRc;
    GetWindowRect(win->hwndPdfProperties, &wRc);
    GetClientRect(win->hwndPdfProperties, &cRc);
    wRc.right += RectDx(&rc) - RectDx(&cRc);
    wRc.bottom += RectDy(&rc) - RectDy(&cRc);
    MoveWindow(win->hwndPdfProperties, wRc.left, wRc.top, RectDx(&wRc), RectDy(&wRc), FALSE);

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

    DisplayModel *dm = win->dm;
    if (!dm || !dm->pdfEngine) {
        return;
    }
    PdfPropertiesLayout *layoutData = SA(PdfPropertiesLayout);
    if (!layoutData)
        return;
    layoutData->first = layoutData->last = NULL;

    fz_obj *info = dm->pdfEngine->getPdfInfo();

    TCHAR *str = (TCHAR *)win->dm->fileName();
    AddPdfProperty(layoutData, _TR("File:"), str);

    str = PdfToString(fz_dictgets(info, "Title"));
    AddPdfProperty(layoutData, _TR("Title:"), str);
    free(str);

    str = PdfToString(fz_dictgets(info, "Subject"));
    AddPdfProperty(layoutData, _TR("Subject:"), str);
    free(str);

    str = PdfToString(fz_dictgets(info, "Author"));
    AddPdfProperty(layoutData, _TR("Author:"), str);
    free(str);

    str = PdfDateToDisplay(fz_dictgets(info, "CreationDate"));
    AddPdfProperty(layoutData, _TR("Created:"), str);
    free(str);

    str = PdfDateToDisplay(fz_dictgets(info, "ModDate"));
    AddPdfProperty(layoutData, _TR("Modified:"), str);
    free(str);

    str = PdfToString(fz_dictgets(info, "Creator"));
    AddPdfProperty(layoutData, _TR("Application:"), str);
    free(str);

    str = PdfToString(fz_dictgets(info, "Producer"));
    AddPdfProperty(layoutData, _TR("PDF Producer:"), str);
    free(str);

    int version = win->dm->pdfEngine->getPdfVersion();
    if (version >= 10000) {
        if (version % 100 > 0)
            str = tstr_printf(_T("%d.%d Adobe Extension Level %d"), version / 10000, (version / 100) % 100, version % 100);
        else
            str = tstr_printf(_T("%d.%d"), version / 10000, (version / 100) % 100);
        AddPdfProperty(layoutData, _TR("PDF Version:"), str);
        free(str);
    }

    uint64_t fileSize = WinFileSizeGet(win->dm->fileName());
    if (fileSize == INVALID_FILE_SIZE) {
        fz_buffer *data = win->dm->pdfEngine->getStreamData();
        if (data) {
            fileSize = data->len;
            fz_dropbuffer(data);
        }
    }
    if (fileSize != INVALID_FILE_SIZE) {
        str = FormatPdfSize(fileSize);
        AddPdfProperty(layoutData, _TR("File Size:"), str);
        free(str);
    }

    str = tstr_printf(_T("%d"), dm->pageCount());
    AddPdfProperty(layoutData, _TR("Number of Pages:"), str);
    free(str);

    str = FormatPdfPageSize(dm->getPageInfo(dm->currentPageNo())->page);
    AddPdfProperty(layoutData, _TR("Page Size:"), str);
    free(str);

    str = FormatPdfPermissions(dm->pdfEngine);
    AddPdfProperty(layoutData, _TR("Denied Permissions:"), str);
    free(str);

    // TODO: this is about linearlized PDF. Looks like mupdf would
    // have to be extended to detect linearlized PDF. The rules are described
    // in F3.3 of http://www.adobe.com/devnet/acrobat/pdfs/PDF32000_2008.pdf
    //AddPdfProperty(layoutData, _T("Fast Web View:"), _T("No"));

    // TODO: probably needs to extend mupdf to get this information.
    // Tagged PDF rules are described in 14.8.2 of
    // http://www.adobe.com/devnet/acrobat/pdfs/PDF32000_2008.pdf
    //AddPdfProperty(layoutData, _T("Tagged PDF:"), _T("No"));

    CreatePropertiesWindow(win, layoutData);
}

static void DrawProperties(HWND hwnd, HDC hdc, RECT *rect)
{
    WindowInfo * win = WindowInfoList::Find(hwnd);
    PdfPropertiesLayout *layoutData = (PdfPropertiesLayout *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    HBRUSH brushBg = CreateSolidBrush(gGlobalPrefs.m_bgColor);
#if 0
    HPEN penBorder = CreatePen(PS_SOLID, ABOUT_LINE_OUTER_SIZE, COL_BLACK);
#endif

    HFONT fontLeftTxt = Win32_Font_GetSimple(hdc, LEFT_TXT_FONT, LEFT_TXT_FONT_SIZE);
    HFONT fontRightTxt = Win32_Font_GetSimple(hdc, RIGHT_TXT_FONT, RIGHT_TXT_FONT_SIZE);

    HFONT origFont = (HFONT)SelectObject(hdc, fontLeftTxt); /* Just to remember the orig font */

    SetBkMode(hdc, TRANSPARENT);

    RECT rcClient;
    GetClientRect(hwnd, &rcClient);
    FillRect(hdc, &rcClient, brushBg);

#if 0
    SelectObject(hdc, brushBg);
    SelectObject(hdc, penBorder);
#endif

    SetTextColor(hdc, WIN_COL_BLACK);

    /* render text on the left*/
    (HFONT)SelectObject(hdc, fontLeftTxt);
    for (PdfPropertyEl *el = layoutData->first; el; el = el->next) {
        RECT rc = RECT_FromRectI(&el->leftPos);
        DrawText(hdc, el->leftTxt, -1, &rc, DT_RIGHT);
    }

    /* render text on the right */
    (HFONT)SelectObject(hdc, fontRightTxt);
    for (PdfPropertyEl *el = layoutData->first; el; el = el->next) {
        RECT rc = RECT_FromRectI(&el->rightPos);
        if (rc.right > rcClient.right - PROPERTIES_RECT_PADDING)
            rc.right = rcClient.right - PROPERTIES_RECT_PADDING;
        DrawText(hdc, el->rightTxt, -1, &rc, DT_LEFT | DT_PATH_ELLIPSIS);
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
    RECT rc;
    HDC hdc = BeginPaint(hwnd, &ps);
    UpdatePropertiesLayout(hwnd, hdc, &rc);
    DrawProperties(hwnd, hdc, &rc);
    EndPaint(hwnd, &ps);
}

void CopyPropertiesToClipboard(HWND hwnd)
{
    TCHAR *result = tstr_dup(_T(""));

    // just concatenate all the properties into a multi-line string
    PdfPropertiesLayout *layoutData = (PdfPropertiesLayout *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    for (PdfPropertyEl *el = layoutData->first; el; el = el->next) {
        TCHAR *newResult = tstr_printf(_T("%s%s %s\r\n"), result, el->leftTxt, el->rightTxt);
        free(result);
        if (!newResult)
            return;
        result = newResult;
    }

    if (OpenClipboard(NULL)) {
        HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, (tstr_len(result) + 1) * sizeof(TCHAR));
        if (handle) {
            TCHAR *selText = (TCHAR *)GlobalLock(handle);
            lstrcpy(selText, result);
            GlobalUnlock(handle);

            EmptyClipboard();
            SetClipboardData(CF_T_TEXT, handle);
        }
        CloseClipboard();
    }

    free(result);
}

LRESULT CALLBACK WndProcProperties(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WindowInfo * win = WindowInfoList::Find(hwnd);

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
            FreePdfProperties(hwnd);
            assert(win->hwndPdfProperties);
            win->hwndPdfProperties = NULL;
            break;

        /* TODO: handle mouse move/down/up so that links work */
        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}
