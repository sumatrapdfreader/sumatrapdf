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
#include "AppTools.h"
#include "Translations.h"
#include "DarkModeSubclass.h"
#include "InverseSearchDialog.h"

// Label, Help and OK/Cancel are VirtCtrl; the command line is an editable DropDown.
// Same WindowBase layout as Change Theme / Custom Zoom.
struct InverseSearchWnd : WindowBase {
    ~InverseSearchWnd() override = default;

    MainWindow* win = nullptr;
    VirtText* label = nullptr;
    DropDown* dropDown = nullptr;
    VirtButton* btnHelp = nullptr;
    VirtButton* btnCancel = nullptr;
    VirtButton* btnOk = nullptr;

    bool Create(MainWindow* win);
    void FillCommands();
    void OnKeyDown(KeyEvent* ev);

    void UpdateTheme();
    void OnCancel(VirtMouseEvent* ev = nullptr);
    void OnOk(VirtMouseEvent* ev = nullptr);
    void OnHelp(VirtMouseEvent* ev = nullptr);
    void ScheduleDelete();
};

static InverseSearchWnd* gInverseSearchWnd = nullptr;

void SafeDeleteInverseSearchDialog() {
    if (!gInverseSearchWnd) {
        return;
    }
    auto* tmp = gInverseSearchWnd;
    gInverseSearchWnd = nullptr;
    delete tmp;
}

void InverseSearchWnd::ScheduleDelete() {
    if (gInverseSearchWnd != this) {
        return;
    }
    auto fn = MkFunc0Void(SafeDeleteInverseSearchDialog);
    uitask::Post(fn, "SafeDeleteInverseSearchDialog");
}

