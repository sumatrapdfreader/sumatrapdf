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
#include "MainWindow.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "AppTools.h"
#include "Translations.h"
#include "DarkMode.h"
#include "SumatraDialogs.h"
#include "shared/PdfToolsCommon.h"

//   Enter the command line to invoke when you double-click on the PDF document:
//   [code -g "%f:%l"                          v]
//   ---------------------------------------------- hairline
//   Help                             [Cancel] [OK]
constexpr int kInverseSearchDlgDx = 520;
constexpr int kInverseSearchPadTop = 10;
constexpr int kInverseSearchPadBottom = 16;
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
    Str cmdLine = gSettings ? gSettings->inverseSearchCmdLine : Str{};
    CollectInverseSearchCommands(items, cmdLine);
    if (len(cmdLine) == 0 && len(items) > 0) {
        cmdLine = items[0];
    }
    dropDown->SetItems(items);
    if (len(cmdLine) == 0) {
        return;
    }
    int idx = items.Find(cmdLine);
    if (idx >= 0) {
        CbSetCurrentSelection(dropDown, idx);
    } else {
        dropDown->SetText(cmdLine);
    }
}

void InverseSearchWnd::OnCancel(VirtMouseEvent*) {
    ScheduleDelete();
}

void InverseSearchWnd::OnOk(VirtMouseEvent*) {
    TempStr tmp = dropDown ? dropDown->GetTextTemp() : Str{};
    str::ReplaceWithCopy(&gSettings->inverseSearchCmdLine, tmp);
    gSettings->enableTeXEnhancements = true;
    ScheduleSaveSettings();
    ScheduleDelete();
}

void InverseSearchWnd::OnHelp(VirtMouseEvent*) {
    LaunchDocumentation(StrL("/LaTeX-integration"));
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
    Color colTxt = ThemeWindowTextColor();
    Color colBg = ThemeWindowControlBackgroundColor();
    PlatformFont* fontText = DlgUiFont(kToolDlgTextFontPx);
    SetFont(fontText);
    bool isRtl = IsUIRtl();
    {
        CreateCustomArgs args;
        args.owner = win ? win->hwndFrame : nullptr;
        args.title = Tr("Set inverse search command line");
        args.visible = false;
        args.style = WS_POPUPWINDOW | WS_CAPTION;
        args.font = fontText;
        args.isRtl = isRtl;
        args.bgColor = colBg;
        args.icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(GetAppIconID()));
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }
    SetColors(colTxt, colBg);

    auto* vbox = new VBox();
    vbox->alignCross = CrossAxisAlign::Stretch;
    label = NewDlgCaption(Tr("Enter the command line to invoke when you double-click on the PDF document:"), fontText,
                          ThemeWindowTextDisabledColor());
    vbox->AddChild(label);

    DropDown::CreateArgs args;
    args.parent = hwnd;
    args.font = fontText;
    args.isRtl = isRtl;
    args.isEditable = true;
    dropDown = new DropDown();
    dropDown->Create(args);
    Insets pad = DpiScaledInsets(kInverseSearchPadTop, kToolDlgPadDx, kInverseSearchPadBottom, kToolDlgPadDx);
    vbox->AddChild(new Padding(dropDown, pad));
    FillCommands();

    btnHelp = NewDlgButton(hwnd, Tr("Help"), fontText, DlgButtonStyle::Ghost, false);
    btnHelp->onClick = MkMethod1<InverseSearchWnd, VirtMouseEvent*, &InverseSearchWnd::OnHelp>(this);
    btnCancel = NewDlgButton(hwnd, Tr("Cancel"), fontText, DlgButtonStyle::Outline, false);
    btnCancel->onClick = MkMethod1<InverseSearchWnd, VirtMouseEvent*, &InverseSearchWnd::OnCancel>(this);
    btnOk = NewDlgButton(hwnd, Tr("OK"), fontText, DlgButtonStyle::Filled, true);
    btnOk->onClick = MkMethod1<InverseSearchWnd, VirtMouseEvent*, &InverseSearchWnd::OnOk>(this);
    vbox->AddChild(NewDlgFooter(btnCancel, btnOk, btnHelp));
    layout = vbox;

    LayoutAndSizeToContent(layout, DpiScale(kInverseSearchDlgDx), 0, hwnd);
    DoLayout(HwndClientRect(hwnd).Size());
    HwndCenterDialog(hwnd, win ? win->hwndFrame : nullptr);
    UpdateTheme();

    SetIsVisible(true);
    HwndSetFocus(dropDown->hwnd);
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
    RunModalWindow(wnd->hwnd, win ? win->hwndFrame : nullptr);
}
