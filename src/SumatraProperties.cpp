/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/WinUtil.h"

#include "wingui/TreeModel.h"
#include "EngineBase.h"
#include "EngineManager.h"
#include "SettingsStructs.h"
#include "Controller.h"
#include "DisplayModel.h"
#include "AppColors.h"
#include "ProgressUpdateUI.h"
#include "Notifications.h"
#include "SumatraPDF.h"
#include "WindowInfo.h"
#include "resource.h"
#include "SumatraProperties.h"
#include "Translations.h"

#define PROPERTIES_LEFT_RIGHT_SPACE_DX 8
#define PROPERTIES_RECT_PADDING 8
#define PROPERTIES_TXT_DY_PADDING 2
#define PROPERTIES_WIN_TITLE _TR("Document Properties")

constexpr double KB = 1024;
constexpr double MB = 1024 * 1024;
constexpr double GB = 1024 * 1024 * 1024;

class PropertyEl {
  public:
    PropertyEl(const WCHAR* leftTxt, WCHAR* rightTxt, bool isPath = false) : leftTxt(leftTxt), isPath(isPath) {
        this->rightTxt.Set(rightTxt);
    }

    // A property is always in format: Name (left): Value (right)
    // (leftTxt is static, rightTxt will be freed)
    const WCHAR* leftTxt;
    AutoFreeWstr rightTxt;

    // data calculated by the layout
    RectI leftPos;
    RectI rightPos;

    // overlong paths get the ellipsis in the middle instead of at the end
    bool isPath;
};

class PropertiesLayout : public Vec<PropertyEl*> {
  public:
    PropertiesLayout() : hwnd(nullptr), hwndParent(nullptr) {
    }
    ~PropertiesLayout() {
        DeleteVecMembers(*this);
    }

    void AddProperty(const WCHAR* key, WCHAR* value, bool isPath = false) {
        // don't display value-less properties
        if (!str::IsEmpty(value))
            Append(new PropertyEl(key, value, isPath));
        else
            free(value);
    }
    bool HasProperty(const WCHAR* key) {
        for (size_t i = 0; i < size(); i++) {
            if (str::Eq(key, at(i)->leftTxt))
                return true;
        }
        return false;
    }

    HWND hwnd;
    HWND hwndParent;
};

static Vec<PropertiesLayout*> gPropertiesWindows;

static PropertiesLayout* FindPropertyWindowByParent(HWND hwndParent) {
    for (size_t i = 0; i < gPropertiesWindows.size(); i++) {
        PropertiesLayout* pl = gPropertiesWindows.at(i);
        if (pl->hwndParent == hwndParent)
            return pl;
    }
    return nullptr;
}

static PropertiesLayout* FindPropertyWindowByHwnd(HWND hwnd) {
    for (size_t i = 0; i < gPropertiesWindows.size(); i++) {
        PropertiesLayout* pl = gPropertiesWindows.at(i);
        if (pl->hwnd == hwnd)
            return pl;
    }
    return nullptr;
}

void DeletePropertiesWindow(HWND hwndParent) {
    PropertiesLayout* pl = FindPropertyWindowByParent(hwndParent);
    if (pl)
        DestroyWindow(pl->hwnd);
}

// See: http://www.verypdf.com/pdfinfoeditor/pdf-date-format.htm
// Format:  "D:YYYYMMDDHHMMSSxxxxxxx"
// Example: "D:20091222171933-05'00'"
static bool PdfDateParse(const WCHAR* pdfDate, SYSTEMTIME* timeOut) {
    ZeroMemory(timeOut, sizeof(SYSTEMTIME));
    // "D:" at the beginning is optional
    if (str::StartsWith(pdfDate, L"D:"))
        pdfDate += 2;
    return str::Parse(pdfDate,
                      L"%4d%2d%2d"
                      L"%2d%2d%2d",
                      &timeOut->wYear, &timeOut->wMonth, &timeOut->wDay, &timeOut->wHour, &timeOut->wMinute,
                      &timeOut->wSecond) != nullptr;
    // don't bother about the day of week, we won't display it anyway
}

