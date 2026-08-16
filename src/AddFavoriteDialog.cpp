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
#include "Favorites.h"
#include "Translations.h"
#include "DarkMode_win.h"
#include "AddFavoriteDialog.h"

// Label and buttons are VirtCtrl; the name field is a real HWND Edit.
// Same WindowBase layout pattern as Change Theme / Change Language.
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

    bool Create(MainWindow* win, Str filePath, int pageNo, Str pageLabel, Str name);
    void SetTarget(MainWindow* win, Str filePath, int pageNo, Str pageLabel, Str name);

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

static TempStr FavoritePromptTemp(Str pageLabel) {
    return fmt(_TRA("Add page %s to favorites with (optional) name:").s, pageLabel);
}

void AddFavoriteWnd::SetTarget(MainWindow* mainWin, Str path, int page, Str labelIn, Str name) {
    win = mainWin;
    pageNo = page;
    str::ReplaceWithCopy(&pageLabel, labelIn);
    str::ReplaceWithCopy(&filePath, path);
    if (label) {
        label->SetText(FavoritePromptTemp(pageLabel));
    }
    if (editName) {
        editName->SetText(name);
        editName->SelectAll();
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

bool AddFavoriteWnd::Create(MainWindow* mainWin, Str path, int page, Str labelIn, Str name) {
    SetTarget(mainWin, path, page, labelIn, name);

    {
        CreateCustomArgs args;
        args.title = _TRA("Add Favorite");
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
            .s = FavoritePromptTemp(pageLabel),
            .font = font,
            .isRtl = isRtl,
            .padding = DpiScaledInsets(0, 0, 4, 0),
        });
        label = c;
        vbox->AddChild(c);
    }

    {
        Edit::CreateArgs args;
        args.parent = hwnd;
        args.font = GetFont();
        args.withBorder = true;
        args.selectAllOnFocus = true;
        args.isRtl = isRtl;
        args.text = name;
        auto* c = new Edit();
        c->Create(args);
        editName = c;
        vbox->AddChild(c);
    }

    {
        auto* hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainEnd;
        hbox->alignCross = CrossAxisAlign::CrossCenter;
        hbox->gap = font->averageCharWidth;
        auto pad = Insets{4, 0, 4, 0};

        btnCancel = NewThemedButton(hwnd, _TRA("Cancel"), font, false);
        btnCancel->onClick = MkMethod1<AddFavoriteWnd, VirtMouseEvent*, &AddFavoriteWnd::OnCancel>(this);
        hbox->AddChild(new Padding(btnCancel, pad));
        btnOk = NewThemedButton(hwnd, _TRA("OK"), font, true);
        btnOk->onClick = MkMethod1<AddFavoriteWnd, VirtMouseEvent*, &AddFavoriteWnd::OnOk>(this);
        hbox->AddChild(new Padding(btnOk, pad));
        vbox->AddChild(hbox);
    }

    auto* padding = new Padding(vbox, DpiScaledInsets(4, 8));
    layout = padding;

    int dx = DpiScale(360);
    LayoutAndSizeToContent(layout, dx, 0, hwnd);
    DoLayout(HwndClientRect(hwnd).Size());
    HwndCenterDialog(hwnd, win ? win->hwndFrame : nullptr);
    UpdateTheme();

    SetIsVisible(true);
    if (editName) {
        editName->SelectAll();
        HwndSetFocus(editName->hwnd);
    }
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
        if (gAddFavoriteWnd->editName) {
            HwndSetFocus(gAddFavoriteWnd->editName->hwnd);
        }
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
}
