/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "SumatraPDF.h"
#include "SumatraDialogs.h"
#include "AppTools.h"
#include "Resource.h"

#include "WinUtil.h"
#include "DialogSizer.h"
#include "translations.h"

// cf. http://msdn.microsoft.com/en-us/library/ms645398(v=VS.85).aspx
struct DLGTEMPLATEEX {
    WORD      dlgVer;   // 0x0001
    WORD      signature;// 0xFFFF
    DWORD     helpID;
    DWORD     exStyle;
    DWORD     style;
    WORD      cDlgItems;
    short     x, y, cx, cy;
    /* ... */
};

// gets a dialog template from the resources and sets the RTL flag
// cf. http://www.ureader.com/msg/1484387.aspx
static DLGTEMPLATE *GetRtLDlgTemplate(int dlgId)
{
    HRSRC dialogRC = FindResource(NULL, MAKEINTRESOURCE(dlgId), RT_DIALOG);
    HGLOBAL dlgTemplate = LoadResource(NULL, dialogRC);
    void *origDlgTemplate = LockResource(dlgTemplate);
    size_t size = SizeofResource(NULL, dialogRC);

    DLGTEMPLATE *rtlDlgTemplate = (DLGTEMPLATE *)memdup(origDlgTemplate, size);
    if (rtlDlgTemplate->style == MAKELONG(0x0001, 0xFFFF))
        ((DLGTEMPLATEEX *)rtlDlgTemplate)->exStyle |= WS_EX_LAYOUTRTL;
    else
        rtlDlgTemplate->dwExtendedStyle |= WS_EX_LAYOUTRTL;
    UnlockResource(dlgTemplate);

    return rtlDlgTemplate;
}

// creates a dialog box that dynamically gets a right-to-left layout if needed
static INT_PTR CreateDialogBox(int dlgId, HWND parent, DLGPROC DlgProc, LPARAM data)
{
    if (!IsUIRightToLeft())
        return DialogBoxParam(NULL, MAKEINTRESOURCE(dlgId), parent, DlgProc, data);

    ScopedMem<DLGTEMPLATE> rtlDlgTemplate(GetRtLDlgTemplate(dlgId));
    return DialogBoxIndirectParam(NULL, rtlDlgTemplate, parent, DlgProc, data);
}


/* For passing data to/from GetPassword dialog */
struct Dialog_GetPassword_Data {
    const TCHAR *  fileName;   /* name of the file for which we need the password */
    TCHAR *        pwdOut;     /* password entered by the user */
    bool *         remember;   /* remember the password (encrypted) or ask again? */
};

static INT_PTR CALLBACK Dialog_GetPassword_Proc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    Dialog_GetPassword_Data *data;

    if (WM_INITDIALOG == msg)
    {
        data = (Dialog_GetPassword_Data*)lParam;
        win::SetText(hDlg, _TR("Enter password"));
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)data);
        EnableWindow(GetDlgItem(hDlg, IDC_REMEMBER_PASSWORD), data->remember != NULL);

        ScopedMem<TCHAR> txt(str::Format(_TR("Enter password for %s"), data->fileName));
        SetDlgItemText(hDlg, IDC_GET_PASSWORD_LABEL, txt);
        SetDlgItemText(hDlg, IDC_GET_PASSWORD_EDIT, _T(""));
        SetDlgItemText(hDlg, IDC_STATIC, _TR("&Password:"));
        SetDlgItemText(hDlg, IDC_REMEMBER_PASSWORD, _TR("&Remember the password for this document"));
        SetDlgItemText(hDlg, IDOK, _TR("OK"));
        SetDlgItemText(hDlg, IDCANCEL, _TR("Cancel"));

        CenterDialog(hDlg);
        SetFocus(GetDlgItem(hDlg, IDC_GET_PASSWORD_EDIT));
        return FALSE;
    }

    switch (msg)
    {
        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case IDOK:
                    data = (Dialog_GetPassword_Data*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
                    assert(data);
                    data->pwdOut = win::GetText(GetDlgItem(hDlg, IDC_GET_PASSWORD_EDIT));
                    if (data->remember)
                        *data->remember = BST_CHECKED == IsDlgButtonChecked(hDlg, IDC_REMEMBER_PASSWORD);
                    EndDialog(hDlg, IDOK);
                    return TRUE;

                case IDCANCEL:
                    EndDialog(hDlg, IDCANCEL);
                    return TRUE;
            }
            break;
    }
    return FALSE;
}

/* Shows a 'get password' dialog for a given file.
   Returns a password entered by user as a newly allocated string or
   NULL if user cancelled the dialog or there was an error.
   Caller needs to free() the result.
*/
TCHAR *Dialog_GetPassword(HWND hwndParent, const TCHAR *fileName, bool *rememberPassword)
{
    Dialog_GetPassword_Data data = { 0 };
    data.fileName = fileName;
    data.remember = rememberPassword;

    INT_PTR res = CreateDialogBox(IDD_DIALOG_GET_PASSWORD, hwndParent,
                                  Dialog_GetPassword_Proc, (LPARAM)&data);
    if (IDOK != res) {
        free((void*)data.pwdOut);
        return NULL;
    }
    return data.pwdOut;
}

/* For passing data to/from GoToPage dialog */
struct Dialog_GoToPage_Data {
    const TCHAR *   currPageLabel;  // currently shown page label
    int             pageCount;      // total number of pages
    bool            onlyNumeric;    // whether the page label must be numeric
    TCHAR *         newPageLabel;   // page number entered by user
};

