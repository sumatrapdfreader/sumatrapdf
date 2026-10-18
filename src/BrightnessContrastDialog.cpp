/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
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
#include "EngineBase.h"
#include "DisplayModel.h"
#include "RenderCache.h"
#include "WindowTab.h"
#include "MainWindow.h"
#include "FileHistory.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "SumatraDialogs.h"
#include "shared/PdfToolsCommon.h"
#include "BrightnessContrastDialogCommon.h"
#include "BrightnessContrastDialog.h"

// Brightness and contrast of the rendered pages of one file (#4788). The
// sliders preview live; OK remembers the values in the file's FileState,
// Cancel restores what the file had.
//
//   Brightness:  ------o------   0
//   Contrast:    ------o------   0
//   Reset                 [Cancel] [OK]
constexpr int kBcDlgDx = 380;

struct BrightnessContrastWnd : WindowBase {
    MainWindow* win = nullptr;
    Str filePath;
    // what the file had when the dialog opened, restored by Cancel
    int origBrightness = 0;
    int origContrast = 0;

    VirtSlider* sliderBrightness = nullptr;
    VirtSlider* sliderContrast = nullptr;
    VirtText* valueBrightness = nullptr;
    VirtText* valueContrast = nullptr;

    ~BrightnessContrastWnd() override;
    bool Create(MainWindow* win);
    WindowTab* TargetTab();
    void OnSliderChanged();
    void UpdateValues();
    void OnReset(VirtMouseEvent* ev = nullptr);
    void OnCancel(VirtMouseEvent* ev = nullptr);
    void OnOk(VirtMouseEvent* ev = nullptr);
};

static BrightnessContrastWnd* gBrightnessContrastWnd = nullptr;

BrightnessContrastWnd::~BrightnessContrastWnd() {
    str::Free(filePath);
}

static void ClearBrightnessContrastWnd() {
    gBrightnessContrastWnd = nullptr;
}

WindowTab* BrightnessContrastWnd::TargetTab() {
    if (!IsMainWindowValidAndNotClosing(win)) {
        return nullptr;
    }
    WindowTab* t = FindTabByFilePath(filePath);
    if (!t || t->win != win) {
        return nullptr;
    }
    return t;
}

void BrightnessContrastWnd::UpdateValues() {
    valueBrightness->SetText(fmt("%d", sliderBrightness->value));
    valueContrast->SetText(fmt("%d", sliderContrast->value));
    // no relayout: the labels are sized for "-100" and relayout drops keyboard focus
    valueBrightness->Invalidate();
    valueContrast->Invalidate();
}

void BrightnessContrastWnd::OnSliderChanged() {
    UpdateValues();
    SetTabPageAdjust(TargetTab(), sliderBrightness->value, sliderContrast->value, PageAdjustSave::Preview);
}

void BrightnessContrastWnd::OnReset(VirtMouseEvent*) {
    sliderBrightness->SetValue(0, false);
    sliderContrast->SetValue(0, false);
    OnSliderChanged();
}

void BrightnessContrastWnd::OnCancel(VirtMouseEvent*) {
    SetTabPageAdjust(TargetTab(), origBrightness, origContrast, PageAdjustSave::Preview);
    ScheduleDelete();
}

void BrightnessContrastWnd::OnOk(VirtMouseEvent*) {
    SetTabPageAdjust(TargetTab(), sliderBrightness->value, sliderContrast->value, PageAdjustSave::Remember);
    ScheduleDelete();
}

static void OnClose(WindowBase::CloseEvent*) {
    if (gBrightnessContrastWnd) {
        gBrightnessContrastWnd->OnCancel();
    }
}

static void OnDestroy(WindowBase::DestroyEvent*) {
    if (gBrightnessContrastWnd) {
        gBrightnessContrastWnd->ScheduleDelete();
    }
}

static VirtSlider* NewAdjustSlider(BrightnessContrastWnd* wnd, int value) {
    auto* sl = new VirtSlider();
    sl->minVal = kPageAdjustMin;
    sl->maxVal = kPageAdjustMax;
    sl->value = value;
    sl->onValueChanged = MkMethod0<BrightnessContrastWnd, &BrightnessContrastWnd::OnSliderChanged>(wnd);
    return sl;
}

