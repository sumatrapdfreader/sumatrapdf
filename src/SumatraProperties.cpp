/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "SumatraProperties.h"

#include "EbookWindow.h"
#include "FileUtil.h"
#include "resource.h"
#include "SumatraPDF.h"
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
static bool PdfDateParse(const WCHAR *pdfDate, SYSTEMTIME *timeOut)
{
    ZeroMemory(timeOut, sizeof(SYSTEMTIME));
    // "D:" at the beginning is optional
    if (str::StartsWith(pdfDate, L"D:"))
        pdfDate += 2;
    return str::Parse(pdfDate, L"%4d%2d%2d" L"%2d%2d%2d",
        &timeOut->wYear, &timeOut->wMonth, &timeOut->wDay,
        &timeOut->wHour, &timeOut->wMinute, &timeOut->wSecond) != NULL;
    // don't bother about the day of week, we won't display it anyway
}

// See: ISO 8601 specification
// Format:  "YYYY-MM-DDTHH:MM:SSZ"
// Example: "2011-04-19T22:10:48Z"
static bool IsoDateParse(const WCHAR *xpsDate, SYSTEMTIME *timeOut)
{
    ZeroMemory(timeOut, sizeof(SYSTEMTIME));
    const WCHAR *end = str::Parse(xpsDate, L"%4d-%2d-%2d", &timeOut->wYear, &timeOut->wMonth, &timeOut->wDay);
    if (end) // time is optional
        str::Parse(end, L"T%2d:%2d:%2dZ", &timeOut->wHour, &timeOut->wMinute, &timeOut->wSecond);
    return end != NULL;
    // don't bother about the day of week, we won't display it anyway
}

static WCHAR *FormatSystemTime(SYSTEMTIME& date)
{
    WCHAR buf[512];
    int cchBufLen = dimof(buf);
    int ret = GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &date, NULL, buf, cchBufLen);
    if (0 == ret) // GetDateFormat() failed
        ret = 1;

    WCHAR *tmp = buf + ret - 1;
    if (ret > 1)
        *tmp++ = ' ';
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
static void ConvDateToDisplay(WCHAR **s, bool (* DateParse)(const WCHAR *date, SYSTEMTIME *timeOut))
{
    if (!s || !*s || !DateParse)
        return;

    SYSTEMTIME date;
    bool ok = DateParse(*s, &date);
    if (!ok)
        return;

    WCHAR *formatted = FormatSystemTime(date);
    if (formatted) {
        free(*s);
        *s = formatted;
    }
}

// Format the file size in a short form that rounds to the largest size unit
// e.g. "3.48 GB", "12.38 MB", "23 KB"
// Caller needs to free the result.
static WCHAR *FormatSizeSuccint(size_t size)
{
    const WCHAR *unit = NULL;
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

    ScopedMem<WCHAR> sizestr(str::FormatFloatWithThousandSep(s));
    if (!unit)
        return sizestr.StealData();
    return str::Format(L"%s %s", sizestr, unit);
}

// format file size in a readable way e.g. 1348258 is shown
// as "1.29 MB (1,348,258 Bytes)"
// Caller needs to free the result
static WCHAR *FormatFileSize(size_t size)
{
    ScopedMem<WCHAR> n1(FormatSizeSuccint(size));
    ScopedMem<WCHAR> n2(str::FormatNumWithThousandSep(size));

    return str::Format(L"%s (%s %s)", n1, n2, _TR("Bytes"));
}

// format page size according to locale (e.g. "29.7 x 20.9 cm" or "11.69 x 8.23 in")
// Caller needs to free the result
static WCHAR *FormatPageSize(BaseEngine *engine, int pageNo, int rotation)
{
    RectD mediabox = engine->PageMediabox(pageNo);
    SizeD size = engine->Transform(mediabox, pageNo, 1.0, rotation).Size();

    WCHAR unitSystem[2];
    GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_IMEASURE, unitSystem, dimof(unitSystem));
    bool isMetric = unitSystem[0] == '0';
    double unitsPerInch = isMetric ? 2.54 : 1.0;

    double width = size.dx * unitsPerInch / engine->GetFileDPI();
    double height = size.dy * unitsPerInch / engine->GetFileDPI();
    if (((int)(width * 100)) % 100 == 99)
        width += 0.01;
    if (((int)(height * 100)) % 100 == 99)
        height += 0.01;

    ScopedMem<WCHAR> strWidth(str::FormatFloatWithThousandSep(width));
    ScopedMem<WCHAR> strHeight(str::FormatFloatWithThousandSep(height));

    return str::Format(L"%s x %s %s", strWidth, strHeight, isMetric ? L"cm" : L"in");
}

