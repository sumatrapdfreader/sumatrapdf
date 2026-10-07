/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's properties window is a top-level HWND holding a read-only
// multi-line Edit in Consolas and a "Copy To Clipboard" button. The text it
// shows is built here exactly as orig builds it (same rows, same order, same
// alignment); the window is a gpui Dialog with a read-only monospaced editor.

#include "gui/GpuiBridge.h"
#if OS_WIN
#include "base/Win.h"
#endif
#include "VirtKeys.h"
#include "base/File.h"
#include "base/UITask.h"

#if !OS_WIN
#include <locale.h>
#endif

#include "gui/Dpi.h"
#include "gui/UIModels.h"
#if OS_WIN
#include "gui/PlatformFont.h"
#endif

#include "Settings.h"
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
#include "Theme.h"
#include "EutlTrust.h"
#include "Notifications.h"
#include "Print.h"
#include "gui/AppShell.h"
#include "gui/DialogWidgets.h"
#include "gui/ToolWindow.h"

#if OS_WIN
#include <wincrypt.h>
#endif
#include "DocumentProperties.h"

#include "SumatraLog.h"

// ng: orig's PropertiesWnd. There is one dialog per window, so the state is a
// single struct instead of orig's gPropertiesWindows list.
struct PropertiesDlg {
    MainWindow* win = nullptr;
    bool visible = false;
    str::Builder propsText;
    gp::InputState* textEdit = nullptr;
    // ng: the longest line and one "x" as gpui lays them out; filled by the
    // frame that drew them, so a text change takes one more frame to size
    gp::Bounds longestLine{};
    gp::Bounds oneChar{};
    bool remeasure = false;
    bool eutlUpdating = false;
    // orig's window, where the platform can have one (gui/ToolWindow.h);
    // null: a dialog in the frame
    ToolWindow* tw = nullptr;
    // where the window was first put: only a moved one is remembered
    Point initialPos;
    // the window was placed (its position counts as the user's from then on)
    bool toolSized = false;
    // orig's edit font as GDI measures it, in dips: Consolas 14 is 8 x 17
    float toolCharDx = 0;
    float toolLineDy = 0;
#if OS_WIN
    PdfSigCert* certs = nullptr;
    DialogSelect certSelect;
#endif
};

static PropertiesDlg gProps;

