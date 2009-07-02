/* Copyright Krzysztof Kowalczyk 2006-2009
   License: GPLv2 */
#ifndef SUMATRA_PDF_DIALOGS_H_
#define SUMATRA_PDF_DIALOGS_H_

#define DIALOG_OK_PRESSED 1
#define DIALOG_YES_PRESSED 1
#define DIALOG_CANCEL_PRESSED 2
#define DIALOG_NO_PRESSED 3

class WindowInfo;

/* For passing data to/from SetInverseSearch dialog */
int     Dialog_GoToPage(WindowInfo *win);
#ifdef _TEX_ENHANCEMENT
TCHAR *  Dialog_SetInverseSearchCmdline(WindowInfo *win, const TCHAR *cmdline);
#endif
TCHAR * Dialog_GetPassword(WindowInfo *win, const TCHAR *fileName);
int     Dialog_PdfAssociate(HWND hwnd, BOOL *dontAskAgainOut);
int     Dialog_ChangeLanguge(HWND hwnd, int currLangId);

/* For passing data to/from 'new version available' dialog */
typedef struct {
    const TCHAR *currVersion;
    const TCHAR *newVersion;
    BOOL skipThisVersion;
} Dialog_NewVersion_Data;

int     Dialog_NewVersionAvailable(HWND hwnd, Dialog_NewVersion_Data *data);

int     Dialog_Settings(HWND hwnd, SerializableGlobalPrefs *prefs);

#endif
