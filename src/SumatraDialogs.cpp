/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "wingui/DialogSizer.h"
#include "utils/WinUtil.h"

#include "Settings.h"
#include "AppSettings.h"

#include "GlobalPrefs.h"

#include "Annotation.h"
#include "SumatraPDF.h"
#include "resource.h"
#include "Commands.h"
#include "AppTools.h"
#include "SumatraDialogs.h"
#include "Translations.h"
#include "Theme.h"
#include "DarkModeSubclass.h"

// http://msdn.microsoft.com/en-us/library/ms645398(v=VS.85).aspx
#pragma pack(push, 1)
struct DLGTEMPLATEEX {
    WORD dlgVer;    // 0x0001
    WORD signature; // 0xFFFF
    DWORD helpID;
    DWORD exStyle;
    DWORD style;
    WORD cDlgItems;
    short x, y, cx, cy;
    /*
    sz_Or_Ord menu;
    sz_Or_Ord windowClass;
    WCHAR     title[titleLen];
    WORD      pointsize;
    WORD      weight;
    BYTE      italic;
    BYTE      charset;
    WCHAR     typeface[stringLen];
    */
};
#pragma pack(pop)

DLGTEMPLATE* DupTemplate(int dlgId) {
    HRSRC dialogRC = FindResourceW(nullptr, MAKEINTRESOURCE(dlgId), RT_DIALOG);
    ReportIf(!dialogRC);
    HGLOBAL dlgTemplate = LoadResource(nullptr, dialogRC);
    ReportIf(!dlgTemplate);
    void* orig = LockResource(dlgTemplate);
    size_t size = SizeofResource(nullptr, dialogRC);
    ReportIf(size == 0);
    DLGTEMPLATE* ret = (DLGTEMPLATE*)memdup(orig, size);
    UnlockResource(orig);
    return ret;
}

/*
Type: sz_Or_Ord

A variable-length array of 16-bit elements that identifies a menu resource for the dialog box. If the first element of
this array is 0x0000, the dialog box has no menu and the array has no other elements. If the first element is 0xFFFF,
the array has one additional element that specifies the ordinal value of a menu resource in an executable file. If the
first element has any other value, the system treats the array as a null-terminated Unicode string that specifies the
name of a menu resource in an executable file.
*/
static u8* SkipSzOrOrd(u8* d) {
    WORD* pw = (WORD*)d;
    WORD w = *pw++;
    if (w == 0x0000) {
        // no menu
    } else if (w == 0xffff) {
        // menu id followed by another WORD item
        pw++;
    } else {
        // anything else: zero-terminated WCHAR*
        WCHAR* s = (WCHAR*)pw;
        while (*s) {
            s++;
        }
        s++;
        pw = (WORD*)s;
    }
    return (u8*)pw;
}

static u8* SkipSz(u8* d) {
    WCHAR* s = (WCHAR*)d;
    while (*s) {
        s++;
    }
    s++;
    return (u8*)s;
}

static bool IsDlgTemplateEx(DLGTEMPLATE* tpl) {
    return tpl->style == MAKELONG(0x0001, 0xFFFF);
}

static bool HasDlgTemplateExFont(DLGTEMPLATEEX* tpl) {
    DWORD style = tpl->style & (DS_SETFONT | DS_FIXEDSYS);
    return style != 0;
}

// gets a dialog template from the resources and sets the RTL flag
// cf. http://www.ureader.com/msg/1484387.aspx
static void SetDlgTemplateRtl(DLGTEMPLATE* tpl) {
    if (IsDlgTemplateEx(tpl)) {
        ((DLGTEMPLATEEX*)tpl)->exStyle |= WS_EX_LAYOUTRTL;
    } else {
        tpl->dwExtendedStyle |= WS_EX_LAYOUTRTL;
    }
}

static int ToFontPointSize(int fontSize) {
    int res = (fontSize * 72) / 96;
    return res;
}

// https://stackoverflow.com/questions/14370238/can-i-dynamically-change-the-font-size-of-a-dialog-window-created-with-c-in-vi
// TODO: if changing font name would have do more complicated dance of replacing
// variable string in the middle of the struct
static void SetDlgTemplateExFont(DLGTEMPLATE* tmp, bool isRtl, int fontSize) {
    ReportIf(!IsDlgTemplateEx(tmp));
    if (isRtl) {
        SetDlgTemplateRtl(tmp);
    }
    DLGTEMPLATEEX* tpl = (DLGTEMPLATEEX*)tmp;
    ReportIf(!HasDlgTemplateExFont(tpl));
    u8* d = (u8*)tpl;
    d += sizeof(DLGTEMPLATEEX);
    // sz_Or_Ord menu
    d = SkipSzOrOrd(d);
    // sz_Or_Ord windowClass;
    d = SkipSzOrOrd(d);
    // WCHAR[] title
    d = SkipSz(d);
    // WCHAR pointSize;
    WORD* wd = (WORD*)d;
    fontSize = ToFontPointSize(fontSize);
    *wd = fontSize;
}

DLGTEMPLATE* GetRtLDlgTemplate(int dlgId) {
    DLGTEMPLATE* tpl = DupTemplate(dlgId);
    SetDlgTemplateRtl(tpl);
    return tpl;
}

// creates a dialog box that dynamically gets a right-to-left layout if needed
static INT_PTR CreateDialogBox(int dlgId, HWND parent, DLGPROC DlgProc, LPARAM data) {
    bool isRtl = IsUIRtl();
    bool isDefaultFont = IsAppFontSizeDefault();
    if (!isRtl && isDefaultFont) {
        return DialogBoxParam(nullptr, MAKEINTRESOURCE(dlgId), parent, DlgProc, data);
    }

    DLGTEMPLATE* tpl = DupTemplate(dlgId);
    int fntSize = GetAppFontSize();
    if (isDefaultFont) {
        SetDlgTemplateRtl(tpl);
    } else {
        SetDlgTemplateExFont(tpl, isRtl, fntSize);
    }

    INT_PTR res = DialogBoxIndirectParamW(nullptr, tpl, parent, DlgProc, data);
    free(tpl);
    return res;
}

