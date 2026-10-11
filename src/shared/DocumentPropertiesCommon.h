/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by DocumentPropertiesCommon.cpp and each app's DocumentProperties.cpp ---

TempStr FormatPageSizeTemp(EngineBase* engine, int pageNo, int rotation);
TempStr FormatPermissionsTemp(DocController* ctrl);
void AppendProp(str::Builder& out, Str key, Str value);
void AppendPdfFileStructure(str::Builder& out, Str fstruct, Str filePath);
void GetAllProps(DocController* ctrl, Props& propsOut);
void AppendFileType(str::Builder& out, Str path);
void AppendReadingDirection(str::Builder& out, DisplayModel* dm);
void EndWithSingleNewline(str::Builder& b);
void AlignPropertiesText(str::Builder& text);

TempStr AddTimeZone(TempStr s, int timeZone);
void GetPropsText(DocController* ctrl, str::Builder& out);

// implemented by each app
void AppendDateProp(str::Builder& out, Str key, Str val, bool isPdfDate);

#if OS_WIN
void AppendCertsText(str::Builder& out, PdfSigCert* certs);
// implemented by each app
TempStr FileTimeLocalTemp(const FILETIME& ft);
#endif

struct EutlUpdateJob {
    // who asked: orig keeps the dialog's window, ng the main window
    HWND hwnd = nullptr;
    MainWindow* win = nullptr;
    bool ok = false;
    Str msg;
};
void EutlUpdateThread(EutlUpdateJob* job);
// implemented by each app
void OnEutlUpdateDone(EutlUpdateJob* job);

// the properties window the fonts are for: an HWND in orig, the main window in ng
struct GetFontsResult {
    HWND hwnd = nullptr;
    MainWindow* win = nullptr;
    str::Builder fontsText;
};

struct GetFontsData {
    HWND hwnd = nullptr;
    MainWindow* win = nullptr;
    EngineBase* engine = nullptr;
};
void GetFontsThread(GetFontsData* data);
// implemented by each app
void OnGetFontsFinished(GetFontsResult* result);
