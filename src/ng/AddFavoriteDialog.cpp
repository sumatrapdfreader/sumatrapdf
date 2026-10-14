/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's AddFavoriteDialog.cpp is a WS_POPUPWINDOW with a prompt, the name
// and Cancel / Add Favorite. Here it is the same column of gpui elements, in a
// window of its own where the platform has one, else in a card over the frame.

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
#include "Favorites.h"
#include "SumatraDialogs.h"
#include "AppHelpersCommon.h"

struct AddFavoriteDlg {
    MainWindow* win = nullptr;
    bool visible = false;
    bool wantFocus = false;
    int pageNo = 0;
    Str pageLabel; // owned
    Str filePath;  // owned
    gpui::InputState* editName = nullptr;
};

static AddFavoriteDlg gAddFav;

constexpr float kAddFavDlgDx = 400;

struct AddFavoriteView {
    static void OnOk(AddFavoriteView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnCancel(AddFavoriteView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnInput(AddFavoriteView* self, gp::Ctx* cx, const gp::InputEvent* ev);
};

static gp::Entity<AddFavoriteView> gAddFavView;

// orig's modal window, where the platform can have one (DlgWindowOpen); null: a
// dialog in the frame
static ToolWindow* gAddFavoriteTw = nullptr;

// the dialog in the frame; a window of its own is not the frame's business
bool IsAddFavoriteDialogVisible() {
    return gAddFav.visible && !gAddFavoriteTw;
}

void CloseAddFavoriteDialog() {
    if (!gAddFav.visible) {
        return;
    }
    gAddFav.visible = false;
    MainWindow* win = gAddFav.win;
    // Blur the window that holds the field before freeing it. The tool
    // window's close otherwise blurs the freed input.
    gp::Window* host = gAddFavoriteTw ? ToolWindowGpui(gAddFavoriteTw) : (win ? win->gpuiWin : nullptr);
    if (host && gAddFav.editName) {
        gp::InputBlur(gAddFav.editName, host->app, host);
    }
    delete gAddFav.editName;
    gAddFav.editName = nullptr;
    DlgWindowClose(&gAddFavoriteTw);
    str::Free(gAddFav.pageLabel);
    gAddFav.pageLabel = {};
    str::Free(gAddFav.filePath);
    gAddFav.filePath = {};
    AppShellInvalidate(win);
}

// orig's AddFavoriteWnd::OnOk
static void AddFavoriteOk() {
    MainWindow* win = gAddFav.win;
    TempStr name;
    if (gAddFav.editName) {
        name = str::DupTemp(FromGpui(gp::InputValue(gAddFav.editName)));
    }
    str::TrimWSInPlace(name, str::TrimOpt::Both);
    if (len(name) == 0) {
        name = {};
    }
    ApplyAddFavorite(win, gAddFav.filePath, gAddFav.pageNo, gAddFav.pageLabel, name);
    CloseAddFavoriteDialog();
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

static Str AddFavoriteDlgTitle() {
    return Tr("Add Favorite");
}

void ShowAddFavoriteDialog(MainWindow* win, Str filePath, int pageNo, Str pageLabel, Str name) {
    if (!IsMainWindowValidAndNotClosing(win) || len(filePath) == 0) {
        return;
    }
    // orig reuses the open dialog and re-targets it
    bool wasVisible = gAddFav.visible;
    if (!wasVisible) {
        delete gAddFav.editName;
        gAddFav.editName = new gp::InputState();
        gAddFav.editName->focus = gp::FocusHandleNew(win->gpuiWin ? win->gpuiWin->app : nullptr);
    }
    gAddFav.win = win;
    gAddFav.pageNo = pageNo;
    str::ReplaceWithCopy(&gAddFav.pageLabel, pageLabel);
    str::ReplaceWithCopy(&gAddFav.filePath, filePath);
    gp::InputSetValue(gAddFav.editName, ToGpui(name));
    gAddFav.visible = true;
    DlgWindowSpec spec;
    spec.name = "addfavorite";
    spec.title = AddFavoriteDlgTitle;
    spec.modal = true;
    spec.build = AddFavoriteDialogBuild;
    spec.close = CloseAddFavoriteDialog;
    spec.clientDx = kAddFavDlgDx;
    gAddFavoriteTw = DlgWindowOpen(spec, win);
    gAddFav.wantFocus = true;
    AppShellInvalidate(win);
}

// orig's layout: the prompt, the name over a hairline, Cancel / Add Favorite
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
    gAddFav.editName->onChange = gp::ListenTo(gAddFavView, &AddFavoriteView::OnInput);

    gp::Listener onOk = gp::ListenTo(gAddFavView, &AddFavoriteView::OnOk);
    gp::Listener onCancel = gp::ListenTo(gAddFavView, &AddFavoriteView::OnCancel);
    DlgSetDefault(cx, onOk);
    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill);
    col->Child(DlgCaption(cx, FavoritePromptTemp(gAddFav.pageLabel)));
    col->Child(DlgLine(cx, {}, 0, StrL("addfav-name"), gAddFav.editName, DlgField::Underlined));
    col->Child(DlgFooter(cx, Tr("Add Favorite"), onOk, onCancel));
    gp::El* content = DlgToolContent(cx, col, Tr("Add Favorite"), kAddFavDlgDx, onCancel);

    if (gAddFav.wantFocus) {
        gp::InputFocus(gAddFav.editName, cx->app, cx->win);
        gp::InputSelectAll(gAddFav.editName, cx->app, cx->win);
        gAddFav.wantFocus = cx->win->input != gAddFav.editName;
    }
    return content;
}