/* For passing data to/from GetPassword dialog */
struct Dialog_GetPassword_Data {
    const char* fileName; /* name of the file for which we need the password */
    char* pwdOut;         /* password entered by the user */
    bool* remember;       /* remember the password (encrypted) or ask again? */
};

static INT_PTR CALLBACK Dialog_GetPassword_Proc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) {
    Dialog_GetPassword_Data* data;

    //[ ACCESSKEY_GROUP Password Dialog
    if (WM_INITDIALOG == msg) {
        data = (Dialog_GetPassword_Data*)lp;
        HwndSetText(hDlg, _TRA("Enter password"));
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)data);
        if (gUseDarkModeLib) {
            DarkMode::setDarkWndSafe(hDlg);
        }
        EnableWindow(GetDlgItem(hDlg, IDC_REMEMBER_PASSWORD), data->remember != nullptr);

        TempStr txt = str::FormatTemp(_TRA("Enter password for %s"), data->fileName);
        HwndSetDlgItemText(hDlg, IDC_GET_PASSWORD_LABEL, txt);
        HwndSetDlgItemText(hDlg, IDC_GET_PASSWORD_EDIT, "");
        HwndSetDlgItemText(hDlg, IDC_STATIC, _TRA("&Password:"));
        HwndSetDlgItemText(hDlg, IDC_REMEMBER_PASSWORD, _TRA("&Remember the password for this document"));
        HwndSetDlgItemText(hDlg, IDOK, _TRA("OK"));
        HwndSetDlgItemText(hDlg, IDCANCEL, _TRA("Cancel"));

        CenterDialog(hDlg);
        HwndSetFocus(GetDlgItem(hDlg, IDC_GET_PASSWORD_EDIT));
        BringWindowToTop(hDlg);
        return FALSE;
    }
    //] ACCESSKEY_GROUP Password Dialog

    char* tmp;
    switch (msg) {
        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case IDOK:
                    data = (Dialog_GetPassword_Data*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
                    tmp = HwndGetTextTemp(GetDlgItem(hDlg, IDC_GET_PASSWORD_EDIT));
                    data->pwdOut = str::Dup(tmp);
                    if (data->remember) {
                        *data->remember = BST_CHECKED == IsDlgButtonChecked(hDlg, IDC_REMEMBER_PASSWORD);
                    }
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
   nullptr if user cancelled the dialog or there was an error.
   Caller needs to free() the result.
*/
char* Dialog_GetPassword(HWND hwndParent, const char* fileName, bool* rememberPassword) {
    Dialog_GetPassword_Data data = {nullptr};
    data.fileName = fileName;
    data.remember = rememberPassword;

    INT_PTR res = CreateDialogBox(IDD_DIALOG_GET_PASSWORD, hwndParent, Dialog_GetPassword_Proc, (LPARAM)&data);
    if (IDOK != res) {
        free(data.pwdOut);
        return nullptr;
    }
    return data.pwdOut;
}

/* For passing data to/from GoToPage dialog */
struct Dialog_GoToPage_Data {
    char* currPageLabel = nullptr; // currently shown page label
    int pageCount = 0;             // total number of pages
    bool onlyNumeric = false;      // whether the page label must be numeric
    char* newPageLabel = nullptr;  // page number entered by user

    ~Dialog_GoToPage_Data() {
        str::Free(currPageLabel);
        str::Free(newPageLabel);
    }
};

static INT_PTR CALLBACK Dialog_GoToPage_Proc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) {
    HWND editPageNo;
    Dialog_GoToPage_Data* data;

    //[ ACCESSKEY_GROUP GoTo Page Dialog
    if (WM_INITDIALOG == msg) {
        data = (Dialog_GoToPage_Data*)lp;
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)data);
        if (gUseDarkModeLib) {
            DarkMode::setDarkWndSafe(hDlg);
        }
        HwndSetText(hDlg, _TRA("Go to page"));

        editPageNo = GetDlgItem(hDlg, IDC_GOTO_PAGE_EDIT);
        if (!data->onlyNumeric) {
            SetWindowLong(editPageNo, GWL_STYLE, GetWindowLong(editPageNo, GWL_STYLE) & ~ES_NUMBER);
        }
        ReportIf(!data->currPageLabel);
        HwndSetDlgItemText(hDlg, IDC_GOTO_PAGE_EDIT, data->currPageLabel);
        TempStr totalCount = str::FormatTemp(_TRA("(of %d)"), data->pageCount);
        HwndSetDlgItemText(hDlg, IDC_GOTO_PAGE_LABEL_OF, totalCount);

        EditSelectAll(editPageNo);
        HwndSetDlgItemText(hDlg, IDC_STATIC, _TRA("&Go to page:"));
        HwndSetDlgItemText(hDlg, IDOK, _TRA("Go to page"));
        HwndSetDlgItemText(hDlg, IDCANCEL, _TRA("Cancel"));

        CenterDialog(hDlg);
        HwndSetFocus(editPageNo);
        return FALSE;
    }
    //] ACCESSKEY_GROUP GoTo Page Dialog

    char* tmp;
    switch (msg) {
        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case IDOK:
                    data = (Dialog_GoToPage_Data*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
                    editPageNo = GetDlgItem(hDlg, IDC_GOTO_PAGE_EDIT);
                    tmp = HwndGetTextTemp(editPageNo);
                    str::ReplaceWithCopy(&data->newPageLabel, tmp);
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
   or nullptr if user clicked the "cancel" button or there was an error.
   The caller must free() the result. */
char* Dialog_GoToPage(HWND hwnd, const char* currentPageLabel, int pageCount, bool onlyNumeric) {
    Dialog_GoToPage_Data data;
    data.currPageLabel = str::Dup(currentPageLabel);
    data.pageCount = pageCount;
    data.onlyNumeric = onlyNumeric;
    data.newPageLabel = nullptr;

    CreateDialogBox(IDD_DIALOG_GOTO_PAGE, hwnd, Dialog_GoToPage_Proc, (LPARAM)&data);
    return str::Dup(data.newPageLabel);
}

/* For passing data to/from Find dialog */
struct Dialog_Find_Data {
    char* searchTerm;
    bool matchCase;
    WNDPROC editWndProc;
};

static LRESULT CALLBACK Dialog_Find_Edit_Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    ExtendedEditWndProc(hwnd, msg, wp, lp);

    Dialog_Find_Data* data = (Dialog_Find_Data*)GetWindowLongPtr(GetParent(hwnd), GWLP_USERDATA);
    return CallWindowProc(data->editWndProc, hwnd, msg, wp, lp);
}

static INT_PTR CALLBACK Dialog_Find_Proc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) {
    Dialog_Find_Data* data;

    switch (msg) {
        case WM_INITDIALOG: {
            //[ ACCESSKEY_GROUP Find Dialog
            data = (Dialog_Find_Data*)lp;
            SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)data);
            if (gUseDarkModeLib) {
                DarkMode::setDarkWndSafe(hDlg);
            }
            HwndSetText(hDlg, _TRA("Find"));
            HwndSetDlgItemText(hDlg, IDC_STATIC, _TRA("&Find what:"));
            HwndSetDlgItemText(hDlg, IDC_MATCH_CASE, _TRA("&Match case"));
            HwndSetDlgItemText(hDlg, IDC_FIND_NEXT_HINT, _TRA("Hint: Use the F3 key for finding again"));
            HwndSetDlgItemText(hDlg, IDOK, _TRA("Find"));
            HwndSetDlgItemText(hDlg, IDCANCEL, _TRA("Cancel"));
            if (data->searchTerm) {
                HwndSetDlgItemText(hDlg, IDC_FIND_EDIT, data->searchTerm);
            }
            data->searchTerm = nullptr;
            CheckDlgButton(hDlg, IDC_MATCH_CASE, data->matchCase ? BST_CHECKED : BST_UNCHECKED);
            data->editWndProc = (WNDPROC)SetWindowLongPtr(GetDlgItem(hDlg, IDC_FIND_EDIT), GWLP_WNDPROC,
                                                          (LONG_PTR)Dialog_Find_Edit_Proc);
            EditSelectAll(GetDlgItem(hDlg, IDC_FIND_EDIT));

            CenterDialog(hDlg);
            HwndSetFocus(GetDlgItem(hDlg, IDC_FIND_EDIT));
            return FALSE;
            //] ACCESSKEY_GROUP Find Dialog
        }

        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case IDOK: {
                    data = (Dialog_Find_Data*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
                    TempStr tmp = HwndGetTextTemp(GetDlgItem(hDlg, IDC_FIND_EDIT));
                    data->searchTerm = str::Dup(tmp);
                    data->matchCase = BST_CHECKED == IsDlgButtonChecked(hDlg, IDC_MATCH_CASE);
                    EndDialog(hDlg, IDOK);
                    return TRUE;
                }

                case IDCANCEL:
                    EndDialog(hDlg, IDCANCEL);
                    return TRUE;
            }
            break;
    }
    return FALSE;
}

