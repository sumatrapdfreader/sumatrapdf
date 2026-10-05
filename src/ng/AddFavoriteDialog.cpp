/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's AddFavoriteDialog.cpp is a WS_POPUPWINDOW with a VirtText prompt,
// an Edit for the name and Cancel / OK buttons. Here it is a gpui Dialog with
// the same prompt text and the same fields.

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
#include "Favorites.h"
#include "SumatraDialogs.h"

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

// orig's window at 96 dpi: a 360 wide client area, 4 / 8 around; the prompt
// with 4 under it, the edit, the buttons with 4 above and below
constexpr float kAddFavWinDx = 360;
constexpr float kAddFavWinPadX = 8;
constexpr float kAddFavWinPadY = 4;

struct AddFavoriteView {
    static void OnOk(AddFavoriteView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnCancel(AddFavoriteView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnInput(AddFavoriteView* self, gp::Ctx* cx, const gp::InputEvent* ev);
};

static gp::Entity<AddFavoriteView> gAddFavView;

static TempStr FavoritePromptTemp(Str pageLabel) {
    int chapter = 0, page = 0;
    if (str::Parse(pageLabel, "%d/%d%$", &chapter, &page)) {
        return fmt(Tr("Name for chapter %d page %d (optional):").s, chapter, page);
    }
    return fmt(Tr("Name for page %s (optional):").s, pageLabel);
}

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
    DlgWindowClose(&gAddFavoriteTw);
    MainWindow* win = gAddFav.win;
    if (win && win->gpuiWin && gAddFav.editName) {
        gp::InputBlur(gAddFav.editName, win->gpuiWin->app, win->gpuiWin);
    }
    delete gAddFav.editName;
    gAddFav.editName = nullptr;
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
    spec.clientDx = kAddFavWinDx;
    gAddFavoriteTw = DlgWindowOpen(spec, win);
    gAddFav.wantFocus = true;
    AppShellInvalidate(win);
}

static gp::El* AddFavoriteWinBuild(gp::Ctx* cx) {
    float font = DlgWinFont(cx);
    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->PadX(kAddFavWinPadX)->PadY(kAddFavWinPadY);
    col->Child(DlgWinLabel(cx, GpuiDup(cx->a, FavoritePromptTemp(gAddFav.pageLabel)), font, kAddFavWinPadY));
    col->Child(DlgWinEdit(cx, GStrL("addfav-name"), gAddFav.editName));

    gp::Listener onOk = gp::ListenTo(gAddFavView, &AddFavoriteView::OnOk);
    DlgSetDefault(cx, onOk);
    gp::El* buttons = DlgWinButtonRow(cx, kAddFavWinPadY);
    buttons->Child(DlgWinButton(cx, GStrL("dlg-cancel"), Tr("Cancel"),
                                gp::ListenTo(gAddFavView, &AddFavoriteView::OnCancel), false));
    buttons->Child(DlgWinButton(cx, GStrL("dlg-ok"), Tr("OK"), onOk, true));
    col->Child(buttons);
    return DlgWinContent(cx, col);
}

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
    const gp::Theme& th = gp::ThemeNow(cx->app);
    gAddFav.editName->onChange = gp::ListenTo(gAddFavView, &AddFavoriteView::OnInput);

    if (DlgWindowIsHost(cx)) {
        gp::El* content = AddFavoriteWinBuild(cx);
        if (gAddFav.wantFocus) {
            gp::InputFocus(gAddFav.editName, cx->app, cx->win);
            gp::InputSelectAll(gAddFav.editName, cx->app, cx->win);
            gAddFav.wantFocus = cx->win->input != gAddFav.editName;
        }
        return content;
    }

    gp::El* body = gp::Div(cx->a)->FlexCol()->Gap(8);
    body->Child(gp::TextEl(cx->a, GpuiDup(cx->a, FavoritePromptTemp(gAddFav.pageLabel)))->Font(13)->Fg(th.foreground));
    body->Child(gpc::Input::New(cx, GStrL("addfav-name"), gAddFav.editName)
                    ->WithSize(gp::UiSize::Small)
                    ->W(gp::kFill)
                    ->IntoEl());

    DlgSetDefault(cx, gp::ListenTo(gAddFavView, &AddFavoriteView::OnOk));
    gp::El* dlg = DlgIntoEl(cx, gpc::Dialog::New(cx)
                                    ->Open(true)
                                    ->Title(ToGpui(Tr("Add Favorite")))
                                    ->Body(body)
                                    ->W(360)
                                    ->OkText(ToGpui(Tr("OK")))
                                    ->CancelText(ToGpui(Tr("Cancel")))
                                    ->ShowCancel(true)
                                    ->OnOk(gp::ListenTo(gAddFavView, &AddFavoriteView::OnOk))
                                    ->OnCancel(gp::ListenTo(gAddFavView, &AddFavoriteView::OnCancel))
                                    ->OnClose(gp::ListenTo(gAddFavView, &AddFavoriteView::OnCancel)));

    if (gAddFav.wantFocus) {
        gAddFav.wantFocus = false;
        gp::InputFocus(gAddFav.editName, cx->app, cx->win);
        gp::InputSelectAll(gAddFav.editName, cx->app, cx->win);
    }
    return dlg;
}
