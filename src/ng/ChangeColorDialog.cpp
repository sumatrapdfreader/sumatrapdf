/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's ChangeColorDialog.cpp paints its own HSV area, swatches and
// checkerboard into a WS_POPUPWINDOW. Here the HSV area is an image element
// with orig's pixmap and mouse listeners, and everything else is elements: the "RGB:" field, the preview swatch,
// the three preset swatches, the two rows of custom swatches, the opacity
// slider, the This file / For all ... radios and Remove / Cancel / OK. The
// model - what the picked color means, where it is written and how the custom
// set is edited and saved - is orig's.

#include "gui/GpuiBridge.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "Annotation.h"
#include "WindowTab.h"
#include "MainWindow.h"
#include "FileHistory.h"
#include "Tabs.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "gui/AppShell.h"
#include "gui/TabsUI.h"
#include "gui/DialogWidgets.h"
#include "gui/ToolWindow.h"
#include "SumatraDialogs.h"
#include "shared/PdfToolsCommon.h"
#include "ChangeColorDialogCommon.h"

#include "SumatraLog.h"

// the client width of orig's window
constexpr float kColorWinDx = 420;

struct ChangeColorDlg {
    MainWindow* win = nullptr;
    bool visible = false;
    WindowTab* tab = nullptr;
    Str filePath;
    // non-null: generic color picker mode, owned
    ChangeColorsArgs* colorsArgs = nullptr;
    bool withOpacity = false;
    bool isCbx = false;
    bool isImage = false;
    bool isEbook = false;

    Color currentColor = 0;
    bool isCheckered = false;
    u8 opacity = 0xff;
    Color customColors[kMaxCustomColors]{};
    int nCustom = 0;
    bool customColorsChanged = false;
    int selectedCustomIdx = -1;
    bool previewSelected = true;
    bool allFiles = false;
    bool updatingEdit = false;
    // orig's EditSetFocus / EditSelectAll when the dialog is shown
    bool wantFocus = false;

    gpui::InputState* editRgb = nullptr;
    gpui::SliderState opacitySlider;
};

static ChangeColorDlg gColor;

