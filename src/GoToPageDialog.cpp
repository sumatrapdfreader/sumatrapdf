/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"
#include "gui/Dpi.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"

#include "Settings.h"
#include "AppSettings.h"
#include "EngineBase.h"
#include "DocController.h"
#include "MainWindow.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "DarkMode_win.h"
#include "SumatraDialogs.h"

// Labels and buttons are VirtCtrl; the page field is a real HWND Edit.
// Same WindowBase layout pattern as Change Theme / Add Favorite.
struct GoToPageWnd : WindowBase {
    ~GoToPageWnd() override = default;

    MainWindow* win = nullptr;
    int pageCount = 0;
    bool onlyNumeric = true;
    bool hasChapters = false;
    VirtText* label = nullptr;
    Edit* editPage = nullptr;
    VirtText* labelOf = nullptr;
    // chapter row; only built when hasChapters was true at Create() time
    VirtText* chapterLabel = nullptr;
    Edit* editChapter = nullptr;
    VirtText* chapterLabelOf = nullptr;
    VirtButton* btnCancel = nullptr;
    VirtButton* btnGo = nullptr;

    bool Create(MainWindow* win);
    void SetTarget(MainWindow* win);

    void OnCancel(VirtMouseEvent* ev = nullptr);
    void OnOk(VirtMouseEvent* ev = nullptr);
};

static GoToPageWnd* gGoToPageWnd = nullptr;

static void ClearGoToPageWnd() {
    gGoToPageWnd = nullptr;
}

// caller (ShowGoToPageDialog) only reuses this window when hasChapters still
// matches, so the row layout built in Create() stays valid here
void GoToPageWnd::SetTarget(MainWindow* mainWin) {
    win = mainWin;
    pageCount = 0;
    onlyNumeric = true;
    Str pageLabel;
    int chapterCount = 0;
    int chapterCur = 0;
    if (IsMainWindowValidAndNotClosing(win) && win->IsDocLoaded() && win->ctrl) {
        DocController* ctrl = win->ctrl;
        if (hasChapters) {
            Location cur = ctrl->CurrentLocation();
            chapterCount = ctrl->ChapterCount();
            chapterCur = cur.chapter;
            pageCount = ctrl->ChapterPageCount(cur.chapter);
            pageLabel = fmt("%d", cur.page);
        } else {
            pageCount = ctrl->PageCount();
            onlyNumeric = !ctrl->HasPageLabels();
            pageLabel = ctrl->GetPageLabeTemp(ctrl->CurrentPageNo());
        }
    }
    if (labelOf) {
        labelOf->SetText(fmt(_TRA("(of %d)").s, pageCount));
    }
    if (editPage) {
        EditSetNumbersOnly(editPage, onlyNumeric);
        editPage->SetText(pageLabel);
        EditSelectAll(editPage);
    }
    if (chapterLabelOf) {
        chapterLabelOf->SetText(fmt(_TRA("(of %d)").s, chapterCount));
    }
    if (editChapter) {
        editChapter->SetText(fmt("%d", chapterCur));
    }
}

void GoToPageWnd::OnCancel(VirtMouseEvent*) {
    ScheduleDelete();
}

void GoToPageWnd::OnOk(VirtMouseEvent*) {
    if (!IsMainWindowValidAndNotClosing(win) || !win->IsDocLoaded() || !win->ctrl) {
        ScheduleDelete();
        return;
    }
    if (hasChapters) {
        int chapter = editChapter ? ParseInt(editChapter->GetTextTemp()) : 1;
        int page = editPage ? ParseInt(editPage->GetTextTemp()) : 1;
        Location loc = win->ctrl->ClampLocation({chapter, page});
        win->ctrl->GoToLocation(loc, true);
        ScheduleDelete();
        return;
    }
    TempStr pageLabel = editPage ? editPage->GetTextTemp() : Str{};
    int newPageNo = win->ctrl->GetPageByLabel(pageLabel);
    if (win->ctrl->ValidPageNo(newPageNo)) {
        win->ctrl->GoToPage(newPageNo, true);
    }
    ScheduleDelete();
}

static void OnClose(WindowBase::CloseEvent* /*ev*/) {
    if (gGoToPageWnd) {
        gGoToPageWnd->OnCancel();
    }
}

static void OnDestroy(WindowBase::DestroyEvent* /*ev*/) {
    if (gGoToPageWnd) {
        gGoToPageWnd->ScheduleDelete();
    }
}