bool BrightnessContrastWnd::Create(MainWindow* mainWin) {
    win = mainWin;
    WindowTab* tab = win->CurrentTab();
    DisplayModel* dm = tab->AsFixed();
    filePath = str::Dup(tab->filePath);
    origBrightness = dm->brightness;
    origContrast = dm->contrast;

    Color colTxt = ThemeWindowTextColor();
    Color colBg = ThemeWindowControlBackgroundColor();
    Color colMuted = ThemeWindowTextDisabledColor();
    PlatformFont* fontText = DlgUiFont(kToolDlgTextFontPx);
    SetFont(fontText);
    bool isRtl = IsUIRtl();
    {
        CreateCustomArgs args;
        // owned by the main window, so it can't end up behind it
        args.owner = win->hwndFrame;
        args.title = Tr("Brightness / Contrast");
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
    auto* rows = new VBox();
    rows->alignCross = CrossAxisAlign::Stretch;

    // label, slider, value; the labels share a column
    Vec<VirtText*> labels;
    struct {
        Str label;
        VirtSlider** slider;
        VirtText** value;
        int initial;
    } specs[] = {
        {Tr("&Brightness:"), &sliderBrightness, &valueBrightness, origBrightness},
        {Tr("&Contrast:"), &sliderContrast, &valueContrast, origContrast},
    };
    for (auto& spec : specs) {
        HBox* row = NewDlgFieldRow();
        VirtText* label = NewDlgLabel(spec.label, fontText, colMuted, kToolDlgRowDy, true);
        VecAppend(labels, label);
        row->AddChild(label);
        *spec.slider = NewAdjustSlider(this, spec.initial);
        row->AddChild(*spec.slider, 1);
        // wide enough for "-100"
        *spec.value = NewVirtText({.s = StrL("-100"), .font = fontText, .textColor = colMuted, .isRtl = isRtl});
        row->AddChild(*spec.value);
        rows->AddChild(row);
    }
    AlignDlgLabels(labels);
    vbox->AddChild(NewDlgSection(rows));

    auto* btnReset = NewDlgButton(hwnd, Tr("&Reset"), fontText, DlgButtonStyle::Ghost, false);
    btnReset->onClick = MkMethod1<BrightnessContrastWnd, VirtMouseEvent*, &BrightnessContrastWnd::OnReset>(this);
    auto* btnCancel = NewDlgButton(hwnd, Tr("Cancel"), fontText, DlgButtonStyle::Outline, false);
    btnCancel->onClick = MkMethod1<BrightnessContrastWnd, VirtMouseEvent*, &BrightnessContrastWnd::OnCancel>(this);
    auto* btnOk = NewDlgButton(hwnd, Tr("OK"), fontText, DlgButtonStyle::Filled, true);
    btnOk->onClick = MkMethod1<BrightnessContrastWnd, VirtMouseEvent*, &BrightnessContrastWnd::OnOk>(this);
    vbox->AddChild(NewDlgFooter(btnCancel, btnOk, btnReset));
    layout = vbox;

    LayoutAndSizeToContent(layout, DpiScale(kBcDlgDx), 0, hwnd);
    UpdateValues();
    HwndCenterDialog(hwnd, win->hwndFrame);
    UpdateTheme();
    SetIsVisible(true);
    return true;
}

// CmdAdjustBrightness and CmdAdjustContrast both open this dialog
void ShowBrightnessContrastDialog(MainWindow* win) {
    if (!IsMainWindowValidAndNotClosing(win) || !win->CurrentTab() || !win->CurrentTab()->AsFixed()) {
        return;
    }
    if (gBrightnessContrastWnd) {
        HwndSetFocus(gBrightnessContrastWnd->hwnd);
        return;
    }
    auto* wnd = new BrightnessContrastWnd();
    wnd->closeOnEsc = true;
    wnd->onBeforeDelete = MkFunc0Void(ClearBrightnessContrastWnd);
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnDestroy);
    wnd->SetFont(GetAppFont());
    if (!wnd->Create(win)) {
        delete wnd;
        return;
    }
    gBrightnessContrastWnd = wnd;
}