struct ChangeColorView {
    static void OnOk(ChangeColorView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnCancel(ChangeColorView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnRemove(ChangeColorView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnEditChanged(ChangeColorView* self, gp::Ctx* cx, const gp::InputEvent* ev);
    static void OnSwatch(ChangeColorView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t id);
    static void OnOpacity(ChangeColorView* self, gp::Ctx* cx, const gp::SliderEvent* ev);
    static void OnTarget(ChangeColorView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t all);
    static void OnAreaDown(ChangeColorView* self, gp::Ctx* cx, const gp::MouseDownEvent* ev);
    static void OnAreaMove(ChangeColorView* self, gp::Ctx* cx, const gp::MouseMoveEvent* ev);
};

static gp::Entity<ChangeColorView> gColorView;

// orig's window, where the platform can have one (DlgWindowOpen): modeless
// and owned by the main window; null: a dialog in the frame
static ToolWindow* gColorTw = nullptr;

// the dialog in the frame; a window of its own is not the frame's business
bool IsChangeColorDialogVisible() {
    return gColor.visible && !gColorTw;
}

static Str ChangeColorDlgTitle() {
    return gColor.colorsArgs ? gColor.colorsArgs->title : Tr("Change Background Color");
}

// orig's LoadColors
static void LoadColors() {
    Vec<Color> colors;
    if (gColor.colorsArgs) {
        colors = gColor.colorsArgs->colors;
    } else if (gSettings) {
        ParseColorList(gSettings->customColors, colors, kMaxCustomColors);
    }
    gColor.nCustom = 0;
    gColor.customColorsChanged = false;
    for (Color col : colors) {
        if (gColor.nCustom >= kMaxCustomColors) {
            break;
        }
        gColor.customColors[gColor.nCustom++] = col;
    }
}

static void SaveCustomColorsIfChanged() {
    if (!gColor.customColorsChanged) {
        return;
    }
    Vec<Color> colors;
    for (int i = 0; i < gColor.nCustom; i++) {
        VecAppend(colors, gColor.customColors[i]);
    }
    SaveCustomColors(colors);
}

// orig's UpdateEditFromColor
static void UpdateEditFromColor() {
    gColor.updatingEdit = true;
    if (gColor.editRgb) {
        Str text = gColor.isCheckered ? (gColor.colorsArgs ? StrL("unset") : StrL("checkered"))
                                      : Str(SerializeColorTemp(gColor.currentColor));
        gp::InputSetValue(gColor.editRgb, ToGpui(text));
    }
    gColor.updatingEdit = false;
    if (gColor.selectedCustomIdx >= 0 && !gColor.isCheckered) {
        gColor.customColors[gColor.selectedCustomIdx] = gColor.currentColor;
        gColor.customColorsChanged = true;
    }
}

static void SyncOpacityFromColor() {
    if (!gColor.withOpacity) {
        return;
    }
    gColor.opacity = gColor.isCheckered ? 0xff : OpacityOf(gColor.currentColor);
    gp::SliderSetValue(&gColor.opacitySlider, gp::SliderSingle((float)gColor.opacity));
}

// orig's SetCustomColor: setting a color on the empty slot defines it
static void SetCustomColor(int idx, Color col) {
    if (idx < 0 || idx >= kMaxCustomColors) {
        return;
    }
    gColor.customColors[idx] = col;
    gColor.customColorsChanged = true;
    if (idx < gColor.nCustom) {
        return;
    }
    gColor.nCustom = idx + 1;
}

static void SelectPreview() {
    gColor.selectedCustomIdx = -1;
    gColor.previewSelected = true;
}

static void RemoveCustom(int idx) {
    if (idx < 0 || idx >= gColor.nCustom) {
        return;
    }
    for (int i = idx; i < gColor.nCustom - 1; i++) {
        gColor.customColors[i] = gColor.customColors[i + 1];
    }
    gColor.nCustom--;
    gColor.customColorsChanged = true;
    SelectPreview();
}

// orig's ClassifyTab
static void ClassifyTab(WindowTab* t) {
    gColor.isCbx = false;
    gColor.isImage = false;
    gColor.isEbook = false;
    if (!t) {
        return;
    }
    auto* engine = t->GetEngine();
    if (!engine) {
        return;
    }
    gColor.isImage = engine->isImageCollection;
    gColor.isCbx = engine->kind == kindEngineComicBooks;
    gColor.isEbook = engine->kind == kindEngineMupdf && !str::EqI(engine->defaultExt, StrL(".pdf"));
}

// orig's LoadCurrentColor
static void LoadCurrentColor() {
    if (gColor.colorsArgs) {
        gColor.currentColor = gColor.colorsArgs->color;
        gColor.isCheckered = (gColor.currentColor == kColorUnset);
        if (gColor.isCheckered) {
            gColor.currentColor = ThemeControlBackgroundColor();
        }
        return;
    }
    WindowTab* t = gColor.tab;
    if (!t) {
        gColor.currentColor = kColWhite;
        gColor.isCheckered = false;
        return;
    }
    if (t->bgColorCheckered) {
        gColor.currentColor = kColorUnset;
        gColor.isCheckered = true;
        return;
    }
    if (t->bgColor != kColorUnset) {
        gColor.currentColor = t->bgColor;
        gColor.isCheckered = false;
        return;
    }
    ParsedColor* bgOverride = nullptr;
    if (gColor.isCbx) {
        bgOverride = GetPrefsColor(gSettings->comicBookUI.windowBgCol);
    } else if (gColor.isImage) {
        bgOverride = GetPrefsColor(gSettings->imageUI.windowBgCol);
    } else if (gColor.isEbook) {
        bgOverride = GetPrefsColor(gSettings->eBookUI.windowBgCol);
    } else {
        bgOverride = GetPrefsColor(gSettings->fixedPageUI.windowBgCol);
    }
    if (bgOverride && bgOverride->parsedOk) {
        gColor.currentColor = bgOverride->col;
        gColor.isCheckered = (bgOverride->col == kColorUnset);
        return;
    }
    Color bg;
    ThemeDocumentColors(bg);
    gColor.currentColor = bg;
    gColor.isCheckered = false;
}

static WindowTab* TargetTab() {
    if (!IsMainWindowValidAndNotClosing(gColor.win)) {
        return nullptr;
    }
    WindowTab* t = FindTabByFilePath(gColor.filePath);
    if (!t || t->win != gColor.win) {
        return nullptr;
    }
    return t;
}

// orig's ApplyBackground
static void ApplyBackground() {
    WindowTab* t = TargetTab();
    if (!t || !t->ctrl) {
        return;
    }
    Str colorStr = gColor.isCheckered ? StrL("checkered") : Str(SerializeColorTemp(gColor.currentColor));
    Color newColor = gColor.isCheckered ? kColorUnset : gColor.currentColor;
    logf("ChangeColorDialog: background '%s', all files %d\n", colorStr, (int)gColor.allFiles);

    if (gColor.allFiles) {
        if (gColor.isCbx) {
            SetColorText(gSettings->comicBookUI.windowBgCol, colorStr);
        } else if (gColor.isImage) {
            SetColorText(gSettings->imageUI.windowBgCol, colorStr);
        } else if (gColor.isEbook) {
            SetColorText(gSettings->eBookUI.windowBgCol, colorStr);
        } else {
            SetColorText(gSettings->fixedPageUI.windowBgCol, colorStr);
        }
        FileState* fs = FileHistoryFindByPath(t->filePath);
        if (fs) {
            SetColorText(fs->bgCol, StrL(""));
        }
        t->bgColor = kColorUnset;
        t->bgColorCheckered = false;
    } else {
        FileState* fs = FileHistoryFindByPath(t->filePath);
        if (fs) {
            SetColorText(fs->bgCol, colorStr);
        }
        t->bgColor = newColor;
        t->bgColorCheckered = gColor.isCheckered;
    }
    ScheduleSaveSettings();
    gColor.win->RedrawAll(true);
}

// hands the picked color and the edited set back to the caller (orig's
// NotifyColorsArgs)
static void NotifyColorsArgs(CloseAction action) {
    if (!gColor.colorsArgs) {
        return;
    }
    ChangeColorsArgs* args = gColor.colorsArgs;
    gColor.colorsArgs = nullptr;
    VecClear(args->colors);
    for (int i = 0; i < gColor.nCustom; i++) {
        VecAppend(args->colors, gColor.customColors[i]);
    }
    args->color = gColor.isCheckered ? kColorUnset : gColor.currentColor;
    args->didSelect = (action == CloseAction::Select);
    args->colorsChanged = gColor.customColorsChanged;
    args->onClose.Call(args);
    delete args;
}

static void FinishColorDialog(CloseAction action) {
    if (!gColor.visible) {
        return;
    }
    gColor.visible = false;
    // The edit lives in this window. Blur it here so close does not use it
    // after it is freed.
    gp::Window* dlg = ToolWindowGpui(gColorTw);
    if (dlg && gColor.editRgb) {
        gp::InputBlur(gColor.editRgb, dlg->app, dlg);
    }
    DlgWindowClose(&gColorTw);
    // its window can outlive the main window by a moment (the layer tells
    // it later): nothing of a closed main window is touched
    if (!IsMainWindowValid(gColor.win)) {
        gColor.win = nullptr;
    }
    MainWindow* win = gColor.win;
    if (gColor.colorsArgs) {
        NotifyColorsArgs(action);
    } else {
        SaveCustomColorsIfChanged();
        if (action == CloseAction::Select) {
            ApplyBackground();
        }
    }
    if (!dlg && win && win->gpuiWin && gColor.editRgb) {
        gp::InputBlur(gColor.editRgb, win->gpuiWin->app, win->gpuiWin);
    }
    delete gColor.editRgb;
    gColor.editRgb = nullptr;
    str::Free(gColor.filePath);
    gColor.filePath = {};
    AppShellInvalidate(win);
}

void CloseChangeColorDialog() {
    FinishColorDialog(CloseAction::Cancel);
}

// orig keeps its window when it is asked for another color (SetTargetColors /
// SetTargetBackground): the caller of the one that was up is told "cancel"
// and the window stays where it is. True when the window of `win` is reused
static bool RetargetColorWindow(MainWindow* win) {
    if (!gColor.visible || !gColorTw || gColor.win != win) {
        CloseChangeColorDialog();
        return false;
    }
    NotifyColorsArgs(CloseAction::Cancel);
    return true;
}

static void OpenColorDialog(MainWindow* win, bool reuse) {
    gColor.win = win;
    gp::App* app = win && win->gpuiWin ? win->gpuiWin->app : nullptr;
    if (!reuse) {
        auto* s = new gp::InputState();
        s->focus = gp::FocusHandleNew(app);
        gColor.editRgb = s;
    }
    gColor.opacitySlider = gp::SliderStateNew(0, 255, gp::SliderSingle((float)gColor.opacity), 1);
    gColor.selectedCustomIdx = -1;
    gColor.previewSelected = true;
    LoadColors();
    SyncOpacityFromColor();
    UpdateEditFromColor();
    gColor.visible = true;
    gColor.wantFocus = true;
    if (reuse) {
        ToolWindowActivate(gColorTw);
        AppShellInvalidate(win);
        return;
    }
    DlgWindowSpec spec;
    spec.name = "changecolor";
    spec.title = ChangeColorDlgTitle;
    // orig's is modeless and owned by the main window
    spec.modal = false;
    spec.owned = true;
    spec.build = ChangeColorDialogBuild;
    spec.close = CloseChangeColorDialog;
    spec.clientDx = kColorWinDx;
    gColorTw = DlgWindowOpen(spec, win);
    AppShellInvalidate(win);
}

void ShowChangeBackgroundColorDialog(MainWindow* win) {
    if (!IsMainWindowValidAndNotClosing(win) || !win->CurrentTab() || !win->CurrentTab()->ctrl) {
        return;
    }
    bool reuse = RetargetColorWindow(win);
    gColor.colorsArgs = nullptr;
    gColor.withOpacity = false;
    gColor.tab = win->CurrentTab();
    str::ReplaceWithCopy(&gColor.filePath, gColor.tab->filePath);
    ClassifyTab(gColor.tab);
    gColor.allFiles = false;
    LoadCurrentColor();
    OpenColorDialog(win, reuse);
}

void ShowChangeColorsDialog(ChangeColorsArgs* args) {
    if (!IsMainWindowValidAndNotClosing(args->win)) {
        delete args;
        return;
    }
    bool reuse = RetargetColorWindow(args->win);
    gColor.colorsArgs = args;
    gColor.withOpacity = args->withOpacity;
    gColor.tab = nullptr;
    str::ReplaceWithCopy(&gColor.filePath, Str{});
    ClassifyTab(nullptr);
    LoadCurrentColor();
    OpenColorDialog(args->win, reuse);
}

void ChangeColorView::OnOk(ChangeColorView*, gp::Ctx* cx, const gp::ClickEvent*) {
    FinishColorDialog(CloseAction::Select);
    gp::Notify(cx);
}

void ChangeColorView::OnCancel(ChangeColorView*, gp::Ctx* cx, const gp::ClickEvent*) {
    FinishColorDialog(CloseAction::Cancel);
    gp::Notify(cx);
}

void ChangeColorView::OnRemove(ChangeColorView*, gp::Ctx* cx, const gp::ClickEvent*) {
    RemoveCustom(gColor.selectedCustomIdx);
    gp::Notify(cx);
    AppShellInvalidate(gColor.win);
}

// orig's OnEditChanged / TryParseEdit
void ChangeColorView::OnEditChanged(ChangeColorView*, gp::Ctx* cx, const gp::InputEvent* ev) {
    if (ev->kind != gp::InputEventKind::Change || gColor.updatingEdit) {
        return;
    }
    TempStr text = str::DupTemp(FromGpui(gp::InputValue(gColor.editRgb)));
    if (len(text) == 0) {
        return;
    }
    ParsedColor parsed;
    ParseColor(parsed, text);
    if (!parsed.parsedOk) {
        return;
    }
    if (parsed.col == kColorUnset) {
        gColor.isCheckered = true;
    } else {
        gColor.isCheckered = false;
        gColor.currentColor = parsed.col;
    }
    SyncOpacityFromColor();
    if (gColor.selectedCustomIdx >= 0 && !gColor.isCheckered) {
        SetCustomColor(gColor.selectedCustomIdx, gColor.currentColor);
    }
    gp::Notify(cx);
    AppShellInvalidate(gColor.win);
}

// orig's OnSwatchClick
void ChangeColorView::OnSwatch(ChangeColorView*, gp::Ctx* cx, const gp::ClickEvent* ev, int64_t idIn) {
    int id = (int)idIn;
    if (ev->button == gp::MouseButton::Right && id >= kIdCustom0 && id < kIdCustom0 + kMaxCustomColors) {
        RemoveCustom(id - kIdCustom0);
        gp::Notify(cx);
        AppShellInvalidate(gColor.win);
        return;
    }
    if (id == kIdPreview) {
        SelectPreview();
    } else if (id >= kIdPreset0 && id < kIdPreset0 + kNumPresets) {
        Color col = kBgPresetColors[id - kIdPreset0];
        if (col == kColorUnset) {
            gColor.isCheckered = true;
        } else {
            gColor.isCheckered = false;
            gColor.currentColor = col;
        }
        SelectPreview();
        SyncOpacityFromColor();
        UpdateEditFromColor();
    } else if (id >= kIdCustom0 && id < kIdCustom0 + kMaxCustomColors) {
        int idx = id - kIdCustom0;
        if (idx > gColor.nCustom) {
            return;
        }
        if (gColor.selectedCustomIdx == idx) {
            SelectPreview();
        } else {
            gColor.selectedCustomIdx = idx;
            gColor.previewSelected = false;
            if (idx < gColor.nCustom) {
                gColor.isCheckered = false;
                gColor.currentColor = gColor.customColors[idx];
                SyncOpacityFromColor();
                UpdateEditFromColor();
            } else {
                // the empty slot: the current color defines it
                SetCustomColor(idx, gColor.currentColor);
            }
        }
    }
    gp::Notify(cx);
    AppShellInvalidate(gColor.win);
}

void ChangeColorView::OnOpacity(ChangeColorView*, gp::Ctx* cx, const gp::SliderEvent* ev) {
    gColor.opacity = (u8)limitValue((int)ev->value.End(), 0, 255);
    if (!gColor.isCheckered) {
        gColor.currentColor = WithAlpha(gColor.currentColor, gColor.opacity);
        UpdateEditFromColor();
    }
    gp::Notify(cx);
    AppShellInvalidate(gColor.win);
}

void ChangeColorView::OnTarget(ChangeColorView*, gp::Ctx* cx, const gp::ClickEvent*, int64_t all) {
    gColor.allFiles = all != 0;
    gp::Notify(cx);
    AppShellInvalidate(gColor.win);
}

// --- orig's inline HSV area ---------------------------------------------------

// hue left to right, value top (full) to bottom (black), saturation 1
constexpr int kColorAreaDx = 380;
constexpr int kColorAreaDy = 150;

// orig's MakeHsvPixmap. ng: made once for the process for each width the
// area has (the frame's dialog, orig's window), at the area's inner size in
// dips; gpui scales it to the element and keeps its proportions
struct HsvImage {
    int areaDx = 0;
    u8* pixels = nullptr;
    gp::RenderImage* image = nullptr;
};
static HsvImage gHsvImages[2];

static gp::ImageLoadState HsvImageLoad(gp::PaintApp* pa, void* user, gp::RenderImage** imgOut) {
    auto* hsv = (HsvImage*)user;
    u8*& gHsvPixels = hsv->pixels;
    gp::RenderImage*& gHsvImage = hsv->image;
    int w = hsv->areaDx - 2;
    int h = kColorAreaDy - 2;
    if (!gHsvPixels) {
        gHsvPixels = AllocArray<u8>((size_t)w * (size_t)h * 4);
        for (int y = 0; gHsvPixels && y < h; y++) {
            float val = 1.0f - ((float)y / (float)h);
            u8* row = gHsvPixels + ((size_t)y * (size_t)w * 4);
            for (int x = 0; x < w; x++) {
                float hue = (float)x / (float)w * 360.0f;
                u8 r, g, b;
                HsvToRgb(hue, 1.0f, val, r, g, b);
                row[(size_t)x * 4] = b;
                row[(x * 4) + 1] = g;
                row[(x * 4) + 2] = r;
                row[(x * 4) + 3] = 255;
            }
        }
    }
    if (!gHsvImage && gHsvPixels) {
        gHsvImage = gp::RenderImageFromBgra(pa, gHsvPixels, w, h);
    }
    *imgOut = gHsvImage;
    return gHsvImage ? gp::ImageLoadState::Ready : gp::ImageLoadState::Failed;
}

// orig's PickFromArea; `local` is relative to the area, its 1 px frame included
static void PickFromArea(float localX, float localY, float areaDx, float areaDy) {
    float dx = areaDx - 2;
    float dy = areaDy - 2;
    if (dx <= 0 || dy <= 0) {
        return;
    }
    float x = limitValue(localX - 1, 0.f, dx - 1);
    float y = limitValue(localY - 1, 0.f, dy - 1);
    float hue = x / dx * 360.0f;
    float val = 1.0f - (y / dy);
    u8 cr, cg, cb;
    HsvToRgb(hue, 1.0f, val, cr, cg, cb);
    gColor.isCheckered = false;
    gColor.currentColor = gColor.withOpacity ? WithAlpha(MkRgb(cr, cg, cb), gColor.opacity) : MkRgb(cr, cg, cb);
    UpdateEditFromColor();
}

void ChangeColorView::OnAreaDown(ChangeColorView*, gp::Ctx* cx, const gp::MouseDownEvent* ev) {
    if (ev->button != gp::MouseButton::Left) {
        return;
    }
    PickFromArea(ev->x - ev->el.x, ev->y - ev->el.y, ev->el.w, ev->el.h);
    gp::Notify(cx);
    gp::AppInvalidate(cx->win);
}

// orig's OnAreaMouseMove: a drag with the button down keeps picking. ng: the
// Windows backend does not set MouseMoveEvent::pressed; the window knows
void ChangeColorView::OnAreaMove(ChangeColorView*, gp::Ctx* cx, const gp::MouseMoveEvent* ev) {
    bool pressed = ev->pressed || (cx->win && cx->win->mouseDown && cx->win->pressedButton == gp::MouseButton::Left);
    if (!pressed) {
        return;
    }
    PickFromArea(ev->x - ev->el.x, ev->y - ev->el.y, ev->el.w, ev->el.h);
    gp::Notify(cx);
    gp::AppInvalidate(cx->win);
}

static gp::El* ColorAreaEl(gp::Ctx* cx, float areaDx = (float)kColorAreaDx) {
    HsvImage* hsv = &gHsvImages[(int)areaDx == kColorAreaDx ? 0 : 1];
    hsv->areaDx = (int)areaDx;
    gp::ImageSource src = gp::ImageSource::FromCustom(HsvImageLoad, hsv);
    return gp::Div(cx->a)
        ->W(areaDx)
        ->H((float)kColorAreaDy)
        ->Shrink0()
        ->Border(1, ToGpui(ThemeEdgeColor()))
        ->Cursor(gp::CursorKind::Crosshair)
        ->PathClick(GStrL("color-area"))
        ->OnMouseDown(gp::ListenTo(gColorView, &ChangeColorView::OnAreaDown))
        ->OnMouseMove(gp::ListenTo(gColorView, &ChangeColorView::OnAreaMove))
        ->Child(gp::ImageEl(cx->a, src, GStrL(""))->W(areaDx - 2)->H((float)kColorAreaDy - 2));
}

static Str AllFilesLabel() {
    if (gColor.isCbx) {
        return Tr("For all &comic books");
    }
    if (gColor.isImage) {
        return Tr("For all &images");
    }
    if (gColor.isEbook) {
        return Tr("For all &ebooks");
    }
    return Tr("For all &PDF files");
}

// --- orig's layout, for the dialog in a window of its own -------------------

// orig's layout: the area, the opacity and the color as text, the swatches;
// this file or all of them; the footer; hairlines between
constexpr float kColorWinPreviewDx = 42;
constexpr float kColorWinPreviewDy = 20;
constexpr float kColorWinSwatchDx = 36;
constexpr float kColorWinSwatchDy = 22;
constexpr float kColorWinSwatchGap = 6;
constexpr float kColorWinRgbDx = 100;

struct WinSwatchPaint {
    Color col = 0;
    bool checkered = false;
    bool empty = false;
    bool selected = false;
};

// the preview, the presets and the custom colors
static WinSwatchPaint gWinSwatches[1 + kNumPresets + kMaxCustomColors];

static void PaintWinChecker(gp::PaintCtx* ctx, gp::Bounds rc, Color light, Color dark) {
    constexpr int kCheckerSize = 8;
    for (int cy = 0; cy < (int)rc.h; cy += kCheckerSize) {
        for (int cxx = 0; cxx < (int)rc.w; cxx += kCheckerSize) {
            float cellW = (float)std::min(kCheckerSize, (int)rc.w - cxx);
            float cellH = (float)std::min(kCheckerSize, (int)rc.h - cy);
            bool isDark = ((cxx / kCheckerSize) + (cy / kCheckerSize)) % 2 != 0;
            gp::CanvasFillRect(ctx, rc.x + (float)cxx, rc.y + (float)cy, cellW, cellH, ToGpui(isDark ? dark : light));
        }
    }
}

// orig's PaintSwatch
static void PaintWinSwatch(gp::PaintCtx* ctx, gp::El* e, void* user) {
    auto* p = (WinSwatchPaint*)user;
    gp::Bounds rc = e->Bounds();
    if (p->selected) {
        constexpr float kSelEdge = 3;
        gp::CanvasFillRect(ctx, rc.x, rc.y, rc.w, rc.h, ToGpui(SysHighlightBgColor()));
        rc = {rc.x + kSelEdge, rc.y + kSelEdge, rc.w - 2 * kSelEdge, rc.h - 2 * kSelEdge};
    }
    if (p->empty) {
        gp::Rgba edge = ToGpui(ThemeEdgeColor());
        gp::CanvasFillRect(ctx, rc.x, rc.y, rc.w, rc.h, ToGpui(ThemeWindowControlBackgroundColor()));
        gp::CanvasStrokeRound(ctx, rc.x + 0.5f, rc.y + 0.5f, rc.w - 1, rc.h - 1, 0, 1, edge);
        gp::CanvasLine(ctx, rc.x, rc.y, rc.x + rc.w - 1, rc.y + rc.h - 1, 1, edge, nullptr);
        gp::CanvasLine(ctx, rc.x + rc.w - 1, rc.y, rc.x, rc.y + rc.h - 1, 1, edge, nullptr);
        return;
    }
    if (p->checkered) {
        PaintWinChecker(ctx, rc, kColWhite, kColCheckerDark);
        return;
    }
    u8 a = GetAlpha(p->col);
    if (gColor.withOpacity && a != 0 && a != 0xff) {
        // the color over a checkerboard, so that opacity shows
        PaintWinChecker(ctx, rc, BlendOver(p->col, kColWhite, a), BlendOver(p->col, kColCheckerDark, a));
        return;
    }
    gp::Rgba c = ToGpui(p->col);
    c.a = 255;
    gp::CanvasFillRect(ctx, rc.x, rc.y, rc.w, rc.h, c);
}

static gp::El* WinSwatchEl(gp::Ctx* cx, Str id, int slot, int swatchId, Color col, bool checkered, bool empty,
                           bool selected, float w, float h) {
    WinSwatchPaint* p = &gWinSwatches[slot];
    p->col = col;
    p->checkered = checkered;
    p->empty = empty;
    p->selected = selected;
    gp::El* box = gp::Div(cx->a)
                      ->W(w)
                      ->H(h)
                      ->Shrink0()
                      ->Cursor(gp::CursorKind::Pointer)
                      ->PathClick(GpuiDup(cx->a, id))
                      ->OnClick(gp::ListenTo(gColorView, &ChangeColorView::OnSwatch, (intptr_t)swatchId));
    box->customPaint = &PaintWinSwatch;
    box->customUser = p;
    return box;
}

gp::El* ChangeColorDialogBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gColor.visible || gColor.win != win) {
        return nullptr;
    }
    if (gColorTw && !DlgWindowIsHost(cx)) {
        return nullptr;
    }
    if (!gColorView.IsValid()) {
        gColorView = gp::EntityNewState<ChangeColorView>(cx->app);
    }
    gColor.editRgb->onChange = gp::ListenTo(gColorView, &ChangeColorView::OnEditChanged);
    // ng: Slider::IntoEl() replaces the state's listener with the component's
    gColor.opacitySlider.onChange = gp::ListenTo(gColorView, &ChangeColorView::OnOpacity);
    gp::Listener onOk = gp::ListenTo(gColorView, &ChangeColorView::OnOk);
    gp::Listener onCancel = gp::ListenTo(gColorView, &ChangeColorView::OnCancel);
    DlgSetDefault(cx, onOk, true);
    Color colMuted = ThemeWindowTextDisabledColor();

    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill);
    gp::El* picker = DlgSection(cx);
    picker->Child(
        gp::Div(cx->a)->PadY(kToolDlgFilesPadDy)->Shrink0()->Child(ColorAreaEl(cx, kColorWinDx - 2 * kToolDlgPadDx)));

