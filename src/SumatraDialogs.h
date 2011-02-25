/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef SUMATRA_PDF_DIALOGS_H_
#define SUMATRA_PDF_DIALOGS_H_

#define DIALOG_OK_PRESSED 1
#define DIALOG_YES_PRESSED 1
#define DIALOG_CANCEL_PRESSED 2
#define DIALOG_NO_PRESSED 3

class WindowInfo;

int     Dialog_GoToPage(WindowInfo *win);
TCHAR * Dialog_Find(HWND hwnd, const TCHAR *previousSearch, bool *matchCase);
TCHAR * Dialog_GetPassword(HWND hwnd, const TCHAR *fileName, bool *rememberPassword);
INT_PTR Dialog_PdfAssociate(HWND hwnd, BOOL *dontAskAgainOut);
int     Dialog_ChangeLanguge(HWND hwnd, int currLangId);

/* For passing data to/from 'new version available' dialog */
typedef struct {
    const TCHAR *currVersion;
    const TCHAR *newVersion;
    BOOL skipThisVersion;
} Dialog_NewVersion_Data;

INT_PTR Dialog_NewVersionAvailable(HWND hwnd, Dialog_NewVersion_Data *data);

INT_PTR Dialog_CustomZoom(HWND hwnd, float *currZoom);
INT_PTR Dialog_Settings(HWND hwnd, SerializableGlobalPrefs *prefs);

enum PrintRangeAdv { PrintRangeAll = 0, PrintRangeEven, PrintRangeOdd };
enum PrintScaleAdv { PrintScaleNone = 0, PrintScaleShrink, PrintScaleFit };

typedef struct {
    enum PrintRangeAdv range;
    enum PrintScaleAdv scale;
} Print_Advanced_Data;

HPROPSHEETPAGE CreatePrintAdvancedPropSheet(HINSTANCE hInst, Print_Advanced_Data *data);

// in SumatraPDF.cpp
void AssociateExeWithPdfExtension();

#endif
