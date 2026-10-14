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
#include "gui/GuiColors.h"
#include "gui/VirtCtrl.h"

#include "Settings.h"
#include "AppSettings.h"
#include "MainWindow.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "DarkMode.h"
#include "PdfDarkMode.h"
#include "SumatraDialogs.h"
#include "shared/PdfToolsCommon.h"
#include "AppHelpersCommon.h"
#include "ChangeThemeDialogCommon.h"

//   Follow Windows
//   Light                         themes, previewed live
//   ---------------------------------------------- hairline
//   Document colors follow theme        [off  v]
//   ---------------------------------------------- hairline
//                                [Cancel] [Change]
// Only the drop-down is a real HWND.
constexpr int kThemeDlgDx = 320;
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
    void Recolor();

    void KeepFocus();
    void OnSelectionChanged();
    void OnDocumentColorsFollowThemeChanged();
    void PreviewDocumentColors();
    void ApplyPreview();
    void SchedulePreview();
    void KillPreviewTimer();
    void OnTimer(WindowBase::TimerEvent*);
    void OnCancel(VirtMouseEvent* ev = nullptr);
    void OnChange(VirtMouseEvent* ev = nullptr);
};

static ChangeThemeWnd* gChangeThemeWnd = nullptr;

static constexpr UINT_PTR kThemePreviewTimerId = 1;
static constexpr UINT kThemePreviewDebounceMs = 300;
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
    KillPreviewTimer();
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

void ChangeThemeWnd::KillPreviewTimer() {
    if (hwnd) {
        KillTimer(hwnd, kThemePreviewTimerId);
    }
}

// Live-preview the list's current theme. Arrowing through the list would
// rebuild chrome on every item; SchedulePreview waits 300ms after the last
// change. Change/Enter calls this so the saved theme matches the highlight.
void ChangeThemeWnd::ApplyPreview() {
    KillPreviewTimer();
    if (!listBox) {
        return;
    }
    int idx = listBox->GetCurrentSelection();
    if (idx < 0) {
        return;
    }
    if (idx == kFollowWindowsThemeListIndex) {
        SetTheme(StrL("System"));
    } else {
        SetThemeByIndex(idx - 1);
    }
    UpdateTheme();
    Recolor();
    PreviewDocumentColors();
    KeepFocus();
}

// UpdateTheme() recolors the native controls; the drawn ones follow here
void ChangeThemeWnd::Recolor() {
    SetColors(ThemeWindowTextColor(), ThemeWindowControlBackgroundColor());
    if (listBox) {
        SetDlgListColors(listBox);
    }
    labelDocumentColorsFollowTheme->SetColor(kColText, ThemeWindowTextDisabledColor());
    SetDlgButtonColors(btnCancel, DlgButtonStyle::Outline);
    SetDlgButtonColors(btnChange, DlgButtonStyle::Filled);
    HwndInvalidate(hwnd);
}

void ChangeThemeWnd::SchedulePreview() {
    if (!hwnd) {
        return;
    }
    KillTimer(hwnd, kThemePreviewTimerId);
    SetTimer(hwnd, kThemePreviewTimerId, kThemePreviewDebounceMs, nullptr);
}

void ChangeThemeWnd::OnTimer(WindowBase::TimerEvent* ev) {
    if (ev->timerId != kThemePreviewTimerId) {
        return;
    }
    ApplyPreview();
}

void ChangeThemeWnd::OnSelectionChanged() {
    SchedulePreview();
}

void ChangeThemeWnd::OnDocumentColorsFollowThemeChanged() {
    int idx = CbGetCurrentSelection(dropDownDocumentColorsFollowTheme);
    if (idx < 0) {
        return;
    }
    SetDocumentColorsFollowTheme(DocumentColorsFollowThemeFromDropDownIndex(idx));
    PreviewDocumentColors();
    KeepFocus();
}

void ChangeThemeWnd::OnCancel(VirtMouseEvent*) {
    KillPreviewTimer();
    if (!documentColorsFollowThemeOnly) {
        SetTheme(startThemePref);
    }
    SetDocumentColorsFollowTheme(startDocumentColorsFollowTheme);
    PreviewDocumentColors();
    ScheduleDelete();
}

