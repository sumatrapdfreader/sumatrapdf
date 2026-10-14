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
#include "DocController.h"
#include "MainWindow.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "DarkMode.h"
#include "SumatraDialogs.h"
#include "shared/PdfToolsCommon.h"

// The zoom to use is whatever is in the edit field: a level's name ("Fit Page")
// or a number. The list under it is the levels to choose from, and picking one
// writes it into the field, so the two behave like the editable combo box this
// replaces -- except that the whole list is visible instead of one row of it.
// Label, list and buttons are VirtCtrl; only the edit is a real HWND.
struct CustomZoomWnd : WindowBase {
    ~CustomZoomWnd() override = default;

    MainWindow* win = nullptr;
    bool forChm = false;
    float startZoom = 0;
    Vec<float> zoomLevels;
    Edit* editZoom = nullptr;
    VirtListBox* listBox = nullptr;
    ListBoxModelStrings* model = nullptr; // owned by listBox
    VirtButton* btnCancel = nullptr;
    VirtButton* btnZoom = nullptr;
    PlatformFont* fontText = nullptr;
    Color colTxt = kColorUnset;
    Color colBg = kColorUnset;

    bool Create(MainWindow* win);
    void SetTarget(MainWindow* win);
    void FillZoom();
    float SelectedZoom();

    void SetEditFromSelection();
    void SelectLevelFromEdit();
    bool MoveSelection(int vkey);
    void OnListSelectionChanged();
    void OnListDoubleClick();
    void DrawLevel(VirtListBox::DrawItemEvent* ev);
    void OnKeyDown(KeyEvent* ev);
    void OnCancel(VirtMouseEvent* ev = nullptr);
    void OnOk(VirtMouseEvent* ev = nullptr);
};

static CustomZoomWnd* gCustomZoomWnd = nullptr;

static void ClearCustomZoomWnd() {
    gCustomZoomWnd = nullptr;
}

// the fewest rows the list is allowed to shrink to when the screen is too
// short for all of them
constexpr int kZoomListMinLines = 6;
constexpr int kZoomDlgDx = 280;
constexpr int kZoomListRowDy = 24;
// the rows' text lines up with the field's
constexpr int kZoomListInsetDx = 8;
constexpr int kZoomListPadDx = kToolDlgPadDx - kZoomListInsetDx;
constexpr int kZoomListRadius = 4;
constexpr int kZoomListSelShift = 28;

void CustomZoomWnd::FillZoom() {
    if (!listBox || !model) {
        return;
    }
    CollectZoomLevels(zoomLevels, forChm);
    model->strings.Reset();
    for (float z : zoomLevels) {
        model->strings.Append(ZoomLevelStrExact(z));
    }
    listBox->SetModel(model);
    // every level, so there is nothing to scroll to; Create() cuts this back if
    // the screen turns out to be too short for that
    listBox->idealSizeLines = len(zoomLevels);
    int sel = -1;
    for (int i = 0; i < len(zoomLevels); i++) {
        if (zoomLevels[i] == startZoom) {
            sel = i;
            break;
        }
    }
    listBox->SetCurrentSelection(sel);
    if (sel >= 0) {
        listBox->EnsureVisible(sel);
        SetEditFromSelection();
    } else if (editZoom) {
        // a zoom that is none of them, e.g. one typed in here before
        editZoom->SetText(fmt("%.0f%%", startZoom));
    }
}

void CustomZoomWnd::SetTarget(MainWindow* mainWin) {
    win = mainWin;
    forChm = false;
    startZoom = 0;
    if (IsMainWindowValidAndNotClosing(win) && win->IsDocLoaded() && win->ctrl) {
        forChm = IsBrowserDocController(win->ctrl);
        startZoom = win->ctrl->GetZoomVirtual();
    }
    FillZoom();
}

// The row the list is on, written into the edit field the way an editable combo
// box does it, selected so the next keystroke replaces it.
void CustomZoomWnd::SetEditFromSelection() {
    int idx = listBox ? listBox->GetCurrentSelection() : -1;
    if (!editZoom || idx < 0 || idx >= len(zoomLevels)) {
        return;
    }
    editZoom->SetText(ZoomLevelStrExact(zoomLevels[idx]));
    EditSelectAll(editZoom);
}

// The other way round: typing a level's name or one of the percentages puts the
// list on it, so the field and the list never disagree.
void CustomZoomWnd::SelectLevelFromEdit() {
    if (!listBox || !editZoom) {
        return;
    }
    TempStr text = editZoom->GetTextTemp();
    int sel = -1;
    for (int i = 0; i < len(zoomLevels); i++) {
        if (str::EqI(text, ZoomLevelStrExact(zoomLevels[i]))) {
            sel = i;
            break;
        }
    }
    listBox->SetCurrentSelection(sel);
    if (sel >= 0) {
        listBox->EnsureVisible(sel);
    }
}

