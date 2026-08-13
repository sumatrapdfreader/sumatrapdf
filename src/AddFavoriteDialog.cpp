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
#include "Favorites.h"
#include "Translations.h"
#include "DarkModeSubclass.h"
#include "AddFavoriteDialog.h"

// Label and buttons are VirtCtrl; the name field is a real HWND Edit.
// Same WindowBase layout pattern as Change Theme / Change Language.
struct AddFavoriteWnd : WindowBase {
    ~AddFavoriteWnd() override;

    HFONT font = nullptr;
    PlatformFont* platformFont = nullptr;
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
    void OnKeyDown(KeyEvent* ev);

    void UpdateTheme();
    void OnCancel();
    void OnOk();
    void ScheduleDelete();
};

static AddFavoriteWnd* gAddFavoriteWnd = nullptr;

AddFavoriteWnd::~AddFavoriteWnd() {
    str::Free(pageLabel);
    str::Free(filePath);
}

void SafeDeleteAddFavoriteDialog() {
    if (!gAddFavoriteWnd) {
        return;
    }
    auto* tmp = gAddFavoriteWnd;
    gAddFavoriteWnd = nullptr;
    delete tmp;
}

void AddFavoriteWnd::ScheduleDelete() {
    if (gAddFavoriteWnd != this) {
        return;
    }
    auto fn = MkFunc0Void(SafeDeleteAddFavoriteDialog);
    uitask::Post(fn, "SafeDeleteAddFavoriteDialog");
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

void AddFavoriteWnd::UpdateTheme() {
    Color colBg = ThemeWindowControlBackgroundColor();
    Color colTxt = ThemeWindowTextColor();
    SetColors(colTxt, colBg);
    if (label) {
        label->textColor = colTxt;
    }
    if (editName) {
        editName->SetColors(colTxt, colBg);
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

void AddFavoriteWnd::OnCancel() {
    ScheduleDelete();
}

void AddFavoriteWnd::OnOk() {
    TempStr name = editName ? editName->GetTextTemp() : Str{};
    str::TrimWSInPlace(name, str::TrimOpt::Both);
    if (len(name) == 0) {
        name = {};
    }
    ApplyAddFavorite(win, filePath, pageNo, pageLabel, name);
    ScheduleDelete();
}

void AddFavoriteWnd::OnKeyDown(KeyEvent* ev) {
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
    if (gAddFavoriteWnd) {
        gAddFavoriteWnd->OnCancel();
    }
}

static void OnDestroy(WindowBase::DestroyEvent* /*ev*/) {
    if (gAddFavoriteWnd) {
        gAddFavoriteWnd->ScheduleDelete();
    }
}

static void CancelClicked(AddFavoriteWnd* wnd, VirtMouseEvent*) {
    wnd->OnCancel();
}

static void OkClicked(AddFavoriteWnd* wnd, VirtMouseEvent*) {
    wnd->OnOk();
}

bool AddFavoriteWnd::Create(MainWindow* mainWin, Str path, int page, Str labelIn, Str name) {
    SetTarget(mainWin, path, page, labelIn, name);

    {
        CreateCustomArgs args;
        args.title = _TRA("Add Favorite");
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
        auto* c = NewVirtText({
            .s = FavoritePromptTemp(pageLabel),
            .font = platformFont,
            .isRtl = isRtl,
            .padding = DpiScaledInsets(0, 0, 4, 0),
        });
        label = c;
        vbox->AddChild(c);
    }

    {
        Edit::CreateArgs args;
        args.parent = hwnd;
        args.font = font;
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
        auto pad = Insets{4, 8, 4, 8};

        btnCancel = NewThemedButton(hwnd, _TRA("Cancel"), platformFont, false);
        btnCancel->onClick = MkFunc1(CancelClicked, this);
        hbox->AddChild(new Padding(btnCancel, pad));
        btnOk = NewThemedButton(hwnd, _TRA("OK"), platformFont, true);
        btnOk->onClick = MkFunc1(OkClicked, this);
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
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnDestroy);
    wnd->onKeyDown = MkMethod1<AddFavoriteWnd, KeyEvent*, &AddFavoriteWnd::OnKeyDown>(wnd);
    wnd->font = GetAppFont();
    bool ok = wnd->Create(win, filePath, pageNo, pageLabel, name);
    if (!ok) {
        delete wnd;
        return;
    }
    gAddFavoriteWnd = wnd;
}