/* Shows a 'Find' dialog and returns the new search term entered by the user
   or nullptr if the search was canceled. previousSearch is the search term to
   be displayed as default. */
char* Dialog_Find(HWND hwnd, const char* previousSearch, bool* matchCase) {
    Dialog_Find_Data data;
    data.searchTerm = str::DupTemp(previousSearch);
    data.matchCase = matchCase ? *matchCase : false;
    INT_PTR res = CreateDialogBox(IDD_DIALOG_FIND, hwnd, Dialog_Find_Proc, (LPARAM)&data);
    if (res != IDOK) {
        return nullptr;
    }

    if (matchCase) {
        *matchCase = data.matchCase;
    }
    return data.searchTerm;
}

/* For passing data to/from AssociateWithPdf dialog */
struct Dialog_PdfAssociate_Data {
    bool dontAskAgain = false;
};

static INT_PTR CALLBACK Dialog_PdfAssociate_Proc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) {
    Dialog_PdfAssociate_Data* data;

    //[ ACCESSKEY_GROUP Associate Dialog
    if (WM_INITDIALOG == msg) {
        data = (Dialog_PdfAssociate_Data*)lp;
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)data);
        if (gUseDarkModeLib) {
            DarkMode::setDarkWndSafe(hDlg);
        }
        HwndSetText(hDlg, _TRA("Associate with PDF files?"));
        HwndSetDlgItemText(hDlg, IDC_STATIC, _TRA("Make SumatraPDF default application for PDF files?"));
        HwndSetDlgItemText(hDlg, IDC_DONT_ASK_ME_AGAIN, _TRA("&Don't ask me again"));
        CheckDlgButton(hDlg, IDC_DONT_ASK_ME_AGAIN, BST_UNCHECKED);
        HwndSetDlgItemText(hDlg, IDOK, _TRA("&Yes"));
        HwndSetDlgItemText(hDlg, IDCANCEL, _TRA("&No"));

        CenterDialog(hDlg);
        HwndSetFocus(GetDlgItem(hDlg, IDOK));
        return FALSE;
    }
    //] ACCESSKEY_GROUP Associate Dialog

    switch (msg) {
        case WM_COMMAND:
            data = (Dialog_PdfAssociate_Data*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
            data->dontAskAgain = (BST_CHECKED == IsDlgButtonChecked(hDlg, IDC_DONT_ASK_ME_AGAIN));
            switch (LOWORD(wp)) {
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
INT_PTR Dialog_PdfAssociate(HWND hwnd, bool* dontAskAgainOut) {
    Dialog_PdfAssociate_Data data;
    INT_PTR res = CreateDialogBox(IDD_DIALOG_PDF_ASSOCIATE, hwnd, Dialog_PdfAssociate_Proc, (LPARAM)&data);
    *dontAskAgainOut = data.dontAskAgain;
    return res;
}

/* For passing data to/from ChangeLanguage dialog */
struct Dialog_ChangeLanguage_Data {
    const char* langCode;
};

static INT_PTR CALLBACK Dialog_ChangeLanguage_Proc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) {
    Dialog_ChangeLanguage_Data* data;
    HWND langList;

    if (WM_INITDIALOG == msg) {
        DIALOG_SIZER_START(sz)
        DIALOG_SIZER_ENTRY(IDOK, DS_MoveX | DS_MoveY)
        DIALOG_SIZER_ENTRY(IDCANCEL, DS_MoveX | DS_MoveY)
        DIALOG_SIZER_ENTRY(IDC_CHANGE_LANG_LANG_LIST, DS_SizeY | DS_SizeX)
        DIALOG_SIZER_END()
        DialogSizer_Set(hDlg, sz, TRUE);

        data = (Dialog_ChangeLanguage_Data*)lp;
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)data);
        if (gUseDarkModeLib) {
            DarkMode::setDarkWndSafe(hDlg);
        }
        // for non-latin languages this depends on the correct fonts being installed,
        // otherwise all the user will see are squares
        HwndSetText(hDlg, _TRA("Change Language"));
        langList = GetDlgItem(hDlg, IDC_CHANGE_LANG_LANG_LIST);
        int itemToSelect = 0;
        for (int i = 0; i < trans::GetLangsCount(); i++) {
            const char* name = trans::GetLangNameByIdx(i);
            const char* langCode = trans::GetLangCodeByIdx(i);
            auto langName = ToWStrTemp(name);
            ListBox_AppendString_NoSort(langList, langName);
            if (str::Eq(langCode, data->langCode)) {
                itemToSelect = i;
            }
        }
        ListBox_SetCurSel(langList, itemToSelect);
        // the language list is meant to be laid out left-to-right
        SetWindowExStyle(langList, WS_EX_LAYOUTRTL, false);
        HwndSetDlgItemText(hDlg, IDOK, _TRA("OK"));
        HwndSetDlgItemText(hDlg, IDCANCEL, _TRA("Cancel"));

        CenterDialog(hDlg);
        HwndSetFocus(langList);
        return FALSE;
    }

    switch (msg) {
        case WM_COMMAND:
            data = (Dialog_ChangeLanguage_Data*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
            if (HIWORD(wp) == LBN_DBLCLK) {
                ReportIf(IDC_CHANGE_LANG_LANG_LIST != LOWORD(wp));
                langList = GetDlgItem(hDlg, IDC_CHANGE_LANG_LANG_LIST);
                ReportIf(langList != (HWND)lp);
                int langIdx = (int)ListBox_GetCurSel(langList);
                data->langCode = trans::GetLangCodeByIdx(langIdx);
                EndDialog(hDlg, IDOK);
                return FALSE;
            }
            switch (LOWORD(wp)) {
                case IDOK: {
                    langList = GetDlgItem(hDlg, IDC_CHANGE_LANG_LANG_LIST);
                    int langIdx = ListBox_GetCurSel(langList);
                    data->langCode = trans::GetLangCodeByIdx(langIdx);
                    EndDialog(hDlg, IDOK);
                }
                    return TRUE;

                case IDCANCEL:
                    EndDialog(hDlg, IDCANCEL);
                    return TRUE;
            }
            break;
    }

    return FALSE;
}

/* Returns nullptr  -1 if user choses 'cancel' */
const char* Dialog_ChangeLanguge(HWND hwnd, const char* currLangCode) {
    Dialog_ChangeLanguage_Data data;
    data.langCode = currLangCode;

    INT_PTR res = CreateDialogBox(IDD_DIALOG_CHANGE_LANGUAGE, hwnd, Dialog_ChangeLanguage_Proc, (LPARAM)&data);
    if (IDCANCEL == res) {
        return nullptr;
    }
    return data.langCode;
}

TempStr ZoomLevelStr(float zoom) {
    if (zoom == kZoomFitPage) {
        return (TempStr)_TRA("Fit Page");
    }
    if (zoom == kZoomFitWidth) {
        return (TempStr)_TRA("Fit Width");
    }
    if (zoom == kZoomFitContent) {
        return (TempStr)_TRA("Fit Content");
    }
    if (zoom == 0) {
        return (TempStr) "-";
    }
    TempStr res = str::FormatTemp("%.f%%", zoom);
    return res;
}

// clang-format off
static float gZoomLevels[] = {
    kZoomFitPage,
    kZoomFitWidth,
    kZoomFitContent,
    0,
    6400.0,
    3200.0,
    1600.0,
    800.0,
    400.0,
    200.0,
    150.0,
    125.0,
    100.0,
    50.0,
    25.0,
    12.5,
    8.33f
};
static float gZoomLevelsChm[] = {
    800.0,
    400.0,
    200.0,
    150.0,
    125.0,
    100.0,
    50.0,
    25.0,
};
// clang-format on

static Vec<float>* gCurrZoomLevels = nullptr;

static void AddZoomLevel(float zoomLevel, HWND hwnd, Vec<float>* levels) {
    TempStr s = ZoomLevelStr(zoomLevel);
    CbAddString(hwnd, s);
    levels->Append(zoomLevel);
}

static void SetupZoomComboBox(HWND hDlg, UINT idComboBox, bool forChm, float currZoom) {
    HWND hwnd = GetDlgItem(hDlg, idComboBox);

    auto prefs = gGlobalPrefs;
    auto customZoomLevels = prefs->zoomLevels;
    auto currZoomLevels = new Vec<float>();
    int n = customZoomLevels->Size();
    if (n > 0) {
        if (!forChm) {
            float* zoomLevels = gZoomLevels;
            for (int i = 0; i < 4; i++) {
                AddZoomLevel(zoomLevels[i], hwnd, currZoomLevels);
            }
        }
        float maxZoom = forChm ? 800 : kZoomMax;
        float minZoom = forChm ? 16 : kZoomMin;
        for (int i = 0; i < n; i++) {
            float zl = customZoomLevels->At(n - i - 1); // largest first
            if (zl >= minZoom && zl <= maxZoom) {
                AddZoomLevel(zl, hwnd, currZoomLevels);
            }
        }
    } else {
        float* zoomLevels = forChm ? gZoomLevelsChm : gZoomLevels;
        n = forChm ? dimofi(gZoomLevelsChm) : dimofi(gZoomLevels);
        for (int i = 0; i < n; i++) {
            AddZoomLevel(zoomLevels[i], hwnd, currZoomLevels);
        }
    }

    n = currZoomLevels->Size();
    for (int i = 0; i < n; i++) {
        float zl = currZoomLevels->At(i);
        if (zl == currZoom) {
            CbSetCurrentSelection(hwnd, i);
        }
    }

    if (SendDlgItemMessage(hDlg, idComboBox, CB_GETCURSEL, 0, 0) == -1) {
        TempStr customZoom = str::FormatTemp("%.0f%%", currZoom);
        SetDlgItemTextW(hDlg, idComboBox, ToWStrTemp(customZoom));
    }
    delete gCurrZoomLevels;
    gCurrZoomLevels = currZoomLevels;
}

static float GetZoomComboBoxValue(HWND hDlg, UINT idComboBox, float defaultZoom) {
    float newZoom = defaultZoom;
    int idx = ComboBox_GetCurSel(GetDlgItem(hDlg, idComboBox));
    if (idx == -1) {
        char* customZoom = HwndGetTextTemp(GetDlgItem(hDlg, idComboBox));
        float zoom = (float)atof(customZoom);
        newZoom = limitValue(zoom, kZoomMin, kZoomMax);
        return newZoom;
    }
    newZoom = gCurrZoomLevels->At(idx);
    if (newZoom == 0) {
        newZoom = defaultZoom;
    }
    return newZoom;
}

struct Dialog_CustomZoom_Data {
    float zoomArg = 0;
    float zoomResult = 0;
    bool forChm = false;
};

static INT_PTR CALLBACK Dialog_CustomZoom_Proc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) {
    Dialog_CustomZoom_Data* data;

    switch (msg) {
        case WM_INITDIALOG:
            //[ ACCESSKEY_GROUP Zoom Dialog
            data = (Dialog_CustomZoom_Data*)lp;
            SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)data);
            if (gUseDarkModeLib) {
                DarkMode::setDarkWndSafe(hDlg);
            }
            SetupZoomComboBox(hDlg, IDC_DEFAULT_ZOOM, data->forChm, data->zoomArg);

            HwndSetText(hDlg, _TRA("Zoom factor"));
            HwndSetDlgItemText(hDlg, IDC_STATIC, _TRA("&Magnification:"));
            HwndSetDlgItemText(hDlg, IDOK, _TRA("Zoom"));
            HwndSetDlgItemText(hDlg, IDCANCEL, _TRA("Cancel"));

            CenterDialog(hDlg);
            HwndSetFocus(GetDlgItem(hDlg, IDC_DEFAULT_ZOOM));
            return FALSE;
            //] ACCESSKEY_GROUP Zoom Dialog

        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case IDOK:
                    data = (Dialog_CustomZoom_Data*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
                    data->zoomResult = GetZoomComboBoxValue(hDlg, IDC_DEFAULT_ZOOM, data->zoomArg);
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

bool Dialog_CustomZoom(HWND hwnd, bool forChm, float* currZoomInOut) {
    Dialog_CustomZoom_Data data;
    data.forChm = forChm;
    data.zoomArg = *currZoomInOut;
    INT_PTR res = CreateDialogBox(IDD_DIALOG_CUSTOM_ZOOM, hwnd, Dialog_CustomZoom_Proc, (LPARAM)&data);
    if (res == IDCANCEL) {
        return false;
    }

    *currZoomInOut = data.zoomResult;
    return true;
}

static void RemoveDialogItem(HWND hDlg, int itemId, int prevId = 0) {
    HWND hItem = GetDlgItem(hDlg, itemId);
    Rect itemRc = MapRectToWindow(WindowRect(hItem), HWND_DESKTOP, hDlg);
    // shrink by the distance to the previous item
    HWND hPrev = prevId ? GetDlgItem(hDlg, prevId) : GetWindow(hItem, GW_HWNDPREV);
    Rect prevRc = MapRectToWindow(WindowRect(hPrev), HWND_DESKTOP, hDlg);
    int shrink = itemRc.y - prevRc.y + itemRc.dy - prevRc.dy;
    // move items below up, shrink container items and hide contained items
    for (HWND item = GetWindow(hDlg, GW_CHILD); item; item = GetWindow(item, GW_HWNDNEXT)) {
        Rect rc = MapRectToWindow(WindowRect(item), HWND_DESKTOP, hDlg);
        if (rc.y >= itemRc.y + itemRc.dy) { // below
            MoveWindow(item, rc.x, rc.y - shrink, rc.dx, rc.dy, TRUE);
        } else if (rc.Intersect(itemRc) == rc) { // contained (or self)
            ShowWindow(item, SW_HIDE);
        } else if (itemRc.Intersect(rc) == itemRc) { // container
            MoveWindow(item, rc.x, rc.y, rc.dx, rc.dy - shrink, TRUE);
        }
    }
    // shrink the dialog
    Rect dlgRc = WindowRect(hDlg);
    MoveWindow(hDlg, dlgRc.x, dlgRc.y, dlgRc.dx, dlgRc.dy - shrink, TRUE);
}

static INT_PTR CALLBACK Dialog_Settings_Proc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) {
    GlobalPrefs* prefs;

    switch (msg) {
        //[ ACCESSKEY_GROUP Settings Dialog
        case WM_INITDIALOG:
            prefs = (GlobalPrefs*)lp;
            SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)prefs);
            if (gUseDarkModeLib) {
                DarkMode::setDarkWndSafe(hDlg);
            }
            {
                HWND hwndCb = GetDlgItem(hDlg, IDC_DEFAULT_LAYOUT);
                // Fill the page layouts into the select box
                CbAddString(hwndCb, _TRA("Automatic"));
                CbAddString(hwndCb, _TRA("Single Page"));
                CbAddString(hwndCb, _TRA("Facing"));
                CbAddString(hwndCb, _TRA("Book View"));
                CbAddString(hwndCb, _TRA("Continuous"));
                CbAddString(hwndCb, _TRA("Continuous Facing"));
                CbAddString(hwndCb, _TRA("Continuous Book View"));
                int selIdx = (int)prefs->defaultDisplayModeEnum - (int)DisplayMode::Automatic;
                CbSetCurrentSelection(hwndCb, selIdx);
            }

            SetupZoomComboBox(hDlg, IDC_DEFAULT_ZOOM, false, prefs->defaultZoomFloat);

            CheckDlgButton(hDlg, IDC_DEFAULT_SHOW_TOC, prefs->showToc ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_REMEMBER_STATE_PER_DOCUMENT,
                           prefs->rememberStatePerDocument ? BST_CHECKED : BST_UNCHECKED);
            EnableWindow(GetDlgItem(hDlg, IDC_REMEMBER_STATE_PER_DOCUMENT), prefs->rememberOpenedFiles);
            CheckDlgButton(hDlg, IDC_USE_TABS, prefs->useTabs ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_CHECK_FOR_UPDATES, prefs->checkForUpdates ? BST_CHECKED : BST_UNCHECKED);
            EnableWindow(GetDlgItem(hDlg, IDC_CHECK_FOR_UPDATES), HasPermission(Perm::InternetAccess));
            CheckDlgButton(hDlg, IDC_REMEMBER_OPENED_FILES, prefs->rememberOpenedFiles ? BST_CHECKED : BST_UNCHECKED);

            HwndSetText(hDlg, _TRA("SumatraPDF Options"));
            HwndSetDlgItemText(hDlg, IDC_SECTION_VIEW, _TRA("View"));
            HwndSetDlgItemText(hDlg, IDC_DEFAULT_LAYOUT_LABEL, _TRA("Default &Layout:"));
            HwndSetDlgItemText(hDlg, IDC_DEFAULT_ZOOM_LABEL, _TRA("Default &Zoom:"));
            HwndSetDlgItemText(hDlg, IDC_DEFAULT_SHOW_TOC, _TRA("Show the &bookmarks sidebar when available"));
            HwndSetDlgItemText(hDlg, IDC_REMEMBER_STATE_PER_DOCUMENT,
                               _TRA("&Remember these settings for each document"));
            HwndSetDlgItemText(hDlg, IDC_SECTION_ADVANCED, _TRA("Advanced"));
            HwndSetDlgItemText(hDlg, IDC_USE_TABS, _TRA("Use &tabs"));
            HwndSetDlgItemText(hDlg, IDC_CHECK_FOR_UPDATES, _TRA("Automatically check for &updates"));
            HwndSetDlgItemText(hDlg, IDC_REMEMBER_OPENED_FILES, _TRA("Remember &opened files"));
            HwndSetDlgItemText(hDlg, IDC_SECTION_INVERSESEARCH, _TRA("Set inverse search command-line"));
            HwndSetDlgItemText(hDlg, IDC_CMDLINE_LABEL,
                               _TRA("Enter the command-line to invoke when you double-click on the PDF document:"));
            HwndSetDlgItemText(hDlg, IDOK, _TRA("OK"));
            HwndSetDlgItemText(hDlg, IDCANCEL, _TRA("Cancel"));

            if (prefs->enableTeXEnhancements && CanAccessDisk()) {
                // Fill the combo with the list of possible inverse search commands
                // Try to select a correct default when first showing this dialog
                const char* cmdLine = prefs->inverseSearchCmdLine;
                HWND hwndComboBox = GetDlgItem(hDlg, IDC_CMDLINE);
                Vec<TextEditor*> textEditors;
                DetectTextEditors(textEditors);
                StrVec detected;
                for (auto e : textEditors) {
                    const char* open = e->openFileCmd;
                    AppendIfNotExists(&detected, open);
                }
                if (cmdLine) {
                    AppendIfNotExists(&detected, cmdLine);
                } else {
                    cmdLine = detected[0];
                }
                for (char* s : detected) {
                    // if no existing command was selected then set the user custom command in the combo
                    CbAddString(hwndComboBox, s);
                }

                // Find the index of the active command line
                TempWStr cmdLineW = ToWStrTemp(cmdLine);
                LRESULT ind = SendMessageW(hwndComboBox, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)cmdLineW);
                if (CB_ERR == ind) {
                    HwndSetDlgItemText(hDlg, IDC_CMDLINE, cmdLine);
                } else {
                    // select the active command
                    CbSetCurrentSelection(hwndComboBox, ind);
                }
            } else {
                RemoveDialogItem(hDlg, IDC_SECTION_INVERSESEARCH, IDC_SECTION_ADVANCED);
            }

            CenterDialog(hDlg);
            HwndSetFocus(GetDlgItem(hDlg, IDC_DEFAULT_LAYOUT));
            return FALSE;
            //] ACCESSKEY_GROUP Settings Dialog

        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case IDOK:
                    prefs = (GlobalPrefs*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
                    prefs->defaultDisplayModeEnum =
                        (DisplayMode)(SendDlgItemMessage(hDlg, IDC_DEFAULT_LAYOUT, CB_GETCURSEL, 0, 0) +
                                      (int)DisplayMode::Automatic);
                    prefs->defaultZoomFloat = GetZoomComboBoxValue(hDlg, IDC_DEFAULT_ZOOM, prefs->defaultZoomFloat);

                    prefs->showToc = (BST_CHECKED == IsDlgButtonChecked(hDlg, IDC_DEFAULT_SHOW_TOC));
                    prefs->rememberStatePerDocument =
                        (BST_CHECKED == IsDlgButtonChecked(hDlg, IDC_REMEMBER_STATE_PER_DOCUMENT));
                    prefs->useTabs = (BST_CHECKED == IsDlgButtonChecked(hDlg, IDC_USE_TABS));
                    prefs->checkForUpdates = (BST_CHECKED == IsDlgButtonChecked(hDlg, IDC_CHECK_FOR_UPDATES));
                    prefs->rememberOpenedFiles = (BST_CHECKED == IsDlgButtonChecked(hDlg, IDC_REMEMBER_OPENED_FILES));
                    if (prefs->enableTeXEnhancements && CanAccessDisk()) {
                        char* tmp = HwndGetTextTemp(GetDlgItem(hDlg, IDC_CMDLINE));
                        char* cmdLine = str::Dup(tmp);
                        str::ReplacePtr(&prefs->inverseSearchCmdLine, cmdLine);
                    }
                    EndDialog(hDlg, IDOK);
                    return TRUE;

                case IDCANCEL:
                    EndDialog(hDlg, IDCANCEL);
                    return TRUE;

                case IDC_REMEMBER_OPENED_FILES: {
                    bool rememberOpenedFiles = (BST_CHECKED == IsDlgButtonChecked(hDlg, IDC_REMEMBER_OPENED_FILES));
                    EnableWindow(GetDlgItem(hDlg, IDC_REMEMBER_STATE_PER_DOCUMENT), rememberOpenedFiles);
                }
                    return TRUE;

                case IDC_DEFAULT_SHOW_TOC:
                case IDC_REMEMBER_STATE_PER_DOCUMENT:
                case IDC_CHECK_FOR_UPDATES:
                    return TRUE;
            }
            break;
    }
    return FALSE;
}

