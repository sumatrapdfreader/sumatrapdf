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

struct ChangeThemeWnd : WindowBase {
    ~ChangeThemeWnd() override;

    HFONT font = nullptr;
    MainWindow* win = nullptr;
    bool documentColorsFollowThemeOnly = false;
    ListBox* listBox = nullptr;
    ListBoxModelStrings* model = nullptr; // owned by listBox
    Static* staticDocumentColorsFollowTheme = nullptr;
    DropDown* dropDownDocumentColorsFollowTheme = nullptr;
    Button* btnCancel = nullptr;
    Button* btnChange = nullptr;
    Str startThemePref; // prefs theme at open, for Cancel revert
    DocumentColorsFollowTheme startDocumentColorsFollowTheme = DocumentColorsFollowTheme::Off;

    bool Create(MainWindow* win);
    bool PreTranslateMessage(MSG& msg) override;

    void UpdateTheme();
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
    if (mode == DocumentColorsFollowTheme::Smart) {
        return 1;
    }
    if (mode == DocumentColorsFollowTheme::Legacy) {
        return 2;
    }
    return 0;
}

static DocumentColorsFollowTheme DocumentColorsFollowThemeFromDropDownIndex(int idx) {
    if (idx == 1) {
        return DocumentColorsFollowTheme::Smart;
    }
    if (idx == 2) {
        return DocumentColorsFollowTheme::Legacy;
    }
    return DocumentColorsFollowTheme::Off;
}

ChangeThemeWnd::~ChangeThemeWnd() {
    str::Free(startThemePref);
}