static INT_PTR CALLBACK Dialog_GoToPage_Proc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    HWND                    editPageNo;
    Dialog_GoToPage_Data *  data;

    if (WM_INITDIALOG == msg)
    {
        data = (Dialog_GoToPage_Data*)lParam;
        assert(data);
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)data);
        win::SetText(hDlg, _TR("Go to page"));

        editPageNo = GetDlgItem(hDlg, IDC_GOTO_PAGE_EDIT);
        if (!data->onlyNumeric)
            SetWindowLong(editPageNo, GWL_STYLE, GetWindowLong(editPageNo, GWL_STYLE) & ~ES_NUMBER);
        assert(data->currPageLabel);
        SetDlgItemText(hDlg, IDC_GOTO_PAGE_EDIT, data->currPageLabel);
        ScopedMem<TCHAR> totalCount(str::Format(_TR("(of %d)"), data->pageCount));
        SetDlgItemText(hDlg, IDC_GOTO_PAGE_LABEL_OF, totalCount);

        Edit_SelectAll(editPageNo);
        SetDlgItemText(hDlg, IDC_STATIC, _TR("&Go to page:"));
        SetDlgItemText(hDlg, IDOK, _TR("Go to page"));
        SetDlgItemText(hDlg, IDCANCEL, _TR("Cancel"));

        CenterDialog(hDlg);
        SetFocus(editPageNo);
        return FALSE;
    }

    switch (msg)
    {
        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case IDOK:
                    data = (Dialog_GoToPage_Data*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
                    assert(data);
                    editPageNo = GetDlgItem(hDlg, IDC_GOTO_PAGE_EDIT);
                    data->newPageLabel = win::GetText(editPageNo);
                    EndDialog(hDlg, IDOK);
                    return TRUE;

                case IDCANCEL:
                    EndDialog(hDlg, IDCANCEL);
                    return TRUE;
            }
            break;
    }
    return FALSE;
}

/* Shows a 'go to page' dialog and returns the page label entered by the user
   or NULL if user clicked the "cancel" button or there was an error.
   The caller must free() the result. */
TCHAR *Dialog_GoToPage(HWND hwnd, const TCHAR *currentPageLabel, int pageCount, bool onlyNumeric)
{
    Dialog_GoToPage_Data data;
    data.currPageLabel = currentPageLabel;
    data.pageCount = pageCount;
    data.onlyNumeric = onlyNumeric;
    data.newPageLabel = NULL;

    CreateDialogBox(IDD_DIALOG_GOTO_PAGE, hwnd,
                    Dialog_GoToPage_Proc, (LPARAM)&data);
    return data.newPageLabel;
}

/* For passing data to/from Find dialog */
struct Dialog_Find_Data {
    TCHAR * searchTerm;
    bool    matchCase;
    WNDPROC editWndProc;
};

static LRESULT CALLBACK Dialog_Find_Edit_Proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    ExtendedEditWndProc(hwnd, message, wParam, lParam);

    Dialog_Find_Data *data = (Dialog_Find_Data *)GetWindowLongPtr(GetParent(hwnd), GWLP_USERDATA);
    return CallWindowProc(data->editWndProc, hwnd, message, wParam, lParam);
}

static INT_PTR CALLBACK Dialog_Find_Proc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    Dialog_Find_Data * data;

    switch (msg)
    {
    case WM_INITDIALOG:
        data = (Dialog_Find_Data*)lParam;
        assert(data);
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)data);

        win::SetText(hDlg, _TR("Find"));
        SetDlgItemText(hDlg, IDC_STATIC, _TR("&Find what:"));
        SetDlgItemText(hDlg, IDC_MATCH_CASE, _TR("&Match case"));
        SetDlgItemText(hDlg, IDC_FIND_NEXT_HINT, _TR("Hint: Use the F3 key for finding again"));
        SetDlgItemText(hDlg, IDOK, _TR("Find"));
        SetDlgItemText(hDlg, IDCANCEL, _TR("Cancel"));
        if (data->searchTerm)
            SetDlgItemText(hDlg, IDC_FIND_EDIT, data->searchTerm);
        CheckDlgButton(hDlg, IDC_MATCH_CASE, data->matchCase ? BST_CHECKED : BST_UNCHECKED);
        data->editWndProc = (WNDPROC)SetWindowLongPtr(GetDlgItem(hDlg, IDC_FIND_EDIT), GWLP_WNDPROC, (LONG_PTR)Dialog_Find_Edit_Proc);
        Edit_SelectAll(GetDlgItem(hDlg, IDC_FIND_EDIT));

        CenterDialog(hDlg);
        SetFocus(GetDlgItem(hDlg, IDC_FIND_EDIT));
        return FALSE;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            data = (Dialog_Find_Data*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
            assert(data);
            data->searchTerm = win::GetText(GetDlgItem(hDlg, IDC_FIND_EDIT));
            data->matchCase = BST_CHECKED == IsDlgButtonChecked(hDlg, IDC_MATCH_CASE);
            EndDialog(hDlg, IDOK);
            return TRUE;

        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

/* Shows a 'Find' dialog and returns the new search term entered by the user
   or NULL if the search was canceled. previousSearch is the search term to
   be displayed as default. */
TCHAR * Dialog_Find(HWND hwnd, const TCHAR *previousSearch, bool *matchCase)
{
    Dialog_Find_Data data;
    data.searchTerm = (TCHAR *)previousSearch;
    data.matchCase = matchCase ? *matchCase : false;

    INT_PTR res = CreateDialogBox(IDD_DIALOG_FIND, hwnd,
                                  Dialog_Find_Proc, (LPARAM)&data);
    if (res != IDOK)
        return NULL;

    if (matchCase)
        *matchCase = data.matchCase;
    return data.searchTerm;
}

/* For passing data to/from AssociateWithPdf dialog */
struct Dialog_PdfAssociate_Data {
    bool dontAskAgain;
};

static INT_PTR CALLBACK Dialog_PdfAssociate_Proc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    Dialog_PdfAssociate_Data *  data;

    if (WM_INITDIALOG == msg)
    {
        data = (Dialog_PdfAssociate_Data*)lParam;
        assert(data);
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)data);
        win::SetText(hDlg, _TR("Associate with PDF files?"));
        SetDlgItemText(hDlg, IDC_STATIC, _TR("Make SumatraPDF default application for PDF files?"));
        SetDlgItemText(hDlg, IDC_DONT_ASK_ME_AGAIN, _TR("&Don't ask me again"));
        CheckDlgButton(hDlg, IDC_DONT_ASK_ME_AGAIN, BST_UNCHECKED);
        SetDlgItemText(hDlg, IDOK, _TR("&Yes"));
        SetDlgItemText(hDlg, IDCANCEL, _TR("&No"));

        CenterDialog(hDlg);
        SetFocus(GetDlgItem(hDlg, IDOK));
        return FALSE;
    }

    switch (msg)
    {
        case WM_COMMAND:
            data = (Dialog_PdfAssociate_Data*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
            assert(data);
            data->dontAskAgain = (BST_CHECKED == IsDlgButtonChecked(hDlg, IDC_DONT_ASK_ME_AGAIN));
            switch (LOWORD(wParam))
            {
                case IDOK:
                    EndDialog(hDlg, IDYES);
                    return TRUE;

                case IDCANCEL:
                    EndDialog(hDlg, IDNO);
                    return TRUE;

                case IDC_DONT_ASK_ME_AGAIN:
                    return TRUE;
            }
            break;
    }
    return FALSE;
}

