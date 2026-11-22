/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// orig's file dialogs: GetSaveFileNameW / GetOpenFileNameW with orig's
// OPENFILENAME fields, and the IFileOpenDialog of File / Open.
// ng: shown with gpui's open and save prompts. Save As uses the native one on
// Windows only; other platforms keep the port's own save dialog.

#include "gui/GpuiBridge.h"
#include "base/File.h"
#include "base/UITask.h"
#include "base/GuessFileType.h"
#if OS_WIN
#include "base/Win.h"
#endif

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
#if OS_WIN
#include "OpenFileFilters.h"
#endif

#include "SumatraLog.h"

bool NativeFileDlgEnabled() {
    return OS_WIN;
}

// orig's lpstrFilter: name and patterns pairs with \1 in place of \0, so the
// string functions don't cut them short ("PDF\1*.pdf\1All files\1*.*\1").
// The filters are views of `filter`, read before the prompt's loop resets
// the temp arena; an empty one is "All files"
static void FilterToGpui(Str filter, Vec<gp::PathFilter>* out) {
    Str parts[2];
    int nParts = 0;
    int start = 0;
    for (int i = 0; i <= len(filter); i++) {
        if (i < len(filter) && filter.s[i] != '\1') {
            continue;
        }
        if (i == len(filter) && i == start) {
            break;
        }
        parts[nParts++] = Str(filter.s + start, i - start);
        start = i + 1;
        if (nParts < 2) {
            continue;
        }
        gp::PathFilter f;
        f.name = ToGpui(parts[0]);
        f.patterns = ToGpui(parts[1]);
        VecAppend(*out, f);
        nParts = 0;
    }
    if (len(*out) == 0) {
        gp::PathFilter all;
        all.name = ToGpui(Tr("All files"));
        all.patterns = GStrL("*.*");
        VecAppend(*out, all);
    }
}

// orig's lpstrFile: a name, or a path whose folder wins over lpstrInitialDir
static void SplitDirName(Str path, Str defDir, Str* dirOut, Str* nameOut) {
    int i = len(path);
    while (i > 0 && !path::IsSep(path.s[i - 1])) {
        i--;
    }
    *dirOut = i > 0 ? Str(path.s, i) : defDir;
    *nameOut = Str(path.s + i, len(path) - i);
}

// a path tests set through the control channel, taken instead of showing the
// dialog
static TempStr TakeTestPathTemp() {
#if OS_WIN
    WCHAR buf[MAX_PATH + 1]{};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = buf;
    ofn.nMaxFile = dimof(buf);
    if (TakeTestSaveFileName(&ofn)) {
        return ToUtf8Temp(WStr(buf));
    }
#endif
    return {};
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
    if (!IsMainWindowValidAndNotClosing(win) || !win->gpuiWin) {
        args->path = {};
        args->onDone.Call(args);
        FreeSaveArgs(args);
        return;
    }

    TempStr path = TakeTestPathTemp();
    int filterIndex = args->filterIndex;
    if (len(path) == 0) {
        Str dir, name;
        SplitDirName(len(args->nativeFile) > 0 ? args->nativeFile : args->initialPath, args->nativeDir, &dir, &name);
        Vec<gp::PathFilter> filters;
        FilterToGpui(args->filter, &filters);
        int chosen = -1;

        gp::NewPathPrompt prompt;
        prompt.directory = ToGpui(dir);
        prompt.suggestedName = ToGpui(name);
        if (args->nativeTitle) {
            prompt.title = ToGpui(args->title);
        }
        prompt.filters.items = filters.els;
        prompt.filters.count = len(filters);
        prompt.filters.index = filterIndex > 0 ? filterIndex - 1 : 0;
        prompt.filters.chosen = &chosen;
        if (!args->noDefExt) {
            prompt.defaultExt = ToGpui(args->defExt);
        }
        gp::TempStr picked = gp::PromptForNewPathTemp(win->gpuiWin, prompt);
        // the dialog ran a message loop: temp strings from before it are gone
        path = str::DupTemp(FromGpui(picked));
        filterIndex = chosen + 1;
    }
    logf("NativeSaveFileDlg: filter %d, '%s'\n", filterIndex, path);
    args->path = path;
    args->filterIndex = len(path) > 0 ? filterIndex : 0;
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

static bool OpenFileDlg(MainWindow* win, Str title, const Vec<gp::PathFilter>& filters, Str initialPath,
                        bool multiSelect, StrVec* pathsOut) {
    if (!win || !win->gpuiWin) {
        return false;
    }
    // a test's paths instead of the dialog (the first, for one file)
    StrVec testPaths;
    if (TakeTestOpenFileNames(testPaths)) {
        for (int i = 0; i < (multiSelect ? len(testPaths) : 1); i++) {
            pathsOut->Append(testPaths[i]);
        }
        return true;
    }
    Str dir, name;
    SplitDirName(initialPath, {}, &dir, &name);
    gp::PathPrompt prompt;
    prompt.files = true;
    prompt.multiple = multiSelect;
    prompt.title = ToGpui(title);
    prompt.filters.items = filters.els;
    prompt.filters.count = len(filters);
    prompt.directory = ToGpui(dir);
    prompt.fileName = ToGpui(name);
    gp::TempStr picked = gp::PromptForPathTemp(win->gpuiWin, prompt);

    // several, with multiSelect, a path a line
    StrVec paths;
    Split(&paths, FromGpui(picked), StrL("\n"), true);
    for (Str path : paths) {
        pathsOut->Append(path);
    }
    return len(*pathsOut) > 0;
}

// filter is orig's lpstrFilter with \1 for \0; initialPath seeds the name.
// An empty title is the system's
bool NativeOpenFileDlg(MainWindow* win, Str title, Str filter, Str initialPath, bool multiSelect, StrVec* pathsOut) {
    Vec<gp::PathFilter> filters;
    FilterToGpui(filter, &filters);
    return OpenFileDlg(win, title, filters, initialPath, multiSelect, pathsOut);
}

#if OS_WIN
// orig's File / Open: multi-select with the document types
bool NativeOpenDocsDlg(MainWindow* win, StrVec* pathsOut) {
    OpenFileFilterList list;
    BuildOpenFileFilters(list);
    Vec<gp::PathFilter> filters;
    for (int i = 0; i < len(list.specs); i++) {
        gp::PathFilter f;
        f.name = ToGpui(ToUtf8Temp(list.names[i]));
        f.patterns = ToGpui(ToUtf8Temp(list.patterns[i]));
        VecAppend(filters, f);
    }
    // "All supported documents" is the first
    return OpenFileDlg(win, {}, filters, {}, true, pathsOut);
}
#endif
