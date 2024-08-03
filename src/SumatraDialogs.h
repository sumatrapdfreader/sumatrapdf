/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct GlobalPrefs;

char* Dialog_GoToPage(HWND hwnd, const char* currentPageLabel, int pageCount, bool onlyNumeric = true);
char* Dialog_Find(HWND hwnd, const char* previousSearch, bool* matchCase);
char* Dialog_GetPassword(HWND hwnd, const char* fileName, bool* rememberPassword);
INT_PTR Dialog_PdfAssociate(HWND hwnd, bool* dontAskAgainOut);
const char* Dialog_ChangeLanguge(HWND hwnd, const char* currLangCode);
bool Dialog_CustomZoom(HWND hwnd, bool forChm, float* currZoomInOut);
INT_PTR Dialog_Settings(HWND hwnd, GlobalPrefs* prefs);
bool Dialog_AddFavorite(HWND hwnd, const char* pageNo, AutoFreeStr& favName);

enum class PrintRangeAdv { All = 0, Even, Odd };
enum class PrintScaleAdv { None = 0, Shrink, Fit };
enum class PrintRotationAdv { Auto = 0, Portrait, Landscape };

struct Print_Advanced_Data {
    PrintRangeAdv range;
    PrintScaleAdv scale;
    PrintRotationAdv rotation;
    bool autoRotate;

    explicit Print_Advanced_Data(PrintRangeAdv range = PrintRangeAdv::All, PrintScaleAdv scale = PrintScaleAdv::Shrink,
                                 PrintRotationAdv rotation = PrintRotationAdv::Auto, bool autoRotate = true)
        : range(range), scale(scale), rotation(rotation), autoRotate(autoRotate) {
    }
};

HPROPSHEETPAGE CreatePrintAdvancedPropSheet(Print_Advanced_Data* data, ScopedMem<DLGTEMPLATE>& dlgTemplate);

TempStr ZoomLevelStr(float zoom);