void CustomZoomWnd::OnListSelectionChanged() {
    SetEditFromSelection();
}

// a level: its name, on a rounded selection
void CustomZoomWnd::DrawLevel(VirtListBox::DrawItemEvent* ev) {
    Gfx* gfx = ev->gfx;
    Rect rc = ev->itemRect;
    gfx->FillRect(rc, colBg);
    if (ev->selected) {
        gfx->FillRoundedRect(rc, DpiScale(kZoomListRadius), AccentColor(colBg, kZoomListSelShift));
    }
    int inset = DpiScale(kZoomListInsetDx);
    Rect rcText{rc.x + inset, rc.y, rc.dx - (2 * inset), rc.dy};
    u32 fmt = gfxTextEllipsis | gfxTextVCenter;
    fmt |= IsUIRtl() ? (gfxTextRight | gfxTextRtl) : gfxTextLeft;
    gfx->DrawText(ZoomLevelStrExact(zoomLevels[ev->itemIndex]), rcText, fmt, fontText, colTxt);
}

void CustomZoomWnd::OnListDoubleClick() {
    SetEditFromSelection();
    OnOk();
}

// Up / Down work wherever the focus is, the way they do in a combo box: they
// move the list and the field follows. They stop at the ends rather than wrap:
// the list is short enough to see where they stop.
bool CustomZoomWnd::MoveSelection(int vkey) {
    if (!listBox) {
        return false;
    }
    int n = listBox->ItemsCount();
    if (n == 0) {
        return false;
    }
    int sel = listBox->GetCurrentSelection();
    if (sel < 0) {
        // a typed zoom that is none of them: step into the list from the end
        // the key comes from
        sel = (vkey == VK_UP) ? n - 1 : 0;
    } else if (vkey == VK_UP) {
        sel = std::max(sel - 1, 0);
    } else {
        sel = std::min(sel + 1, n - 1);
    }
    listBox->SetCurrentSelection(sel);
    listBox->EnsureVisible(sel);
    SetEditFromSelection();
    return true;
}

void CustomZoomWnd::OnKeyDown(KeyEvent* ev) {
    if (ev->vkey == VK_UP || ev->vkey == VK_DOWN) {
        ev->didHandle = MoveSelection(ev->vkey);
    }
}

// What the edit field says: one of the levels by name, or a number typed into
// it. Empty or unreadable leaves the zoom alone.
float CustomZoomWnd::SelectedZoom() {
    TempStr text = editZoom ? editZoom->GetTextTemp() : Str{};
    for (int i = 0; i < len(zoomLevels); i++) {
        if (str::EqI(text, ZoomLevelStrExact(zoomLevels[i]))) {
            return zoomLevels[i];
        }
    }
    if (len(text) == 0) {
        return startZoom;
    }
    float zoom = (float)atof(CStrTemp(text));
    if (zoom == 0) {
        return startZoom;
    }
    return limitValue(zoom, kZoomMin, kZoomMax);
}

void CustomZoomWnd::OnCancel(VirtMouseEvent*) {
    ScheduleDelete();
}

void CustomZoomWnd::OnOk(VirtMouseEvent*) {
    if (IsMainWindowValidAndNotClosing(win) && win->IsDocLoaded()) {
        SmartZoom(win, SelectedZoom(), nullptr, true);
    }
    ScheduleDelete();
}

static void OnClose(WindowBase::CloseEvent* /*ev*/) {
    if (gCustomZoomWnd) {
        gCustomZoomWnd->OnCancel();
    }
}

static void OnDestroy(WindowBase::DestroyEvent* /*ev*/) {
    if (gCustomZoomWnd) {
        gCustomZoomWnd->ScheduleDelete();
    }
}

