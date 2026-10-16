/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"
#include "gui/Dpi.h"

#include <commdlg.h>

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"

#include "Settings.h"
#include "AppSettings.h"
#include "MainWindow.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Canvas.h"
#include "Translations.h"
#include "DarkMode.h"
#include "SumatraDialogs.h"
#include "shared/PdfToolsCommon.h"
#include "PageGridDialogCommon.h"

// CmdConfigurePageGrid: spacing / origin / color / line style, like
// PDF-XChange's Measurement > Grid and Guides. Appearance is saved in
// FixedPageUI.PageGrid; the Show Grid checkbox is the session toggle.
//
//   Units:          [inches          v]
//   ---------------------------------------------- hairline
//   Distance between grid lines
//   Horizontal:     1____
//   Vertical:       1____
//   Subdivisions:   4____
//   Slant:          90____   degrees from horizontal; 90 is upright
//   Drag a line on the page to resize it
//   Grid line origin offset
//   From left:      0____
//   From bottom:    0____
//   ---------------------------------------------- hairline
//   Grid style:     [Dots            v]
//   Color:          [#] #8080ff____
//                   [x] Show Grid
//   Reset to defaults                [Cancel] [OK]
constexpr int kGridDlgDx = 360;
constexpr int kGridNumChars = 8;
constexpr int kGridSwatchDx = 22;
constexpr int kGridSwatchRadius = 4;

struct PageGridWnd : WindowBase {
    ~PageGridWnd() override;

    MainWindow* win = nullptr;
    PageGridSnap snap{};
    Color currentColor = kPageGridDefaultColor;
    int unitIdx = kPageGridDefaultUnitIdx;
    bool updating = false;

    DropDown* ddUnits = nullptr;
    Edit* editWidth = nullptr;
    Edit* editHeight = nullptr;
    Edit* editSub = nullptr;
    Edit* editSlant = nullptr;
    Edit* editOffX = nullptr;
    Edit* editOffY = nullptr;
    DropDown* ddStyle = nullptr;
    VirtCustom* swatch = nullptr;
    Edit* editColor = nullptr;
    Checkbox* cbShow = nullptr;
    VirtButton* btnReset = nullptr;
    VirtButton* btnCancel = nullptr;
    VirtButton* btnOk = nullptr;

    bool Create(MainWindow* win);
    void LoadFromPrefs();
    void WriteSnapToPrefs();
    void FillEditsFromPt();
    bool ReadControlsToPt(float& widthPt, float& heightPt, int& subdiv, float& offX, float& offY);
    void ApplyLive();
    void OnUnitsChanged();
    void OnColorEditChanged();
    void OnSwatchClick(VirtMouseEvent* ev);
    void OnCancel(VirtMouseEvent* ev = nullptr);
    void OnReset(VirtMouseEvent* ev = nullptr);
    void OnOk(VirtMouseEvent* ev = nullptr);
};

static PageGridWnd* gPageGridWnd = nullptr;
static COLORREF gPageGridCustColors[16]{};

PageGridWnd::~PageGridWnd() {
    str::Free(snap.color);
    str::Free(snap.style);
    str::Free(snap.units);
}

static void ClearPageGridWnd() {
    gPageGridWnd = nullptr;
}

static bool ParseEditFloat(Edit* e, float* out) {
    if (!e || !out) {
        return false;
    }
    TempStr s = e->GetTextTemp();
    if (len(s) == 0) {
        return false;
    }
    const char* cs = CStrTemp(s);
    char* end = nullptr;
    float v = strtof(cs, &end);
    if (end == cs) {
        return false;
    }
    *out = v;
    return true;
}

static bool ParseEditInt(Edit* e, int* out) {
    float v = 0;
    if (!ParseEditFloat(e, &v)) {
        return false;
    }
    *out = (int)(v + (v >= 0 ? 0.5f : -0.5f));
    return true;
}