/* Show "associate this application with PDF files" dialog.
   Returns IDYES if "Yes" button was pressed or
   IDNO if "No" button was pressed.
   Returns the state of "don't ask me again" checkbox" in <dontAskAgain> */
INT_PTR Dialog_PdfAssociate(HWND hwnd, bool *dontAskAgainOut)
{
    assert(dontAskAgainOut);

    Dialog_PdfAssociate_Data data;
    INT_PTR res = CreateDialogBox(IDD_DIALOG_PDF_ASSOCIATE, hwnd,
                                  Dialog_PdfAssociate_Proc, (LPARAM)&data);
    if (dontAskAgainOut)
        *dontAskAgainOut = data.dontAskAgain;
    return res;
}

/* For passing data to/from ChangeLanguage dialog */
struct Dialog_ChangeLanguage_Data {
    int langId;
};

static INT_PTR CALLBACK Dialog_ChangeLanguage_Proc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    Dialog_ChangeLanguage_Data *  data;
    HWND                          langList;

    if (WM_INITDIALOG == msg)
    {
        DIALOG_SIZER_START(sz)
            DIALOG_SIZER_ENTRY(IDOK, DS_MoveX | DS_MoveY)
            DIALOG_SIZER_ENTRY(IDCANCEL, DS_MoveX | DS_MoveY)
            DIALOG_SIZER_ENTRY(IDC_CHANGE_LANG_LANG_LIST, DS_SizeY | DS_SizeX)
        DIALOG_SIZER_END()
        DialogSizer_Set(hDlg, sz, TRUE);

        data = (Dialog_ChangeLanguage_Data*)lParam;
        assert(data);
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)data);
        // for non-latin languages this depends on the correct fonts being installed,
        // otherwise all the user will see are squares
        win::SetText(hDlg, _TR("Change Language"));
        langList = GetDlgItem(hDlg, IDC_CHANGE_LANG_LANG_LIST);
        for (int i = 0; Trans::GetLanguageCode(i) != NULL; i++) {
            ScopedMem<TCHAR> langName(Trans::GetLanguageName(i));
            assert(langName);
            ListBox_AppendString_NoSort(langList, langName);
        }
        ListBox_SetCurSel(langList, data->langId);
        // the language list is meant to be layed out left-to-right
        ToggleWindowStyle(langList, WS_EX_LAYOUTRTL, false, GWL_EXSTYLE);
        SetDlgItemText(hDlg, IDOK, _TR("OK"));
        SetDlgItemText(hDlg, IDCANCEL, _TR("Cancel"));

        CenterDialog(hDlg);
        SetFocus(langList);
        return FALSE;
    }

    switch (msg)
    {
        case WM_COMMAND:
            data = (Dialog_ChangeLanguage_Data*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
            assert(data);
            if (HIWORD(wParam) == LBN_DBLCLK) {
                assert(IDC_CHANGE_LANG_LANG_LIST == LOWORD(wParam));
                langList = GetDlgItem(hDlg, IDC_CHANGE_LANG_LANG_LIST);
                assert(langList == (HWND)lParam);
                data->langId = ListBox_GetCurSel(langList);
                EndDialog(hDlg, IDOK);
                return FALSE;
            }
            switch (LOWORD(wParam))
            {
                case IDOK:
                    langList = GetDlgItem(hDlg, IDC_CHANGE_LANG_LANG_LIST);
                    data->langId = ListBox_GetCurSel(langList);
                    EndDialog(hDlg, IDOK);
                    return TRUE;

                case IDCANCEL:
                    EndDialog(hDlg, IDCANCEL);
                    return TRUE;
            }
            break;
    }

    return FALSE;
}

