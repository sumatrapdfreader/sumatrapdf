/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// orig's file dialogs: GetSaveFileNameW / GetOpenFileNameW with orig's
// OPENFILENAME fields, and the IFileOpenDialog of File / Open.
// ng: gpui has no save prompt, no file-type filter and no multi-select, so on
// Windows these are shown on the frame's HWND (the one gui/NativeWindow.cpp
// subclasses). Other platforms keep the port's own dialogs.

#include "base/Base.h"
#include "base/File.h"
#include "base/UITask.h"
#include "base/Win.h"
#include "base/ScopedWin.h"
#include "base/GuessFileType.h"

#include <shobjidl.h>
#include <commdlg.h>

#include "gui/UIModels.h"
#include "Settings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "Translations.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "gui/AppShell.h"
#include "SumatraDialogs.h"
#include "gui/NativeFileDlg.h"
#include "OpenFileFilters.h"

#include "SumatraLog.h"

// ng: the automation channel switches these off to get the port's own
// dialogs, which it can answer (TestSavePathDialog)
static bool gNativeDlgOff = false;
// the frame whose dialog is up, for the automation channel
static HWND gDlgOwner = nullptr;
static bool gDlgIsSave = false;

bool NativeFileDlgEnabled() {
    return !gNativeDlgOff;
}

// orig builds its filters with \1 in place of \0 so the string functions
// don't cut them short.
// ng: heap strings, the dialog's message loop lets gpui render, which resets
// the temp arena under the OPENFILENAME
static WStr FilterToW(Str filter) {
    if (len(filter) == 0) {
        filter = fmt("%s\1*.*\1", Tr("All files"));
    }
    WStr ws = ToWStr(filter);
    wstr::TransCharsInPlace(ws, WStrL(L"\1"), WStrL(L"\0"));
    return ws;
}

static void FreeSaveArgs(SavePathArgs* args) {
    str::Free(args->title);
    str::Free(args->initialPath);
    str::Free(args->defExt);
    str::Free(args->filter);
    str::Free(args->nativeFile);
    str::Free(args->nativeDir);
    delete args;
}

static void SaveFileDlgNow(SavePathArgs* args) {
    MainWindow* win = args->win;
    HWND hwnd = IsMainWindowValidAndNotClosing(win) ? AppShellNativeHwnd(win) : nullptr;
    if (!hwnd) {
        args->path = {};
        args->onDone.Call(args);
        FreeSaveArgs(args);
        return;
    }

    WCHAR dstFileName[MAX_PATH + 1]{};
    Str name = len(args->nativeFile) > 0 ? args->nativeFile : args->initialPath;
    wstr::BufSet(WStr(dstFileName, MAX_PATH), ToWStrTemp(name));

    // we want to skip '.'
    Str defExt = args->defExt;
    if (len(defExt) > 0 && defExt.s[0] == '.') {
        defExt = Str(defExt.s + 1, defExt.len - 1);
    }

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = dstFileName;
    ofn.nMaxFile = dimof(dstFileName);
    WStr filterW = FilterToW(args->filter);
    WStr titleW = ToWStr(args->title);
    WStr defExtW = ToWStr(defExt);
    WStr dirW = ToWStr(args->nativeDir);
    ofn.lpstrFilter = filterW.s;
    ofn.nFilterIndex = (DWORD)args->filterIndex;
    if (args->nativeTitle) {
        ofn.lpstrTitle = titleW.s;
    }
    if (!args->noDefExt && len(defExt) > 0) {
        ofn.lpstrDefExt = defExtW.s;
    }
    if (len(args->nativeDir) > 0) {
        ofn.lpstrInitialDir = dirW.s;
    }
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

    gDlgOwner = hwnd;
    gDlgIsSave = true;
    bool ok = GetSaveFileNameW(&ofn);
    gDlgOwner = nullptr;
    wstr::Free(filterW);
    wstr::Free(titleW);
    wstr::Free(defExtW);
    wstr::Free(dirW);
    if (!ok) {
        // FALSE both on cancellation (extended error == 0) and on a failure
        // such as a path that is too long
        DWORD cdErr = CommDlgExtendedError();
        if (cdErr != 0) {
            logf("GetSaveFileNameW() failed, CommDlgExtendedError() = 0x%x\n", (uint)cdErr);
        }
    }
    // the dialog ran a message loop: the temp strings above are gone
    TempStr path = ok ? ToUtf8Temp(WStr(dstFileName)) : TempStr{};
    logf("NativeSaveFileDlg: ok %d, filter %d, '%s'\n", ok ? 1 : 0, (int)ofn.nFilterIndex, path);
    args->path = path;
    args->filterIndex = ok ? (int)ofn.nFilterIndex : 0;
    args->onDone.Call(args);
    FreeSaveArgs(args);
    if (IsMainWindowValid(win)) {
        AppShellInvalidate(win);
    }
}

