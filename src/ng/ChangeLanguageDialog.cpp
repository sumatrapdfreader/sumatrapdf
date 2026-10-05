/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's ChangeLanguageDialog.cpp is a WS_POPUPWINDOW with a search Edit
// and a VirtListBox. Here it is a gpui Dialog with the same rows: the search
// box, the filtered language list with the current language selected, and
// Cancel / OK.

#include "gui/GpuiBridge.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "MainWindow.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "gui/AppShell.h"
#include "gui/DialogWidgets.h"
#include "SumatraDialogs.h"

#include "SumatraLog.h"

static constexpr float kLangRowDy = 22;
static constexpr float kLangListMinDy = 200;
static constexpr float kLangListMaxDy = 360;
// orig's window at 96 dpi: a 280 wide client area, 4 / 8 around; the search
// box, 16 rows of the list, the buttons with 4 above and below
static constexpr float kLangWinDx = 280;
static constexpr float kLangWinPadX = 8;
static constexpr float kLangWinPadY = 4;
static constexpr int kLangWinListLines = 16;

struct ChangeLanguageDlg {
    MainWindow* win = nullptr;
    bool visible = false;
    int sel = -1;
    Vec<int> langIdxByListIdx;
    gpui::InputState* editSearch = nullptr;
    bool wantFocus = false;
    float scrollY = 0;
};

static ChangeLanguageDlg gChangeLanguage;

