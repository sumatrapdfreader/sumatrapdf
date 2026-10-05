/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: gpui has no save-file prompt (PathPrompt opens only), so this is the
// port's stand-in for win32's GetSaveFileNameW: a dialog with the directory it
// is in, the sub-directories and matching files in it, and a file-name field.
// Every save-as in the port goes through ShowSavePathDialog(), which on
// Windows shows orig's dialog instead (gui/NativeFileDlg_win.cpp).

#include "gui/GpuiBridge.h"

#include "base/File.h"
#include "base/DirScan.h"
#include "gui/Dpi.h"
#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "MainWindow.h"
#include "Theme.h"
#include "Translations.h"
#include "SumatraPDF.h"
#include "AppTools.h"
#include "gui/AppShell.h"
#include "gui/DialogWidgets.h"
#include "gui/WasmBridge.h"
#include "SumatraDialogs.h"
#include "gui/NativeFileDlg.h"

#include "SumatraLog.h"

constexpr int kMaxRows = 12;

struct SavePathEntry {
    Str name; // owned
    bool isDir = false;
};

struct SavePathDlg {
    SavePathArgs* args = nullptr;
    bool visible = false;
    bool wantFocus = false;
    Str dir; // owned, no trailing separator
    Vec<SavePathEntry> entries;
    int top = 0;
    gpui::InputState* nameEdit = nullptr;
    // a name that already exists: OK asks before it overwrites
    bool confirmingOverwrite = false;
    Str pendingPath; // owned
};

static SavePathDlg gSave;

