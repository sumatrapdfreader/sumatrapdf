/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "wingui/DialogSizer.h"
#include "base/Win.h"

#include "Settings.h"
#include "AppSettings.h"

#include "GlobalPrefs.h"

#include "SumatraPDF.h"
#include "resource.h"
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
    *wd = (WORD)fontSize;
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
    Str fileName;       /* name of the file for which we need the password */
    Str pwdOut;         /* password entered by the user */
    bool* remember;     /* remember the password (encrypted) or ask again? */
    bool* showPassword; /* keep the "show password" state across retries */
};

static INT_PTR CALLBACK Dialog_GetPassword_Proc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) {
    Dialog_GetPassword_Data* data;

    //[ ACCESSKEY_GROUP Password Dialog
    if (WM_INITDIALOG == msg) {
        data = (Dialog_GetPassword_Data*)lp;
        HwndSetText(hDlg, _TRA("Enter password"));
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)data);
        if (UseDarkModeLib()) {
            DarkMode::setDarkWndSafe(hDlg);
        }
        EnableWindow(GetDlgItem(hDlg, IDC_REMEMBER_PASSWORD), data->remember != nullptr);

        TempStr txt = fmt(_TRA("Enter password for %s").s, data->fileName);
        HwndSetDlgItemText(hDlg, IDC_GET_PASSWORD_LABEL, txt);
        HwndSetDlgItemText(hDlg, IDC_GET_PASSWORD_EDIT, "");
        HwndSetDlgItemText(hDlg, IDC_STATIC, _TRA("&Password:"));
        HwndSetDlgItemText(hDlg, IDC_SHOW_PASSWORD, _TRA("&Show password"));
        HwndSetDlgItemText(hDlg, IDC_REMEMBER_PASSWORD, _TRA("&Remember the password for this document"));
        HwndSetDlgItemText(hDlg, IDOK, _TRA("OK"));
        HwndSetDlgItemText(hDlg, IDCANCEL, _TRA("Cancel"));
        if (data->showPassword && *data->showPassword) {
            CheckDlgButton(hDlg, IDC_SHOW_PASSWORD, BST_CHECKED);
            HWND hwndEdit = GetDlgItem(hDlg, IDC_GET_PASSWORD_EDIT);
            SendMessageW(hwndEdit, EM_SETPASSWORDCHAR, 0, 0);
            InvalidateRect(hwndEdit, nullptr, TRUE);
        }

        CenterDialog(hDlg);
        HwndSetFocus(GetDlgItem(hDlg, IDC_GET_PASSWORD_EDIT));
        BringWindowToTop(hDlg);
        return FALSE;
    }
    //] ACCESSKEY_GROUP Password Dialog

    switch (msg) {
        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case IDOK: {
                    data = (Dialog_GetPassword_Data*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
                    TempStr tmp = HwndGetTextTemp(GetDlgItem(hDlg, IDC_GET_PASSWORD_EDIT));
                    data->pwdOut = str::Dup(tmp);
                    if (data->remember) {
                        *data->remember = BST_CHECKED == IsDlgButtonChecked(hDlg, IDC_REMEMBER_PASSWORD);
                    }
                    EndDialog(hDlg, IDOK);
                    return TRUE;
                }

                case IDCANCEL:
                    EndDialog(hDlg, IDCANCEL);
                    return TRUE;

                case IDC_SHOW_PASSWORD: {
                    HWND hwndEdit = GetDlgItem(hDlg, IDC_GET_PASSWORD_EDIT);
                    bool show = BST_CHECKED == IsDlgButtonChecked(hDlg, IDC_SHOW_PASSWORD);
                    data = (Dialog_GetPassword_Data*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
                    if (data && data->showPassword) {
                        *data->showPassword = show;
                    }
                    SendMessageW(hwndEdit, EM_SETPASSWORDCHAR, show ? 0 : (WPARAM)L'\x25CF', 0);
                    InvalidateRect(hwndEdit, nullptr, TRUE);
                    return TRUE;
                }
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
Str Dialog_GetPassword(HWND hwndParent, Str fileName, bool* rememberPassword, bool* showPassword) {
    Dialog_GetPassword_Data data;
    data.fileName = fileName;
    data.remember = rememberPassword;
    data.showPassword = showPassword;

    INT_PTR res = CreateDialogBox(IDD_DIALOG_GET_PASSWORD, hwndParent, Dialog_GetPassword_Proc, (LPARAM)&data);
    if (IDOK != res) {
        str::Free(data.pwdOut);
        return Str();
    }
    return str::Dup(data.pwdOut);
}

/* For passing data to/from GoToPage dialog */
struct Dialog_GoToPage_Data {
    Str currPageLabel;        // currently shown page label
    int pageCount = 0;        // total number of pages
    bool onlyNumeric = false; // whether the page label must be numeric
    Str newPageLabel;         // page number entered by user

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
        if (UseDarkModeLib()) {
            DarkMode::setDarkWndSafe(hDlg);
        }
        HwndSetText(hDlg, _TRA("Go to page"));

        editPageNo = GetDlgItem(hDlg, IDC_GOTO_PAGE_EDIT);
        if (!data->onlyNumeric) {
            SetWindowLong(editPageNo, GWL_STYLE, GetWindowLong(editPageNo, GWL_STYLE) & ~ES_NUMBER);
        }
        ReportIf(!data->currPageLabel);
        HwndSetDlgItemText(hDlg, IDC_GOTO_PAGE_EDIT, data->currPageLabel);
        TempStr totalCount = fmt(_TRA("(of %d)").s, data->pageCount);
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

    switch (msg) {
        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case IDOK: {
                    data = (Dialog_GoToPage_Data*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
                    editPageNo = GetDlgItem(hDlg, IDC_GOTO_PAGE_EDIT);
                    TempStr tmp = HwndGetTextTemp(editPageNo);
                    str::ReplaceWithCopy(&data->newPageLabel, tmp);
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

/* Shows a 'go to page' dialog and returns the page label entered by the user
   or nullptr if user clicked the "cancel" button or there was an error.
   The caller must free() the result. */
Str Dialog_GoToPage(HWND hwnd, Str currentPageLabel, int pageCount, bool onlyNumeric) {
    Dialog_GoToPage_Data data;
    data.currPageLabel = str::Dup(currentPageLabel);
    data.pageCount = pageCount;
    data.onlyNumeric = onlyNumeric;
    CreateDialogBox(IDD_DIALOG_GOTO_PAGE, hwnd, Dialog_GoToPage_Proc, (LPARAM)&data);
    return str::Dup(data.newPageLabel);
}

/* For passing data to/from Find dialog */
struct Dialog_Find_Data {
    Str searchTerm;
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
            if (UseDarkModeLib()) {
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
            str::Free(data->searchTerm);
            data->searchTerm = {};
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
Str Dialog_Find(HWND hwnd, Str previousSearch, bool* matchCase) {
    Dialog_Find_Data data;
    data.searchTerm = str::DupTemp(previousSearch);
    data.matchCase = matchCase ? *matchCase : false;
    INT_PTR res = CreateDialogBox(IDD_DIALOG_FIND, hwnd, Dialog_Find_Proc, (LPARAM)&data);
    if (res != IDOK) {
        str::Free(data.searchTerm);
        return Str();
    }

    if (matchCase) {
        *matchCase = data.matchCase;
    }
    return str::Dup(data.searchTerm);
}

/* For passing data to/from ChangeLanguage dialog */
struct Dialog_ChangeLanguage_Data {
    Str langCode;
};

// maps listbox index to lang index when filtered
static Vec<int>* gLangListMap = nullptr;

static void FilterLangList(HWND hDlg, Str filter, Str currLangCode) {
    HWND langList = GetDlgItem(hDlg, IDC_CHANGE_LANG_LANG_LIST);
    ListBox_ResetContent(langList);

    delete gLangListMap;
    gLangListMap = new Vec<int>();

    int itemToSelect = 0;
    for (int i = 0; i < trans::GetLangsCount(); i++) {
        TempStr name = trans::GetLangNameByIdxTemp(i);
        if (filter && !str::ContainsI(name, filter)) {
            continue;
        }
        auto langName = ToWStrTemp(name);
        ListBox_AppendString_NoSort(langList, langName);
        TempStr langCode = trans::GetLangCodeByIdxTemp(i);
        if (str::Eq(langCode, currLangCode)) {
            itemToSelect = len(*gLangListMap);
        }
        gLangListMap->Append(i);
    }
    if (len(*gLangListMap) > 0) {
        ListBox_SetCurSel(langList, itemToSelect);
    }
}

static INT_PTR CALLBACK Dialog_ChangeLanguage_Proc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) {
    Dialog_ChangeLanguage_Data* data;
    HWND langList;

    if (WM_INITDIALOG == msg) {
        DIALOG_SIZER_START(sz)
        DIALOG_SIZER_ENTRY(IDOK, DS_MoveX | DS_MoveY)
        DIALOG_SIZER_ENTRY(IDCANCEL, DS_MoveX | DS_MoveY)
        DIALOG_SIZER_ENTRY(IDC_CHANGE_LANG_SEARCH, DS_SizeX)
        DIALOG_SIZER_ENTRY(IDC_CHANGE_LANG_LANG_LIST, DS_SizeY | DS_SizeX)
        DIALOG_SIZER_END()
        DialogSizer_Set(hDlg, sz, TRUE);

        data = (Dialog_ChangeLanguage_Data*)lp;
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)data);
        if (UseDarkModeLib()) {
            DarkMode::setDarkWndSafe(hDlg);
        }
        // for non-latin languages this depends on the correct fonts being installed,
        // otherwise all the user will see are squares
        HwndSetText(hDlg, _TRA("Change Language"));

        FilterLangList(hDlg, nullptr, data->langCode);

        langList = GetDlgItem(hDlg, IDC_CHANGE_LANG_LANG_LIST);
        // the language list is meant to be laid out left-to-right
        SetWindowExStyle(langList, WS_EX_LAYOUTRTL, false);
        HwndSetDlgItemText(hDlg, IDOK, _TRA("OK"));
        HwndSetDlgItemText(hDlg, IDCANCEL, _TRA("Cancel"));

        CenterDialog(hDlg);
        HwndSetFocus(GetDlgItem(hDlg, IDC_CHANGE_LANG_SEARCH));
        return FALSE;
    }

    switch (msg) {
        case WM_COMMAND:
            data = (Dialog_ChangeLanguage_Data*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
            if (LOWORD(wp) == IDC_CHANGE_LANG_SEARCH && HIWORD(wp) == EN_CHANGE) {
                TempStr filter = HwndGetTextTemp(GetDlgItem(hDlg, IDC_CHANGE_LANG_SEARCH));
                FilterLangList(hDlg, filter, data->langCode);
                return TRUE;
            }
            if (HIWORD(wp) == LBN_DBLCLK) {
                ReportIf(IDC_CHANGE_LANG_LANG_LIST != LOWORD(wp));
                langList = GetDlgItem(hDlg, IDC_CHANGE_LANG_LANG_LIST);
                ReportIf(langList != (HWND)lp);
                int idx = (int)ListBox_GetCurSel(langList);
                if (gLangListMap && idx >= 0 && idx < len(*gLangListMap)) {
                    int langIdx = gLangListMap->At(idx);
                    data->langCode = trans::GetLangCodeByIdxTemp(langIdx);
                    EndDialog(hDlg, IDOK);
                }
                return FALSE;
            }
            switch (LOWORD(wp)) {
                case IDOK: {
                    langList = GetDlgItem(hDlg, IDC_CHANGE_LANG_LANG_LIST);
                    int idx = ListBox_GetCurSel(langList);
                    if (gLangListMap && idx >= 0 && idx < len(*gLangListMap)) {
                        int langIdx = gLangListMap->At(idx);
                        data->langCode = trans::GetLangCodeByIdxTemp(langIdx);
                    }
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
Str Dialog_ChangeLanguge(HWND hwnd, Str currLangCode) {
    Dialog_ChangeLanguage_Data data;
    data.langCode = currLangCode;

    INT_PTR res = CreateDialogBox(IDD_DIALOG_CHANGE_LANGUAGE, hwnd, Dialog_ChangeLanguage_Proc, (LPARAM)&data);
    delete gLangListMap;
    gLangListMap = nullptr;
    if (IDCANCEL == res) {
        return Str();
    }
    return str::Dup(data.langCode);
}

TempStr ZoomLevelStr(float zoom) {
    if (zoom == kZoomFitPage) {
        return _TRA("Fit Page");
    }
    if (zoom == kZoomFitWidth) {
        return _TRA("Fit Width");
    }
    if (zoom == kZoomFitContent) {
        return _TRA("Fit Content");
    }
    if (zoom == kZoomShrinkToFit) {
        return _TRA("Shrink To Fit");
    }
    if (zoom == kZoomFitByOrientation) {
        return _TRA("Fit by Orientation");
    }
    if (zoom == 0) {
        return "-";
    }
    TempStr res = fmt("%.f%%", zoom);
    return res;
}

// clang-format off
static float gZoomLevels[] = {
    kZoomFitPage,
    kZoomFitWidth,
    kZoomFitByOrientation,
    kZoomFitContent,
    kZoomShrinkToFit,
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
    int n = len(*customZoomLevels);
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

    n = len(*currZoomLevels);
    for (int i = 0; i < n; i++) {
        float zl = currZoomLevels->At(i);
        if (zl == currZoom) {
            CbSetCurrentSelection(hwnd, i);
        }
    }

    if (SendDlgItemMessage(hDlg, idComboBox, CB_GETCURSEL, 0, 0) == -1) {
        TempStr customZoom = fmt("%.0f%%", currZoom);
        SetDlgItemTextW(hDlg, idComboBox, CWStrTemp(customZoom));
    }
    delete gCurrZoomLevels;
    gCurrZoomLevels = currZoomLevels;
}

static float GetZoomComboBoxValue(HWND hDlg, UINT idComboBox, float defaultZoom) {
    float newZoom = defaultZoom;
    int idx = ComboBox_GetCurSel(GetDlgItem(hDlg, idComboBox));
    if (idx == -1) {
        TempStr customZoom = HwndGetTextTemp(GetDlgItem(hDlg, idComboBox));
        float zoom = (float)atof(customZoom.s);
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
            if (UseDarkModeLib()) {
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

static INT_PTR CALLBACK Dialog_ChangeScrollbar_Proc(HWND hDlg, UINT msg, WPARAM wp, LPARAM) {
    switch (msg) {
        case WM_INITDIALOG: {
            if (UseDarkModeLib()) {
                DarkMode::setDarkWndSafe(hDlg);
            }
            Str s = gGlobalPrefs->scrollbars;
            int checkId = IDC_SCROLLBAR_WINDOWS;
            if (str::EqI(s, "smart")) {
                checkId = IDC_SCROLLBAR_SMART;
            } else if (str::EqI(s, "overlay")) {
                checkId = IDC_SCROLLBAR_OVERLAY;
            } else if (str::EqI(s, "hidden")) {
                checkId = IDC_SCROLLBAR_HIDDEN;
            }
            CheckRadioButton(hDlg, IDC_SCROLLBAR_WINDOWS, IDC_SCROLLBAR_HIDDEN, checkId);
            HwndSetText(hDlg, _TRA("Change Scrollbar"));
            HwndSetDlgItemText(hDlg, IDOK, _TRA("OK"));
            HwndSetDlgItemText(hDlg, IDCANCEL, _TRA("Cancel"));
            CenterDialog(hDlg);
            return TRUE;
        }
        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case IDOK: {
                    Str val = "windows";
                    if (IsDlgButtonChecked(hDlg, IDC_SCROLLBAR_SMART) == BST_CHECKED) {
                        val = "smart";
                    } else if (IsDlgButtonChecked(hDlg, IDC_SCROLLBAR_OVERLAY) == BST_CHECKED) {
                        val = "overlay";
                    } else if (IsDlgButtonChecked(hDlg, IDC_SCROLLBAR_HIDDEN) == BST_CHECKED) {
                        val = "hidden";
                    }
                    str::ReplaceWithCopy(&gGlobalPrefs->scrollbars, val);
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

bool Dialog_ChangeScrollbar(HWND hwnd) {
    INT_PTR res = CreateDialogBox(IDD_DIALOG_CHANGE_SCROLLBAR, hwnd, Dialog_ChangeScrollbar_Proc, 0);
    return res == IDOK;
}

static void FillInverseSearchCombo(HWND hwndComboBox, Str cmdLine) {
    Vec<TextEditor*> textEditors;
    DetectTextEditors(textEditors);
    StrVec detected;
    for (auto e : textEditors) {
        AppendIfNotExists(&detected, e->openFileCmd);
    }
    if (cmdLine) {
        AppendIfNotExists(&detected, cmdLine);
    } else if (len(detected) > 0) {
        cmdLine = detected.At(0);
    }
    for (Str s : detected) {
        CbAddString(hwndComboBox, s);
    }
    if (!cmdLine) {
        return;
    }
    WCHAR* cmdLineW = CWStrTemp(cmdLine);
    LRESULT ind = SendMessageW(hwndComboBox, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)cmdLineW);
    if (CB_ERR == ind) {
        HwndSetText(hwndComboBox, cmdLine);
    } else {
        CbSetCurrentSelection(hwndComboBox, (int)ind);
    }
}

static void ApplyInverseSearchSettings(GlobalPrefs* prefs, HWND hwndComboBox) {
    TempStr tmp = HwndGetTextTemp(hwndComboBox);
    str::ReplaceWithCopy(&prefs->inverseSearchCmdLine, tmp);
    prefs->enableTeXEnhancements = true;
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
            if (UseDarkModeLib()) {
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
                FillInverseSearchCombo(GetDlgItem(hDlg, IDC_CMDLINE), prefs->inverseSearchCmdLine);
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
                        TempStr tmp = HwndGetTextTemp(GetDlgItem(hDlg, IDC_CMDLINE));
                        str::ReplaceWithCopy(&prefs->inverseSearchCmdLine, tmp);
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

static INT_PTR CALLBACK Dialog_SetInverseSearch_Proc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) {
    GlobalPrefs* prefs;

    switch (msg) {
        case WM_INITDIALOG:
            prefs = (GlobalPrefs*)lp;
            SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)prefs);
            if (UseDarkModeLib()) {
                DarkMode::setDarkWndSafe(hDlg);
            }
            HwndSetText(hDlg, _TRA("Set inverse search command-line"));
            HwndSetDlgItemText(hDlg, IDC_SECTION_INVERSESEARCH, _TRA("Set inverse search command-line"));
            HwndSetDlgItemText(hDlg, IDC_CMDLINE_LABEL,
                               _TRA("Enter the command-line to invoke when you double-click on the PDF document:"));
            HwndSetDlgItemText(hDlg, IDC_INVERSE_SEARCH_HELP, _TRA("Help"));
            HwndSetDlgItemText(hDlg, IDOK, _TRA("OK"));
            HwndSetDlgItemText(hDlg, IDCANCEL, _TRA("Cancel"));
            FillInverseSearchCombo(GetDlgItem(hDlg, IDC_CMDLINE), prefs->inverseSearchCmdLine);
            CenterDialog(hDlg);
            HwndSetFocus(GetDlgItem(hDlg, IDC_CMDLINE));
            return FALSE;

        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case IDOK:
                    prefs = (GlobalPrefs*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
                    ApplyInverseSearchSettings(prefs, GetDlgItem(hDlg, IDC_CMDLINE));
                    EndDialog(hDlg, IDOK);
                    return TRUE;

                case IDCANCEL:
                    EndDialog(hDlg, IDCANCEL);
                    return TRUE;

                case IDC_INVERSE_SEARCH_HELP:
                    LaunchDocumentation("/LaTeX-integration");
                    return TRUE;
            }
            break;
    }
    return FALSE;
}

bool Dialog_SetInverseSearch(HWND hwnd, GlobalPrefs* prefs) {
    if (!CanAccessDisk() || !HasPermission(Perm::SavePreferences)) {
        return false;
    }
    INT_PTR res = CreateDialogBox(IDD_DIALOG_INVERSE_SEARCH, hwnd, Dialog_SetInverseSearch_Proc, (LPARAM)prefs);
    return res == IDOK;
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
            if (UseDarkModeLib()) {
                DarkMode::setDarkWndSafe(hDlg);
            }
            HwndSetDlgItemText(hDlg, IDC_SECTION_PRINT_RANGE, _TRA("Print range"));
            HwndSetDlgItemText(hDlg, IDC_PRINT_RANGE_ALL, _TRA("&All selected pages"));
            HwndSetDlgItemText(hDlg, IDC_PRINT_RANGE_EVEN, _TRA("&Even pages only"));
            HwndSetDlgItemText(hDlg, IDC_PRINT_RANGE_ODD, _TRA("&Odd pages only"));
            HwndSetDlgItemText(hDlg, IDC_SECTION_PRINT_SCALE, _TRA("Page scaling"));
            HwndSetDlgItemText(hDlg, IDC_PRINT_SCALE_SHRINK, _TRA("&Shrink pages to printable area (if necessary)"));
            HwndSetDlgItemText(hDlg, IDC_PRINT_SCALE_FIT, _TRA("&Fit pages to printable area"));
            HwndSetDlgItemText(hDlg, IDC_PRINT_SCALE_STRETCH,
                               _TRA("S&tretch pages to fill paper (ignore aspect ratio)"));
            HwndSetDlgItemText(hDlg, IDC_PRINT_SCALE_NONE, _TRA("&Use original page sizes"));
            HwndSetDlgItemText(hDlg, IDC_PRINT_CENTER_HORIZONTALLY, _TRA("Center page hori&zontally on the paper"));
            HwndSetDlgItemText(hDlg, IDC_PRINT_PAPER_SOURCE_BY_SIZE,
                               _TRA("Choose &paper source by document page size"));
            HwndSetDlgItemText(hDlg, IDC_PRINT_PER_PAGE_PAPER_SIZE,
                               _TRA("Print each page at its &document page size (mixed sizes)"));
            HwndSetDlgItemText(hDlg, IDC_PRINT_ROTATE_LABEL, _TRA("&Rotate printout:"));
            {
                HWND hwndCb = GetDlgItem(hDlg, IDC_PRINT_ROTATE);
                CbAddString(hwndCb, _TRA("None"));
                CbAddString(hwndCb, "90°");
                CbAddString(hwndCb, "180°");
                CbAddString(hwndCb, "270°");
                int rotIdx = (data->extraRotation / 90) % 4;
                CbSetCurrentSelection(hwndCb, rotIdx);
            }
            HwndSetDlgItemText(hDlg, IDC_SECTION_PRINT_COMPATIBILITY, _TRA("Compatibility"));

            CheckRadioButton(hDlg, IDC_PRINT_RANGE_ALL, IDC_PRINT_RANGE_ODD,
                             data->range == PrintRangeAdv::Even  ? IDC_PRINT_RANGE_EVEN
                             : data->range == PrintRangeAdv::Odd ? IDC_PRINT_RANGE_ODD
                                                                 : IDC_PRINT_RANGE_ALL);
            CheckRadioButton(hDlg, IDC_PRINT_SCALE_SHRINK, IDC_PRINT_SCALE_STRETCH,
                             data->scale == PrintScaleAdv::Fit       ? IDC_PRINT_SCALE_FIT
                             : data->scale == PrintScaleAdv::Stretch ? IDC_PRINT_SCALE_STRETCH
                             : data->scale == PrintScaleAdv::Shrink  ? IDC_PRINT_SCALE_SHRINK
                                                                     : IDC_PRINT_SCALE_NONE);
            CheckDlgButton(hDlg, IDC_PRINT_CENTER_HORIZONTALLY, data->centerHorizontally ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_PRINT_PAPER_SOURCE_BY_SIZE,
                           data->paperSourceByPageSize ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_PRINT_PER_PAGE_PAPER_SIZE, data->perPagePaperSize ? BST_CHECKED : BST_UNCHECKED);

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
                } else if (IsDlgButtonChecked(hDlg, IDC_PRINT_SCALE_STRETCH)) {
                    data->scale = PrintScaleAdv::Stretch;
                } else if (IsDlgButtonChecked(hDlg, IDC_PRINT_SCALE_SHRINK)) {
                    data->scale = PrintScaleAdv::Shrink;
                } else {
                    data->scale = PrintScaleAdv::None;
                }
                data->centerHorizontally = IsDlgButtonChecked(hDlg, IDC_PRINT_CENTER_HORIZONTALLY) != 0;
                data->paperSourceByPageSize = IsDlgButtonChecked(hDlg, IDC_PRINT_PAPER_SOURCE_BY_SIZE) != 0;
                data->perPagePaperSize = IsDlgButtonChecked(hDlg, IDC_PRINT_PER_PAGE_PAPER_SIZE) != 0;
                int rotIdx = (int)SendDlgItemMessage(hDlg, IDC_PRINT_ROTATE, CB_GETCURSEL, 0, 0);
                data->extraRotation = rotIdx > 0 ? rotIdx * 90 : 0;
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
                case IDC_PRINT_SCALE_STRETCH:
                case IDC_PRINT_SCALE_NONE:
                case IDC_PRINT_CENTER_HORIZONTALLY:
                case IDC_PRINT_PAPER_SOURCE_BY_SIZE:
                case IDC_PRINT_PER_PAGE_PAPER_SIZE: {
                    HWND hApplyButton = GetDlgItem(GetParent(hDlg), ID_APPLY_NOW);
                    EnableWindow(hApplyButton, TRUE);
                } break;
                case IDC_PRINT_ROTATE:
                    if (HIWORD(wp) == CBN_SELCHANGE) {
                        EnableWindow(GetDlgItem(GetParent(hDlg), ID_APPLY_NOW), TRUE);
                    }
                    break;
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
    psp.pszTitle = CWStrTemp(s);

    if (IsUIRtl()) {
        dlgTemplate.Set(GetRtLDlgTemplate(IDD_PROPSHEET_PRINT_ADVANCED));
        psp.pResource = dlgTemplate.Get();
        psp.dwFlags |= PSP_DLGINDIRECT;
    }

    return CreatePropertySheetPage(&psp);
}

struct Dialog_AddFav_Data {
    Str pageNo;
    Str favName;
    ~Dialog_AddFav_Data() {
        str::Free(pageNo);
        str::Free(favName);
    }
};

static INT_PTR CALLBACK Dialog_AddFav_Proc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) {
    if (WM_INITDIALOG == msg) {
        Dialog_AddFav_Data* data = (Dialog_AddFav_Data*)lp;
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)data);
        if (UseDarkModeLib()) {
            DarkMode::setDarkWndSafe(hDlg);
        }
        HwndSetText(hDlg, _TRA("Add Favorite"));
        TempStr s = fmt(_TRA("Add page %s to favorites with (optional) name:").s, data->pageNo);
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
            TempStr name = HwndGetTextTemp(GetDlgItem(hDlg, IDC_FAV_NAME_EDIT));
            str::TrimWSInPlace(name, str::TrimOpt::Both);
            if (len(name) > 0) {
                str::ReplaceWithCopy(&data->favName, name);
            } else {
                str::Free(data->favName);
                data->favName = {};
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
// --- Change Background Color dialog ---

static const int kMaxCustomColors = 13;

struct BgColorDlgData {
    COLORREF currentColor; // current selected color
    bool isCheckered;      // true if "checkered" is selected
    bool applyToAll;       // radio: all files like this
    COLORREF customColors[kMaxCustomColors];
    bool customColorSet[kMaxCustomColors]; // true if slot has a color
    bool customColorsChanged;
    int selectedCustomIdx; // -1 = no custom button selected
    bool previewSelected;  // true if preview button is selected
    Str title;             // dialog title (empty = default)
    bool showRadioButtons; // show "this file" / "all files" radio buttons
    Str allFilesLabel;     // label for "all files" radio button (empty = default)
};

// fixed preset colors: checkered, black, white
static const COLORREF kBgPresetColors[] = {
    kColorUnset,        // checkered
    RGB(0, 0, 0),       // black
    RGB(255, 255, 255), // white
};
static const int kNumPresets = 3;

static void ParseCustomColors(BgColorDlgData* data) {
    for (int i = 0; i < kMaxCustomColors; i++) {
        data->customColorSet[i] = false;
        data->customColors[i] = 0;
    }
    data->customColorsChanged = false;
    Str s = gGlobalPrefs->customColors;
    if (len(s) == 0) {
        return;
    }
    int idx = 0;
    int i = 0;
    while (i < s.len && idx < kMaxCustomColors) {
        while (i < s.len && s.s[i] == ' ') {
            i++;
        }
        if (i >= s.len) {
            break;
        }
        int start = i;
        while (i < s.len && s.s[i] != ' ') {
            i++;
        }
        ParsedColor parsed;
        ParseColor(parsed, Str(s.s + start, i - start));
        if (parsed.parsedOk) {
            data->customColors[idx] = parsed.col;
            data->customColorSet[idx] = true;
            idx++;
        }
    }
}

static void SaveCustomColors(BgColorDlgData* data) {
    str::Builder buf;
    for (int i = 0; i < kMaxCustomColors; i++) {
        if (!data->customColorSet[i]) {
            continue;
        }
        if (len(buf) > 0) {
            buf.AppendChar(' ');
        }
        TempStr cs = SerializeColorTemp(data->customColors[i]);
        buf.Append(cs);
    }
    str::ReplaceWithCopy(&gGlobalPrefs->customColors, ToStr(buf));
    SaveSettings();
}

static void HsvToRgb(float h, float s, float v, u8& r, u8& g, u8& b) {
    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    float rf, gf, bf;
    if (h < 60) {
        rf = c;
        gf = x;
        bf = 0;
    } else if (h < 120) {
        rf = x;
        gf = c;
        bf = 0;
    } else if (h < 180) {
        rf = 0;
        gf = c;
        bf = x;
    } else if (h < 240) {
        rf = 0;
        gf = x;
        bf = c;
    } else if (h < 300) {
        rf = x;
        gf = 0;
        bf = c;
    } else {
        rf = c;
        gf = 0;
        bf = x;
    }
    r = (u8)((rf + m) * 255.0f);
    g = (u8)((gf + m) * 255.0f);
    b = (u8)((bf + m) * 255.0f);
}

static void PaintColorArea(HDC hdc, RECT* rc) {
    int w = rc->right - rc->left;
    int h = rc->bottom - rc->top;
    if (w <= 0 || h <= 0) {
        return;
    }
    // rows must be DWORD-aligned; each pixel is 3 bytes (BGR)
    int stride = (w * 3 + 3) & ~3;
    u8* bits = (u8*)malloc(stride * h);
    if (!bits) {
        return;
    }
    for (int y = 0; y < h; y++) {
        float val = 1.0f - (float)y / (float)h;
        u8* row = bits + y * stride;
        for (int x = 0; x < w; x++) {
            float hue = (float)x / (float)w * 360.0f;
            u8 r, g, b;
            HsvToRgb(hue, 1.0f, val, r, g, b);
            row[x * 3] = b;
            row[x * 3 + 1] = g;
            row[x * 3 + 2] = r;
        }
    }
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;
    SetDIBitsToDevice(hdc, rc->left, rc->top, w, h, 0, 0, 0, h, bits, &bmi, DIB_RGB_COLORS);
    free(bits);
}

static void SelectPreviewButton(HWND hDlg, BgColorDlgData* data) {
    int prevCustom = data->selectedCustomIdx;
    bool wasPreview = data->previewSelected;
    data->selectedCustomIdx = -1;
    data->previewSelected = true;
    if (prevCustom >= 0) {
        InvalidateRect(GetDlgItem(hDlg, IDC_BGCOL_CUSTOM_FIRST + prevCustom), nullptr, TRUE);
    }
    if (!wasPreview) {
        InvalidateRect(GetDlgItem(hDlg, IDC_BGCOL_PREVIEW), nullptr, TRUE);
    }
}

static void SelectCustomButton(HWND hDlg, BgColorDlgData* data, int idx) {
    int prevCustom = data->selectedCustomIdx;
    bool wasPreview = data->previewSelected;
    data->selectedCustomIdx = idx;
    data->previewSelected = false;
    if (prevCustom >= 0 && prevCustom != idx) {
        InvalidateRect(GetDlgItem(hDlg, IDC_BGCOL_CUSTOM_FIRST + prevCustom), nullptr, TRUE);
    }
    if (wasPreview) {
        InvalidateRect(GetDlgItem(hDlg, IDC_BGCOL_PREVIEW), nullptr, TRUE);
    }
    InvalidateRect(GetDlgItem(hDlg, IDC_BGCOL_CUSTOM_FIRST + idx), nullptr, TRUE);
}

static void InvalidatePreview(HWND hDlg, BgColorDlgData* data) {
    if (data->selectedCustomIdx >= 0) {
        InvalidateRect(GetDlgItem(hDlg, IDC_BGCOL_CUSTOM_FIRST + data->selectedCustomIdx), nullptr, TRUE);
    }
    if (data->previewSelected) {
        InvalidateRect(GetDlgItem(hDlg, IDC_BGCOL_PREVIEW), nullptr, TRUE);
    }
}

static void UpdateBgColorEditFromColor(HWND hDlg, BgColorDlgData* data) {
    if (data->isCheckered) {
        HwndSetDlgItemText(hDlg, IDC_BGCOL_EDIT, data->showRadioButtons ? "checkered" : "unset");
    } else {
        TempStr s = SerializeColorTemp(data->currentColor);
        HwndSetDlgItemText(hDlg, IDC_BGCOL_EDIT, s);
    }
    // update selected custom button color and refresh preview
    if (data->selectedCustomIdx >= 0 && !data->isCheckered) {
        data->customColors[data->selectedCustomIdx] = data->currentColor;
        data->customColorSet[data->selectedCustomIdx] = true;
        data->customColorsChanged = true;
    }
    InvalidatePreview(hDlg, data);
}

static bool TryParseBgColorEdit(HWND hDlg, BgColorDlgData* data) {
    TempStr text = HwndGetTextTemp(GetDlgItem(hDlg, IDC_BGCOL_EDIT));
    if (!text || !text.s[0]) {
        return false;
    }
    ParsedColor parsed;
    ParseColor(parsed, text);
    if (!parsed.parsedOk) {
        return false;
    }
    if (parsed.col == kColorUnset) {
        data->isCheckered = true;
    } else {
        data->isCheckered = false;
        data->currentColor = parsed.col;
    }
    return true;
}

static void PickColorFromArea(HWND hwndCA, BgColorDlgData* data, HWND hDlg) {
    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(hwndCA, &pt);
    HDC hdcCA = GetDC(hwndCA);
    COLORREF picked = GetPixel(hdcCA, pt.x, pt.y);
    ReleaseDC(hwndCA, hdcCA);
    if (picked != CLR_INVALID) {
        data->isCheckered = false;
        data->currentColor = picked;
        UpdateBgColorEditFromColor(hDlg, data);
    }
}

static WNDPROC gOrigColorAreaProc = nullptr;

static LRESULT CALLBACK ColorAreaSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    HWND hDlg = GetParent(hwnd);
    BgColorDlgData* data = (BgColorDlgData*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);

    switch (msg) {
        case WM_LBUTTONDOWN:
            SetCapture(hwnd);
            PickColorFromArea(hwnd, data, hDlg);
            return 0;
        case WM_MOUSEMOVE:
            if (wp & MK_LBUTTON) {
                PickColorFromArea(hwnd, data, hDlg);
            }
            return 0;
        case WM_LBUTTONUP:
            ReleaseCapture();
            return 0;
    }
    return CallWindowProcW(gOrigColorAreaProc, hwnd, msg, wp, lp);
}

static INT_PTR CALLBACK Dialog_ChangeBgColor_Proc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) {
    BgColorDlgData* data;
    if (msg == WM_INITDIALOG) {
        data = (BgColorDlgData*)lp;
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)data);
    } else {
        data = (BgColorDlgData*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
    }

    switch (msg) {
        case WM_INITDIALOG: {
            if (UseDarkModeLib()) {
                DarkMode::setDarkWndSafe(hDlg);
            }
            HwndSetText(hDlg, data->title ? data->title : _TRA("Change Background Color"));
            HwndSetDlgItemText(hDlg, IDOK, _TRA("OK"));
            HwndSetDlgItemText(hDlg, IDCANCEL, _TRA("Cancel"));
            if (data->showRadioButtons) {
                if (data->allFilesLabel) {
                    HwndSetDlgItemText(hDlg, IDC_BGCOL_ALL_FILES, data->allFilesLabel);
                }
                CheckRadioButton(hDlg, IDC_BGCOL_THIS_FILE, IDC_BGCOL_ALL_FILES,
                                 data->applyToAll ? IDC_BGCOL_ALL_FILES : IDC_BGCOL_THIS_FILE);
            } else {
                ShowWindow(GetDlgItem(hDlg, IDC_BGCOL_THIS_FILE), SW_HIDE);
                ShowWindow(GetDlgItem(hDlg, IDC_BGCOL_ALL_FILES), SW_HIDE);
            }
            ParseCustomColors(data);
            UpdateBgColorEditFromColor(hDlg, data);
            // subclass color area for mouse drag tracking
            HWND hwndCA = GetDlgItem(hDlg, IDC_BGCOL_COLORAREA);
            gOrigColorAreaProc = (WNDPROC)SetWindowLongPtrW(hwndCA, GWLP_WNDPROC, (LONG_PTR)ColorAreaSubclassProc);
            CenterDialog(hDlg);
            return TRUE;
        }

        case WM_DRAWITEM: {
            DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lp;
            int ctlId = (int)dis->CtlID;
            if (ctlId == IDC_BGCOL_COLORAREA) {
                PaintColorArea(dis->hDC, &dis->rcItem);
                return TRUE;
            }
            // preview button shows the currently selected color
            if (ctlId == IDC_BGCOL_PREVIEW) {
                RECT rc = dis->rcItem;
                if (data->previewSelected) {
                    FillRect(dis->hDC, &rc, (HBRUSH)(COLOR_HIGHLIGHT + 1));
                    InflateRect(&rc, -3, -3);
                }
                if (data->isCheckered) {
                    PaintCheckerboard(dis->hDC, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
                } else {
                    HBRUSH br = CreateSolidBrush(data->currentColor);
                    FillRect(dis->hDC, &rc, br);
                    DeleteObject(br);
                }
                return TRUE;
            }
            // preset color buttons
            if (ctlId >= IDC_BGCOL_PRESET_FIRST && ctlId < IDC_BGCOL_PRESET_FIRST + kNumPresets) {
                int idx = ctlId - IDC_BGCOL_PRESET_FIRST;
                COLORREF col = kBgPresetColors[idx];
                if (col == kColorUnset) {
                    PaintCheckerboard(dis->hDC, dis->rcItem.left, dis->rcItem.top, dis->rcItem.right - dis->rcItem.left,
                                      dis->rcItem.bottom - dis->rcItem.top);
                } else {
                    HBRUSH br = CreateSolidBrush(col);
                    FillRect(dis->hDC, &dis->rcItem, br);
                    DeleteObject(br);
                }
                // draw focus rect if focused
                if (dis->itemState & ODS_FOCUS) {
                    DrawFocusRect(dis->hDC, &dis->rcItem);
                }
                return TRUE;
            }
            // custom color buttons
            if (ctlId >= IDC_BGCOL_CUSTOM_FIRST && ctlId < IDC_BGCOL_CUSTOM_FIRST + kMaxCustomColors) {
                int idx = ctlId - IDC_BGCOL_CUSTOM_FIRST;
                RECT rc = dis->rcItem;
                bool isSelected = (idx == data->selectedCustomIdx);
                if (isSelected) {
                    // draw selection outline: fill background, then inset for 2px gap
                    FillRect(dis->hDC, &rc, (HBRUSH)(COLOR_HIGHLIGHT + 1));
                    InflateRect(&rc, -3, -3);
                }
                if (data->customColorSet[idx]) {
                    HBRUSH br = CreateSolidBrush(data->customColors[idx]);
                    FillRect(dis->hDC, &rc, br);
                    DeleteObject(br);
                } else {
                    // empty slot: window background with accent border and diagonal X
                    FillRect(dis->hDC, &rc, (HBRUSH)(COLOR_WINDOW + 1));
                    HPEN pen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_BTNSHADOW));
                    HPEN oldPen = (HPEN)SelectObject(dis->hDC, pen);
                    // border
                    MoveToEx(dis->hDC, rc.left, rc.top, nullptr);
                    LineTo(dis->hDC, rc.right - 1, rc.top);
                    LineTo(dis->hDC, rc.right - 1, rc.bottom - 1);
                    LineTo(dis->hDC, rc.left, rc.bottom - 1);
                    LineTo(dis->hDC, rc.left, rc.top);
                    // diagonal lines
                    MoveToEx(dis->hDC, rc.left, rc.top, nullptr);
                    LineTo(dis->hDC, rc.right - 1, rc.bottom - 1);
                    MoveToEx(dis->hDC, rc.right - 1, rc.top, nullptr);
                    LineTo(dis->hDC, rc.left, rc.bottom - 1);
                    SelectObject(dis->hDC, oldPen);
                    DeleteObject(pen);
                }
                return TRUE;
            }
            break;
        }

        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case IDOK:
                    TryParseBgColorEdit(hDlg, data);
                    data->applyToAll = IsDlgButtonChecked(hDlg, IDC_BGCOL_ALL_FILES) == BST_CHECKED;
                    if (data->customColorsChanged) {
                        SaveCustomColors(data);
                    }
                    EndDialog(hDlg, IDOK);
                    return TRUE;
                case IDCANCEL:
                    if (data->customColorsChanged) {
                        SaveCustomColors(data);
                    }
                    EndDialog(hDlg, IDCANCEL);
                    return TRUE;
                case IDC_BGCOL_EDIT:
                    if (HIWORD(wp) == EN_CHANGE) {
                        if (TryParseBgColorEdit(hDlg, data)) {
                            // update selected button color
                            if (data->selectedCustomIdx >= 0 && !data->isCheckered) {
                                data->customColors[data->selectedCustomIdx] = data->currentColor;
                                data->customColorSet[data->selectedCustomIdx] = true;
                                data->customColorsChanged = true;
                            }
                            InvalidatePreview(hDlg, data);
                        }
                    }
                    break;
                case IDC_BGCOL_PREVIEW:
                    SelectPreviewButton(hDlg, data);
                    break;
                default: {
                    int id = LOWORD(wp);
                    // preset buttons: select preview button
                    if (id >= IDC_BGCOL_PRESET_FIRST && id < IDC_BGCOL_PRESET_FIRST + kNumPresets) {
                        int idx = id - IDC_BGCOL_PRESET_FIRST;
                        COLORREF col = kBgPresetColors[idx];
                        if (col == kColorUnset) {
                            data->isCheckered = true;
                        } else {
                            data->isCheckered = false;
                            data->currentColor = col;
                        }
                        SelectPreviewButton(hDlg, data);
                        UpdateBgColorEditFromColor(hDlg, data);
                    }
                    // custom color buttons: select this button
                    if (id >= IDC_BGCOL_CUSTOM_FIRST && id < IDC_BGCOL_CUSTOM_FIRST + kMaxCustomColors) {
                        int idx = id - IDC_BGCOL_CUSTOM_FIRST;
                        if (data->selectedCustomIdx == idx) {
                            // clicking selected button deselects it
                            SelectPreviewButton(hDlg, data);
                        } else {
                            SelectCustomButton(hDlg, data, idx);
                            // load the button's color as current selection
                            if (data->customColorSet[idx]) {
                                data->isCheckered = false;
                                data->currentColor = data->customColors[idx];
                                UpdateBgColorEditFromColor(hDlg, data);
                            }
                        }
                    }
                } break;
            }
            break;

        case WM_CONTEXTMENU: {
            HWND hwndClicked = (HWND)wp;
            int ctlId = GetDlgCtrlID(hwndClicked);
            if (ctlId >= IDC_BGCOL_CUSTOM_FIRST && ctlId < IDC_BGCOL_CUSTOM_FIRST + kMaxCustomColors) {
                int idx = ctlId - IDC_BGCOL_CUSTOM_FIRST;
                if (data->customColorSet[idx]) {
                    data->customColorSet[idx] = false;
                    data->customColorsChanged = true;
                    if (data->selectedCustomIdx == idx) {
                        SelectPreviewButton(hDlg, data);
                    }
                    InvalidateRect(hwndClicked, nullptr, TRUE);
                }
                return TRUE;
            }
            break;
        }
    }
    return FALSE;
}

bool Dialog_ChangeBackgroundColor(HWND hwnd, COLORREF currentColor, bool isCheckered, Str allFilesLabel,
                                  BgColorResult& result) {
    BgColorDlgData data;
    data.currentColor = currentColor;
    data.isCheckered = isCheckered;
    data.applyToAll = false;
    data.selectedCustomIdx = -1;
    data.previewSelected = true;
    data.showRadioButtons = true;
    data.allFilesLabel = allFilesLabel;

    INT_PTR res = CreateDialogBox(IDD_DIALOG_CHANGE_BG_COLOR, hwnd, Dialog_ChangeBgColor_Proc, (LPARAM)&data);
    if (res != IDOK) {
        return false;
    }

    result.color = data.currentColor;
    result.isCheckered = data.isCheckered;
    result.applyToAllFiles = data.applyToAll;
    return true;
}

bool Dialog_SetTabColor(HWND hwnd, COLORREF currentColor, bool isUnset, COLORREF& resultColor, bool& resultIsUnset) {
    BgColorDlgData data;
    data.currentColor = currentColor;
    data.isCheckered = isUnset;
    data.applyToAll = false;
    data.selectedCustomIdx = -1;
    data.previewSelected = true;
    data.title = _TRA("Set Tab Color");
    data.showRadioButtons = false;

    INT_PTR res = CreateDialogBox(IDD_DIALOG_CHANGE_BG_COLOR, hwnd, Dialog_ChangeBgColor_Proc, (LPARAM)&data);
    if (res != IDOK) {
        return false;
    }

    resultColor = data.currentColor;
    resultIsUnset = data.isCheckered;
    return true;
}

bool Dialog_AddFavorite(HWND hwnd, Str pageNo, Str& favName) {
    Dialog_AddFav_Data data;
    data.pageNo = str::Dup(pageNo);
    data.favName = str::Dup(favName);

    INT_PTR res = CreateDialogBox(IDD_DIALOG_FAV_ADD, hwnd, Dialog_AddFav_Proc, (LPARAM)&data);
    if (IDCANCEL == res) {
        return false;
    }

    str::ReplaceWithCopy(&favName, data.favName);
    return true;
}