// takes ownership of args. ng: orig calls GetSaveFileNameW where the command
// runs; here that is inside gpui's dispatch, so the dialog is shown from the
// ui task queue once gpui has unwound
void NativeSaveFileDlg(SavePathArgs* args) {
    uitask::Post(MkFunc0(SaveFileDlgNow, args), "NativeSaveFileDlg");
}

// filter is orig's lpstrFilter with \1 for \0; initialPath seeds the name.
// With multiSelect the buffer is the folder, then the names, then an empty string
bool NativeOpenFileDlg(MainWindow* win, Str filter, Str initialPath, bool multiSelect, StrVec* pathsOut) {
    HWND hwnd = AppShellNativeHwnd(win);
    if (!hwnd) {
        return false;
    }
    constexpr int kBufSize = 64 * 1024;
    Vec<WCHAR> buf;
    VecAppendBlanks(buf, kBufSize);
    if (len(initialPath) > 0 && len(initialPath) < MAX_PATH) {
        wstr::BufSet(WStr(buf.els, MAX_PATH), ToWStrTemp(initialPath));
    }

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = buf.els;
    ofn.nMaxFile = kBufSize;
    WStr filterW = FilterToW(filter);
    ofn.lpstrFilter = filterW.s;
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    if (multiSelect) {
        ofn.Flags |= OFN_ALLOWMULTISELECT | OFN_EXPLORER;
    }
    gDlgOwner = hwnd;
    gDlgIsSave = false;
    bool ok = GetOpenFileNameW(&ofn);
    gDlgOwner = nullptr;
    wstr::Free(filterW);
    if (!ok) {
        return false;
    }
    WCHAR* first = buf.els;
    if (!multiSelect) {
        pathsOut->Append(ToUtf8Temp(first));
        return true;
    }
    WCHAR* name = first + wcslen(first) + 1;
    if (*name == 0) {
        // one file: the buffer is its full path
        pathsOut->Append(ToUtf8Temp(first));
    }
    for (; *name; name += wcslen(name) + 1) {
        pathsOut->Append(path::JoinTemp(ToUtf8Temp(first), ToUtf8Temp(name)));
    }
    return len(*pathsOut) > 0;
}

// Standard Windows IFileOpenDialog multi-select open.
bool NativeOpenDocsDlg(MainWindow* win, StrVec* pathsOut) {
    HWND hwnd = AppShellNativeHwnd(win);
    if (!hwnd) {
        return false;
    }
    ScopedComPtr<IFileOpenDialog> dlg;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg));
    if (FAILED(hr) || !dlg) {
        logf("NativeOpenDocsDlg: CoCreateInstance(CLSID_FileOpenDialog) failed: 0x%x\n", (uint)hr);
        return false;
    }

    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST | FOS_ALLOWMULTISELECT);

    OpenFileFilterList filters;
    BuildOpenFileFilters(filters);
    dlg->SetFileTypes((UINT)len(filters.specs), VecData(filters.specs));
    dlg->SetFileTypeIndex(1); // "All supported documents" (1-based)

    gDlgOwner = hwnd;
    gDlgIsSave = false;
    hr = dlg->Show(hwnd);
    gDlgOwner = nullptr;
    if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        return false;
    }
    if (FAILED(hr)) {
        logf("NativeOpenDocsDlg: IFileOpenDialog::Show failed: 0x%x\n", (uint)hr);
        return false;
    }

    ScopedComPtr<IShellItemArray> results;
    hr = dlg->GetResults(&results);
    if (FAILED(hr) || !results) {
        return false;
    }

    DWORD count = 0;
    hr = results->GetCount(&count);
    if (FAILED(hr) || count == 0) {
        return false;
    }

    for (DWORD i = 0; i < count; i++) {
        ScopedComPtr<IShellItem> item;
        hr = results->GetItemAt(i, &item);
        if (FAILED(hr) || !item) {
            continue;
        }
        PWSTR pathW = nullptr;
        hr = item->GetDisplayName(SIGDN_FILESYSPATH, &pathW);
        if (FAILED(hr) || !pathW) {
            continue;
        }
        TempStr path = ToUtf8Temp(WStr(pathW));
        CoTaskMemFree(pathW);
        if (len(path) == 0) {
            continue;
        }
        pathsOut->Append(path);
    }
    return len(*pathsOut) > 0;
}

