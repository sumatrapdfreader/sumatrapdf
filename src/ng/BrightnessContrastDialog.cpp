/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's BrightnessContrastDialog.cpp is a WS_POPUPWINDOW with two
// VirtSliders. Here they are gpui sliders in the same layout. The labels'
// access keys focus the sliders; a focused slider steps with the arrow keys.

#include "gui/GpuiBridge.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "WindowTab.h"
#include "MainWindow.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "gui/AppShell.h"
#include "gui/DialogWidgets.h"
#include "gui/ToolWindow.h"
#include "SumatraDialogs.h"
#include "shared/PdfToolsCommon.h"
#include "BrightnessContrastDialogCommon.h"

#include "SumatraLog.h"

// the client width of orig's window
constexpr float kBcWinDx = 380;

struct BrightnessContrastDlg {
    MainWindow* win = nullptr;
    bool visible = false;
    Str filePath;
    // what the file had when the dialog opened, restored by Cancel
    int origBrightness = 0;
    int origContrast = 0;
    int brightness = 0;
    int contrast = 0;
    gpui::SliderState sliderBrightness;
    gpui::SliderState sliderContrast;
};

static BrightnessContrastDlg gBc;

struct BrightnessContrastView {
    static void OnOk(BrightnessContrastView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnCancel(BrightnessContrastView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnReset(BrightnessContrastView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnBrightness(BrightnessContrastView* self, gp::Ctx* cx, const gp::SliderEvent* ev);
    static void OnContrast(BrightnessContrastView* self, gp::Ctx* cx, const gp::SliderEvent* ev);
};

static gp::Entity<BrightnessContrastView> gBcView;

// orig's window, where the platform can have one (DlgWindowOpen): modeless
// and owned by the main window; null: a dialog in the frame
static ToolWindow* gBcTw = nullptr;

// the dialog in the frame; a window of its own is not the frame's business
bool IsBrightnessContrastDialogVisible() {
    return gBc.visible && !gBcTw;
}

static Str BrightnessContrastTitle() {
    return Tr("Brightness / Contrast");
}

static WindowTab* TargetTab() {
    if (!IsMainWindowValidAndNotClosing(gBc.win)) {
        return nullptr;
    }
    return FindTabByFilePath(gBc.filePath, gBc.win);
}

static void Finish(PageAdjustSave save) {
    if (!gBc.visible) {
        return;
    }
    gBc.visible = false;
    if (save == PageAdjustSave::Remember) {
        SetTabPageAdjust(TargetTab(), gBc.brightness, gBc.contrast, save);
    } else {
        SetTabPageAdjust(TargetTab(), gBc.origBrightness, gBc.origContrast, save);
    }
    MainWindow* win = gBc.win;
    if (gBcTw) {
        DlgWindowClose(&gBcTw);
    }
    str::Free(gBc.filePath);
    gBc.filePath = {};
    AppShellInvalidate(win);
}

void CloseBrightnessContrastDialog() {
    Finish(PageAdjustSave::Preview);
}

static void Preview(gp::Ctx* cx) {
    SetTabPageAdjust(TargetTab(), gBc.brightness, gBc.contrast, PageAdjustSave::Preview);
    gp::Notify(cx);
    AppShellInvalidate(gBc.win);
}

void BrightnessContrastView::OnOk(BrightnessContrastView*, gp::Ctx* cx, const gp::ClickEvent*) {
    Finish(PageAdjustSave::Remember);
    gp::Notify(cx);
}

void BrightnessContrastView::OnCancel(BrightnessContrastView*, gp::Ctx* cx, const gp::ClickEvent*) {
    CloseBrightnessContrastDialog();
    gp::Notify(cx);
}

void BrightnessContrastView::OnReset(BrightnessContrastView*, gp::Ctx* cx, const gp::ClickEvent*) {
    gBc.brightness = 0;
    gBc.contrast = 0;
    gp::SliderSetValue(&gBc.sliderBrightness, gp::SliderSingle(0));
    gp::SliderSetValue(&gBc.sliderContrast, gp::SliderSingle(0));
    Preview(cx);
}

void BrightnessContrastView::OnBrightness(BrightnessContrastView*, gp::Ctx* cx, const gp::SliderEvent* ev) {
    gBc.brightness = limitValue((int)lroundf(ev->value.End()), kPageAdjustMin, kPageAdjustMax);
    Preview(cx);
}

void BrightnessContrastView::OnContrast(BrightnessContrastView*, gp::Ctx* cx, const gp::SliderEvent* ev) {
    gBc.contrast = limitValue((int)lroundf(ev->value.End()), kPageAdjustMin, kPageAdjustMax);
    Preview(cx);
}

static gp::SliderState NewAdjustSlider(int value) {
    return gp::SliderStateNew(kPageAdjustMin, kPageAdjustMax, gp::SliderSingle((float)value), 1);
}

// CmdAdjustBrightness and CmdAdjustContrast both open this dialog
void ShowBrightnessContrastDialog(MainWindow* win) {
    if (!IsMainWindowValidAndNotClosing(win) || !win->CurrentTab() || !win->CurrentTab()->AsFixed()) {
        return;
    }
    if (gBc.visible && gBcTw) {
        ToolWindowActivate(gBcTw);
        return;
    }
    CloseBrightnessContrastDialog();
    DisplayModel* dm = win->CurrentTab()->AsFixed();
    gBc.win = win;
    gBc.filePath = str::Dup(win->CurrentTab()->filePath);
    gBc.origBrightness = gBc.brightness = dm->brightness;
    gBc.origContrast = gBc.contrast = dm->contrast;
    gBc.sliderBrightness = NewAdjustSlider(gBc.brightness);
    gBc.sliderContrast = NewAdjustSlider(gBc.contrast);
    gBc.visible = true;
    DlgWindowSpec spec;
    spec.name = "brightnesscontrast";
    spec.title = BrightnessContrastTitle;
    // orig's is modeless and owned by the main window
    spec.modal = false;
    spec.owned = true;
    spec.build = BrightnessContrastDialogBuild;
    spec.close = CloseBrightnessContrastDialog;
    spec.clientDx = kBcWinDx;
    gBcTw = DlgWindowOpen(spec, win);
    AppShellInvalidate(win);
}

// label, slider, value; the label's access key focuses the slider
static gp::El* AdjustRow(gp::Ctx* cx, Str label, float labelDx, gp::Str id, gp::SliderState* slider,
                         gp::Listener onChange, int value) {
    gp::Str text = DlgAccelFocus(cx, label, gp::HashClickId(id));
    gp::El* row = DlgFieldRow(cx);
    row->Child(gp::Div(cx->a)->Shrink0()->W(labelDx)->Child(
        DlgAccelText(cx, text)->Font(kToolDlgTextFontPx)->Fg(ToGpui(ThemeWindowTextDisabledColor()))));
    // ng: Slider::IntoEl() replaces the state's listener with the component's
    slider->onChange = onChange;
    row->Child(gp::Div(cx->a)->Flex1()->MinW(0)->Child(
        gpc::Slider::New(cx, id, slider)->OnChange(onChange)->W(gp::kFill)->IntoEl()));
    // wide enough for "-100"
    row->Child(gp::Div(cx->a)
                   ->Shrink0()
                   ->W(DlgFieldLabelDx(cx, StrL("-100")))
                   ->Child(gp::TextEl(cx->a, GpuiDup(cx->a, fmt("%d", value)))
                               ->Font(kToolDlgTextFontPx)
                               ->Fg(ToGpui(ThemeWindowTextDisabledColor()))));
    return row;
}

//   Brightness:  ------o------   0
//   Contrast:    ------o------   0
//   Reset                 [Cancel] [OK]
gp::El* BrightnessContrastDialogBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gBc.visible || gBc.win != win) {
        return nullptr;
    }
    if (gBcTw && !DlgWindowIsHost(cx)) {
        return nullptr;
    }
    if (!gBcView.IsValid()) {
        gBcView = gp::EntityNewState<BrightnessContrastView>(cx->app);
    }
    gp::Listener onOk = gp::ListenTo(gBcView, &BrightnessContrastView::OnOk);
    gp::Listener onCancel = gp::ListenTo(gBcView, &BrightnessContrastView::OnCancel);
    DlgSetDefault(cx, onOk, true);

    Str lblBrightness = Tr("&Brightness:");
    Str lblContrast = Tr("&Contrast:");
    float labelDx = std::max(DlgFieldLabelDx(cx, lblBrightness), DlgFieldLabelDx(cx, lblContrast));
    gp::El* rows = DlgSection(cx);
    rows->Child(AdjustRow(cx, lblBrightness, labelDx, GStrL("bc-brightness"), &gBc.sliderBrightness,
                          gp::ListenTo(gBcView, &BrightnessContrastView::OnBrightness), gBc.brightness));
    rows->Child(AdjustRow(cx, lblContrast, labelDx, GStrL("bc-contrast"), &gBc.sliderContrast,
                          gp::ListenTo(gBcView, &BrightnessContrastView::OnContrast), gBc.contrast));

    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill);
    col->Child(rows);
    gp::El* reset =
        DlgGhostButton(cx, GStrL("bc-reset"), Tr("&Reset"), gp::ListenTo(gBcView, &BrightnessContrastView::OnReset));
    col->Child(DlgFooter(cx, Tr("OK"), onOk, onCancel, reset));
    return DlgToolContent(cx, col, BrightnessContrastTitle(), kBcWinDx, onCancel);
}