bool GoToPageWnd::Create(MainWindow* mainWin) {
    win = mainWin;
    Str pageLabel;
    int chapterCount = 0;
    int chapterCur = 0;
    if (IsMainWindowValidAndNotClosing(win) && win->IsDocLoaded() && win->ctrl) {
        DocController* ctrl = win->ctrl;
        hasChapters = ctrl->HasChapters();
        if (hasChapters) {
            Location cur = ctrl->CurrentLocation();
            chapterCount = ctrl->ChapterCount();
            chapterCur = cur.chapter;
            pageCount = ctrl->ChapterPageCount(cur.chapter);
            pageLabel = fmt("%d", cur.page);
        } else {
            pageCount = ctrl->PageCount();
            onlyNumeric = !ctrl->HasPageLabels();
            pageLabel = ctrl->GetPageLabeTemp(ctrl->CurrentPageNo());
        }
    }

    {
        CreateCustomArgs args;
        args.owner = win ? win->hwndFrame : nullptr;
        args.title = _TRA("Go to page");
        args.visible = false;
        args.style = WS_POPUPWINDOW | WS_CAPTION;
        args.font = GetFont();
        args.icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(GetAppIconID()));
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }
    bool isRtl = IsUIRtl();

    auto* vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    if (hasChapters) {
        auto* hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainStart;
        hbox->alignCross = CrossAxisAlign::CrossCenter;

        auto* lab = NewVirtText({
            .s = _TRA("&Chapter:"),
            .font = font,
            .isRtl = isRtl,
            .prefix = true,
        });
        chapterLabel = lab;
        hbox->AddChild(lab);

        Edit::CreateArgs args;
        args.parent = hwnd;
        args.font = GetFont();
        args.withBorder = true;
        args.alignRight = true;
        args.numbersOnly = true;
        args.selectAllOnFocus = true;
        args.isRtl = isRtl;
        args.text = fmt("%d", chapterCur);
        args.idealWidthChars = 6;
        auto* ce = new Edit();
        ce->SetInsetsPt(0, 8, 0, 8);
        ce->Create(args);
        editChapter = ce;
        hbox->AddChild(ce);

        auto* of = NewVirtText({
            .s = fmt(_TRA("(of %d)").s, chapterCount),
            .font = font,
            .isRtl = isRtl,
        });
        chapterLabelOf = of;
        hbox->AddChild(of);
        vbox->AddChild(hbox);
    }

    {
        auto* hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainStart;
        hbox->alignCross = CrossAxisAlign::CrossCenter;

        auto* lab = NewVirtText({
            .s = hasChapters ? _TRA("&Page:") : _TRA("&Go to page:"),
            .font = font,
            .isRtl = isRtl,
            .prefix = true,
        });
        label = lab;
        hbox->AddChild(lab);

        Edit::CreateArgs args;
        args.parent = hwnd;
        args.font = GetFont();
        args.withBorder = true;
        args.alignRight = true;
        args.numbersOnly = onlyNumeric;
        args.selectAllOnFocus = true;
        args.isRtl = isRtl;
        args.text = pageLabel;
        args.idealWidthChars = 6;
        auto* e = new Edit();
        e->SetInsetsPt(0, 8, 0, 8);
        e->Create(args);
        editPage = e;
        hbox->AddChild(e);

        auto* of = NewVirtText({
            .s = fmt(_TRA("(of %d)").s, pageCount),
            .font = font,
            .isRtl = isRtl,
        });
        labelOf = of;
        hbox->AddChild(of);
        vbox->AddChild(hbox);
    }

    {
        auto* hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainEnd;
        hbox->alignCross = CrossAxisAlign::CrossCenter;
        hbox->gap = font->averageCharWidth;
        auto pad = Insets{4, 0, 4, 0};

        btnCancel = NewThemedButton(hwnd, _TRA("Cancel"), font, false);
        btnCancel->onClick = MkMethod1<GoToPageWnd, VirtMouseEvent*, &GoToPageWnd::OnCancel>(this);
        hbox->AddChild(new Padding(btnCancel, pad));
        btnGo = NewThemedButton(hwnd, _TRA("Go to page"), font, true);
        btnGo->onClick = MkMethod1<GoToPageWnd, VirtMouseEvent*, &GoToPageWnd::OnOk>(this);
        hbox->AddChild(new Padding(btnGo, pad));
        vbox->AddChild(hbox);
    }

    auto* padding = new Padding(vbox, DpiScaledInsets(4, 8));
    layout = padding;

    int dx = DpiScale(300);
    LayoutAndSizeToContent(layout, dx, 0, hwnd);
    DoLayout(HwndClientRect(hwnd).Size());
    HwndCenterDialog(hwnd, win ? win->hwndFrame : nullptr);
    UpdateTheme();

    SetIsVisible(true);
    Edit* focusTarget = hasChapters && editChapter ? editChapter : editPage;
    EditSelectAll(focusTarget);
    EditSetFocus(focusTarget);
    return true;
}

void ShowGoToPageDialog(MainWindow* win) {
    if (!IsMainWindowValidAndNotClosing(win) || !win->IsDocLoaded()) {
        return;
    }
    // the chapter row is only built in Create(); rebuild if the doc kind changed
    bool hasChapters = win->ctrl && win->ctrl->HasChapters();
    if (gGoToPageWnd && gGoToPageWnd->hasChapters != hasChapters) {
        gGoToPageWnd->ScheduleDelete();
        gGoToPageWnd = nullptr;
    }
    if (gGoToPageWnd) {
        gGoToPageWnd->SetTarget(win);
        if (gGoToPageWnd->hwnd) {
            gGoToPageWnd->DoLayout(HwndClientRect(gGoToPageWnd->hwnd).Size());
            HwndInvalidate(gGoToPageWnd->hwnd);
        }
        HwndSetFocus(gGoToPageWnd->hwnd);
        Edit* focusTarget =
            gGoToPageWnd->hasChapters && gGoToPageWnd->editChapter ? gGoToPageWnd->editChapter : gGoToPageWnd->editPage;
        EditSetFocus(focusTarget);
        return;
    }
    auto* wnd = new GoToPageWnd();
    wnd->closeOnEsc = true;
    wnd->onBeforeDelete = MkFunc0Void(ClearGoToPageWnd);
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnDestroy);
    wnd->SetFont(GetAppFont());
    bool ok = wnd->Create(win);
    if (!ok) {
        delete wnd;
        return;
    }
    gGoToPageWnd = wnd;
    RunModalWindow(wnd->hwnd, win ? win->hwndFrame : nullptr);
}
