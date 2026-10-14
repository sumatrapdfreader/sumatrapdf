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
#include "Translations.h"
#include "DarkMode.h"
#include "SumatraDialogs.h"
#include "shared/PdfToolsCommon.h"
#include "AppHelpersCommon.h"

//   Automatic
//   Always visible                the modes, the current one selected
//   ---------------------------------------------- hairline
//                                    [Cancel] [OK]
constexpr int kScrollbarDlgDx = 280;
struct ChangeScrollbarWnd : WindowBase {
    ~ChangeScrollbarWnd() override = default;

    MainWindow* win = nullptr;
    VirtListBox* listBox = nullptr;
    ListBoxModelStrings* model = nullptr; // owned by listBox
    VirtButton* btnCancel = nullptr;
    VirtButton* btnOk = nullptr;

    bool Create(MainWindow* win);

    void OnCancel(VirtMouseEvent* ev = nullptr);
    void OnOk(VirtMouseEvent* ev = nullptr);
    void OnListDoubleClick();
};

static ChangeScrollbarWnd* gChangeScrollbarWnd = nullptr;

static void ClearChangeScrollbarWnd() {
    gChangeScrollbarWnd = nullptr;
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
        str::ReplaceWithCopy(&gSettings->scrollbars, val);
        UpdateFixedPageScrollbarsVisibility();
        ScheduleSaveSettings();
    }
    ScheduleDelete();
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
    Color colTxt = ThemeWindowTextColor();
    Color colBg = ThemeWindowControlBackgroundColor();
    PlatformFont* fontText = DlgUiFont(kToolDlgTextFontPx);
    SetFont(fontText);
    {
        CreateCustomArgs args;
        args.owner = win ? win->hwndFrame : nullptr;
        args.title = Tr("Change Scrollbar");
        args.visible = false;
        args.style = WS_POPUPWINDOW | WS_CAPTION;
        args.font = fontText;
        args.isRtl = IsUIRtl();
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

    listBox = NewDlgList(GetDpi(), fontText);
    model = new ListBoxModelStrings();
    for (int i = 0; i <= kScrollbarHidden; i++) {
        model->strings.Append(ScrollbarModeDisplayName(i));
    }
    listBox->idealSizeLines = len(model->strings);
    listBox->SetModel(model);
    int curr = ScrollbarModeFromPrefs();
    if (curr >= 0 && curr <= kScrollbarHidden) {
        listBox->SetCurrentSelection(curr);
    }
    listBox->onDoubleClick = MkMethod0<ChangeScrollbarWnd, &ChangeScrollbarWnd::OnListDoubleClick>(this);
    vbox->AddChild(NewDlgListBox(listBox));

    btnCancel = NewDlgButton(hwnd, Tr("Cancel"), fontText, DlgButtonStyle::Outline, false);
    btnCancel->onClick = MkMethod1<ChangeScrollbarWnd, VirtMouseEvent*, &ChangeScrollbarWnd::OnCancel>(this);
    btnOk = NewDlgButton(hwnd, Tr("OK"), fontText, DlgButtonStyle::Filled, true);
    btnOk->onClick = MkMethod1<ChangeScrollbarWnd, VirtMouseEvent*, &ChangeScrollbarWnd::OnOk>(this);
    vbox->AddChild(NewDlgFooter(btnCancel, btnOk));
    layout = vbox;

    LayoutAndSizeToContent(layout, DpiScale(kScrollbarDlgDx), 0, hwnd);
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
    wnd->closeOnEsc = true;
    wnd->onBeforeDelete = MkFunc0Void(ClearChangeScrollbarWnd);
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnDestroy);
    wnd->SetFont(GetAppFont());
    bool ok = wnd->Create(win);
    if (!ok) {
        delete wnd;
        return;
    }
    gChangeScrollbarWnd = wnd;
    RunModalWindow(wnd->hwnd, win ? win->hwndFrame : nullptr);
}
