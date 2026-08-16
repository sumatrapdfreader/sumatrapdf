/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/Win.h"
#include "gui/Dpi.h"
#include "base/UITask.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"

#include "Settings.h"
#include "GlobalPrefs.h"
#include "AppSettings.h"
#include "DocProperties.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
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
#include "DarkMode_win.h"

void ShowProperties(HWND parent, DocController* ctrl);

constexpr int kButtonPadding = 8;

struct PropertiesWnd : WindowBase {
    HWND hwndParent = nullptr;
    Edit* editProps = nullptr;
    VirtButton* btnCopyToClipboard = nullptr;
    PlatformFont* propsFont = nullptr;
    str::Builder propsText;
    Point initialPos;

    bool Create(HWND parent);
    void UpdateTheme() override;
    void ApplyDarkMode() override;
    void SetPropsText(Str text);
    void SizeToContent();
    void CopyToClipboard(VirtMouseEvent* ev = nullptr);
    void OnCommand(WindowBase::CommandEvent* ev);
};

static Vec<PropertiesWnd*> gPropertiesWindows;

static int ButtonPadding() {
    return DpiScale(kButtonPadding);
}

PropertiesWnd* FindPropertyWindowByHwnd(HWND hwnd) {
    for (PropertiesWnd* w : gPropertiesWindows) {
        if (w->hwnd == hwnd) {
            return w;
        }
        if (w->hwndParent == hwnd) {
            return w;
        }
    }
    return nullptr;
}

static void SavePropertiesWindowPos(PropertiesWnd* w, HWND hwnd);

void DeletePropertiesWindow(HWND hwndParent) {
    PropertiesWnd* w = FindPropertyWindowByHwnd(hwndParent);
    if (!w) {
        return;
    }
    // Unregister first so a second closer cannot find this instance and schedule
    // another delete (CloseDocumentInCurrentTab then DeleteMainWindow).
    gPropertiesWindows.Remove(w);
    if (w->hwnd && IsWindow(w->hwnd)) {
        SavePropertiesWindowPos(w, w->hwnd);
        // Destroy HWND now (sync). Do not use Close()/PostMessage: that left the
        // object findable until WM_CLOSE ran and allowed a double ScheduleDelete.
        w->Destroy();
    }
    w->ScheduleDelete();
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
    str::TrimPrefix(slice, StrL("D:"));
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
        *timeZoneOut = sign * ((tzHour * 100) + tzMin);
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
                *timeZoneOut = sign * ((tzHour * 100) + tzMin);
            }
        }
    }
    return end.s != nullptr;
    // don't bother about the day of week, we won't display it anyway
}