bool CustomZoomWnd::Create(MainWindow* mainWin) {
    win = mainWin;
    if (IsMainWindowValidAndNotClosing(win) && win->IsDocLoaded() && win->ctrl) {
        forChm = IsBrowserDocController(win->ctrl);
        startZoom = win->ctrl->GetZoomVirtual();
    }

    colTxt = ThemeWindowTextColor();
    colBg = ThemeWindowControlBackgroundColor();
    Color colMuted = ThemeWindowTextDisabledColor();
    fontText = DlgUiFont(kToolDlgTextFontPx);
    SetFont(fontText);
    bool isRtl = IsUIRtl();
    {
        CreateCustomArgs args;
        args.owner = win ? win->hwndFrame : nullptr;
        args.title = Tr("Zoom");
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

    // the zoom, in the larger font
    {
        HBox* row = NewDlgRow();
        row->AddChild(NewDlgLabel(Tr("&Magnification:"), fontText, colMuted, kToolDlgLineDy, true));
        Edit::CreateArgs args;
        args.parent = hwnd;
        args.font = DlgUiFont(kToolDlgQueryFontPx);
        args.withBorder = false;
        args.selectAllOnFocus = true;
        args.isRtl = isRtl;
        editZoom = new Edit();
        editZoom->SetColors(colTxt, colBg);
        editZoom->Create(args);
        editZoom->onTextChanged = MkMethod0<CustomZoomWnd, &CustomZoomWnd::SelectLevelFromEdit>(this);
        row->AddChild(editZoom, 1);
        vbox->AddChild(new Padding(row, DpiScaledInsets(0, kToolDlgPadDx)));
    }
    vbox->AddChild(new VirtLine());

    // the levels, the one in the field on a rounded selection
    {
        auto* c = new VirtListBox();
        c->dpi = GetDpi();
        c->font = fontText;
        c->itemDy = DpiScale(kZoomListRowDy);
        c->showFocusRing = false;
        c->SetColor(kColListText, colTxt);
        c->SetColor(kColListBg, colBg);
        c->onDrawItem = MkMethod1<CustomZoomWnd, VirtListBox::DrawItemEvent*, &CustomZoomWnd::DrawLevel>(this);
        listBox = c;
        model = new ListBoxModelStrings();
        c->onSelectionChanged = MkMethod0<CustomZoomWnd, &CustomZoomWnd::OnListSelectionChanged>(this);
        c->onDoubleClick = MkMethod0<CustomZoomWnd, &CustomZoomWnd::OnListDoubleClick>(this);
        vbox->AddChild(new Padding(c, DpiScaledInsets(kToolDlgFilesPadDy, kZoomListPadDx)));
        FillZoom();
    }

    btnCancel = NewDlgButton(hwnd, Tr("Cancel"), fontText, DlgButtonStyle::Outline, false);
    btnCancel->onClick = MkMethod1<CustomZoomWnd, VirtMouseEvent*, &CustomZoomWnd::OnCancel>(this);
    btnZoom = NewDlgButton(hwnd, Tr("Zoom"), fontText, DlgButtonStyle::Filled, true);
    btnZoom->onClick = MkMethod1<CustomZoomWnd, VirtMouseEvent*, &CustomZoomWnd::OnOk>(this);
    vbox->AddChild(NewDlgFooter(btnCancel, btnZoom));
    layout = vbox;

    int dx = DpiScale(kZoomDlgDx);
    LayoutAndSizeToContent(layout, dx, 0, hwnd);
    {
        // the list asked for all of its rows: give some back if that made the
        // window taller than the screen it will open on
        Rect wa = GetWorkAreaRect({}, win ? win->hwndFrame : hwnd);
        int dy = HwndWindowRect(hwnd).dy;
        int rowDy = listBox->GetItemHeight();
        if (dy > wa.dy && rowDy > 0) {
            int drop = ((dy - wa.dy) + rowDy - 1) / rowDy;
            int lines = std::max(listBox->idealSizeLines - drop, kZoomListMinLines);
            if (lines != listBox->idealSizeLines) {
                listBox->idealSizeLines = lines;
                LayoutAndSizeToContent(layout, dx, 0, hwnd);
            }
        }
    }
    DoLayout(HwndClientRect(hwnd).Size());
    HwndCenterDialog(hwnd, win ? win->hwndFrame : nullptr);
    UpdateTheme();

    SetIsVisible(true);
    if (editZoom) {
        HwndSetFocus(editZoom->hwnd);
        EditSelectAll(editZoom);
    }
    return true;
}

void ShowCustomZoomDialog(MainWindow* win) {
    if (!IsMainWindowValidAndNotClosing(win) || !win->IsDocLoaded()) {
        return;
    }
    if (gCustomZoomWnd) {
        gCustomZoomWnd->SetTarget(win);
        HwndSetFocus(gCustomZoomWnd->hwnd);
        if (gCustomZoomWnd->editZoom) {
            HwndSetFocus(gCustomZoomWnd->editZoom->hwnd);
            EditSelectAll(gCustomZoomWnd->editZoom);
        }
        return;
    }
    auto* wnd = new CustomZoomWnd();
    wnd->closeOnEsc = true;
    wnd->onBeforeDelete = MkFunc0Void(ClearCustomZoomWnd);
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnDestroy);
    wnd->SetFont(GetAppFont());
    bool ok = wnd->Create(win);
    if (!ok) {
        delete wnd;
        return;
    }
    wnd->onKeyDown = MkMethod1<CustomZoomWnd, KeyEvent*, &CustomZoomWnd::OnKeyDown>(wnd);
    gCustomZoomWnd = wnd;
    RunModalWindow(wnd->hwnd, win ? win->hwndFrame : nullptr);
}
