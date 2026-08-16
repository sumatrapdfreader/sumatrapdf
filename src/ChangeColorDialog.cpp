/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"
#include "gui/Dpi.h"
#include "base/Pixmap.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"
#include "gui/win/TabsCtrl.h"

#include "Settings.h"
#include "AppSettings.h"
#include "GlobalPrefs.h"
#include "DocController.h"
#include "EngineBase.h"
#include "Annotation.h"
#include "WindowTab.h"
#include "MainWindow.h"
#include "FileHistory.h"
#include "Tabs.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "DarkMode_win.h"
#include "ChangeColorDialog.h"

static const int kMaxCustomColors = 13;
static const int kNumPresets = 3;
static const Color kBgPresetColors[] = {
    kColorUnset,
    kColBlack,
    kColWhite,
};

static const int kIdPreview = 1;
static const int kIdPreset0 = 10;
static const int kIdCustom0 = 20;

// HSV picker, hex edit, swatches and OK/Cancel. Same WindowBase layout as
// Settings. Used for both Change Background Color and Change Tab Color.
struct ChangeColorWnd : WindowBase {
    ~ChangeColorWnd() override;

    MainWindow* win = nullptr;
    WindowTab* tab = nullptr;
    Str filePath;
    bool forTabColor = false;
    bool isCbx = false;
    bool isImage = false;
    bool isEbook = false;

    Color currentColor = 0;
    bool isCheckered = false;
    Color customColors[kMaxCustomColors]{};
    bool customColorSet[kMaxCustomColors]{};
    bool customColorsChanged = false;
    int selectedCustomIdx = -1;
    bool previewSelected = true;
    bool updatingEdit = false;

    Pixmap* hsvPx = nullptr;
    VirtCustom* colorArea = nullptr;
    VirtText* labelRgb = nullptr;
    Edit* editRgb = nullptr;
    VirtCustom* swatchPreview = nullptr;
    VirtCustom* swatchPreset[kNumPresets]{};
    VirtCustom* swatchCustom[kMaxCustomColors]{};
    Checkbox* radioThisFile = nullptr;
    Checkbox* radioAllFiles = nullptr;
    VirtButton* btnCancel = nullptr;
    VirtButton* btnOk = nullptr;

    bool Create(MainWindow* win);
    void SetTargetBackground(MainWindow* win);
    void SetTargetTab(MainWindow* win, WindowTab* tab);
    void ClassifyTab(WindowTab* tab);
    void LoadCurrentColor();
    void ParseCustomColors();
    void SaveCustomColorsIfChanged();
    void UpdateEditFromColor();
    bool TryParseEdit();
    void SelectPreview();
    void SelectCustom(int idx);
    void InvalidateSwatches();
    void PickFromArea(Point ptLocal);
    void OnAreaMouse(VirtMouseEvent* ev);
    void OnSwatchClick(VirtMouseEvent* ev);
    void OnSwatchContext(VirtMouseEvent* ev);
    void OnEditChanged();
    void RelayoutRadios();

    void OnCancel(VirtMouseEvent* ev = nullptr);
    void OnOk(VirtMouseEvent* ev = nullptr);
    void ApplyBackground();
    void ApplyTabColor();
    WindowTab* TargetTab();
};

static ChangeColorWnd* gChangeColorWnd = nullptr;

ChangeColorWnd::~ChangeColorWnd() {
    str::Free(filePath);
    FreePixmap(hsvPx);
}

static void ClearChangeColorWnd() {
    gChangeColorWnd = nullptr;
}

static void HsvToRgb(float h, float s, float v, u8& r, u8& g, u8& b) {
    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    float rf, gf, bf;
    if (h < 60) {
        rf = c;
        gf = x;
        bf = 0;
    } else if (h < 120) {
        rf = x;
        gf = c;
        bf = 0;
    } else if (h < 180) {
        rf = 0;
        gf = c;
        bf = x;
    } else if (h < 240) {
        rf = 0;
        gf = x;
        bf = c;
    } else if (h < 300) {
        rf = x;
        gf = 0;
        bf = c;
    } else {
        rf = c;
        gf = 0;
        bf = x;
    }
    r = (u8)((rf + m) * 255.0f);
    g = (u8)((gf + m) * 255.0f);
    b = (u8)((bf + m) * 255.0f);
}

