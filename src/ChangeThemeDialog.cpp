/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"
#include "base/Dpi.h"
#include "base/UITask.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Settings.h"
#include "AppSettings.h"
#include "GlobalPrefs.h"
#include "MainWindow.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "DarkModeSubclass.h"
#include "ChangeThemeDialog.h"

struct ChangeThemeWnd : Wnd {
    ~ChangeThemeWnd() override;

    HFONT font = nullptr;
    MainWindow* win = nullptr;
    ListBox* listBox = nullptr;
    ListBoxModelStrings* model = nullptr; // owned by listBox
    Str startThemePref;                   // prefs theme at open, for Cancel revert

    bool Create(MainWindow* win);
    bool PreTranslateMessage(MSG&) override;

    void OnSelectionChanged();
    void OnCancel();
    void OnChange();
    void ScheduleDelete();
};

static ChangeThemeWnd* gChangeThemeWnd = nullptr;

ChangeThemeWnd::~ChangeThemeWnd() {
    str::Free(startThemePref);
}

void SafeDeleteChangeThemeDialog() {
    if (!gChangeThemeWnd) {
        return;
    }
    auto tmp = gChangeThemeWnd;
    gChangeThemeWnd = nullptr;
    delete tmp;
}

void ChangeThemeWnd::ScheduleDelete() {
    if (gChangeThemeWnd != this) {
        return;
    }
    auto fn = MkFunc0Void(SafeDeleteChangeThemeDialog);
    uitask::Post(fn, "SafeDeleteChangeThemeDialog");
}

static void PositionDialog(HWND hwnd, HWND hwndRelative) {
    Rect rRelative = WindowRect(hwndRelative);
    Rect r = WindowRect(hwnd);
    int x = rRelative.x + (rRelative.dx / 2) - (r.dx / 2);
    int y = rRelative.y + (rRelative.dy / 2) - (r.dy / 2);
    r = {x, y, r.dx, r.dy};
    Rect r2 = ShiftRectToWorkArea(r, hwndRelative, true);
    SetWindowPos(hwnd, nullptr, r2.x, r2.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}

void ChangeThemeWnd::OnSelectionChanged() {
    int idx = listBox->GetCurrentSelection();
    if (idx < 0) {
        return;
    }
    SetThemeByIndex(idx);
    COLORREF colBg = ThemeWindowControlBackgroundColor();
    COLORREF colTxt = ThemeWindowTextColor();
    SetColors(colTxt, colBg);
    listBox->SetColors(colTxt, colBg);
    if (UseDarkModeLib()) {
        DarkMode::setDarkWndSafe(hwnd);
    }
    HwndScheduleRepaint(hwnd);
}

void ChangeThemeWnd::OnCancel() {
    SetTheme(startThemePref);
    ScheduleDelete();
}

void ChangeThemeWnd::OnChange() {
    SaveSettings();
    ScheduleDelete();
}

bool ChangeThemeWnd::PreTranslateMessage(MSG& msg) {
    if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
        OnCancel();
        return true;
    }
    return false;
}

static void OnClose(Wnd::CloseEvent*) {
    if (gChangeThemeWnd) {
        gChangeThemeWnd->OnCancel();
    }
}

static void OnDestroy(Wnd::DestroyEvent*) {
    if (gChangeThemeWnd) {
        gChangeThemeWnd->ScheduleDelete();
    }
}

bool ChangeThemeWnd::Create(MainWindow* mainWin) {
    win = mainWin;
    startThemePref = str::Dup(gGlobalPrefs->theme);

    {
        CreateCustomArgs args;
        args.title = _TRA("Change Theme");
        args.visible = false;
        args.style = WS_POPUPWINDOW | WS_CAPTION;
        args.font = font;
        args.icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(GetAppIconID()));
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }

    auto colBg = ThemeWindowControlBackgroundColor();
    auto colTxt = ThemeWindowTextColor();
    SetColors(colTxt, colBg);
    bool isRtl = IsUIRtl();

    auto vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    {
        ListBox::CreateArgs args;
        args.parent = hwnd;
        args.font = font;
        args.isRtl = isRtl;
        auto c = new ListBox();
        c->idealSizeLines = 16;
        c->Create(args);
        c->SetColors(colTxt, colBg);
        listBox = c;
        model = new ListBoxModelStrings();
        int n = ThemeGetCount();
        for (int i = 0; i < n; i++) {
            model->strings.Append(ThemeGetNameAt(i));
        }
        c->onSelectionChanged = MkMethod0<ChangeThemeWnd, &ChangeThemeWnd::OnSelectionChanged>(this);
        c->SetModel(model);
        int currIdx = ThemeGetCurrentIndex();
        if (currIdx >= 0 && currIdx < n) {
            c->SetCurrentSelection(currIdx);
        }
        vbox->AddChild(c, 1);
    }

    {
        auto hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainEnd;
        hbox->alignCross = CrossAxisAlign::CrossCenter;
        auto pad = Insets{4, 8, 4, 8};

        Button* b =
            CreateButton(hwnd, _TRA("Cancel"), MkMethod0<ChangeThemeWnd, &ChangeThemeWnd::OnCancel>(this), isRtl);
        hbox->AddChild(new Padding(b, pad));
        b = CreateButton(hwnd, _TRA("Change"), MkMethod0<ChangeThemeWnd, &ChangeThemeWnd::OnChange>(this), isRtl);
        hbox->AddChild(new Padding(b, pad));
        vbox->AddChild(hbox);
    }

    auto padding = new Padding(vbox, DpiScaledInsets(hwnd, 4, 8));
    layout = padding;

    int dx = DpiScale(hwnd, 280);
    int dy = DpiScale(hwnd, 360);
    LayoutAndSizeToContent(layout, dx, dy, hwnd);
    PositionDialog(hwnd, win->hwndFrame);

    if (UseDarkModeLib()) {
        DarkMode::setDarkWndSafe(hwnd);
    }

    SetIsVisible(true);
    HwndSetFocus(listBox->hwnd);
    return true;
}

void ShowChangeThemeDialog(MainWindow* win) {
    if (!HasPermission(Perm::SavePreferences)) {
        return;
    }
    if (gChangeThemeWnd) {
        HwndSetFocus(gChangeThemeWnd->hwnd);
        return;
    }
    auto wnd = new ChangeThemeWnd();
    wnd->onClose = MkFunc1Void<Wnd::CloseEvent*>(OnClose);
    wnd->onDestroy = MkFunc1Void<Wnd::DestroyEvent*>(OnDestroy);
    wnd->font = GetAppFont(win->hwndFrame);
    bool ok = wnd->Create(win);
    if (!ok) {
        delete wnd;
        return;
    }
    gChangeThemeWnd = wnd;
}