// See: ISO 8601 specification
// Format:  "YYYY-MM-DDTHH:MM:SSZ"
// Example: "2011-04-19T22:10:48Z"
static bool IsoDateParse(const WCHAR* isoDate, SYSTEMTIME* timeOut) {
    ZeroMemory(timeOut, sizeof(SYSTEMTIME));
    const WCHAR* end = str::Parse(isoDate, L"%4d-%2d-%2d", &timeOut->wYear, &timeOut->wMonth, &timeOut->wDay);
    if (end) // time is optional
        str::Parse(end, L"T%2d:%2d:%2dZ", &timeOut->wHour, &timeOut->wMinute, &timeOut->wSecond);
    return end != nullptr;
    // don't bother about the day of week, we won't display it anyway
}

static WCHAR* FormatSystemTime(SYSTEMTIME& date) {
    WCHAR buf[512] = {0};
    int cchBufLen = dimof(buf);
    int ret = GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &date, nullptr, buf, cchBufLen);
    if (ret < 2) // GetDateFormat() failed or returned an empty result
        return nullptr;

    // don't add 00:00:00 for dates without time
    if (0 == date.wHour && 0 == date.wMinute && 0 == date.wSecond)
        return str::Dup(buf);

    WCHAR* tmp = buf + ret;
    tmp[-1] = ' ';
    ret = GetTimeFormat(LOCALE_USER_DEFAULT, 0, &date, nullptr, tmp, cchBufLen - ret);
    if (ret < 2) // GetTimeFormat() failed or returned an empty result
        tmp[-1] = '\0';

    return str::Dup(buf);
}

// Convert a date in PDF or XPS format, e.g. "D:20091222171933-05'00'" to a display
// format e.g. "12/22/2009 5:19:33 PM"
// See: http://www.verypdf.com/pdfinfoeditor/pdf-date-format.htm
// The conversion happens in place
static void ConvDateToDisplay(WCHAR** s, bool (*DateParse)(const WCHAR* date, SYSTEMTIME* timeOut)) {
    if (!s || !*s || !DateParse)
        return;

    SYSTEMTIME date = {0};
    bool ok = DateParse(*s, &date);
    if (!ok)
        return;

    WCHAR* formatted = FormatSystemTime(date);
    if (formatted) {
        free(*s);
        *s = formatted;
    }
}

// Format the file size in a short form that rounds to the largest size unit
// e.g. "3.48 GB", "12.38 MB", "23 KB"
// Caller needs to free the result.
static WCHAR* FormatSizeSuccint(size_t size) {
    const WCHAR* unit = nullptr;
    double s = (double)size;

    if (s > GB) {
        s = s / GB;
        unit = _TR("GB");
    } else if (s > MB) {
        s = s / MB;
        unit = _TR("MB");
    } else {
        s = s / KB;
        unit = _TR("KB");
    }

    AutoFreeWstr sizestr = str::FormatFloatWithThousandSep(s);
    if (!unit) {
        return sizestr.StealData();
    }
    return str::Format(L"%s %s", sizestr.Get(), unit);
}

// format file size in a readable way e.g. 1348258 is shown
// as "1.29 MB (1,348,258 Bytes)"
// Caller needs to free the result
static WCHAR* FormatFileSize(size_t size) {
    AutoFreeWstr n1(FormatSizeSuccint(size));
    AutoFreeWstr n2(str::FormatNumWithThousandSep(size));

    return str::Format(L"%s (%s %s)", n1.Get(), n2.Get(), _TR("Bytes"));
}

