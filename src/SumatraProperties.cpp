/* Copyright 2006-2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "EbookController.h"
#include "FileUtil.h"
#include "MobiDoc.h"
#include "MobiWindow.h"
#include "resource.h"
#include "Scoped.h"
#include "SumatraPDF.h"
#include "SumatraProperties.h"
#include "Translations.h"
#include "WindowInfo.h"
#include "WinUtil.h"

#define PROPERTIES_LEFT_RIGHT_SPACE_DX 8
#define PROPERTIES_RECT_PADDING     8
#define PROPERTIES_TXT_DY_PADDING 2
#define PROPERTIES_WIN_TITLE    _TR("Document Properties")

static Vec<PropertiesLayout*> gPropertiesWindows;

static PropertiesLayout* FindPropertyWindowByParent(HWND hwndParent)
{
    for (size_t i = 0; i < gPropertiesWindows.Count(); i++) {
        PropertiesLayout *pl = gPropertiesWindows.At(i);
        if (pl->hwndParent == hwndParent)
            return pl;
    }
    return NULL;
}

static PropertiesLayout* FindPropertyWindowByHwnd(HWND hwnd)
{
    for (size_t i = 0; i < gPropertiesWindows.Count(); i++) {
        PropertiesLayout *pl = gPropertiesWindows.At(i);
        if (pl->hwnd == hwnd)
            return pl;
    }
    return NULL;
}

void DeletePropertiesWindow(HWND hwndParent)
{
    PropertiesLayout *pl = FindPropertyWindowByParent(hwndParent);
    if (pl)
        DestroyWindow(pl->hwnd);
}

// See: http://www.verypdf.com/pdfinfoeditor/pdf-date-format.htm
// Format:  "D:YYYYMMDDHHMMSSxxxxxxx"
// Example: "D:20091222171933-05'00'"
static bool PdfDateParse(const TCHAR *pdfDate, SYSTEMTIME *timeOut)
{
    ZeroMemory(timeOut, sizeof(SYSTEMTIME));
    // "D:" at the beginning is optional
    if (str::StartsWith(pdfDate, _T("D:")))
        pdfDate += 2;
    return str::Parse(pdfDate, _T("%4d%2d%2d") _T("%2d%2d%2d"),
        &timeOut->wYear, &timeOut->wMonth, &timeOut->wDay,
        &timeOut->wHour, &timeOut->wMinute, &timeOut->wSecond) != NULL;
    // don't bother about the day of week, we won't display it anyway
}

// See: ISO 8601 specification
// Format:  "YYYY-MM-DDTHH:MM:SSZ"
// Example: "2011-04-19T22:10:48Z"
static bool IsoDateParse(const TCHAR *xpsDate, SYSTEMTIME *timeOut)
{
    ZeroMemory(timeOut, sizeof(SYSTEMTIME));
    const TCHAR *end = str::Parse(xpsDate, _T("%4d-%2d-%2d"), &timeOut->wYear, &timeOut->wMonth, &timeOut->wDay);
    if (end) // time is optional
        str::Parse(end, _T("T%2d:%2d:%2dZ"), &timeOut->wHour, &timeOut->wMinute, &timeOut->wSecond);
    return end != NULL;
    // don't bother about the day of week, we won't display it anyway
}

static TCHAR *FormatSystemTime(SYSTEMTIME& date)
{
    TCHAR buf[512];
    int cchBufLen = dimof(buf);
    int ret = GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &date, NULL, buf, cchBufLen);
    if (0 == ret) // GetDateFormat() failed
        ret = 1;

    TCHAR *tmp = buf + ret - 1;
    if (ret > 1)
        *tmp++ = _T(' ');
    cchBufLen -= ret;
    ret = GetTimeFormat(LOCALE_USER_DEFAULT, 0, &date, NULL, tmp, cchBufLen);
    if (0 == ret) // GetTimeFormat() failed
        *tmp = '\0';

    return tmp > buf ? str::Dup(buf) : NULL;
}

// Convert a date in PDF or XPS format, e.g. "D:20091222171933-05'00'" to a display
// format e.g. "12/22/2009 5:19:33 PM"
// See: http://www.verypdf.com/pdfinfoeditor/pdf-date-format.htm
// The conversion happens in place
static void ConvDateToDisplay(TCHAR **s, bool (* DateParse)(const TCHAR *date, SYSTEMTIME *timeOut))
{
    if (!s || !*s || !DateParse)
        return;

    SYSTEMTIME date;
    bool ok = DateParse(*s, &date);
    free(*s);

    *s = ok ? FormatSystemTime(date) : NULL;
}

// Format the file size in a short form that rounds to the largest size unit
// e.g. "3.48 GB", "12.38 MB", "23 KB"
// Caller needs to free the result.
static TCHAR *FormatSizeSuccint(size_t size)
{
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

    ScopedMem<TCHAR> sizestr(str::FormatFloatWithThousandSep(s));
    if (!unit)
        return sizestr.StealData();
    return str::Format(_T("%s %s"), sizestr, unit);
}

// format file size in a readable way e.g. 1348258 is shown
// as "1.29 MB (1,348,258 Bytes)"
// Caller needs to free the result
static TCHAR *FormatFileSize(size_t size)
{
    ScopedMem<TCHAR> n1(FormatSizeSuccint(size));
    ScopedMem<TCHAR> n2(str::FormatNumWithThousandSep(size));

    return str::Format(_T("%s (%s %s)"), n1, n2, _TR("Bytes"));
}

// format page size according to locale (e.g. "29.7 x 20.9 cm" or "11.69 x 8.23 in")
// Caller needs to free the result
static TCHAR *FormatPageSize(BaseEngine *engine, int pageNo, int rotation)
{
    RectD mediabox = engine->PageMediabox(pageNo);
    SizeD size = engine->Transform(mediabox, pageNo, 1.0, rotation).Size();

    TCHAR unitSystem[2];
    GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_IMEASURE, unitSystem, dimof(unitSystem));
    bool isMetric = unitSystem[0] == '0';
    double unitsPerInch = isMetric ? 2.54 : 1.0;

    double width = size.dx * unitsPerInch / engine->GetFileDPI();
    double height = size.dy * unitsPerInch / engine->GetFileDPI();
    if (((int)(width * 100)) % 100 == 99)
        width += 0.01;
    if (((int)(height * 100)) % 100 == 99)
        height += 0.01;

    ScopedMem<TCHAR> strWidth(str::FormatFloatWithThousandSep(width));
    ScopedMem<TCHAR> strHeight(str::FormatFloatWithThousandSep(height));

    return str::Format(_T("%s x %s %s"), strWidth, strHeight, isMetric ? _T("cm") : _T("in"));
}

// returns a list of permissions denied by this document
// Caller needs to free the result
static TCHAR *FormatPermissions(BaseEngine *engine)
{
    StrVec denials;

    if (!engine->IsPrintingAllowed())
        denials.Push(str::Dup(_TR("printing document")));
    if (!engine->IsCopyingTextAllowed())
        denials.Push(str::Dup(_TR("copying text")));

    return denials.Join(_T(", "));
}

void PropertiesLayout::AddProperty(const TCHAR *key, TCHAR *value)
{
    // don't display value-less properties
    if (!str::IsEmpty(value))
        Append(new PropertyEl(key, value));
    else
        free(value);
}

bool PropertiesLayout::HasProperty(const TCHAR *key)
{
    for (size_t i = 0; i < Count(); i++) {
        if (str::Eq(key, At(i)->leftTxt))
            return true;
    }
    return false;
}

static void UpdatePropertiesLayout(PropertiesLayout *layoutData, HDC hdc, RectI *rect)
{
    ScopedFont fontLeftTxt(GetSimpleFont(hdc, LEFT_TXT_FONT, LEFT_TXT_FONT_SIZE));
    ScopedFont fontRightTxt(GetSimpleFont(hdc, RIGHT_TXT_FONT, RIGHT_TXT_FONT_SIZE));
    HGDIOBJ origFont = SelectObject(hdc, fontLeftTxt);

    /* calculate text dimensions for the left side */
    SelectObject(hdc, fontLeftTxt);
    int leftMaxDx = 0;
    for (size_t i = 0; i < layoutData->Count(); i++) {
        PropertyEl *el = layoutData->At(i);
        RECT rc = { 0 };
        DrawText(hdc, el->leftTxt, -1, &rc, DT_NOPREFIX | DT_CALCRECT);
        el->leftPos.dx = rc.right - rc.left;
        // el->leftPos.dy is set below to be equal to el->rightPos.dy

        if (el->leftPos.dx > leftMaxDx)
            leftMaxDx = el->leftPos.dx;
    }

    /* calculate text dimensions for the right side */
    SelectObject(hdc, fontRightTxt);
    int rightMaxDx = 0;
    int lineCount = 0;
    int textDy = 0;
    for (size_t i = 0; i < layoutData->Count(); i++) {
        PropertyEl *el = layoutData->At(i);
        RECT rc = { 0 };
        DrawText(hdc, el->rightTxt, -1, &rc, DT_NOPREFIX | DT_CALCRECT);
        el->rightPos.dx = rc.right - rc.left;
        el->leftPos.dy = el->rightPos.dy = rc.bottom - rc.top;
        textDy += el->rightPos.dy;

        if (el->rightPos.dx > rightMaxDx)
            rightMaxDx = el->rightPos.dx;
        lineCount++;
    }

    assert(lineCount > 0 && textDy > 0);
    int totalDx = leftMaxDx + PROPERTIES_LEFT_RIGHT_SPACE_DX + rightMaxDx;

    int totalDy = 4;
    totalDy += textDy + (lineCount - 1) * PROPERTIES_TXT_DY_PADDING;
    totalDy += 4;

    int offset = PROPERTIES_RECT_PADDING;
    if (rect)
        *rect = RectI(0, 0, totalDx + 2 * offset, totalDy + offset);

    int currY = 0;
    for (size_t i = 0; i < layoutData->Count(); i++) {
        PropertyEl *el = layoutData->At(i);
        el->leftPos = RectI(offset, offset + currY, leftMaxDx, el->leftPos.dy);
        el->rightPos.x = offset + leftMaxDx + PROPERTIES_LEFT_RIGHT_SPACE_DX;
        el->rightPos.y = offset + currY;
        currY += el->rightPos.dy + PROPERTIES_TXT_DY_PADDING;
    }

    SelectObject(hdc, origFont);
}

