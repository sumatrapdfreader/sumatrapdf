/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's AddFavoriteDialog.cpp is a WS_POPUPWINDOW that adds a favorite
// (Ctrl+B) or, without the new favorite's name, edits the existing ones
// (CmdEditFavorites). Here it is the same column of gpui elements, in a window
// of its own where the platform has one, else in a card over the frame.

#include "gui/GpuiBridge.h"

#include "gui/Dpi.h"
#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "EngineBase.h"
#include "DocController.h"
#include "MainWindow.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "gui/AppShell.h"
#include "gui/DialogWidgets.h"
#include "gui/ToolWindow.h"
#include "shared/PdfToolsCommon.h"
#include "Favorites.h"
#include "SumatraDialogs.h"
#include "AppHelpersCommon.h"

enum class FavDlgMode {
    Add,
    Edit,
};

struct AddFavoriteDlg {
    FavDlgMode mode = FavDlgMode::Add;
    MainWindow* win = nullptr;
    bool visible = false;
    bool wantFocus = false;
    int pageNo = 0;
    Str pageLabel; // owned
    Str filePath;  // owned
    gpui::InputState* editName = nullptr;
    gpui::InputState* editRename = nullptr;
    Vec<FavEditRow> rows;
    int sel = -1;
    float scrollY = 0;
};

static AddFavoriteDlg gAddFav;

constexpr float kAddFavDlgDx = 440;
constexpr int kFavListLines = 10;