PaperFormat GetPaperFormat(SizeD size) {
    SizeD sizeP = size.dx < size.dy ? size : SizeD(size.dy, size.dx);
    // common ISO 216 formats (metric)
    if (limitValue(sizeP.dx, 16.53, 16.55) == sizeP.dx && limitValue(sizeP.dy, 23.38, 23.40) == sizeP.dy)
        return PaperFormat::A2;
    if (limitValue(sizeP.dx, 11.68, 11.70) == sizeP.dx && limitValue(sizeP.dy, 16.53, 16.55) == sizeP.dy)
        return PaperFormat::A3;
    if (limitValue(sizeP.dx, 8.26, 8.28) == sizeP.dx && limitValue(sizeP.dy, 11.68, 11.70) == sizeP.dy)
        return PaperFormat::A4;
    if (limitValue(sizeP.dx, 5.82, 5.85) == sizeP.dx && limitValue(sizeP.dy, 8.26, 8.28) == sizeP.dy)
        return PaperFormat::A5;
    if (limitValue(sizeP.dx, 4.08, 4.10) == sizeP.dx && limitValue(sizeP.dy, 5.82, 5.85) == sizeP.dy)
        return PaperFormat::A6;
    // common US/ANSI formats (imperial)
    if (limitValue(sizeP.dx, 8.49, 8.51) == sizeP.dx && limitValue(sizeP.dy, 10.99, 11.01) == sizeP.dy)
        return PaperFormat::Letter;
    if (limitValue(sizeP.dx, 8.49, 8.51) == sizeP.dx && limitValue(sizeP.dy, 13.99, 14.01) == sizeP.dy)
        return PaperFormat::Legal;
    if (limitValue(sizeP.dx, 10.99, 11.01) == sizeP.dx && limitValue(sizeP.dy, 16.99, 17.01) == sizeP.dy)
        return PaperFormat::Tabloid;
    if (limitValue(sizeP.dx, 5.49, 5.51) == sizeP.dx && limitValue(sizeP.dy, 8.49, 8.51) == sizeP.dy)
        return PaperFormat::Statement;
    return PaperFormat::Other;
}

// format page size according to locale (e.g. "29.7 x 21.0 cm" or "11.69 x 8.27 in")
// Caller needs to free the result
static WCHAR* FormatPageSize(EngineBase* engine, int pageNo, int rotation) {
    RectD mediabox = engine->PageMediabox(pageNo);
    SizeD size = engine->Transform(mediabox, pageNo, 1.0f / engine->GetFileDPI(), rotation).Size();

    const WCHAR* formatName = L"";
    switch (GetPaperFormat(size)) {
        case PaperFormat::A2:
            formatName = L" (A2)";
            break;
        case PaperFormat::A3:
            formatName = L" (A3)";
            break;
        case PaperFormat::A4:
            formatName = L" (A4)";
            break;
        case PaperFormat::A5:
            formatName = L" (A5)";
            break;
        case PaperFormat::A6:
            formatName = L" (A6)";
            break;
        case PaperFormat::Letter:
            formatName = L" (Letter)";
            break;
        case PaperFormat::Legal:
            formatName = L" (Legal)";
            break;
        case PaperFormat::Tabloid:
            formatName = L" (Tabloid)";
            break;
        case PaperFormat::Statement:
            formatName = L" (Statement)";
            break;
    }

    bool isMetric = GetMeasurementSystem() == 0;
    double unitsPerInch = isMetric ? 2.54 : 1.0;
    const WCHAR* unit = isMetric ? L"cm" : L"in";

    double width = size.dx * unitsPerInch;
    double height = size.dy * unitsPerInch;
    if (((int)(width * 100)) % 100 == 99)
        width += 0.01;
    if (((int)(height * 100)) % 100 == 99)
        height += 0.01;

    AutoFreeWstr strWidth(str::FormatFloatWithThousandSep(width));
    AutoFreeWstr strHeight(str::FormatFloatWithThousandSep(height));

    return str::Format(L"%s x %s %s%s", strWidth.Get(), strHeight.Get(), unit, formatName);
}

static WCHAR* FormatPdfFileStructure(Controller* ctrl) {
    AutoFreeWstr fstruct(ctrl->GetProperty(DocumentProperty::PdfFileStructure));
    if (str::IsEmpty(fstruct.Get()))
        return nullptr;
    WStrVec parts;
    parts.Split(fstruct, L",", true);

    WStrVec props;

    if (parts.Contains(L"linearized"))
        props.Push(str::Dup(_TR("Fast Web View")));
    if (parts.Contains(L"tagged"))
        props.Push(str::Dup(_TR("Tagged PDF")));
    if (parts.Contains(L"PDFX"))
        props.Push(str::Dup(L"PDF/X (ISO 15930)"));
    if (parts.Contains(L"PDFA1"))
        props.Push(str::Dup(L"PDF/A (ISO 19005)"));
    if (parts.Contains(L"PDFE1"))
        props.Push(str::Dup(L"PDF/E (ISO 24517)"));

    return props.Join(L", ");
}

