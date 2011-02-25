/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "DisplayModel.h"
#include "AppPrefs.h"
#include "SumatraDialogs.h"
#include "WindowInfo.h"
#include "AppTools.h"
#include "Resource.h"

#include "win_util.h"
#include "WinUtil.hpp"
#include "dialogsizer.h"
#include "LangMenuDef.h"
#include "translations.h"

/* For passing data to/from GetPassword dialog */
typedef struct {
    const TCHAR *  fileName;   /* name of the file for which we need the password */
    TCHAR *        pwdOut;     /* password entered by the user */
    bool *         remember;   /* remember the password (encrypted) or ask again? */
} Dialog_GetPassword_Data;

static INT_PTR CALLBACK Dialog_GetPassword_Proc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    Dialog_GetPassword_Data *  data;

    if (WM_INITDIALOG == message)
    {
        data = (Dialog_GetPassword_Data*)lParam;
        assert(data);
        assert(data->fileName);
        assert(!data->pwdOut);

        win_set_text(hDlg, _TR("Enter password"));
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)data);
        EnableWindow(GetDlgItem(hDlg, IDC_REMEMBER_PASSWORD), data->remember != NULL);

        TCHAR *txt = tstr_printf(_TR("Enter password for %s"), data->fileName);
        SetDlgItemText(hDlg, IDC_GET_PASSWORD_LABEL, txt);
        free(txt);
        SetDlgItemText(hDlg, IDC_GET_PASSWORD_EDIT, _T(""));
        SetDlgItemText(hDlg, IDC_STATIC, _TR("&Password:"));
        SetDlgItemText(hDlg, IDC_REMEMBER_PASSWORD, _TR("&Remember the password for this document"));
        SetDlgItemText(hDlg, IDOK, _TR("OK"));
        SetDlgItemText(hDlg, IDCANCEL, _TR("Cancel"));

        CenterDialog(hDlg);
        SetFocus(GetDlgItem(hDlg, IDC_GET_PASSWORD_EDIT));
        return FALSE;
    }

    switch (message)
    {
        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case IDOK:
                    data = (Dialog_GetPassword_Data*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
                    assert(data);
                    data->pwdOut = win_get_text(GetDlgItem(hDlg, IDC_GET_PASSWORD_EDIT));
                    if (data->remember)
                        *data->remember = BST_CHECKED == IsDlgButtonChecked(hDlg, IDC_REMEMBER_PASSWORD);
                    EndDialog(hDlg, DIALOG_OK_PRESSED);
                    return TRUE;

                case IDCANCEL:
                    EndDialog(hDlg, DIALOG_CANCEL_PRESSED);
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
    
    assert(fileName);
    if (!fileName) return NULL;

    data.fileName = fileName;
    data.remember = rememberPassword;

    INT_PTR dialogResult = DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_DIALOG_GET_PASSWORD), hwndParent, Dialog_GetPassword_Proc, (LPARAM)&data);
    if (DIALOG_OK_PRESSED != dialogResult) {
        free((void*)data.pwdOut);
        return NULL;
    }

    return data.pwdOut;
}

/* For passing data to/from GoToPage dialog */
typedef struct {
    int     currPageNo;      /* currently shown page number */
    int     pageCount;       /* total number of pages */
    int     pageEnteredOut;  /* page number entered by user */
} Dialog_GoToPage_Data;

