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
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "DarkMode.h"
#include "SumatraDialogs.h"
#include "shared/PdfToolsCommon.h"
#include "AppHelpersCommon.h"
#include "OpenFromUrlCommon.h"

//   URL of a document to download and open:
//   https://example.com/doc.pdf
//   ______________________________________________
//   Save to  C:\Users\me\Downloads_______  [Browse...]
//                                [Cancel] [Download]
constexpr int kOpenFromUrlDlgDx = 520;
constexpr int kUrlFieldPadTop = 10;
constexpr int kUrlFieldPadBottom = 12;

struct OpenFromUrlWnd : WindowBase {
    MainWindow* win = nullptr;
    Edit* editUrl = nullptr;
    Edit* editDir = nullptr;
    VirtButton* btnBrowse = nullptr;
    VirtButton* btnCancel = nullptr;
    VirtButton* btnOk = nullptr;

    bool Create(MainWindow* win);
    void OnBrowse(VirtMouseEvent* ev = nullptr);
    void OnCancel(VirtMouseEvent* ev = nullptr);
    void OnOk(VirtMouseEvent* ev = nullptr);
};

static OpenFromUrlWnd* gOpenFromUrlWnd = nullptr;

static void ClearOpenFromUrlWnd() {
    gOpenFromUrlWnd = nullptr;
}

void OpenFromUrlWnd::OnBrowse(VirtMouseEvent*) {
    TempStr dir = AskForFolderTemp(hwnd, Tr("Save downloaded files to"), editDir->GetTextTemp());
    if (len(dir) > 0) {
        editDir->SetText(dir);
    }
}

void OpenFromUrlWnd::OnCancel(VirtMouseEvent*) {
    ScheduleDelete();
}

void OpenFromUrlWnd::OnOk(VirtMouseEvent*) {
    if (StartOpenFromUrl(win, editUrl->GetTextTemp(), editDir->GetTextTemp())) {
        ScheduleDelete();
    }
}

static void OnClose(WindowBase::CloseEvent* /*ev*/) {
    if (gOpenFromUrlWnd) {
        gOpenFromUrlWnd->OnCancel();
    }
}

static void OnDestroy(WindowBase::DestroyEvent* /*ev*/) {
    if (gOpenFromUrlWnd) {
        gOpenFromUrlWnd->ScheduleDelete();
    }
}

bool OpenFromUrlWnd::Create(MainWindow* win) {
    this->win = win;
    Color colTxt = ThemeWindowTextColor();
    Color colBg = ThemeWindowControlBackgroundColor();
    Color colMuted = ThemeWindowTextDisabledColor();
    PlatformFont* fontText = DlgUiFont(kToolDlgTextFontPx);
    SetFont(fontText);
    bool isRtl = IsUIRtl();
    {
        CreateCustomArgs args;
        args.owner = win ? win->hwndFrame : nullptr;
        args.title = Tr("Open from URL");
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
    vbox->AddChild(NewDlgCaption(Tr("URL of a document to download and open:"), fontText, colMuted));

    // the URL, in the larger font
    editUrl = NewDlgEdit(hwnd, DlgUiFont(kToolDlgQueryFontPx), {}, 0);
    vbox->AddChild(new Padding(NewDlgUnderlined(editUrl),
                               DpiScaledInsets(kUrlFieldPadTop, kToolDlgPadDx, kUrlFieldPadBottom, kToolDlgPadDx)));

    // the folder: the remembered one, else the Downloads folder
    HBox* row = NewDlgFieldRow();
    row->AddChild(NewDlgLabel(Tr("Save to"), fontText, colMuted, kToolDlgRowDy));
    editDir = NewDlgEdit(hwnd, fontText, DownloadDirTemp(), 0);
    row->AddChild(NewDlgUnderlined(editDir), 1);
    btnBrowse = NewDlgButton(hwnd, Tr("Browse..."), fontText, DlgButtonStyle::Ghost, false);
    btnBrowse->onClick = MkMethod1<OpenFromUrlWnd, VirtMouseEvent*, &OpenFromUrlWnd::OnBrowse>(this);
    row->AddChild(btnBrowse);
    vbox->AddChild(NewDlgSection(row));

    btnCancel = NewDlgButton(hwnd, Tr("Cancel"), fontText, DlgButtonStyle::Outline, false);
    btnCancel->onClick = MkMethod1<OpenFromUrlWnd, VirtMouseEvent*, &OpenFromUrlWnd::OnCancel>(this);
    btnOk = NewDlgButton(hwnd, Tr("Download"), fontText, DlgButtonStyle::Filled, true);
    btnOk->onClick = MkMethod1<OpenFromUrlWnd, VirtMouseEvent*, &OpenFromUrlWnd::OnOk>(this);
    vbox->AddChild(NewDlgFooter(btnCancel, btnOk));
    layout = vbox;

    LayoutAndSizeToContent(layout, DpiScale(kOpenFromUrlDlgDx), 0, hwnd);
    DoLayout(HwndClientRect(hwnd).Size());
    HwndCenterDialog(hwnd, win ? win->hwndFrame : nullptr);
    UpdateTheme();

    SetIsVisible(true);
    EditSetFocus(editUrl);
    return true;
}

void ShowOpenFromUrlDialog(MainWindow* win) {
    if (gOpenFromUrlWnd) {
        HwndSetFocus(gOpenFromUrlWnd->hwnd);
        EditSetFocus(gOpenFromUrlWnd->editUrl);
        return;
    }
    auto* wnd = new OpenFromUrlWnd();
    wnd->closeOnEsc = true;
    wnd->onBeforeDelete = MkFunc0Void(ClearOpenFromUrlWnd);
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnDestroy);
    wnd->SetFont(GetAppFont());
    if (!wnd->Create(win)) {
        delete wnd;
        return;
    }
    gOpenFromUrlWnd = wnd;
    RunModalWindow(wnd->hwnd, win ? win->hwndFrame : nullptr);
}