// returns a list of permissions denied by this document
// Caller needs to free the result
static WCHAR* FormatPermissions(Controller* ctrl) {
    if (!ctrl->AsFixed()) {
        return nullptr;
    }

    WStrVec denials;

    EngineBase* engine = ctrl->AsFixed()->GetEngine();
    if (!engine->AllowsPrinting()) {
        denials.Push(str::Dup(_TR("printing document")));
    }
    if (!engine->AllowsCopyingText()) {
        denials.Push(str::Dup(_TR("copying text")));
    }

    return denials.Join(L", ");
}

static void UpdatePropertiesLayout(PropertiesLayout* layoutData, HDC hdc, RectI* rect) {
    AutoDeleteFont fontLeftTxt(CreateSimpleFont(hdc, LEFT_TXT_FONT, LEFT_TXT_FONT_SIZE));
    AutoDeleteFont fontRightTxt(CreateSimpleFont(hdc, RIGHT_TXT_FONT, RIGHT_TXT_FONT_SIZE));
    HGDIOBJ origFont = SelectObject(hdc, fontLeftTxt);

    /* calculate text dimensions for the left side */
    SelectObject(hdc, fontLeftTxt);
    int leftMaxDx = 0;
    for (size_t i = 0; i < layoutData->size(); i++) {
        PropertyEl* el = layoutData->at(i);
        const WCHAR* txt = el->leftTxt;
        RECT rc = {0};
        DrawText(hdc, txt, -1, &rc, DT_NOPREFIX | DT_CALCRECT);
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
    for (size_t i = 0; i < layoutData->size(); i++) {
        PropertyEl* el = layoutData->at(i);
        const WCHAR* txt = el->rightTxt;
        RECT rc = {0};
        DrawText(hdc, txt, -1, &rc, DT_NOPREFIX | DT_CALCRECT);
        el->rightPos.dx = rc.right - rc.left;
        el->leftPos.dy = el->rightPos.dy = rc.bottom - rc.top;
        textDy += el->rightPos.dy;

        if (el->rightPos.dx > rightMaxDx)
            rightMaxDx = el->rightPos.dx;
        lineCount++;
    }

    AssertCrash(lineCount > 0 && textDy > 0);
    int totalDx = leftMaxDx + PROPERTIES_LEFT_RIGHT_SPACE_DX + rightMaxDx;

    int totalDy = 4;
    totalDy += textDy + (lineCount - 1) * PROPERTIES_TXT_DY_PADDING;
    totalDy += 4;

    int offset = PROPERTIES_RECT_PADDING;
    if (rect)
        *rect = RectI(0, 0, totalDx + 2 * offset, totalDy + offset);

    int currY = 0;
    for (size_t i = 0; i < layoutData->size(); i++) {
        PropertyEl* el = layoutData->at(i);
        el->leftPos = RectI(offset, offset + currY, leftMaxDx, el->leftPos.dy);
        el->rightPos.x = offset + leftMaxDx + PROPERTIES_LEFT_RIGHT_SPACE_DX;
        el->rightPos.y = offset + currY;
        currY += el->rightPos.dy + PROPERTIES_TXT_DY_PADDING;
    }

    SelectObject(hdc, origFont);
}

static bool CreatePropertiesWindow(HWND hParent, PropertiesLayout* layoutData) {
    CrashIf(layoutData->hwnd);
    HWND hwnd = CreateWindow(PROPERTIES_CLASS_NAME, PROPERTIES_WIN_TITLE, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                             CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr,
                             GetModuleHandle(nullptr), nullptr);
    if (!hwnd)
        return false;

    layoutData->hwnd = hwnd;
    layoutData->hwndParent = hParent;
    SetRtl(hwnd, IsUIRightToLeft());

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
    wRc.dx = std::min(rc.dx + wRc.dx - cRc.dx, work.dx);
    wRc.dy = std::min(rc.dy + wRc.dy - cRc.dy, work.dy);
    MoveWindow(hwnd, wRc.x, wRc.y, wRc.dx, wRc.dy, FALSE);
    CenterDialog(hwnd, hParent);

    ShowWindow(hwnd, SW_SHOW);
    return true;
}

static void GetProps(Controller* ctrl, PropertiesLayout* layoutData, bool extended) {
    CrashIf(!ctrl);

    WCHAR* str = str::Dup(gPluginMode ? gPluginURL : ctrl->FilePath());
    layoutData->AddProperty(_TR("File:"), str, true);

    str = ctrl->GetProperty(DocumentProperty::Title);
    layoutData->AddProperty(_TR("Title:"), str);

    str = ctrl->GetProperty(DocumentProperty::Subject);
    layoutData->AddProperty(_TR("Subject:"), str);

    str = ctrl->GetProperty(DocumentProperty::Author);
    layoutData->AddProperty(_TR("Author:"), str);

    str = ctrl->GetProperty(DocumentProperty::Copyright);
    layoutData->AddProperty(_TR("Copyright:"), str);

    DisplayModel* dm = ctrl->AsFixed();
    str = ctrl->GetProperty(DocumentProperty::CreationDate);
    if (str && dm && kindEnginePdf == dm->engineType) {
        ConvDateToDisplay(&str, PdfDateParse);
    } else {
        ConvDateToDisplay(&str, IsoDateParse);
    }
    layoutData->AddProperty(_TR("Created:"), str);

    str = ctrl->GetProperty(DocumentProperty::ModificationDate);
    if (str && dm && kindEnginePdf == dm->engineType)
        ConvDateToDisplay(&str, PdfDateParse);
    else
        ConvDateToDisplay(&str, IsoDateParse);
    layoutData->AddProperty(_TR("Modified:"), str);

    str = ctrl->GetProperty(DocumentProperty::CreatorApp);
    layoutData->AddProperty(_TR("Application:"), str);

    str = ctrl->GetProperty(DocumentProperty::PdfProducer);
    layoutData->AddProperty(_TR("PDF Producer:"), str);

    str = ctrl->GetProperty(DocumentProperty::PdfVersion);
    layoutData->AddProperty(_TR("PDF Version:"), str);

    str = FormatPdfFileStructure(ctrl);
    layoutData->AddProperty(_TR("PDF Optimizations:"), str);

    AutoFreeStr path = strconv::WstrToUtf8(ctrl->FilePath());
    int64_t fileSize = file::GetSize(path.as_view());
    if (-1 == fileSize && dm) {
        EngineBase* engine = dm->GetEngine();
        AutoFree d = engine->GetFileData();
        if (!d.empty()) {
            fileSize = d.size();
        }
    }
    if (-1 != fileSize) {
        str = FormatFileSize((size_t)fileSize);
        layoutData->AddProperty(_TR("File Size:"), str);
    }

    // TODO: display page count per current layout for ebooks?
    if (!ctrl->AsEbook()) {
        str = str::Format(L"%d", ctrl->PageCount());
        layoutData->AddProperty(_TR("Number of Pages:"), str);
    }

    if (dm) {
        str = FormatPageSize(dm->GetEngine(), ctrl->CurrentPageNo(), dm->GetRotation());
        if (IsUIRightToLeft() && IsVistaOrGreater()) {
            // ensure that the size remains ungarbled left-to-right
            // (note: XP doesn't know about \u202A...\u202C)
            WCHAR* tmp = str;
            str = str::Format(L"\u202A%s\u202C", tmp);
            free(tmp);
        }
        layoutData->AddProperty(_TR("Page Size:"), str);
    }

    str = FormatPermissions(ctrl);
    layoutData->AddProperty(_TR("Denied Permissions:"), str);

#if defined(DEBUG) || defined(ENABLE_EXTENDED_PROPERTIES)
    if (extended) {
        // TODO: FontList extraction can take a while
        str = ctrl->GetProperty(DocumentProperty::FontList);
        if (str) {
            // add a space between basic and extended file properties
            layoutData->AddProperty(L" ", str::Dup(L" "));
        }
        layoutData->AddProperty(_TR("Fonts:"), str);
    }
#else
    UNUSED(extended);
#endif
}

static void ShowProperties(HWND parent, Controller* ctrl, bool extended = false) {
    PropertiesLayout* layoutData = FindPropertyWindowByParent(parent);
    if (layoutData) {
        SetActiveWindow(layoutData->hwnd);
        return;
    }

    if (!ctrl)
        return;
    layoutData = new PropertiesLayout();
    gPropertiesWindows.Append(layoutData);
    GetProps(ctrl, layoutData, extended);

    if (!CreatePropertiesWindow(parent, layoutData))
        delete layoutData;
}

void OnMenuProperties(WindowInfo* win) {
    ShowProperties(win->hwndFrame, win->ctrl);
}

static void DrawProperties(HWND hwnd, HDC hdc) {
    PropertiesLayout* layoutData = FindPropertyWindowByHwnd(hwnd);

    AutoDeleteFont fontLeftTxt(CreateSimpleFont(hdc, LEFT_TXT_FONT, LEFT_TXT_FONT_SIZE));
    AutoDeleteFont fontRightTxt(CreateSimpleFont(hdc, RIGHT_TXT_FONT, RIGHT_TXT_FONT_SIZE));

    HGDIOBJ origFont = SelectObject(hdc, fontLeftTxt); /* Just to remember the orig font */

    SetBkMode(hdc, TRANSPARENT);

    ClientRect rcClient(hwnd);
    RECT rTmp = rcClient.ToRECT();
    auto col = GetAppColor(AppColor::MainWindowBg);
    ScopedGdiObj<HBRUSH> brushAboutBg(CreateSolidBrush(col));
    FillRect(hdc, &rTmp, brushAboutBg);

    col = GetAppColor(AppColor::MainWindowText);
    SetTextColor(hdc, col);

    /* render text on the left*/
    SelectObject(hdc, fontLeftTxt);
    for (size_t i = 0; i < layoutData->size(); i++) {
        PropertyEl* el = layoutData->at(i);
        const WCHAR* txt = el->leftTxt;
        rTmp = el->leftPos.ToRECT();
        DrawText(hdc, txt, -1, &rTmp, DT_RIGHT | DT_NOPREFIX);
    }

    /* render text on the right */
    SelectObject(hdc, fontRightTxt);
    for (size_t i = 0; i < layoutData->size(); i++) {
        PropertyEl* el = layoutData->at(i);
        const WCHAR* txt = el->rightTxt;
        RectI rc = el->rightPos;
        if (rc.x + rc.dx > rcClient.x + rcClient.dx - PROPERTIES_RECT_PADDING)
            rc.dx = rcClient.x + rcClient.dx - PROPERTIES_RECT_PADDING - rc.x;
        rTmp = rc.ToRECT();
        UINT format = DT_LEFT | DT_NOPREFIX | (el->isPath ? DT_PATH_ELLIPSIS : DT_WORD_ELLIPSIS);
        DrawText(hdc, txt, -1, &rTmp, format);
    }

    SelectObject(hdc, origFont);
}

static void OnPaintProperties(HWND hwnd) {
    PAINTSTRUCT ps;
    RectI rc;
    HDC hdc = BeginPaint(hwnd, &ps);
    UpdatePropertiesLayout(FindPropertyWindowByHwnd(hwnd), hdc, &rc);
    DrawProperties(hwnd, hdc);
    EndPaint(hwnd, &ps);
}

static void CopyPropertiesToClipboard(HWND hwnd) {
    PropertiesLayout* layoutData = FindPropertyWindowByHwnd(hwnd);
    if (!layoutData)
        return;

    // concatenate all the properties into a multi-line string
    str::WStr lines(256);
    for (size_t i = 0; i < layoutData->size(); i++) {
        PropertyEl* el = layoutData->at(i);
        lines.AppendFmt(L"%s %s\r\n", el->leftTxt, el->rightTxt.get());
    }

    CopyTextToClipboard(lines.LendData());
}

static void PropertiesOnCommand(HWND hwnd, WPARAM wParam) {
    switch (LOWORD(wParam)) {
        case IDM_COPY_SELECTION:
            CopyPropertiesToClipboard(hwnd);
            break;

        case IDM_PROPERTIES:
#if defined(DEBUG) || defined(ENABLE_EXTENDED_PROPERTIES)
            // make a repeated Ctrl+D display some extended properties
            // TODO: expose this through a UI button or similar
            PropertiesLayout* pl = FindPropertyWindowByHwnd(hwnd);
            if (pl) {
                WindowInfo* win = FindWindowInfoByHwnd(pl->hwndParent);
                if (win && !pl->HasProperty(_TR("Fonts:"))) {
                    DestroyWindow(hwnd);
                    ShowProperties(win->hwndFrame, win->ctrl, true);
                }
            }
#endif
            break;
    }
}

LRESULT CALLBACK WndProcProperties(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    PropertiesLayout* pl;

    switch (message) {
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