static bool CreatePropertiesWindow(HWND hParent, PropertiesLayout* layoutData)
{
    CrashIf(layoutData->hwnd);
    HWND hwnd = CreateWindow(
           PROPERTIES_CLASS_NAME, PROPERTIES_WIN_TITLE,
           WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
           CW_USEDEFAULT, CW_USEDEFAULT,
           CW_USEDEFAULT, CW_USEDEFAULT,
           NULL, NULL,
           ghinst, NULL);
    if (!hwnd)
        return false;

    layoutData->hwnd = hwnd;
    layoutData->hwndParent = hParent;
    ToggleWindowStyle(hwnd, WS_EX_LAYOUTRTL | WS_EX_NOINHERITLAYOUT, IsUIRightToLeft(), GWL_EXSTYLE);

    // get the dimensions required for the about box's content
    RectI rc;
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    UpdatePropertiesLayout(layoutData, hdc, &rc);
    EndPaint(hwnd, &ps);

    // resize the new window to just match these dimensions
    WindowRect wRc(hwnd);
    ClientRect cRc(hwnd);
    wRc.dx += rc.dx - cRc.dx;
    wRc.dy += rc.dy - cRc.dy;
    MoveWindow(hwnd, wRc.x, wRc.y, wRc.dx, wRc.dy, FALSE);
    CenterDialog(hwnd, hParent);

    ShowWindow(hwnd, SW_SHOW);
    return true;
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
static void GetProps(BaseEngine *engine, EngineType engineType, PropertiesLayout *layoutData)
{
    TCHAR *str = str::Dup(gPluginMode ? gPluginURL : engine->FileName());
    layoutData->AddProperty(_TR("File:"), str);

    str = engine->GetProperty("Title");
    layoutData->AddProperty(_TR("Title:"), str);

    str = engine->GetProperty("Subject");
    layoutData->AddProperty(_TR("Subject:"), str);

    str = engine->GetProperty("Author");
    layoutData->AddProperty(_TR("Author:"), str);

    str = engine->GetProperty("Copyright");
    layoutData->AddProperty(_TR("Copyright:"), str);

    str = engine->GetProperty("CreationDate");
    if (Engine_PDF == engineType)
        ConvDateToDisplay(&str, PdfDateParse);
    else if (Engine_XPS == engineType)
        ConvDateToDisplay(&str, IsoDateParse);
    else if (Engine_Epub == engineType)
        ConvDateToDisplay(&str, IsoDateParse);
    layoutData->AddProperty(_TR("Created:"), str);

    str = engine->GetProperty("ModDate");
    if (Engine_PDF == engineType)
        ConvDateToDisplay(&str, PdfDateParse);
    else if (Engine_XPS == engineType)
        ConvDateToDisplay(&str, IsoDateParse);
    else if (Engine_Epub == engineType)
        ConvDateToDisplay(&str, IsoDateParse);
    layoutData->AddProperty(_TR("Modified:"), str);

    str = engine->GetProperty("Creator");
    layoutData->AddProperty(_TR("Application:"), str);

    str = engine->GetProperty("Producer");
    layoutData->AddProperty(_TR("PDF Producer:"), str);

    str = engine->GetProperty("PdfVersion");
    layoutData->AddProperty(_TR("PDF Version:"), str);

    size_t fileSize = file::GetSize(engine->FileName());
    if (fileSize == INVALID_FILE_SIZE) {
        unsigned char *data = engine->GetFileData(&fileSize);
        free(data);
    }
    if (fileSize != INVALID_FILE_SIZE) {
        str = FormatFileSize(fileSize);
        layoutData->AddProperty(_TR("File Size:"), str);
    }

    str = str::Format(_T("%d"), engine->PageCount());
    layoutData->AddProperty(_TR("Number of Pages:"), str);
}

static void ShowProperties(WindowInfo& win, bool extended=false)
{
    TCHAR *str;
    PropertiesLayout *layoutData = FindPropertyWindowByParent(win.hwndFrame);

    if (layoutData) {
        SetActiveWindow(layoutData->hwnd);
        return;
    }

    if (!win.IsDocLoaded())
        return;
    BaseEngine *engine = win.dm->engine;
    layoutData = new PropertiesLayout();
    gPropertiesWindows.Append(layoutData);
    GetProps(engine, win.dm->engineType, layoutData);

    if (!win.IsChm()) {
        str = FormatPageSize(engine, win.dm->CurrentPageNo(), win.dm->Rotation());
#ifdef UNICODE
        if (IsUIRightToLeft() && WindowsVerVistaOrGreater()) {
            ScopedMem<TCHAR> tmp(str);
            // ensure that the size remains ungarbled left-to-right
            // (note: XP doesn't know about \u202A...\u202C)
            str = str::Format(_T("\u202A%s\u202C"), tmp);
        }
#endif
        layoutData->AddProperty(_TR("Page Size:"), str);
    }

    str = FormatPermissions(engine);
    layoutData->AddProperty(_TR("Denied Permissions:"), str);

    // TODO: this is about linearlized PDF. Looks like mupdf would
    // have to be extended to detect linearlized PDF. The rules are described
    // in F3.3 of http://www.adobe.com/devnet/acrobat/pdfs/PDF32000_2008.pdf
    // layoutData->AddProperty(_T("Fast Web View:"), str::Dup(_T("No")));

    // TODO: probably needs to extend mupdf to get this information.
    // Tagged PDF rules are described in 14.8.2 of
    // http://www.adobe.com/devnet/acrobat/pdfs/PDF32000_2008.pdf
    // layoutData->AddProperty(_T("Tagged PDF:"), str::Dup(_T("No")));

#if defined(DEBUG) || defined(ENABLE_EXTENDED_PROPERTIES)
    if (extended) {
        // TODO: FontList extraction can take a while
        str = engine->GetProperty("FontList");
        if (str) {
            // add a space between basic and extended file properties
            layoutData->AddProperty(_T(" "), str::Dup(_T(" ")));
        }
        layoutData->AddProperty(_TR("Fonts:"), str);
    }
#endif

    if (!CreatePropertiesWindow(win.hwndFrame, layoutData))
        delete layoutData;
}
static void GetMobiProps(MobiDoc *mobiDoc, PropertiesLayout *layoutData)
{
    TCHAR *str = str::Dup(gPluginMode ? gPluginURL : mobiDoc->GetFileName());
    layoutData->AddProperty(_TR("File:"), str);

    size_t fileSize = file::GetSize(mobiDoc->GetFileName());
    if (fileSize != INVALID_FILE_SIZE) {
        str = FormatFileSize(fileSize);
        layoutData->AddProperty(_TR("File Size:"), str);
    }

}

static bool GetProps(Doc doc, PropertiesLayout *layoutData)
{
    if (doc.AsMobi()) {
        GetMobiProps(doc.AsMobi(), layoutData);
        return true;
    } else if (doc.AsEpub()) {
        // TODO: GetEpubProps(doc.AsEpub(), layoutData);
        return false;
    }
    CrashIf(true);
    return false;
}

static void ShowProperties(MobiWindow *win)
{
    PropertiesLayout *layoutData = FindPropertyWindowByParent(win->hwndFrame);
    if (layoutData) {
        SetActiveWindow(layoutData->hwnd);
        return;
    }
    Doc doc = win->ebookController->GetDoc();
    if (!doc.IsNone())
        return;
    layoutData = new PropertiesLayout();
    gPropertiesWindows.Append(layoutData);

    if (!GetProps(doc, layoutData)) {
        delete layoutData;
        return;
    }

    // TODO: show properties
    if (!CreatePropertiesWindow(win->hwndFrame, layoutData))
        delete layoutData;
}

void OnMenuProperties(SumatraWindow& win)
{
    if (win.AsWindowInfo())
        ShowProperties(*win.AsWindowInfo());
    else if (win.AsMobiWindow())
        ShowProperties(win.AsMobiWindow());
}

static void DrawProperties(HWND hwnd, HDC hdc)
{
    PropertiesLayout *layoutData = FindPropertyWindowByHwnd(hwnd);
#if 0
    HPEN penBorder = CreatePen(PS_SOLID, ABOUT_LINE_OUTER_SIZE, COL_BLACK);
#endif

    ScopedFont fontLeftTxt(GetSimpleFont(hdc, LEFT_TXT_FONT, LEFT_TXT_FONT_SIZE));
    ScopedFont fontRightTxt(GetSimpleFont(hdc, RIGHT_TXT_FONT, RIGHT_TXT_FONT_SIZE));

    HGDIOBJ origFont = SelectObject(hdc, fontLeftTxt); /* Just to remember the orig font */

    SetBkMode(hdc, TRANSPARENT);

    ClientRect rcClient(hwnd);
    FillRect(hdc, &rcClient.ToRECT(), gBrushAboutBg);

#if 0
    SelectObject(hdc, gBrushAboutBg);
    SelectObject(hdc, penBorder);
#endif

    SetTextColor(hdc, WIN_COL_BLACK);

    /* render text on the left*/
    SelectObject(hdc, fontLeftTxt);
    for (size_t i = 0; i < layoutData->Count(); i++) {
        PropertyEl *el = layoutData->At(i);
        DrawText(hdc, el->leftTxt, -1, &el->leftPos.ToRECT(), DT_RIGHT | DT_NOPREFIX);
    }

    /* render text on the right */
    SelectObject(hdc, fontRightTxt);
    for (size_t i = 0; i < layoutData->Count(); i++) {
        PropertyEl *el = layoutData->At(i);
        RectI rc = el->rightPos;
        if (rc.x + rc.dx > rcClient.x + rcClient.dx - PROPERTIES_RECT_PADDING)
            rc.dx = rcClient.x + rcClient.dx - PROPERTIES_RECT_PADDING - rc.x;
        DrawText(hdc, el->rightTxt, -1, &rc.ToRECT(), DT_LEFT | DT_PATH_ELLIPSIS | DT_NOPREFIX);
    }

    SelectObject(hdc, origFont);

#if 0
    DeleteObject(penBorder);
#endif
}

static void OnPaintProperties(HWND hwnd)
{
    PAINTSTRUCT ps;
    RectI rc;
    HDC hdc = BeginPaint(hwnd, &ps);
    UpdatePropertiesLayout(FindPropertyWindowByHwnd(hwnd), hdc, &rc);
    DrawProperties(hwnd, hdc);
    EndPaint(hwnd, &ps);
}

// returns true if properties have been copied to the clipboard
static void CopyPropertiesToClipboard(HWND hwnd)
{
    PropertiesLayout *layoutData = FindPropertyWindowByHwnd(hwnd);
    if (!layoutData)
        return;

    // concatenate all the properties into a multi-line string
    str::Str<TCHAR> lines(256);
    for (size_t i = 0; i < layoutData->Count(); i++) {
        PropertyEl *el = layoutData->At(i);
        lines.AppendFmt(_T("%s %s\r\n"), el->leftTxt, el->rightTxt);
    }

    CopyTextToClipboard(lines.LendData());
}

static void PropertiesOnCommand(HWND hwnd, WPARAM wParam)
{
    switch (LOWORD(wParam)) {
    case IDM_COPY_SELECTION:
        CopyPropertiesToClipboard(hwnd);
        break;

    case IDM_PROPERTIES:
#if defined(DEBUG) || defined(ENABLE_EXTENDED_PROPERTIES)
        // make a repeated Ctrl+D display some extended properties
        // TODO: expose this through a UI button or similar
        PropertiesLayout *pl = FindPropertyWindowByHwnd(hwnd);
        if (pl) {
            WindowInfo *win = FindWindowInfoByHwnd(pl->hwndParent);
            if (win && !pl->HasProperty(_TR("Fonts:"))) {
                DestroyWindow(hwnd);
                ShowProperties(*win, true);
            }
        }
#endif
        break;
    }
}

LRESULT CALLBACK WndProcProperties(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    PropertiesLayout *pl;

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
            pl = FindPropertyWindowByHwnd(hwnd);
            CrashIf(!pl);
            gPropertiesWindows.Remove(pl);
            delete pl;
            break;

        case WM_COMMAND:
            PropertiesOnCommand(hwnd, wParam);
            break;

        /* TODO: handle mouse move/down/up so that links work (?) */
        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}
