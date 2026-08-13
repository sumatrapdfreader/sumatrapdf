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
#include "MainWindow.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "DarkModeSubclass.h"
#include "ChangeScrollbarDialog.h"

// Mode list and buttons are VirtCtrl. Same WindowBase layout as Change Theme.
struct ChangeScrollbarWnd : WindowBase {
    ~ChangeScrollbarWnd() override = default;

    MainWindow* win = nullptr;
    VirtListBox* listBox = nullptr;
    ListBoxModelStrings* model = nullptr; // owned by listBox
    VirtButton* btnCancel = nullptr;
    VirtButton* btnOk = nullptr;

    bool Create(MainWindow* win);
    void OnKeyDown(KeyEvent* ev);

    void UpdateTheme();
    void OnCancel(VirtMouseEvent* ev = nullptr);
    void OnOk(VirtMouseEvent* ev = nullptr);
    void OnListDoubleClick();
    void ScheduleDelete();
};

static ChangeScrollbarWnd* gChangeScrollbarWnd = nullptr;

static Str ScrollbarModeDisplayName(int idx) {
    if (idx == kScrollbarSmart) {
        return _TRA("Smart Overlay");
    }
    if (idx == kScrollbarOverlay) {
        return _TRA("Overlay");
    }
    if (idx == kScrollbarHidden) {
        return _TRA("Hidden");
    }
    return _TRA("Windows");
}

void SafeDeleteChangeScrollbarDialog() {
    if (!gChangeScrollbarWnd) {
        return;
    }
    auto* tmp = gChangeScrollbarWnd;
    gChangeScrollbarWnd = nullptr;
    delete tmp;
}

void ChangeScrollbarWnd::ScheduleDelete() {
    if (gChangeScrollbarWnd != this) {
        return;
    }
    auto fn = MkFunc0Void(SafeDeleteChangeScrollbarDialog);
    uitask::Post(fn, "SafeDeleteChangeScrollbarDialog");
}

void ChangeScrollbarWnd::UpdateTheme() {
    Color colBg = ThemeWindowControlBackgroundColor();
    Color colTxt = ThemeWindowTextColor();
    SetColors(colTxt, colBg);
    if (listBox) {
        listBox->textColor = colTxt;
        listBox->bgColor = colBg;
    }
    if (btnCancel) {
        StyleThemedButton(btnCancel, false);
    }
    if (btnOk) {
        StyleThemedButton(btnOk, true);
    }
    if (UseDarkModeLib()) {
        DarkMode::setDarkWndSafe(hwnd);
    }
    RedrawWindow(hwnd, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
}

void ChangeScrollbarWnd::OnCancel(VirtMouseEvent*) {
    ScheduleDelete();
}

void ChangeScrollbarWnd::OnListDoubleClick() {
    OnOk();
}

void ChangeScrollbarWnd::OnOk(VirtMouseEvent*) {
    int idx = listBox ? listBox->GetCurrentSelection() : -1;
    if (idx >= 0) {
        Str val = SeqStrByIndex(gScrollbarModeNames, idx);
        str::ReplaceWithCopy(&gGlobalPrefs->scrollbars, val);
        UpdateFixedPageScrollbarsVisibility();
        SaveSettings();
    }
    ScheduleDelete();
}

void ChangeScrollbarWnd::OnKeyDown(KeyEvent* ev) {
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
    if (gChangeScrollbarWnd) {
        gChangeScrollbarWnd->OnCancel();
    }
}

static void OnDestroy(WindowBase::DestroyEvent* /*ev*/) {
    if (gChangeScrollbarWnd) {
        gChangeScrollbarWnd->ScheduleDelete();
    }
}

bool ChangeScrollbarWnd::Create(MainWindow* mainWin) {
    win = mainWin;

    {
        CreateCustomArgs args;
        args.title = _TRA("Change Scrollbar");
        args.visible = false;
        args.style = WS_POPUPWINDOW | WS_CAPTION;
        args.font = GetHFont();
        args.icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(GetAppIconID()));
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }

    auto* vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    {
        auto* c = new VirtListBox();
        c->dpi = GetDpi();
        c->font = font;
        c->idealSizeLines = 4;
        listBox = c;
        model = new ListBoxModelStrings();
        for (int i = 0; i <= kScrollbarHidden; i++) {
            model->strings.Append(ScrollbarModeDisplayName(i));
        }
        c->SetModel(model);
        int curr = ScrollbarModeFromPrefs();
        if (curr >= 0 && curr <= kScrollbarHidden) {
            c->SetCurrentSelection(curr);
        }
        c->onDoubleClick = MkMethod0<ChangeScrollbarWnd, &ChangeScrollbarWnd::OnListDoubleClick>(this);
        vbox->AddChild(c);
    }

    {
        auto* hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainEnd;
        hbox->alignCross = CrossAxisAlign::CrossCenter;
        auto pad = Insets{4, 8, 4, 8};

        btnCancel = NewThemedButton(hwnd, _TRA("Cancel"), font, false);
        btnCancel->onClick = MkMethod1<ChangeScrollbarWnd, VirtMouseEvent*, &ChangeScrollbarWnd::OnCancel>(this);
        hbox->AddChild(new Padding(btnCancel, pad));
        btnOk = NewThemedButton(hwnd, _TRA("OK"), font, true);
        btnOk->onClick = MkMethod1<ChangeScrollbarWnd, VirtMouseEvent*, &ChangeScrollbarWnd::OnOk>(this);
        hbox->AddChild(new Padding(btnOk, pad));
        vbox->AddChild(hbox);
    }

    auto* padding = new Padding(vbox, DpiScaledInsets(4, 8));
    layout = padding;

    int dx = DpiScale(260);
    LayoutAndSizeToContent(layout, dx, 0, hwnd);
    DoLayout(HwndClientRect(hwnd).Size());
    HwndCenterDialog(hwnd, win ? win->hwndFrame : nullptr);
    UpdateTheme();

    SetIsVisible(true);
    SetFocusTo(listBox);
    return true;
}

void ShowChangeScrollbarDialog(MainWindow* win) {
    if (!HasPermission(Perm::SavePreferences)) {
        return;
    }
    if (gChangeScrollbarWnd) {
        HwndSetFocus(gChangeScrollbarWnd->hwnd);
        return;
    }
    auto* wnd = new ChangeScrollbarWnd();
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnDestroy);
    wnd->onKeyDown = MkMethod1<ChangeScrollbarWnd, KeyEvent*, &ChangeScrollbarWnd::OnKeyDown>(wnd);
    wnd->SetFont(GetPlatformFont(GetAppFont()));
    bool ok = wnd->Create(win);
    if (!ok) {
        delete wnd;
        return;
    }
    gChangeScrollbarWnd = wnd;
}