static WCHAR *FormatPdfFileStructure(Doc doc)
{
    ScopedMem<WCHAR> fstruct(doc.GetProperty(Prop_PdfFileStructure));
    if (str::IsEmpty(fstruct.Get()))
        return NULL;
    WStrVec parts;
    parts.Split(fstruct, L",", true);
    
    WStrVec props;

    if (parts.Find(L"linearized") != -1)
        props.Push(str::Dup(_TR("Fast Web View")));
    if (parts.Find(L"tagged") != -1)
        props.Push(str::Dup(_TR("Tagged PDF")));
    if (parts.Find(L"PDFX") != -1)
        props.Push(str::Dup(L"PDF/X (ISO 15930)"));
    if (parts.Find(L"PDFA1") != -1)
        props.Push(str::Dup(L"PDF/A (ISO 19005)"));
    if (parts.Find(L"PDFE1") != -1)
        props.Push(str::Dup(L"PDF/E (ISO 24517)"));

    return props.Join(L", ");
}

// returns a list of permissions denied by this document
// Caller needs to free the result
static WCHAR *FormatPermissions(Doc doc)
{
    if (!doc.IsEngine())
        return NULL;

    WStrVec denials;

    if (!doc.AsEngine()->IsPrintingAllowed())
        denials.Push(str::Dup(_TR("printing document")));
    if (!doc.AsEngine()->IsCopyingTextAllowed())
        denials.Push(str::Dup(_TR("copying text")));

    return denials.Join(L", ");
}

void PropertiesLayout::AddProperty(const WCHAR *key, WCHAR *value)
{
    // don't display value-less properties
    if (!str::IsEmpty(value))
        Append(new PropertyEl(key, value));
    else
        free(value);
}

bool PropertiesLayout::HasProperty(const WCHAR *key)
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
    // (as long as they fit into the current monitor's work area)
    WindowRect wRc(hwnd);
    ClientRect cRc(hwnd);
    RectI work = GetWorkAreaRect(WindowRect(hParent));
    wRc.dx = min(rc.dx + wRc.dx - cRc.dx, work.dx);
    wRc.dy = min(rc.dy + wRc.dy - cRc.dy, work.dy);
    MoveWindow(hwnd, wRc.x, wRc.y, wRc.dx, wRc.dy, FALSE);
    CenterDialog(hwnd, hParent);

    ShowWindow(hwnd, SW_SHOW);
    return true;
}

