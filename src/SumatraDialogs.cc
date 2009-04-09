/* Copyright Krzysztof Kowalczyk 2006-2007
   License: GPLv2 */
#include <windows.h>
#include <assert.h>

#include "SumatraPDF.h"
#include "SumatraDialogs.h"

#include "DisplayModel.h"
#include "dstring.h"
#include "Resource.h"
#include "win_util.h"
#include "dialogsizer.h"
#include "LangMenuDef.h"
#include "utf_util.h"
#include "translations.h"
#include "wstr_util.h"

typedef struct {
    const WCHAR *  in_cmdline;   /* current inverse search command line */
    WCHAR *  out_cmdline;         /* inverse search command line selected by the user */
} Dialog_InverseSearch_Data;

#ifdef _TEX_ENHANCEMENT
static BOOL CALLBACK Dialog_InverseSearch_Proc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    HWND                       edit;
    Dialog_InverseSearch_Data *  data;

    if (WM_INITDIALOG == message)
    {
        data = (Dialog_InverseSearch_Data*)lParam;
        assert(data);
        assert(data->in_cmdline);
        SetWindowLongPtr(hDlg, GWL_USERDATA, (LONG_PTR)data);
        SetDlgItemTextW(hDlg, IDC_CMDLINE, data->in_cmdline);
        edit = GetDlgItem(hDlg, IDC_CMDLINE);
        SetFocus(edit);
        return FALSE;
    }


    switch (message)
    {
        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case IDOK:
                    data = (Dialog_InverseSearch_Data*)GetWindowLongPtr(hDlg, GWL_USERDATA);
                    assert(data);
                    edit = GetDlgItem(hDlg, IDC_CMDLINE);
                    data->out_cmdline = win_get_textw(edit);
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

/* Shows a dialog that let the user configure the inverser search command line
   Returns the command line as a newly allocated string or
   NULL if user cancelled the dialog or there was an error.
   Caller needs to free() the result.
*/
WCHAR *Dialog_SetInverseSearchCmdline(WindowInfo *win, const WCHAR *cmdline)
{
    int                     dialogResult;
    Dialog_InverseSearch_Data data;
    
    assert(cmdline);
    if (!cmdline) return NULL;

    data.in_cmdline = cmdline;
    dialogResult = DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_DIALOG_INVERSESEARCH), win->hwndFrame, Dialog_InverseSearch_Proc, (LPARAM)&data);
    if (DIALOG_OK_PRESSED == dialogResult) {
        return data.out_cmdline;
    }
    return NULL;
}
#endif

/* For passing data to/from GetPassword dialog */
typedef struct {
    const WCHAR *  fileName;   /* name of the file for which we need the password */
    char *         pwdOut;     /* password entered by the user */
} Dialog_GetPassword_Data;

static BOOL CALLBACK Dialog_GetPassword_Proc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    Dialog_GetPassword_Data *  data;

    if (WM_INITDIALOG == message)
    {
        /* TODO: intelligently center the dialog within the parent window? */
        data = (Dialog_GetPassword_Data*)lParam;
        assert(data);
        assert(data->fileName);
        assert(!data->pwdOut);
        win_set_textw(hDlg, _TRW("Enter password"));
        SetWindowLongPtr(hDlg, GWL_USERDATA, (LONG_PTR)data);
        WCHAR *txt = wstr_printf(_TRW("Enter password for %s"), data->fileName);
        SetDlgItemTextW(hDlg, IDC_GET_PASSWORD_LABEL, txt);
        free(txt);
        SetDlgItemTextA(hDlg, IDC_GET_PASSWORD_EDIT, "");
        SetFocus(GetDlgItem(hDlg, IDC_GET_PASSWORD_EDIT));
        return FALSE;
    }

    switch (message)
    {
        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case IDOK:
                    data = (Dialog_GetPassword_Data*)GetWindowLongPtr(hDlg, GWL_USERDATA);
                    assert(data);
                    data->pwdOut = win_get_text(GetDlgItem(hDlg, IDC_GET_PASSWORD_EDIT));
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
char *Dialog_GetPassword(WindowInfo *win, const WCHAR *fileName)
{
    int                     dialogResult;
    Dialog_GetPassword_Data data;
    
    assert(fileName);
    if (!fileName) return NULL;

    data.fileName = fileName;
    data.pwdOut = NULL;
    dialogResult = DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_DIALOG_GET_PASSWORD), win->hwndFrame, Dialog_GetPassword_Proc, (LPARAM)&data);
    if (DIALOG_OK_PRESSED == dialogResult) {
        return data.pwdOut;
    }
    free((void*)data.pwdOut);
    return NULL;
}