INT_PTR Dialog_Settings(HWND hwnd, GlobalPrefs* prefs) {
    return CreateDialogBox(IDD_DIALOG_SETTINGS, hwnd, Dialog_Settings_Proc, (LPARAM)prefs);
}

#ifndef ID_APPLY_NOW
#define ID_APPLY_NOW 0x3021
#endif

static INT_PTR CALLBACK Sheet_Print_Advanced_Proc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) {
    Print_Advanced_Data* data;

    switch (msg) {
        //[ ACCESSKEY_GROUP Advanced Print Tab
        case WM_INITDIALOG:
            data = (Print_Advanced_Data*)((PROPSHEETPAGE*)lp)->lParam;
            SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)data);
            if (gUseDarkModeLib) {
                DarkMode::setDarkWndSafe(hDlg);
            }
            HwndSetDlgItemText(hDlg, IDC_SECTION_PRINT_RANGE, _TRA("Print range"));
            HwndSetDlgItemText(hDlg, IDC_PRINT_RANGE_ALL, _TRA("&All selected pages"));
            HwndSetDlgItemText(hDlg, IDC_PRINT_RANGE_EVEN, _TRA("&Even pages only"));
            HwndSetDlgItemText(hDlg, IDC_PRINT_RANGE_ODD, _TRA("&Odd pages only"));
            HwndSetDlgItemText(hDlg, IDC_SECTION_PRINT_SCALE, _TRA("Page scaling"));
            HwndSetDlgItemText(hDlg, IDC_PRINT_SCALE_SHRINK, _TRA("&Shrink pages to printable area (if necessary)"));
            HwndSetDlgItemText(hDlg, IDC_PRINT_SCALE_FIT, _TRA("&Fit pages to printable area"));
            HwndSetDlgItemText(hDlg, IDC_PRINT_SCALE_NONE, _TRA("&Use original page sizes"));
            HwndSetDlgItemText(hDlg, IDC_SECTION_PRINT_COMPATIBILITY, _TRA("Compatibility"));

            CheckRadioButton(hDlg, IDC_PRINT_RANGE_ALL, IDC_PRINT_RANGE_ODD,
                             data->range == PrintRangeAdv::Even  ? IDC_PRINT_RANGE_EVEN
                             : data->range == PrintRangeAdv::Odd ? IDC_PRINT_RANGE_ODD
                                                                 : IDC_PRINT_RANGE_ALL);
            CheckRadioButton(hDlg, IDC_PRINT_SCALE_SHRINK, IDC_PRINT_SCALE_NONE,
                             data->scale == PrintScaleAdv::Fit      ? IDC_PRINT_SCALE_FIT
                             : data->scale == PrintScaleAdv::Shrink ? IDC_PRINT_SCALE_SHRINK
                                                                    : IDC_PRINT_SCALE_NONE);

            return FALSE;
            //] ACCESSKEY_GROUP Advanced Print Tab

        case WM_NOTIFY:
            if (((LPNMHDR)lp)->code == PSN_APPLY) {
                data = (Print_Advanced_Data*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
                if (IsDlgButtonChecked(hDlg, IDC_PRINT_RANGE_EVEN)) {
                    data->range = PrintRangeAdv::Even;
                } else if (IsDlgButtonChecked(hDlg, IDC_PRINT_RANGE_ODD)) {
                    data->range = PrintRangeAdv::Odd;
                } else {
                    data->range = PrintRangeAdv::All;
                }
                if (IsDlgButtonChecked(hDlg, IDC_PRINT_SCALE_FIT)) {
                    data->scale = PrintScaleAdv::Fit;
                } else if (IsDlgButtonChecked(hDlg, IDC_PRINT_SCALE_SHRINK)) {
                    data->scale = PrintScaleAdv::Shrink;
                } else {
                    data->scale = PrintScaleAdv::None;
                }
                return TRUE;
            }
            break;

        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case IDC_PRINT_RANGE_ALL:
                case IDC_PRINT_RANGE_EVEN:
                case IDC_PRINT_RANGE_ODD:
                case IDC_PRINT_SCALE_SHRINK:
                case IDC_PRINT_SCALE_FIT:
                case IDC_PRINT_SCALE_NONE: {
                    HWND hApplyButton = GetDlgItem(GetParent(hDlg), ID_APPLY_NOW);
                    EnableWindow(hApplyButton, TRUE);
                } break;
            }
    }
    return FALSE;
}