struct PropertiesView {
    static void OnCopy(PropertiesView* self, gp::Ctx* cx, const gp::ClickEvent*);
#if OS_WIN
    static void OnViewCert(PropertiesView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnUpdateEutl(PropertiesView* self, gp::Ctx* cx, const gp::ClickEvent*);
#endif
    static void OnClose(PropertiesView* self, gp::Ctx* cx, const gp::ClickEvent*);
};

static gp::Entity<PropertiesView> gPropertiesView;

// ng: orig parses dates into a win32 SYSTEMTIME. Only the six fields matter,
// and they have to exist off Windows too.
struct PropDate {
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
};

// See: http://www.verypdf.com/pdfinfoeditor/pdf-date-format.htm
// Format:  "D:YYYYMMDDHHMMSSxxxxxxx"
// Example: "D:20091222171933-05'00'"
static bool PdfDateParseA(Str date, PropDate* timeOut, int* timeZoneOut) {
    if (len(date) == 0) {
        return false;
    }

    *timeOut = PropDate();
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
    timeOut->year = year;
    timeOut->month = month;
    timeOut->day = day;
    timeOut->hour = hour;
    timeOut->minute = minute;
    timeOut->second = second;
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
static bool IsoDateParse(Str date, PropDate* timeOut, int* timeZoneOut) {
    if (len(date) == 0) {
        return false;
    }

    *timeOut = PropDate();
    *timeZoneOut = 0;

    int year = 0, month = 0, day = 0;
    Str end = str::Parse(date, "%4d-%2d-%2d", &year, &month, &day);
    if (end.s) { // time is optional
        timeOut->year = year;
        timeOut->month = month;
        timeOut->day = day;
        int hour = 0, minute = 0, second = 0;
        Str timeEnd = str::Parse(end, "T%2d:%2d:%2d", &hour, &minute, &second);
        if (timeEnd.s) {
            timeOut->hour = hour;
            timeOut->minute = minute;
            timeOut->second = second;
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
    if (timeZone == 0) {
        return s;
    }

    Str tzSign = (timeZone > 0) ? StrL("+") : StrL("-");
    int abs = (timeZone > 0) ? timeZone : -timeZone;
    int hours = abs / 100;
    int mins = abs % 100;
    return fmt("%s %s%02d:%02d", s, tzSign, hours, mins);
}

static TempStr FormatDateTemp(const PropDate& date, int timeZone) {
#if OS_WIN
    SYSTEMTIME st{};
    st.wYear = (WORD)date.year;
    st.wMonth = (WORD)date.month;
    st.wDay = (WORD)date.day;
    st.wHour = (WORD)date.hour;
    st.wMinute = (WORD)date.minute;
    st.wSecond = (WORD)date.second;
    WCHAR bufW[512]{};
    int cchBufLen = dimof(bufW);
    int ret = GetDateFormatW(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, nullptr, bufW, cchBufLen);
    if (ret < 2) { // GetDateFormat() failed or returned an empty result
        return {};
    }

    // don't add 00:00:00 for dates without time
    if (0 == date.hour && 0 == date.minute && 0 == date.second) {
        TempStr res = ToUtf8Temp(bufW);
        return AddTimeZone(res, timeZone);
    }

    WCHAR* tmp = bufW + ret;
    tmp[-1] = ' ';
    ret = GetTimeFormatW(LOCALE_USER_DEFAULT, 0, &st, nullptr, tmp, cchBufLen - ret);
    if (ret < 2) { // GetTimeFormat() failed or returned an empty result
        tmp[-1] = '\0';
    }
    TempStr res = ToUtf8Temp(bufW);
    return AddTimeZone(res, timeZone);
#else
    if (date.year == 0) {
        return {};
    }

    static const char* locale = setlocale(LC_TIME, "");
    (void)locale;
    struct tm tm{};
    tm.tm_year = date.year - 1900;
    tm.tm_mon = date.month - 1;
    tm.tm_mday = date.day;
    tm.tm_hour = date.hour;
    tm.tm_min = date.minute;
    tm.tm_sec = date.second;
    char buf[512]{};
    const char* format = (date.hour == 0 && date.minute == 0 && date.second == 0) ? "%x" : "%x %X";
    if (strftime(buf, dimof(buf), format, &tm) == 0) {
        return {};
    }
    return AddTimeZone(Str(buf), timeZone);
#endif
}

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

// Format page size in cm/mm/in (locale unit first) and points (issue #2186).
// Metric: "21.0 x 29.7 cm, 210 x 297 mm, 8.27 x 11.69 in, 595 x 842 pt (A4)"
// US:     "8.27 x 11.69 in, 21.0 x 29.7 cm, 210 x 297 mm, 595 x 842 pt (A4)"
static TempStr FormatPageSizeTemp(EngineBase* engine, int pageNo, int rotation) {
    RectF mediabox = engine->PageMediabox(pageNo);
    float fileDpi = engine->fileDPI;
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
        case PaperFormat::Other:
            formatName = StrL(" (Other)");
            break;
    }

    TempStr inStr = FormatPageSizeUnitTemp(size, 1.0, StrL("in"));
    TempStr cmStr = FormatPageSizeUnitTemp(size, 2.54, StrL("cm"));
    TempStr mmStr = FormatPageSizeUnitTemp(size, 25.4, StrL("mm"));

    int ptW = (int)lroundf(size.dx * 72.0f);
    int ptH = (int)lroundf(size.dy * 72.0f);
    TempStr ptStr = fmt("%dx%d pt", ptW, ptH);

    // Locale unit first, then the other two, then points (issue #2186)
    bool isMetric = GetMeasurementSystem() == 0;
    if (isMetric) {
        return fmt("%s, %s, %s, %s%s", cmStr, mmStr, inStr, ptStr, formatName);
    }
    return fmt("%s, %s, %s, %s%s", inStr, cmStr, mmStr, ptStr, formatName);
}

// returns a list of permissions denied by this document
static TempStr FormatPermissionsTemp(DocController* ctrl) {
    if (!ctrl->AsFixed()) {
        return {};
    }

    StrVec denials;

    EngineBase* engine = ctrl->AsFixed()->GetEngine();
    if (!engine->AllowsPrinting()) {
        denials.Append(Tr("printing document"));
    }
    if (!engine->allowsCopyingText) {
        denials.Append(Tr("copying text"));
    }

    return JoinTemp(&denials, StrL(", "));
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
    {DocProp::Title, TrN("Title:")},
    {DocProp::Subject, TrN("Subject:")},
    {DocProp::Author, TrN("Author:")},
    {DocProp::Copyright, TrN("Copyright:")},
    {DocProp::CreatorApp, TrN("Application:")},
    {DocProp::PdfProducer, TrN("PDF Producer:")},
    {DocProp::PdfVersion, TrN("PDF Version:")},
    {DocProp::Files, TrN("Files:")},
    {DocProp::Keywords, TrN("Keywords:")},
    {DocProp::Encryption, TrN("Encryption:")},
    {DocProp::Signatures, TrN("Signatures:")},
    {DocProp::ImageSize, TrN("Image Size:")},
    {DocProp::Dpi, TrN("DPI:")},
    {DocProp::Comment, TrN("Comment:")},
    {DocProp::CameraMake, TrN("Camera Make:")},
    {DocProp::CameraModel, TrN("Camera Model:")},
    {DocProp::DateOriginal, TrN("Date Original:")},
    {DocProp::ExposureTime, TrN("Exposure Time:")},
    {DocProp::FNumber, TrN("F-Number:")},
    {DocProp::IsoSpeed, TrN("ISO Speed:")},
    {DocProp::FocalLength, TrN("Focal Length:")},
    {DocProp::FocalLength35mm, TrN("Focal Length (35mm):")},
    {DocProp::Flash, TrN("Flash:")},
    {DocProp::Orientation, TrN("Orientation:")},
    {DocProp::ExposureProgram, TrN("Exposure Program:")},
    {DocProp::MeteringMode, TrN("Metering Mode:")},
    {DocProp::WhiteBalance, TrN("White Balance:")},
    {DocProp::ExposureBias, TrN("Exposure Bias:")},
    {DocProp::BitsPerSample, TrN("Bits Per Sample:")},
    {DocProp::ResolutionUnit, TrN("Resolution Unit:")},
    {DocProp::Software, TrN("Software:")},
    {DocProp::DateTime, TrN("Date/Time:")},
    {DocProp::YCbCrPositioning, TrN("YCbCr Positioning:")},
    {DocProp::ExifVersion, TrN("Exif Version:")},
    {DocProp::DateTimeDigitized, TrN("Date/Time Digitized:")},
    {DocProp::ComponentsConfig, TrN("Components Configuration:")},
    {DocProp::CompressedBpp, TrN("Compressed Bits/Pixel:")},
    {DocProp::MaxAperture, TrN("Max Aperture:")},
    {DocProp::LightSource, TrN("Light Source:")},
    {DocProp::UserComment, TrN("User Comment:")},
    {DocProp::FlashpixVersion, TrN("Flashpix Version:")},
    {DocProp::ColorSpace, TrN("Color Space:")},
    {DocProp::PixelXDimension, TrN("Pixel X Dimension:")},
    {DocProp::PixelYDimension, TrN("Pixel Y Dimension:")},
    {DocProp::FileSource, TrN("File Source:")},
    {DocProp::SceneType, TrN("Scene Type:")},
    {DocProp::ImageFileSize, TrN("Image File Size:")},
    {DocProp::ImagePath, TrN("Path:")},
    {DocProp::None, {}},
};
// clang-format on

static void AppendPropTranslated(str::Builder& out, DocProp prop, Str val) {
    if (prop == DocProp::None || len(val) == 0) {
        return;
    }
    if (prop == DocProp::ImageFileSize) {
        TempStr valFormatted = FormatFileSizeTransTemp(ParseInt64(val));
        AppendProp(out, Tr("File Size:"), valFormatted);
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
            AppendProp(out, str::JoinTemp(Tr("Fast Web View"), StrL(":")), Tr("No"));
        }
        return;
    }
    StrVec parts;
    Split(&parts, fstruct, StrL(","), true);

    StrVec props;

    Str linearized = Tr("No");
    if (parts.Contains(StrL("linearized"))) {
        linearized = Tr("Yes");
    }
    AppendProp(out, str::JoinTemp(Tr("Fast Web View"), StrL(":")), linearized);

    if (parts.Contains(StrL("tagged"))) {
        props.Append(Tr("Tagged PDF"));
    }
    if (parts.Contains(StrL("PDFX"))) {
        props.Append(StrL("PDF/X (ISO 15930)"));
    }
    if (parts.Contains(StrL("PDFA1"))) {
        props.Append(StrL("PDF/A (ISO 19005)"));
    }
    if (parts.Contains(StrL("PDFE1"))) {
        props.Append(StrL("PDF/E (ISO 24517)"));
    }

    TempStr val = JoinTemp(&props, StrL(", "));
    AppendProp(out, Tr("PDF Optimizations:"), val);
}

static void GetAllProps(DocController* ctrl, Props& propsOut) {
    DisplayModel* dm = ctrl->AsFixed();
    if (dm) {
        EngineBase* engine = dm->GetEngine();
        engine->GetProperties(propsOut);
        return;
    }
    for (DocProp prop : kCommonDocProps) {
        TempStr val = ctrl->GetPropertyTemp(prop);
        if (val) {
            AddProp(propsOut, prop, val);
        }
    }
}

static void AppendDateProp(str::Builder& out, Str key, Str val, bool isPdfDate) {
    PropDate date;
    int timeZone = 0;
    bool ok = false;
    if (len(val) == 0) {
        return;
    }
    if (isPdfDate) {
        ok = PdfDateParseA(val, &date, &timeZone);
    } else {
        ok = IsoDateParse(val, &date, &timeZone);
    }
    if (!ok) {
        return;
    }
    TempStr dateStr = FormatDateTemp(date, timeZone);
    AppendProp(out, key, dateStr);
}

static void AddImageProperties(EngineBase* engine, int pageNo, str::Builder& out) {
    // for image engines, show EXIF properties for the current image
    ReportIf(!IsEngineImages(engine));
    Props imageProps;
    EngineImagesGetImageProperties(engine, pageNo, imageProps);
    int nImageProps = len(imageProps);
    if (nImageProps == 0) {
        return;
    }
    out.AppendChar('\n');
    TempStr header = fmt(Tr("Current Image (%d):").s, pageNo);
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
    if (len(name) == 0) {
        TempStr ext = GetExtForFileTypeTemp(ft);
        if (len(ext) < 2) {
            return;
        }
        // ".pdf" reads better as "PDF" next to the file name it belongs to
        TempStr fromExt = str::DupTemp(Str(ext.s + 1, ext.len - 1));
        str::ToUpperInPlace(fromExt);
        name = fromExt;
    }
    AppendProp(out, Tr("File Type:"), name);
}

// Which way the pages run, and where that came from. A PDF can state it with
// ViewerPreferences /Direction; an EPUB with page-progression-direction.
// Otherwise it's the manga-mode default or the user's own toggle
// (issues #1264, #2022).
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
    Str dir = r2l ? Tr("Right to left") : Tr("Left to right");
    Str src;
    const PageLayout& layout = dm->GetEngine()->preferredLayout;
    if (layout.r2lDeclared && r2l == layout.r2l) {
        src = Tr("from the document");
    } else if (r2l != layout.r2l) {
        src = Tr("changed by you");
    }
    TempStr val = src ? str::JoinTemp(dir, StrL(" ("), src, StrL(")")) : TempStr(dir);
    AppendProp(out, Tr("Reading Direction:"), val);
}

static void GetPropsText(DocController* ctrl, str::Builder& out) {
    ReportIf(!ctrl);

    Str path = gPluginMode ? gPluginURL : Str(ctrl->GetFilePath());
    AppendProp(out, Tr("File:"), len(path) == 0 ? StrL("(not available)") : path);

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
        AppendProp(out, Tr("File Size:"), strTemp);
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
    AppendDateProp(out, Tr("Created:"), val, isPdfDate);
    val = GetPropValueTemp(props, DocProp::ModificationDate);
    AppendDateProp(out, Tr("Modified:"), val, isPdfDate);

    AppendPropTranslated(out, DocProp::CreatorApp, GetPropValueTemp(props, DocProp::CreatorApp));
    AppendPropTranslated(out, DocProp::PdfProducer, GetPropValueTemp(props, DocProp::PdfProducer));
    AppendPropTranslated(out, DocProp::PdfVersion, GetPropValueTemp(props, DocProp::PdfVersion));
    strTemp = FormatPermissionsTemp(ctrl);
    AppendProp(out, Tr("Denied Permissions:"), strTemp);

    AppendPdfFileStructure(out, GetPropValueTemp(props, DocProp::PdfFileStructure), ctrl->GetFilePath());

    int pageNo = ctrl->CurrentPageNo();
    bool isImages = false;
    if (dm) {
        EngineBase* engine = dm->GetEngine();
        isImages = IsEngineImages(engine);
    }

    strTemp = fmt("%d", ctrl->PageCount());
    if (isImages) {
        AppendProp(out, Tr("Number of Images:"), strTemp);
    } else {
        AppendProp(out, Tr("Number of Pages:"), strTemp);
    }

    if (dm && !isImages) { // we show image size below
        strTemp = FormatPageSizeTemp(dm->GetEngine(), pageNo, dm->GetRotation());
        TempStr s = fmt(Tr("Current Page (%d) Size:").s, pageNo);
        AppendProp(out, s, strTemp);
    }
    if (isImages) {
        AddImageProperties(dm->GetEngine(), pageNo, out);
    }

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
    int nProps = len(props);
    for (int i = 0; i < nProps; i++) {
        DocProp prop = props[i].prop;
        Str propVal = props[i].val;
        if (len(propVal) == 0) {
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
            aligned.Append(StrL("  "));
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

#if OS_WIN
static void ViewCertDer(HWND parent, Str der) {
    if (len(der) == 0) {
        return;
    }
    PCCERT_CONTEXT cert =
        CertCreateCertificateContext(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, (const BYTE*)der.s, (DWORD)len(der));
    if (!cert) {
        return;
    }
    using Fn = BOOL(WINAPI*)(DWORD, const void*, HWND, LPCWSTR, DWORD, void*);
    HMODULE dll = LoadLibraryW(L"cryptui.dll");
    Fn view = dll ? (Fn)GetProcAddress(dll, "CryptUIDlgViewContext") : nullptr;
    if (view) {
        view(CERT_STORE_CERTIFICATE_CONTEXT, cert, parent, L"Certificate", 0, nullptr);
    }
    if (dll) {
        FreeLibrary(dll);
    }
    CertFreeCertificateContext(cert);
}

static TempStr HexBytesTemp(const BYTE* p, int n, bool reverse) {
    if (!p || n <= 0) {
        return {};
    }
    char* buf = AllocArrayTemp<char>((n * 2) + 1);
    for (int i = 0; i < n; i++) {
        BYTE v = reverse ? p[n - 1 - i] : p[i];
        buf[i * 2] = "0123456789ABCDEF"[v >> 4];
        buf[(i * 2) + 1] = "0123456789ABCDEF"[v & 0xf];
    }
    return Str(buf, n * 2);
}

static TempStr CertNameTemp(PCCERT_CONTEXT cert, DWORD flags) {
    char buf[512];
    DWORD n = CertGetNameStringA(cert, CERT_NAME_SIMPLE_DISPLAY_TYPE, flags, nullptr, buf, dimof(buf));
    if (n <= 1) {
        return {};
    }
    return str::DupTemp(Str(buf));
}

static TempStr FileTimeLocalTemp(const FILETIME& ft) {
    FILETIME local{};
    SYSTEMTIME st{};
    if (!FileTimeToLocalFileTime(&ft, &local) || !FileTimeToSystemTime(&local, &st)) {
        return {};
    }
    PropDate d;
    d.year = st.wYear;
    d.month = st.wMonth;
    d.day = st.wDay;
    d.hour = st.wHour;
    d.minute = st.wMinute;
    d.second = st.wSecond;
    return FormatDateTemp(d, 0);
}

static void AppendOneCert(str::Builder& out, PdfSigCert* c) {
    out.Append(c->label);
    out.AppendChar('\n');
    if (len(c->der) == 0) {
        return;
    }
    PCCERT_CONTEXT cert = CertCreateCertificateContext(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, (const BYTE*)c->der.s,
                                                       (DWORD)len(c->der));
    if (!cert) {
        return;
    }
    AppendProp(out, Tr("Subject:"), CertNameTemp(cert, 0));
    AppendProp(out, Tr("Issuer:"), CertNameTemp(cert, CERT_NAME_ISSUER_FLAG));
    if (cert->pCertInfo) {
        AppendProp(out, Tr("Serial Number:"),
                   HexBytesTemp(cert->pCertInfo->SerialNumber.pbData, (int)cert->pCertInfo->SerialNumber.cbData, true));
        AppendProp(out, Tr("Valid From:"), FileTimeLocalTemp(cert->pCertInfo->NotBefore));
        AppendProp(out, Tr("Valid To:"), FileTimeLocalTemp(cert->pCertInfo->NotAfter));
    }
    BYTE hash[20];
    DWORD hashLen = sizeof(hash);
    if (CertGetCertificateContextProperty(cert, CERT_HASH_PROP_ID, hash, &hashLen) && hashLen > 0) {
        AppendProp(out, Tr("SHA-1:"), HexBytesTemp(hash, (int)hashLen, false));
    }
    Str trust = EutlCertIsEuTrusted((const u8*)c->der.s, len(c->der)) ? StrL("European Union Trusted List (EUTL)")
                                                                      : StrL("Windows Certificate Store");
    AppendProp(out, Tr("Trust:"), trust);
    CertFreeCertificateContext(cert);
}

static void AppendCertsText(str::Builder& out, PdfSigCert* certs) {
    if (!certs) {
        return;
    }
    out.AppendChar('\n');
    out.Append(Tr("Certificates:"));
    out.AppendChar('\n');
    for (PdfSigCert* c = certs; c; c = c->next) {
        AppendOneCert(out, c);
        if (c->next) {
            out.AppendChar('\n');
        }
    }
}
#endif

// --- the dialog -------------------------------------------------------------

// the dialog in the frame; a window of its own is not the frame's business
bool IsPropertiesDialogVisible() {
    return gProps.visible && !gProps.tw;
}

// orig's SavePropertiesWindowPos: only a window the user moved is remembered
static void SavePropertiesWindowPos() {
    Rect rc = ToolWindowRect(gProps.tw);
    Point pos = {rc.x, rc.y};
    if (rc.IsEmpty() || !gProps.toolSized || pos == gProps.initialPos) {
        return;
    }
    gSettings->propWinPos = pos;
    ScheduleSaveSettings();
}

void DeletePropertiesWindow(MainWindow* win) {
    if (!gProps.visible || (win && gProps.win != win)) {
        return;
    }
    gProps.visible = false;
    gp::Window* host = gProps.tw ? ToolWindowGpui(gProps.tw) : (gProps.win ? gProps.win->gpuiWin : nullptr);
    if (gProps.textEdit && host) {
        gp::InputBlur(gProps.textEdit, host->app, host);
    }
    if (gProps.tw) {
        SavePropertiesWindowPos();
        ToolWindowClose(gProps.tw);
        gProps.tw = nullptr;
    }
    delete gProps.textEdit;
    gProps.textEdit = nullptr;
    gProps.propsText.Reset();
#if OS_WIN
    gProps.certSelect.Free();
    FreePdfSigCerts(gProps.certs);
    gProps.certs = nullptr;
#endif
    AppShellInvalidate(gProps.win);
}

void PropertiesView::OnCopy(PropertiesView*, gp::Ctx* cx, const gp::ClickEvent*) {
    Str s = ToStr(gProps.propsText);
    gp::ClipboardSetText(cx->win, ToGpui(s));
    logf("DocumentProperties: copied %d bytes to the clipboard\n", len(s));
    ShowTemporaryNotification(gProps.win, Tr("Copied to clipboard"));
    gp::Notify(cx);
}

#if OS_WIN
void PropertiesView::OnViewCert(PropertiesView*, gp::Ctx* cx, const gp::ClickEvent*) {
    PdfSigCert* cert = gProps.certs;
    for (int i = 0; cert && i < gProps.certSelect.sel; i++) {
        cert = cert->next;
    }
    if (cert) {
        ViewCertDer((HWND)gp::PlatWindowHandle(cx->win), cert->der);
    }
}

struct EutlUpdateJob {
    MainWindow* win = nullptr;
    bool ok = false;
    Str msg;
};

static void OnEutlUpdateDone(EutlUpdateJob* job) {
    gProps.eutlUpdating = false;
    if (IsMainWindowValidAndNotClosing(job->win)) {
        if (job->ok) {
            ShowTemporaryNotification(job->win, job->msg, kNotif5SecsTimeOut);
        } else {
            ShowWarningNotification(job->win, job->msg, kNotif5SecsTimeOut);
        }
        AppShellInvalidate(job->win);
    }
    str::Free(job->msg);
    delete job;
}

static void EutlUpdateThread(EutlUpdateJob* job) {
    Str err;
    job->ok = EutlUpdate(&err);
    TempStr msg = job->ok ? EutlCacheInfoTemp() : (err ? str::DupTemp(err) : StrL("update failed"));
    job->msg = str::Dup(msg);
    str::Free(err);
    uitask::Post(MkFunc0<EutlUpdateJob>(OnEutlUpdateDone, job), "EutlUpdateDone");
}

void PropertiesView::OnUpdateEutl(PropertiesView*, gp::Ctx* cx, const gp::ClickEvent*) {
    if (gProps.eutlUpdating) {
        return;
    }
    gProps.eutlUpdating = true;
    auto* job = new EutlUpdateJob{gProps.win};
    RunAsync(MkFunc0<EutlUpdateJob>(EutlUpdateThread, job), StrL("EutlUpdate"));
    gp::Notify(cx);
}
#endif

void PropertiesView::OnClose(PropertiesView*, gp::Ctx* cx, const gp::ClickEvent*) {
    DeletePropertiesWindow(nullptr);
    gp::Notify(cx);
}

static void PropsToolSizeToContent();

struct GetFontsResult {
    MainWindow* win;
    str::Builder fontsText;
};

static void OnGetFontsFinished(GetFontsResult* result) {
    if (gProps.visible && gProps.win == result->win) {
        Str marker = Tr("Getting font information...");
        Str props = ToStr(gProps.propsText);
        int pos = str::IndexOf(props, marker);
        if (pos >= 0) {
            gProps.propsText.RemoveAt(pos, len(gProps.propsText) - pos);
        }
        // fontsText starts with "\n" to separate it with a single empty line
        EndWithSingleNewline(gProps.propsText);
        gProps.propsText.Append(ToStr(result->fontsText));
        gp::InputSetValue(gProps.textEdit, ToGpui(ToStr(gProps.propsText)));
        gProps.remeasure = true;
        PropsToolSizeToContent();
        AppShellInvalidate(gProps.win);
    }
    result->fontsText.Reset();
    delete result;
}

struct GetFontsData {
    MainWindow* win;
    EngineBase* engine;
};

static void GetFontsThread(GetFontsData* data) {
    TempStr val = data->engine->GetPropertyTemp(DocProp::FontList);
    auto* result = new GetFontsResult;
    result->win = data->win;
    if (val) {
        result->fontsText.Append(StrL("\n"));
        result->fontsText.Append(Tr("Fonts:"));
        result->fontsText.Append(StrL("\n"));
        result->fontsText.Append(val);
    }
    auto fn = MkFunc0<GetFontsResult>(OnGetFontsFinished, result);
    uitask::Post(fn, "GetFontsFinished");
    data->engine->Release();
    delete data;
    DestroyTempArena();
}

static void PropsOpenToolWindow(MainWindow* win);

void ShowProperties(MainWindow* win, DocController* ctrl) {
    if (gProps.visible) {
        // orig: SetActiveWindow() on the one that is open
        ToolWindowActivate(gProps.tw);
        return;
    }
    if (!ctrl) {
        return;
    }

    gProps.win = win;
    gProps.propsText.Reset();
    DisplayModel* dm = ctrl->AsFixed();
#if OS_WIN
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (EngineMupdfIsPdf(engine)) {
        EutlRegisterLookup();
        gProps.certs = EngineMupdfGetSignatureCerts(engine);
        if (gProps.certs) {
            StrVec labels;
            for (PdfSigCert* cert = gProps.certs; cert; cert = cert->next) {
                labels.Append(cert->label);
            }
            gProps.certSelect.Init(win->gpuiWin ? win->gpuiWin->app : nullptr);
            gProps.certSelect.SetItems(labels, 0);
        }
    }
#endif
    GetPropsText(ctrl, gProps.propsText);
#if OS_WIN
    AppendCertsText(gProps.propsText, gProps.certs);
#endif
    AlignPropertiesText(gProps.propsText);
    EndWithSingleNewline(gProps.propsText);
    gProps.propsText.Append(StrL("\n"));
    gProps.propsText.Append(Tr("Getting font information..."));
    gProps.textEdit = new gp::InputState;
    gProps.textEdit->focus = gp::FocusHandleNew(win->gpuiWin ? win->gpuiWin->app : nullptr);
    gProps.textEdit->kind = gp::InputKind::Textarea;
    gProps.textEdit->mode.kind = gp::LayoutModeKind::PlainText;
    gProps.textEdit->softWrap = false;
    gProps.textEdit->readonly = true;
    gp::InputSetValue(gProps.textEdit, ToGpui(ToStr(gProps.propsText)));
    gProps.visible = true;
    gProps.longestLine = {};
    gProps.oneChar = {};
    gProps.remeasure = true;
    logf("ShowProperties: %d bytes of properties\n", len(gProps.propsText));
    PropsOpenToolWindow(win);
    AppShellInvalidate(win);

    if (!dm || !dm->engine) {
        auto* result = new GetFontsResult;
        result->win = win;
        OnGetFontsFinished(result);
        return;
    }
    auto* data = new GetFontsData;
    data->win = win;
    data->engine = dm->engine;
    data->engine->AddRef();
    auto fn = MkFunc0<GetFontsData>(GetFontsThread, data);
    RunAsync(fn, StrL("GetFontsThread"));
}

// orig sizes the window to the widest line and the number of lines, capped at
// 80% of the work area. The dialog lives in this window, so the cap is 80% of
// the window instead.
constexpr float kPropsMinDx = 420;
// a monospace advance, until the first frame has measured the real one
constexpr float kPropsCharDxGuess = 8;
constexpr float kPropsLineDyGuess = 20;
// orig's SizeToContent: 16 + four "x" of slack after the longest line, then
// the scrollbar, the edit's two edges and 16 more
constexpr float kPropsLineSlackDx = 16;
constexpr int kPropsSlackChars = 4;
constexpr float kPropsEditPadDx = 17 + (2 * 2) + 16;
constexpr float kPropsEditBorderDy = 2 * 2;
constexpr int kPropsExtraLines = 3;
// gpui's Dialog: the padding around the body, and the title + footer rows
constexpr float kPropsDialogPadDx = 2 * 17;
constexpr float kPropsDialogChromeDy = 130;

static void PropsRelayout(MainWindow* win) {
    if (gProps.visible && gProps.win == win) {
        AppShellInvalidate(win);
    }
}

// the line with the most characters (the text is monospaced) and the line count
static Str PropsLongestLine(Str text, int* nLinesOut) {
    Str longest;
    int maxChars = -1;
    int nLines = 0;
    for (int off = 0; off < text.len;) {
        Str rest = Str(text.s + off, text.len - off);
        int nl = str::IndexOfChar(rest, '\n');
        int lineLen = nl >= 0 ? nl : rest.len;
        Str line = Str(rest.s, lineLen);
        int nChars = 0;
        for (int i = 0; i < lineLen; i++) {
            // utf8: count the bytes that start a character
            if (((u8)line.s[i] & 0xc0) != 0x80) {
                nChars++;
            }
        }
        if (nChars > maxChars) {
            maxChars = nChars;
            longest = line;
        }
        nLines++;
        off += lineLen + (nl >= 0 ? 1 : 0);
    }
    *nLinesOut = nLines;
    return longest;
}

// --- a window of its own (Windows) ------------------------------------------

// orig's layout: the edit from the top edge, 8 at the sides and under the
// button row, whose buttons have 8 above them
constexpr float kPropsToolPad = 8;
// orig's Button::GetIdealSize(): the text plus 2 x 12 and 2 x 5
constexpr float kPropsBtnPadDx = 12;
constexpr float kPropsBtnDy = 25;
constexpr float kPropsBtnMinDx = 70;
constexpr float kPropsBtnFontPx = 12;
// the edit control's own margins around its text
constexpr float kPropsEditMarginDx = 4;
constexpr float kPropsEditMarginDy = 1;
// Consolas: the advance of a character over the font's size
constexpr float kConsolasAdvance = 1126.f / 2048.f;
// gpui's inputs make a line 1.25 rem
constexpr float kInputLineRems = 1.25f;

static Str PropsToolTitle() {
    return Tr("Document Properties");
}

// orig's closeOnEsc
static bool PropsToolOnKey(MainWindow*, gp::Ctx*, const gp::KeyEvent* ev) {
    if (ev->vk != VK_ESCAPE) {
        return false;
    }
    DeletePropertiesWindow(nullptr);
    return true;
}

static void PropsToolOnClosed(MainWindow*) {
    gProps.tw = nullptr;
    DeletePropertiesWindow(nullptr);
}

static void PropsToolOnMoved(MainWindow*, Rect) {
    // orig saves on close; this keeps the last place for a window that goes
    // away with its owner
    if (gProps.visible && gProps.tw) {
        SavePropertiesWindowPos();
    }
}

#if OS_WIN
static gp::El* PropsToolBuild(MainWindow* win, gp::Ctx* cx);

static ToolWindowDesc PropsToolDesc() {
    // orig: WS_OVERLAPPEDWINDOW without an owner
    ToolWindowDesc desc;
    desc.name = "properties";
    desc.title = PropsToolTitle;
    desc.frame = ToolWinFrame::Overlapped;
    desc.resize = ToolWinResize::Resizable;
    desc.owner = ToolWinOwner::TopLevel;
    desc.minClient = Size(320, 200);
    desc.build = PropsToolBuild;
    desc.onKey = PropsToolOnKey;
    desc.onClosed = PropsToolOnClosed;
    desc.onMoved = PropsToolOnMoved;
    return desc;
}

// orig's PropertiesWnd::SizeToContent, with its arithmetic (it adds the
// classic frame sizes, not what the window really has): the window size in
// pixels for the text as GDI measures it in orig's font. ng: gpui measures
// text only while it paints, and the window has to have its size before it is
// shown
static Size PropsToolWindowSize(MainWindow* win) {
    HWND hwndFrame = AppShellNativeHwnd(win);
    int dpi = AppShellWindowDpi(win);
    if (dpi <= 0) {
        dpi = 96;
    }
    HDC hdc = GetDC(hwndFrame);
    PlatformFont* propsFont = HdcCreateSimpleFont(hdc, StrL("Consolas"), 14);
    ReleaseDC(hwndFrame, hdc);

    int maxLineDx = 0;
    int nLines = 0;
    Str text = ToStr(gProps.propsText);
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
    int charDx = PlatformFontMeasureText(propsFont, StrL("x")).dx;
    gProps.toolCharDx = (float)charDx * 96.f / (float)dpi;
    gProps.toolLineDy = (float)lineHeight * 96.f / (float)dpi;
    // a bit of slack so the longest lines don't touch the right edge
    maxLineDx += 4 * charDx;

    UINT udpi = (UINT)dpi;
    int pad = MulDiv((int)kPropsToolPad, dpi, 96);
    int editPadding = GetSystemMetricsForDpi(SM_CXVSCROLL, udpi) + (2 * GetSystemMetricsForDpi(SM_CXEDGE, udpi)) + 16;
    int frameDx = GetSystemMetricsForDpi(SM_CXFRAME, udpi) * 2;
    Size btnText = PlatformFontMeasureText(GetDefaultGuiFont(), Tr("Copy To Clipboard"));
    int btnDx = std::max(btnText.dx + MulDiv(2 * (int)kPropsBtnPadDx, dpi, 96), MulDiv((int)kPropsBtnMinDx, dpi, 96));
    int btnDy = btnText.dy + MulDiv(2 * 5, dpi, 96);
    int wantedClientDx = std::max(maxLineDx + editPadding, btnDx + (2 * pad));
    int wantedDx = wantedClientDx + frameDx;

    int editBorderDy = 2 * GetSystemMetricsForDpi(SM_CYEDGE, udpi);
    int frameDy = (GetSystemMetricsForDpi(SM_CYFRAME, udpi) * 2) + GetSystemMetricsForDpi(SM_CYCAPTION, udpi);
    int btnAreaDy = std::max(MulDiv(40, dpi, 96), btnDy + (2 * pad));
    int wantedDy = ((nLines + kPropsExtraLines) * lineHeight) + editBorderDy + btnAreaDy + pad + frameDy;

    // cap at 80% of screen
    Rect work = GetWorkAreaRect(HwndWindowRect(hwndFrame), hwndFrame);
    wantedDx = std::min(wantedDx, (work.dx * 80) / 100);
    wantedDy = std::min(wantedDy, (work.dy * 80) / 100);
    return Size(wantedDx, wantedDy);
}

// orig's placement: at PropWinPos or centered on the frame, kept on screen
static Rect PropsToolRect(MainWindow* win, Size outer) {
    HWND hwndFrame = AppShellNativeHwnd(win);
    Rect frame = HwndWindowRect(hwndFrame);
    Point saved = gSettings->propWinPos;
    Rect r{saved.x, saved.y, outer.dx, outer.dy};
    if (saved.IsEmpty()) {
        r.x = frame.x + (frame.dx - outer.dx) / 2;
        r.y = frame.y + (frame.dy - outer.dy) / 2;
    }
    return ShiftRectToWorkArea(r, hwndFrame, true);
}
#endif

static void PropsOpenToolWindow(MainWindow* win) {
    if (gProps.tw || !ToolWindowsAvailable()) {
        return;
    }
#if OS_WIN
    Rect r = PropsToolRect(win, PropsToolWindowSize(win));
    gProps.initialPos = {r.x, r.y};
    gProps.toolSized = true;
    gProps.tw = ToolWindowOpen(PropsToolDesc(), win, r);
#endif
}

// the text changed (the fonts arrived): orig sizes the window again, where
// it is
static void PropsToolSizeToContent() {
#if OS_WIN
    if (!gProps.visible || !gProps.tw || !IsMainWindowValid(gProps.win)) {
        return;
    }
    Rect cur = ToolWindowRect(gProps.tw);
    Size sz = PropsToolWindowSize(gProps.win);
    ToolWindowMove(gProps.tw, Rect(cur.x, cur.y, sz.dx, sz.dy));
#endif
}

#if OS_WIN
// orig's themed button, in the window's rem (see PropsToolBuild)
static gp::El* PropsToolButton(gp::Ctx* cx, gp::Str id, Str label, gp::Listener onClick, float fontPx,
                               bool disabled = false) {
    gp::El* btn = gpc::Button::New(cx, id)->Label(ToGpui(label))->Disabled(disabled)->OnClick(onClick)->IntoEl();
    return btn->H(kPropsBtnDy)->MinW(kPropsBtnMinDx)->PadX(kPropsBtnPadDx)->Font(fontPx)->Shrink0();
}

// ng: orig's edit is Consolas 14 px, which GDI lays out 8 wide and 17 high.
// A gpui input's line is 1.25 rem whatever its font, and a font size is in
// rems as well, so this window gets the rem that makes a line 17 and the font
// sizes are given in that rem
static gp::El* PropsToolBuild(MainWindow*, gp::Ctx* cx) {
    if (!gProps.visible || !gProps.tw || !gProps.textEdit) {
        return nullptr;
    }
    if (!gPropertiesView.IsValid()) {
        gPropertiesView = gp::EntityNewState<PropertiesView>(cx->app);
    }
    float rem = gProps.toolLineDy / kInputLineRems;
    gp::WindowSetRemSize(cx->win, rem);
    float remScale = 16.f / rem;
    const gp::Theme& th = gp::ThemeNow(cx->app);
    gp::WinSize ws = gp::WindowSize(cx->win);

    gp::InputState* state = gProps.textEdit;
    state->softWrap = false;
    gp::InputEditorStyle style;
    style.foreground = th.foreground;
    style.mutedForeground = th.mutedFg;
    style.caret = th.caret;
    style.selection = gp::RgbaOpacity(th.selection, 0.4f);
    style.fontSize = (gProps.toolCharDx / kConsolasAdvance) * remScale;
    style.fontFamily = gp::FontFamilyIntern(GStrL("Consolas"));

    float btnRowDy = kPropsToolPad + kPropsBtnDy;
    float editDy = std::max(ws.dipH - btnRowDy - kPropsToolPad, gProps.toolLineDy);
    state->viewH = editDy - 2 * (1 + kPropsEditMarginDy);
    gp::El* edit = gp::InputBase::New(cx, GStrL("properties-text"), true, gp::AccessibilityRole::MultilineTextInput)
                       ->BindInput(state)
                       ->W(gp::kFill)
                       ->H(editDy)
                       ->Shrink0()
                       ->PadX(kPropsEditMarginDx)
                       ->PadY(kPropsEditMarginDy)
                       ->ClipY()
                       ->ScrollY(state->scrollY)
                       ->ScrollX(state->scrollX)
                       ->ScrollFromPath()
                       ->Bg(th.inputBg)
                       ->Border(1, th.inputBorder)
                       ->Child(gp::Textarea::New(cx, state, style));

    float btnFont = kPropsBtnFontPx * remScale;
    gp::El* footer = gp::Div(cx->a)->FlexRow()->JustifyEnd()->ItemsEnd()->Gap(kPropsToolPad)->H(btnRowDy)->Shrink0();
    footer->Child(PropsToolButton(cx, GStrL("props-copy"), Tr("Copy To Clipboard"),
                                  gp::ListenTo(gPropertiesView, &PropertiesView::OnCopy), btnFont));
    if (gProps.certs) {
        gProps.certSelect.PollChanged(cx->app);
        if (gProps.certs->next) {
            footer->Child(gProps.certSelect.Build(cx, StrL("props-certificate"), 220));
        }
        footer->Child(PropsToolButton(cx, GStrL("props-view-certificate"), Tr("View Certificate..."),
                                      gp::ListenTo(gPropertiesView, &PropertiesView::OnViewCert), btnFont));
        footer->Child(PropsToolButton(cx, GStrL("props-update-eutl"), Tr("Update EU Trusted List"),
                                      gp::ListenTo(gPropertiesView, &PropertiesView::OnUpdateEutl), btnFont,
                                      gProps.eutlUpdating));
    }
    return gp::Div(cx->a)
        ->FlexCol()
        ->W(gp::kFill)
        ->Flex1()
        ->MinH(0)
        ->PadX(kPropsToolPad)
        ->PadB(kPropsToolPad)
        ->Child(edit)
        ->Child(footer);
}
#endif

gp::El* PropertiesDialogBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gProps.visible || gProps.tw || gProps.win != win) {
        return nullptr;
    }
    if (!gPropertiesView.IsValid()) {
        gPropertiesView = gp::EntityNewState<PropertiesView>(cx->app);
    }
    gp::WinSize ws = gp::WindowSize(cx->win);
    int nLines = 0;
    Str longest = PropsLongestLine(ToStr(gProps.propsText), &nLines);
    float charDx = gProps.oneChar.w > 0 ? gProps.oneChar.w : kPropsCharDxGuess;
    float lineDy = gProps.oneChar.h > 0 ? gProps.oneChar.h : kPropsLineDyGuess;
    float lineDx = gProps.longestLine.w;
    if (lineDx <= 0 || gProps.remeasure) {
        // not measured yet (or measured for the previous text): estimate, and
        // come back once this frame has laid the measuring line out
        float guessDx = (float)len(longest) * charDx;
        lineDx = std::max(lineDx, guessDx);
        gProps.remeasure = false;
        uitask::Post(MkFunc0(PropsRelayout, win), "PropsRelayout");
    }
    float editDx = lineDx + kPropsLineSlackDx + ((float)kPropsSlackChars * charDx) + kPropsEditPadDx;
    float dx = std::max(kPropsMinDx, editDx + kPropsDialogPadDx);
    dx = std::min(dx, ws.dipW * 0.8f);
    float dy = ((float)(nLines + kPropsExtraLines) * lineDy) + kPropsEditBorderDy;
    dy = std::min(dy, (ws.dipH * 0.8f) - kPropsDialogChromeDy);
    dy = std::max(dy, lineDy * (float)kPropsExtraLines);

    gpc::Textarea* area = gpc::Textarea::New(cx, GStrL("properties-text"), gProps.textEdit);
    gp::El* edit = area->H(dy)->SoftWrap(false)->Readonly()->IntoEl()->Mono();
    // the size gpui's inputs draw their text in, whatever the element says
    float fontSize = gp::UiInputFontPx(gp::UiSize::Medium);
    // ng: gpui measures text only while it paints, so the longest line and an
    // "x" are laid out invisibly and their bounds read back on the next frame
    gp::Rgba transparent{0, 0, 0, 0};
    gp::El* measure = gp::Div(cx->a)->FlexCol()->ItemsStart()->H(0)->ClipX();
    measure->Child(gp::TextEl(cx->a, GpuiDup(cx->a, longest))
                       ->Mono()
                       ->Font(fontSize)
                       ->Fg(transparent)
                       ->Shrink0()
                       ->BoundsOut(&gProps.longestLine));
    measure->Child(
        gp::TextEl(cx->a, GStrL("x"))->Mono()->Font(fontSize)->Fg(transparent)->Shrink0()->BoundsOut(&gProps.oneChar));
    gp::El* body = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->Child(edit)->Child(measure);

    gp::El* footer = gp::Div(cx->a)->FlexRow()->JustifyEnd()->Gap(8);
#if OS_WIN
    if (gProps.certs) {
        gProps.certSelect.PollChanged(cx->app);
        if (gProps.certs->next) {
            footer->Child(gProps.certSelect.Build(cx, StrL("props-certificate"), 220));
        }
        footer->Child(gpc::Button::New(cx, GStrL("props-view-certificate"))
                          ->Label(ToGpui(Tr("View Certificate...")))
                          ->OnClick(gp::ListenTo(gPropertiesView, &PropertiesView::OnViewCert))
                          ->IntoEl());
        footer->Child(gpc::Button::New(cx, GStrL("props-update-eutl"))
                          ->Label(ToGpui(Tr("Update EU Trusted List")))
                          ->Disabled(gProps.eutlUpdating)
                          ->OnClick(gp::ListenTo(gPropertiesView, &PropertiesView::OnUpdateEutl))
                          ->IntoEl());
    }
#endif
    footer->Child(gpc::Button::New(cx, GStrL("props-copy"))
                      ->Label(ToGpui(Tr("Copy To Clipboard")))
                      ->OnClick(gp::ListenTo(gPropertiesView, &PropertiesView::OnCopy))
                      ->IntoEl());
    // ng: a dialog in the frame has no close box
    footer->Child(gpc::Button::New(cx, GStrL("props-close"))
                      ->Label(ToGpui(Tr("Close")))
                      ->Outline()
                      ->OnClick(gp::ListenTo(gPropertiesView, &PropertiesView::OnClose))
                      ->IntoEl());

    return gpc::Dialog::New(cx)
        ->Open(true)
        ->Title(ToGpui(Tr("Document Properties")))
        ->Body(body)
        ->W(dx)
        ->Footer(footer)
        ->OnClose(gp::ListenTo(gPropertiesView, &PropertiesView::OnClose))
        ->OnCancel(gp::ListenTo(gPropertiesView, &PropertiesView::OnClose))
        ->IntoEl(ws);
}