static INT_PTR CALLBACK Dialog_GoToPage_Proc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    HWND                    editPageNo;
    TCHAR *                 newPageNoTxt;
    Dialog_GoToPage_Data *  data;

    if (WM_INITDIALOG == message)
    {
        data = (Dialog_GoToPage_Data*)lParam;
        assert(data);
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)data);
        assert(INVALID_PAGE_NO != data->currPageNo);
        assert(data->pageCount >= 1);
        win_set_text(hDlg, _TR("Go to page"));

        newPageNoTxt = tstr_printf(_T("%d"), data->currPageNo);
        SetDlgItemText(hDlg, IDC_GOTO_PAGE_EDIT, newPageNoTxt);
        free(newPageNoTxt);
        newPageNoTxt = tstr_printf(_TR("(of %d)"), data->pageCount);
        SetDlgItemText(hDlg, IDC_GOTO_PAGE_LABEL_OF, newPageNoTxt);
        free(newPageNoTxt);

        editPageNo = GetDlgItem(hDlg, IDC_GOTO_PAGE_EDIT);
        Edit_SelectAll(editPageNo);
        SetDlgItemText(hDlg, IDC_STATIC, _TR("&Go to page:"));
        SetDlgItemText(hDlg, IDOK, _TR("Go to page"));
        SetDlgItemText(hDlg, IDCANCEL, _TR("Cancel"));

        CenterDialog(hDlg);
        SetFocus(editPageNo);
        return FALSE;
    }

    switch (message)
    {
        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case IDOK:
                    data = (Dialog_GoToPage_Data*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
                    assert(data);
                    data->pageEnteredOut = INVALID_PAGE_NO;
                    editPageNo = GetDlgItem(hDlg, IDC_GOTO_PAGE_EDIT);
                    newPageNoTxt = win_get_text(editPageNo);
                    if (newPageNoTxt) {
                        data->pageEnteredOut = _ttoi(newPageNoTxt);
                        free((void*)newPageNoTxt);
                    }
                    EndDialog(hDlg, DIALOG_OK_PRESSED);
                    return TRUE;

                case IDCANCEL:
                    EndDialog(hDlg, DIALOG_CANCEL_PRESSED);
                    return TRUE;
            }
            break;
    }
    return FALSE;
}

/* Shows a 'go to page' dialog and returns a page number entered by the user
   or INVALID_PAGE_NO if user clicked "cancel" button, entered invalid
   page number or there was an error. */
int Dialog_GoToPage(WindowInfo *win)
{
    Dialog_GoToPage_Data    data;
    
    assert(win);
    if (!win) return INVALID_PAGE_NO;

    data.currPageNo = win->dm->currentPageNo();
    data.pageCount = win->dm->pageCount();
    INT_PTR dialogResult = DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_DIALOG_GOTO_PAGE), win->hwndFrame, Dialog_GoToPage_Proc, (LPARAM)&data);
    if (DIALOG_OK_PRESSED == dialogResult) {
        if (win->dm->validPageNo(data.pageEnteredOut)) {
            return data.pageEnteredOut;
        }
    }
    return INVALID_PAGE_NO;
}

/* For passing data to/from Find dialog */
typedef struct {
    TCHAR * searchTerm;
    bool    matchCase;
} Dialog_Find_Data;

static INT_PTR CALLBACK Dialog_Find_Proc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    Dialog_Find_Data * data;

    switch (message)
    {
    case WM_INITDIALOG:
        data = (Dialog_Find_Data*)lParam;
        assert(data);
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)data);

        win_set_text(hDlg, _TR("Find"));
        SetDlgItemText(hDlg, IDC_STATIC, _TR("&Find what:"));
        SetDlgItemText(hDlg, IDC_MATCH_CASE, _TR("&Match case"));
        SetDlgItemText(hDlg, IDC_FIND_NEXT_HINT, _TR("Hint: Use the F3 key for finding again"));
        SetDlgItemText(hDlg, IDOK, _TR("Find"));
        SetDlgItemText(hDlg, IDCANCEL, _TR("Cancel"));
        if (data->searchTerm)
            SetDlgItemText(hDlg, IDC_FIND_EDIT, data->searchTerm);
        CheckDlgButton(hDlg, IDC_MATCH_CASE, data->matchCase ? BST_CHECKED : BST_UNCHECKED);
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
            data->searchTerm = win_get_text(GetDlgItem(hDlg, IDC_FIND_EDIT));
            data->matchCase = BST_CHECKED == IsDlgButtonChecked(hDlg, IDC_MATCH_CASE);
            EndDialog(hDlg, DIALOG_OK_PRESSED);
            return TRUE;

        case IDCANCEL:
            EndDialog(hDlg, DIALOG_CANCEL_PRESSED);
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

    INT_PTR dialogResult = DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_DIALOG_FIND), hwnd, Dialog_Find_Proc, (LPARAM)&data);
    if (dialogResult != DIALOG_OK_PRESSED)
        return NULL;

    if (matchCase)
        *matchCase = data.matchCase;
    return data.searchTerm;
}