void InverseSearchWnd::UpdateTheme() {
    Color colBg = ThemeWindowControlBackgroundColor();
    Color colTxt = ThemeWindowTextColor();
    SetColors(colTxt, colBg);
    if (label) {
        label->textColor = colTxt;
    }
    if (dropDown) {
        dropDown->SetColors(colTxt, colBg);
    }
    if (btnHelp) {
        StyleThemedButton(btnHelp, false);
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

void InverseSearchWnd::FillCommands() {
    if (!dropDown) {
        return;
    }
    StrVec items;
    Str cmdLine = gGlobalPrefs ? gGlobalPrefs->inverseSearchCmdLine : Str{};
    CollectInverseSearchCommands(items, cmdLine);
    if (!cmdLine && len(items) > 0) {
        cmdLine = items[0];
    }
    dropDown->SetItems(items);
    if (!cmdLine) {
        return;
    }
    int idx = items.Find(cmdLine);
    if (idx >= 0) {
        dropDown->SetCurrentSelection(idx);
    } else {
        dropDown->SetText(cmdLine);
    }
}

void InverseSearchWnd::OnCancel(VirtMouseEvent*) {
    ScheduleDelete();
}

void InverseSearchWnd::OnOk(VirtMouseEvent*) {
    TempStr tmp = dropDown ? dropDown->GetTextTemp() : Str{};
    str::ReplaceWithCopy(&gGlobalPrefs->inverseSearchCmdLine, tmp);
    gGlobalPrefs->enableTeXEnhancements = true;
    SaveSettings();
    ScheduleDelete();
}

void InverseSearchWnd::OnHelp(VirtMouseEvent*) {
    LaunchDocumentation("/LaTeX-integration");
}

void InverseSearchWnd::OnKeyDown(KeyEvent* ev) {
    if (ev->vkey == VK_ESCAPE) {
        OnCancel();
        ev->didHandle = true;
        return;
    }
    if (ev->vkey == VK_RETURN) {
        HWND hwndDrop = dropDown ? dropDown->hwnd : nullptr;
        if (hwndDrop && SendMessageW(hwndDrop, CB_GETDROPPEDSTATE, 0, 0)) {
            return;
        }
        if (btnCancel && vroot && vroot->focused == btnCancel) {
            OnCancel();
        } else if (btnHelp && vroot && vroot->focused == btnHelp) {
            OnHelp();
        } else {
            OnOk();
        }
        ev->didHandle = true;
    }
}

static void OnClose(WindowBase::CloseEvent* /*ev*/) {
    if (gInverseSearchWnd) {
        gInverseSearchWnd->OnCancel();
    }
}

static void OnDestroy(WindowBase::DestroyEvent* /*ev*/) {
    if (gInverseSearchWnd) {
        gInverseSearchWnd->ScheduleDelete();
    }
}

bool InverseSearchWnd::Create(MainWindow* mainWin) {
    win = mainWin;

    {
        CreateCustomArgs args;
        args.title = _TRA("Set inverse search command line");
        args.visible = false;
        args.style = WS_POPUPWINDOW | WS_CAPTION;
        args.font = GetHFont();
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

    {
        auto* c = NewVirtText({
            .s = _TRA("Enter the command line to invoke when you double-click on the PDF document:"),
            .font = font,
            .isRtl = isRtl,
            .padding = DpiScaledInsets(0, 0, 4, 0),
        });
        label = c;
        vbox->AddChild(c);
    }

    {
        DropDown::CreateArgs args;
        args.parent = hwnd;
        args.font = GetHFont();
        args.isRtl = isRtl;
        args.isEditable = true;
        auto* c = new DropDown();
        c->Create(args);
        dropDown = c;
        vbox->AddChild(c);
        FillCommands();
    }

    {
        auto* row = new HBox();
        row->alignMain = MainAxisAlign::SpaceBetween;
        row->alignCross = CrossAxisAlign::CrossCenter;
        auto pad = Insets{4, 8, 4, 8};

        btnHelp = NewThemedButton(hwnd, _TRA("Help"), font, false);
        btnHelp->onClick = MkMethod1<InverseSearchWnd, VirtMouseEvent*, &InverseSearchWnd::OnHelp>(this);
        row->AddChild(new Padding(btnHelp, pad));

        auto* right = new HBox();
        right->alignMain = MainAxisAlign::MainEnd;
        right->alignCross = CrossAxisAlign::CrossCenter;
        btnCancel = NewThemedButton(hwnd, _TRA("Cancel"), font, false);
        btnCancel->onClick = MkMethod1<InverseSearchWnd, VirtMouseEvent*, &InverseSearchWnd::OnCancel>(this);
        right->AddChild(new Padding(btnCancel, pad));
        btnOk = NewThemedButton(hwnd, _TRA("OK"), font, true);
        btnOk->onClick = MkMethod1<InverseSearchWnd, VirtMouseEvent*, &InverseSearchWnd::OnOk>(this);
        right->AddChild(new Padding(btnOk, pad));
        row->AddChild(right);
        vbox->AddChild(row);
    }

    auto* padding = new Padding(vbox, DpiScaledInsets(4, 8));
    layout = padding;

    int dx = DpiScale(520);
    LayoutAndSizeToContent(layout, dx, 0, hwnd);
    DoLayout(HwndClientRect(hwnd).Size());
    HwndCenterDialog(hwnd, win ? win->hwndFrame : nullptr);
    UpdateTheme();

    SetIsVisible(true);
    if (dropDown) {
        HwndSetFocus(dropDown->hwnd);
    }
    return true;
}

void ShowInverseSearchDialog(MainWindow* win) {
    if (!CanAccessDisk() || !HasPermission(Perm::SavePreferences)) {
        return;
    }
    if (gInverseSearchWnd) {
        gInverseSearchWnd->FillCommands();
        HwndSetFocus(gInverseSearchWnd->hwnd);
        if (gInverseSearchWnd->dropDown) {
            HwndSetFocus(gInverseSearchWnd->dropDown->hwnd);
        }
        return;
    }
    auto* wnd = new InverseSearchWnd();
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnDestroy);
    wnd->onKeyDown = MkMethod1<InverseSearchWnd, KeyEvent*, &InverseSearchWnd::OnKeyDown>(wnd);
    wnd->SetFont(GetPlatformFont(GetAppFont()));
    bool ok = wnd->Create(win);
    if (!ok) {
        delete wnd;
        return;
    }
    gInverseSearchWnd = wnd;
}