static Pixmap* MakeHsvPixmap(int w, int h) {
    Pixmap* px = AllocPixmap(w, h, PixmapFormat::BGRA8, false);
    if (!px) {
        return nullptr;
    }
    for (int y = 0; y < h; y++) {
        float val = 1.0f - ((float)y / (float)h);
        u8* row = px->data + ((size_t)y * (size_t)px->stride);
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
    return px;
}

static void PaintCheckerboard(Gfx* gfx, Rect rc) {
    constexpr int kCheckerSize = 8;
    Color light = kColWhite;
    Color dark = MkRgb(204, 204, 204);
    for (int cy = 0; cy < rc.dy; cy += kCheckerSize) {
        for (int cx = 0; cx < rc.dx; cx += kCheckerSize) {
            int cellW = kCheckerSize;
            if (cellW > rc.dx - cx) {
                cellW = rc.dx - cx;
            }
            int cellH = kCheckerSize;
            if (cellH > rc.dy - cy) {
                cellH = rc.dy - cy;
            }
            bool isDark = ((cx / kCheckerSize) + (cy / kCheckerSize)) % 2 != 0;
            gfx->FillRect({rc.x + cx, rc.y + cy, cellW, cellH}, isDark ? dark : light);
        }
    }
}

void ChangeColorWnd::ParseCustomColors() {
    for (int i = 0; i < kMaxCustomColors; i++) {
        customColorSet[i] = false;
        customColors[i] = 0;
    }
    customColorsChanged = false;
    Str s = gGlobalPrefs ? gGlobalPrefs->customColors : Str{};
    if (len(s) == 0) {
        return;
    }
    int idx = 0;
    int i = 0;
    while (i < s.len && idx < kMaxCustomColors) {
        while (i < s.len && s.s[i] == ' ') {
            i++;
        }
        if (i >= s.len) {
            break;
        }
        int start = i;
        while (i < s.len && s.s[i] != ' ') {
            i++;
        }
        ParsedColor parsed;
        ParseColor(parsed, Str(s.s + start, i - start));
        if (parsed.parsedOk) {
            customColors[idx] = parsed.col;
            customColorSet[idx] = true;
            idx++;
        }
    }
}

void ChangeColorWnd::SaveCustomColorsIfChanged() {
    if (!customColorsChanged || !gGlobalPrefs) {
        return;
    }
    str::Builder buf;
    for (int i = 0; i < kMaxCustomColors; i++) {
        if (!customColorSet[i]) {
            continue;
        }
        if (len(buf) > 0) {
            buf.AppendChar(' ');
        }
        buf.Append(SerializeColorTemp(customColors[i]));
    }
    str::ReplaceWithCopy(&gGlobalPrefs->customColors, ToStr(buf));
    SaveSettings();
}

void ChangeColorWnd::InvalidateSwatches() {
    if (swatchPreview) {
        swatchPreview->Invalidate();
    }
    for (VirtCustom* sw : swatchCustom) {
        if (sw) {
            sw->Invalidate();
        }
    }
}

void ChangeColorWnd::SelectPreview() {
    selectedCustomIdx = -1;
    previewSelected = true;
    InvalidateSwatches();
}

void ChangeColorWnd::SelectCustom(int idx) {
    selectedCustomIdx = idx;
    previewSelected = false;
    InvalidateSwatches();
}

void ChangeColorWnd::UpdateEditFromColor() {
    updatingEdit = true;
    if (editRgb) {
        if (isCheckered) {
            editRgb->SetText(forTabColor ? StrL("unset") : StrL("checkered"));
        } else {
            editRgb->SetText(SerializeColorTemp(currentColor));
        }
    }
    updatingEdit = false;
    if (selectedCustomIdx >= 0 && !isCheckered) {
        customColors[selectedCustomIdx] = currentColor;
        customColorSet[selectedCustomIdx] = true;
        customColorsChanged = true;
    }
    InvalidateSwatches();
}

bool ChangeColorWnd::TryParseEdit() {
    if (!editRgb) {
        return false;
    }
    TempStr text = editRgb->GetTextTemp();
    if (!text || !text.s[0]) {
        return false;
    }
    ParsedColor parsed;
    ParseColor(parsed, text);
    if (!parsed.parsedOk) {
        return false;
    }
    if (parsed.col == kColorUnset) {
        isCheckered = true;
    } else {
        isCheckered = false;
        currentColor = parsed.col;
    }
    return true;
}

void ChangeColorWnd::OnEditChanged() {
    if (updatingEdit) {
        return;
    }
    if (!TryParseEdit()) {
        return;
    }
    if (selectedCustomIdx >= 0 && !isCheckered) {
        customColors[selectedCustomIdx] = currentColor;
        customColorSet[selectedCustomIdx] = true;
        customColorsChanged = true;
    }
    InvalidateSwatches();
}

void ChangeColorWnd::PickFromArea(Point ptLocal) {
    if (!colorArea) {
        return;
    }
    Size sz = colorArea->bounds.Size();
    int x = ptLocal.x - 1;
    int y = ptLocal.y - 1;
    int dx = sz.dx - 2;
    int dy = sz.dy - 2;
    if (dx <= 0 || dy <= 0) {
        return;
    }
    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }
    if (x >= dx) {
        x = dx - 1;
    }
    if (y >= dy) {
        y = dy - 1;
    }
    float hue = (float)x / (float)dx * 360.0f;
    float val = 1.0f - ((float)y / (float)dy);
    u8 cr, cg, cb;
    HsvToRgb(hue, 1.0f, val, cr, cg, cb);
    isCheckered = false;
    currentColor = MkRgb(cr, cg, cb);
    UpdateEditFromColor();
}