// the dialog a frame owns while one of the calls above is in its message loop
static HWND FindOpenDialog() {
    if (!gDlgOwner) {
        return nullptr;
    }
    HWND popup = GetWindow(gDlgOwner, GW_ENABLEDPOPUP);
    if (!popup || popup == gDlgOwner) {
        return nullptr;
    }
    return popup;
}

static BOOL CALLBACK FindNameEdit(HWND hwnd, LPARAM lp) {
    WCHAR cls[64]{};
    GetClassNameW(hwnd, cls, dimof(cls));
    if (!wstr::Eq(WStr(cls), WStrL(L"Edit"))) {
        return TRUE;
    }
    // the address bar has an edit too; the file name is the other one
    for (HWND p = GetParent(hwnd); p; p = GetParent(p)) {
        GetClassNameW(p, cls, dimof(cls));
        if (wstr::Eq(WStr(cls), WStrL(L"Address Band Root"))) {
            return TRUE;
        }
    }
    *(HWND*)lp = hwnd;
    return FALSE;
}

// ng: for the automation channel, which cannot type into a system dialog.
//   off | on        the port's own dialogs / Windows' (the default)
//   state           whether a dialog is up, its window and kind
//   cancel          Cancel in the dialog that is up
//   accept <name>   put <name> in its file-name field and press the default button
TempStr NativeFileDlgTestTemp(MainWindow*, Str what, Str arg) {
    if (str::Eq(what, StrL("off")) || str::Eq(what, StrL("on"))) {
        gNativeDlgOff = str::Eq(what, StrL("off"));
        return fmt("OK native=%d", gNativeDlgOff ? 0 : 1);
    }
    HWND dlg = FindOpenDialog();
    if (str::Eq(what, StrL("state"))) {
        return fmt("OK native=%d open=%d save=%d hwnd=%d", gNativeDlgOff ? 0 : 1, dlg ? 1 : 0,
                   dlg && gDlgIsSave ? 1 : 0, (int)(INT_PTR)dlg);
    }
    if (!dlg) {
        return StrL("NOTREADY no-dialog");
    }
    if (str::Eq(what, StrL("cancel"))) {
        PostMessageW(dlg, WM_COMMAND, IDCANCEL, 0);
        return StrL("OK");
    }
    if (str::Eq(what, StrL("accept"))) {
        // a save dialog opens in the folder the user last saved to: a test
        // must say where it writes
        if (gDlgIsSave && !path::IsAbsolute(arg)) {
            return StrL("ERR save-needs-absolute-path");
        }
        HWND edit = nullptr;
        EnumChildWindows(dlg, FindNameEdit, (LPARAM)&edit);
        if (!edit) {
            return StrL("ERR no-name-edit");
        }
        SendMessageW(edit, WM_SETTEXT, 0, (LPARAM)CWStrTemp(arg));
        // the dialog takes the name from the combo box's notification
        HWND combo = GetParent(edit);
        SendMessageW(GetParent(combo), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(combo), CBN_EDITCHANGE), (LPARAM)combo);
        // never press the button on a name that is not the one asked for
        WCHAR got[1024]{};
        SendMessageW(edit, WM_GETTEXT, dimof(got), (LPARAM)got);
        if (!str::Eq(ToUtf8Temp(WStr(got)), arg)) {
            return StrL("ERR name-not-set");
        }
        HWND btn = GetDlgItem(dlg, IDOK);
        if (!btn) {
            return StrL("ERR no-default-button");
        }
        PostMessageW(btn, BM_CLICK, 0, 0);
        return StrL("OK");
    }
    return StrL("ERR what");
}
