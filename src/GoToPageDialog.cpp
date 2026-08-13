/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"
#include "gui/Dpi.h"
#include "base/UITask.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"

#include "Settings.h"
#include "AppSettings.h"
#include "GlobalPrefs.h"
#include "DocController.h"
#include "MainWindow.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "DarkModeSubclass.h"
#include "GoToPageDialog.h"

// Labels and buttons are VirtCtrl; the page field is a real HWND Edit.
// Same WindowBase layout pattern as Change Theme / Add Favorite.
struct GoToPageWnd : WindowBase {
    ~GoToPageWnd() override = default;

    HFONT font = nullptr;
    PlatformFont* platformFont = nullptr;
    MainWindow* win = nullptr;
    int pageCount = 0;
    bool onlyNumeric = true;
    VirtText* label = nullptr;
    Edit* editPage = nullptr;
    VirtText* labelOf = nullptr;
    VirtButton* btnCancel = nullptr;
    VirtButton* btnGo = nullptr;

    bool Create(MainWindow* win);
    void SetTarget(MainWindow* win);
    void OnKeyDown(KeyEvent* ev);

    void UpdateTheme();
    void OnCancel();
    void OnOk();
    void ScheduleDelete();
};

static GoToPageWnd* gGoToPageWnd = nullptr;

void SafeDeleteGoToPageDialog() {
    if (!gGoToPageWnd) {
        return;
    }
    auto* tmp = gGoToPageWnd;
    gGoToPageWnd = nullptr;
    delete tmp;
}

void GoToPageWnd::ScheduleDelete() {
    if (gGoToPageWnd != this) {
        return;
    }
    auto fn = MkFunc0Void(SafeDeleteGoToPageDialog);
    uitask::Post(fn, "SafeDeleteGoToPageDialog");
}

void GoToPageWnd::UpdateTheme() {
    Color colBg = ThemeWindowControlBackgroundColor();
    Color colTxt = ThemeWindowTextColor();
    SetColors(colTxt, colBg);
    if (label) {
        label->textColor = colTxt;
    }
    if (labelOf) {
        labelOf->textColor = colTxt;
    }
    if (editPage) {
        editPage->SetColors(colTxt, colBg);
    }
    if (btnCancel) {
        StyleThemedButton(btnCancel, false);
    }
    if (btnGo) {
        StyleThemedButton(btnGo, true);
    }
    if (UseDarkModeLib()) {
        DarkMode::setDarkWndSafe(hwnd);
    }
    RedrawWindow(hwnd, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
}

void GoToPageWnd::SetTarget(MainWindow* mainWin) {
    win = mainWin;
    pageCount = 0;
    onlyNumeric = true;
    Str pageLabel;
    if (IsMainWindowValid(win) && win->IsDocLoaded() && win->ctrl) {
        pageCount = win->ctrl->PageCount();
        onlyNumeric = !win->ctrl->HasPageLabels();
        pageLabel = win->ctrl->GetPageLabeTemp(win->ctrl->CurrentPageNo());
    }
    if (labelOf) {
        labelOf->SetText(fmt(_TRA("(of %d)").s, pageCount));
    }
    if (editPage) {
        editPage->SetNumbersOnly(onlyNumeric);
        editPage->SetText(pageLabel);
        editPage->SelectAll();
    }
}

void GoToPageWnd::OnCancel() {
    ScheduleDelete();
}

void GoToPageWnd::OnOk() {
    if (!IsMainWindowValid(win) || !win->IsDocLoaded() || !win->ctrl) {
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

void GoToPageWnd::OnKeyDown(KeyEvent* ev) {
    if (ev->vkey == VK_ESCAPE) {
        OnCancel();
        ev->didHandle = true;
        return;
    }
    if (ev->vkey == VK_RETURN) {
        if (btnCancel && vroot && vroot->focused == btnCancel) {
            OnCancel();
        } else {
            OnOk();
        }
        ev->didHandle = true;
    }
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

static void CancelClicked(GoToPageWnd* wnd, VirtMouseEvent*) {
    wnd->OnCancel();
}

static void GoClicked(GoToPageWnd* wnd, VirtMouseEvent*) {
    wnd->OnOk();
}

bool GoToPageWnd::Create(MainWindow* mainWin) {
    win = mainWin;
    Str pageLabel;
    if (IsMainWindowValid(win) && win->IsDocLoaded() && win->ctrl) {
        pageCount = win->ctrl->PageCount();
        onlyNumeric = !win->ctrl->HasPageLabels();
        pageLabel = win->ctrl->GetPageLabeTemp(win->ctrl->CurrentPageNo());
    }

    {
        CreateCustomArgs args;
        args.title = _TRA("Go to page");
        args.visible = false;
        args.style = WS_POPUPWINDOW | WS_CAPTION;
        args.font = font;
        args.icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(GetAppIconID()));
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }
    platformFont = GetPlatformFont(font);
    bool isRtl = IsUIRtl();

    auto* vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    {
        auto* hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainStart;
        hbox->alignCross = CrossAxisAlign::CrossCenter;

        auto* lab = NewVirtText({
            .s = _TRA("&Go to page:"),
            .font = platformFont,
            .isRtl = isRtl,
        });
        label = lab;
        hbox->AddChild(lab);

        Edit::CreateArgs args;
        args.parent = hwnd;
        args.font = font;
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
            .font = platformFont,
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
        auto pad = Insets{4, 8, 4, 8};

        btnCancel = NewThemedButton(hwnd, _TRA("Cancel"), platformFont, false);
        btnCancel->onClick = MkFunc1(CancelClicked, this);
        hbox->AddChild(new Padding(btnCancel, pad));
        btnGo = NewThemedButton(hwnd, _TRA("Go to page"), platformFont, true);
        btnGo->onClick = MkFunc1(GoClicked, this);
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
    if (editPage) {
        editPage->SelectAll();
        HwndSetFocus(editPage->hwnd);
    }
    return true;
}

void ShowGoToPageDialog(MainWindow* win) {
    if (!IsMainWindowValid(win) || !win->IsDocLoaded()) {
        return;
    }
    if (gGoToPageWnd) {
        gGoToPageWnd->SetTarget(win);
        if (gGoToPageWnd->hwnd) {
            gGoToPageWnd->DoLayout(HwndClientRect(gGoToPageWnd->hwnd).Size());
            HwndInvalidate(gGoToPageWnd->hwnd);
        }
        HwndSetFocus(gGoToPageWnd->hwnd);
        if (gGoToPageWnd->editPage) {
            HwndSetFocus(gGoToPageWnd->editPage->hwnd);
        }
        return;
    }
    auto* wnd = new GoToPageWnd();
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnDestroy);
    wnd->onKeyDown = MkMethod1<GoToPageWnd, KeyEvent*, &GoToPageWnd::OnKeyDown>(wnd);
    wnd->font = GetAppFont();
    bool ok = wnd->Create(win);
    if (!ok) {
        delete wnd;
        return;
    }
    gGoToPageWnd = wnd;
}