/* For passing data to/from AssociateWithPdf dialog */
typedef struct {
    BOOL    dontAskAgain;
} Dialog_PdfAssociate_Data;

static INT_PTR CALLBACK Dialog_PdfAssociate_Proc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    Dialog_PdfAssociate_Data *  data;

    if (WM_INITDIALOG == message)
    {
        data = (Dialog_PdfAssociate_Data*)lParam;
        assert(data);
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)data);
        win_set_text(hDlg, _TR("Associate with PDF files?"));
        SetDlgItemText(hDlg, IDC_STATIC, _TR("Make SumatraPDF default application for PDF files?"));
        SetDlgItemText(hDlg, IDC_DONT_ASK_ME_AGAIN, _TR("&Don't ask me again"));
        CheckDlgButton(hDlg, IDC_DONT_ASK_ME_AGAIN, BST_UNCHECKED);
        SetDlgItemText(hDlg, IDOK, _TR("&Yes"));
        SetDlgItemText(hDlg, IDCANCEL, _TR("&No"));

        CenterDialog(hDlg);
        SetFocus(GetDlgItem(hDlg, IDOK));
        return FALSE;
    }

    switch (message)
    {
        case WM_COMMAND:
            data = (Dialog_PdfAssociate_Data*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
            assert(data);
            data->dontAskAgain = FALSE;
            switch (LOWORD(wParam))
            {
                case IDOK:
                    if (BST_CHECKED == IsDlgButtonChecked(hDlg, IDC_DONT_ASK_ME_AGAIN))
                        data->dontAskAgain = TRUE;
                    EndDialog(hDlg, DIALOG_OK_PRESSED);
                    return TRUE;

                case IDCANCEL:
                    if (BST_CHECKED == IsDlgButtonChecked(hDlg, IDC_DONT_ASK_ME_AGAIN))
                        data->dontAskAgain = TRUE;
                    EndDialog(hDlg, DIALOG_NO_PRESSED);
                    return TRUE;

                case IDC_DONT_ASK_ME_AGAIN:
                    return TRUE;
            }
            break;
    }
    return FALSE;
}

/* Show "associate this application with PDF files" dialog.
   Returns DIALOG_YES_PRESSED if "Yes" button was pressed or
   DIALOG_NO_PRESSED if "No" button was pressed.
   Returns the state of "don't ask me again" checkbox" in <dontAskAgain> */
INT_PTR Dialog_PdfAssociate(HWND hwnd, BOOL *dontAskAgainOut)
{
    assert(dontAskAgainOut);

    Dialog_PdfAssociate_Data data;
    INT_PTR dialogResult = DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_DIALOG_PDF_ASSOCIATE), hwnd, Dialog_PdfAssociate_Proc, (LPARAM)&data);
    if (dontAskAgainOut)
        *dontAskAgainOut = data.dontAskAgain;
    return dialogResult;
}

/* For passing data to/from ChangeLanguage dialog */
typedef struct {
    int langId;
} Dialog_ChangeLanguage_Data;

