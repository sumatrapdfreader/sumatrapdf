/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/StrFormat.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/WinUtil.h"
#include "utils/ThreadUtil.h"
#include "utils/UITask.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Settings.h"
#include "GlobalPrefs.h"
#include "AppSettings.h"
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

void ShowProperties(HWND parent, DocController* ctrl);

constexpr const WCHAR* kPropertiesWinClassName = L"SUMATRA_PDF_PROPERTIES";

constexpr int kButtonAreaDy = 40;
constexpr int kButtonPadding = 8;

struct PropertiesLayout {
    HWND hwnd = nullptr;
    HWND hwndParent = nullptr;
    HWND hwndEdit = nullptr;
    Button* btnCopyToClipboard = nullptr;
    str::Str propsText;
    Point initialPos;

    PropertiesLayout() = default;
    ~PropertiesLayout() { delete btnCopyToClipboard; }
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
static bool PdfDateParseA(const char* date, SYSTEMTIME* timeOut, int* timeZoneOut) {
    if (!date || !*date) return false;

    ZeroMemory(timeOut, sizeof(SYSTEMTIME));
    *timeZoneOut = 0;

    // "D:" at the beginning is optional
    if (str::StartsWith(date, "D:")) {
        date += 2;
    }
    const char* end = str::Parse(date,
                                 "%4d%2d%2d"
                                 "%2d%2d%2d",
                                 &timeOut->wYear, &timeOut->wMonth, &timeOut->wDay, &timeOut->wHour, &timeOut->wMinute,
                                 &timeOut->wSecond);
    if (!end) {
        return false;
    }
    // parse optional timezone: Z, +HH'MM', -HH'MM' (or +HH'MM, +HHMM, +HH)
    if (*end == 'Z') {
        *timeZoneOut = 0;
    } else if (*end == '+' || *end == '-') {
        int sign = (*end == '+') ? 1 : -1;
        int tzHour = 0;
        int tzMin = 0;
        const char* tz = end + 1;
        const char* tzEnd = str::Parse(tz, "%2d'%2d", &tzHour, &tzMin);
        if (!tzEnd) {
            tzEnd = str::Parse(tz, "%2d:%2d", &tzHour, &tzMin);
        }
        if (!tzEnd) {
            str::Parse(tz, "%2d", &tzHour);
        }
        *timeZoneOut = sign * (tzHour * 100 + tzMin);
    }
    return true;
    // don't bother about the day of week, we won't display it anyway
}

// See: ISO 8601 specification
// Format:  "YYYY-MM-DDTHH:MM:SSZ"
// Example: "2011-04-19T22:10:48Z"
static bool IsoDateParse(const char* date, SYSTEMTIME* timeOut, int* timeZoneOut) {
    if (!date || !*date) return false;

    ZeroMemory(timeOut, sizeof(SYSTEMTIME));
    *timeZoneOut = 0;

    const char* end = str::Parse(date, "%4d-%2d-%2d", &timeOut->wYear, &timeOut->wMonth, &timeOut->wDay);
    if (end) { // time is optional
        const char* timeEnd = str::Parse(end, "T%2d:%2d:%2d", &timeOut->wHour, &timeOut->wMinute, &timeOut->wSecond);
        if (timeEnd) {
            // parse optional timezone: Z, +HH:MM, -HH:MM
            if (*timeEnd == 'Z') {
                *timeZoneOut = 0;
            } else if (*timeEnd == '+' || *timeEnd == '-') {
                int sign = (*timeEnd == '+') ? 1 : -1;
                int tzHour = 0;
                int tzMin = 0;
                const char* tz = timeEnd + 1;
                const char* tzEnd = str::Parse(tz, "%2d:%2d", &tzHour, &tzMin);
                if (!tzEnd) {
                    str::Parse(tz, "%2d%2d", &tzHour, &tzMin);
                }
                *timeZoneOut = sign * (tzHour * 100 + tzMin);
            }
        }
    }
    return end != nullptr;
    // don't bother about the day of week, we won't display it anyway
}

static TempStr AddTimeZone(TempStr s, int timeZone) {
    if (timeZone == 0) return nullptr;

    const char* tzSign = (timeZone > 0) ? "+" : "-";
    int abs = (timeZone > 0) ? timeZone : -timeZone;
    int hours = abs / 100;
    int mins = abs % 100;
    return str::FormatTemp("%s %s%02d:%02d", s, tzSign, hours, mins);
}

static TempStr FormatSystemTimeTemp(SYSTEMTIME& date, int timeZone) {
    WCHAR bufW[512]{};
    int cchBufLen = dimof(bufW);
    int ret = GetDateFormatW(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &date, nullptr, bufW, cchBufLen);
    if (ret < 2) { // GetDateFormat() failed or returned an empty result
        return nullptr;
    }

    // don't add 00:00:00 for dates without time
    if (0 == date.wHour && 0 == date.wMinute && 0 == date.wSecond) {
        TempStr res = ToUtf8Temp(bufW);
        return AddTimeZone(res, timeZone);
    }

    WCHAR* tmp = bufW + ret;
    tmp[-1] = ' ';
    ret = GetTimeFormatW(LOCALE_USER_DEFAULT, 0, &date, nullptr, tmp, cchBufLen - ret);
    if (ret < 2) { // GetTimeFormat() failed or returned an empty result
        tmp[-1] = '\0';
    }
    TempStr res = ToUtf8Temp(bufW);
    return AddTimeZone(res, timeZone);
}

// Convert a date in PDF or XPS format, e.g. "D:20091222171933-05'00'" to a display
// format e.g. "12/22/2009 5:19:33 PM"
// See: http://www.verypdf.com/pdfinfoeditor/pdf-date-format.htm
// The conversion happens in place
// static TempStr ConvDateToDisplayTemp(SYSTEMTIME* timeOut) {
//    return
//}

// format page size according to locale (e.g. "29.7 x 21.0 cm" or "11.69 x 8.27 in")
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

static void AppendProp(str::Str& out, const char* key, const char* value) {
    if (str::IsEmpty(value)) {
        return;
    }
    out.AppendFmt("%s %s\n", key, value);
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
    kPropFiles, _TRN("Files:"),
    kPropKeywords, _TRN("Keywords:"),
    kPropEncryption, _TRN("Encryption:"),
    kPropImageSize, _TRN("Image Size:"),
    kPropDpi, _TRN("DPI:"),
    kPropComment, _TRN("Comment:"),
    kPropCameraMake, _TRN("Camera Make:"),
    kPropCameraModel, _TRN("Camera Model:"),
    kPropDateOriginal, _TRN("Date Original:"),
    kPropExposureTime, _TRN("Exposure Time:"),
    kPropFNumber, _TRN("F-Number:"),
    kPropIsoSpeed, _TRN("ISO Speed:"),
    kPropFocalLength, _TRN("Focal Length:"),
    kPropFocalLength35mm, _TRN("Focal Length (35mm):"),
    kPropFlash, _TRN("Flash:"),
    kPropOrientation, _TRN("Orientation:"),
    kPropExposureProgram, _TRN("Exposure Program:"),
    kPropMeteringMode, _TRN("Metering Mode:"),
    kPropWhiteBalance, _TRN("White Balance:"),
    kPropExposureBias, _TRN("Exposure Bias:"),
    kPropBitsPerSample, _TRN("Bits Per Sample:"),
    nullptr,
};
// clang-format on

static void AppendPropTranslated(str::Str& out, const char* propName, const char* val) {
    if (!val) return;
    const char* s = GetMatchingString(propToName, propName);
    ReportIf(!s);
    const char* trans = trans::GetTranslation(s);
    AppendProp(out, trans, val);
}

static void AppendPdfFileStructure(str::Str& out, const char* fstruct, const char* filePath) {
    if (str::IsEmpty(fstruct)) {
        bool isPDF = str::EndsWithI(filePath, ".pdf");
        if (isPDF) {
            AppendProp(out, str::JoinTemp(_TRA("Fast Web View"), ":"), _TRA("No"));
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
    AppendProp(out, str::JoinTemp(_TRA("Fast Web View"), ":"), linearized);

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
    AppendProp(out, _TRA("PDF Optimizations:"), val);
}

static void GetAllProps(DocController* ctrl, Props& propsOut) {
    DisplayModel* dm = ctrl->AsFixed();
    if (dm) {
        EngineBase* engine = dm->GetEngine();
        engine->GetProperties(propsOut);
    } else {
        for (int i = 0; gAllProps[i]; i++) {
            TempStr val = ctrl->GetPropertyTemp(gAllProps[i]);
            propsOut.Append(gAllProps[i]);
            propsOut.Append(val);
        }
    }
}

void AppendDateProp(str::Str& out, const char* key, const char* val, bool isPdfDate) {
    SYSTEMTIME date;
    int timeZone = 0;
    bool ok = false;
    if (!val) return;
    if (isPdfDate) {
        ok = PdfDateParseA(val, &date, &timeZone);
    } else {
        ok = IsoDateParse(val, &date, &timeZone);
    }
    if (!ok) return;
    TempStr dateStr = FormatSystemTimeTemp(date, timeZone);
    AppendProp(out, key, dateStr);
}

static void GetPropsText(DocController* ctrl, str::Str& out) {
    ReportIf(!ctrl);

    const char* path = gPluginMode ? gPluginURL : ctrl->GetFilePath();
    if (!path) {
        path = "unknown";
    }
    AppendProp(out, _TRA("File:"), path);

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
        AppendProp(out, _TRA("File Size:"), strTemp);
    }

    Props props;
    GetAllProps(ctrl, props);

    AppendPropTranslated(out, kPropTitle, GetPropValueTemp(props, kPropTitle));
    AppendPropTranslated(out, kPropSubject, GetPropValueTemp(props, kPropSubject));
    AppendPropTranslated(out, kPropAuthor, GetPropValueTemp(props, kPropAuthor));
    AppendPropTranslated(out, kPropCopyright, GetPropValueTemp(props, kPropCopyright));

    bool isPdfDate = dm && kindEngineMupdf == dm->engineType;
    char* val = GetPropValueTemp(props, kPropCreationDate);
    AppendDateProp(out, _TRA("Created:"), val, isPdfDate);
    val = GetPropValueTemp(props, kPropModificationDate);
    AppendDateProp(out, _TRA("Modified:"), val, isPdfDate);

    AppendPropTranslated(out, kPropCreatorApp, GetPropValueTemp(props, kPropCreatorApp));
    AppendPropTranslated(out, kPropPdfProducer, GetPropValueTemp(props, kPropPdfProducer));
    AppendPropTranslated(out, kPropPdfVersion, GetPropValueTemp(props, kPropPdfVersion));
    strTemp = FormatPermissionsTemp(ctrl);
    AppendProp(out, _TRA("Denied Permissions:"), strTemp);

    AppendPdfFileStructure(out, GetPropValueTemp(props, kPropPdfFileStructure), ctrl->GetFilePath());

    strTemp = str::FormatTemp("%d", ctrl->PageCount());
    AppendProp(out, _TRA("Number of Pages:"), strTemp);

    if (dm) {
        strTemp = FormatPageSizeTemp(dm->GetEngine(), ctrl->CurrentPageNo(), dm->GetRotation());
        AppendProp(out, _TRA("Page Size:"), strTemp);
    }
    AppendPropTranslated(out, kPropFiles, GetPropValueTemp(props, kPropFiles));

    // clang-format off
    // properties already shown above, skip when appending remaining
    static const char* handledProps[] = {
        kPropTitle, kPropSubject, kPropAuthor, kPropCopyright,
        kPropCreationDate, kPropModificationDate,
        kPropCreatorApp, kPropPdfProducer, kPropPdfVersion,
        kPropPdfFileStructure, kPropFiles,
        kPropUnsupportedFeatures, kPropFontList,
        nullptr,
    };
    // clang-format on

    // append any remaining properties not already shown
    int nProps = PropsCount(props);
    for (int i = 0; i < nProps; i++) {
        char* propName = props.At(i * 2);
        char* propVal = props.At(i * 2 + 1);
        if (str::IsEmpty(propVal)) {
            continue;
        }
        bool handled = false;
        for (int j = 0; handledProps[j]; j++) {
            if (str::Eq(propName, handledProps[j])) {
                handled = true;
                break;
            }
        }
        if (handled) {
            continue;
        }
        const char* s = GetMatchingString(propToName, propName);
        if (s) {
            const char* trans = trans::GetTranslation(s);
            AppendProp(out, trans, propVal);
        } else {
            TempStr label = str::FormatTemp("%s:", propName);
            AppendProp(out, label, propVal);
        }
    }
}

static void SetEditText(HWND hwndEdit, const char* text) {
    // edit control needs \r\n line endings
    str::Str crlfText;
    for (const char* s = text; *s; s++) {
        if (*s == '\n' && (s == text || *(s - 1) != '\r')) {
            crlfText.AppendChar('\r');
        }
        crlfText.AppendChar(*s);
    }
    HwndSetText(hwndEdit, crlfText.CStr());
    SendMessageW(hwndEdit, EM_SETSEL, 0, 0);
}

static void CopyPropertiesToClipboard(PropertiesLayout* pl) {
    if (!pl) {
        return;
    }
    CopyTextToClipboard(pl->propsText.CStr());
}

static void SizeToContent(PropertiesLayout* pl) {
    HWND hwnd = pl->hwnd;
    HWND hwndEdit = pl->hwndEdit;

    HFONT font = (HFONT)SendMessageW(hwndEdit, WM_GETFONT, 0, 0);
    HDC hdcEdit = GetDC(hwndEdit);
    HGDIOBJ origFont = SelectObject(hdcEdit, font);
    int maxLineDx = 0;
    int nLines = 0;
    const char* text = pl->propsText.CStr();
    while (*text) {
        const char* nl = str::FindChar(text, '\n');
        int lineLen = nl ? (int)(nl - text) : str::Leni(text);
        SIZE sz{};
        TempWStr lineW = ToWStrTemp(text, (size_t)lineLen);
        GetTextExtentPoint32W(hdcEdit, lineW, str::Leni(lineW), &sz);
        if (sz.cx > maxLineDx) {
            maxLineDx = sz.cx;
        }
        nLines++;
        text = nl ? nl + 1 : text + lineLen;
    }
    maxLineDx += 16;

    TEXTMETRICW tm{};
    GetTextMetricsW(hdcEdit, &tm);
    int lineHeight = tm.tmHeight + tm.tmExternalLeading;

    SelectObject(hdcEdit, origFont);
    ReleaseDC(hwndEdit, hdcEdit);

    // add padding for scrollbar, border, window frame
    int editPadding = GetSystemMetrics(SM_CXVSCROLL) + 2 * GetSystemMetrics(SM_CXEDGE) + 16;
    int frameDx = GetSystemMetrics(SM_CXFRAME) * 2;
    int wantedClientDx = maxLineDx + editPadding;
    int wantedDx = wantedClientDx + frameDx;

    // calculate height to fit all lines
    int editBorderDy = 2 * GetSystemMetrics(SM_CYEDGE);
    int frameDy = GetSystemMetrics(SM_CYFRAME) * 2 + GetSystemMetrics(SM_CYCAPTION);
    int wantedDy = (nLines + 3) * lineHeight + editBorderDy + kButtonAreaDy + frameDy;

    // cap at 80% of screen
    Rect work = GetWorkAreaRect(WindowRect(hwnd), hwnd);
    int maxDx = (work.dx * 80) / 100;
    int maxDy = (work.dy * 80) / 100;
    wantedDx = std::min(wantedDx, maxDx);
    wantedDy = std::min(wantedDy, maxDy);

    Rect wRc = WindowRect(hwnd);
    MoveWindow(hwnd, wRc.x, wRc.y, wantedDx, wantedDy, TRUE);
}

static void LayoutButtons(PropertiesLayout* pl) {
    Rect cRc = ClientRect(pl->hwnd);
    int btnY = cRc.dy - kButtonAreaDy + kButtonPadding;

    if (pl->btnCopyToClipboard) {
        auto sz = pl->btnCopyToClipboard->GetIdealSize();
        int x = cRc.dx - kButtonPadding - sz.dx;
        Rect rc{x, btnY, sz.dx, sz.dy};
        pl->btnCopyToClipboard->SetBounds(rc);
    }
}

LRESULT CALLBACK WndProcProperties(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

static WNDPROC DefWndProcPropertiesEdit = nullptr;

static LRESULT CALLBACK WndProcPropertiesEdit(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (WM_CHAR == msg && VK_ESCAPE == wp) {
        DestroyWindow(GetParent(hwnd));
        return 0;
    }
    return CallWindowProc(DefWndProcPropertiesEdit, hwnd, msg, wp, lp);
}

static void PropertiesOnCommand(HWND hwnd, WPARAM wp) {
    auto cmd = LOWORD(wp);
    PropertiesLayout* pl = FindPropertyWindowByHwnd(hwnd);
    switch (cmd) {
        case CmdCopySelection:
            CopyPropertiesToClipboard(pl);
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

        case WM_SIZE:
            pl = FindPropertyWindowByHwnd(hwnd);
            if (pl && pl->hwndEdit) {
                int dx = LOWORD(lp);
                int dy = HIWORD(lp);
                int editDy = dy - kButtonAreaDy;
                MoveWindow(pl->hwndEdit, 0, 0, dx, editDy, TRUE);
                LayoutButtons(pl);
                RECT rc = {0, editDy, dx, dy};
                InvalidateRect(hwnd, &rc, TRUE);
            }
            return 0;

        case WM_CHAR:
            if (VK_ESCAPE == wp) {
                DestroyWindow(hwnd);
            }
            break;

        case WM_DESTROY:
            pl = FindPropertyWindowByHwnd(hwnd);
            ReportIf(!pl);
            if (pl) {
                Rect rc = WindowRect(hwnd);
                Point pos = {rc.x, rc.y};
                if (pos != pl->initialPos) {
                    gGlobalPrefs->propWinPos = pos;
                    SaveSettings();
                }
            }
            gPropertiesWindows.Remove(pl);
            delete pl;
            break;

        case WM_COMMAND:
            PropertiesOnCommand(hwnd, wp);
            break;

        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }
    return 0;
}

struct GetFontsResult {
    HWND hwnd;
    str::Str fontsText;
};

static void OnGetFontsFinished(GetFontsResult* result) {
    PropertiesLayout* pl = FindPropertyWindowByHwnd(result->hwnd);
    if (pl) {
        // remove "Getting fonts information..." line
        const char* marker = _TRA("Getting fonts information...");
        const char* found = str::Find(pl->propsText.CStr(), marker);
        if (found) {
            int pos = (int)(found - pl->propsText.CStr());
            if (pos > 0 && pl->propsText.CStr()[pos - 1] == '\n') {
                pos--;
            }
            pl->propsText.RemoveAt(pos, pl->propsText.Size() - pos);
        }
        pl->propsText.Append(result->fontsText.CStr());
        SetEditText(pl->hwndEdit, pl->propsText.CStr());
        SizeToContent(pl);
    }
    delete result;
}

struct GetFontsData {
    HWND hwnd;
    DocController* ctrl;
};

static void GetFontsThread(GetFontsData* data) {
    TempStr val = data->ctrl->GetPropertyTemp(kPropFontList);
    auto result = new GetFontsResult;
    result->hwnd = data->hwnd;
    if (val) {
        result->fontsText.Append("\n");
        result->fontsText.Append(_TRA("Fonts:"));
        result->fontsText.Append("\n");
        result->fontsText.Append(val);
    }
    auto fn = MkFunc0<GetFontsResult>(OnGetFontsFinished, result);
    uitask::Post(fn, "GetFontsFinished");
    delete data;
    DestroyTempAllocator();
}

void ShowProperties(HWND parent, DocController* ctrl) {
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
    GetPropsText(ctrl, layoutData->propsText);
    layoutData->propsText.Append("\n");
    layoutData->propsText.Append(_TRA("Getting fonts information..."));

    HMODULE h = GetModuleHandleW(nullptr);
    WNDCLASSEX wcex = {};
    FillWndClassEx(wcex, kPropertiesWinClassName, WndProcProperties);
    wcex.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    WCHAR* iconName = MAKEINTRESOURCEW(GetAppIconID());
    wcex.hIcon = LoadIconW(h, iconName);
    ReportIf(!wcex.hIcon);
    RegisterClassEx(&wcex);

    DWORD dwStyle = WS_OVERLAPPEDWINDOW;
    auto title = ToWStrTemp(_TRA("Document Properties"));
    HWND hwnd = CreateWindowExW(0, kPropertiesWinClassName, title, dwStyle, CW_USEDEFAULT, CW_USEDEFAULT, 500, 400,
                                nullptr, nullptr, h, nullptr);
    if (!hwnd) {
        gPropertiesWindows.Remove(layoutData);
        delete layoutData;
        return;
    }

    layoutData->hwnd = hwnd;
    layoutData->hwndParent = parent;

    bool isRtl = IsUIRtl();
    HwndSetRtl(hwnd, isRtl);

    // create the edit control
    Rect cRc = ClientRect(hwnd);
    int editDy = cRc.dy - kButtonAreaDy;
    DWORD editStyle =
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL;
    HWND hwndEdit =
        CreateWindowExW(WS_EX_CLIENTEDGE, WC_EDITW, L"", editStyle, 0, 0, cRc.dx, editDy, hwnd, nullptr, h, nullptr);
    layoutData->hwndEdit = hwndEdit;

    if (!DefWndProcPropertiesEdit) {
        DefWndProcPropertiesEdit = (WNDPROC)GetWindowLongPtr(hwndEdit, GWLP_WNDPROC);
    }
    SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)WndProcPropertiesEdit);

    HDC hdc = GetDC(hwnd);
    HFONT font = CreateSimpleFont(hdc, "Consolas", 14);
    ReleaseDC(hwnd, hdc);
    if (font) {
        SendMessageW(hwndEdit, WM_SETFONT, (WPARAM)font, TRUE);
    }

    SetEditText(hwndEdit, layoutData->propsText.CStr());

    SizeToContent(layoutData);

    // create buttons
    {
        Button::CreateArgs args;
        args.parent = hwnd;
        args.text = _TRA("Copy To Clipboard");
        args.isRtl = isRtl;

        auto b = new Button();
        b->Create(args);
        layoutData->btnCopyToClipboard = b;
        b->onClick = MkFunc0(CopyPropertiesToClipboard, layoutData);
    }

    LayoutButtons(layoutData);

    Point savedPos = gGlobalPrefs->propWinPos;
    if (!savedPos.IsEmpty()) {
        SetWindowPos(hwnd, nullptr, savedPos.x, savedPos.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
    } else {
        CenterDialog(hwnd, parent);
    }
    HwndEnsureVisible(hwnd);
    {
        Rect rc = WindowRect(hwnd);
        layoutData->initialPos = {rc.x, rc.y};
    }
    if (UseDarkModeLib()) {
        DarkMode::setDarkWndSafe(hwnd);
        DarkMode::setWindowEraseBgSubclass(hwnd);
    }
    ShowWindow(hwnd, SW_SHOW);

    // start background font loading
    auto data = new GetFontsData;
    data->hwnd = hwnd;
    data->ctrl = ctrl;
    auto fn = MkFunc0<GetFontsData>(GetFontsThread, data);
    RunAsync(fn, "GetFontsThread");
}
