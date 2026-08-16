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
#include "GlobalPrefs.h"
#include "MainWindow.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "AppTools.h"
#include "Translations.h"
#include "DarkMode_win.h"
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

    void OnCancel(VirtMouseEvent* ev = nullptr);
    void OnOk(VirtMouseEvent* ev = nullptr);
    void OnHelp(VirtMouseEvent* ev = nullptr);
};

static InverseSearchWnd* gInverseSearchWnd = nullptr;

static void ClearInverseSearchWnd() {
    gInverseSearchWnd = nullptr;
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
        args.font = GetFont();
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
        row->gap = font->averageCharWidth;
        auto pad = Insets{4, 0, 4, 0};

        btnHelp = NewThemedButton(hwnd, _TRA("Help"), font, false);
        btnHelp->onClick = MkMethod1<InverseSearchWnd, VirtMouseEvent*, &InverseSearchWnd::OnHelp>(this);
        row->AddChild(new Padding(btnHelp, pad));

        auto* right = new HBox();
        right->alignMain = MainAxisAlign::MainEnd;
        right->alignCross = CrossAxisAlign::CrossCenter;
        right->gap = font->averageCharWidth;
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
    wnd->closeOnEsc = true;
    wnd->onBeforeDelete = MkFunc0Void(ClearInverseSearchWnd);
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnDestroy);
    wnd->SetFont(GetAppFont());
    bool ok = wnd->Create(win);
    if (!ok) {
        delete wnd;
        return;
    }
    gInverseSearchWnd = wnd;
}
