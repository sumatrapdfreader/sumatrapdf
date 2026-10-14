/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's ChangeLanguageDialog.cpp is a WS_POPUPWINDOW with a search Edit
// and a VirtListBox. Here it is the same column of gpui elements: the search,
// the filtered language list with the current language selected, and
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
#include "shared/PdfToolsCommon.h"

#include "SumatraLog.h"

static constexpr float kLangDlgDx = 320;
static constexpr int kLangListLines = 16;

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
    float viewDy = kLangListLines * kDlgListRowDy;
    gChangeLanguage.scrollY = DialogScrollToRow(gChangeLanguage.scrollY, gChangeLanguage.sel, n, kDlgListRowDy, viewDy);
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
    gp::InputSetPlaceholder(s, ToGpui(Tr("Search")));
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
    spec.clientDx = kLangDlgDx;
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

// orig's layout: the search, the languages, Cancel / OK
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
    gChangeLanguage.editSearch->onChange = gp::ListenTo(gChangeLanguageView, &ChangeLanguageView::OnInput);
    gp::Listener onOk = gp::ListenTo(gChangeLanguageView, &ChangeLanguageView::OnOk);
    gp::Listener onCancel = gp::ListenTo(gChangeLanguageView, &ChangeLanguageView::OnCancel);
    DlgSetDefault(cx, onOk);

    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill);
    col->Child(DlgLine(cx, {}, 0, StrL("language-search"), gChangeLanguage.editSearch, DlgField::Plain));
    col->Child(DlgHairline(cx));
    gp::El* list = gp::Div(cx->a)
                       ->Id(GStrL("language-list"))
                       ->FlexCol()
                       ->W(gp::kFill)
                       ->H(kLangListLines * kDlgListRowDy)
                       ->Shrink0()
                       ->ScrollY(gChangeLanguage.scrollY)
                       ->ScrollFromPath()
                       ->OnScroll(gp::ListenTo(gChangeLanguageView, &ChangeLanguageView::OnScroll));
    for (int i = 0; i < len(gChangeLanguage.langIdxByListIdx); i++) {
        TempStr name = trans::GetLangNameByIdxTemp(gChangeLanguage.langIdxByListIdx[i]);
        list->Child(DlgListRow(cx, fmt("language-row-%d", i), name, i == gChangeLanguage.sel,
                               gp::ListenTo(gChangeLanguageView, &ChangeLanguageView::OnRowClick, (intptr_t)i)));
    }
    col->Child(DlgListBox(cx, list));
    col->Child(DlgFooter(cx, Tr("OK"), onOk, onCancel));
    gp::El* content = DlgToolContent(cx, col, ChangeLanguageDlgTitle(), kLangDlgDx, onCancel);

    if (gChangeLanguage.wantFocus) {
        gp::InputFocus(gChangeLanguage.editSearch, cx->app, cx->win);
        gChangeLanguage.wantFocus = cx->win->input != gChangeLanguage.editSearch;
    }
    return content;
}