void ChangeColorWnd::OnAreaMouse(VirtMouseEvent* ev) {
    Point local = ev->pt;
    if (colorArea) {
        Rect b = colorArea->BoundsInWindow();
        local = {ev->ptWindow.x - b.x, ev->ptWindow.y - b.y};
    }
    PickFromArea(local);
    ev->didHandle = true;
}

static void OnAreaMouseMove(ChangeColorWnd* wnd, VirtMouseEvent* ev) {
    if (wnd->colorArea && wnd->colorArea->HasFlag(vwfPressed)) {
        wnd->OnAreaMouse(ev);
    }
}

void ChangeColorWnd::OnSwatchClick(VirtMouseEvent* ev) {
    VirtCtrl* hit = ev->hit;
    if (!hit) {
        return;
    }
    int id = hit->id;
    if (id == kIdPreview) {
        SelectPreview();
        ev->didHandle = true;
        return;
    }
    if (id >= kIdPreset0 && id < kIdPreset0 + kNumPresets) {
        Color col = kBgPresetColors[id - kIdPreset0];
        if (col == kColorUnset) {
            isCheckered = true;
        } else {
            isCheckered = false;
            currentColor = col;
        }
        SelectPreview();
        UpdateEditFromColor();
        ev->didHandle = true;
        return;
    }
    if (id >= kIdCustom0 && id < kIdCustom0 + kMaxCustomColors) {
        int idx = id - kIdCustom0;
        if (selectedCustomIdx == idx) {
            SelectPreview();
        } else {
            SelectCustom(idx);
            if (customColorSet[idx]) {
                isCheckered = false;
                currentColor = customColors[idx];
                UpdateEditFromColor();
            }
        }
        ev->didHandle = true;
    }
}