static INT_PTR CALLBACK Dialog_ChangeLanguage_Proc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    Dialog_ChangeLanguage_Data *  data;
    HWND                          langList;

    if (WM_INITDIALOG == message)
    {
        DIALOG_SIZER_START(sz)
            DIALOG_SIZER_ENTRY(IDOK, DS_MoveX | DS_MoveY)
            DIALOG_SIZER_ENTRY(IDCANCEL, DS_MoveX | DS_MoveY)
            DIALOG_SIZER_ENTRY(IDC_CHANGE_LANG_LANG_LIST, DS_SizeY | DS_SizeX)
        DIALOG_SIZER_END()
        DialogSizer_Set(hDlg, sz, TRUE, NULL);
        data = (Dialog_ChangeLanguage_Data*)lParam;
        assert(data);
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)data);
        // for non-latin languages this depends on the correct fonts being installed,
        // otherwise all the user will see are squares
        win_set_text(hDlg, _TR("Change Language"));
        langList = GetDlgItem(hDlg, IDC_CHANGE_LANG_LANG_LIST);
        for (int i = 0; i < LANGS_COUNT; i++) {
            TCHAR *langName = utf8_to_tstr(g_langs[i]._langMenuTitle);
            ListBox_AppendString_NoSort(langList, langName);
            free(langName);
        }
        ListBox_SetCurSel(langList, data->langId);
        SetDlgItemText(hDlg, IDOK, _TR("OK"));
        SetDlgItemText(hDlg, IDCANCEL, _TR("Cancel"));

        CenterDialog(hDlg);
        SetFocus(langList);
        return FALSE;
    }

    switch (message)
    {
        case WM_COMMAND:
            data = (Dialog_ChangeLanguage_Data*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
            assert(data);
            if (HIWORD(wParam) == LBN_DBLCLK) {
                assert(IDC_CHANGE_LANG_LANG_LIST == LOWORD(wParam));
                langList = GetDlgItem(hDlg, IDC_CHANGE_LANG_LANG_LIST);
                assert(langList == (HWND)lParam);
                data->langId = ListBox_GetCurSel(langList);
                EndDialog(hDlg, DIALOG_OK_PRESSED);
                return FALSE;
            }
            switch (LOWORD(wParam))
            {
                case IDOK:
                    langList = GetDlgItem(hDlg, IDC_CHANGE_LANG_LANG_LIST);
                    data->langId = ListBox_GetCurSel(langList);
                    EndDialog(hDlg, DIALOG_OK_PRESSED);
                    return TRUE;

                case IDCANCEL:
                    EndDialog(hDlg, DIALOG_CANCEL_PRESSED);
                    return TRUE;
            }
            break;
    }

    return FALSE;
}

/* Show "Change Language" dialog.
   Returns language id (as stored in g_langs[]._langId) or -1 if the user 
   chose 'cancel' */
int Dialog_ChangeLanguge(HWND hwnd, int currLangId)
{
    Dialog_ChangeLanguage_Data data;
    data.langId = currLangId;
    INT_PTR dialogResult = DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_DIALOG_CHANGE_LANGUAGE), hwnd, Dialog_ChangeLanguage_Proc, (LPARAM)&data);
    if (DIALOG_CANCEL_PRESSED == dialogResult)
        return -1;
    return data.langId;
}

static INT_PTR CALLBACK Dialog_NewVersion_Proc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    Dialog_NewVersion_Data *  data;
    TCHAR *txt;

    if (WM_INITDIALOG == message)
    {
        data = (Dialog_NewVersion_Data*)lParam;
        assert(NULL != data);
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)data);
        win_set_text(hDlg, _TR("SumatraPDF Update"));

        txt = tstr_printf(_TR("You have version %s"), data->currVersion);
        SetDlgItemText(hDlg, IDC_YOU_HAVE, txt);
        free((void*)txt);

        txt = tstr_printf(_TR("New version %s is available. Download new version?"), data->newVersion);
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

    switch (message)
    {
        case WM_COMMAND:
            data = (Dialog_NewVersion_Data*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
            assert(data);
            data->skipThisVersion= FALSE;
            switch (LOWORD(wParam))
            {
                case IDOK:
                    if (BST_CHECKED == IsDlgButtonChecked(hDlg, IDC_SKIP_THIS_VERSION))
                        data->skipThisVersion= TRUE;
                    EndDialog(hDlg, DIALOG_OK_PRESSED);
                    return TRUE;

                case IDCANCEL:
                    if (BST_CHECKED == IsDlgButtonChecked(hDlg, IDC_SKIP_THIS_VERSION))
                        data->skipThisVersion= TRUE;
                    EndDialog(hDlg, DIALOG_NO_PRESSED);
                    return TRUE;

                case IDC_SKIP_THIS_VERSION:
                    return TRUE;
            }
            break;
    }
    return FALSE;
}

INT_PTR Dialog_NewVersionAvailable(HWND hwnd, Dialog_NewVersion_Data *data)
{
    return DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_DIALOG_NEW_VERSION), hwnd, Dialog_NewVersion_Proc, (LPARAM)data);
}

