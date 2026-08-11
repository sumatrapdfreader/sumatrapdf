/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"
#include "base/Dpi.h"
#include "base/UITask.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"
#include "wingui/PlatformFont.h"
#include "wingui/Gfx.h"
#include "wingui/VirtWnd.h"

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

// The theme list, the label and the buttons are virtual controls (VirtWnd);
// only the drop-down is a real HWND. They all sit in the same layout tree,
// which the window paints and dispatches input to
struct ChangeThemeWnd : WindowBase {
    ~ChangeThemeWnd() override;

    HFONT font = nullptr;
    PlatformFont* platformFont = nullptr;
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
    bool PreTranslateMessage(MSG& msg) override;
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override;

    VirtButton* NewButton(Str text, bool isDefault);
    void StyleButton(VirtButton*, bool isDefault);

    void UpdateTheme();
    void KeepFocus();
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

// the buttons are virtual controls, so they are styled here rather than by the
// system: a filled box with a border, brighter on hover (same look as the PDF
// tool dialogs). Re-applied on every theme change
void ChangeThemeWnd::StyleButton(VirtButton* b, bool isDefault) {
    COLORREF bg = ThemeWindowControlBackgroundColor();
    b->textColor = ThemeWindowTextColor();
    b->textColorDisabled = ThemeWindowTextDisabledColor();
    // the default button is a shade stronger, like a native default button
    b->bgColor = AccentColor(bg, isDefault ? 26 : 14);
    b->bgColorHover = AccentColor(bg, isDefault ? 40 : 28);
    b->borderColor = isDefault ? ThemeHotEdgeColor() : ThemeEdgeColor();
}

VirtButton* ChangeThemeWnd::NewButton(Str text, bool isDefault) {
    auto* b = new VirtButton(text, platformFont);
    StyleButton(b, isDefault);
    b->textPadding = DpiScaledInsets(hwnd, 5, 12);
    return b;
}

void ChangeThemeWnd::UpdateTheme() {
    COLORREF colBg = ThemeWindowControlBackgroundColor();
    COLORREF colTxt = ThemeWindowTextColor();
    SetColors(colTxt, colBg);
    if (listBox) {
        listBox->textColor = colTxt;
        listBox->bgColor = colBg;
    }
    if (labelDocumentColorsFollowTheme) {
        labelDocumentColorsFollowTheme->textColor = colTxt;
    }
    if (dropDownDocumentColorsFollowTheme) {
        dropDownDocumentColorsFollowTheme->SetColors(colTxt, colBg);
    }
    if (btnCancel) {
        StyleButton(btnCancel, false);
    }
    if (btnChange) {
        StyleButton(btnChange, true);
    }
    if (UseDarkModeLib()) {
        DarkMode::setDarkWndSafe(hwnd);
    }
    RedrawWindow(hwnd, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
}

// Picking a theme refreshes the whole app: the main window's toolbar, find bar,
// menu bar and AI chat webview are rebuilt, and some of that takes the keyboard
// focus. The user is working in this dialog, so take the focus back (a focus
// that is already on one of our controls, e.g. the drop-down, stays put)
void ChangeThemeWnd::KeepFocus() {
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

LRESULT ChangeThemeWnd::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_ERASEBKGND) {
        return TRUE; // OnPaint covers the whole client area, double-buffered
    }
    // WindowBase::WndProcDefault sends the virtual controls their input
    return WndProcDefault(hwnd, msg, wp, lp);
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
        if (btnCancel && vroot && vroot->focused == btnCancel) {
            OnCancel();
        } else {
            OnChange();
        }
        return true;
    }
    // Tab moves between the list, the drop-down and the buttons; the arrow keys
    // go to whichever virtual control has the focus
    return WindowBase::PreTranslateMessage(msg);
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

static void CancelClicked(ChangeThemeWnd* wnd, VirtMouseEvent*) {
    wnd->OnCancel();
}

static void ChangeClicked(ChangeThemeWnd* wnd, VirtMouseEvent*) {
    wnd->OnChange();
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
    platformFont = GetPlatformFont(font);

    bool isRtl = IsUIRtl();

    auto* vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    if (!documentColorsFollowThemeOnly) {
        int n = ThemeGetCount();
        auto* c = new VirtListBox();
        c->hwndForDpi = hwnd;
        c->font = platformFont;
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
            .font = platformFont,
            .isRtl = isRtl,
            .padding = DpiScaledInsets(hwnd, 8, 0, 0, 0),
        });
        labelDocumentColorsFollowTheme = label;
        vbox->AddChild(label);
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

        btnCancel = NewButton(_TRA("Cancel"), false);
        btnCancel->onClick = MkFunc1(CancelClicked, this);
        hbox->AddChild(new Padding(btnCancel, pad));
        // Enter runs this one (see PreTranslateMessage), so draw it as the
        // default button to say so
        btnChange = NewButton(_TRA("Change"), true);
        btnChange->onClick = MkFunc1(ChangeClicked, this);
        hbox->AddChild(new Padding(btnChange, pad));
        vbox->AddChild(hbox);
    }

    auto* padding = new Padding(vbox, DpiScaledInsets(hwnd, 4, 8));
    layout = padding;

    int dx = DpiScale(hwnd, 280);
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