struct SavePathView {
    static void OnOk(SavePathView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnCancel(SavePathView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnRow(SavePathView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t idx);
    static void OnUp(SavePathView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnPage(SavePathView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t dir);
    static void OnName(SavePathView* self, gp::Ctx* cx, const gp::InputEvent* ev);
};

static gp::Entity<SavePathView> gSavePathView;

static void FreeEntries() {
    for (SavePathEntry& e : gSave.entries) {
        str::Free(e.name);
    }
    VecReset(gSave.entries);
}

static int CmpEntries(const void* a, const void* b) {
    const auto* ea = (const SavePathEntry*)a;
    const auto* eb = (const SavePathEntry*)b;
    if (ea->isDir != eb->isDir) {
        return ea->isDir ? -1 : 1;
    }
    return str::CmpNatural(ea->name, eb->name);
}

// "Image files\1*.png;*.jpg\1All files\1*.*\1": does name match the first entry's patterns
static bool MatchesOpenFilter(Str name, Str filter) {
    StrVec parts;
    Split(&parts, filter, StrL("\1"), false);
    if (len(parts) < 2) {
        return true;
    }
    StrVec pats;
    Split(&pats, parts[1], StrL(";"), true);
    for (Str pat : pats) {
        // "*.png"
        if (len(pat) < 2 || str::Eq(pat, StrL("*.*"))) {
            return true;
        }
        if (str::EndsWithI(name, Str(pat.s + 1, len(pat) - 1))) {
            return true;
        }
    }
    return false;
}

// directories first, then the files whose extension matches the filter
static void ReadDir(Str dir) {
    FreeEntries();
    gSave.top = 0;
    Str ext = gSave.args ? gSave.args->defExt : Str{};
    bool open = gSave.args && gSave.args->openExisting;
    if (open) {
        ext = {};
    }
    DirIter di{dir};
    di.includeFiles = true;
    di.includeDirs = true;
    for (DirIterEntry* e : di) {
        if (str::Eq(e->name, StrL(".")) || str::Eq(e->name, StrL(".."))) {
            continue;
        }
        if (!e->isDir && len(ext) > 0 && !str::EndsWithI(e->name, ext)) {
            continue;
        }
        if (!e->isDir && open && !MatchesOpenFilter(e->name, gSave.args->filter)) {
            continue;
        }
        SavePathEntry se;
        se.name = str::Dup(e->name);
        se.isDir = e->isDir;
        VecAppend(gSave.entries, se);
    }
    if (len(gSave.entries) > 1) {
        qsort(gSave.entries.els, (size_t)len(gSave.entries), sizeof(SavePathEntry), CmpEntries);
    }
}

bool IsSavePathDialogVisible() {
    return gSave.visible;
}

static void FinishSavePathDialog(Str path) {
    SavePathArgs* args = gSave.args;
    gSave.args = nullptr;
    gSave.visible = false;
    gSave.confirmingOverwrite = false;
    MainWindow* win = args ? args->win : nullptr;
    if (win && win->gpuiWin && gSave.nameEdit) {
        gp::InputBlur(gSave.nameEdit, win->gpuiWin->app, win->gpuiWin);
    }
    delete gSave.nameEdit;
    gSave.nameEdit = nullptr;
    FreeEntries();
    str::Free(gSave.dir);
    gSave.dir = {};
    str::Free(gSave.pendingPath);
    gSave.pendingPath = {};
    AppShellInvalidate(win);
    if (args) {
        args->path = path;
        args->onDone.Call(args);
#if OS_WASM
        // a file in MEMFS is not a file the user has; hand it to the browser
        if (len(path) > 0) {
            WasmDownloadFile(path);
        }
#endif
        str::Free(args->title);
        str::Free(args->initialPath);
        str::Free(args->defExt);
        str::Free(args->filter);
        str::Free(args->nativeFile);
        str::Free(args->nativeDir);
        delete args;
    }
}

void CloseSavePathDialog() {
    if (gSave.visible) {
        FinishSavePathDialog({});
    }
}

// ng: the automation channel's stand-in for typing a name and pressing Save.
// An empty path is Cancel. Returns false when no dialog is up.
bool TestFinishSavePathDialog(Str path) {
    if (!gSave.visible) {
        return false;
    }
    FinishSavePathDialog(str::DupTemp(path));
    return true;
}

void ShowSavePathDialog(SavePathArgs* args) {
    if (!args) {
        return;
    }
    if (gSave.visible) {
        CloseSavePathDialog();
    }
    MainWindow* win = args->win;
    if (!win || !win->gpuiWin) {
        args->onDone.Call(args);
        delete args;
        return;
    }
#if OS_WIN
    if (NativeFileDlgEnabled() && !args->openExisting) {
        NativeSaveFileDlg(args);
        return;
    }
#endif
    // the port's dialog does not report a filter
    args->filterIndex = 0;
    gSave.args = args;
    TempStr dir = path::GetDirTemp(args->initialPath);
    if (len(dir) == 0 || !dir::Exists(dir)) {
        dir = GetAppDataDirTemp();
    }
    str::ReplaceWithCopy(&gSave.dir, dir);
    ReadDir(gSave.dir);
    gSave.nameEdit = new gp::InputState();
    gSave.nameEdit->focus = gp::FocusHandleNew(win->gpuiWin->app);
    gp::InputSetValue(gSave.nameEdit, ToGpui(path::GetBaseNameTemp(args->initialPath)));
    gSave.visible = true;
    gSave.wantFocus = true;
    logf("ShowSavePathDialog: dir '%s', name '%s'\n", gSave.dir, path::GetBaseNameTemp(args->initialPath));
    AppShellInvalidate(win);
}

// join the directory and the name, adding the default extension if it is missing
static TempStr ChosenPathTemp() {
    TempStr name = str::DupTemp(FromGpui(gp::InputValue(gSave.nameEdit)));
    str::TrimWSInPlace(name, str::TrimOpt::Both);
    if (len(name) == 0) {
        return {};
    }
    Str ext = gSave.args ? gSave.args->defExt : Str{};
    if (gSave.args && gSave.args->openExisting) {
        ext = {};
    }
    if (len(ext) > 0 && !str::EndsWithI(name, ext)) {
        name = str::JoinTemp(name, ext);
    }
    if (path::IsAbsolute(name)) {
        return name;
    }
    return path::JoinTemp(gSave.dir, name);
}

static void TryAccept() {
    TempStr path = ChosenPathTemp();
    if (len(path) == 0) {
        return;
    }
    if (gSave.args && gSave.args->openExisting) {
        // a name that is not there keeps the dialog up
        if (file::Exists(path)) {
            FinishSavePathDialog(path);
        }
        return;
    }
    if (file::Exists(path) && !gSave.confirmingOverwrite) {
        gSave.confirmingOverwrite = true;
        str::ReplaceWithCopy(&gSave.pendingPath, path);
        AppShellInvalidate(gSave.args ? gSave.args->win : nullptr);
        return;
    }
    FinishSavePathDialog(path);
}

void SavePathView::OnOk(SavePathView*, gp::Ctx* cx, const gp::ClickEvent*) {
    TryAccept();
    gp::Notify(cx);
}

void SavePathView::OnCancel(SavePathView*, gp::Ctx* cx, const gp::ClickEvent*) {
    if (gSave.confirmingOverwrite) {
        gSave.confirmingOverwrite = false;
        gp::Notify(cx);
        return;
    }
    CloseSavePathDialog();
    gp::Notify(cx);
}

void SavePathView::OnRow(SavePathView*, gp::Ctx* cx, const gp::ClickEvent*, int64_t idx) {
    if (!VecIsValidIndex(gSave.entries, (int)idx)) {
        return;
    }
    SavePathEntry& e = gSave.entries[(int)idx];
    if (e.isDir) {
        TempStr next = path::JoinTemp(gSave.dir, e.name);
        str::ReplaceWithCopy(&gSave.dir, next);
        ReadDir(gSave.dir);
    } else {
        gp::InputSetValue(gSave.nameEdit, ToGpui(e.name));
    }
    gp::Notify(cx);
}

void SavePathView::OnUp(SavePathView*, gp::Ctx* cx, const gp::ClickEvent*) {
    TempStr up = path::GetDirTemp(gSave.dir);
    if (len(up) > 0 && !str::Eq(up, gSave.dir)) {
        str::ReplaceWithCopy(&gSave.dir, up);
        ReadDir(gSave.dir);
    }
    gp::Notify(cx);
}

void SavePathView::OnPage(SavePathView*, gp::Ctx* cx, const gp::ClickEvent*, int64_t dir) {
    int n = len(gSave.entries);
    gSave.top = limitValue(gSave.top + (int)dir * kMaxRows, 0, std::max(n - kMaxRows, 0));
    gp::Notify(cx);
}

void SavePathView::OnName(SavePathView*, gp::Ctx* cx, const gp::InputEvent* ev) {
    if (ev->kind != gp::InputEventKind::PressEnter) {
        return;
    }
    TryAccept();
    gp::Notify(cx);
}

gp::El* SavePathDialogBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gSave.visible || !gSave.args || gSave.args->win != win) {
        return nullptr;
    }
    if (!gSavePathView.IsValid()) {
        gSavePathView = gp::EntityNewState<SavePathView>(cx->app);
    }
    const gp::Theme& th = gp::ThemeNow(cx->app);
    gSave.nameEdit->onChange = gp::ListenTo(gSavePathView, &SavePathView::OnName);

    gp::El* body = gp::Div(cx->a)->FlexCol()->Gap(6)->W(gp::kFill);
    gp::El* dirRow = gp::Div(cx->a)->FlexRow()->ItemsCenter()->Gap(6)->W(gp::kFill);
    dirRow->Child(gpc::Button::New(cx, GStrL("save-up"))
                      ->Label(GStrL(".."))
                      ->WithSize(gp::UiSize::Small)
                      ->OnClick(gp::ListenTo(gSavePathView, &SavePathView::OnUp))
                      ->IntoEl());
    dirRow->Child(gp::TextEl(cx->a, GpuiDup(cx->a, gSave.dir))->Font(12)->Fg(th.mutedFg)->Flex1()->Truncate());
    body->Child(dirRow);

    int n = len(gSave.entries);
    gp::El* list = gp::Div(cx->a)
                       ->FlexCol()
                       ->W(gp::kFill)
                       ->H((float)(kMaxRows * DpiScale(20)))
                       ->Bg(th.tokens.background)
                       ->Border(1, th.border)
                       ->ClipY();
    for (int i = gSave.top; i < n && i < gSave.top + kMaxRows; i++) {
        SavePathEntry& e = gSave.entries[i];
        TempStr id = fmt("save-row-%d", i);
        TempStr label = e.isDir ? fmt("[%s]", e.name) : str::DupTemp(e.name);
        list->Child(gp::Div(cx->a)
                        ->FlexRow()
                        ->ItemsCenter()
                        ->W(gp::kFill)
                        ->H((float)DpiScale(20))
                        ->PadX(6)
                        ->Cursor(gp::CursorKind::Pointer)
                        ->HoverBg(th.tokens.accent)
                        ->PathClick(GpuiDup(cx->a, id))
                        ->OnClick(gp::ListenTo(gSavePathView, &SavePathView::OnRow, (intptr_t)i))
                        ->Child(gp::TextEl(cx->a, GpuiDup(cx->a, label))
                                    ->Font(13)
                                    ->Fg(e.isDir ? th.mutedFg : th.foreground)
                                    ->Truncate()));
    }
    body->Child(list);
    if (n > kMaxRows) {
        gp::El* pager = gp::Div(cx->a)->FlexRow()->Gap(6)->ItemsCenter()->W(gp::kFill);
        pager->Child(gpc::Button::New(cx, GStrL("save-prev"))
                         ->Label(GStrL("<"))
                         ->WithSize(gp::UiSize::Small)
                         ->OnClick(gp::ListenTo(gSavePathView, &SavePathView::OnPage, (intptr_t)-1))
                         ->IntoEl());
        pager->Child(gp::TextEl(cx->a, GpuiDup(cx->a, fmt("%d / %d", gSave.top + 1, n)))->Font(12)->Fg(th.mutedFg));
        pager->Child(gpc::Button::New(cx, GStrL("save-next"))
                         ->Label(GStrL(">"))
                         ->WithSize(gp::UiSize::Small)
                         ->OnClick(gp::ListenTo(gSavePathView, &SavePathView::OnPage, (intptr_t)1))
                         ->IntoEl());
        body->Child(pager);
    }
    body->Child(
        gpc::Input::New(cx, GStrL("save-name"), gSave.nameEdit)->WithSize(gp::UiSize::Small)->W(gp::kFill)->IntoEl());
    if (gSave.confirmingOverwrite) {
        TempStr msg = fmt(Tr("File %s already exists. Overwrite?").s, path::GetBaseNameTemp(gSave.pendingPath));
        body->Child(gp::TextEl(cx->a, GpuiDup(cx->a, msg))->Font(13)->Fg(th.danger)->Wrap());
    }
    if (gSave.wantFocus) {
        gSave.wantFocus = false;
        gp::InputFocus(gSave.nameEdit, cx->app, cx->win);
        gp::InputSelectAll(gSave.nameEdit, cx->app, cx->win);
    }

    DlgSetDefault(cx, gp::ListenTo(gSavePathView, &SavePathView::OnOk));
    return gpc::Dialog::New(cx)
        ->Open(true)
        ->Title(GpuiDup(cx->a, gSave.args->title))
        ->Body(body)
        ->W(460)
        ->OkText(ToGpui(gSave.confirmingOverwrite ? Tr("Yes") : (gSave.args->openExisting ? Tr("Open") : Tr("Save"))))
        ->CancelText(ToGpui(Tr("Cancel")))
        ->ShowCancel(true)
        ->OnOk(gp::ListenTo(gSavePathView, &SavePathView::OnOk))
        ->OnCancel(gp::ListenTo(gSavePathView, &SavePathView::OnCancel))
        ->OnClose(gp::ListenTo(gSavePathView, &SavePathView::OnCancel))
        ->IntoEl(gp::WindowSize(cx->win));
}
