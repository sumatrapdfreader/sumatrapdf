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
#include "PdfDarkMode.h"
#include "ChangeThemeDialog.h"

struct ChangeThemeWnd : Wnd {
    ~ChangeThemeWnd() override;

    HFONT font = nullptr;
    MainWindow* win = nullptr;
    bool documentColorsFollowThemeOnly = false;
    ListBox* listBox = nullptr;
    ListBoxModelStrings* model = nullptr; // owned by listBox
    DropDown* dropDownDocumentColorsFollowTheme = nullptr;
    Str startThemePref; // prefs theme at open, for Cancel revert
    DocumentColorsFollowTheme startDocumentColorsFollowTheme = DocumentColorsFollowTheme::Off;

    bool Create(MainWindow* win);
    bool PreTranslateMessage(MSG&) override;

    void OnSelectionChanged();
    void OnDocumentColorsFollowThemeChanged();
    void PreviewDocumentColors();
    void OnCancel();
    void OnChange();
    void ScheduleDelete();
};

static ChangeThemeWnd* gChangeThemeWnd = nullptr;

static SeqStrings gDocumentColorsFollowThemeNames = "off\0smart\0legacy\0";

static int DocumentColorsFollowThemeToDropDownIndex(DocumentColorsFollowTheme mode) {
    switch (mode) {
        case DocumentColorsFollowTheme::Smart:
            return 1;
        case DocumentColorsFollowTheme::Legacy:
            return 2;
        case DocumentColorsFollowTheme::Off:
        default:
            return 0;
    }
}

static DocumentColorsFollowTheme DocumentColorsFollowThemeFromDropDownIndex(int idx) {
    switch (idx) {
        case 1:
            return DocumentColorsFollowTheme::Smart;
        case 2:
            return DocumentColorsFollowTheme::Legacy;
        case 0:
        default:
            return DocumentColorsFollowTheme::Off;
    }
}

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

void ChangeThemeWnd::PreviewDocumentColors() {
    UpdateDocumentColors();
    if (win && win->AsFixed()) {
        MainWindowRerender(win);
    }
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
    PreviewDocumentColors();
}

void ChangeThemeWnd::OnDocumentColorsFollowThemeChanged() {
    int idx = dropDownDocumentColorsFollowTheme->GetCurrentSelection();
    if (idx < 0) {
        return;
    }
    SetDocumentColorsFollowTheme(DocumentColorsFollowThemeFromDropDownIndex(idx));
    PreviewDocumentColors();
}

void ChangeThemeWnd::OnCancel() {
    if (!documentColorsFollowThemeOnly) {
        SetTheme(startThemePref);
    }
    SetDocumentColorsFollowTheme(startDocumentColorsFollowTheme);
    PreviewDocumentColors();
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
    startDocumentColorsFollowTheme = GetDocumentColorsFollowTheme();

    {
        CreateCustomArgs args;
        args.title = documentColorsFollowThemeOnly ? _TRA("Document colors follow theme") : _TRA("Change Theme");
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

    if (!documentColorsFollowThemeOnly) {
        int n = ThemeGetCount();
        ListBox::CreateArgs args;
        args.parent = hwnd;
        args.font = font;
        args.isRtl = isRtl;
        auto c = new ListBox();
        c->idealSizeLines = n;
        c->Create(args);
        c->SetColors(colTxt, colBg);
        listBox = c;
        model = new ListBoxModelStrings();
        for (int i = 0; i < n; i++) {
            model->strings.Append(ThemeGetNameAt(i));
        }
        c->onSelectionChanged = MkMethod0<ChangeThemeWnd, &ChangeThemeWnd::OnSelectionChanged>(this);
        c->SetModel(model);
        int currIdx = ThemeGetCurrentIndex();
        if (currIdx >= 0 && currIdx < n) {
            c->SetCurrentSelection(currIdx);
        }
        vbox->AddChild(c);
    }

    if (!documentColorsFollowThemeOnly) {
        Static::CreateArgs args;
        args.parent = hwnd;
        args.font = font;
        args.text = _TRA("Document colors follow theme");
        args.isRtl = isRtl;
        auto c = new Static();
        c->SetColors(colTxt, colBg);
        c->SetInsetsPt(8, 0, 0, 0);
        c->Create(args);
        vbox->AddChild(c);
    }

    {
        DropDown::CreateArgs args;
        args.parent = hwnd;
        args.font = font;
        args.isRtl = isRtl;
        auto c = new DropDown();
        c->SetInsetsPt(4, 0, 0, 0);
        c->Create(args);
        c->SetColors(colTxt, colBg);
        c->SetItemsSeqStrings(gDocumentColorsFollowThemeNames);
        c->onSelectionChanged = MkMethod0<ChangeThemeWnd, &ChangeThemeWnd::OnDocumentColorsFollowThemeChanged>(this);
        dropDownDocumentColorsFollowTheme = c;
        c->SetCurrentSelection(DocumentColorsFollowThemeToDropDownIndex(startDocumentColorsFollowTheme));
        vbox->AddChild(c);
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
    LayoutAndSizeToContent(layout, dx, 0, hwnd);
    PositionDialog(hwnd, win->hwndFrame);

    if (UseDarkModeLib()) {
        DarkMode::setDarkWndSafe(hwnd);
    }

    SetIsVisible(true);
    HwndSetFocus(documentColorsFollowThemeOnly ? dropDownDocumentColorsFollowTheme->hwnd : listBox->hwnd);
    return true;
}

static void ShowThemeDialog(MainWindow* win, bool documentColorsFollowThemeOnly) {
    if (!HasPermission(Perm::SavePreferences)) {
        return;
    }
    if (gChangeThemeWnd) {
        HwndSetFocus(gChangeThemeWnd->hwnd);
        return;
    }
    auto wnd = new ChangeThemeWnd();
    wnd->documentColorsFollowThemeOnly = documentColorsFollowThemeOnly;
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

void ShowChangeThemeDialog(MainWindow* win) {
    ShowThemeDialog(win, false);
}

void ShowSetDocumentColorsFollowThemeDialog(MainWindow* win) {
    ShowThemeDialog(win, true);
}