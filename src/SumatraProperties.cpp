/* Copyright 2006-2010 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "SumatraPDF.h"
#include "AppPrefs.h"

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

static bool IsUtf16be(uint8_t *s, unsigned short len) {
    if (len < 2)
        return false;
    if (len % 2 != 0)
        return false;
    if ((0xfe == s[0]) && (0xff == s[1]))
        return true;
    return false;
}

static bool IsUtf16le(uint8_t *s, unsigned short len) {
    if (len < 2)
        return false;
    if (len % 2 != 0)
        return false;
    if ((0xff == s[0]) && (0xfe == s[1]))
        return true;
    return false;
}

#define SWAPWORD(x)	MAKEWORD(HIBYTE(x), LOBYTE(x))

static char *Utf16beToUtf8(uint8_t *s, unsigned short len) {
    WCHAR *utf16 = (WCHAR *)malloc(len + 2);
    memcpy(utf16, s, len);

    // convert from big- to little-endian
    for (int i = 0; i < len / 2; i++)
        utf16[i] = SWAPWORD(utf16[i]);
    utf16[len / 2] = '\0';

    char *utf8 = wstr_to_utf8(utf16 + 1);
    free(utf16);
    return utf8;
}

// Note: returns NULL instead of an empty string
static char *fz_toutf8(fz_obj *obj) {
    int len = fz_tostrlen(obj);
    char *s = fz_tostrbuf(obj);

    if (IsUtf16be((uint8_t *)s, len))
        return len > 2 ? Utf16beToUtf8((uint8_t *)s, len) : NULL;
    if (IsUtf16le((uint8_t *)s, len))
        return len > 2 ? wstr_to_utf8(((WCHAR *)s) + 1) : NULL;
    // TODO: it's possible we need to support other utf encodings,
    // but we'll wait until we have a PDF that needs that
    return len > 0 ? str_dup(s) : NULL;
}

static char *PdfDateParseInt(char *s, int numDigits, int *valOut) {
    int n = 0;
    while (numDigits > 0) {
        char c = *s++;
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
static bool PdfDateParse(char *pdfDate, SYSTEMTIME *timeOut) {
    int n;

    // "D:" at the beginning is optional
    if ('D' == pdfDate[0] && ':' == pdfDate[1]) {
        pdfDate += 2;
    }
    // parse YYYY part
    pdfDate = PdfDateParseInt(pdfDate, 4, &n);
    if (!pdfDate) { return false; }
    timeOut->wYear = n;
    // parse MM part
    pdfDate = PdfDateParseInt(pdfDate, 2, &n);
    if (!pdfDate) { return false; }
    timeOut->wMonth = n;    
    // parse DD part
    pdfDate = PdfDateParseInt(pdfDate, 2, &n);
    if (!pdfDate) { return false; }
    timeOut->wDay = n;
    // parse HH part
    pdfDate = PdfDateParseInt(pdfDate, 2, &n);
    if (!pdfDate) { return false; }
    timeOut->wHour = n;
    // parse MM part
    pdfDate = PdfDateParseInt(pdfDate, 2, &n);
    if (!pdfDate) { return false; }
    timeOut->wMinute = n;

    timeOut->wSecond = 0;
    timeOut->wMilliseconds = 0;
    // TODO: this is wrong but I don't know how to calculate wDayOfWeek
    // and it doesn't matter anyway because we don't display day of the week
    timeOut->wDayOfWeek = 0;
    return true;
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
    char *s;
    int cchBufLen = dimof(buf);

    if (!dateObj) {
        return NULL;
    }
    s = fz_toutf8(dateObj);
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
    tmp = utf8_to_tstr(s);
    free(s);
    return tmp;
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
static TCHAR *FormatPdfPageSize(double width, double height) {
    TCHAR unitSystem[2];
    GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_IMEASURE, unitSystem, dimof(unitSystem));
    bool isMetric = unitSystem[0] == '0';

    width *= (isMetric ? 2.54 : 1.0) / 72;
    height *= (isMetric ? 2.54 : 1.0) / 72;

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
    TStrList *denials = NULL;
    if (!pdfEngine->hasPermission(PDF_PERM_PRINT))
        TStrList_Insert(&denials, (TCHAR *)_TR("printing document"));
    if (!pdfEngine->hasPermission(PDF_PERM_COPY))
        TStrList_Insert(&denials, (TCHAR *)_TR("copying text"));

    TStrList_Reverse(&denials);
    TCHAR *denialList = TStrList_Join(denials, _T(", "));
    TStrList_Destroy(&denials);

    if (!*denialList) {
        free(denialList);
        return NULL;
    }

    return denialList;
}

static void AddPdfProperty(WindowInfo *win, const TCHAR *left, const TCHAR *right) {
    if (win->pdfPropertiesCount >= MAX_PDF_PROPERTIES) {
        return;
    }
    win->pdfProperties[win->pdfPropertiesCount].leftTxt = left;
    win->pdfProperties[win->pdfPropertiesCount].rightTxt = tstr_dup(right);
    ++win->pdfPropertiesCount;
}

static void AddPdfPropertyUtf8(WindowInfo *win, const TCHAR *left, const char *right) {
    TCHAR *rightTxt = utf8_to_tstr(right);
    AddPdfProperty(win, left, rightTxt);
    free(rightTxt);
}

void FreePdfProperties(WindowInfo *win)
{
    // free the text on the right. Text on left is static, so doesn't need to
    // be freed
    for (int i=0; i<win->pdfPropertiesCount; i++) {
        free((void*)win->pdfProperties[i].rightTxt);
    }
    win->pdfPropertiesCount = 0;
}

static void UpdatePropertiesLayout(HWND hwnd, HDC hdc, RECT *rect) {
    SIZE            txtSize;
    int             totalDx, totalDy;
    int             leftDy, rightDy;
    int             leftMaxDx, rightMaxDx;
    int             currY;
    int             offX, offY;
    const TCHAR *   txt;
    WindowInfo *    win = WindowInfo_FindByHwnd(hwnd);

    HFONT fontLeftTxt = Win32_Font_GetSimple(hdc, LEFT_TXT_FONT, LEFT_TXT_FONT_SIZE);
    HFONT fontRightTxt = Win32_Font_GetSimple(hdc, RIGHT_TXT_FONT, RIGHT_TXT_FONT_SIZE);
    HFONT origFont = (HFONT)SelectObject(hdc, fontLeftTxt);

    /* calculate text dimensions for the left side */
    (HFONT)SelectObject(hdc, fontLeftTxt);
    leftMaxDx = 0;
    leftDy = 0;
    for (int i = 0; i < win->pdfPropertiesCount; i++) {
        txt = win->pdfProperties[i].leftTxt;
        GetTextExtentPoint32(hdc, txt, lstrlen(txt), &txtSize);
        win->pdfProperties[i].leftTxtDx = (int)txtSize.cx;
        win->pdfProperties[i].leftTxtDy = (int)txtSize.cy;

        if (i > 0) {
            assert(win->pdfProperties[i-1].leftTxtDy == win->pdfProperties[i].leftTxtDy);
        }

        if (win->pdfProperties[i].leftTxtDx > leftMaxDx) {
            leftMaxDx = win->pdfProperties[i].leftTxtDx;
        }
    }

    /* calculate text dimensions for the right side */
    (HFONT)SelectObject(hdc, fontRightTxt);
    rightMaxDx = 0;
    rightDy = 0;
    for (int i = 0; i < win->pdfPropertiesCount; i++) {
        txt = win->pdfProperties[i].rightTxt;
        GetTextExtentPoint32(hdc, txt, lstrlen(txt), &txtSize);
        win->pdfProperties[i].rightTxtDx = (int)txtSize.cx;
        win->pdfProperties[i].rightTxtDy = (int)txtSize.cy;

        if (i > 0) {
            assert(win->pdfProperties[i-1].rightTxtDy == win->pdfProperties[i].rightTxtDy);
        }

        if (win->pdfProperties[i].rightTxtDx > rightMaxDx) {
            rightMaxDx = win->pdfProperties[i].rightTxtDx;
        }
    }

    int textDy = win->pdfProperties[0].rightTxtDy;

    totalDx = leftMaxDx + PROPERTIES_LEFT_RIGHT_SPACE_DX + rightMaxDx;

    totalDy = 4;
    totalDy += (win->pdfPropertiesCount * (textDy + PROPERTIES_TXT_DY_PADDING));
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
    for (int i=0; i < win->pdfPropertiesCount; i++) {
        win->pdfProperties[i].leftTxtPosX = offX + leftMaxDx - win->pdfProperties[i].leftTxtDx;
        win->pdfProperties[i].leftTxtPosY = offY + currY;
        win->pdfProperties[i].rightTxtPosX = offX + leftMaxDx + PROPERTIES_LEFT_RIGHT_SPACE_DX;
        win->pdfProperties[i].rightTxtPosY = offY + currY;
        currY += (textDy + PROPERTIES_TXT_DY_PADDING);
    }

    SelectObject(hdc, origFont);
    Win32_Font_Delete(fontLeftTxt);
    Win32_Font_Delete(fontRightTxt);
}

