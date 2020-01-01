/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct GlobalPrefs;

WCHAR* Dialog_GoToPage(HWND hwnd, const WCHAR* currentPageLabel, int pageCount, bool onlyNumeric = true);
WCHAR* Dialog_Find(HWND hwnd, const WCHAR* previousSearch, bool* matchCase);
WCHAR* Dialog_GetPassword(HWND hwnd, const WCHAR* fileName, bool* rememberPassword);
INT_PTR Dialog_PdfAssociate(HWND hwnd, bool* dontAskAgainOut);
const char* Dialog_ChangeLanguge(HWND hwnd, const char* currLangCode);
INT_PTR Dialog_NewVersionAvailable(HWND hwnd, const WCHAR* currentVersion, const WCHAR* newVersion,
                                   bool* skipThisVersion);
bool Dialog_CustomZoom(HWND hwnd, bool forChm, float* currZoomInOut);
INT_PTR Dialog_Settings(HWND hwnd, GlobalPrefs* prefs);
bool Dialog_AddFavorite(HWND hwnd, const WCHAR* pageNo, AutoFreeWstr& favName);

enum class PrintRangeAdv { All = 0, Even, Odd };
enum class PrintScaleAdv { None = 0, Shrink, Fit };
enum class PrintRotationAdv { Auto = 0, Portrait, Landscape };

struct Print_Advanced_Data {
    PrintRangeAdv range;
    PrintScaleAdv scale;
    PrintRotationAdv rotation;

    explicit Print_Advanced_Data(PrintRangeAdv range = PrintRangeAdv::All, PrintScaleAdv scale = PrintScaleAdv::Shrink,
                                 PrintRotationAdv rotation = PrintRotationAdv::Auto)
        : range(range), scale(scale), rotation(rotation) {
    }
};

HPROPSHEETPAGE CreatePrintAdvancedPropSheet(Print_Advanced_Data* data, ScopedMem<DLGTEMPLATE>& dlgTemplate);