static float gItemZoom[] = { ZOOM_FIT_PAGE, ZOOM_FIT_WIDTH, ZOOM_FIT_CONTENT, 0,
    6400.0, 3200.0, 1600.0, 800.0, 400.0, 200.0, 150.0, 125.0, 100.0, 50.0, 25.0, 12.5, 8.33f };

static void SetupZoomComboBox(HWND hDlg, UINT idComboBox, float currZoom)
{
    // Fill the possible zoom settings into the select box
    SendDlgItemMessage(hDlg, idComboBox, CB_ADDSTRING, 0, (LPARAM)_TR("Fit Page"));
    SendDlgItemMessage(hDlg, idComboBox, CB_ADDSTRING, 0, (LPARAM)_TR("Fit Width"));
    SendDlgItemMessage(hDlg, idComboBox, CB_ADDSTRING, 0, (LPARAM)_TR("Fit Content"));
    SendDlgItemMessage(hDlg, idComboBox, CB_ADDSTRING, 0, (LPARAM)_T("-"));
    SendDlgItemMessage(hDlg, idComboBox, CB_ADDSTRING, 0, (LPARAM)_T("6400%"));
    SendDlgItemMessage(hDlg, idComboBox, CB_ADDSTRING, 0, (LPARAM)_T("3200%"));
    SendDlgItemMessage(hDlg, idComboBox, CB_ADDSTRING, 0, (LPARAM)_T("1600%"));
    SendDlgItemMessage(hDlg, idComboBox, CB_ADDSTRING, 0, (LPARAM)_T("800%"));
    SendDlgItemMessage(hDlg, idComboBox, CB_ADDSTRING, 0, (LPARAM)_T("400%"));
    SendDlgItemMessage(hDlg, idComboBox, CB_ADDSTRING, 0, (LPARAM)_T("200%"));
    SendDlgItemMessage(hDlg, idComboBox, CB_ADDSTRING, 0, (LPARAM)_T("150%"));
    SendDlgItemMessage(hDlg, idComboBox, CB_ADDSTRING, 0, (LPARAM)_T("125%"));
    SendDlgItemMessage(hDlg, idComboBox, CB_ADDSTRING, 0, (LPARAM)_T("100%"));
    SendDlgItemMessage(hDlg, idComboBox, CB_ADDSTRING, 0, (LPARAM)_T("50%"));
    SendDlgItemMessage(hDlg, idComboBox, CB_ADDSTRING, 0, (LPARAM)_T("25%"));
    SendDlgItemMessage(hDlg, idComboBox, CB_ADDSTRING, 0, (LPARAM)_T("12.5%"));
    SendDlgItemMessage(hDlg, idComboBox, CB_ADDSTRING, 0, (LPARAM)_T("8.33%"));
    for (int i = 0; i < dimof(gItemZoom); i++)
        if (gItemZoom[i] == currZoom)
            SendDlgItemMessage(hDlg, idComboBox, CB_SETCURSEL, i, 0);
    if (SendDlgItemMessage(hDlg, idComboBox, CB_GETCURSEL, 0, 0) == -1) {
        TCHAR *customZoom = tstr_printf(_T("%.0f%%"), currZoom);
        SetDlgItemText(hDlg, idComboBox, customZoom);
        free(customZoom);
    }
}

static float GetZoomComboBoxValue(HWND hDlg, UINT idComboBox, float defaultZoom)
{
    float newZoom = defaultZoom;

    int ix = ComboBox_GetCurSel(GetDlgItem(hDlg, idComboBox));
    if (ix == -1) {
        TCHAR *customZoom = win_get_text(GetDlgItem(hDlg, idComboBox));
        float zoom = (float)_tstof(customZoom);
        if (zoom >= ZOOM_MIN && zoom <= ZOOM_MAX)
            newZoom = zoom;
        free(customZoom);
    } else if (0 != gItemZoom[ix])
        newZoom = gItemZoom[ix];

    return newZoom;
}

