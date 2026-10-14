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
#include "Favorites.h"
#include "Translations.h"
#include "DarkMode.h"
#include "SumatraDialogs.h"
#include "shared/PdfToolsCommon.h"
#include "AppHelpersCommon.h"

//   Name for page 5 (optional):
//   Chapter 2
//   ______________________________________________ underline
//   ---------------------------------------------- hairline
//                          [Cancel] [Add Favorite]
constexpr int kAddFavoriteDlgDx = 400;
constexpr int kFavFieldPadTop = 10;
constexpr int kFavFieldPadBottom = 16;
struct AddFavoriteWnd : WindowBase {
    ~AddFavoriteWnd() override;

    MainWindow* win = nullptr;
    int pageNo = 0;
    Str pageLabel;
    Str filePath;
    VirtText* label = nullptr;
    Edit* editName = nullptr;
    VirtButton* btnCancel = nullptr;
    VirtButton* btnOk = nullptr;

    bool Create(MainWindow* win, Str path, int page, Str labelIn, Str name);
    void SetTarget(MainWindow* win, Str path, int page, Str labelIn, Str name);

    void OnCancel(VirtMouseEvent* ev = nullptr);
    void OnOk(VirtMouseEvent* ev = nullptr);
};

static AddFavoriteWnd* gAddFavoriteWnd = nullptr;

AddFavoriteWnd::~AddFavoriteWnd() {
    str::Free(pageLabel);
    str::Free(filePath);
}

static void ClearAddFavoriteWnd() {
    gAddFavoriteWnd = nullptr;
}

void AddFavoriteWnd::SetTarget(MainWindow* win, Str path, int page, Str labelIn, Str name) {
    this->win = win;
    pageNo = page;
    str::ReplaceWithCopy(&pageLabel, labelIn);
    str::ReplaceWithCopy(&filePath, path);
    if (label) {
        label->SetText(FavoritePromptTemp(pageLabel));
    }
    if (editName) {
        editName->SetText(name);
        EditSelectAll(editName);
    }
}

void AddFavoriteWnd::OnCancel(VirtMouseEvent*) {
    ScheduleDelete();
}

void AddFavoriteWnd::OnOk(VirtMouseEvent*) {
    TempStr name = editName ? editName->GetTextTemp() : Str{};
    str::TrimWSInPlace(name, str::TrimOpt::Both);
    if (len(name) == 0) {
        name = {};
    }
    ApplyAddFavorite(win, filePath, pageNo, pageLabel, name);
    ScheduleDelete();
}

static void OnClose(WindowBase::CloseEvent* /*ev*/) {
    if (gAddFavoriteWnd) {
        gAddFavoriteWnd->OnCancel();
    }
}

static void OnDestroy(WindowBase::DestroyEvent* /*ev*/) {
    if (gAddFavoriteWnd) {
        gAddFavoriteWnd->ScheduleDelete();
    }
}

bool AddFavoriteWnd::Create(MainWindow* win, Str path, int page, Str labelIn, Str name) {
    SetTarget(win, path, page, labelIn, name);

    Color colTxt = ThemeWindowTextColor();
    Color colBg = ThemeWindowControlBackgroundColor();
    Color colMuted = ThemeWindowTextDisabledColor();
    PlatformFont* fontText = DlgUiFont(kToolDlgTextFontPx);
    SetFont(fontText);
    bool isRtl = IsUIRtl();
    {
        CreateCustomArgs args;
        args.owner = win ? win->hwndFrame : nullptr;
        args.title = Tr("Add Favorite");
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
    label = NewDlgCaption(FavoritePromptTemp(pageLabel), fontText, colMuted);
    vbox->AddChild(label);

    // the name, in the larger font
    Edit::CreateArgs args;
    args.parent = hwnd;
    args.font = DlgUiFont(kToolDlgQueryFontPx);
    args.withBorder = false;
    args.selectAllOnFocus = true;
    args.isRtl = isRtl;
    args.text = name;
    editName = new Edit();
    editName->SetColors(colTxt, colBg);
    editName->Create(args);
    vbox->AddChild(new Padding(NewDlgUnderlined(editName),
                               DpiScaledInsets(kFavFieldPadTop, kToolDlgPadDx, kFavFieldPadBottom, kToolDlgPadDx)));

    btnCancel = NewDlgButton(hwnd, Tr("Cancel"), fontText, DlgButtonStyle::Outline, false);
    btnCancel->onClick = MkMethod1<AddFavoriteWnd, VirtMouseEvent*, &AddFavoriteWnd::OnCancel>(this);
    btnOk = NewDlgButton(hwnd, Tr("Add Favorite"), fontText, DlgButtonStyle::Filled, true);
    btnOk->onClick = MkMethod1<AddFavoriteWnd, VirtMouseEvent*, &AddFavoriteWnd::OnOk>(this);
    vbox->AddChild(NewDlgFooter(btnCancel, btnOk));
    layout = vbox;

    LayoutAndSizeToContent(layout, DpiScale(kAddFavoriteDlgDx), 0, hwnd);
    DoLayout(HwndClientRect(hwnd).Size());
    HwndCenterDialog(hwnd, win ? win->hwndFrame : nullptr);
    UpdateTheme();

    SetIsVisible(true);
    EditSelectAll(editName);
    EditSetFocus(editName);
    return true;
}

void ShowAddFavoriteDialog(MainWindow* win, Str filePath, int pageNo, Str pageLabel, Str name) {
    if (gAddFavoriteWnd) {
        gAddFavoriteWnd->SetTarget(win, filePath, pageNo, pageLabel, name);
        if (gAddFavoriteWnd->hwnd) {
            gAddFavoriteWnd->DoLayout(HwndClientRect(gAddFavoriteWnd->hwnd).Size());
            HwndInvalidate(gAddFavoriteWnd->hwnd);
        }
        HwndSetFocus(gAddFavoriteWnd->hwnd);
        EditSetFocus(gAddFavoriteWnd->editName);
        return;
    }
    auto* wnd = new AddFavoriteWnd();
    wnd->closeOnEsc = true;
    wnd->onBeforeDelete = MkFunc0Void(ClearAddFavoriteWnd);
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnDestroy);
    wnd->SetFont(GetAppFont());
    bool ok = wnd->Create(win, filePath, pageNo, pageLabel, name);
    if (!ok) {
        delete wnd;
        return;
    }
    gAddFavoriteWnd = wnd;
    RunModalWindow(wnd->hwnd, win ? win->hwndFrame : nullptr);
}