void ChangeColorWnd::OnSwatchContext(VirtMouseEvent* ev) {
    VirtCtrl* hit = ev->hit;
    if (!hit) {
        return;
    }
    int id = hit->id;
    if (id < kIdCustom0 || id >= kIdCustom0 + kMaxCustomColors) {
        return;
    }
    int idx = id - kIdCustom0;
    if (!customColorSet[idx]) {
        return;
    }
    customColorSet[idx] = false;
    customColorsChanged = true;
    if (selectedCustomIdx == idx) {
        SelectPreview();
    }
    if (swatchCustom[idx]) {
        swatchCustom[idx]->Invalidate();
    }
    ev->didHandle = true;
}

static void PaintColorArea(ChangeColorWnd* wnd, VirtPaintCtx* ctx) {
    Rect r = ctx->content;
    if (r.dx < 4 || r.dy < 4) {
        return;
    }
    Rect inner = r;
    inner.Inflate(-1, -1);
    if (!wnd->hsvPx || wnd->hsvPx->width != inner.dx || wnd->hsvPx->height != inner.dy) {
        FreePixmap(wnd->hsvPx);
        wnd->hsvPx = MakeHsvPixmap(inner.dx, inner.dy);
    }
    if (wnd->hsvPx) {
        ctx->gfx->DrawPixmap(wnd->hsvPx, inner);
    }
    ctx->gfx->DrawRect(r, ThemeWindowTextColor());
}

static void PaintSwatch(VirtCustom* sw, VirtPaintCtx* ctx) {
    auto* wnd = (ChangeColorWnd*)sw->userData;
    if (!wnd) {
        return;
    }
    Rect rc = ctx->content;
    bool selected = false;
    bool checkered = false;
    Color col = 0;
    bool empty = false;
    int id = sw->id;
    if (id == kIdPreview) {
        selected = wnd->previewSelected;
        checkered = wnd->isCheckered;
        col = wnd->currentColor;
    } else if (id >= kIdPreset0 && id < kIdPreset0 + kNumPresets) {
        col = kBgPresetColors[id - kIdPreset0];
        checkered = (col == kColorUnset);
    } else if (id >= kIdCustom0 && id < kIdCustom0 + kMaxCustomColors) {
        int idx = id - kIdCustom0;
        selected = (idx == wnd->selectedCustomIdx);
        if (wnd->customColorSet[idx]) {
            col = wnd->customColors[idx];
        } else {
            empty = true;
        }
    }

    if (selected) {
        ctx->gfx->FillRect(rc, GetSysColor(COLOR_HIGHLIGHT));
        rc.Inflate(-3, -3);
    }
    if (empty) {
        ctx->gfx->FillRect(rc, ThemeWindowControlBackgroundColor());
        Color edge = ThemeEdgeColor();
        ctx->gfx->DrawRect(rc, edge);
        ctx->gfx->DrawLineAA({rc.x, rc.y}, {rc.x + rc.dx - 1, rc.y + rc.dy - 1}, edge);
        ctx->gfx->DrawLineAA({rc.x + rc.dx - 1, rc.y}, {rc.x, rc.y + rc.dy - 1}, edge);
        return;
    }
    if (checkered) {
        PaintCheckerboard(ctx->gfx, rc);
    } else {
        ctx->gfx->FillRect(rc, col);
    }
    if (sw->HasFlag(vwfFocused) && id >= kIdPreset0 && id < kIdPreset0 + kNumPresets) {
        ctx->gfx->DrawFocusRect(ctx->content);
    }
}

void ChangeColorWnd::OnCancel(VirtMouseEvent*) {
    SaveCustomColorsIfChanged();
    ScheduleDelete();
}

WindowTab* ChangeColorWnd::TargetTab() {
    if (!IsMainWindowValid(win)) {
        return nullptr;
    }
    WindowTab* t = FindTabByFilePath(filePath);
    if (!t || t->win != win) {
        return nullptr;
    }
    return t;
}

