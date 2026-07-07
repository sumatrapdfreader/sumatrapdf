/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/Win.h"
#include "base/Dpi.h"
#include "base/UITask.h"

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
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "Commands.h"
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
    str::Builder propsText;
    Point initialPos;

    PropertiesLayout() = default;
    ~PropertiesLayout() { delete btnCopyToClipboard; }
};

static Vec<PropertiesLayout*> gPropertiesWindows;

static int ButtonPadding(HWND hwnd) {
    return DpiScale(hwnd, kButtonPadding);
}

static int ButtonAreaDy(PropertiesLayout* pl) {
    int padding = ButtonPadding(pl->hwnd);
    int buttonAreaDy = DpiScale(pl->hwnd, kButtonAreaDy);
    if (pl->btnCopyToClipboard) {
        Size buttonSize = pl->btnCopyToClipboard->GetIdealSize();
        buttonAreaDy = std::max(buttonAreaDy, buttonSize.dy + 2 * padding);
    }
    return buttonAreaDy;
}

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
static bool PdfDateParseA(Str date, SYSTEMTIME* timeOut, int* timeZoneOut) {
    if (!date) return false;

    ZeroMemory(timeOut, sizeof(SYSTEMTIME));
    *timeZoneOut = 0;

    Str slice = date;
    // "D:" at the beginning is optional
    if (str::StartsWith(slice, "D:")) {
        slice = Str(slice.s + 2, slice.len - 2);
    }
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    Str end = str::Parse(slice,
                         "%4d%2d%2d"
                         "%2d%2d%2d",
                         &year, &month, &day, &hour, &minute, &second);
    if (!end.s) {
        return false;
    }
    timeOut->wYear = (WORD)year;
    timeOut->wMonth = (WORD)month;
    timeOut->wDay = (WORD)day;
    timeOut->wHour = (WORD)hour;
    timeOut->wMinute = (WORD)minute;
    timeOut->wSecond = (WORD)second;
    // parse optional timezone: Z, +HH'MM', -HH'MM' (or +HH'MM, +HHMM, +HH)
    if (end.s[0] == 'Z') {
        *timeZoneOut = 0;
    } else if (end.s[0] == '+' || end.s[0] == '-') {
        int sign = (end.s[0] == '+') ? 1 : -1;
        int tzHour = 0;
        int tzMin = 0;
        Str tz = Str(end.s + 1, end.len - 1);
        Str tzEnd = str::Parse(tz, "%2d'%2d", &tzHour, &tzMin);
        if (!tzEnd.s) {
            tzEnd = str::Parse(tz, "%2d:%2d", &tzHour, &tzMin);
        }
        if (!tzEnd.s) {
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
static bool IsoDateParse(Str date, SYSTEMTIME* timeOut, int* timeZoneOut) {
    if (!date) return false;

    ZeroMemory(timeOut, sizeof(SYSTEMTIME));
    *timeZoneOut = 0;

    int year = 0, month = 0, day = 0;
    Str end = str::Parse(date, "%4d-%2d-%2d", &year, &month, &day);
    if (end.s) { // time is optional
        timeOut->wYear = (WORD)year;
        timeOut->wMonth = (WORD)month;
        timeOut->wDay = (WORD)day;
        int hour = 0, minute = 0, second = 0;
        Str timeEnd = str::Parse(end, "T%2d:%2d:%2d", &hour, &minute, &second);
        if (timeEnd.s) {
            timeOut->wHour = (WORD)hour;
            timeOut->wMinute = (WORD)minute;
            timeOut->wSecond = (WORD)second;
            // parse optional timezone: Z, +HH:MM, -HH:MM
            if (timeEnd.s[0] == 'Z') {
                *timeZoneOut = 0;
            } else if (timeEnd.s[0] == '+' || timeEnd.s[0] == '-') {
                int sign = (timeEnd.s[0] == '+') ? 1 : -1;
                int tzHour = 0;
                int tzMin = 0;
                Str tz = Str(timeEnd.s + 1, timeEnd.len - 1);
                Str tzEnd = str::Parse(tz, "%2d:%2d", &tzHour, &tzMin);
                if (!tzEnd.s) {
                    str::Parse(tz, "%2d%2d", &tzHour, &tzMin);
                }
                *timeZoneOut = sign * (tzHour * 100 + tzMin);
            }
        }
    }
    return end.s != nullptr;
    // don't bother about the day of week, we won't display it anyway
}

static TempStr AddTimeZone(TempStr s, int timeZone) {
    if (timeZone == 0) return {};

    Str tzSign = (timeZone > 0) ? StrL("+") : StrL("-");
    int abs = (timeZone > 0) ? timeZone : -timeZone;
    int hours = abs / 100;
    int mins = abs % 100;
    return fmt("%s %s%02d:%02d", s, tzSign, hours, mins);
}

static TempStr FormatSystemTimeTemp(SYSTEMTIME& date, int timeZone) {
    WCHAR bufW[512]{};
    int cchBufLen = dimof(bufW);
    int ret = GetDateFormatW(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &date, nullptr, bufW, cchBufLen);
    if (ret < 2) { // GetDateFormat() failed or returned an empty result
        return {};
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

    Str formatName;
    switch (GetPaperFormatFromSizeApprox(size)) {
        case PaperFormat::A2:
            formatName = StrL(" (A2)");
            break;
        case PaperFormat::A3:
            formatName = StrL(" (A3)");
            break;
        case PaperFormat::A4:
            formatName = StrL(" (A4)");
            break;
        case PaperFormat::A5:
            formatName = StrL(" (A5)");
            break;
        case PaperFormat::A6:
            formatName = StrL(" (A6)");
            break;
        case PaperFormat::Letter:
            formatName = StrL(" (Letter)");
            break;
        case PaperFormat::Legal:
            formatName = StrL(" (Legal)");
            break;
        case PaperFormat::Tabloid:
            formatName = StrL(" (Tabloid)");
            break;
        case PaperFormat::Statement:
            formatName = StrL(" (Statement)");
            break;
    }

    bool isMetric = GetMeasurementSystem() == 0;
    double unitsPerInch = isMetric ? 2.54 : 1.0;
    Str unit = isMetric ? StrL("cm") : StrL("in");

    double width = size.dx * unitsPerInch;
    double height = size.dy * unitsPerInch;
    if (((int)(width * 100)) % 100 == 99) {
        width += 0.01;
    }
    if (((int)(height * 100)) % 100 == 99) {
        height += 0.01;
    }

    TempStr strWidth = str::FormatFloatWithThousandSepTemp(width);
    TempStr strHeight = str::FormatFloatWithThousandSepTemp(height);

    return fmt("%s x %s %s%s", strWidth, strHeight, unit, formatName);
}

// returns a list of permissions denied by this document
static TempStr FormatPermissionsTemp(DocController* ctrl) {
    if (!ctrl->AsFixed()) {
        return {};
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

static void AppendProp(str::Builder& out, Str key, Str value) {
    if (!value) {
        return;
    }
    out.Append(fmt("%s %s\n", key, value));
}

// clang-format off
struct PropLabel {
    DocProp prop;
    Str label;
};

static const PropLabel propToName[] = {
    {DocProp::Title, _TRN("Title:")},
    {DocProp::Subject, _TRN("Subject:")},
    {DocProp::Author, _TRN("Author:")},
    {DocProp::Copyright, _TRN("Copyright:")},
    {DocProp::CreatorApp, _TRN("Application:")},
    {DocProp::PdfProducer, _TRN("PDF Producer:")},
    {DocProp::PdfVersion, _TRN("PDF Version:")},
    {DocProp::Files, _TRN("Files:")},
    {DocProp::Keywords, _TRN("Keywords:")},
    {DocProp::Encryption, _TRN("Encryption:")},
    {DocProp::ImageSize, _TRN("Image Size:")},
    {DocProp::Dpi, _TRN("DPI:")},
    {DocProp::Comment, _TRN("Comment:")},
    {DocProp::CameraMake, _TRN("Camera Make:")},
    {DocProp::CameraModel, _TRN("Camera Model:")},
    {DocProp::DateOriginal, _TRN("Date Original:")},
    {DocProp::ExposureTime, _TRN("Exposure Time:")},
    {DocProp::FNumber, _TRN("F-Number:")},
    {DocProp::IsoSpeed, _TRN("ISO Speed:")},
    {DocProp::FocalLength, _TRN("Focal Length:")},
    {DocProp::FocalLength35mm, _TRN("Focal Length (35mm):")},
    {DocProp::Flash, _TRN("Flash:")},
    {DocProp::Orientation, _TRN("Orientation:")},
    {DocProp::ExposureProgram, _TRN("Exposure Program:")},
    {DocProp::MeteringMode, _TRN("Metering Mode:")},
    {DocProp::WhiteBalance, _TRN("White Balance:")},
    {DocProp::ExposureBias, _TRN("Exposure Bias:")},
    {DocProp::BitsPerSample, _TRN("Bits Per Sample:")},
    {DocProp::ResolutionUnit, _TRN("Resolution Unit:")},
    {DocProp::Software, _TRN("Software:")},
    {DocProp::DateTime, _TRN("Date/Time:")},
    {DocProp::YCbCrPositioning, _TRN("YCbCr Positioning:")},
    {DocProp::ExifVersion, _TRN("Exif Version:")},
    {DocProp::DateTimeDigitized, _TRN("Date/Time Digitized:")},
    {DocProp::ComponentsConfig, _TRN("Components Configuration:")},
    {DocProp::CompressedBpp, _TRN("Compressed Bits/Pixel:")},
    {DocProp::MaxAperture, _TRN("Max Aperture:")},
    {DocProp::LightSource, _TRN("Light Source:")},
    {DocProp::UserComment, _TRN("User Comment:")},
    {DocProp::FlashpixVersion, _TRN("Flashpix Version:")},
    {DocProp::ColorSpace, _TRN("Color Space:")},
    {DocProp::PixelXDimension, _TRN("Pixel X Dimension:")},
    {DocProp::PixelYDimension, _TRN("Pixel Y Dimension:")},
    {DocProp::FileSource, _TRN("File Source:")},
    {DocProp::SceneType, _TRN("Scene Type:")},
    {DocProp::ImageFileSize, _TRN("Image File Size:")},
    {DocProp::ImagePath, _TRN("Path:")},
    {DocProp::None, {}},
};
// clang-format on

static void AppendPropTranslated(str::Builder& out, DocProp prop, Str val) {
    if (prop == DocProp::None || !val) return;
    if (prop == DocProp::ImageFileSize) {
        TempStr valFormatted = FormatFileSizeTransTemp(ParseInt64(val));
        AppendProp(out, _TRA("File Size:"), valFormatted);
        return;
    }
    Str s;
    for (int i = 0; propToName[i].prop != DocProp::None; i++) {
        if (propToName[i].prop == prop) {
            s = propToName[i].label;
            break;
        }
    }
    if (!s) {
        TempStr propName = PropNameTemp(prop);
        TempStr label = fmt("%s:", propName);
        AppendProp(out, label, val);
        return;
    }
    Str trans = trans::GetTranslation(s);
    AppendProp(out, trans, val);
}

static void AppendPdfFileStructure(str::Builder& out, Str fstruct, Str filePath) {
    if (len(fstruct) == 0) {
        bool isPDF = str::EndsWithI(filePath, ".pdf");
        if (isPDF) {
            AppendProp(out, str::JoinTemp(_TRA("Fast Web View"), StrL(":")), _TRA("No"));
        }
        return;
    }
    StrVec parts;
    Split(&parts, fstruct, ",", true);

    StrVec props;

    Str linearized = _TRA("No");
    if (parts.Contains("linearized")) {
        linearized = _TRA("Yes");
    }
    AppendProp(out, str::JoinTemp(_TRA("Fast Web View"), StrL(":")), linearized);

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
        for (int i = 0; gAllProps[i] != DocProp::None; i++) {
            DocProp prop = gAllProps[i];
            TempStr val = ctrl->GetPropertyTemp(prop);
            if (val) {
                AddProp(propsOut, prop, val);
            }
        }
    }
}

void AppendDateProp(str::Builder& out, Str key, Str val, bool isPdfDate) {
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

static void AddImageProperties(EngineBase* engine, int pageNo, str::Builder& out) {
    // for image engines, show EXIF properties for the current image
    ReportIf(!IsEngineImages(engine));
    Props imageProps;
    EngineImagesGetImageProperties(engine, pageNo, imageProps);
    int nImageProps = PropsCount(imageProps);
    if (nImageProps == 0) return;
    out.AppendChar('\n');
    TempStr header = fmt(_TRA("Current Image (%d):").s, pageNo);
    out.Append(header);
    out.AppendChar('\n');
    for (int i = 0; i < nImageProps; i++) {
        AppendPropTranslated(out, imageProps[i].prop, imageProps[i].val);
    }
}

static void GetPropsText(DocController* ctrl, str::Builder& out) {
    ReportIf(!ctrl);

    Str path = gPluginMode ? gPluginURL : Str(ctrl->GetFilePath());
    AppendProp(out, _TRA("File:"), len(path) == 0 ? StrL("(not available)") : path);

    DisplayModel* dm = ctrl->AsFixed();
    i64 fileSize = file::GetSize(path); // can be gPluginURL
    if (-1 == fileSize && dm) {
        EngineBase* engine = dm->GetEngine();
        Str d = engine->GetFileData();
        if (len(d) > 0) {
            fileSize = d.len;
        }
        str::Free(d);
    }
    TempStr strTemp;
    if (-1 != fileSize) {
        strTemp = FormatFileSizeTransTemp(fileSize);
        AppendProp(out, _TRA("File Size:"), strTemp);
    }

    Props props;
    GetAllProps(ctrl, props);

    AppendPropTranslated(out, DocProp::Title, GetPropValueTemp(props, DocProp::Title));
    AppendPropTranslated(out, DocProp::Subject, GetPropValueTemp(props, DocProp::Subject));
    AppendPropTranslated(out, DocProp::Author, GetPropValueTemp(props, DocProp::Author));
    AppendPropTranslated(out, DocProp::Copyright, GetPropValueTemp(props, DocProp::Copyright));

    bool isPdfDate = dm && kindEngineMupdf == dm->engineType;
    Str val = GetPropValueTemp(props, DocProp::CreationDate);
    AppendDateProp(out, _TRA("Created:"), val, isPdfDate);
    val = GetPropValueTemp(props, DocProp::ModificationDate);
    AppendDateProp(out, _TRA("Modified:"), val, isPdfDate);

    AppendPropTranslated(out, DocProp::CreatorApp, GetPropValueTemp(props, DocProp::CreatorApp));
    AppendPropTranslated(out, DocProp::PdfProducer, GetPropValueTemp(props, DocProp::PdfProducer));
    AppendPropTranslated(out, DocProp::PdfVersion, GetPropValueTemp(props, DocProp::PdfVersion));
    strTemp = FormatPermissionsTemp(ctrl);
    AppendProp(out, _TRA("Denied Permissions:"), strTemp);

    AppendPdfFileStructure(out, GetPropValueTemp(props, DocProp::PdfFileStructure), ctrl->GetFilePath());

    int pageNo = ctrl->CurrentPageNo();
    bool isImages = false;
    if (dm) {
        EngineBase* engine = dm->GetEngine();
        isImages = IsEngineImages(engine);
    }

    strTemp = fmt("%d", ctrl->PageCount());
    if (isImages) {
        AppendProp(out, _TRA("Number of Images:"), strTemp);
    } else {
        AppendProp(out, _TRA("Number of Pages:"), strTemp);
    }

    if (dm && !isImages) { // we show image size below
        strTemp = FormatPageSizeTemp(dm->GetEngine(), pageNo, dm->GetRotation());
        TempStr s = fmt(_TRA("Current Page (%d) Size:").s, pageNo);
        AppendProp(out, s, strTemp);
    }
    if (isImages) AddImageProperties(dm->GetEngine(), pageNo, out);

    // clang-format off
    // properties already shown above, skip when appending remaining
    static const DocProp handledProps[] = {
        DocProp::Title, DocProp::Subject, DocProp::Author, DocProp::Copyright,
        DocProp::CreationDate, DocProp::ModificationDate,
        DocProp::CreatorApp, DocProp::PdfProducer, DocProp::PdfVersion,
        DocProp::PdfFileStructure, DocProp::Files,
        DocProp::UnsupportedFeatures, DocProp::FontList,
        DocProp::None,
    };
    // clang-format on

    // append any remaining properties not already shown
    int nProps = PropsCount(props);
    for (int i = 0; i < nProps; i++) {
        DocProp prop = props[i].prop;
        Str propVal = props[i].val;
        if (!propVal) {
            continue;
        }
        bool handled = false;
        for (int j = 0; handledProps[j] != DocProp::None; j++) {
            if (prop == handledProps[j]) {
                handled = true;
                break;
            }
        }
        if (handled) {
            continue;
        }
        AppendPropTranslated(out, prop, propVal);
    }

    out.AppendChar('\n');
    AppendPropTranslated(out, DocProp::Files, GetPropValueTemp(props, DocProp::Files));
}

static int GetPropertyLabelWidth(Str line, int* labelBytesOut) {
    for (int i = 0; i + 2 < line.len; i++) {
        if (line.s[i] != ':' || line.s[i + 1] != ' ') {
            continue;
        }
        TempWStr label = ToWStrTemp(Str(line.s, i + 1));
        *labelBytesOut = i + 1;
        return len(label);
    }
    return -1;
}

static void AlignPropertiesText(str::Builder& text) {
    int maxLabelWidth = 0;
    Str content = ToStr(text);
    for (int off = 0; off < content.len;) {
        Str rest = Str(content.s + off, content.len - off);
        int nl = str::IndexOfChar(rest, '\n');
        int lineLen = nl >= 0 ? nl : rest.len;
        int labelBytes = 0;
        int labelWidth = GetPropertyLabelWidth(Str(rest.s, lineLen), &labelBytes);
        if (labelWidth > maxLabelWidth) {
            maxLabelWidth = labelWidth;
        }
        off += lineLen + (nl >= 0 ? 1 : 0);
    }
    if (maxLabelWidth == 0) {
        return;
    }

    str::Builder aligned;
    for (int off = 0; off < content.len;) {
        Str rest = Str(content.s + off, content.len - off);
        int nl = str::IndexOfChar(rest, '\n');
        int lineLen = nl >= 0 ? nl : rest.len;
        int labelBytes = 0;
        int labelWidth = GetPropertyLabelWidth(Str(rest.s, lineLen), &labelBytes);
        if (labelWidth >= 0) {
            int nSpacesBefore = maxLabelWidth - labelWidth;
            for (int i = 0; i < nSpacesBefore; i++) {
                aligned.AppendChar(' ');
            }
            aligned.Append(Str(rest.s, labelBytes));
            aligned.Append("  ");
            aligned.Append(Str(rest.s + labelBytes + 1, lineLen - labelBytes - 1));
        } else {
            aligned.Append(Str(rest.s, lineLen));
        }
        if (nl >= 0) {
            aligned.AppendChar('\n');
        }
        off += lineLen + (nl >= 0 ? 1 : 0);
    }
    text.Reset(ToStr(aligned));
}

static void SetEditText(HWND hwndEdit, Str text) {
    // edit control needs \r\n line endings
    str::Builder crlfText;
    for (int i = 0; i < text.len; i++) {
        char c = text.s[i];
        if (c == '\n' && (i == 0 || text.s[i - 1] != '\r')) {
            crlfText.AppendChar('\r');
        }
        crlfText.AppendChar(c);
    }
    HwndSetText(hwndEdit, ToStr(crlfText));
    SendMessageW(hwndEdit, EM_SETSEL, 0, 0);
}

static void CopyPropertiesToClipboard(PropertiesLayout* pl) {
    if (!pl) {
        return;
    }
    CopyTextToClipboard(ToStr(pl->propsText));
}

static void SizeToContent(PropertiesLayout* pl) {
    HWND hwnd = pl->hwnd;
    HWND hwndEdit = pl->hwndEdit;

    HFONT font = (HFONT)SendMessageW(hwndEdit, WM_GETFONT, 0, 0);
    HDC hdcEdit = GetDC(hwndEdit);
    HGDIOBJ origFont = SelectObject(hdcEdit, font);
    int maxLineDx = 0;
    int nLines = 0;
    Str text = ToStr(pl->propsText);
    for (int off = 0; off < text.len;) {
        Str rest = Str(text.s + off, text.len - off);
        int nl = str::IndexOfChar(rest, '\n');
        int lineLen = nl >= 0 ? nl : rest.len;
        SIZE sz{};
        TempWStr lineW = ToWStrTemp(Str(rest.s, lineLen));
        GetTextExtentPoint32W(hdcEdit, lineW.s, lineW.len, &sz);
        if (sz.cx > maxLineDx) {
            maxLineDx = sz.cx;
        }
        nLines++;
        off += lineLen + (nl >= 0 ? 1 : 0);
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
    if (pl->btnCopyToClipboard) {
        Size buttonSize = pl->btnCopyToClipboard->GetIdealSize();
        wantedClientDx = std::max(wantedClientDx, buttonSize.dx + 2 * ButtonPadding(hwnd));
    }
    int wantedDx = wantedClientDx + frameDx;

    // calculate height to fit all lines
    int editBorderDy = 2 * GetSystemMetrics(SM_CYEDGE);
    int frameDy = GetSystemMetrics(SM_CYFRAME) * 2 + GetSystemMetrics(SM_CYCAPTION);
    int wantedDy = (nLines + 3) * lineHeight + editBorderDy + ButtonAreaDy(pl) + frameDy;

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
    int padding = ButtonPadding(pl->hwnd);
    int btnY = cRc.dy - ButtonAreaDy(pl) + padding;

    if (pl->btnCopyToClipboard) {
        auto sz = pl->btnCopyToClipboard->GetIdealSize();
        int x = cRc.dx - padding - sz.dx;
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
                int editDy = dy - ButtonAreaDy(pl);
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
    str::Builder fontsText;
};

static void OnGetFontsFinished(GetFontsResult* result) {
    PropertiesLayout* pl = FindPropertyWindowByHwnd(result->hwnd);
    if (pl) {
        // remove "Getting fonts information..." line
        Str marker = _TRA("Getting fonts information...");
        Str props = ToStr(pl->propsText);
        int pos = str::IndexOf(props, marker);
        if (pos >= 0) {
            if (pos > 0 && props.s[pos - 1] == '\n') {
                pos--;
            }
            pl->propsText.RemoveAt(pos, len(pl->propsText) - pos);
        }
        pl->propsText.Append(ToStr(result->fontsText));
        SetEditText(pl->hwndEdit, ToStr(pl->propsText));
        SizeToContent(pl);
    }
    delete result;
}

struct GetFontsData {
    HWND hwnd;
    DocController* ctrl;
};

static void GetFontsThread(GetFontsData* data) {
    TempStr val = data->ctrl->GetPropertyTemp(DocProp::FontList);
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
    DestroyTempArena();
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
    AlignPropertiesText(layoutData->propsText);
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
    WCHAR* title = CWStrTemp(_TRA("Document Properties"));
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
    int editDy = cRc.dy - DpiScale(hwnd, kButtonAreaDy);
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

    SetEditText(hwndEdit, ToStr(layoutData->propsText));

    SizeToContent(layoutData);
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