void PageGridWnd::WriteSnapToPrefs() {
    PageGrid* pg = PageGridPrefs();
    if (!pg) {
        return;
    }
    pg->width = snap.width;
    pg->height = snap.height;
    pg->subdivisions = snap.subdivisions;
    pg->offsetX = snap.offsetX;
    pg->offsetY = snap.offsetY;
    pg->slant = snap.slant;
    SetColorText(pg->color, snap.color);
    str::ReplaceWithCopy(&pg->style, snap.style);
    str::ReplaceWithCopy(&pg->units, snap.units);
    SetShowPageGrid(snap.showGrid);
}

void PageGridWnd::FillEditsFromPt() {
    PageGrid* pg = PageGridPrefs();
    if (!pg) {
        return;
    }
    updating = true;
    if (editWidth) {
        editWidth->SetText(
            PageGridNumTemp(PageGridFromPt(pg->width > 0 ? pg->width : kPageGridDefaultSizePt, unitIdx)));
    }
    if (editHeight) {
        editHeight->SetText(
            PageGridNumTemp(PageGridFromPt(pg->height > 0 ? pg->height : kPageGridDefaultSizePt, unitIdx)));
    }
    if (editSub) {
        int n = pg->subdivisions > 0 ? pg->subdivisions : kPageGridDefaultSubdivisions;
        editSub->SetText(fmt("%d", n));
    }
    if (editSlant) {
        float slant = pg->slant == 0 ? kPageGridDefaultSlant : pg->slant;
        editSlant->SetText(PageGridNumTemp(slant));
    }
    if (editOffX) {
        editOffX->SetText(PageGridNumTemp(PageGridFromPt(pg->offsetX, unitIdx)));
    }
    if (editOffY) {
        editOffY->SetText(PageGridNumTemp(PageGridFromPt(pg->offsetY, unitIdx)));
    }
    updating = false;
}

bool PageGridWnd::ReadControlsToPt(float& widthPt, float& heightPt, int& subdiv, float& offX, float& offY) {
    float w = 0, h = 0, ox = 0, oy = 0;
    int sub = 0;
    if (!ParseEditFloat(editWidth, &w) || !ParseEditFloat(editHeight, &h) || !ParseEditInt(editSub, &sub)) {
        return false;
    }
    ParseEditFloat(editOffX, &ox);
    ParseEditFloat(editOffY, &oy);
    w = PageGridToPt(w, unitIdx);
    h = PageGridToPt(h, unitIdx);
    ox = PageGridToPt(ox, unitIdx);
    oy = PageGridToPt(oy, unitIdx);
    if (w < 1.f || h < 1.f || sub < 1) {
        return false;
    }
    widthPt = limitValue(w, 1.f, 720.f);
    heightPt = limitValue(h, 1.f, 720.f);
    subdiv = limitValue(sub, 1, 32);
    offX = limitValue(ox, -720.f, 720.f);
    offY = limitValue(oy, -720.f, 720.f);
    return true;
}

void PageGridWnd::ApplyLive() {
    if (updating) {
        return;
    }
    PageGrid* pg = PageGridPrefs();
    if (!pg) {
        return;
    }
    float w, h, ox, oy;
    int sub;
    if (ReadControlsToPt(w, h, sub, ox, oy)) {
        pg->width = w;
        pg->height = h;
        pg->subdivisions = sub;
        pg->offsetX = ox;
        pg->offsetY = oy;
    }
    float slant = pg->slant;
    if (ParseEditFloat(editSlant, &slant)) {
        pg->slant = PageGridClampSlant(slant);
    }
    int styleIdx = ddStyle ? CbGetCurrentSelection(ddStyle) : kPageGridDefaultStyleIdx;
    if (styleIdx < 0) {
        styleIdx = kPageGridDefaultStyleIdx;
    }
    str::ReplaceWithCopy(&pg->style, SeqStrByIndex(kPageGridStyleTok, styleIdx));
    str::ReplaceWithCopy(&pg->units, SeqStrByIndex(kPageGridUnitTok, unitIdx));
    SetColorText(pg->color, SerializeColorTemp(currentColor));
    if (cbShow) {
        SetShowPageGrid(cbShow->IsChecked());
    }
    RedrawPageGridWindows();
}