void ChangeColorWnd::ApplyBackground() {
    WindowTab* t = TargetTab();
    if (!t || !t->ctrl) {
        return;
    }
    Str colorStr;
    if (isCheckered) {
        colorStr = StrL("checkered");
    } else {
        colorStr = SerializeColorTemp(currentColor);
    }
    Color newColor = isCheckered ? kColorUnset : currentColor;
    bool applyToAll = radioAllFiles && radioAllFiles->IsChecked();

    if (applyToAll) {
        if (isCbx) {
            SetColorText(gGlobalPrefs->comicBookUI.windowBgCol, colorStr);
        } else if (isImage) {
            SetColorText(gGlobalPrefs->imageUI.windowBgCol, colorStr);
        } else if (isEbook) {
            SetColorText(gGlobalPrefs->eBookUI.windowBgCol, colorStr);
        } else {
            SetColorText(gGlobalPrefs->fixedPageUI.windowBgCol, colorStr);
        }
        FileState* fs = FileHistoryFindByPath(t->filePath);
        if (fs) {
            SetColorText(fs->bgCol, "");
        }
        t->bgColor = kColorUnset;
        t->bgColorCheckered = false;
    } else {
        FileState* fs = FileHistoryFindByPath(t->filePath);
        if (fs) {
            SetColorText(fs->bgCol, colorStr);
        }
        t->bgColor = newColor;
        t->bgColorCheckered = isCheckered;
    }
    SaveSettings();
    HwndInvalidate(win->hwndCanvas, true);
}

void ChangeColorWnd::ApplyTabColor() {
    WindowTab* t = TargetTab();
    if (!t || !t->ctrl) {
        return;
    }
    t->tabColor = isCheckered ? kColorUnset : currentColor;
    SetTabInfoColor(t);
    FileState* fs = FileHistoryFindByPath(t->filePath);
    if (fs) {
        if (isCheckered) {
            SetColorText(fs->tabCol, "");
        } else {
            SetColorText(fs->tabCol, SerializeColorTemp(currentColor));
        }
    }
    SaveSettings();
    if (win->tabsCtrl) {
        win->tabsCtrl->ScheduleRepaint();
    }
}

void ChangeColorWnd::OnOk(VirtMouseEvent*) {
    TryParseEdit();
    SaveCustomColorsIfChanged();
    if (forTabColor) {
        ApplyTabColor();
    } else {
        ApplyBackground();
    }
    ScheduleDelete();
}

static void OnClose(WindowBase::CloseEvent* /*ev*/) {
    if (gChangeColorWnd) {
        gChangeColorWnd->OnCancel();
    }
}

static void OnDestroy(WindowBase::DestroyEvent* /*ev*/) {
    if (gChangeColorWnd) {
        gChangeColorWnd->ScheduleDelete();
    }
}

static void SwatchClicked(ChangeColorWnd* wnd, VirtMouseEvent* ev) {
    wnd->OnSwatchClick(ev);
}

static void SwatchContext(ChangeColorWnd* wnd, VirtMouseEvent* ev) {
    wnd->OnSwatchContext(ev);
}

void ChangeColorWnd::ClassifyTab(WindowTab* t) {
    isCbx = false;
    isImage = false;
    isEbook = false;
    if (!t) {
        return;
    }
    auto* engine = t->GetEngine();
    if (!engine) {
        return;
    }
    isImage = engine->IsImageCollection();
    isCbx = engine->kind == kindEngineComicBooks;
    isEbook = engine->kind == kindEngineMupdf && !str::EqI(engine->defaultExt, StrL(".pdf"));
}

