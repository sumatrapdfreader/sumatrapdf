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
#include "DocController.h"
#include "MainWindow.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "DarkMode_win.h"
#include "CustomZoomDialog.h"

// Label and buttons are VirtCtrl; the zoom field is an editable DropDown.
// Same WindowBase layout pattern as Change Theme / Add Favorite.
struct CustomZoomWnd : WindowBase {
    ~CustomZoomWnd() override = default;

    MainWindow* win = nullptr;
    bool forChm = false;
    float startZoom = 0;
    Vec<float> zoomLevels;
    VirtText* label = nullptr;
    DropDown* dropDown = nullptr;
    VirtButton* btnCancel = nullptr;
    VirtButton* btnZoom = nullptr;

    bool Create(MainWindow* win);
    void SetTarget(MainWindow* win);
    void FillZoom();
    float SelectedZoom();

    void OnCancel(VirtMouseEvent* ev = nullptr);
    void OnOk(VirtMouseEvent* ev = nullptr);
};

static CustomZoomWnd* gCustomZoomWnd = nullptr;

static void ClearCustomZoomWnd() {
    gCustomZoomWnd = nullptr;
}

void CustomZoomWnd::FillZoom() {
    if (!dropDown) {
        return;
    }
    CollectZoomLevels(zoomLevels, forChm);
    StrVec items;
    for (float z : zoomLevels) {
        items.Append(ZoomLevelStr(z));
    }
    dropDown->SetItems(items);
    int sel = -1;
    for (int i = 0; i < len(zoomLevels); i++) {
        if (zoomLevels[i] == startZoom) {
            sel = i;
            break;
        }
    }
    if (sel >= 0) {
        dropDown->SetCurrentSelection(sel);
    } else {
        dropDown->SetText(fmt("%.0f%%", startZoom));
    }
}

void CustomZoomWnd::SetTarget(MainWindow* mainWin) {
    win = mainWin;
    forChm = false;
    startZoom = 0;
    if (IsMainWindowValid(win) && win->IsDocLoaded() && win->ctrl) {
        forChm = IsBrowserDocController(win->ctrl);
        startZoom = win->ctrl->GetZoomVirtual();
    }
    FillZoom();
}

// Selected list entry, or a typed number (empty / non-numeric keeps startZoom).
float CustomZoomWnd::SelectedZoom() {
    int idx = dropDown ? dropDown->GetCurrentSelection() : -1;
    if (idx >= 0 && idx < len(zoomLevels)) {
        float z = zoomLevels[idx];
        return z == 0 ? startZoom : z;
    }
    TempStr text = dropDown ? dropDown->GetTextTemp() : Str{};
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
    if (IsMainWindowValid(win) && win->IsDocLoaded()) {
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
    if (IsMainWindowValid(win) && win->IsDocLoaded() && win->ctrl) {
        forChm = IsBrowserDocController(win->ctrl);
        startZoom = win->ctrl->GetZoomVirtual();
    }

    {
        CreateCustomArgs args;
        args.title = _TRA("Zoom factor");
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
        DropDown::CreateArgs args;
        args.parent = hwnd;
        args.font = GetFont();
        args.isRtl = isRtl;
        args.isEditable = true;
        auto* c = new DropDown();
        c->Create(args);
        dropDown = c;
        vbox->AddChild(c);
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
    DoLayout(HwndClientRect(hwnd).Size());
    HwndCenterDialog(hwnd, win ? win->hwndFrame : nullptr);
    UpdateTheme();

    SetIsVisible(true);
    if (dropDown) {
        HwndSetFocus(dropDown->hwnd);
    }
    return true;
}

void ShowCustomZoomDialog(MainWindow* win) {
    if (!IsMainWindowValid(win) || !win->IsDocLoaded()) {
        return;
    }
    if (gCustomZoomWnd) {
        gCustomZoomWnd->SetTarget(win);
        HwndSetFocus(gCustomZoomWnd->hwnd);
        if (gCustomZoomWnd->dropDown) {
            HwndSetFocus(gCustomZoomWnd->dropDown->hwnd);
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
    gCustomZoomWnd = wnd;
}