/* Show "Change Language" dialog.
   Returns language id (as stored in g_langs[]._langId) or -1 if the user 
   choses 'cancel' */
int Dialog_ChangeLanguge(HWND hwnd, int currLangId)
{
    Dialog_ChangeLanguage_Data data;
    data.langId = currLangId;

    INT_PTR res = CreateDialogBox(IDD_DIALOG_CHANGE_LANGUAGE, hwnd,
                                  Dialog_ChangeLanguage_Proc, (LPARAM)&data);
    if (IDCANCEL == res)
        return -1;
    return data.langId;
}

/* For passing data to/from 'new version available' dialog */
struct Dialog_NewVersion_Data {
    const TCHAR *currVersion;
    const TCHAR *newVersion;
    bool skipThisVersion;
};

static INT_PTR CALLBACK Dialog_NewVersion_Proc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    Dialog_NewVersion_Data *  data;
    TCHAR *txt;

    if (WM_INITDIALOG == msg)
    {
        data = (Dialog_NewVersion_Data*)lParam;
        assert(NULL != data);
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)data);
        win::SetText(hDlg, _TR("SumatraPDF Update"));

        txt = str::Format(_TR("You have version %s"), data->currVersion);
        SetDlgItemText(hDlg, IDC_YOU_HAVE, txt);
        free((void*)txt);

        txt = str::Format(_TR("New version %s is available. Download new version?"), data->newVersion);
        SetDlgItemText(hDlg, IDC_NEW_AVAILABLE, txt);
        free(txt);

        SetDlgItemText(hDlg, IDC_SKIP_THIS_VERSION, _TR("&Skip this version"));
        CheckDlgButton(hDlg, IDC_SKIP_THIS_VERSION, BST_UNCHECKED);
        SetDlgItemText(hDlg, IDOK, _TR("Download"));
        SetDlgItemText(hDlg, IDCANCEL, _TR("&No, thanks"));

        CenterDialog(hDlg);
        SetFocus(GetDlgItem(hDlg, IDOK));
        return FALSE;
    }

    switch (msg)
    {
        case WM_COMMAND:
            data = (Dialog_NewVersion_Data*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
            assert(data);
            data->skipThisVersion = false;
            switch (LOWORD(wParam))
            {
                case IDOK:
                    if (BST_CHECKED == IsDlgButtonChecked(hDlg, IDC_SKIP_THIS_VERSION))
                        data->skipThisVersion = true;
                    EndDialog(hDlg, IDYES);
                    return TRUE;

                case IDCANCEL:
                    if (BST_CHECKED == IsDlgButtonChecked(hDlg, IDC_SKIP_THIS_VERSION))
                        data->skipThisVersion = true;
                    EndDialog(hDlg, IDNO);
                    return TRUE;

                case IDC_SKIP_THIS_VERSION:
                    return TRUE;
            }
            break;
    }
    return FALSE;
}

INT_PTR Dialog_NewVersionAvailable(HWND hwnd, const TCHAR *currentVersion, const TCHAR *newVersion, bool *skipThisVersion)
{
    Dialog_NewVersion_Data data;
    data.currVersion = currentVersion;
    data.newVersion = newVersion;
    data.skipThisVersion = false;

    INT_PTR res = CreateDialogBox(IDD_DIALOG_NEW_VERSION, hwnd,
                                  Dialog_NewVersion_Proc, (LPARAM)&data);
    if (skipThisVersion)
        *skipThisVersion = data.skipThisVersion;

    return res;
}

static float gItemZoom[] = { ZOOM_FIT_PAGE, ZOOM_FIT_WIDTH, ZOOM_FIT_CONTENT, 0,
    6400.0, 3200.0, 1600.0, 800.0, 400.0, 200.0, 150.0, 125.0, 100.0, 50.0, 25.0, 12.5, 8.33f };

static void SetupZoomComboBox(HWND hDlg, UINT idComboBox, bool forChm, float currZoom)
{
    if (!forChm) {
        SendDlgItemMessage(hDlg, idComboBox, CB_ADDSTRING, 0, (LPARAM)_TR("Fit Page"));
        SendDlgItemMessage(hDlg, idComboBox, CB_ADDSTRING, 0, (LPARAM)_TR("Fit Width"));
        SendDlgItemMessage(hDlg, idComboBox, CB_ADDSTRING, 0, (LPARAM)_TR("Fit Content"));
        SendDlgItemMessage(hDlg, idComboBox, CB_ADDSTRING, 0, (LPARAM)_T("-"));
        SendDlgItemMessage(hDlg, idComboBox, CB_ADDSTRING, 0, (LPARAM)_T("6400%"));
        SendDlgItemMessage(hDlg, idComboBox, CB_ADDSTRING, 0, (LPARAM)_T("3200%"));
        SendDlgItemMessage(hDlg, idComboBox, CB_ADDSTRING, 0, (LPARAM)_T("1600%"));
    }
    SendDlgItemMessage(hDlg, idComboBox, CB_ADDSTRING, 0, (LPARAM)_T("800%"));
    SendDlgItemMessage(hDlg, idComboBox, CB_ADDSTRING, 0, (LPARAM)_T("400%"));
    SendDlgItemMessage(hDlg, idComboBox, CB_ADDSTRING, 0, (LPARAM)_T("200%"));
    SendDlgItemMessage(hDlg, idComboBox, CB_ADDSTRING, 0, (LPARAM)_T("150%"));
    SendDlgItemMessage(hDlg, idComboBox, CB_ADDSTRING, 0, (LPARAM)_T("125%"));
    SendDlgItemMessage(hDlg, idComboBox, CB_ADDSTRING, 0, (LPARAM)_T("100%"));
    SendDlgItemMessage(hDlg, idComboBox, CB_ADDSTRING, 0, (LPARAM)_T("50%"));
    SendDlgItemMessage(hDlg, idComboBox, CB_ADDSTRING, 0, (LPARAM)_T("25%"));
    if (!forChm) {
        SendDlgItemMessage(hDlg, idComboBox, CB_ADDSTRING, 0, (LPARAM)_T("12.5%"));
        SendDlgItemMessage(hDlg, idComboBox, CB_ADDSTRING, 0, (LPARAM)_T("8.33%"));
    }
    int first = forChm ? 7 : 0;
    int last = forChm ? dimof(gItemZoom) - 2 : dimof(gItemZoom);
    for (int i = first; i < last; i++) {
        if (gItemZoom[i] == currZoom)
            SendDlgItemMessage(hDlg, idComboBox, CB_SETCURSEL, i - first, 0);
    }

    if (SendDlgItemMessage(hDlg, idComboBox, CB_GETCURSEL, 0, 0) == -1) {
        TCHAR *customZoom = str::Format(_T("%.0f%%"), currZoom);
        SetDlgItemText(hDlg, idComboBox, customZoom);
        free(customZoom);
    }
}