void ChangeThemeWnd::OnChange(VirtMouseEvent*) {
    ApplyPreview();
    ScheduleSaveSettings();
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
    startThemePref = str::Dup(gSettings->theme);
    startDocumentColorsFollowTheme = GetDocumentColorsFollowTheme();

    PlatformFont* fontText = DlgUiFont(kToolDlgTextFontPx);
    SetFont(fontText);
    bool isRtl = IsUIRtl();
    {
        CreateCustomArgs args;
        args.title = documentColorsFollowThemeOnly ? Tr("Make Document Colors Follow Theme") : Tr("Change Theme");
        args.visible = false;
        args.style = WS_POPUPWINDOW | WS_CAPTION;
        args.font = fontText;
        args.isRtl = isRtl;
        args.bgColor = ThemeWindowControlBackgroundColor();
        args.icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(GetAppIconID()));
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }
    onTimer = MkMethod1<ChangeThemeWnd, WindowBase::TimerEvent*, &ChangeThemeWnd::OnTimer>(this);

    auto* vbox = new VBox();
    vbox->alignCross = CrossAxisAlign::Stretch;

    if (!documentColorsFollowThemeOnly) {
        int nThemes = ThemeGetCount();
        listBox = NewDlgList(GetDpi(), fontText);
        model = new ListBoxModelStrings();
        // This is a mode, not another color theme, so keep its user-facing
        // name distinct from the persisted Theme = System value.
        model->strings.Append(Tr("Follow Windows"));
        for (int i = 0; i < nThemes; i++) {
            model->strings.Append(ThemeGetNameAt(i));
        }
        listBox->onSelectionChanged = MkMethod0<ChangeThemeWnd, &ChangeThemeWnd::OnSelectionChanged>(this);
        listBox->SetModel(model);
        int currIdx = kFollowWindowsThemeListIndex;
        if (!str::EqI(gSettings->theme, StrL("System"))) {
            currIdx = ThemeGetCurrentIndex() + 1;
        }
        if (currIdx >= 0 && currIdx < len(model->strings)) {
            listBox->SetCurrentSelection(currIdx);
        }
        vbox->AddChild(NewDlgListBox(listBox));
        vbox->AddChild(new VirtLine());
    }

    // whether the document's colors follow: the label muted, the drop-down
    {
        HBox* row = NewDlgRow();
        labelDocumentColorsFollowTheme =
            NewDlgLabel(Tr("Document colors follow theme"), fontText, ThemeWindowTextDisabledColor(), kToolDlgLineDy);
        row->AddChild(labelDocumentColorsFollowTheme);
        row->AddChild(new Spacer(0, 0), 1);

        DropDown::CreateArgs args;
        args.parent = hwnd;
        args.font = fontText;
        args.isRtl = isRtl;
        auto* c = new DropDown();
        c->Create(args);
        c->SetItemsSeqStrings(gDocumentColorsFollowThemeNames);
        c->onSelectionChanged = MkMethod0<ChangeThemeWnd, &ChangeThemeWnd::OnDocumentColorsFollowThemeChanged>(this);
        c->onCloseUp = MkMethod0<ChangeThemeWnd, &ChangeThemeWnd::KeepFocus>(this);
        dropDownDocumentColorsFollowTheme = c;
        CbSetCurrentSelection(c, DocumentColorsFollowThemeToDropDownIndex(startDocumentColorsFollowTheme));
        row->AddChild(c);
        vbox->AddChild(new Padding(row, DpiScaledInsets(0, kToolDlgPadDx)));
    }

    btnCancel = NewDlgButton(hwnd, Tr("Cancel"), fontText, DlgButtonStyle::Outline, false);
    btnCancel->onClick = MkMethod1<ChangeThemeWnd, VirtMouseEvent*, &ChangeThemeWnd::OnCancel>(this);
    // Enter runs this one (WindowBase::ActivateOnEnter), so draw it as the
    // default button to say so
    btnChange = NewDlgButton(hwnd, Tr("Change"), fontText, DlgButtonStyle::Filled, true);
    btnChange->onClick = MkMethod1<ChangeThemeWnd, VirtMouseEvent*, &ChangeThemeWnd::OnChange>(this);
    vbox->AddChild(NewDlgFooter(btnCancel, btnChange));
    layout = vbox;

    int dx = DpiScale(kThemeDlgDx);
    LayoutAndSizeToContent(layout, dx, 0, hwnd);
    // pick up the virtual controls so we paint them and they get their input
    DoLayout(HwndClientRect(hwnd).Size());
    PositionDialog(hwnd, win->hwndFrame);
    UpdateTheme();
    Recolor();

    SetIsVisible(true);
    if (documentColorsFollowThemeOnly) {
        HwndSetFocus(dropDownDocumentColorsFollowTheme->hwnd);
    } else {
        SetFocusTo(listBox);
    }
    return true;
}

void ShowThemeDialog(MainWindow* win, bool documentColorsFollowThemeOnly) {
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