/* For passing data to/from GoToPage dialog */
typedef struct {
    int     currPageNo;      /* currently shown page number */
    int     pageCount;       /* total number of pages */
    int     pageEnteredOut;  /* page number entered by user */
} Dialog_GoToPage_Data;

static BOOL CALLBACK Dialog_GoToPage_Proc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    HWND                    editPageNo;
    DString                 ds;
    TCHAR *                 newPageNoTxt;
    Dialog_GoToPage_Data *  data;

    if (WM_INITDIALOG == message)
    {
        /* TODO: intelligently center the dialog within the parent window? */
        data = (Dialog_GoToPage_Data*)lParam;
        assert(data);
        SetWindowLongPtr(hDlg, GWL_USERDATA, (LONG_PTR)data);
        assert(INVALID_PAGE_NO != data->currPageNo);
        assert(data->pageCount >= 1);
        win_set_textw(hDlg, _TRW("Go to page"));
        DStringInit(&ds);
        DStringSprintf(&ds, "%d", data->currPageNo);
        SetDlgItemTextA(hDlg, IDC_GOTO_PAGE_EDIT, ds.pString);
        DStringFree(&ds);
        DStringSprintf(&ds, "(of %d)", data->pageCount);
        SetDlgItemTextA(hDlg, IDC_GOTO_PAGE_LABEL_OF, ds.pString);
        DStringFree(&ds);
        editPageNo = GetDlgItem(hDlg, IDC_GOTO_PAGE_EDIT);
        win_edit_select_all(editPageNo);
        SetFocus(editPageNo);
        return FALSE;
    }

    switch (message)
    {
        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case IDOK:
                    data = (Dialog_GoToPage_Data*)GetWindowLongPtr(hDlg, GWL_USERDATA);
                    assert(data);
                    data->pageEnteredOut = INVALID_PAGE_NO;
                    editPageNo = GetDlgItem(hDlg, IDC_GOTO_PAGE_EDIT);
                    newPageNoTxt = win_get_text(editPageNo);
                    if (newPageNoTxt) {
                        data->pageEnteredOut = atoi(newPageNoTxt);
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
    int                     dialogResult;
    Dialog_GoToPage_Data    data;
    
    assert(win);
    if (!win) return INVALID_PAGE_NO;

    data.currPageNo = win->dm->startPage();
    data.pageCount = win->dm->pageCount();
    dialogResult = DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_DIALOG_GOTO_PAGE), win->hwndFrame, Dialog_GoToPage_Proc, (LPARAM)&data);
    if (DIALOG_OK_PRESSED == dialogResult) {
        if (win->dm->validPageNo(data.pageEnteredOut)) {
            return data.pageEnteredOut;
        }
    }
    return INVALID_PAGE_NO;
}

/* For passing data to/from AssociateWithPdf dialog */
typedef struct {
    BOOL    dontAskAgain;
} Dialog_PdfAssociate_Data;

static BOOL CALLBACK Dialog_PdfAssociate_Proc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    Dialog_PdfAssociate_Data *  data;

    if (WM_INITDIALOG == message)
    {
        /* TODO: intelligently center the dialog within the parent window? */
        data = (Dialog_PdfAssociate_Data*)lParam;
        assert(data);
        SetWindowLongPtr(hDlg, GWL_USERDATA, (LONG_PTR)data);
        win_set_textw(hDlg, _TRW("Associate with PDF files?"));
        SetDlgItemTextW(hDlg, IDC_STATIC, _TRW("Make SumatraPDF default application for PDF files?"));
        SetDlgItemTextW(hDlg, IDC_DONT_ASK_ME_AGAIN, _TRW("Don't ask me again"));
        CheckDlgButton(hDlg, IDC_DONT_ASK_ME_AGAIN, BST_UNCHECKED);
        SetFocus(GetDlgItem(hDlg, IDOK));
        return FALSE;
    }

    switch (message)
    {
        case WM_COMMAND:
            data = (Dialog_PdfAssociate_Data*)GetWindowLongPtr(hDlg, GWL_USERDATA);
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
int Dialog_PdfAssociate(HWND hwnd, BOOL *dontAskAgainOut)
{
    assert(dontAskAgainOut);

    Dialog_PdfAssociate_Data data;
    int dialogResult = DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_DIALOG_PDF_ASSOCIATE), hwnd, Dialog_PdfAssociate_Proc, (LPARAM)&data);
    if (dontAskAgainOut)
        *dontAskAgainOut = data.dontAskAgain;
    return dialogResult;
}

/* For passing data to/from ChangeLanguage dialog */
typedef struct {
    int langId;
} Dialog_ChangeLanguage_Data;

