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
#include "Translations.h"
#include "DarkMode_win.h"
#include "PdfDarkMode.h"
#include "ChangeThemeDialog.h"

// The theme list, the label and the buttons are virtual controls (VirtCtrl);
// only the drop-down is a real HWND. They all sit in the same layout tree,
// which the window paints and dispatches input to
struct ChangeThemeWnd : WindowBase {
    ~ChangeThemeWnd() override;

    MainWindow* win = nullptr;
    bool documentColorsFollowThemeOnly = false;
    VirtListBox* listBox = nullptr;
    ListBoxModelStrings* model = nullptr; // owned by listBox
    VirtText* labelDocumentColorsFollowTheme = nullptr;
    DropDown* dropDownDocumentColorsFollowTheme = nullptr;
    VirtButton* btnCancel = nullptr;
    VirtButton* btnChange = nullptr;
    Str startThemePref; // prefs theme at open, for Cancel revert
    DocumentColorsFollowTheme startDocumentColorsFollowTheme = DocumentColorsFollowTheme::Off;

    bool Create(MainWindow* win);

    void KeepFocus();
    void OnSelectionChanged();
    void OnDocumentColorsFollowThemeChanged();
    void PreviewDocumentColors();
    void OnCancel(VirtMouseEvent* ev = nullptr);
    void OnChange(VirtMouseEvent* ev = nullptr);
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

static void ClearChangeThemeWnd() {
    gChangeThemeWnd = nullptr;
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

    int gap = DpiScale(8);
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

// Picking a theme refreshes the whole app: the main window's toolbar, find bar,
// menu bar and AI chat webview are rebuilt, and some of that takes the keyboard
// focus. The user is working in this dialog, so take the focus back (a focus
// that is already on one of our controls, e.g. the drop-down, stays put)
void ChangeThemeWnd::KeepFocus() {
    HwndToForeground(hwnd);
    HWND focused = ::GetFocus();
    if (focused == hwnd || ::IsChild(hwnd, focused)) {
        return;
    }
    HwndSetFocus(hwnd);
}

void ChangeThemeWnd::OnSelectionChanged() {
    int idx = listBox->GetCurrentSelection();
    if (idx < 0) {
        return;
    }
    SetThemeByIndex(idx);
    UpdateTheme();
    PreviewDocumentColors();
    KeepFocus();
}

void ChangeThemeWnd::OnDocumentColorsFollowThemeChanged() {
    int idx = dropDownDocumentColorsFollowTheme->GetCurrentSelection();
    if (idx < 0) {
        return;
    }
    SetDocumentColorsFollowTheme(DocumentColorsFollowThemeFromDropDownIndex(idx));
    PreviewDocumentColors();
    KeepFocus();
}

void ChangeThemeWnd::OnCancel(VirtMouseEvent*) {
    if (!documentColorsFollowThemeOnly) {
        SetTheme(startThemePref);
    }
    SetDocumentColorsFollowTheme(startDocumentColorsFollowTheme);
    PreviewDocumentColors();
    ScheduleDelete();
}

void ChangeThemeWnd::OnChange(VirtMouseEvent*) {
    SaveSettings();
    ScheduleDelete();
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

    if (!documentColorsFollowThemeOnly) {
        int n = ThemeGetCount();
        auto* c = new VirtListBox();
        c->dpi = GetDpi();
        c->font = font;
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

        auto* label = NewVirtText({
            .s = _TRA("Document colors follow theme"),
            .font = font,
            .isRtl = isRtl,
            .padding = DpiScaledInsets(8, 0, 0, 0),
        });
        labelDocumentColorsFollowTheme = label;
        vbox->AddChild(label);
    }

    {
        DropDown::CreateArgs args;
        args.parent = hwnd;
        args.font = GetFont();
        args.isRtl = isRtl;
        auto* c = new DropDown();
        c->SetInsetsPt(4, 0, 0, 0);
        c->Create(args);
        c->SetItemsSeqStrings(gDocumentColorsFollowThemeNames);
        c->onSelectionChanged = MkMethod0<ChangeThemeWnd, &ChangeThemeWnd::OnDocumentColorsFollowThemeChanged>(this);
        c->onCloseUp = MkMethod0<ChangeThemeWnd, &ChangeThemeWnd::KeepFocus>(this);
        dropDownDocumentColorsFollowTheme = c;
        c->SetCurrentSelection(DocumentColorsFollowThemeToDropDownIndex(startDocumentColorsFollowTheme));
        vbox->AddChild(c);
    }

    {
        auto* hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainEnd;
        hbox->alignCross = CrossAxisAlign::CrossCenter;
        hbox->gap = font->averageCharWidth;
        auto pad = Insets{4, 0, 4, 0};

        btnCancel = NewThemedButton(hwnd, _TRA("Cancel"), font, false);
        btnCancel->onClick = MkMethod1<ChangeThemeWnd, VirtMouseEvent*, &ChangeThemeWnd::OnCancel>(this);
        hbox->AddChild(new Padding(btnCancel, pad));
        // Enter runs this one (WindowBase::ActivateOnEnter), so draw it
        // as the default button to say so
        btnChange = NewThemedButton(hwnd, _TRA("Change"), font, true);
        btnChange->onClick = MkMethod1<ChangeThemeWnd, VirtMouseEvent*, &ChangeThemeWnd::OnChange>(this);
        hbox->AddChild(new Padding(btnChange, pad));
        vbox->AddChild(hbox);
    }

    auto* padding = new Padding(vbox, DpiScaledInsets(4, 8));
    layout = padding;

    int dx = DpiScale(280);
    LayoutAndSizeToContent(layout, dx, 0, hwnd);
    // pick up the virtual controls so we paint them and they get their input
    DoLayout(HwndClientRect(hwnd).Size());
    PositionDialog(hwnd, win->hwndFrame);
    UpdateTheme();

    SetIsVisible(true);
    if (documentColorsFollowThemeOnly) {
        HwndSetFocus(dropDownDocumentColorsFollowTheme->hwnd);
    } else {
        SetFocusTo(listBox);
    }
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
    wnd->closeOnEsc = true;
    wnd->onBeforeDelete = MkFunc0Void(ClearChangeThemeWnd);
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnDestroy);
    wnd->SetFont(GetAppFont());
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