static void GetProps(Doc doc, PropertiesLayout *layoutData, DisplayModel *dm, bool extended)
{
    CrashIf(!doc.IsEngine() && !doc.IsEbook());
    DocType engineType = doc.GetDocType();

    WCHAR *str = str::Dup(gPluginMode ? gPluginURL : doc.GetFilePath());
    layoutData->AddProperty(_TR("File:"), str);

    str = doc.GetProperty(Prop_Title);
    layoutData->AddProperty(_TR("Title:"), str);

    str = doc.GetProperty(Prop_Subject);
    layoutData->AddProperty(_TR("Subject:"), str);

    str = doc.GetProperty(Prop_Author);
    layoutData->AddProperty(_TR("Author:"), str);

    str = doc.GetProperty(Prop_Copyright);
    layoutData->AddProperty(_TR("Copyright:"), str);

    str = doc.GetProperty(Prop_CreationDate);
    if (Engine_PDF == engineType)
        ConvDateToDisplay(&str, PdfDateParse);
    else if (Engine_XPS == engineType)
        ConvDateToDisplay(&str, IsoDateParse);
    else if (Engine_Epub == engineType || Doc_Epub == engineType)
        ConvDateToDisplay(&str, IsoDateParse);
    else if (Engine_Mobi == engineType || Doc_Mobi == engineType)
        ConvDateToDisplay(&str, IsoDateParse);
    layoutData->AddProperty(_TR("Created:"), str);

    str = doc.GetProperty(Prop_ModificationDate);
    if (Engine_PDF == engineType)
        ConvDateToDisplay(&str, PdfDateParse);
    else if (Engine_XPS == engineType)
        ConvDateToDisplay(&str, IsoDateParse);
    else if (Engine_Epub == engineType || Doc_Epub == engineType)
        ConvDateToDisplay(&str, IsoDateParse);
    else if (Engine_Mobi == engineType || Doc_Mobi == engineType)
        ConvDateToDisplay(&str, IsoDateParse);
    layoutData->AddProperty(_TR("Modified:"), str);

    str = doc.GetProperty(Prop_CreatorApp);
    layoutData->AddProperty(_TR("Application:"), str);

    str = doc.GetProperty(Prop_PdfProducer);
    // TODO: remove PDF from string
    layoutData->AddProperty(_TR("PDF Producer:"), str);

    str = doc.GetProperty(Prop_PdfVersion);
    layoutData->AddProperty(_TR("PDF Version:"), str);

    str = FormatPdfFileStructure(doc);
    layoutData->AddProperty(_TR("PDF Optimizations:"), str);

    size_t fileSize = file::GetSize(doc.GetFilePath());
    if (fileSize == INVALID_FILE_SIZE && doc.IsEngine())
        free(doc.AsEngine()->GetFileData(&fileSize));
    if (fileSize != INVALID_FILE_SIZE) {
        str = FormatFileSize(fileSize);
        layoutData->AddProperty(_TR("File Size:"), str);
    }

    if (doc.IsEngine()) {
        str = str::Format(L"%d", doc.AsEngine()->PageCount());
        layoutData->AddProperty(_TR("Number of Pages:"), str);
    }

    if (dm && dm->engineType != Engine_Chm) {
        str = FormatPageSize(dm->engine, dm->CurrentPageNo(), dm->Rotation());
        if (IsUIRightToLeft() && IsVistaOrGreater()) {
            // ensure that the size remains ungarbled left-to-right
            // (note: XP doesn't know about \u202A...\u202C)
            str = str::Format(L"\u202A%s\u202C", ScopedMem<WCHAR>(str));
        }
        layoutData->AddProperty(_TR("Page Size:"), str);
    }

    str = FormatPermissions(doc);
    layoutData->AddProperty(_TR("Denied Permissions:"), str);

#if defined(DEBUG) || defined(ENABLE_EXTENDED_PROPERTIES)
    if (extended) {
        // TODO: FontList extraction can take a while
        str = doc.GetProperty(Prop_FontList);
        if (str) {
            // add a space between basic and extended file properties
            layoutData->AddProperty(L" ", str::Dup(L" "));
        }
        layoutData->AddProperty(_TR("Fonts:"), str);
    }
#endif
}

static void ShowProperties(HWND parent, Doc doc, DisplayModel *dm, bool extended=false)
{
    PropertiesLayout *layoutData = FindPropertyWindowByParent(parent);
    if (layoutData) {
        SetActiveWindow(layoutData->hwnd);
        return;
    }

    if (!doc.IsEngine() && !doc.IsEbook())
        return;
    layoutData = new PropertiesLayout();
    gPropertiesWindows.Append(layoutData);
    GetProps(doc, layoutData, dm, extended);

    if (!CreatePropertiesWindow(parent, layoutData))
        delete layoutData;
}

void OnMenuProperties(SumatraWindow& win)
{
    if (win.AsWindowInfo())
        ShowProperties(win.AsWindowInfo()->hwndFrame, GetDocForWindow(win), win.AsWindowInfo()->dm);
    else if (win.AsEbookWindow())
        ShowProperties(win.AsEbookWindow()->hwndFrame, GetDocForWindow(win), NULL);
}

static void DrawProperties(HWND hwnd, HDC hdc)
{
    PropertiesLayout *layoutData = FindPropertyWindowByHwnd(hwnd);

    ScopedFont fontLeftTxt(GetSimpleFont(hdc, LEFT_TXT_FONT, LEFT_TXT_FONT_SIZE));
    ScopedFont fontRightTxt(GetSimpleFont(hdc, RIGHT_TXT_FONT, RIGHT_TXT_FONT_SIZE));

    HGDIOBJ origFont = SelectObject(hdc, fontLeftTxt); /* Just to remember the orig font */

    SetBkMode(hdc, TRANSPARENT);

    ClientRect rcClient(hwnd);
    FillRect(hdc, &rcClient.ToRECT(), gBrushAboutBg);

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
    str::Str<WCHAR> lines(256);
    for (size_t i = 0; i < layoutData->Count(); i++) {
        PropertyEl *el = layoutData->At(i);
        lines.AppendFmt(L"%s %s\r\n", el->leftTxt, el->rightTxt);
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
                ShowProperties(win->hwndFrame, GetDocForWindow(SumatraWindow::Make(win)), win->dm, true);
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
