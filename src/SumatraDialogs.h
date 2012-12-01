/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef SumatraDialogs_h
#define SumatraDialogs_h

struct SerializableGlobalPrefs;

WCHAR * Dialog_GoToPage(HWND hwnd, const WCHAR *currentPageLabel, int pageCount, bool onlyNumeric=true);
WCHAR * Dialog_Find(HWND hwnd, const WCHAR *previousSearch, bool *matchCase);
WCHAR * Dialog_GetPassword(HWND hwnd, const WCHAR *fileName, bool *rememberPassword);
INT_PTR Dialog_PdfAssociate(HWND hwnd, bool *dontAskAgainOut);
int     Dialog_ChangeLanguge(HWND hwnd, int currLangId);
INT_PTR Dialog_NewVersionAvailable(HWND hwnd, const WCHAR *currentVersion, const WCHAR *newVersion, bool *skipThisVersion);
bool    Dialog_CustomZoom(HWND hwnd, bool forChm, float *currZoomInOut);
INT_PTR Dialog_Settings(HWND hwnd, SerializableGlobalPrefs *prefs);
bool    Dialog_AddFavorite(HWND hwnd, const WCHAR *pageNo, ScopedMem<WCHAR>& favName);

enum PrintRangeAdv { PrintRangeAll = 0, PrintRangeEven, PrintRangeOdd };
enum PrintScaleAdv { PrintScaleNone = 0, PrintScaleShrink, PrintScaleFit };

struct Print_Advanced_Data {
    PrintRangeAdv range;
    PrintScaleAdv scale;
    bool asImage;

    Print_Advanced_Data(PrintRangeAdv range=PrintRangeAll,
                        PrintScaleAdv scale=PrintScaleShrink,
                        bool asImage=false) :
        range(range), scale(scale), asImage(asImage) { }
};

HPROPSHEETPAGE CreatePrintAdvancedPropSheet(Print_Advanced_Data *data, ScopedMem<DLGTEMPLATE>& dlgTemplate);

#endif