void PageGridWnd::OnUnitsChanged() {
    if (updating || !ddUnits) {
        return;
    }
    PageGrid* pg = PageGridPrefs();
    float w, h, ox, oy;
    int sub;
    if (ReadControlsToPt(w, h, sub, ox, oy) && pg) {
        pg->width = w;
        pg->height = h;
        pg->subdivisions = sub;
        pg->offsetX = ox;
        pg->offsetY = oy;
    }
    int idx = CbGetCurrentSelection(ddUnits);
    if (idx < 0) {
        idx = kPageGridDefaultUnitIdx;
    }
    unitIdx = idx;
    FillEditsFromPt();
    ApplyLive();
}

void PageGridWnd::OnColorEditChanged() {
    if (updating || !editColor) {
        return;
    }
    ParsedColor parsed;
    ParseColor(parsed, editColor->GetTextTemp());
    if (!parsed.parsedOk || IsSpecialColor(parsed.col)) {
        return;
    }
    currentColor = parsed.col;
    if (swatch) {
        swatch->Invalidate();
    }
    ApplyLive();
}

void PageGridWnd::OnSwatchClick(VirtMouseEvent*) {
    CHOOSECOLORW cc{};
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = hwnd;
    cc.lpCustColors = gPageGridCustColors;
    cc.rgbResult = currentColor;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
    if (!DarkModeChooseColor(&cc)) {
        return;
    }
    currentColor = cc.rgbResult;
    updating = true;
    if (editColor) {
        editColor->SetText(SerializeColorTemp(currentColor));
    }
    updating = false;
    if (swatch) {
        swatch->Invalidate();
    }
    ApplyLive();
}

void PageGridWnd::LoadFromPrefs() {
    PageGrid* pg = PageGridPrefs();
    if (!pg) {
        return;
    }
    CopyPageGridSnap(snap, *pg, ShowPageGrid());
    ParseColor(pg->color);
    currentColor = (pg->color.parsedOk && !IsSpecialColor(pg->color.col)) ? pg->color.col : kPageGridDefaultColor;
    unitIdx = SeqStrIndexIS(kPageGridUnitTok, pg->units);
    if (unitIdx < 0) {
        unitIdx = kPageGridDefaultUnitIdx;
    }
    int styleIdx = SeqStrIndexIS(kPageGridStyleTok, pg->style);
    if (styleIdx < 0) {
        styleIdx = kPageGridDefaultStyleIdx;
    }
    updating = true;
    if (ddUnits) {
        CbSetCurrentSelection(ddUnits, unitIdx);
    }
    if (ddStyle) {
        CbSetCurrentSelection(ddStyle, styleIdx);
    }
    if (editColor) {
        editColor->SetText(SerializeColorTemp(currentColor));
    }
    if (cbShow) {
        cbShow->SetIsChecked(ShowPageGrid());
    }
    updating = false;
    FillEditsFromPt();
    if (swatch) {
        swatch->Invalidate();
    }
}

void PageGridWnd::OnCancel(VirtMouseEvent*) {
    WriteSnapToPrefs();
    RedrawPageGridWindows();
    ScheduleDelete();
}

void PageGridWnd::OnReset(VirtMouseEvent*) {
    PageGrid* pg = PageGridPrefs();
    if (!pg) {
        return;
    }
    ResetPageGridToDefaults();

    currentColor = kPageGridDefaultColor;
    unitIdx = kPageGridDefaultUnitIdx;
    updating = true;
    if (ddUnits) {
        CbSetCurrentSelection(ddUnits, unitIdx);
    }
    if (ddStyle) {
        CbSetCurrentSelection(ddStyle, kPageGridDefaultStyleIdx);
    }
    if (editColor) {
        editColor->SetText(SerializeColorTemp(currentColor));
    }
    updating = false;
    FillEditsFromPt();
    if (swatch) {
        swatch->Invalidate();
    }
    ApplyLive();
}

void PageGridWnd::OnOk(VirtMouseEvent*) {
    ApplyLive();
    if (HasPermission(Perm::SavePreferences)) {
        ScheduleSaveSettings();
    }
    ScheduleDelete();
}