static void CreatePropertiesWindow(WindowInfo *win) {
    win->hwndPdfProperties = CreateWindow(
           PROPERTIES_CLASS_NAME, PROPERTIES_WIN_TITLE,
           WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
           CW_USEDEFAULT, CW_USEDEFAULT,
           CW_USEDEFAULT, CW_USEDEFAULT,
           NULL, NULL,
           ghinst, NULL);
    if (!win->hwndPdfProperties)
        return;

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
    pdf_xref *  xref;
    fz_obj *    info = NULL;
    fz_obj *    subject = NULL;
    fz_obj *    author = NULL;
    fz_obj *    title = NULL;
    fz_obj *    producer = NULL;
    fz_obj *    creator = NULL;
    fz_obj *    creationDate = NULL;
    fz_obj *    modDate = NULL;

    char *      subjectStr = NULL;
    char *      authorStr = NULL;
    char *      titleStr = NULL;
    char *      producerStr = NULL;
    char *      creatorStr  = NULL;
    TCHAR *     creationDateStr = NULL;
    TCHAR *     modDateStr = NULL;

    if (win->hwndPdfProperties) {
        SetActiveWindow(win->hwndPdfProperties);
        return;
    }

    DisplayModel *dm = win->dm;
    if (!dm || !dm->pdfEngine || !dm->pdfEngine->_xref) {
        return;
    }

    FreePdfProperties(win);

    xref = dm->pdfEngine->_xref;
    info = fz_dictgets(xref->trailer, "Info");;

    if (!fz_isdict(info)) {
        info = NULL;
    }

    if (info) {
        title = fz_dictgets(info, "Title");
        if (title) {
            titleStr = fz_toutf8(title);
        }
        author = fz_dictgets(info, "Author");
        if (author) {
            authorStr = fz_toutf8(author);
        }
        subject = fz_dictgets(info, "Subject");
        if (subject) {
            subjectStr = fz_toutf8(subject);
        }
        producer = fz_dictgets(info, "Producer");
        if (producer) {
            producerStr = fz_toutf8(producer);
        }
        creator = fz_dictgets(info, "Creator");
        if (creator) {
            creatorStr = fz_toutf8(creator);
        }
        creationDate = fz_dictgets(info, "CreationDate");
        creationDateStr = PdfDateToDisplay(creationDate);

        modDate = fz_dictgets(info, "ModDate");
        modDateStr = PdfDateToDisplay(modDate);
    }

    if (win->dm->fileName()) {
        AddPdfProperty(win, _TR("File:"), win->dm->fileName());
    }
    if (titleStr) {
        AddPdfPropertyUtf8(win, _TR("Title:"), titleStr);
        free(titleStr);
    }
    if (subjectStr) {
        AddPdfPropertyUtf8(win, _TR("Subject:"), subjectStr);
        free(subjectStr);
    }
    if (authorStr) {
        AddPdfPropertyUtf8(win, _TR("Author:"), authorStr);
        free(authorStr);
    }
    if (creationDateStr) {
        AddPdfProperty(win, _TR("Created:"), creationDateStr);
        free(creationDateStr);
    }
    if (modDateStr) {
        AddPdfProperty(win, _TR("Modified:"), modDateStr);
        free(modDateStr);
    }
    if (creatorStr) {
        AddPdfPropertyUtf8(win, _TR("Application:"), creatorStr);
        free(creatorStr);
    }
    if (producerStr) {
        AddPdfPropertyUtf8(win, _TR("PDF Producer:"), producerStr);
        free(producerStr);
    }

    tmp = tstr_printf(_T("%d.%d"), xref->version / 10, xref->version % 10);
    AddPdfProperty(win, _TR("PDF Version:"), tmp);
    free(tmp);

    fileSize = WinFileSizeGet(win->dm->fileName());
    tmp = FormatPdfSize(fileSize);
    AddPdfProperty(win, _TR("File Size:"), tmp);
    free(tmp);

    tmp = tstr_printf(_T("%d"), dm->pageCount());
    AddPdfProperty(win, _TR("Number of Pages:"), tmp);
    free(tmp);

    tmp = FormatPdfPageSize(dm->getPageInfo(dm->currentPageNo())->pageDx, dm->getPageInfo(dm->currentPageNo())->pageDy);
    AddPdfProperty(win, _TR("Page Size:"), tmp);
    free(tmp);

    tmp = FormatPdfPermissions(dm->pdfEngine);
    if (tmp) {
        AddPdfProperty(win, _TR("Denied Permissions:"), tmp);
        free(tmp);
    }

    // TODO: this is about linearlized PDF. Looks like mupdf would
    // have to be extended to detect linearlized PDF. The rules are described
    // in F3.3 of http://www.adobe.com/devnet/acrobat/pdfs/PDF32000_2008.pdf
    //AddPdfProperty(win, _T("Fast Web View:"), _T("No"));

    // TODO: probably needs to extend mupdf to get this information.
    // Tagged PDF rules are described in 14.8.2 of
    // http://www.adobe.com/devnet/acrobat/pdfs/PDF32000_2008.pdf
    //AddPdfProperty(win, _T("Tagged PDF:"), _T("No"));

    CreatePropertiesWindow(win);
}