static TempStr AddTimeZone(TempStr s, int timeZone) {
    // timeZone 0 means UTC or unspecified: nothing to append, return the date as-is
    // (returning {} here would drop the whole formatted date, e.g. for "D:...Z" dates)
    if (timeZone == 0) return s;

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

// One "W x H unit" fragment for FormatPageSizeTemp (size.dx/dy are inches).
static TempStr FormatPageSizeUnitTemp(SizeF sizeInches, double unitsPerInch, Str unit) {
    double width = sizeInches.dx * unitsPerInch;
    double height = sizeInches.dy * unitsPerInch;
    if (((int)(width * 100)) % 100 == 99) {
        width += 0.01;
    }
    if (((int)(height * 100)) % 100 == 99) {
        height += 0.01;
    }
    TempStr strWidth = str::FormatFloatWithThousandSepTemp(width);
    TempStr strHeight = str::FormatFloatWithThousandSepTemp(height);
    return fmt("%sx%s %s", strWidth, strHeight, unit);
}

// Format page size in cm/mm/in (locale unit first) and pixels (issue #2186).
// Metric: "21.0 x 29.7 cm, 210 x 297 mm, 8.27 x 11.69 in, 595 x 842 px (A4)"
// US:     "8.27 x 11.69 in, 21.0 x 29.7 cm, 210 x 297 mm, 595 x 842 px (A4)"
static TempStr FormatPageSizeTemp(EngineBase* engine, int pageNo, int rotation) {
    RectF mediabox = engine->PageMediabox(pageNo);
    float fileDpi = engine->GetFileDPI();
    float zoom = 1.0f / fileDpi;
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

    TempStr inStr = FormatPageSizeUnitTemp(size, 1.0, StrL("in"));
    TempStr cmStr = FormatPageSizeUnitTemp(size, 2.54, StrL("cm"));
    TempStr mmStr = FormatPageSizeUnitTemp(size, 25.4, StrL("mm"));

    // Pixel size at the document's native DPI (PDF media box is typically 72 dpi)
    int pxW = (int)lroundf(size.dx * fileDpi);
    int pxH = (int)lroundf(size.dy * fileDpi);
    TempStr pxStr = fmt("%dx%d px", pxW, pxH);

    // Locale unit first, then the other two, then pixels (issue #2186)
    bool isMetric = GetMeasurementSystem() == 0;
    if (isMetric) {
        return fmt("%s, %s, %s, %s%s", cmStr, mmStr, inStr, pxStr, formatName);
    }
    return fmt("%s, %s, %s, %s%s", inStr, cmStr, mmStr, pxStr, formatName);
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
    if (len(value) == 0) {
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
    if (len(s) > 0) {
        // found a display label (e.g. "Application:"); show its translation
        Str trans = trans::GetTranslation(s);
        AppendProp(out, trans, val);
        return;
    }
    // no display label: fall back to the raw property name
    TempStr propName = PropNameTemp(prop);
    TempStr label = fmt("%s:", propName);
    AppendProp(out, label, val);
}

static void AppendPdfFileStructure(str::Builder& out, Str fstruct, Str filePath) {
    if (len(fstruct) == 0) {
        bool isPDF = str::EndsWithI(filePath, StrL(".pdf"));
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
        return;
    }
    for (int i = 0; gAllProps[i] != DocProp::None; i++) {
        DocProp prop = gAllProps[i];
        TempStr val = ctrl->GetPropertyTemp(prop);
        if (val) {
            AddProp(propsOut, prop, val);
        }
    }
}

static void AppendDateProp(str::Builder& out, Str key, Str val, bool isPdfDate) {
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

// Types whose name is more than an upper-cased extension. Everything else
// reads fine as-is (".pdf" -> "PDF"), so only the exceptions live here.
static Str FileTypeDisplayName(FileType ft) {
    if (ft == FileType::Jxl) {
        return StrL("JPEG-XL");
    }
    return {};
}

// The file type as sniffed from the content, which is what the app went by
// when it picked an engine - the extension can say something else entirely.
// Only for a plain file on disk: a directory document has no content of its
// own, an embedded PDF stream isn't a file, and in plugin mode the "path" is
// a URL we can't read.
static void AppendFileType(str::Builder& out, Str path) {
    if (gPluginMode || len(path) == 0) {
        return;
    }
    if (path::IsDirectory(path) || !file::Exists(path)) {
        return;
    }
    FileType ft = GuessFileTypeFromFile(path);
    if (ft == FileType::Unknown) {
        return;
    }
    Str name = FileTypeDisplayName(ft);
    if (!name) {
        TempStr ext = GetExtForFileTypeTemp(ft);
        if (len(ext) < 2) {
            return;
        }
        // ".pdf" reads better as "PDF" next to the file name it belongs to
        TempStr fromExt = str::DupTemp(Str(ext.s + 1, ext.len - 1));
        str::ToUpperInPlace(fromExt);
        name = fromExt;
    }
    AppendProp(out, _TRA("File Type:"), name);
}

// Which way the pages run, and where that came from. An EPUB can state it with
// page-progression-direction; for everything else it's the manga-mode default
// or the user's own toggle (issue #1264).
// Not shown for a standalone image (no page sequence) or a one-page document
// that neither declares a direction nor has had manga mode toggled (#5950).
static bool ShouldShowReadingDirection(DisplayModel* dm) {
    if (!dm) {
        return false;
    }
    EngineBase* engine = dm->GetEngine();
    if (!engine || engine->kind == kindEngineImage) {
        return false;
    }
    const PageLayout& layout = engine->preferredLayout;
    if (layout.r2lDeclared) {
        return true;
    }
    if (dm->GetDisplayR2L() != layout.r2l) {
        return true;
    }
    return dm->PageCount() > 1;
}

static void AppendReadingDirection(str::Builder& out, DisplayModel* dm) {
    if (!ShouldShowReadingDirection(dm)) {
        return;
    }
    bool r2l = dm->GetDisplayR2L();
    Str dir = r2l ? _TRA("Right to left") : _TRA("Left to right");
    Str src;
    const PageLayout& layout = dm->GetEngine()->preferredLayout;
    if (layout.r2lDeclared && r2l == layout.r2l) {
        src = _TRA("from the document");
    } else if (r2l != layout.r2l) {
        src = _TRA("changed by you");
    }
    TempStr val = src ? str::JoinTemp(dir, StrL(" ("), src, StrL(")")) : TempStr(dir);
    AppendProp(out, _TRA("Reading Direction:"), val);
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
    AppendFileType(out, path);
    AppendReadingDirection(out, dm);

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

// make text end with exactly one '\n' (if not empty) so that appending
// a section preceded by "\n" yields a single empty line, not more.
// the last property is optional (e.g. "Files:") so text can end with
// a stray blank line
static void EndWithSingleNewline(str::Builder& b) {
    while (len(b) > 0 && b.LastChar() == '\n') {
        b.RemoveLast();
    }
    if (len(b) > 0) {
        b.AppendChar('\n');
    }
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
        maxLabelWidth = std::max(labelWidth, maxLabelWidth);
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

void PropertiesWnd::CopyToClipboard(VirtMouseEvent*) {
    CopyTextToClipboard(ToStr(propsText));
}

void PropertiesWnd::SetPropsText(Str text) {
    if (!editProps) {
        return;
    }
    str::Builder crlfText;
    for (int i = 0; i < text.len; i++) {
        char c = text.s[i];
        if (c == '\n' && (i == 0 || text.s[i - 1] != '\r')) {
            crlfText.AppendChar('\r');
        }
        crlfText.AppendChar(c);
    }
    editProps->SetText(ToStr(crlfText));
    SendMessageW(editProps->hwnd, EM_SETSEL, 0, 0);
}

void PropertiesWnd::SizeToContent() {
    if (!editProps) {
        return;
    }
    int maxLineDx = 0;
    int nLines = 0;
    Str text = ToStr(propsText);
    for (int off = 0; off < text.len;) {
        Str rest = Str(text.s + off, text.len - off);
        int nl = str::IndexOfChar(rest, '\n');
        int lineLen = nl >= 0 ? nl : rest.len;
        Size size = PlatformFontMeasureText(propsFont, Str(rest.s, lineLen));
        maxLineDx = std::max(size.dx, maxLineDx);
        nLines++;
        off += lineLen + (nl >= 0 ? 1 : 0);
    }
    maxLineDx += 16;

    int lineHeight = PlatformFontLineHeight(propsFont);
    // a bit of slack so the longest lines don't touch the right edge
    maxLineDx += 4 * PlatformFontMeasureText(propsFont, "x").dx;

    // add padding for scrollbar, border, window frame
    int editPadding = DpiGetSystemMetrics(SM_CXVSCROLL) + (2 * DpiGetSystemMetrics(SM_CXEDGE)) + 16;
    int frameDx = DpiGetSystemMetrics(SM_CXFRAME) * 2;
    int wantedClientDx = maxLineDx + editPadding;
    if (btnCopyToClipboard) {
        Size buttonSize = btnCopyToClipboard->GetIdealSize();
        wantedClientDx = std::max(wantedClientDx, buttonSize.dx + (2 * ButtonPadding()));
    }
    int wantedDx = wantedClientDx + frameDx;

    // calculate height to fit all lines
    int editBorderDy = 2 * DpiGetSystemMetrics(SM_CYEDGE);
    int frameDy = (DpiGetSystemMetrics(SM_CYFRAME) * 2) + DpiGetSystemMetrics(SM_CYCAPTION);
    int btnAreaDy = DpiScale(40);
    if (btnCopyToClipboard) {
        btnAreaDy = std::max(btnAreaDy, btnCopyToClipboard->GetIdealSize().dy + (2 * ButtonPadding()));
    }
    int bottomMargin = ButtonPadding();
    int wantedDy = ((nLines + 3) * lineHeight) + editBorderDy + btnAreaDy + bottomMargin + frameDy;

    // cap at 80% of screen
    Rect work = GetWorkAreaRect(HwndWindowRect(hwnd), hwnd);
    int maxDx = (work.dx * 80) / 100;
    int maxDy = (work.dy * 80) / 100;
    wantedDx = std::min(wantedDx, maxDx);
    wantedDy = std::min(wantedDy, maxDy);

    Rect wRc = HwndWindowRect(hwnd);
    MoveWindow(hwnd, wRc.x, wRc.y, wantedDx, wantedDy, TRUE);
    DoLayout();
}

void PropertiesWnd::ApplyDarkMode() {
    DarkModeApplyToWindowAndEraseBg(hwnd);
}

void PropertiesWnd::UpdateTheme() {
    WindowBase::UpdateTheme();
    // Re-apply monospaced font after darkmode child theming (may reset font).
    if (editProps && propsFont) {
        editProps->SetFont(propsFont);
    }
}

void PropertiesWnd::OnCommand(WindowBase::CommandEvent* ev) {
    auto cmd = LOWORD(ev->wparam);
    if (cmd == CmdCopySelection) {
        CopyToClipboard();
        ev->didHandle = true;
    }
}

static void SavePropertiesWindowPos(PropertiesWnd* w, HWND hwnd) {
    if (!w || !hwnd || !IsWindow(hwnd)) {
        return;
    }
    Rect rc = HwndWindowRect(hwnd);
    Point pos = {rc.x, rc.y};
    if (pos != w->initialPos) {
        gGlobalPrefs->propWinPos = pos;
        SaveSettings();
    }
}

// WM_CLOSE must clean up here: WindowBase::Destroy() clears hwnd and drops the WindowBase from
// the hwnd map before DestroyWindow(), so WM_DESTROY never reaches onDestroy.
static void OnPropertiesClose(WindowBase::CloseEvent* ev) {
    PropertiesWnd* w = (PropertiesWnd*)ev->e->self;
    if (!w || w->deleteScheduled) {
        return;
    }
    SavePropertiesWindowPos(w, w->hwnd);
    gPropertiesWindows.Remove(w);
    w->ScheduleDelete();
}

static void OnPropertiesDestroy(WindowBase::DestroyEvent* ev) {
    PropertiesWnd* w = (PropertiesWnd*)ev->e->self;
    // Fallback if the window was destroyed without WM_CLOSE (or Close already
    // scheduled the deferred free — then deleteScheduled is set and we no-op).
    if (!w || w->deleteScheduled) {
        return;
    }
    if (gPropertiesWindows.Find(w) >= 0) {
        SavePropertiesWindowPos(w, ev->e->hwnd);
        gPropertiesWindows.Remove(w);
    }
    w->ScheduleDelete();
}

bool PropertiesWnd::Create(HWND parent) {
    hwndParent = parent;
    bool isRtl = IsUIRtl();

    {
        CreateCustomArgs args;
        args.title = _TRA("Document Properties");
        args.visible = false;
        args.style = WS_OVERLAPPEDWINDOW;
        args.font = GetAppFont();
        args.isRtl = isRtl;
        args.icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(GetAppIconID()));
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }

    HDC hdc = GetDC(hwnd);
    propsFont = HdcCreateSimpleFont(hdc, "Consolas", 14);
    ReleaseDC(hwnd, hdc);

    auto* vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    {
        Edit::CreateArgs args;
        args.parent = hwnd;
        args.font = propsFont;
        args.isMultiLine = true;
        args.withBorder = true;
        args.isRtl = isRtl;
        editProps = new Edit();
        editProps->Create(args);
        SendMessageW(editProps->hwnd, EM_SETREADONLY, TRUE, 0);
        // multi-line edit default can still cap text; font lists may be large
        SendMessageW(editProps->hwnd, EM_SETLIMITTEXT, 0, 0);
        DWORD tabStop = 16;
        SendMessageW(editProps->hwnd, EM_SETTABSTOPS, 1, (LPARAM)&tabStop);
        vbox->AddChild(editProps, 1);
    }

    {
        auto* btnRow = new HBox();
        btnRow->alignMain = MainAxisAlign::MainEnd;
        btnRow->alignCross = CrossAxisAlign::CrossCenter;
        btnRow->gap = GetAppFont()->averageCharWidth;
        btnCopyToClipboard = NewThemedButton(hwnd, _TRA("Copy To Clipboard"), GetAppFont(), true);
        btnCopyToClipboard->onClick = MkMethod1<PropertiesWnd, VirtMouseEvent*, &PropertiesWnd::CopyToClipboard>(this);
        btnRow->AddChild(new Padding(btnCopyToClipboard, DpiScaledInsets(kButtonPadding, 0, 0, 0)));
        vbox->AddChild(btnRow);
    }

    layout = new Padding(vbox, DpiScaledInsets(0, kButtonPadding, kButtonPadding, kButtonPadding));

    SetPropsText(ToStr(propsText));
    SizeToContent();
    UpdateTheme();
    SetIsVisible(true);
    return true;
}

struct GetFontsResult {
    HWND hwnd;
    str::Builder fontsText;
};

static void OnGetFontsFinished(GetFontsResult* result) {
    PropertiesWnd* w = FindPropertyWindowByHwnd(result->hwnd);
    if (w) {
        Str marker = _TRA("Getting font information...");
        Str props = ToStr(w->propsText);
        int pos = str::IndexOf(props, marker);
        if (pos >= 0) {
            w->propsText.RemoveAt(pos, len(w->propsText) - pos);
        }
        // fontsText starts with "\n" to separate it with a single empty line
        EndWithSingleNewline(w->propsText);
        w->propsText.Append(ToStr(result->fontsText));
        w->SetPropsText(ToStr(w->propsText));
        w->SizeToContent();
    }
    delete result;
}

struct GetFontsData {
    HWND hwnd;
    EngineBase* engine;
};

static void GetFontsThread(GetFontsData* data) {
    TempStr val = data->engine->GetPropertyTemp(DocProp::FontList);
    auto* result = new GetFontsResult;
    result->hwnd = data->hwnd;
    if (val) {
        result->fontsText.Append("\n");
        result->fontsText.Append(_TRA("Fonts:"));
        result->fontsText.Append("\n");
        result->fontsText.Append(val);
    }
    auto fn = MkFunc0<GetFontsResult>(OnGetFontsFinished, result);
    uitask::Post(fn, "GetFontsFinished");
    data->engine->Release();
    delete data;
    DestroyTempArena();
}

void ShowProperties(HWND parent, DocController* ctrl) {
    PropertiesWnd* w = FindPropertyWindowByHwnd(parent);
    if (w) {
        SetActiveWindow(w->hwnd);
        return;
    }

    if (!ctrl) {
        return;
    }

    auto* wnd = new PropertiesWnd();
    gPropertiesWindows.Append(wnd);
    GetPropsText(ctrl, wnd->propsText);
    AlignPropertiesText(wnd->propsText);
    EndWithSingleNewline(wnd->propsText);
    wnd->propsText.Append("\n");
    wnd->propsText.Append(_TRA("Getting font information..."));

    wnd->closeOnEsc = true;
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnPropertiesClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnPropertiesDestroy);
    wnd->onCommand = MkMethod1<PropertiesWnd, WindowBase::CommandEvent*, &PropertiesWnd::OnCommand>(wnd);
    if (!wnd->Create(parent)) {
        gPropertiesWindows.Remove(wnd);
        delete wnd;
        return;
    }

    Point savedPos = gGlobalPrefs->propWinPos;
    if (!savedPos.IsEmpty()) {
        SetWindowPos(wnd->hwnd, nullptr, savedPos.x, savedPos.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
        wnd->DoLayout();
    } else {
        HwndCenterDialog(wnd->hwnd, parent);
    }
    HwndEnsureOnScreen(wnd->hwnd);
    {
        Rect rc = HwndWindowRect(wnd->hwnd);
        wnd->initialPos = {rc.x, rc.y};
    }

    DisplayModel* dm = ctrl->AsFixed();
    if (!dm || !dm->engine) {
        auto* result = new GetFontsResult;
        result->hwnd = wnd->hwnd;
        OnGetFontsFinished(result);
        return;
    }
    auto* data = new GetFontsData;
    data->hwnd = wnd->hwnd;
    data->engine = dm->engine;
    data->engine->AddRef();
    auto fn = MkFunc0<GetFontsData>(GetFontsThread, data);
    RunAsync(fn, "GetFontsThread");
}
