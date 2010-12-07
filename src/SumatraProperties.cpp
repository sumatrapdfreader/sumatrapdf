/* Copyright 2006-2010 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "SumatraPDF.h"
#include "WindowInfo.h"
#include "SumatraProperties.h"
#include "AppPrefs.h"
#include "translations.h"

#define PROPERTIES_LEFT_RIGHT_SPACE_DX 8
#define PROPERTIES_RECT_PADDING     8
#define PROPERTIES_TXT_DY_PADDING 2
#define PROPERTIES_WIN_TITLE    _TR("Document Properties")

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
    WCHAR *s = (WCHAR *)pdf_toucs2(obj);
    TCHAR *str = NULL;
    if (s && *s)
        str = wstr_to_tstr(s);
    fz_free(s);
    return str;
}

static TCHAR *PdfDateParseInt(TCHAR *s, int numDigits, WORD *valOut) {
    if (!s)
        return NULL;

    int n = 0;
    while (numDigits > 0) {
        TCHAR c = *s++;
        if (c < '0' || c > '9') {
            return NULL;
        }
        n = n * 10 + (c - '0');
        numDigits--;
    }
    *valOut = n;
    return s;
}

// See: http://www.verypdf.com/pdfinfoeditor/pdf-date-format.htm
// Format:  "D:YYYYMMDDHHMMSSxxxxxxx"
// Example: "D:20091222171933-05'00'"
static bool PdfDateParse(TCHAR *pdfDate, SYSTEMTIME *timeOut) {
    ZeroMemory(timeOut, sizeof(SYSTEMTIME));
    // "D:" at the beginning is optional
    if ('D' == pdfDate[0] && ':' == pdfDate[1]) {
        pdfDate += 2;
    }
    pdfDate = PdfDateParseInt(pdfDate, 4, &timeOut->wYear);
    pdfDate = PdfDateParseInt(pdfDate, 2, &timeOut->wMonth);
    pdfDate = PdfDateParseInt(pdfDate, 2, &timeOut->wDay);
    pdfDate = PdfDateParseInt(pdfDate, 2, &timeOut->wHour);
    pdfDate = PdfDateParseInt(pdfDate, 2, &timeOut->wMinute);
    pdfDate = PdfDateParseInt(pdfDate, 2, &timeOut->wSecond);
    // don't bother about the day of week, we won't display it anyway
    return pdfDate != NULL;
}

// Convert a date in PDF format, e.g. "D:20091222171933-05'00'" to a display
// format e.g. "12/22/2009 5:19:33 PM"
// See: http://www.verypdf.com/pdfinfoeditor/pdf-date-format.htm
// Caller needs to free this string
static TCHAR *PdfDateToDisplay(fz_obj *dateObj) {
    SYSTEMTIME date;
    bool ok;
    int ret;
    TCHAR *tmp;
    TCHAR buf[512];
    TCHAR *s;
    int cchBufLen = dimof(buf);

    if (!dateObj) {
        return NULL;
    }
    s = PdfToString(dateObj);
    if (!s) {
        return NULL;
    }
    ok = PdfDateParse(s, &date);
    if (!ok) {
        goto Error;
    }

    ret = GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &date, NULL, buf, cchBufLen);
    if (0 == ret) {
        // GetDateFormat() failed
        goto Error;
    }
    tmp = buf + ret - 1;
    *tmp++ = _T(' ');
    cchBufLen -= ret;
    ret = GetTimeFormat(LOCALE_USER_DEFAULT, 0, &date, NULL, tmp, cchBufLen);
    if (0 == ret) {
        // GetTimeFormat() failed
        goto Error;
    }
    free(s);
    return tstr_dup(buf);
Error:
    return s;
}

// format a number with a given thousand separator e.g. it turns 1234 into "1,234"
// Caller needs to free() the result.
static TCHAR *FormatNumWithThousandSep(uint64_t num) {
    TCHAR buf[32], buf2[64], thousandSep[4];

    _sntprintf(buf, dimof(buf), _T("%I64d"), num);
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
    uint64_t num = number * 100;

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
    double s = size;

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
    if (denialList && !*denialList) {
        free(denialList);
        denialList = NULL;
    }

    return denialList;
}

static void AddPdfProperty(PdfPropertiesLayout *layoutData, const TCHAR *left, const TCHAR *right) {
    PdfPropertyEl *el = (PdfPropertyEl *)malloc(sizeof(PdfPropertyEl));
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
    int             currY;
    int             offX, offY;
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
    offX = (rect_dx(&rc) - totalDx) / 2;
    offY = (rect_dy(&rc) - totalDy) / 2;

    if (rect) {
        rect->left = offX;
        rect->top = offY;
        rect->right = offX + totalDx;
        rect->bottom = offY + totalDy;
    }

    currY = offY;
    for (PdfPropertyEl *el = layoutData->first; el; el = el->next) {
        el->leftPos.x = offX + leftMaxDx - el->leftPos.dx;
        el->leftPos.y = offY + currY;
        el->rightPos.x = offX + leftMaxDx + PROPERTIES_LEFT_RIGHT_SPACE_DX;
        el->rightPos.y = offY + currY;
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
    InflateRect(&rc, PROPERTIES_RECT_PADDING, PROPERTIES_RECT_PADDING);

    // resize the new window to just match these dimensions
    RECT wRc, cRc;
    GetWindowRect(win->hwndPdfProperties, &wRc);
    GetClientRect(win->hwndPdfProperties, &cRc);
    wRc.right += rect_dx(&rc) - rect_dx(&cRc);
    wRc.bottom += rect_dy(&rc) - rect_dy(&cRc);
    MoveWindow(win->hwndPdfProperties, wRc.left, wRc.top, rect_dx(&wRc), rect_dy(&wRc), FALSE);

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
    uint64_t    fileSize;
    TCHAR *     tmp;
    fz_obj *    info = NULL;
    fz_obj *    subject = NULL;
    fz_obj *    author = NULL;
    fz_obj *    title = NULL;
    fz_obj *    producer = NULL;
    fz_obj *    creator = NULL;
    fz_obj *    creationDate = NULL;
    fz_obj *    modDate = NULL;

    TCHAR *     subjectStr = NULL;
    TCHAR *     authorStr = NULL;
    TCHAR *     titleStr = NULL;
    TCHAR *     producerStr = NULL;
    TCHAR *     creatorStr  = NULL;
    TCHAR *     creationDateStr = NULL;
    TCHAR *     modDateStr = NULL;

    if (win->hwndPdfProperties) {
        SetActiveWindow(win->hwndPdfProperties);
        return;
    }

    DisplayModel *dm = win->dm;
    if (!dm || !dm->pdfEngine) {
        return;
    }

    PdfPropertiesLayout *layoutData = (PdfPropertiesLayout *)malloc(sizeof(PdfPropertiesLayout));
    assert(layoutData);
    if (!layoutData)
        return;
    layoutData->first = layoutData->last = NULL;

    info = dm->pdfEngine->getPdfInfo();
    if (fz_isdict(info)) {
        title = fz_dictgets(info, "Title");
        if (title) {
            titleStr = PdfToString(title);
        }
        author = fz_dictgets(info, "Author");
        if (author) {
            authorStr = PdfToString(author);
        }
        subject = fz_dictgets(info, "Subject");
        if (subject) {
            subjectStr = PdfToString(subject);
        }
        producer = fz_dictgets(info, "Producer");
        if (producer) {
            producerStr = PdfToString(producer);
        }
        creator = fz_dictgets(info, "Creator");
        if (creator) {
            creatorStr = PdfToString(creator);
        }
        creationDate = fz_dictgets(info, "CreationDate");
        creationDateStr = PdfDateToDisplay(creationDate);

        modDate = fz_dictgets(info, "ModDate");
        modDateStr = PdfDateToDisplay(modDate);
    }

    if (win->dm->fileName()) {
        AddPdfProperty(layoutData, _TR("File:"), win->dm->fileName());
    }
    if (titleStr) {
        AddPdfProperty(layoutData, _TR("Title:"), titleStr);
        free(titleStr);
    }
    if (subjectStr) {
        AddPdfProperty(layoutData, _TR("Subject:"), subjectStr);
        free(subjectStr);
    }
    if (authorStr) {
        AddPdfProperty(layoutData, _TR("Author:"), authorStr);
        free(authorStr);
    }
    if (creationDateStr) {
        AddPdfProperty(layoutData, _TR("Created:"), creationDateStr);
        free(creationDateStr);
    }
    if (modDateStr) {
        AddPdfProperty(layoutData, _TR("Modified:"), modDateStr);
        free(modDateStr);
    }
    if (creatorStr) {
        AddPdfProperty(layoutData, _TR("Application:"), creatorStr);
        free(creatorStr);
    }
    if (producerStr) {
        AddPdfProperty(layoutData, _TR("PDF Producer:"), producerStr);
        free(producerStr);
    }

    int version = win->dm->pdfEngine->getPdfVersion();
    tmp = tstr_printf(_T("%d.%d"), version / 10, version % 10);
    AddPdfProperty(layoutData, _TR("PDF Version:"), tmp);
    free(tmp);

    fileSize = WinFileSizeGet(win->dm->fileName());
    if (fileSize == INVALID_FILE_SIZE) {
        fz_buffer *data = win->dm->pdfEngine->getStreamData();
        fileSize = data->len;
        fz_dropbuffer(data);
    }
    tmp = FormatPdfSize(fileSize);
    AddPdfProperty(layoutData, _TR("File Size:"), tmp);
    free(tmp);

    tmp = tstr_printf(_T("%d"), dm->pageCount());
    AddPdfProperty(layoutData, _TR("Number of Pages:"), tmp);
    free(tmp);

    tmp = FormatPdfPageSize(dm->getPageInfo(dm->currentPageNo())->page);
    AddPdfProperty(layoutData, _TR("Page Size:"), tmp);
    free(tmp);

    tmp = FormatPdfPermissions(dm->pdfEngine);
    if (tmp) {
        AddPdfProperty(layoutData, _TR("Denied Permissions:"), tmp);
        free(tmp);
    }

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

    RECT rc;
    GetClientRect(hwnd, &rc);
    FillRect(hdc, &rc, brushBg);

#if 0
    SelectObject(hdc, brushBg);
    SelectObject(hdc, penBorder);
#endif

    SetTextColor(hdc, WIN_COL_BLACK);

    /* render text on the left*/
    (HFONT)SelectObject(hdc, fontLeftTxt);
    for (PdfPropertyEl *el = layoutData->first; el; el = el->next)
        TextOut(hdc, el->leftPos.x, el->leftPos.y, el->leftTxt, lstrlen(el->leftTxt));

    /* render text on the right */
    (HFONT)SelectObject(hdc, fontRightTxt);
    for (PdfPropertyEl *el = layoutData->first; el; el = el->next)
        TextOut(hdc, el->rightPos.x, el->rightPos.y, el->rightTxt, lstrlen(el->rightTxt));

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
        HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, (lstrlen(result) + 1) * sizeof(TCHAR));
        if (handle) {
            TCHAR *selText = (TCHAR *)GlobalLock(handle);
            lstrcpy(selText, result);
            GlobalUnlock(handle);

            EmptyClipboard();
#ifdef UNICODE
            SetClipboardData(CF_UNICODETEXT, handle);
#else
            SetClipboardData(CF_TEXT, handle);
#endif
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