static void DrawProperties(HWND hwnd, HDC hdc, RECT *rect)
{
    const TCHAR *txt;
    int          x, y;
    WindowInfo * win = WindowInfo_FindByHwnd(hwnd);
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

    SetTextColor(hdc, COL_BLACK);

    /* render text on the left*/
    (HFONT)SelectObject(hdc, fontLeftTxt);
    for (int i = 0; i < win->pdfPropertiesCount; i++) {
        txt = win->pdfProperties[i].leftTxt;
        x = win->pdfProperties[i].leftTxtPosX;
        y = win->pdfProperties[i].leftTxtPosY;
        TextOut(hdc, x, y, txt, lstrlen(txt));
    }

    /* render text on the right */
    (HFONT)SelectObject(hdc, fontRightTxt);
    for (int i = 0; i < win->pdfPropertiesCount; i++) {
        txt = win->pdfProperties[i].rightTxt;
        x = win->pdfProperties[i].rightTxtPosX;
        y = win->pdfProperties[i].rightTxtPosY;
        TextOut(hdc, x, y, txt, lstrlen(txt));
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

void CopyPropertiesToClipboard(WindowInfo *win)
{
    TCHAR *result = tstr_dup(_T(""));

    // just concatenate all the properties into a multi-line string
    for (INT i = 0; i < win->pdfPropertiesCount; i++) {
        TCHAR *newResult = tstr_printf(_T("%s%s %s\r\n"), result, win->pdfProperties[i].leftTxt, win->pdfProperties[i].rightTxt);
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
    WindowInfo *win = WindowInfo_FindByHwnd(hwnd);
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
            assert(win->hwndPdfProperties);
            win->hwndPdfProperties = NULL;
            break;

        /* TODO: handle mouse move/down/up so that links work */
        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}