static float GetZoomComboBoxValue(HWND hDlg, UINT idComboBox, bool forChm, float defaultZoom)
{
    float newZoom = defaultZoom;

    int idx = ComboBox_GetCurSel(GetDlgItem(hDlg, idComboBox));
    if (idx == -1) {
        ScopedMem<TCHAR> customZoom(win::GetText(GetDlgItem(hDlg, idComboBox)));
        float zoom = (float)_tstof(customZoom);
        if (zoom >= ZOOM_MIN && zoom <= ZOOM_MAX)
            newZoom = zoom;
    } else {
        if (forChm)
            idx += 7;

        if (0 != gItemZoom[idx])
            newZoom = gItemZoom[idx];
    }

    return newZoom;
}

struct Dialog_CustomZoom_Data {
    float zoomArg;
    float zoomResult;
    bool  forChm;
};

static INT_PTR CALLBACK Dialog_CustomZoom_Proc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    Dialog_CustomZoom_Data *data;

    switch (msg)
    {
    case WM_INITDIALOG:
        data = (Dialog_CustomZoom_Data *)lParam;
        assert(data);
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)data);

        SetupZoomComboBox(hDlg, IDC_DEFAULT_ZOOM, data->forChm, data->zoomArg);

        win::SetText(hDlg, _TR("Zoom factor"));
        SetDlgItemText(hDlg, IDC_STATIC, _TR("&Magnification:"));
        SetDlgItemText(hDlg, IDOK, _TR("Zoom"));
        SetDlgItemText(hDlg, IDCANCEL, _TR("Cancel"));

        CenterDialog(hDlg);
        SetFocus(GetDlgItem(hDlg, IDC_DEFAULT_ZOOM));
        return FALSE;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            data = (Dialog_CustomZoom_Data *)GetWindowLongPtr(hDlg, GWLP_USERDATA);
            assert(data);
            data->zoomResult = GetZoomComboBoxValue(hDlg, IDC_DEFAULT_ZOOM, data->forChm, data->zoomArg);
            EndDialog(hDlg, IDOK);
            return TRUE;

        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

bool Dialog_CustomZoom(HWND hwnd, bool forChm, float *currZoomInOut)
{
    Dialog_CustomZoom_Data data;
    data.forChm = forChm;
    data.zoomArg = *currZoomInOut;
    INT_PTR res = CreateDialogBox(IDD_DIALOG_CUSTOM_ZOOM, hwnd,
                           Dialog_CustomZoom_Proc, (LPARAM)&data);
    if (res == IDCANCEL)
        return false;
    
    *currZoomInOut = data.zoomResult;
    return true;
}