static void OnClose(WindowBase::CloseEvent* /*ev*/) {
    if (gPageGridWnd) {
        gPageGridWnd->OnCancel();
    }
}

static void OnDestroy(WindowBase::DestroyEvent* /*ev*/) {
    if (gPageGridWnd) {
        gPageGridWnd->ScheduleDelete();
    }
}

static void PaintPageGridSwatch(VirtCustom* sw, VirtPaintCtx* ctx) {
    auto* wnd = (PageGridWnd*)sw->userData;
    if (!wnd) {
        return;
    }
    Rect rc = ctx->content;
    ctx->gfx->FillRoundedRect(rc, DpiScale(kGridSwatchRadius), wnd->currentColor, ThemeEdgeColor());
}

bool PageGridWnd::Create(MainWindow* mainWin) {
    win = mainWin;
    Color colTxt = ThemeWindowTextColor();
    Color colBg = ThemeWindowControlBackgroundColor();
    Color colMuted = ThemeWindowTextDisabledColor();
    PlatformFont* fontText = DlgUiFont(kToolDlgTextFontPx);
    PlatformFont* fontMeta = DlgUiFont(kToolDlgMetaFontPx);
    SetFont(fontText);
    bool isRtl = IsUIRtl();
    {
        CreateCustomArgs args;
        args.owner = win ? win->hwndFrame : nullptr;
        args.title = Tr("Page Grid");
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

    // every row is a muted label in one column, then its control
    Vec<VirtText*> labels;
    auto addRow = [&](VBox* box, Str label, ILayout* ctrl, int flex) {
        HBox* row = NewDlgFieldRow();
        VirtText* t = NewDlgLabel(label, fontText, colMuted, kToolDlgRowDy);
        VecAppend(labels, t);
        row->AddChild(t);
        row->AddChild(ctrl, flex);
        box->AddChild(row);
    };
    auto addHeader = [&](VBox* box, Str s) { box->AddChild(NewDlgLabel(s, fontMeta, colMuted, kToolDlgRowDy)); };
    auto makeEdit = [&]() {
        Edit* e = NewDlgEdit(hwnd, fontText, {}, kGridNumChars);
        e->onTextChanged = MkMethod0<PageGridWnd, &PageGridWnd::ApplyLive>(this);
        return e;
    };
    auto makeDropDown = [&](Str (*name)(int), int n) {
        DropDown::CreateArgs args;
        args.parent = hwnd;
        args.font = fontText;
        args.isRtl = isRtl;
        auto* d = new DropDown();
        d->Create(args);
        StrVec items;
        for (int i = 0; i < n; i++) {
            items.Append(name(i));
        }
        d->SetItems(items);
        return d;
    };
    auto newSection = [&]() {
        auto* box = new VBox();
        box->alignCross = CrossAxisAlign::Stretch;
        return box;
    };

    auto* vbox = new VBox();
    vbox->alignCross = CrossAxisAlign::Stretch;

    VBox* units = newSection();
    ddUnits = makeDropDown(PageGridUnitName, 4);
    ddUnits->onSelectionChanged = MkMethod0<PageGridWnd, &PageGridWnd::OnUnitsChanged>(this);
    addRow(units, Tr("Units:"), ddUnits, 1);
    vbox->AddChild(NewDlgSection(units));
    vbox->AddChild(new VirtLine());

    VBox* spacing = newSection();
    addHeader(spacing, Tr("Distance between grid lines"));
    editWidth = makeEdit();
    addRow(spacing, Tr("Horizontal:"), NewDlgUnderlined(editWidth), 0);
    editHeight = makeEdit();
    addRow(spacing, Tr("Vertical:"), NewDlgUnderlined(editHeight), 0);
    editSub = makeEdit();
    addRow(spacing, Tr("Subdivisions:"), NewDlgUnderlined(editSub), 0);
    editSlant = makeEdit();
    addRow(spacing, Tr("Slant:"), NewDlgUnderlined(editSlant), 0);
    addHeader(spacing, Tr("Drag a line on the page to resize it"));
    addHeader(spacing, Tr("Alt+drag when this dialog is closed"));
    addHeader(spacing, Tr("Grid line origin offset"));
    editOffX = makeEdit();
    addRow(spacing, Tr("From left:"), NewDlgUnderlined(editOffX), 0);
    editOffY = makeEdit();
    addRow(spacing, Tr("From bottom:"), NewDlgUnderlined(editOffY), 0);
    vbox->AddChild(NewDlgSection(spacing));
    vbox->AddChild(new VirtLine());

    VBox* look = newSection();
    ddStyle = makeDropDown(PageGridStyleName, 3);
    ddStyle->onSelectionChanged = MkMethod0<PageGridWnd, &PageGridWnd::ApplyLive>(this);
    addRow(look, Tr("Grid style:"), ddStyle, 1);
    {
        HBox* colorRow = NewDlgRow();
        auto* c = new VirtCustom();
        c->idealSize = {DpiScale(kGridSwatchDx), DpiScale(kGridSwatchDx)};
        c->userData = (uintptr_t)this;
        c->SetFlag(vwfFocusable, true);
        c->cursor = CursorId::Hand;
        c->onPaint = MkFunc1(PaintPageGridSwatch, c);
        c->onClick = MkMethod1<PageGridWnd, VirtMouseEvent*, &PageGridWnd::OnSwatchClick>(this);
        swatch = c;
        colorRow->AddChild(c);
        editColor = NewDlgEdit(hwnd, fontText, {}, kGridNumChars);
        editColor->onTextChanged = MkMethod0<PageGridWnd, &PageGridWnd::OnColorEditChanged>(this);
        colorRow->AddChild(NewDlgUnderlined(editColor));
        addRow(look, Tr("Color:"), colorRow, 0);
    }
    cbShow = NewDlgCheckbox(hwnd, Tr("&Show Grid"), fontText, false);
    cbShow->onStateChanged = MkMethod0<PageGridWnd, &PageGridWnd::ApplyLive>(this);
    addRow(look, {}, cbShow, 0);
    vbox->AddChild(NewDlgSection(look));
    AlignDlgLabels(labels);

    btnReset = NewDlgButton(hwnd, Tr("Reset to defaults"), fontText, DlgButtonStyle::Ghost, false);
    btnReset->onClick = MkMethod1<PageGridWnd, VirtMouseEvent*, &PageGridWnd::OnReset>(this);
    btnCancel = NewDlgButton(hwnd, Tr("Cancel"), fontText, DlgButtonStyle::Outline, false);
    btnCancel->onClick = MkMethod1<PageGridWnd, VirtMouseEvent*, &PageGridWnd::OnCancel>(this);
    btnOk = NewDlgButton(hwnd, Tr("OK"), fontText, DlgButtonStyle::Filled, true);
    btnOk->onClick = MkMethod1<PageGridWnd, VirtMouseEvent*, &PageGridWnd::OnOk>(this);
    vbox->AddChild(NewDlgFooter(btnCancel, btnOk, btnReset));
    layout = vbox;

    LayoutAndSizeToContent(layout, DpiScale(kGridDlgDx), 0, hwnd);
    DoLayout(HwndClientRect(hwnd).Size());
    HwndCenterDialog(hwnd, win ? win->hwndFrame : nullptr);
    UpdateTheme();

    LoadFromPrefs();

    SetIsVisible(true);
    EditSelectAll(editWidth);
    EditSetFocus(editWidth);
    return true;
}

bool IsPageGridDialogOpen() {
    return gPageGridWnd != nullptr;
}

void PageGridDialogSyncFromPrefs() {
    if (gPageGridWnd) {
        gPageGridWnd->FillEditsFromPt();
    }
}

void ShowPageGridDialog(MainWindow* win) {
    if (gPageGridWnd) {
        HwndSetFocus(gPageGridWnd->hwnd);
        return;
    }
    auto* wnd = new PageGridWnd();
    wnd->closeOnEsc = true;
    wnd->onBeforeDelete = MkFunc0Void(ClearPageGridWnd);
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnDestroy);
    wnd->SetFont(GetAppFont());
    bool ok = wnd->Create(win);
    if (!ok) {
        delete wnd;
        return;
    }
    gPageGridWnd = wnd;
}