void ChangeColorWnd::LoadCurrentColor() {
    WindowTab* t = tab;
    if (!t) {
        currentColor = kColWhite;
        isCheckered = false;
        return;
    }
    if (forTabColor) {
        currentColor = t->tabColor;
        isCheckered = (currentColor == kColorUnset);
        if (isCheckered) {
            currentColor = ThemeControlBackgroundColor();
        }
        return;
    }
    if (t->bgColorCheckered) {
        currentColor = kColorUnset;
        isCheckered = true;
        return;
    }
    if (t->bgColor != kColorUnset) {
        currentColor = t->bgColor;
        isCheckered = false;
        return;
    }
    ParsedColor* bgOverride = nullptr;
    if (isCbx) {
        bgOverride = GetPrefsColor(gGlobalPrefs->comicBookUI.windowBgCol);
    } else if (isImage) {
        bgOverride = GetPrefsColor(gGlobalPrefs->imageUI.windowBgCol);
    } else if (isEbook) {
        bgOverride = GetPrefsColor(gGlobalPrefs->eBookUI.windowBgCol);
    } else {
        bgOverride = GetPrefsColor(gGlobalPrefs->fixedPageUI.windowBgCol);
    }
    if (bgOverride && bgOverride->parsedOk) {
        currentColor = bgOverride->col;
        isCheckered = (bgOverride->col == kColorUnset);
        return;
    }
    Color bg;
    ThemeDocumentColors(bg);
    currentColor = bg;
    isCheckered = false;
}

void ChangeColorWnd::RelayoutRadios() {
    bool show = !forTabColor;
    Visibility vis = show ? Visibility::Visible : Visibility::Collapse;
    if (radioThisFile) {
        radioThisFile->SetVisibility(vis);
    }
    if (radioAllFiles) {
        radioAllFiles->SetVisibility(vis);
    }
    if (show && radioAllFiles) {
        Str label = _TRA("For all &PDF files");
        if (isCbx) {
            label = _TRA("For all &comic books");
        } else if (isImage) {
            label = _TRA("For all &images");
        } else if (isEbook) {
            label = _TRA("For all &ebooks");
        }
        radioAllFiles->SetText(label);
        radioThisFile->SetIsChecked(true);
        radioAllFiles->SetIsChecked(false);
    }
}

void ChangeColorWnd::SetTargetBackground(MainWindow* mainWin) {
    win = mainWin;
    forTabColor = false;
    tab = (IsMainWindowValid(win) && win->CurrentTab() && win->CurrentTab()->ctrl) ? win->CurrentTab() : nullptr;
    str::ReplaceWithCopy(&filePath, tab ? tab->filePath : Str{});
    ClassifyTab(tab);
    LoadCurrentColor();
    selectedCustomIdx = -1;
    previewSelected = true;
    if (hwnd) {
        HwndSetText(hwnd, _TRA("Change Background Color"));
        RelayoutRadios();
        UpdateEditFromColor();
        int dx = DpiScale(400);
        LayoutAndSizeToContent(layout, dx, 0, hwnd);
        DoLayout(HwndClientRect(hwnd).Size());
        UpdateTheme();
    }
}

void ChangeColorWnd::SetTargetTab(MainWindow* mainWin, WindowTab* colorTab) {
    win = mainWin;
    forTabColor = true;
    tab = colorTab;
    str::ReplaceWithCopy(&filePath, tab ? tab->filePath : Str{});
    ClassifyTab(tab);
    LoadCurrentColor();
    selectedCustomIdx = -1;
    previewSelected = true;
    if (hwnd) {
        HwndSetText(hwnd, _TRA("Change Tab Color"));
        RelayoutRadios();
        UpdateEditFromColor();
        int dx = DpiScale(400);
        LayoutAndSizeToContent(layout, dx, 0, hwnd);
        DoLayout(HwndClientRect(hwnd).Size());
        UpdateTheme();
    }
}

static VirtCustom* MakeSwatch(ChangeColorWnd* wnd, int id, Size sz, bool contextMenu) {
    auto* c = new VirtCustom();
    c->idealSize = sz;
    c->id = id;
    c->userData = (uintptr_t)wnd;
    c->SetFlag(vwfFocusable, true);
    c->cursor = CursorId::Hand;
    c->onPaint = MkFunc1(PaintSwatch, c);
    c->onClick = MkFunc1(SwatchClicked, wnd);
    if (contextMenu) {
        c->onContextMenu = MkFunc1(SwatchContext, wnd);
    }
    return c;
}