static INT_PTR CALLBACK Dialog_Settings_Proc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    SerializableGlobalPrefs *prefs;

    switch (msg)
    {
    case WM_INITDIALOG:
        prefs = (SerializableGlobalPrefs *)lParam;
        assert(prefs);
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)prefs);

        // Fill the page layouts into the select box
        SendDlgItemMessage(hDlg, IDC_DEFAULT_LAYOUT, CB_ADDSTRING, 0, (LPARAM)_TR("Automatic"));
        SendDlgItemMessage(hDlg, IDC_DEFAULT_LAYOUT, CB_ADDSTRING, 0, (LPARAM)_TR("Single Page"));
        SendDlgItemMessage(hDlg, IDC_DEFAULT_LAYOUT, CB_ADDSTRING, 0, (LPARAM)_TR("Facing"));
        SendDlgItemMessage(hDlg, IDC_DEFAULT_LAYOUT, CB_ADDSTRING, 0, (LPARAM)_TR("Book View"));
        SendDlgItemMessage(hDlg, IDC_DEFAULT_LAYOUT, CB_ADDSTRING, 0, (LPARAM)_TR("Continuous"));
        SendDlgItemMessage(hDlg, IDC_DEFAULT_LAYOUT, CB_ADDSTRING, 0, (LPARAM)_TR("Continuous Facing"));
        SendDlgItemMessage(hDlg, IDC_DEFAULT_LAYOUT, CB_ADDSTRING, 0, (LPARAM)_TR("Continuous Book View"));
        SendDlgItemMessage(hDlg, IDC_DEFAULT_LAYOUT, CB_SETCURSEL, prefs->defaultDisplayMode - DM_FIRST, 0);

        SetupZoomComboBox(hDlg, IDC_DEFAULT_ZOOM, false, prefs->defaultZoom);

        CheckDlgButton(hDlg, IDC_DEFAULT_SHOW_TOC, prefs->tocVisible ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_GLOBAL_PREFS_ONLY, !prefs->globalPrefsOnly ? BST_CHECKED : BST_UNCHECKED);
        EnableWindow(GetDlgItem(hDlg, IDC_GLOBAL_PREFS_ONLY), prefs->rememberOpenedFiles);
        CheckDlgButton(hDlg, IDC_AUTO_UPDATE_CHECKS, prefs->enableAutoUpdate ? BST_CHECKED : BST_UNCHECKED);
        EnableWindow(GetDlgItem(hDlg, IDC_AUTO_UPDATE_CHECKS), HasPermission(Perm_InternetAccess));
        CheckDlgButton(hDlg, IDC_REMEMBER_OPENED_FILES, prefs->rememberOpenedFiles ? BST_CHECKED : BST_UNCHECKED);
        if (IsExeAssociatedWithPdfExtension()) {
            SetDlgItemText(hDlg, IDC_SET_DEFAULT_READER, _TR("SumatraPDF is your default PDF reader"));
            EnableWindow(GetDlgItem(hDlg, IDC_SET_DEFAULT_READER), FALSE);
        } else if (IsRunningInPortableMode()) {
            SetDlgItemText(hDlg, IDC_SET_DEFAULT_READER, _TR("Default PDF reader can't be changed in portable mode"));
            EnableWindow(GetDlgItem(hDlg, IDC_SET_DEFAULT_READER), FALSE);
        } else {
            SetDlgItemText(hDlg, IDC_SET_DEFAULT_READER, _TR("Make SumatraPDF my default PDF reader"));
            EnableWindow(GetDlgItem(hDlg, IDC_SET_DEFAULT_READER), HasPermission(Perm_RegistryAccess));
        }

        win::SetText(hDlg, _TR("SumatraPDF Options"));
        SetDlgItemText(hDlg, IDC_SECTION_VIEW, _TR("View"));
        SetDlgItemText(hDlg, IDC_DEFAULT_LAYOUT_LABEL, _TR("Default &Layout:"));
        SetDlgItemText(hDlg, IDC_DEFAULT_ZOOM_LABEL, _TR("Default &Zoom:"));
        SetDlgItemText(hDlg, IDC_DEFAULT_SHOW_TOC, _TR("Show the &bookmarks sidebar when available"));
        SetDlgItemText(hDlg, IDC_GLOBAL_PREFS_ONLY, _TR("&Remember these settings for each document"));
        SetDlgItemText(hDlg, IDC_SECTION_ADVANCED, _TR("Advanced"));
        SetDlgItemText(hDlg, IDC_AUTO_UPDATE_CHECKS, _TR("Automatically check for &updates"));
        SetDlgItemText(hDlg, IDC_REMEMBER_OPENED_FILES, _TR("Remember &opened files"));
        SetDlgItemText(hDlg, IDOK, _TR("OK"));
        SetDlgItemText(hDlg, IDCANCEL, _TR("Cancel"));

        if (prefs->enableTeXEnhancements && HasPermission(Perm_DiskAccess)) {
            // Fit the additional section into the dialog
            // (this should rather happen in SumatraPDF.rc, but the resource
            // editor tends to overwrite conditional stuff which isn't its own)
            RectI rc = WindowRect(GetDlgItem(hDlg, IDC_SECTION_INVERSESEARCH));
            UINT addHeight = rc.dy + 8;
            rc = WindowRect(hDlg);
            MoveWindow(hDlg, rc.x, rc.y, rc.dx, rc.dy + addHeight, TRUE);

            HWND hItem = GetDlgItem(hDlg, IDOK);
            rc = MapRectToWindow(ClientRect(hItem), hItem, hDlg);
            MoveWindow(hItem, rc.x, rc.y + addHeight, rc.dx, rc.dy, TRUE);

            hItem = GetDlgItem(hDlg, IDCANCEL);
            rc = MapRectToWindow(ClientRect(hItem), hItem, hDlg);
            MoveWindow(hItem, rc.x, rc.y + addHeight, rc.dx, rc.dy, TRUE);

            SetDlgItemText(hDlg, IDC_SECTION_INVERSESEARCH, _TR("Set inverse search command-line"));
            SetDlgItemText(hDlg, IDC_CMDLINE_LABEL, _TR("Enter the command-line to invoke when you double-click on the PDF document:"));
            // Fill the combo with the list of possible inverse search commands
            TCHAR *inverseSearch = AutoDetectInverseSearchCommands(GetDlgItem(hDlg, IDC_CMDLINE));
            // Try to select a correct default when first showing this dialog
            if (!prefs->inverseSearchCmdLine)
                prefs->inverseSearchCmdLine = inverseSearch;
            // Find the index of the active command line    
            LRESULT ind = SendMessage(GetDlgItem(hDlg, IDC_CMDLINE), CB_FINDSTRINGEXACT, -1, (LPARAM) prefs->inverseSearchCmdLine);
            if (CB_ERR == ind) {            
                // if no existing command was selected then set the user custom command in the combo
                ComboBox_AddItemData(GetDlgItem(hDlg, IDC_CMDLINE), prefs->inverseSearchCmdLine);
                SetDlgItemText(hDlg, IDC_CMDLINE, prefs->inverseSearchCmdLine);
            }
            else {
                // select the active command
                SendMessage(GetDlgItem(hDlg, IDC_CMDLINE), CB_SETCURSEL, (WPARAM) ind , 0);
            }
            if (prefs->inverseSearchCmdLine == inverseSearch)
                prefs->inverseSearchCmdLine = NULL;
            free(inverseSearch);
        }
        else
        {
            ShowWindow(GetDlgItem(hDlg, IDC_SECTION_INVERSESEARCH), SW_HIDE);
            ShowWindow(GetDlgItem(hDlg, IDC_CMDLINE_LABEL), SW_HIDE);
            ShowWindow(GetDlgItem(hDlg, IDC_CMDLINE), SW_HIDE);
        }

        CenterDialog(hDlg);
        SetFocus(GetDlgItem(hDlg, IDC_DEFAULT_LAYOUT));
        return FALSE;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            prefs = (SerializableGlobalPrefs *)GetWindowLongPtr(hDlg, GWLP_USERDATA);
            assert(prefs);
            prefs->defaultDisplayMode = (DisplayMode)(SendDlgItemMessage(hDlg, IDC_DEFAULT_LAYOUT, CB_GETCURSEL, 0, 0) + DM_FIRST);
            prefs->defaultZoom = GetZoomComboBoxValue(hDlg, IDC_DEFAULT_ZOOM, false, prefs->defaultZoom);

            prefs->tocVisible = (BST_CHECKED == IsDlgButtonChecked(hDlg, IDC_DEFAULT_SHOW_TOC));
            prefs->globalPrefsOnly = (BST_CHECKED != IsDlgButtonChecked(hDlg, IDC_GLOBAL_PREFS_ONLY));
            prefs->enableAutoUpdate = (BST_CHECKED == IsDlgButtonChecked(hDlg, IDC_AUTO_UPDATE_CHECKS));
            prefs->rememberOpenedFiles = (BST_CHECKED == IsDlgButtonChecked(hDlg, IDC_REMEMBER_OPENED_FILES));
            if (prefs->enableTeXEnhancements && HasPermission(Perm_DiskAccess)) {
                free(prefs->inverseSearchCmdLine);
                prefs->inverseSearchCmdLine = win::GetText(GetDlgItem(hDlg, IDC_CMDLINE));
            }
            EndDialog(hDlg, IDOK);
            return TRUE;

        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;

        case IDC_REMEMBER_OPENED_FILES:
            {
                bool rememberOpenedFiles = (BST_CHECKED == IsDlgButtonChecked(hDlg, IDC_REMEMBER_OPENED_FILES));
                EnableWindow(GetDlgItem(hDlg, IDC_GLOBAL_PREFS_ONLY), rememberOpenedFiles);
            }
            return TRUE;

        case IDC_DEFAULT_SHOW_TOC:
        case IDC_GLOBAL_PREFS_ONLY:
        case IDC_AUTO_UPDATE_CHECKS:
            return TRUE;

        case IDC_SET_DEFAULT_READER:
            if (!HasPermission(Perm_RegistryAccess))
                return TRUE;
            AssociateExeWithPdfExtension();
            if (IsExeAssociatedWithPdfExtension()) {
                SetDlgItemText(hDlg, IDC_SET_DEFAULT_READER, _TR("SumatraPDF is your default PDF reader"));
                EnableWindow(GetDlgItem(hDlg, IDC_SET_DEFAULT_READER), FALSE);
                SendMessage(hDlg, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(hDlg, IDOK), TRUE);
            }
            else {
                SetDlgItemText(hDlg, IDC_SET_DEFAULT_READER, _TR("SumatraPDF should now be your default PDF reader"));
            }
            return TRUE;
        }
        break;
    }
    return FALSE;
}