static BOOL CALLBACK Dialog_ChangeLanguage_Proc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    Dialog_ChangeLanguage_Data *  data;
    HWND                          langList;
    int                           sel;

    if (WM_INITDIALOG == message)
    {
        DIALOG_SIZER_START(sz)
            DIALOG_SIZER_ENTRY(IDOK, DS_MoveY)
            DIALOG_SIZER_ENTRY(IDCANCEL, DS_MoveX | DS_MoveY)
            DIALOG_SIZER_ENTRY(IDC_CHANGE_LANG_LANG_LIST, DS_SizeY | DS_SizeX)
        DIALOG_SIZER_END()
        DialogSizer_Set(hDlg, sz, TRUE, NULL);
        data = (Dialog_ChangeLanguage_Data*)lParam;
        assert(data);
		// TODO: figure out how to make it unicode. Is it because resource template is ansi?
		BOOL isUni = IsWindowUnicode(hDlg);
        SetWindowLongPtr(hDlg, GWL_USERDATA, (LONG_PTR)data);
        /* TODO: for some reason this doesn't work well e.g. when using
           russion translation, the russian part of window title is garbage (?)
           not russian text. Maybe I need to change the font ? */
        win_set_textw(hDlg, _TRW("Change language"));
        WCHAR *langName;
        langList = GetDlgItem(hDlg, IDC_CHANGE_LANG_LANG_LIST);
        int idx = 0;
        for (int i=0; i < LANGS_COUNT; i++) {
            langName = utf8_to_utf16(g_menuDefLang[i].m_title);
            lb_append_stringw_no_sort(langList, langName);
            free(langName);
            if (g_menuDefLang[i].m_id == data->langId)
                idx = i;
        }
        lb_set_selection(langList, idx);
        SetFocus(langList);
        return FALSE;
    }

    switch (message)
    {
        case WM_COMMAND:
            data = (Dialog_ChangeLanguage_Data*)GetWindowLongPtr(hDlg, GWL_USERDATA);
            assert(data);
            if (HIWORD(wParam) == LBN_DBLCLK) {
                assert(IDC_CHANGE_LANG_LANG_LIST == LOWORD(wParam));
                langList = GetDlgItem(hDlg, IDC_CHANGE_LANG_LANG_LIST);
                assert(langList == (HWND)lParam);
                sel = lb_get_selection(langList);                    
                data->langId = g_menuDefLang[sel].m_id;
                EndDialog(hDlg, DIALOG_OK_PRESSED);
                return FALSE;
            }
            switch (LOWORD(wParam))
            {
                case IDOK:
                    langList = GetDlgItem(hDlg, IDC_CHANGE_LANG_LANG_LIST);
                    sel = lb_get_selection(langList);                    
                    data->langId = g_menuDefLang[sel].m_id;
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
    int dialogResult = DialogBoxParamW(NULL, MAKEINTRESOURCEW(IDD_DIALOG_CHANGE_LANGUAGE), hwnd, Dialog_ChangeLanguage_Proc, (LPARAM)&data);
    if (DIALOG_CANCEL_PRESSED == dialogResult)
        return -1;
    return data.langId;
}

static BOOL CALLBACK Dialog_NewVersion_Proc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    Dialog_NewVersion_Data *  data;
    WCHAR *txt;

    if (WM_INITDIALOG == message)
    {
        /* TODO: intelligently center the dialog within the parent window? */
        data = (Dialog_NewVersion_Data*)lParam;
        assert(NULL != data);
        SetWindowLongPtr(hDlg, GWL_USERDATA, (LONG_PTR)data);
        win_set_textw(hDlg, _TRW("New version available."));

        txt = wstr_printf(_TRW("You have version %s"), data->currVersion);
        SetDlgItemTextW(hDlg, IDC_YOU_HAVE, txt);
        free((void*)txt);

        txt = wstr_printf(_TRW("New version %s is available. Download new version?"), data->newVersion);
        SetDlgItemTextW(hDlg, IDC_NEW_AVAILABLE, txt);
        free(txt);

        SetDlgItemTextW(hDlg, IDC_SKIP_THIS_VERSION, _TRW("Skip this version"));
        CheckDlgButton(hDlg, IDC_SKIP_THIS_VERSION, BST_UNCHECKED);
        SetFocus(GetDlgItem(hDlg, IDOK));
        return FALSE;
    }

    switch (message)
    {
        case WM_COMMAND:
            data = (Dialog_NewVersion_Data*)GetWindowLongPtr(hDlg, GWL_USERDATA);
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

int Dialog_NewVersionAvailable(HWND hwnd, Dialog_NewVersion_Data *data)
{
    int dialogResult = DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_DIALOG_NEW_VERSION), hwnd, Dialog_NewVersion_Proc, (LPARAM)data);
    return dialogResult;
}