struct AddFavoriteView {
    static void OnOk(AddFavoriteView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnCancel(AddFavoriteView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnInput(AddFavoriteView* self, gp::Ctx* cx, const gp::InputEvent* ev);
    static void OnRenameInput(AddFavoriteView* self, gp::Ctx* cx, const gp::InputEvent* ev);
    static void OnRowClick(AddFavoriteView* self, gp::Ctx* cx, const gp::ClickEvent* ev, int64_t idx);
    static void OnScroll(AddFavoriteView* self, gp::Ctx* cx, const gp::ScrollEvent* ev);
    static void OnRename(AddFavoriteView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnDelete(AddFavoriteView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnMoveUp(AddFavoriteView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnMoveDown(AddFavoriteView* self, gp::Ctx* cx, const gp::ClickEvent*);
};

static gp::Entity<AddFavoriteView> gAddFavView;

// orig's modal window, where the platform can have one (DlgWindowOpen); null: a
// dialog in the frame
static ToolWindow* gAddFavoriteTw = nullptr;

// the dialog in the frame; a window of its own is not the frame's business
bool IsAddFavoriteDialogVisible() {
    return gAddFav.visible && !gAddFavoriteTw;
}

static gp::Window* AddFavoriteHost() {
    if (gAddFavoriteTw) {
        return ToolWindowGpui(gAddFavoriteTw);
    }
    return gAddFav.win ? gAddFav.win->gpuiWin : nullptr;
}

static void FreeInput(gp::Window* host, gp::InputState** input) {
    if (host && *input) {
        gp::InputBlur(*input, host->app, host);
    }
    delete *input;
    *input = nullptr;
}

void CloseAddFavoriteDialog() {
    if (!gAddFav.visible) {
        return;
    }
    gAddFav.visible = false;
    MainWindow* win = gAddFav.win;
    // Blur the window that holds the fields before freeing them. The tool
    // window's close otherwise blurs a freed input.
    gp::Window* host = AddFavoriteHost();
    FreeInput(host, &gAddFav.editName);
    FreeInput(host, &gAddFav.editRename);
    DlgWindowClose(&gAddFavoriteTw);
    str::Free(gAddFav.pageLabel);
    gAddFav.pageLabel = {};
    str::Free(gAddFav.filePath);
    gAddFav.filePath = {};
    VecReset(gAddFav.rows);
    AppShellInvalidate(win);
}

static FavEditRow* SelectedRow() {
    if (gAddFav.sel < 0 || gAddFav.sel >= len(gAddFav.rows)) {
        return nullptr;
    }
    return &gAddFav.rows[gAddFav.sel];
}

// select row `sel`; the name field shows its favorite's name
static void SelectRow(int sel) {
    gAddFav.sel = sel;
    FavEditRow* row = SelectedRow();
    Favorite* fav = row ? row->fav : nullptr;
    gp::InputSetValue(gAddFav.editRename, ToGpui(fav ? fav->name : Str{}));
    float viewDy = kFavListLines * kDlgListRowDy;
    if (sel >= 0) {
        gAddFav.scrollY = DialogScrollToRow(gAddFav.scrollY, sel, len(gAddFav.rows), kDlgListRowDy, viewDy);
    }
    AppShellInvalidate(gAddFav.win);
}

// rebuild the rows after a change, keeping `toSelect` selected
static void ReloadRows(Favorite* toSelect) {
    BuildFavEditRows(gAddFav.rows);
    int sel = -1;
    for (int i = 0; toSelect && i < len(gAddFav.rows); i++) {
        if (gAddFav.rows[i].fav == toSelect) {
            sel = i;
        }
    }
    SelectRow(sel);
}

// orig's AddFavoriteWnd::OnOk
static void AddFavoriteOk() {
    if (gAddFav.mode == FavDlgMode::Add) {
        TempStr name;
        if (gAddFav.editName) {
            name = str::DupTemp(FromGpui(gp::InputValue(gAddFav.editName)));
        }
        str::TrimWSInPlace(name, str::TrimOpt::Both);
        if (len(name) == 0) {
            name = {};
        }
        ApplyAddFavorite(gAddFav.win, gAddFav.filePath, gAddFav.pageNo, gAddFav.pageLabel, name);
    }
    CloseAddFavoriteDialog();
}

static void RenameSelected() {
    FavEditRow* row = SelectedRow();
    if (!row || !row->fav) {
        return;
    }
    Favorite* fav = row->fav;
    RenameFavorite(row->fs, fav, FromGpui(gp::InputValue(gAddFav.editRename)));
    ReloadRows(fav);
}

static void MoveSelected(FavMove dir) {
    FavEditRow* row = SelectedRow();
    if (!row) {
        return;
    }
    Favorite* fav = row->fav;
    MoveFavorite(row->fs, fav, dir);
    ReloadRows(fav);
}

// orig's AddFavoriteWnd::OnDelete: the row that takes its place is selected,
// unless that is another file's name
static void DeleteSelected() {
    FavEditRow* row = SelectedRow();
    if (!row || !row->fav) {
        return;
    }
    int sel = gAddFav.sel;
    DelFavorite(row->fs, row->fav);
    BuildFavEditRows(gAddFav.rows);
    int n = len(gAddFav.rows);
    if (sel >= n || (sel >= 0 && !gAddFav.rows[sel].fav)) {
        sel--;
    }
    if (sel >= 0 && sel < n && !gAddFav.rows[sel].fav) {
        sel = -1;
    }
    SelectRow(sel);
}

void AddFavoriteView::OnOk(AddFavoriteView*, gp::Ctx* cx, const gp::ClickEvent*) {
    AddFavoriteOk();
    gp::Notify(cx);
}

void AddFavoriteView::OnCancel(AddFavoriteView*, gp::Ctx* cx, const gp::ClickEvent*) {
    CloseAddFavoriteDialog();
    gp::Notify(cx);
}

void AddFavoriteView::OnInput(AddFavoriteView*, gp::Ctx* cx, const gp::InputEvent* ev) {
    if (ev->kind != gp::InputEventKind::PressEnter) {
        return;
    }
    AddFavoriteOk();
    gp::Notify(cx);
}

// Enter in the name field renames instead of pressing the default button
void AddFavoriteView::OnRenameInput(AddFavoriteView*, gp::Ctx* cx, const gp::InputEvent* ev) {
    if (ev->kind != gp::InputEventKind::PressEnter) {
        return;
    }
    RenameSelected();
    gp::Notify(cx);
}

void AddFavoriteView::OnRowClick(AddFavoriteView*, gp::Ctx* cx, const gp::ClickEvent*, int64_t idx) {
    SelectRow((int)idx);
    gp::Notify(cx);
}

void AddFavoriteView::OnScroll(AddFavoriteView*, gp::Ctx* cx, const gp::ScrollEvent* ev) {
    gAddFav.scrollY = ev->offsetY;
    gp::Notify(cx);
    AppShellInvalidate(gAddFav.win);
}

void AddFavoriteView::OnRename(AddFavoriteView*, gp::Ctx* cx, const gp::ClickEvent*) {
    RenameSelected();
    gp::Notify(cx);
}

void AddFavoriteView::OnDelete(AddFavoriteView*, gp::Ctx* cx, const gp::ClickEvent*) {
    DeleteSelected();
    gp::Notify(cx);
}

void AddFavoriteView::OnMoveUp(AddFavoriteView*, gp::Ctx* cx, const gp::ClickEvent*) {
    MoveSelected(FavMove::Up);
    gp::Notify(cx);
}

void AddFavoriteView::OnMoveDown(AddFavoriteView*, gp::Ctx* cx, const gp::ClickEvent*) {
    MoveSelected(FavMove::Down);
    gp::Notify(cx);
}

static Str AddFavoriteDlgTitle() {
    return gAddFav.mode == FavDlgMode::Add ? Tr("Add Favorite") : Tr("Edit Favorites");
}

static gp::InputState* NewInput(MainWindow* win) {
    auto* input = new gp::InputState();
    input->focus = gp::FocusHandleNew(win->gpuiWin ? win->gpuiWin->app : nullptr);
    return input;
}

static void ShowFavoriteDialog(MainWindow* win, FavDlgMode mode, Str filePath, int pageNo, Str pageLabel, Str name) {
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    if (mode == FavDlgMode::Add && len(filePath) == 0) {
        return;
    }
    // orig reuses the open dialog and re-targets it
    if (gAddFav.visible && gAddFav.mode != mode) {
        CloseAddFavoriteDialog();
    }
    bool wasVisible = gAddFav.visible;
    gAddFav.mode = mode;
    if (!wasVisible) {
        FreeInput(nullptr, &gAddFav.editName);
        FreeInput(nullptr, &gAddFav.editRename);
        if (mode == FavDlgMode::Add) {
            gAddFav.editName = NewInput(win);
        }
        gAddFav.editRename = NewInput(win);
        gAddFav.scrollY = 0;
        BuildFavEditRows(gAddFav.rows);
        gAddFav.sel = -1;
    }
    gAddFav.win = win;
    gAddFav.pageNo = pageNo;
    str::ReplaceWithCopy(&gAddFav.pageLabel, pageLabel);
    str::ReplaceWithCopy(&gAddFav.filePath, filePath);
    if (gAddFav.editName) {
        gp::InputSetValue(gAddFav.editName, ToGpui(name));
    }
    gAddFav.visible = true;
    DlgWindowSpec spec;
    spec.name = "addfavorite";
    spec.title = AddFavoriteDlgTitle;
    spec.modal = true;
    spec.build = AddFavoriteDialogBuild;
    spec.close = CloseAddFavoriteDialog;
    spec.clientDx = kAddFavDlgDx;
    gAddFavoriteTw = DlgWindowOpen(spec, win);
    gAddFav.wantFocus = gAddFav.editName != nullptr;
    AppShellInvalidate(win);
}

void ShowAddFavoriteDialog(MainWindow* win, Str filePath, int pageNo, Str pageLabel, Str name) {
    ShowFavoriteDialog(win, FavDlgMode::Add, filePath, pageNo, pageLabel, name);
}

void ShowEditFavoritesDialog(MainWindow* win) {
    ShowFavoriteDialog(win, FavDlgMode::Edit, {}, 0, {}, {});
}

static gp::El* FavButton(gp::Ctx* cx, Str id, Str label, void (*fn)(AddFavoriteView*, gp::Ctx*, const gp::ClickEvent*),
                         bool enabled) {
    return DlgGhostButton(cx, GpuiDup(cx->a, id), label, gp::ListenTo(gAddFavView, fn), !enabled);
}

// orig's layout: the prompt and the name (add mode), the favorites, the
// selected one's name with Rename, Delete / Move Up / Move Down, the footer
gp::El* AddFavoriteDialogBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gAddFav.visible || gAddFav.win != win) {
        return nullptr;
    }
    if (gAddFavoriteTw && !DlgWindowIsHost(cx)) {
        return nullptr;
    }
    if (!gAddFavView.IsValid()) {
        gAddFavView = gp::EntityNewState<AddFavoriteView>(cx->app);
    }
    bool isAdd = gAddFav.mode == FavDlgMode::Add;
    gp::Listener onOk = gp::ListenTo(gAddFavView, &AddFavoriteView::OnOk);
    gp::Listener onCancel = gp::ListenTo(gAddFavView, &AddFavoriteView::OnCancel);
    DlgSetDefault(cx, onOk);
    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill);

    if (isAdd) {
        gAddFav.editName->onChange = gp::ListenTo(gAddFavView, &AddFavoriteView::OnInput);
        col->Child(DlgCaption(cx, FavoritePromptTemp(gAddFav.pageLabel)));
        col->Child(DlgLine(cx, {}, 0, StrL("addfav-name"), gAddFav.editName, DlgField::Underlined));
        col->Child(DlgHairline(cx));
    }

    gp::El* list = gp::Div(cx->a)
                       ->Id(GStrL("addfav-list"))
                       ->FlexCol()
                       ->W(gp::kFill)
                       ->H(kFavListLines * kDlgListRowDy)
                       ->Shrink0()
                       ->ScrollY(gAddFav.scrollY)
                       ->ScrollFromPath()
                       ->OnScroll(gp::ListenTo(gAddFavView, &AddFavoriteView::OnScroll));
    for (int i = 0; i < len(gAddFav.rows); i++) {
        list->Child(DlgListRow(cx, fmt("addfav-row-%d", i), FavEditRowTextTemp(gAddFav.rows[i]), i == gAddFav.sel,
                               gp::ListenTo(gAddFavView, &AddFavoriteView::OnRowClick, (intptr_t)i)));
    }
    col->Child(DlgListBox(cx, list));

    FavEditRow* row = SelectedRow();
    Favorite* fav = row ? row->fav : nullptr;
    FileState* fs = row ? row->fs : nullptr;

    // the selected favorite's name, and what can be done to it
    gAddFav.editRename->onChange = gp::ListenTo(gAddFavView, &AddFavoriteView::OnRenameInput);
    gp::El* nameRow = DlgFieldRow(cx);
    nameRow->Child(DlgFieldLabel(cx, Tr("Name:"), 0));
    gp::El* field = DlgUnderlineInput(cx, GStrL("addfav-rename"), gAddFav.editRename, kToolDlgTextFontPx,
                                      ThemeEdgeColor(), false, !fav);
    nameRow->Child(gp::Div(cx->a)->Flex1()->MinW(0)->Child(field));
    nameRow->Child(FavButton(cx, StrL("addfav-btn-rename"), Tr("Rename"), &AddFavoriteView::OnRename, fav));
    col->Child(DlgSection(cx)->Child(nameRow));

    gp::El* actions = gp::Div(cx->a)->FlexRow()->ItemsCenter()->W(gp::kFill)->Shrink0()->PadX(kToolDlgPadDx);
    actions->Gap(kToolDlgGapDx);
    actions->Child(FavButton(cx, StrL("addfav-btn-delete"), Tr("Delete"), &AddFavoriteView::OnDelete, fav));
    actions->Child(FavButton(cx, StrL("addfav-btn-up"), Tr("Move Up"), &AddFavoriteView::OnMoveUp,
                             CanMoveFavorite(fs, fav, FavMove::Up)));
    actions->Child(FavButton(cx, StrL("addfav-btn-down"), Tr("Move Down"), &AddFavoriteView::OnMoveDown,
                             CanMoveFavorite(fs, fav, FavMove::Down)));
    col->Child(actions);

    Str okLabel = isAdd ? Tr("Add Favorite") : Tr("Close");
    col->Child(DlgFooter(cx, okLabel, onOk, isAdd ? onCancel : gp::Listener{}));
    gp::El* content = DlgToolContent(cx, col, AddFavoriteDlgTitle(), kAddFavDlgDx, onCancel);

    if (gAddFav.wantFocus && gAddFav.editName) {
        gp::InputFocus(gAddFav.editName, cx->app, cx->win);
        gp::InputSelectAll(gAddFav.editName, cx->app, cx->win);
        gAddFav.wantFocus = cx->win->input != gAddFav.editName;
    }
    return content;
}
