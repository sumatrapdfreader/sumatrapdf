/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct GlobalPrefs;

char* Dialog_GoToPage(HWND hwnd, const char* currentPageLabel, int pageCount, bool onlyNumeric = true);
char* Dialog_Find(HWND hwnd, const char* previousSearch, bool* matchCase);
char* Dialog_GetPassword(HWND hwnd, const char* fileName, bool* rememberPassword, bool* showPassword);
const char* Dialog_ChangeLanguge(HWND hwnd, const char* currLangCode);
bool Dialog_CustomZoom(HWND hwnd, bool forChm, float* currZoomInOut);
bool Dialog_ChangeScrollbar(HWND hwnd);
INT_PTR Dialog_Settings(HWND hwnd, GlobalPrefs* prefs);
bool Dialog_SetInverseSearch(HWND hwnd, GlobalPrefs* prefs);
bool Dialog_AddFavorite(HWND hwnd, const char* pageNo, AutoFreeStr& favName);

enum class PrintRangeAdv {
    All = 0,
    Even,
    Odd
};
enum class PrintScaleAdv {
    None = 0,
    Shrink,
    Fit,
    Stretch
};
enum class PrintRotationAdv {
    Auto = 0,
    Portrait,
    Landscape
};

struct Print_Advanced_Data {
    PrintRangeAdv range;
    PrintScaleAdv scale;
    PrintRotationAdv rotation;
    bool autoRotate;
    bool centerHorizontally;
    // when true, let the printer pick the input tray whose paper matches the
    // document's page size (DMBIN_FORMSOURCE), independent of page scaling
    bool paperSourceByPageSize;
    // when true, set the paper size to each page's own size before printing it,
    // so mixed page size documents print to the right paper/tray
    bool perPagePaperSize;
    // extra rotation applied to the printout, in degrees (0, 90, 180 or 270),
    // on top of the automatic rotation; lets the user fix wrong orientation
    // (e.g. upside-down output on virtual printers), issue #1246
    int extraRotation;

    explicit Print_Advanced_Data(PrintRangeAdv range = PrintRangeAdv::All, PrintScaleAdv scale = PrintScaleAdv::Shrink,
                                 PrintRotationAdv rotation = PrintRotationAdv::Auto, bool autoRotate = true,
                                 bool centerHorizontally = false, bool paperSourceByPageSize = false,
                                 bool perPagePaperSize = false, int extraRotation = 0)
        : range(range),
          scale(scale),
          rotation(rotation),
          autoRotate(autoRotate),
          centerHorizontally(centerHorizontally),
          paperSourceByPageSize(paperSourceByPageSize),
          perPagePaperSize(perPagePaperSize),
          extraRotation(extraRotation) {}
};

HPROPSHEETPAGE CreatePrintAdvancedPropSheet(Print_Advanced_Data* data, ScopedMem<DLGTEMPLATE>& dlgTemplate);

struct MainWindow;

struct BgColorResult {
    COLORREF color;
    bool isCheckered;
    bool applyToAllFiles; // true = all files like this, false = this file only
};

bool Dialog_ChangeBackgroundColor(HWND hwnd, COLORREF currentColor, bool isCheckered, const char* allFilesLabel,
                                  BgColorResult& result);
bool Dialog_SetTabColor(HWND hwnd, COLORREF currentColor, bool isUnset, COLORREF& resultColor, bool& resultIsUnset);

TempStr ZoomLevelStr(float zoom);