void SafeDeleteChangeThemeDialog() {
    if (!gChangeThemeWnd) {
        return;
    }
    auto* tmp = gChangeThemeWnd;
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

// Put the dialog beside the main window instead of on top of it, so the page
// stays visible while the theme is previewed live. Of the two sides, whichever
// has room for it wins; if both do, the roomier one. When the main window fills
// the monitor (maximized or full screen) neither side has room, so the dialog
// goes against the right edge of the work area.
static void PositionDialog(HWND hwnd, HWND hwndRelative) {
    Rect rRelative = HwndWindowRect(hwndRelative);
    Rect r = HwndWindowRect(hwnd);
    Rect work = GetWorkAreaRect(rRelative, hwndRelative);

    int gap = DpiScale(hwnd, 8);
    int spaceLeft = rRelative.x - work.x;
    int spaceRight = work.Right() - rRelative.Right();
    bool fitsLeft = spaceLeft >= r.dx + gap;
    bool fitsRight = spaceRight >= r.dx + gap;

    int x;
    if (fitsRight && (!fitsLeft || spaceRight >= spaceLeft)) {
        x = rRelative.Right() + gap;
    } else if (fitsLeft) {
        x = rRelative.x - gap - r.dx;
    } else {
        x = work.Right() - r.dx;
    }
    // vertically centered on the main window
    int y = rRelative.y + ((rRelative.dy - r.dy) / 2);

    r = {x, y, r.dx, r.dy};
    // last word on staying fully on screen, e.g. a main window taller than the
    // work area, or dragged partly off it
    Rect r2 = ShiftRectToWorkArea(r, hwndRelative, true);
    SetWindowPos(hwnd, nullptr, r2.x, r2.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}

void ChangeThemeWnd::PreviewDocumentColors() {
    UpdateDocumentColors();
    if (win && win->AsFixed()) {
        MainWindowRerender(win);
    }
}

void ChangeThemeWnd::UpdateTheme() {
    COLORREF colBg = ThemeWindowControlBackgroundColor();
    COLORREF colTxt = ThemeWindowTextColor();
    SetColors(colTxt, colBg);
    if (listBox) {
        listBox->SetColors(colTxt, colBg);
    }
    if (staticDocumentColorsFollowTheme) {
        staticDocumentColorsFollowTheme->SetColors(colTxt, colBg);
    }
    if (dropDownDocumentColorsFollowTheme) {
        dropDownDocumentColorsFollowTheme->SetColors(colTxt, colBg);
    }
    if (btnCancel) {
        btnCancel->SetColors(colTxt, colBg);
    }
    if (btnChange) {
        btnChange->SetColors(colTxt, colBg);
    }
    if (UseDarkModeLib()) {
        DarkMode::setDarkWndSafe(hwnd);
    }
    RedrawWindow(hwnd, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
}

void ChangeThemeWnd::OnSelectionChanged() {
    int idx = listBox->GetCurrentSelection();
    if (idx < 0) {
        return;
    }
    SetThemeByIndex(idx);
    UpdateTheme();
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
    if (msg.message != WM_KEYDOWN) {
        return false;
    }
    if (msg.wParam == VK_ESCAPE) {
        OnCancel();
        return true;
    }
    if (msg.wParam == VK_RETURN) {
        // an open drop-down list gets Enter first: there it commits the
        // highlighted entry rather than the dialog
        HWND hwndDrop = dropDownDocumentColorsFollowTheme ? dropDownDocumentColorsFollowTheme->hwnd : nullptr;
        if (hwndDrop && SendMessageW(hwndDrop, CB_GETDROPPEDSTATE, 0, 0)) {
            return false;
        }
        // Enter presses the focused button, like a real dialog does; anywhere
        // else it's the default action
        if (btnCancel && GetFocus() == btnCancel->hwnd) {
            OnCancel();
        } else {
            OnChange();
        }
        return true;
    }
    return false;
}

static void OnClose(WindowBase::CloseEvent* /*ev*/) {
    if (gChangeThemeWnd) {
        gChangeThemeWnd->OnCancel();
    }
}

static void OnDestroy(WindowBase::DestroyEvent* /*ev*/) {
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
        args.title = documentColorsFollowThemeOnly ? _TRA("Make Document Colors Follow Theme") : _TRA("Change Theme");
        args.visible = false;
        args.style = WS_POPUPWINDOW | WS_CAPTION;
        args.font = font;
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

    if (!documentColorsFollowThemeOnly) {
        int n = ThemeGetCount();
        ListBox::CreateArgs args;
        args.parent = hwnd;
        args.font = font;
        args.isRtl = isRtl;
        auto* c = new ListBox();
        c->Create(args);
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
        auto* c = new Static();
        c->SetInsetsPt(8, 0, 0, 0);
        c->Create(args);
        staticDocumentColorsFollowTheme = c;
        vbox->AddChild(c);
    }

    {
        DropDown::CreateArgs args;
        args.parent = hwnd;
        args.font = font;
        args.isRtl = isRtl;
        auto* c = new DropDown();
        c->SetInsetsPt(4, 0, 0, 0);
        c->Create(args);
        c->SetItemsSeqStrings(gDocumentColorsFollowThemeNames);
        c->onSelectionChanged = MkMethod0<ChangeThemeWnd, &ChangeThemeWnd::OnDocumentColorsFollowThemeChanged>(this);
        dropDownDocumentColorsFollowTheme = c;
        c->SetCurrentSelection(DocumentColorsFollowThemeToDropDownIndex(startDocumentColorsFollowTheme));
        vbox->AddChild(c);
    }

    {
        auto* hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainEnd;
        hbox->alignCross = CrossAxisAlign::CrossCenter;
        auto pad = Insets{4, 8, 4, 8};

        btnCancel =
            CreateButton(hwnd, _TRA("Cancel"), MkMethod0<ChangeThemeWnd, &ChangeThemeWnd::OnCancel>(this), isRtl);
        hbox->AddChild(new Padding(btnCancel, pad));
        btnChange =
            CreateButton(hwnd, _TRA("Change"), MkMethod0<ChangeThemeWnd, &ChangeThemeWnd::OnChange>(this), isRtl);
        // Enter runs this one (see PreTranslateMessage), so draw it as the
        // default button to say so
        btnChange->isDefault = true;
        hbox->AddChild(new Padding(btnChange, pad));
        vbox->AddChild(hbox);
    }

    auto* padding = new Padding(vbox, DpiScaledInsets(hwnd, 4, 8));
    layout = padding;

    int dx = DpiScale(hwnd, 280);
    LayoutAndSizeToContent(layout, dx, 0, hwnd);
    PositionDialog(hwnd, win->hwndFrame);
    UpdateTheme();

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
    auto* wnd = new ChangeThemeWnd();
    wnd->documentColorsFollowThemeOnly = documentColorsFollowThemeOnly;
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnDestroy);
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
