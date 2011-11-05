/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef SumatraDialogs_h
#define SumatraDialogs_h

#include "AppPrefs.h"

TCHAR * Dialog_GoToPage(HWND hwnd, const TCHAR *currentPageLabel, int pageCount, bool onlyNumeric=true);
TCHAR * Dialog_Find(HWND hwnd, const TCHAR *previousSearch, bool *matchCase);
TCHAR * Dialog_GetPassword(HWND hwnd, const TCHAR *fileName, bool *rememberPassword);
INT_PTR Dialog_PdfAssociate(HWND hwnd, bool *dontAskAgainOut);
int     Dialog_ChangeLanguge(HWND hwnd, int currLangId);
INT_PTR Dialog_NewVersionAvailable(HWND hwnd, const TCHAR *currentVersion, const TCHAR *newVersion, bool *skipThisVersion);
bool    Dialog_CustomZoom(HWND hwnd, bool forChm, float *currZoomInOut);
INT_PTR Dialog_Settings(HWND hwnd, SerializableGlobalPrefs *prefs);
bool    Dialog_AddFavorite(HWND hwnd, const TCHAR *pageNo, ScopedMem<TCHAR>& favName);

enum PrintRangeAdv { PrintRangeAll = 0, PrintRangeEven, PrintRangeOdd };
enum PrintScaleAdv { PrintScaleNone = 0, PrintScaleShrink, PrintScaleFit };

struct Print_Advanced_Data {
    PrintRangeAdv range;
    PrintScaleAdv scale;
};

HPROPSHEETPAGE CreatePrintAdvancedPropSheet(Print_Advanced_Data *data, ScopedMem<DLGTEMPLATE>& dlgTemplate);

#endif