INT_PTR Dialog_Settings(HWND hwnd, SerializableGlobalPrefs *prefs)
{
    return CreateDialogBox(IDD_DIALOG_SETTINGS, hwnd,
                           Dialog_Settings_Proc, (LPARAM)prefs);
}

static INT_PTR CALLBACK Sheet_Print_Advanced_Proc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    Print_Advanced_Data *data;

    switch (msg)
    {
    case WM_INITDIALOG:
        data = (Print_Advanced_Data *)((PROPSHEETPAGE *)lParam)->lParam;
        assert(data);
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)data);

        SetDlgItemText(hDlg, IDC_SECTION_PRINT_RANGE, _TR("Print range"));
        SetDlgItemText(hDlg, IDC_PRINT_RANGE_ALL, _TR("&All selected pages"));
        SetDlgItemText(hDlg, IDC_PRINT_RANGE_EVEN, _TR("&Even pages only"));
        SetDlgItemText(hDlg, IDC_PRINT_RANGE_ODD, _TR("&Odd pages only"));
        SetDlgItemText(hDlg, IDC_SECTION_PRINT_SCALE, _TR("Page scaling"));
        SetDlgItemText(hDlg, IDC_PRINT_SCALE_SHRINK, _TR("&Shrink pages to printable area (if necessary)"));
        SetDlgItemText(hDlg, IDC_PRINT_SCALE_FIT, _TR("&Fit pages to printable area"));
        SetDlgItemText(hDlg, IDC_PRINT_SCALE_NONE, _TR("&Use original page sizes"));

        CheckRadioButton(hDlg, IDC_PRINT_RANGE_ALL, IDC_PRINT_RANGE_ODD,
            data->range == PrintRangeEven ? IDC_PRINT_RANGE_EVEN :
            data->range == PrintRangeOdd ? IDC_PRINT_RANGE_ODD : IDC_PRINT_RANGE_ALL);
        CheckRadioButton(hDlg, IDC_PRINT_SCALE_SHRINK, IDC_PRINT_SCALE_FIT,
            data->scale == PrintScaleFit ? IDC_PRINT_SCALE_FIT :
            data->scale == PrintScaleShrink ? IDC_PRINT_SCALE_SHRINK : IDC_PRINT_SCALE_NONE);

        return FALSE;

    case WM_NOTIFY:
        if (((LPNMHDR)lParam)->code == PSN_APPLY) {
            data = (Print_Advanced_Data *)GetWindowLongPtr(hDlg, GWLP_USERDATA);
            assert(data);
            if (IsDlgButtonChecked(hDlg, IDC_PRINT_RANGE_EVEN))
                data->range = PrintRangeEven;
            else if (IsDlgButtonChecked(hDlg, IDC_PRINT_RANGE_ODD))
                data->range = PrintRangeOdd;
            else
                data->range = PrintRangeAll;
            if (IsDlgButtonChecked(hDlg, IDC_PRINT_SCALE_FIT))
                data->scale = PrintScaleFit;
            else if (IsDlgButtonChecked(hDlg, IDC_PRINT_SCALE_SHRINK))
                data->scale = PrintScaleShrink;
            else
                data->scale = PrintScaleNone;
            return TRUE;
        }
        break;
    }
    return FALSE;
}

