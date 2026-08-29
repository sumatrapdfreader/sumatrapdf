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
#include "DocController.h"
#include "MainWindow.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "DarkMode_win.h"
#include "SumatraDialogs.h"

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
    VirtText* label = nullptr;
    Edit* editZoom = nullptr;
    VirtListBox* listBox = nullptr;
    ListBoxModelStrings* model = nullptr; // owned by listBox
    VirtButton* btnCancel = nullptr;
    VirtButton* btnZoom = nullptr;

    bool Create(MainWindow* win);
    void SetTarget(MainWindow* win);
    void FillZoom();
    float SelectedZoom();

    void SetEditFromSelection();
    void SelectLevelFromEdit();
    bool MoveSelection(int vkey);
    void OnListSelectionChanged();
    void OnListDoubleClick();
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

    {
        CreateCustomArgs args;
        args.owner = win ? win->hwndFrame : nullptr;
        args.title = _TRA("Zoom");
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
            .s = _TRA("&Magnification:"),
            .font = font,
            .isRtl = isRtl,
            .prefix = true,
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
        auto* c = new Edit();
        c->Create(args);
        c->onTextChanged = MkMethod0<CustomZoomWnd, &CustomZoomWnd::SelectLevelFromEdit>(this);
        editZoom = c;
        vbox->AddChild(c);
    }

    {
        auto* c = new VirtListBox();
        c->dpi = GetDpi();
        c->font = font;
        listBox = c;
        model = new ListBoxModelStrings();
        c->onSelectionChanged = MkMethod0<CustomZoomWnd, &CustomZoomWnd::OnListSelectionChanged>(this);
        c->onDoubleClick = MkMethod0<CustomZoomWnd, &CustomZoomWnd::OnListDoubleClick>(this);
        vbox->AddChild(new Padding(c, DpiScaledInsets(4, 0, 0, 0)));
        FillZoom();
    }

    {
        auto* hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainEnd;
        hbox->alignCross = CrossAxisAlign::CrossCenter;
        hbox->gap = font->averageCharWidth;
        auto pad = Insets{4, 0, 4, 0};

        btnCancel = NewThemedButton(hwnd, _TRA("Cancel"), font, false);
        btnCancel->onClick = MkMethod1<CustomZoomWnd, VirtMouseEvent*, &CustomZoomWnd::OnCancel>(this);
        hbox->AddChild(new Padding(btnCancel, pad));
        btnZoom = NewThemedButton(hwnd, _TRA("Zoom"), font, true);
        btnZoom->onClick = MkMethod1<CustomZoomWnd, VirtMouseEvent*, &CustomZoomWnd::OnOk>(this);
        hbox->AddChild(new Padding(btnZoom, pad));
        vbox->AddChild(hbox);
    }

    auto* padding = new Padding(vbox, DpiScaledInsets(4, 8));
    layout = padding;

    int dx = DpiScale(240);
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