HPROPSHEETPAGE CreatePrintAdvancedPropSheet(Print_Advanced_Data* data, ScopedMem<DLGTEMPLATE>& dlgTemplate) {
    PROPSHEETPAGE psp{};

    psp.dwSize = sizeof(PROPSHEETPAGE);
    psp.dwFlags = PSP_USETITLE | PSP_PREMATURE;
    psp.pszTemplate = MAKEINTRESOURCE(IDD_PROPSHEET_PRINT_ADVANCED);
    psp.pfnDlgProc = Sheet_Print_Advanced_Proc;
    psp.lParam = (LPARAM)data;
    auto s = _TRA("Advanced");
    psp.pszTitle = ToWStrTemp(s);

    if (IsUIRtl()) {
        dlgTemplate.Set(GetRtLDlgTemplate(IDD_PROPSHEET_PRINT_ADVANCED));
        psp.pResource = dlgTemplate.Get();
        psp.dwFlags |= PSP_DLGINDIRECT;
    }

    return CreatePropertySheetPage(&psp);
}

struct Dialog_AddFav_Data {
    char* pageNo = nullptr;
    char* favName = nullptr;
    ~Dialog_AddFav_Data() {
        str::Free(pageNo);
        str::Free(favName);
    }
};

static INT_PTR CALLBACK Dialog_AddFav_Proc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) {
    if (WM_INITDIALOG == msg) {
        Dialog_AddFav_Data* data = (Dialog_AddFav_Data*)lp;
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)data);
        if (gUseDarkModeLib) {
            DarkMode::setDarkWndSafe(hDlg);
        }
        HwndSetText(hDlg, _TRA("Add Favorite"));
        TempStr s = str::FormatTemp(_TRA("Add page %s to favorites with (optional) name:"), data->pageNo);
        HwndSetDlgItemText(hDlg, IDC_ADD_PAGE_STATIC, s);
        HwndSetDlgItemText(hDlg, IDOK, _TRA("OK"));
        HwndSetDlgItemText(hDlg, IDCANCEL, _TRA("Cancel"));
        if (data->favName) {
            HwndSetDlgItemText(hDlg, IDC_FAV_NAME_EDIT, data->favName);
            EditSelectAll(GetDlgItem(hDlg, IDC_FAV_NAME_EDIT));
        }
        CenterDialog(hDlg);
        HwndSetFocus(GetDlgItem(hDlg, IDC_FAV_NAME_EDIT));
        return FALSE;
    }

    if (WM_COMMAND == msg) {
        Dialog_AddFav_Data* data = (Dialog_AddFav_Data*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
        WORD cmd = LOWORD(wp);
        if (IDOK == cmd) {
            char* name = HwndGetTextTemp(GetDlgItem(hDlg, IDC_FAV_NAME_EDIT));
            str::TrimWSInPlace(name, str::TrimOpt::Both);
            if (!str::IsEmpty(name)) {
                str::ReplaceWithCopy(&data->favName, name);
            } else {
                str::FreePtr(&data->favName);
            }
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
// (passing in a non-nullptr favName will use it as default name)
bool Dialog_AddFavorite(HWND hwnd, const char* pageNo, AutoFreeStr& favName) {
    Dialog_AddFav_Data data;
    data.pageNo = str::Dup(pageNo);
    data.favName = str::Dup(favName);

    INT_PTR res = CreateDialogBox(IDD_DIALOG_FAV_ADD, hwnd, Dialog_AddFav_Proc, (LPARAM)&data);
    if (IDCANCEL == res) {
        return false;
    }

    favName.SetCopy(data.favName);
    return true;
}
