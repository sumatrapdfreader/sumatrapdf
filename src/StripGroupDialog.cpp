/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
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
#include "SumatraPDF.h"
#include "Translations.h"
#include "DarkMode.h"
#include "SumatraDialogs.h"
#include "shared/PdfToolsCommon.h"
#include "StripGroups.h"

//   Name:   Work
//                     [Cancel] [Rename]
constexpr int kRenameDlgDx = 340;
constexpr int kRenameEditChars = 24;

struct RenameStripGroupWnd : WindowBase {
    ~RenameStripGroupWnd() override = default;

    MainWindow* win = nullptr;
    int groupId = 0;
    Edit* editName = nullptr;
    PlatformFont* fontText = nullptr;
    PlatformFont* fontQuery = nullptr;

    bool Create(MainWindow* win, Str name);
    void OnCancel(VirtMouseEvent* ev = nullptr);
    void OnOk(VirtMouseEvent* ev = nullptr);
};

static RenameStripGroupWnd* gRenameWnd = nullptr;

// a replaced dialog is deleted later, when the global points at its successor
static void ClearRenameWnd(RenameStripGroupWnd* wnd) {
    if (gRenameWnd == wnd) {
        gRenameWnd = nullptr;
    }
}

void RenameStripGroupWnd::OnCancel(VirtMouseEvent*) {
    ScheduleDelete();
}

void RenameStripGroupWnd::OnOk(VirtMouseEvent*) {
    if (IsMainWindowValidAndNotClosing(win) && editName) {
        StripGroupSetName(win, groupId, editName->GetTextTemp());
    }
    ScheduleDelete();
}

static void OnClose(WindowBase::CloseEvent*) {
    if (gRenameWnd) {
        gRenameWnd->OnCancel();
    }
}

static void OnDestroy(WindowBase::DestroyEvent*) {
    if (gRenameWnd) {
        gRenameWnd->ScheduleDelete();
    }
}

bool RenameStripGroupWnd::Create(MainWindow* mainWin, Str name) {
    win = mainWin;
    Color colTxt = ThemeWindowTextColor();
    Color colBg = ThemeWindowControlBackgroundColor();
    Color colMuted = ThemeWindowDarkerTextColor();
    fontText = DlgUiFont(kToolDlgTextFontPx);
    fontQuery = DlgUiFont(kToolDlgQueryFontPx);
    SetFont(fontText);
    if (!CreateToolDialog(this, win->hwndFrame, Tr("Rename Group"), fontText, colTxt, colBg)) {
        return false;
    }

    auto* vbox = new VBox();
    vbox->alignCross = CrossAxisAlign::Stretch;
    HBox* row = NewDlgRow();
    row->AddChild(NewDlgLabel(Tr("&Name:"), fontText, colMuted, kToolDlgLineDy, true));
    Edit::CreateArgs args;
    args.parent = hwnd;
    args.font = fontQuery;
    args.withBorder = false;
    args.selectAllOnFocus = true;
    args.isRtl = IsUIRtl();
    args.text = name;
    args.idealWidthChars = kRenameEditChars;
    editName = new Edit();
    editName->SetColors(colTxt, colBg);
    editName->Create(args);
    row->AddChild(editName, 1);
    vbox->AddChild(new Padding(row, DpiScaledInsets(0, kToolDlgPadDx)));

    VirtButton* btnCancel = NewDlgButton(hwnd, Tr("Cancel"), fontText, DlgButtonStyle::Outline, false);
    btnCancel->onClick = MkMethod1<RenameStripGroupWnd, VirtMouseEvent*, &RenameStripGroupWnd::OnCancel>(this);
    VirtButton* btnOk = NewDlgButton(hwnd, Tr("Rename"), fontText, DlgButtonStyle::Filled, true);
    btnOk->onClick = MkMethod1<RenameStripGroupWnd, VirtMouseEvent*, &RenameStripGroupWnd::OnOk>(this);
    vbox->AddChild(NewDlgFooter(btnCancel, btnOk));
    layout = vbox;

    SizeToLayout(this, layout, DpiScale(kRenameDlgDx), 0, win->hwndFrame);
    UpdateTheme();
    SetIsVisible(true);
    EditSelectAll(editName);
    EditSetFocus(editName);
    return true;
}

void ShowRenameStripGroupDialog(MainWindow* win, int groupId) {
    StripGroup* g = StripGroupFind(win, groupId);
    if (!IsMainWindowValidAndNotClosing(win) || !g) {
        return;
    }
    if (gRenameWnd) {
        gRenameWnd->ScheduleDelete();
        gRenameWnd = nullptr;
    }
    auto* wnd = new RenameStripGroupWnd();
    wnd->groupId = groupId;
    wnd->closeOnEsc = true;
    wnd->onBeforeDelete = MkFunc0<RenameStripGroupWnd>(ClearRenameWnd, wnd);
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnDestroy);
    wnd->SetFont(GetAppFont());
    if (!wnd->Create(win, g->name)) {
        delete wnd;
        return;
    }
    gRenameWnd = wnd;
    RunModalWindow(wnd->hwnd, win->hwndFrame);
}