HPROPSHEETPAGE CreatePrintAdvancedPropSheet(Print_Advanced_Data *data, ScopedMem<DLGTEMPLATE>& dlgTemplate)
{
    PROPSHEETPAGE psp;
    ZeroMemory(&psp, sizeof(PROPSHEETPAGE));

    psp.dwSize = sizeof(PROPSHEETPAGE);
    psp.dwFlags = PSP_USETITLE | PSP_PREMATURE;
    psp.pszTemplate = MAKEINTRESOURCE(IDD_PROPSHEET_PRINT_ADVANCED);
    psp.pfnDlgProc = Sheet_Print_Advanced_Proc;
    psp.lParam = (LPARAM)data;
    psp.pszTitle = _TR("Advanced");

    if (IsUIRightToLeft()) {
        dlgTemplate.Set(GetRtLDlgTemplate(IDD_PROPSHEET_PRINT_ADVANCED));
        psp.pResource = dlgTemplate.Get();
        psp.dwFlags |= PSP_DLGINDIRECT;
    }

    return CreatePropertySheetPage(&psp);
}

struct Dialog_AddFav_Data {
    const TCHAR *pageNo;
    TCHAR *favName;
};

static INT_PTR CALLBACK Dialog_AddFav_Proc(HWND hDlg, UINT msg, WPARAM wParam,
    LPARAM lParam)
{
    if (WM_INITDIALOG == msg) {
        Dialog_AddFav_Data *data = (Dialog_AddFav_Data *)lParam;
        assert(data);
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)data);
        win::SetText(hDlg, _TR("Add Favorite"));
        ScopedMem<TCHAR> s(str::Format(_TR("Add page %s to favorites with (optional) name:"), data->pageNo));
        SetDlgItemText(hDlg, IDC_ADD_PAGE_STATIC, s);
        SetDlgItemText(hDlg, IDOK, _TR("OK"));
        SetDlgItemText(hDlg, IDCANCEL, _TR("Cancel"));
        if (data->favName) {
            SetDlgItemText(hDlg, IDC_FAV_NAME_EDIT, data->favName);
            Edit_SelectAll(GetDlgItem(hDlg, IDC_FAV_NAME_EDIT));
        }
        CenterDialog(hDlg);
        SetFocus(GetDlgItem(hDlg, IDC_FAV_NAME_EDIT));
        return FALSE;
    }

    if (WM_COMMAND == msg) {
        Dialog_AddFav_Data *data = (Dialog_AddFav_Data *)GetWindowLongPtr(hDlg, GWLP_USERDATA);
        assert(data);
        WORD cmd = LOWORD(wParam);
        if (IDOK == cmd) {
            ScopedMem<TCHAR> name(win::GetText(GetDlgItem(hDlg, IDC_FAV_NAME_EDIT)));
            str::TrimWS(name);
            if (!str::IsEmpty(name.Get()))
                data->favName = name.StealData();
            else
                data->favName = NULL;
            EndDialog(hDlg, IDOK);
            return TRUE;
        } else if (IDCANCEL == cmd) {
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
    }

    return FALSE;
}

// pageNo is the page we're adding to favorites
// returns true if the user wants to add a favorite.
// favName is the name the user wants the favorite to have
// (passing in a non-NULL favName will use it as default name)
bool Dialog_AddFavorite(HWND hwnd, const TCHAR *pageNo, ScopedMem<TCHAR>& favName)
{
    Dialog_AddFav_Data data;
    data.pageNo = pageNo;
    data.favName = favName;

    INT_PTR res = CreateDialogBox(IDD_DIALOG_FAV_ADD, hwnd,
                                  Dialog_AddFav_Proc, (LPARAM)&data);
    if (IDCANCEL == res) {
        assert(data.favName == favName);
        return false;
    }

    assert(data.favName != favName || !data.favName);
    favName.Set(data.favName);
    return true;
}