static INT_PTR CALLBACK Dialog_CustomZoom_Proc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    float *currZoom;

    switch (message)
    {
    case WM_INITDIALOG:
        currZoom = (float *)lParam;
        assert(currZoom);
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)currZoom);

        SetupZoomComboBox(hDlg, IDC_DEFAULT_ZOOM, *currZoom);

        win_set_text(hDlg, _TR("Zoom factor"));
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
            currZoom = (float *)GetWindowLongPtr(hDlg, GWLP_USERDATA);
            assert(currZoom);
            *currZoom = GetZoomComboBoxValue(hDlg, IDC_DEFAULT_ZOOM, *currZoom);
            EndDialog(hDlg, DIALOG_OK_PRESSED);
            return TRUE;

        case IDCANCEL:
            EndDialog(hDlg, DIALOG_CANCEL_PRESSED);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

INT_PTR Dialog_CustomZoom(HWND hwnd, float *currZoom)
{
    return DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_DIALOG_CUSTOM_ZOOM), hwnd, Dialog_CustomZoom_Proc, (LPARAM)currZoom);
}

static INT_PTR CALLBACK Dialog_Settings_Proc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    SerializableGlobalPrefs *prefs;

    switch (message)
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
        SendDlgItemMessage(hDlg, IDC_DEFAULT_LAYOUT, CB_SETCURSEL, prefs->m_defaultDisplayMode - DM_FIRST, 0);

        SetupZoomComboBox(hDlg, IDC_DEFAULT_ZOOM, gGlobalPrefs.m_defaultZoom);

        CheckDlgButton(hDlg, IDC_DEFAULT_SHOW_TOC, prefs->m_showToc ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_GLOBAL_PREFS_ONLY, !prefs->m_globalPrefsOnly ? BST_CHECKED : BST_UNCHECKED);
        EnableWindow(GetDlgItem(hDlg, IDC_GLOBAL_PREFS_ONLY), prefs->m_rememberOpenedFiles);
        CheckDlgButton(hDlg, IDC_AUTO_UPDATE_CHECKS, prefs->m_enableAutoUpdate ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_REMEMBER_OPENED_FILES, prefs->m_rememberOpenedFiles ? BST_CHECKED : BST_UNCHECKED);
        if (IsExeAssociatedWithPdfExtension()) {
            SetDlgItemText(hDlg, IDC_SET_DEFAULT_READER, _TR("SumatraPDF is your default PDF reader"));
            EnableWindow(GetDlgItem(hDlg, IDC_SET_DEFAULT_READER), FALSE);
        } else if (IsRunningInPortableMode()) {
            SetDlgItemText(hDlg, IDC_SET_DEFAULT_READER, _TR("Default PDF reader can't be changed in portable mode"));
            EnableWindow(GetDlgItem(hDlg, IDC_SET_DEFAULT_READER), FALSE);
        } else {
            SetDlgItemText(hDlg, IDC_SET_DEFAULT_READER, _TR("Make SumatraPDF my default PDF reader"));
        }

        win_set_text(hDlg, _TR("SumatraPDF Options"));
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

        if (prefs->m_enableTeXEnhancements)
        {
            // Fit the additional section into the dialog
            // (this should rather happen in SumatraPDF.rc, but the resource
            // editor tends to overwrite conditional stuff which isn't its own)
            RECT rc;
            GetWindowRect(GetDlgItem(hDlg, IDC_SECTION_INVERSESEARCH), &rc);
            UINT addHeight = RectDy(&rc) + 8;
            GetWindowRect(hDlg, &rc);
            MoveWindow(hDlg, rc.left, rc.top, RectDx(&rc), RectDy(&rc) + addHeight, TRUE);

            GetClientRect(GetDlgItem(hDlg, IDOK), &rc);
            MapWindowPoints(GetDlgItem(hDlg, IDOK), hDlg, (LPPOINT)&rc, 2);
            MoveWindow(GetDlgItem(hDlg, IDOK), rc.left, rc.top + addHeight, RectDx(&rc), RectDy(&rc), TRUE);
            GetClientRect(GetDlgItem(hDlg, IDCANCEL), &rc);
            MapWindowPoints(GetDlgItem(hDlg, IDCANCEL), hDlg, (LPPOINT)&rc, 2);
            MoveWindow(GetDlgItem(hDlg, IDCANCEL), rc.left, rc.top + addHeight, RectDx(&rc), RectDy(&rc), TRUE);

            SetDlgItemText(hDlg, IDC_SECTION_INVERSESEARCH, _TR("Set inverse search command-line"));
            SetDlgItemText(hDlg, IDC_CMDLINE_LABEL, _TR("Enter the command-line to invoke when you double-click on the PDF document:"));
            // Fill the combo with the list of possible inverse search commands
            TCHAR *inverseSearch = AutoDetectInverseSearchCommands(GetDlgItem(hDlg, IDC_CMDLINE));
            // Try to select a correct default when first showing this dialog
            if (!prefs->m_inverseSearchCmdLine)
                prefs->m_inverseSearchCmdLine = inverseSearch;
            // Find the index of the active command line    
            LRESULT ind = SendMessage(GetDlgItem(hDlg, IDC_CMDLINE), CB_FINDSTRINGEXACT, -1, (LPARAM) prefs->m_inverseSearchCmdLine);
            if (CB_ERR == ind)
            {            
                // if no existing command was selected then set the user custom command in the combo
                SetDlgItemText(hDlg, IDC_CMDLINE, prefs->m_inverseSearchCmdLine);
            }
            else
            {
                // select the active command
                SendMessage(GetDlgItem(hDlg, IDC_CMDLINE), CB_SETCURSEL, (WPARAM) ind , 0);
            }
            if (prefs->m_inverseSearchCmdLine == inverseSearch)
                prefs->m_inverseSearchCmdLine = NULL;
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
            prefs->m_defaultDisplayMode = (DisplayMode)(SendDlgItemMessage(hDlg, IDC_DEFAULT_LAYOUT, CB_GETCURSEL, 0, 0) + DM_FIRST);
            prefs->m_defaultZoom = GetZoomComboBoxValue(hDlg, IDC_DEFAULT_ZOOM, prefs->m_defaultZoom);

            prefs->m_showToc = (BST_CHECKED == IsDlgButtonChecked(hDlg, IDC_DEFAULT_SHOW_TOC));
            prefs->m_globalPrefsOnly = (BST_CHECKED != IsDlgButtonChecked(hDlg, IDC_GLOBAL_PREFS_ONLY));
            prefs->m_enableAutoUpdate = (BST_CHECKED == IsDlgButtonChecked(hDlg, IDC_AUTO_UPDATE_CHECKS));
            prefs->m_rememberOpenedFiles = (BST_CHECKED == IsDlgButtonChecked(hDlg, IDC_REMEMBER_OPENED_FILES));
            if (prefs->m_enableTeXEnhancements) {
                free(prefs->m_inverseSearchCmdLine);
                prefs->m_inverseSearchCmdLine = win_get_text(GetDlgItem(hDlg, IDC_CMDLINE));
            }
            EndDialog(hDlg, DIALOG_OK_PRESSED);
            return TRUE;

        case IDCANCEL:
            EndDialog(hDlg, DIALOG_CANCEL_PRESSED);
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
    return DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_DIALOG_SETTINGS), hwnd, Dialog_Settings_Proc, (LPARAM)prefs);
}

static INT_PTR CALLBACK Sheet_Print_Advanced_Proc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    Print_Advanced_Data *data;

    switch (message)
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

HPROPSHEETPAGE CreatePrintAdvancedPropSheet(HINSTANCE hInst, Print_Advanced_Data *data)
{
    PROPSHEETPAGE psp;
    ZeroMemory(&psp, sizeof(PROPSHEETPAGE));

    psp.dwSize = sizeof(PROPSHEETPAGE);
    psp.dwFlags = PSP_USETITLE | PSP_PREMATURE;
    psp.hInstance = hInst;
    psp.pszTemplate = MAKEINTRESOURCE(IDD_PROPSHEET_PRINT_ADVANCED);
    psp.pfnDlgProc = Sheet_Print_Advanced_Proc;
    psp.lParam = (LPARAM)data;
    psp.pszTitle = _TR("Advanced");

    return CreatePropertySheetPage(&psp);
}