    // the opacity, then the color as text with its preview; labels in a column
    float labelDx = DlgFieldLabelDx(cx, Tr("RGB:"));
    if (gColor.withOpacity) {
        labelDx = std::max(labelDx, DlgFieldLabelDx(cx, Tr("Opacity:")));
        gp::El* row = DlgFieldRow(cx);
        row->Child(DlgFieldLabel(cx, Tr("Opacity:"), labelDx));
        row->Child(
            gp::Div(cx->a)->Flex1()->MinW(0)->Child(gpc::Slider::New(cx, GStrL("color-opacity"), &gColor.opacitySlider)
                                                        ->OnChange(gColor.opacitySlider.onChange)
                                                        ->W(gp::kFill)
                                                        ->IntoEl()));
        row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, fmt("%d", (int)gColor.opacity)))
                       ->Font(kToolDlgTextFontPx)
                       ->Fg(ToGpui(colMuted))
                       ->Shrink0());
        picker->Child(row);
    }
    gp::El* rgbRow = DlgFieldRow(cx);
    rgbRow->Child(DlgFieldLabel(cx, Tr("RGB:"), labelDx));
    rgbRow->Child(
        gp::Div(cx->a)
            ->W(kColorWinRgbDx)
            ->Shrink0()
            ->Child(DlgUnderlineInput(cx, GStrL("color-rgb"), gColor.editRgb, kToolDlgTextFontPx, ThemeEdgeColor())));
    rgbRow->Child(WinSwatchEl(cx, StrL("color-preview"), 0, kIdPreview, gColor.currentColor, gColor.isCheckered, false,
                              gColor.previewSelected, kColorWinPreviewDx, kColorWinPreviewDy));
    picker->Child(rgbRow);

    // only the defined colors plus a single empty slot are shown (orig's
    // UpdateSwatchVis)
    int nVisible = gColor.nCustom < kMaxCustomColors ? gColor.nCustom + 1 : kMaxCustomColors;
    auto custom = [&](int i) {
        bool empty = i >= gColor.nCustom;
        return WinSwatchEl(cx, fmt("color-custom-%d", i), 1 + kNumPresets + i, kIdCustom0 + i, gColor.customColors[i],
                           false, empty, i == gColor.selectedCustomIdx, kColorWinSwatchDx, kColorWinSwatchDy);
    };
    auto swatchRow = [&](float padT) {
        return gp::Div(cx->a)->FlexRow()->ItemsCenter()->PadT(padT)->Gap(kColorWinSwatchGap)->Shrink0();
    };
    gp::El* row1 = swatchRow(kToolDlgFilesPadDy);
    for (int i = 0; i < kNumPresets; i++) {
        Color c = kBgPresetColors[i];
        row1->Child(WinSwatchEl(cx, fmt("color-preset-%d", i), 1 + i, kIdPreset0 + i, c, c == kColorUnset, false, false,
                                kColorWinSwatchDx, kColorWinSwatchDy));
    }
    for (int i = 0; i < kCustomInRow1 && i < nVisible; i++) {
        row1->Child(custom(i));
    }
    picker->Child(row1);
    if (nVisible > kCustomInRow1) {
        gp::El* row2 = swatchRow(kColorWinSwatchGap);
        for (int i = kCustomInRow1; i < nVisible; i++) {
            row2->Child(custom(i));
        }
        picker->Child(row2);
    }
    col->Child(picker);

    // this file or every file of its kind (the background only)
    if (!gColor.colorsArgs) {
        col->Child(DlgHairline(cx));
        gp::El* target = DlgSection(cx);
        target->Child(DlgFieldRow(cx)
                          ->Child(DlgCheck(cx, StrL("color-thisfile"), Tr("&This file"), !gColor.allFiles,
                                           gp::ListenTo(gColorView, &ChangeColorView::OnTarget, (intptr_t)0),
                                           DlgCheckKind::Radio))
                          ->Child(DlgCheck(cx, StrL("color-allfiles"), AllFilesLabel(), gColor.allFiles,
                                           gp::ListenTo(gColorView, &ChangeColorView::OnTarget, (intptr_t)1),
                                           DlgCheckKind::Radio)));
        col->Child(target);
    }

    bool canRemove = gColor.selectedCustomIdx >= 0 && gColor.selectedCustomIdx < gColor.nCustom;
    gp::El* remove = DlgGhostButton(cx, GStrL("color-remove"), Tr("Remove"),
                                    gp::ListenTo(gColorView, &ChangeColorView::OnRemove), !canRemove);
    col->Child(DlgFooter(cx, gColor.colorsArgs ? Tr("Select") : Tr("OK"), onOk, onCancel, remove));
    gp::El* content = DlgToolContent(cx, col, ChangeColorDlgTitle(), kColorWinDx, onCancel);

    if (gColor.wantFocus) {
        gp::InputFocus(gColor.editRgb, cx->app, cx->win);
        gp::InputSelectAll(gColor.editRgb, cx->app, cx->win);
        gColor.wantFocus = cx->win->input != gColor.editRgb;
    }
    return content;
}

// --- the tab color, orig's ShowSetTabColorDialog ----------------------------

void TabColorPicked(TabColorTarget* target, ChangeColorsArgs* args) {
    if (args->colorsChanged) {
        SaveCustomColors(args->colors);
    }
    WindowTab* tab = FindTabByFilePath(target->filePath);
    bool tabValid = IsMainWindowValidAndNotClosing(target->win) && tab && tab->win == target->win && tab->ctrl;
    if (args->didSelect && tabValid) {
        tab->tabColor = args->color;
        FileState* fs = FileHistoryFindByPath(tab->filePath);
        if (fs) {
            bool isUnset = (args->color == kColorUnset);
            SetColorText(fs->tabCol, isUnset ? StrL("") : SerializeColorTemp(args->color));
        }
        logf("ChangeColorDialog: tab color for '%s'\n", tab->filePath);
        ScheduleSaveSettings();
        target->win->RedrawAll(true);
    }
    str::Free(target->filePath);
    delete target;
}