static HBox* SwatchRow(VirtCustom** items, int n, int gap) {
    auto* row = new HBox();
    row->alignMain = MainAxisAlign::MainStart;
    row->alignCross = CrossAxisAlign::CrossCenter;
    for (int i = 0; i < n; i++) {
        if (i > 0) {
            row->AddChild(new Spacer(gap, 0));
        }
        row->AddChild(items[i]);
    }
    return row;
}

bool ChangeColorWnd::Create(MainWindow* mainWin) {
    win = mainWin;

    {
        CreateCustomArgs args;
        args.title = forTabColor ? _TRA("Change Tab Color") : _TRA("Change Background Color");
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
    ParseCustomColors();

    auto* vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    {
        auto* c = new VirtCustom();
        c->idealSize = {DpiScale(380), DpiScale(150)};
        c->SetFlag(vwfCapturesMouse, true);
        c->cursor = CursorId::Cross;
        c->onPaint = MkFunc1(PaintColorArea, this);
        c->onMouseDown = MkMethod1<ChangeColorWnd, VirtMouseEvent*, &ChangeColorWnd::OnAreaMouse>(this);
        c->onMouseMove = MkFunc1(OnAreaMouseMove, this);
        colorArea = c;
        vbox->AddChild(c);
    }

    {
        auto* row = new HBox();
        row->alignMain = MainAxisAlign::MainStart;
        row->alignCross = CrossAxisAlign::CrossCenter;
        auto* lab = NewVirtText({
            .s = _TRA("RGB:"),
            .font = font,
            .isRtl = isRtl,
            .padding = DpiScaledInsets(0, 8, 0, 0),
        });
        labelRgb = lab;
        row->AddChild(lab);

        Edit::CreateArgs args;
        args.parent = hwnd;
        args.font = GetFont();
        args.withBorder = true;
        args.isRtl = isRtl;
        args.idealWidthChars = 14;
        auto* e = new Edit();
        e->SetInsetsPt(8, 0, 0, 0);
        e->Create(args);
        e->onTextChanged = MkMethod0<ChangeColorWnd, &ChangeColorWnd::OnEditChanged>(this);
        editRgb = e;
        row->AddChild(e);

        Size previewSz{DpiScale(42), DpiScale(20)};
        swatchPreview = MakeSwatch(this, kIdPreview, previewSz, false);
        row->AddChild(new Padding(swatchPreview, DpiScaledInsets(8, 0, 0, 8)));
        vbox->AddChild(row);
    }

    Size swSz{DpiScale(36), DpiScale(22)};
    int gap = DpiScale(4);
    VirtCustom* row1[8]{};
    for (int i = 0; i < kNumPresets; i++) {
        swatchPreset[i] = MakeSwatch(this, kIdPreset0 + i, swSz, false);
        row1[i] = swatchPreset[i];
    }
    for (int i = 0; i < 5; i++) {
        swatchCustom[i] = MakeSwatch(this, kIdCustom0 + i, swSz, true);
        row1[kNumPresets + i] = swatchCustom[i];
    }
    auto* swatches1 = SwatchRow(row1, 8, gap);
    vbox->AddChild(new Padding(swatches1, DpiScaledInsets(8, 0, 0, 0)));

    VirtCustom* row2[8]{};
    for (int i = 0; i < 8; i++) {
        swatchCustom[5 + i] = MakeSwatch(this, kIdCustom0 + 5 + i, swSz, true);
        row2[i] = swatchCustom[5 + i];
    }
    auto* swatches2 = SwatchRow(row2, 8, gap);
    vbox->AddChild(new Padding(swatches2, DpiScaledInsets(4, 0, 0, 0)));

    {
        auto* row = new HBox();
        row->alignMain = MainAxisAlign::MainStart;
        row->alignCross = CrossAxisAlign::CrossCenter;

        Checkbox::CreateArgs args;
        args.parent = hwnd;
        args.text = _TRA("&This file");
        args.isRtl = isRtl;
        args.isRadio = true;
        args.isGroupStart = true;
        args.initialState = Checkbox::State::Checked;
        auto* r1 = new Checkbox();
        r1->SetInsetsPt(10, 0, 0, 0);
        r1->Create(args);
        radioThisFile = r1;
        row->AddChild(r1);

        args.text = _TRA("For all &PDF files");
        args.isGroupStart = false;
        args.initialState = Checkbox::State::Unchecked;
        auto* r2 = new Checkbox();
        r2->SetInsetsPt(10, 0, 0, 12);
        r2->Create(args);
        radioAllFiles = r2;
        row->AddChild(r2);
        vbox->AddChild(row);
    }

    {
        auto* hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainEnd;
        hbox->alignCross = CrossAxisAlign::CrossCenter;
        hbox->gap = font->averageCharWidth;
        auto pad = Insets{4, 0, 4, 0};

        btnCancel = NewThemedButton(hwnd, _TRA("Cancel"), font, false);
        btnCancel->onClick = MkMethod1<ChangeColorWnd, VirtMouseEvent*, &ChangeColorWnd::OnCancel>(this);
        hbox->AddChild(new Padding(btnCancel, pad));
        btnOk = NewThemedButton(hwnd, _TRA("OK"), font, true);
        btnOk->onClick = MkMethod1<ChangeColorWnd, VirtMouseEvent*, &ChangeColorWnd::OnOk>(this);
        hbox->AddChild(new Padding(btnOk, pad));
        vbox->AddChild(hbox);
    }

    auto* padding = new Padding(vbox, DpiScaledInsets(4, 8));
    layout = padding;

    RelayoutRadios();
    UpdateEditFromColor();

    int dx = DpiScale(400);
    LayoutAndSizeToContent(layout, dx, 0, hwnd);
    DoLayout(HwndClientRect(hwnd).Size());
    HwndCenterDialog(hwnd, win ? win->hwndFrame : nullptr);
    UpdateTheme();

    SetIsVisible(true);
    if (editRgb) {
        HwndSetFocus(editRgb->hwnd);
        editRgb->SelectAll();
    }
    return true;
}

void ShowChangeBackgroundColorDialog(MainWindow* win) {
    if (!IsMainWindowValid(win) || !win->CurrentTab() || !win->CurrentTab()->ctrl) {
        return;
    }
    if (gChangeColorWnd) {
        gChangeColorWnd->SetTargetBackground(win);
        HwndSetFocus(gChangeColorWnd->hwnd);
        if (gChangeColorWnd->editRgb) {
            HwndSetFocus(gChangeColorWnd->editRgb->hwnd);
            gChangeColorWnd->editRgb->SelectAll();
        }
        return;
    }
    auto* wnd = new ChangeColorWnd();
    wnd->SetTargetBackground(win);
    wnd->closeOnEsc = true;
    wnd->onBeforeDelete = MkFunc0Void(ClearChangeColorWnd);
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnDestroy);
    wnd->SetFont(GetAppFont());
    bool ok = wnd->Create(win);
    if (!ok) {
        delete wnd;
        return;
    }
    gChangeColorWnd = wnd;
}

void ShowSetTabColorDialog(MainWindow* win, WindowTab* tab) {
    if (!IsMainWindowValid(win) || !tab || !tab->ctrl) {
        return;
    }
    if (gChangeColorWnd) {
        gChangeColorWnd->SetTargetTab(win, tab);
        HwndSetFocus(gChangeColorWnd->hwnd);
        if (gChangeColorWnd->editRgb) {
            HwndSetFocus(gChangeColorWnd->editRgb->hwnd);
            gChangeColorWnd->editRgb->SelectAll();
        }
        return;
    }
    auto* wnd = new ChangeColorWnd();
    wnd->SetTargetTab(win, tab);
    wnd->closeOnEsc = true;
    wnd->onBeforeDelete = MkFunc0Void(ClearChangeColorWnd);
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnDestroy);
    wnd->SetFont(GetAppFont());
    bool ok = wnd->Create(win);
    if (!ok) {
        delete wnd;
        return;
    }
    gChangeColorWnd = wnd;
}