struct ChangeLanguageView {
    static void OnOk(ChangeLanguageView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnCancel(ChangeLanguageView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnInput(ChangeLanguageView* self, gp::Ctx* cx, const gp::InputEvent* ev);
    static void OnRowClick(ChangeLanguageView* self, gp::Ctx* cx, const gp::ClickEvent* ev, int64_t idx);
    static void OnScroll(ChangeLanguageView* self, gp::Ctx* cx, const gp::ScrollEvent* ev);
};

static gp::Entity<ChangeLanguageView> gChangeLanguageView;

// orig's modal window, where the platform can have one (DlgWindowOpen); null: a
// dialog in the frame
static ToolWindow* gChangeLanguageTw = nullptr;

// the dialog in the frame; a window of its own is not the frame's business
bool IsChangeLanguageDialogVisible() {
    return gChangeLanguage.visible && !gChangeLanguageTw;
}

void CloseChangeLanguageDialog() {
    if (!gChangeLanguage.visible) {
        return;
    }
    gChangeLanguage.visible = false;
    DlgWindowClose(&gChangeLanguageTw);
    MainWindow* win = gChangeLanguage.win;
    if (win && win->gpuiWin && gChangeLanguage.editSearch) {
        gp::InputBlur(gChangeLanguage.editSearch, win->gpuiWin->app, win->gpuiWin);
    }
    delete gChangeLanguage.editSearch;
    gChangeLanguage.editSearch = nullptr;
    VecReset(gChangeLanguage.langIdxByListIdx);
    AppShellInvalidate(win);
}

// what orig's ListBox does when its selection changes
static void ScrollSelIntoView() {
    int n = len(gChangeLanguage.langIdxByListIdx);
    float rowDy = kLangRowDy;
    float viewDy = DialogListViewDy(n, kLangRowDy, kLangListMinDy, kLangListMaxDy);
    if (gChangeLanguageTw) {
        rowDy = kDlgWinRowDy;
        viewDy = kLangWinListLines * kDlgWinRowDy;
    }
    gChangeLanguage.scrollY = DialogScrollToRow(gChangeLanguage.scrollY, gChangeLanguage.sel, n, rowDy, viewDy);
}

// orig's ChangeLanguageWnd::FilterList: rebuild from the search box, keeping
// the current UI language selected while it still matches
static void FilterList() {
    TempStr filter;
    if (gChangeLanguage.editSearch) {
        filter = str::DupTemp(FromGpui(gp::InputValue(gChangeLanguage.editSearch)));
    }
    VecReset(gChangeLanguage.langIdxByListIdx);
    int itemToSelect = 0;
    Str currLangCode = trans::GetCurrentLangCode();
    for (int i = 0; i < trans::GetLangsCount(); i++) {
        TempStr name = trans::GetLangNameByIdxTemp(i);
        if (len(filter) > 0 && !str::ContainsI(name, filter)) {
            continue;
        }
        if (str::Eq(trans::GetLangCodeByIdxTemp(i), currLangCode)) {
            itemToSelect = len(gChangeLanguage.langIdxByListIdx);
        }
        VecAppend(gChangeLanguage.langIdxByListIdx, i);
    }
    gChangeLanguage.sel = len(gChangeLanguage.langIdxByListIdx) > 0 ? itemToSelect : -1;
    gChangeLanguage.scrollY = 0;
    ScrollSelIntoView();
}

static Str ChangeLanguageDlgTitle() {
    return Tr("Change Language");
}

// Up / Down from the search box move the list
static bool ChangeLanguageDlgOnArrow(int dir, bool editFocused) {
    return editFocused && ChangeLanguageMoveSelection(dir);
}

void ShowChangeLanguageDialog(MainWindow* win) {
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    CloseChangeLanguageDialog();
    gChangeLanguage.win = win;
    auto* s = new gp::InputState();
    s->focus = gp::FocusHandleNew(win->gpuiWin ? win->gpuiWin->app : nullptr);
    gChangeLanguage.editSearch = s;
    FilterList();
    gChangeLanguage.visible = true;
    DlgWindowSpec spec;
    spec.name = "changelanguage";
    spec.title = ChangeLanguageDlgTitle;
    spec.modal = true;
    spec.build = ChangeLanguageDialogBuild;
    spec.close = CloseChangeLanguageDialog;
    spec.onArrow = ChangeLanguageDlgOnArrow;
    spec.clientDx = kLangWinDx;
    gChangeLanguageTw = DlgWindowOpen(spec, win);
    // the window's rows are not the frame dialog's
    ScrollSelIntoView();
    gChangeLanguage.wantFocus = true;
    AppShellInvalidate(win);
}

// orig's ChangeLanguageWnd::OnOk
void ChangeLanguageOk() {
    int idx = gChangeLanguage.sel;
    TempStr code;
    if (idx >= 0 && idx < len(gChangeLanguage.langIdxByListIdx)) {
        code = trans::GetLangCodeByIdxTemp(gChangeLanguage.langIdxByListIdx[idx]);
    }
    CloseChangeLanguageDialog();
    if (len(code) == 0) {
        return;
    }
    logf("ChangeLanguageDialog: language '%s'\n", code);
    SetCurrentLanguageAndRefreshUI(code);
}

// orig's OnKeyDown: Up / Down from the search box move the list (wrapping)
bool ChangeLanguageMoveSelection(int dir) {
    if (!gChangeLanguage.visible) {
        return false;
    }
    int n = len(gChangeLanguage.langIdxByListIdx);
    if (n == 0) {
        return false;
    }
    int sel = gChangeLanguage.sel;
    if (dir < 0) {
        sel = sel <= 0 ? n - 1 : sel - 1;
    } else {
        sel = (sel < 0 || sel >= n - 1) ? 0 : sel + 1;
    }
    gChangeLanguage.sel = sel;
    ScrollSelIntoView();
    AppShellInvalidate(gChangeLanguage.win);
    return true;
}

void ChangeLanguageView::OnOk(ChangeLanguageView*, gp::Ctx* cx, const gp::ClickEvent*) {
    ChangeLanguageOk();
    gp::Notify(cx);
}

void ChangeLanguageView::OnCancel(ChangeLanguageView*, gp::Ctx* cx, const gp::ClickEvent*) {
    CloseChangeLanguageDialog();
    gp::Notify(cx);
}

void ChangeLanguageView::OnInput(ChangeLanguageView*, gp::Ctx* cx, const gp::InputEvent* ev) {
    if (ev->kind == gp::InputEventKind::PressEnter) {
        ChangeLanguageOk();
        gp::Notify(cx);
        return;
    }
    if (ev->kind != gp::InputEventKind::Change) {
        return;
    }
    FilterList();
    gp::Notify(cx);
}

void ChangeLanguageView::OnRowClick(ChangeLanguageView*, gp::Ctx* cx, const gp::ClickEvent* ev, int64_t idx) {
    gChangeLanguage.sel = (int)idx;
    if (ev->clickCount >= 2) {
        ChangeLanguageOk();
    }
    gp::Notify(cx);
}

void ChangeLanguageView::OnScroll(ChangeLanguageView*, gp::Ctx* cx, const gp::ScrollEvent* ev) {
    gChangeLanguage.scrollY = ev->offsetY;
    gp::Notify(cx);
    AppShellInvalidate(gChangeLanguage.win);
}

static gp::El* ChangeLanguageWinBuild(gp::Ctx* cx) {
    const gp::Theme& th = gp::ThemeNow(cx->app);
    float font = DlgWinFont(cx);
    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->PadX(kLangWinPadX)->PadY(kLangWinPadY);
    col->Child(DlgWinEdit(cx, GStrL("language-search"), gChangeLanguage.editSearch));

    gp::El* list = gp::Div(cx->a)
                       ->Id(GStrL("language-list"))
                       ->FlexCol()
                       ->W(gp::kFill)
                       ->H(kLangWinListLines * kDlgWinRowDy)
                       ->Shrink0()
                       ->ScrollY(gChangeLanguage.scrollY)
                       ->ScrollFromPath()
                       ->OnScroll(gp::ListenTo(gChangeLanguageView, &ChangeLanguageView::OnScroll));
    for (int i = 0; i < len(gChangeLanguage.langIdxByListIdx); i++) {
        TempStr name = trans::GetLangNameByIdxTemp(gChangeLanguage.langIdxByListIdx[i]);
        gp::El* row = gp::Div(cx->a)
                          ->FlexRow()
                          ->W(gp::kFill)
                          ->H(kDlgWinRowDy)
                          ->Shrink0()
                          ->ItemsCenter()
                          ->PadX(4)
                          ->PathClick(GpuiDup(cx->a, fmt("language-row-%d", i)))
                          ->OnClick(gp::ListenTo(gChangeLanguageView, &ChangeLanguageView::OnRowClick, (intptr_t)i));
        if (i == gChangeLanguage.sel) {
            row->Bg(DlgWinListSelBg(false));
        }
        row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, name))->Font(font)->Fg(th.foreground));
        list->Child(row);
    }
    col->Child(list);

    gp::Listener onOk = gp::ListenTo(gChangeLanguageView, &ChangeLanguageView::OnOk);
    DlgSetDefault(cx, onOk);
    gp::El* buttons = DlgWinButtonRow(cx, kLangWinPadY);
    buttons->Child(DlgWinButton(cx, GStrL("dlg-cancel"), Tr("Cancel"),
                                gp::ListenTo(gChangeLanguageView, &ChangeLanguageView::OnCancel), false));
    buttons->Child(DlgWinButton(cx, GStrL("dlg-ok"), Tr("OK"), onOk, true));
    col->Child(buttons);
    return DlgWinContent(cx, col);
}

gp::El* ChangeLanguageDialogBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gChangeLanguage.visible || gChangeLanguage.win != win) {
        return nullptr;
    }
    if (gChangeLanguageTw && !DlgWindowIsHost(cx)) {
        return nullptr;
    }
    if (!gChangeLanguageView.IsValid()) {
        gChangeLanguageView = gp::EntityNewState<ChangeLanguageView>(cx->app);
    }
    const gp::Theme& th = gp::ThemeNow(cx->app);
    gChangeLanguage.editSearch->onChange = gp::ListenTo(gChangeLanguageView, &ChangeLanguageView::OnInput);

    if (DlgWindowIsHost(cx)) {
        gp::El* content = ChangeLanguageWinBuild(cx);
        if (gChangeLanguage.wantFocus) {
            gp::InputFocus(gChangeLanguage.editSearch, cx->app, cx->win);
            gChangeLanguage.wantFocus = cx->win->input != gChangeLanguage.editSearch;
        }
        return content;
    }

    gp::El* body = gp::Div(cx->a)->FlexCol()->Gap(8);
    body->Child(gpc::Input::New(cx, GStrL("language-search"), gChangeLanguage.editSearch)
                    ->WithSize(gp::UiSize::Small)
                    ->W(gp::kFill)
                    ->IntoEl());

    gp::El* list = gp::Div(cx->a)
                       ->Id(GStrL("language-list"))
                       ->FlexCol()
                       ->W(gp::kFill)
                       ->MinH(kLangListMinDy)
                       ->MaxH(kLangListMaxDy)
                       ->ScrollY(gChangeLanguage.scrollY)
                       ->ScrollFromPath()
                       ->OnScroll(gp::ListenTo(gChangeLanguageView, &ChangeLanguageView::OnScroll))
                       ->Border(1, th.border);
    for (int i = 0; i < len(gChangeLanguage.langIdxByListIdx); i++) {
        TempStr name = trans::GetLangNameByIdxTemp(gChangeLanguage.langIdxByListIdx[i]);
        gp::El* row = gp::Div(cx->a)
                          ->FlexRow()
                          ->W(gp::kFill)
                          ->H(kLangRowDy)
                          ->Shrink0()
                          ->ItemsCenter()
                          ->PadX(6)
                          ->PathClick(GpuiDup(cx->a, fmt("language-row-%d", i)))
                          ->OnClick(gp::ListenTo(gChangeLanguageView, &ChangeLanguageView::OnRowClick, (intptr_t)i));
        if (i == gChangeLanguage.sel) {
            row->Bg(th.selection);
        }
        row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, name))->Font(13)->Fg(th.foreground));
        list->Child(row);
    }
    body->Child(list);

    DlgSetDefault(cx, gp::ListenTo(gChangeLanguageView, &ChangeLanguageView::OnOk));
    gp::El* dlg = DlgIntoEl(cx, gpc::Dialog::New(cx)
                                    ->Open(true)
                                    ->Title(ToGpui(Tr("Change Language")))
                                    ->Body(body)
                                    ->W(360)
                                    ->OkText(ToGpui(Tr("OK")))
                                    ->CancelText(ToGpui(Tr("Cancel")))
                                    ->ShowCancel(true)
                                    ->OnOk(gp::ListenTo(gChangeLanguageView, &ChangeLanguageView::OnOk))
                                    ->OnCancel(gp::ListenTo(gChangeLanguageView, &ChangeLanguageView::OnCancel))
                                    ->OnClose(gp::ListenTo(gChangeLanguageView, &ChangeLanguageView::OnCancel)));

    if (gChangeLanguage.wantFocus) {
        gp::InputFocus(gChangeLanguage.editSearch, cx->app, cx->win);
        gChangeLanguage.wantFocus = cx->win->input != gChangeLanguage.editSearch;
    }
    return dlg;
}
