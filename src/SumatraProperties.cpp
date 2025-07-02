/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/StrFormat.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/WinUtil.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Settings.h"
#include "DocProperties.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "AppTools.h"
#include "AppColors.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "resource.h"
#include "Commands.h"
#include "HomePage.h"
#include "SumatraProperties.h"
#include "Translations.h"
#include "SumatraConfig.h"
#include "Print.h"
#include "Theme.h"
#include "DarkModeSubclass.h"

void ShowProperties(HWND parent, DocController* ctrl, bool extended);

constexpr const WCHAR* kPropertiesWinClassName = L"SUMATRA_PDF_PROPERTIES";

#define kLeftRightPaddingDx 8
#define kRectPadding 8
#define kTxtPaddingDy 2

LRESULT CALLBACK WndProcProperties(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

struct PropertiesLayout {
    struct Pos {
        // overlong paths get the ellipsis in the middle instead of at the end
        bool isPath;

        // data calculated by the layout
        Rect key;
        Rect val;
    };

    PropertiesLayout() = default;
    ~PropertiesLayout() {
        delete btnCopyToClipboard;
        delete btnGetFonts;
    }

    void AddProperty(const char* key, const char* value, bool isPath = false) {
        if (str::IsEmpty(value)) {
            // don't display value-less properties
            return;
        }
        strings.Append(key);
        strings.Append(value);
        Pos p = {isPath};
        positions.Append(p);
    }

    char* PropKey(int i) {
        int idx = i * 2;
        return strings.At(idx);
    }

    char* PropValue(int i) {
        int idx = (i * 2) + 1;
        return strings.At(idx);
    }

    int PropCount() const {
        return strings.Size() / 2;
    }

    bool HasProperty(const char* key) {
        int n = PropCount();
        for (int i = 0; i < n; i++) {
            char* k = PropKey(i);
            if (str::Eq(key, k)) {
                return true;
            }
        }
        return false;
    }

    Pos& GetPos(int i) {
        return positions.At(i);
    }

    HWND hwnd = nullptr;
    HWND hwndParent = nullptr;
    Button* btnCopyToClipboard = nullptr;
    Button* btnGetFonts = nullptr;

  private:
    StrVec strings;
    Vec<Pos> positions;
};

static Vec<PropertiesLayout*> gPropertiesWindows;

PropertiesLayout* FindPropertyWindowByHwnd(HWND hwnd) {
    for (PropertiesLayout* pl : gPropertiesWindows) {
        if (pl->hwnd == hwnd) {
            return pl;
        }
        if (pl->hwndParent == hwnd) {
            return pl;
        }
    }
    return nullptr;
}

void DeletePropertiesWindow(HWND hwndParent) {
    PropertiesLayout* pl = FindPropertyWindowByHwnd(hwndParent);
    if (pl) {
        DestroyWindow(pl->hwnd);
    }
}

// See: http://www.verypdf.com/pdfinfoeditor/pdf-date-format.htm
// Format:  "D:YYYYMMDDHHMMSSxxxxxxx"
// Example: "D:20091222171933-05'00'"
static bool PdfDateParseA(const char* pdfDate, SYSTEMTIME* timeOut) {
    ZeroMemory(timeOut, sizeof(SYSTEMTIME));
    // "D:" at the beginning is optional
    if (str::StartsWith(pdfDate, "D:")) {
        pdfDate += 2;
    }
    return str::Parse(pdfDate,
                      "%4d%2d%2d"
                      "%2d%2d%2d",
                      &timeOut->wYear, &timeOut->wMonth, &timeOut->wDay, &timeOut->wHour, &timeOut->wMinute,
                      &timeOut->wSecond) != nullptr;
    // don't bother about the day of week, we won't display it anyway
}

// See: ISO 8601 specification
// Format:  "YYYY-MM-DDTHH:MM:SSZ"
// Example: "2011-04-19T22:10:48Z"
static bool IsoDateParse(const char* isoDate, SYSTEMTIME* timeOut) {
    ZeroMemory(timeOut, sizeof(SYSTEMTIME));
    const char* end = str::Parse(isoDate, "%4d-%2d-%2d", &timeOut->wYear, &timeOut->wMonth, &timeOut->wDay);
    if (end) { // time is optional
        str::Parse(end, "T%2d:%2d:%2dZ", &timeOut->wHour, &timeOut->wMinute, &timeOut->wSecond);
    }
    return end != nullptr;
    // don't bother about the day of week, we won't display it anyway
}

static TempStr FormatSystemTimeTemp(SYSTEMTIME& date) {
    WCHAR bufW[512]{};
    int cchBufLen = dimof(bufW);
    int ret = GetDateFormatW(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &date, nullptr, bufW, cchBufLen);
    if (ret < 2) { // GetDateFormat() failed or returned an empty result
        return nullptr;
    }

    // don't add 00:00:00 for dates without time
    if (0 == date.wHour && 0 == date.wMinute && 0 == date.wSecond) {
        return ToUtf8Temp(bufW);
    }

    WCHAR* tmp = bufW + ret;
    tmp[-1] = ' ';
    ret = GetTimeFormatW(LOCALE_USER_DEFAULT, 0, &date, nullptr, tmp, cchBufLen - ret);
    if (ret < 2) { // GetTimeFormat() failed or returned an empty result
        tmp[-1] = '\0';
    }

    return ToUtf8Temp(bufW);
}

// Convert a date in PDF or XPS format, e.g. "D:20091222171933-05'00'" to a display
// format e.g. "12/22/2009 5:19:33 PM"
// See: http://www.verypdf.com/pdfinfoeditor/pdf-date-format.htm
// The conversion happens in place
static TempStr ConvDateToDisplayTemp(const char* s, bool (*dateParseFn)(const char* date, SYSTEMTIME* timeOut)) {
    if (!s || !*s || !dateParseFn) {
        return nullptr;
    }

    SYSTEMTIME date{};
    bool ok = dateParseFn(s, &date);
    if (!ok) {
        return nullptr;
    }

    return FormatSystemTimeTemp(date);
}

// format page size according to locale (e.g. "29.7 x 21.0 cm" or "11.69 x 8.27 in")
// Caller needs to free the result
static TempStr FormatPageSizeTemp(EngineBase* engine, int pageNo, int rotation) {
    RectF mediabox = engine->PageMediabox(pageNo);
    float zoom = 1.0f / engine->GetFileDPI();
    SizeF size = engine->Transform(mediabox, pageNo, zoom, rotation).Size();

    const char* formatName = "";
    switch (GetPaperFormatFromSizeApprox(size)) {
        case PaperFormat::A2:
            formatName = " (A2)";
            break;
        case PaperFormat::A3:
            formatName = " (A3)";
            break;
        case PaperFormat::A4:
            formatName = " (A4)";
            break;
        case PaperFormat::A5:
            formatName = " (A5)";
            break;
        case PaperFormat::A6:
            formatName = " (A6)";
            break;
        case PaperFormat::Letter:
            formatName = " (Letter)";
            break;
        case PaperFormat::Legal:
            formatName = " (Legal)";
            break;
        case PaperFormat::Tabloid:
            formatName = " (Tabloid)";
            break;
        case PaperFormat::Statement:
            formatName = " (Statement)";
            break;
    }

    bool isMetric = GetMeasurementSystem() == 0;
    double unitsPerInch = isMetric ? 2.54 : 1.0;
    const char* unit = isMetric ? "cm" : "in";

    double width = size.dx * unitsPerInch;
    double height = size.dy * unitsPerInch;
    if (((int)(width * 100)) % 100 == 99) {
        width += 0.01;
    }
    if (((int)(height * 100)) % 100 == 99) {
        height += 0.01;
    }

    char* strWidth = str::FormatFloatWithThousandSepTemp(width);
    char* strHeight = str::FormatFloatWithThousandSepTemp(height);

    return str::FormatTemp("%s x %s %s%s", strWidth, strHeight, unit, formatName);
}

// returns a list of permissions denied by this document
// Caller needs to free the result
static TempStr FormatPermissionsTemp(DocController* ctrl) {
    if (!ctrl->AsFixed()) {
        return nullptr;
    }

    StrVec denials;

    EngineBase* engine = ctrl->AsFixed()->GetEngine();
    if (!engine->AllowsPrinting()) {
        denials.Append(_TRA("printing document"));
    }
    if (!engine->AllowsCopyingText()) {
        denials.Append(_TRA("copying text"));
    }

    return JoinTemp(&denials, ", ");
}

static Rect CalcPropertiesLayout(PropertiesLayout* layoutData, HDC hdc) {
    HFONT fontLeftTxt = CreateSimpleFont(hdc, kLeftTextFont, kLeftTextFontSize);
    HFONT fontRightTxt = CreateSimpleFont(hdc, kRightTextFont, kRightTextFontSize);
    ScopedSelectFont origFont(hdc, fontLeftTxt);

    /* calculate text dimensions for the left side */
    SelectObject(hdc, fontLeftTxt);
    int leftMaxDx = 0;
    int nProps = layoutData->PropCount();
    for (int i = 0; i < nProps; i++) {
        auto& pos = layoutData->GetPos(i);
        const char* txt = layoutData->PropKey(i);
        RECT rc{};
        HdcDrawText(hdc, txt, &rc, DT_NOPREFIX | DT_CALCRECT);
        pos.key.dx = rc.right - rc.left;
        // el->leftPos.dy is set below to be equal to el->rightPos.dy

        if (pos.key.dx > leftMaxDx) {
            leftMaxDx = pos.key.dx;
        }
    }

    /* calculate text dimensions for the right side */
    SelectObject(hdc, fontRightTxt);
    int rightMaxDx = 0;
    int lineCount = 0;
    int textDy = 0;
    for (int i = 0; i < nProps; i++) {
        auto& pos = layoutData->GetPos(i);
        const char* txt = layoutData->PropValue(i);
        RECT rc{};
        HdcDrawText(hdc, txt, &rc, DT_NOPREFIX | DT_CALCRECT);
        auto dx = rc.right - rc.left;
        // limit the width or right text as some fields can be very long
        if (dx > 720) {
            dx = 720;
        }
        pos.val.dx = dx;
        pos.key.dy = pos.val.dy = rc.bottom - rc.top;
        textDy += pos.val.dy;

        if (dx > rightMaxDx) {
            rightMaxDx = dx;
        }
        lineCount++;
    }

    ReportIf(!(lineCount > 0 && textDy > 0));
    int totalDx = leftMaxDx + kLeftRightPaddingDx + rightMaxDx;

    int totalDy = 4;
    totalDy += textDy + (lineCount - 1) * kTxtPaddingDy;
    totalDy += 4;

    int offset = kRectPadding;

    int currY = 0;
    for (int i = 0; i < nProps; i++) {
        auto& pos = layoutData->GetPos(i);
        pos.key = Rect(offset, offset + currY, leftMaxDx, pos.key.dy);
        pos.val.x = offset + leftMaxDx + kLeftRightPaddingDx;
        pos.val.y = offset + currY;
        currY += pos.val.dy + kTxtPaddingDy;
    }

    auto dx = totalDx + 2 * offset;
    auto dy = totalDy + offset;

    // calc size and pos of buttons
    dy += offset;

    if (layoutData->btnGetFonts) {
        auto sz = layoutData->btnGetFonts->GetIdealSize();
        Rect rc{offset, dy, sz.dx, sz.dy};
        layoutData->btnGetFonts->SetBounds(rc);
    }

    {
        auto sz = layoutData->btnCopyToClipboard->GetIdealSize();
        int x = dx - offset - sz.dx;
        Rect rc{x, dy, sz.dx, sz.dy};
        layoutData->btnCopyToClipboard->SetBounds(rc);
        dy += sz.dy;
    }

    dy += offset;
    auto rect = Rect(0, 0, dx, dy);
    return rect;
}

static void ShowExtendedProperties(PropertiesLayout* pl) {
    MainWindow* win = FindMainWindowByHwnd(pl ? pl->hwndParent : nullptr);
    if (win && !pl->HasProperty(_TRA("Fonts:"))) {
        DestroyWindow(pl->hwnd);
        ShowProperties(win->hwndFrame, win->ctrl, true);
    }
}

static void CopyPropertiesToClipboard(PropertiesLayout* layoutData) {
    if (!layoutData) {
        return;
    }

    // concatenate all the properties into a multi-line string
    str::Str lines(256);
    int nProps = layoutData->PropCount();
    for (int i = 0; i < nProps; i++) {
        auto key = layoutData->PropKey(i);
        auto val = layoutData->PropValue(i);
        lines.AppendFmt("%s %s\r\n", key, val);
    }

    CopyTextToClipboard(lines.LendData());
}

static bool gDidRegister = false;
static bool CreatePropertiesWindow(HWND hParent, PropertiesLayout* layoutData, bool extended) {
    HMODULE h = GetModuleHandleW(nullptr);
    if (!gDidRegister) {
        WNDCLASSEX wcex = {};
        FillWndClassEx(wcex, kPropertiesWinClassName, WndProcProperties);
        WCHAR* iconName = MAKEINTRESOURCEW(GetAppIconID());
        wcex.hIcon = LoadIconW(h, iconName);
        ReportIf(!wcex.hIcon);
        ATOM atom = RegisterClassEx(&wcex);
        ReportIf(!atom);
        gDidRegister = true;
    }

    ReportIf(layoutData->hwnd);
    DWORD dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    auto clsName = kPropertiesWinClassName;
    auto title = ToWStrTemp(_TRA("Document Properties"));
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    int dx = CW_USEDEFAULT;
    int dy = CW_USEDEFAULT;
    HWND hwnd = CreateWindowExW(0, clsName, title, dwStyle, x, y, dx, dy, nullptr, nullptr, h, nullptr);
    if (!hwnd) {
        return false;
    }

    layoutData->hwnd = hwnd;
    layoutData->hwndParent = hParent;
    bool isRtl = IsUIRtl();
    HwndSetRtl(hwnd, isRtl);
    {
        Button::CreateArgs args;
        args.parent = hwnd;
        args.text = _TRA("Copy To Clipboard");

        auto b = new Button();
        b->Create(args);

        layoutData->btnCopyToClipboard = b;
        HwndSetRtl(b->hwnd, isRtl);
        b->onClick = MkFunc0(CopyPropertiesToClipboard, layoutData);
    }

    if (!extended) {
        Button::CreateArgs args;
        args.parent = hwnd;
        args.text = _TRA("Get Fonts Info");

        auto b = new Button();
        b->Create(args);

        HwndSetRtl(b->hwnd, isRtl);
        layoutData->btnGetFonts = b;
        b->onClick = MkFunc0(ShowExtendedProperties, layoutData);
    }

    // get the dimensions required for the about box's content
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    auto rc = CalcPropertiesLayout(layoutData, hdc);
    EndPaint(hwnd, &ps);

    // resize the new window to just match these dimensions
    // (as long as they fit into the current monitor's work area)
    Rect wRc = WindowRect(hwnd);
    Rect cRc = ClientRect(hwnd);
    Rect work = GetWorkAreaRect(WindowRect(hParent), hwnd);
    wRc.dx = std::min(rc.dx + wRc.dx - cRc.dx, work.dx);
    wRc.dy = std::min(rc.dy + wRc.dy - cRc.dy, work.dy);
    MoveWindow(hwnd, wRc.x, wRc.y, wRc.dx, wRc.dy, FALSE);
    CenterDialog(hwnd, hParent);
    if (gUseDarkModeLib) {
        DarkMode::setDarkWndSafe(hwnd);
        DarkMode::setWindowEraseBgSubclass(hwnd);
    }
    ShowWindow(hwnd, SW_SHOW);
    return true;
}

// clang-format off
static const char* propToName[] = {
    kPropTitle, _TRN("Title:"),
    kPropSubject, _TRN("Subject:"),
    kPropAuthor, _TRN("Author:"),
    kPropCopyright, _TRN("Copyright:"),
    kPropCreatorApp, _TRN("Application:"),
    kPropPdfProducer, _TRN("PDF Producer:"),
    kPropPdfVersion, _TRN("PDF Version:"),
    nullptr,
};
// clang-format on

static void AddPropTranslated(PropertiesLayout* layoutData, const char* propName, const char* val) {
    const char* s = GetMatchingString(propToName, propName);
    ReportIf(!s);
    const char* trans = trans::GetTranslation(s);
    layoutData->AddProperty(trans, val);
}

static void AddPropTranslated(DocController* ctrl, PropertiesLayout* layoutData, const char* propName) {
    TempStr val = ctrl->GetPropertyTemp(propName);
    AddPropTranslated(layoutData, propName, val);
}

static void AddPdfFileStructure(DocController* ctrl, PropertiesLayout* layoutData) {
    TempStr fstruct = ctrl->GetPropertyTemp(kPropPdfFileStructure);
    if (str::IsEmpty(fstruct)) {
        bool isPDF = str::EndsWithI(ctrl->GetFilePath(), ".pdf");
        if (isPDF) {
            layoutData->AddProperty(_TRA("Fast Web View"), _TRA("No"));
        }
        return;
    }
    StrVec parts;
    Split(&parts, fstruct, ",", true);

    StrVec props;

    const char* linearized = _TRA("No");
    if (parts.Contains("linearized")) {
        linearized = _TRA("Yes");
    }
    layoutData->AddProperty(_TRA("Fast Web View"), linearized);

    if (parts.Contains("tagged")) {
        props.Append(_TRA("Tagged PDF"));
    }
    if (parts.Contains("PDFX")) {
        props.Append("PDF/X (ISO 15930)");
    }
    if (parts.Contains("PDFA1")) {
        props.Append("PDF/A (ISO 19005)");
    }
    if (parts.Contains("PDFE1")) {
        props.Append("PDF/E (ISO 24517)");
    }

    TempStr val = JoinTemp(&props, ", ");
    layoutData->AddProperty(_TRA("PDF Optimizations:"), val);
}

// https://www.compart.com/en/unicode/U+202A
constexpr const char* leftToRightEmbeding = "\xe2\x80\xaa";
// https://www.compart.com/en/unicode/U+202c
constexpr const char* popDirectionalFormatting = "\xe2\x80\xac";

static void GetProps(DocController* ctrl, PropertiesLayout* layoutData, bool extended) {
    ReportIf(!ctrl);

    const char* path = gPluginMode ? gPluginURL : ctrl->GetFilePath();
    layoutData->AddProperty(_TRA("File:"), path, true);

    DisplayModel* dm = ctrl->AsFixed();
    i64 fileSize = file::GetSize(path); // can be gPluginURL
    if (-1 == fileSize && dm) {
        EngineBase* engine = dm->GetEngine();
        ByteSlice d = engine->GetFileData();
        if (!d.empty()) {
            fileSize = d.size();
        }
        d.Free();
    }
    TempStr strTemp;
    if (-1 != fileSize) {
        strTemp = FormatFileSizeTransTemp(fileSize);
        layoutData->AddProperty(_TRA("File Size:"), strTemp);
    }

    AddPropTranslated(ctrl, layoutData, kPropTitle);
    AddPropTranslated(ctrl, layoutData, kPropSubject);
    AddPropTranslated(ctrl, layoutData, kPropAuthor);
    AddPropTranslated(ctrl, layoutData, kPropCopyright);

    TempStr val = ctrl->GetPropertyTemp(kPropCreationDate);
    if (val && dm && kindEngineMupdf == dm->engineType) {
        strTemp = ConvDateToDisplayTemp(val, PdfDateParseA);
    } else {
        strTemp = ConvDateToDisplayTemp(val, IsoDateParse);
    }
    layoutData->AddProperty(_TRA("Created:"), strTemp);

    val = ctrl->GetPropertyTemp(kPropModificationDate);
    if (val && dm && kindEngineMupdf == dm->engineType) {
        strTemp = ConvDateToDisplayTemp(val, PdfDateParseA);
    } else {
        strTemp = ConvDateToDisplayTemp(val, IsoDateParse);
    }
    layoutData->AddProperty(_TRA("Modified:"), strTemp);

    AddPropTranslated(ctrl, layoutData, kPropCreatorApp);
    AddPropTranslated(ctrl, layoutData, kPropPdfProducer);
    AddPropTranslated(ctrl, layoutData, kPropPdfVersion);

    AddPdfFileStructure(ctrl, layoutData);

    strTemp = str::FormatTemp("%d", ctrl->PageCount());
    layoutData->AddProperty(_TRA("Number of Pages:"), strTemp);

    if (dm) {
        strTemp = FormatPageSizeTemp(dm->GetEngine(), ctrl->CurrentPageNo(), dm->GetRotation());
        if (IsUIRtl() && IsWindowsVistaOrGreater()) {
            // ensure that the size remains ungarbled left-to-right
            // (note: XP doesn't know about \u202A...\u202C)
            strTemp = str::JoinTemp(leftToRightEmbeding, strTemp, popDirectionalFormatting);
        }
        layoutData->AddProperty(_TRA("Page Size:"), strTemp);
    }

    strTemp = FormatPermissionsTemp(ctrl);
    layoutData->AddProperty(_TRA("Denied Permissions:"), strTemp);

    if (extended) {
        // Note: FontList extraction can take a while
        val = ctrl->GetPropertyTemp(kPropFontList);
        if (val) {
            // add a space between basic and extended file properties
            layoutData->AddProperty(" ", " ");
        }
        layoutData->AddProperty(_TRA("Fonts:"), val);
    }
}

void ShowProperties(HWND parent, DocController* ctrl, bool extended) {
    PropertiesLayout* layoutData = FindPropertyWindowByHwnd(parent);
    if (layoutData) {
        SetActiveWindow(layoutData->hwnd);
        return;
    }

    if (!ctrl) {
        return;
    }
    layoutData = new PropertiesLayout();
    gPropertiesWindows.Append(layoutData);
    GetProps(ctrl, layoutData, extended);

    if (!CreatePropertiesWindow(parent, layoutData, extended)) {
        delete layoutData;
    }
}

static void DrawProperties(HWND hwnd, HDC hdc) {
    PropertiesLayout* layoutData = FindPropertyWindowByHwnd(hwnd);

    HFONT fontLeftTxt = CreateSimpleFont(hdc, kLeftTextFont, kLeftTextFontSize);
    HFONT fontRightTxt = CreateSimpleFont(hdc, kRightTextFont, kRightTextFontSize);

    HGDIOBJ origFont = SelectObject(hdc, fontLeftTxt); /* Just to remember the orig font */

    SetBkMode(hdc, TRANSPARENT);

    Rect rcClient = ClientRect(hwnd);
    auto col = ThemeMainWindowBackgroundColor();
    FillRect(hdc, rcClient, col);

    col = ThemeWindowTextColor();
    SetTextColor(hdc, col);

    /* render text on the left*/
    SelectObject(hdc, fontLeftTxt);
    int nProps = layoutData->PropCount();
    for (int i = 0; i < nProps; i++) {
        auto& pos = layoutData->GetPos(i);
        auto txt = layoutData->PropKey(i);
        HdcDrawText(hdc, txt, pos.key, DT_RIGHT | DT_NOPREFIX);
    }

    /* render text on the right */
    SelectObject(hdc, fontRightTxt);
    for (int i = 0; i < nProps; i++) {
        auto& pos = layoutData->GetPos(i);
        auto txt = layoutData->PropValue(i);
        Rect rc = pos.val;
        if (rc.x + rc.dx > rcClient.x + rcClient.dx - kRectPadding) {
            rc.dx = rcClient.x + rcClient.dx - kRectPadding - rc.x;
        }
        uint format = DT_LEFT | DT_NOPREFIX | (pos.isPath ? DT_PATH_ELLIPSIS : DT_WORD_ELLIPSIS);
        HdcDrawText(hdc, txt, rc, format);
    }

    SelectObject(hdc, origFont);
}

static void OnPaintProperties(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    // CalcPropertiesLayout(FindPropertyWindowByHwnd(hwnd), hdc);
    DrawProperties(hwnd, hdc);
    EndPaint(hwnd, &ps);
}

static void PropertiesOnCommand(HWND hwnd, WPARAM wp) {
    auto cmd = LOWORD(wp);
    PropertiesLayout* pl = FindPropertyWindowByHwnd(hwnd);
    switch (cmd) {
        case CmdCopySelection:
            CopyPropertiesToClipboard(pl);
            break;

        case CmdProperties:
            // make a repeated Ctrl+D display some extended properties
            ShowExtendedProperties(pl);
            break;
    }
}

LRESULT CALLBACK WndProcProperties(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    PropertiesLayout* pl;

    LRESULT res = 0;
    res = TryReflectMessages(hwnd, msg, wp, lp);
    if (res != 0) {
        return res;
    }

    switch (msg) {
        case WM_CREATE:
            break;

        case WM_ERASEBKGND:
            // do nothing, helps to avoid flicker
            return TRUE;

        case WM_PAINT:
            OnPaintProperties(hwnd);
            break;

        case WM_CHAR:
            if (VK_ESCAPE == wp) {
                DestroyWindow(hwnd);
            }
            break;

        case WM_DESTROY:
            pl = FindPropertyWindowByHwnd(hwnd);
            ReportIf(!pl);
            gPropertiesWindows.Remove(pl);
            delete pl;
            break;

        case WM_COMMAND:
            PropertiesOnCommand(hwnd, wp);
            break;

        /* TODO: handle mouse move/down/up so that links work (?) */
        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }
    return 0;
}
