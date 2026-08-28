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
#include "GlobalPrefs.h"
#include "MainWindow.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Canvas.h"
#include "Translations.h"
#include "DarkMode_win.h"
#include "SumatraDialogs.h"

// CmdConfigurePageGrid: spacing / origin / color / line style, like
// PDF-XChange's Measurement > Grid and Guides. Appearance is saved in
// FixedPageUI.PageGrid; the Show Grid checkbox is the session toggle.

static SeqStrings kPageGridUnitTok = "pt\0in\0mm\0cm\0";
static SeqStrings kPageGridStyleTok = "dots\0dotted\0solid\0";

constexpr float kPageGridPtPerIn = 72.f;
constexpr float kPageGridMmPerIn = 25.4f;
constexpr float kPageGridDefaultSizePt = 72.f;
constexpr int kPageGridDefaultSubdivisions = 4;
constexpr Color kPageGridDefaultColor = MkRgb(128, 128, 255);
constexpr int kPageGridDefaultStyleIdx = 0;
constexpr int kPageGridDefaultUnitIdx = 1;

struct PageGridSnap {
    float width = kPageGridDefaultSizePt;
    float height = kPageGridDefaultSizePt;
    int subdivisions = kPageGridDefaultSubdivisions;
    float offsetX = 0;
    float offsetY = 0;
    Str color;
    Str style;
    Str units;
    bool showGrid = false;
};

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

static float PageGridToPt(float v, int unit) {
    switch (unit) {
        case 1:
            return v * kPageGridPtPerIn;
        case 2:
            return v * kPageGridPtPerIn / kPageGridMmPerIn;
        case 3:
            return v * kPageGridPtPerIn / 2.54f;
        default:
            return v;
    }
}

static float PageGridFromPt(float pt, int unit) {
    switch (unit) {
        case 1:
            return pt / kPageGridPtPerIn;
        case 2:
            return pt * kPageGridMmPerIn / kPageGridPtPerIn;
        case 3:
            return pt * 2.54f / kPageGridPtPerIn;
        default:
            return pt;
    }
}

static TempStr PageGridNumTemp(float v) {
    return fmt("%.4g", v);
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

static Str PageGridUnitName(int i) {
    switch (i) {
        case 1:
            return _TRA("inches");
        case 2:
            return _TRA("millimeters");
        case 3:
            return _TRA("centimeters");
        default:
            return _TRA("points");
    }
}

static Str PageGridStyleName(int i) {
    if (i == 1) {
        return _TRA("Dotted lines");
    }
    if (i == 2) {
        return _TRA("Solid lines");
    }
    return _TRA("Dots");
}

static PageGrid* PageGridPrefs() {
    return gGlobalPrefs ? &gGlobalPrefs->fixedPageUI.pageGrid : nullptr;
}

static void CopyPageGridSnap(PageGridSnap& dst, const PageGrid& src, bool showGrid) {
    dst.width = src.width > 0 ? src.width : kPageGridDefaultSizePt;
    dst.height = src.height > 0 ? src.height : kPageGridDefaultSizePt;
    dst.subdivisions = src.subdivisions > 0 ? src.subdivisions : kPageGridDefaultSubdivisions;
    dst.offsetX = src.offsetX;
    dst.offsetY = src.offsetY;
    str::ReplaceWithCopy(&dst.color, src.color.s);
    str::ReplaceWithCopy(&dst.style, src.style);
    str::ReplaceWithCopy(&dst.units, src.units);
    dst.showGrid = showGrid;
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
    int styleIdx = ddStyle ? ddStyle->GetCurrentSelection() : kPageGridDefaultStyleIdx;
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
    int idx = ddUnits->GetCurrentSelection();
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
        ddUnits->SetCurrentSelection(unitIdx);
    }
    if (ddStyle) {
        ddStyle->SetCurrentSelection(styleIdx);
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

// Restore the shipped appearance settings as a live preview. Show Grid is a
// session toggle rather than a saved setting, so leave it unchanged.
void PageGridWnd::OnReset(VirtMouseEvent*) {
    PageGrid* pg = PageGridPrefs();
    if (!pg) {
        return;
    }
    pg->width = kPageGridDefaultSizePt;
    pg->height = kPageGridDefaultSizePt;
    pg->subdivisions = kPageGridDefaultSubdivisions;
    pg->offsetX = 0;
    pg->offsetY = 0;
    SetColorText(pg->color, SerializeColorTemp(kPageGridDefaultColor));
    str::ReplaceWithCopy(&pg->style, SeqStrByIndex(kPageGridStyleTok, kPageGridDefaultStyleIdx));
    str::ReplaceWithCopy(&pg->units, SeqStrByIndex(kPageGridUnitTok, kPageGridDefaultUnitIdx));

    currentColor = kPageGridDefaultColor;
    unitIdx = kPageGridDefaultUnitIdx;
    updating = true;
    if (ddUnits) {
        ddUnits->SetCurrentSelection(unitIdx);
    }
    if (ddStyle) {
        ddStyle->SetCurrentSelection(kPageGridDefaultStyleIdx);
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
        SaveSettings();
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
    ctx->gfx->FillRect(rc, wnd->currentColor);
    ctx->gfx->DrawRect(rc, ThemeWindowTextColor());
}

static VirtText* PageGridLabel(Str s, PlatformFont* font, bool isRtl) {
    return NewVirtText({
        .s = s,
        .font = font,
        .isRtl = isRtl,
        .padding = DpiScaledInsets(0, 8, 0, 0),
    });
}

static void AddPageGridRow(Table* t, int row, VirtText* label, ILayout* ctrl, bool stretchCtrl = true) {
    auto& lc = t->SetCell(row, 0, label);
    lc.alignH = CrossAxisAlign::Stretch;
    lc.alignV = CrossAxisAlign::CrossCenter;
    auto& rc = t->SetCell(row, 1, ctrl);
    rc.alignH = stretchCtrl ? CrossAxisAlign::Stretch : CrossAxisAlign::CrossStart;
    rc.alignV = CrossAxisAlign::CrossCenter;
}

bool PageGridWnd::Create(MainWindow* mainWin) {
    win = mainWin;

    {
        CreateCustomArgs args;
        args.owner = win ? win->hwndFrame : nullptr;
        args.title = _TRA("Page Grid");
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

    auto makeEdit = [&]() -> Edit* {
        Edit::CreateArgs args;
        args.parent = hwnd;
        args.font = GetFont();
        args.withBorder = true;
        args.isRtl = isRtl;
        args.idealWidthChars = 8;
        auto* e = new Edit();
        e->Create(args);
        e->onTextChanged = MkMethod0<PageGridWnd, &PageGridWnd::ApplyLive>(this);
        return e;
    };
    auto makeDropDown = [&]() -> DropDown* {
        DropDown::CreateArgs args;
        args.parent = hwnd;
        args.font = GetFont();
        args.isRtl = isRtl;
        auto* d = new DropDown();
        d->Create(args);
        return d;
    };

    auto* vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    {
        auto* t = new Table();
        t->SetSize(1, 2);
        t->colGap = DpiScale(8);
        t->rowGap = DpiScale(6);
        ddUnits = makeDropDown();
        {
            StrVec items;
            for (int i = 0; i < 4; i++) {
                items.Append(PageGridUnitName(i));
            }
            ddUnits->SetItems(items);
            ddUnits->onSelectionChanged = MkMethod0<PageGridWnd, &PageGridWnd::OnUnitsChanged>(this);
        }
        AddPageGridRow(t, 0, PageGridLabel(_TRA("Units:"), font, isRtl), ddUnits);
        vbox->AddChild(t);
    }

    vbox->AddChild(NewVirtText({
        .s = _TRA("Distance between grid lines"),
        .font = font,
        .isRtl = isRtl,
        .padding = DpiScaledInsets(8, 0, 4, 0),
    }));

    {
        auto* t = new Table();
        t->SetSize(3, 2);
        t->colGap = DpiScale(8);
        t->rowGap = DpiScale(6);
        editWidth = makeEdit();
        AddPageGridRow(t, 0, PageGridLabel(_TRA("Horizontal:"), font, isRtl), editWidth, false);
        editHeight = makeEdit();
        AddPageGridRow(t, 1, PageGridLabel(_TRA("Vertical:"), font, isRtl), editHeight, false);
        editSub = makeEdit();
        AddPageGridRow(t, 2, PageGridLabel(_TRA("Subdivisions:"), font, isRtl), editSub, false);
        vbox->AddChild(t);
    }

    {
        auto* t = new Table();
        t->SetSize(2, 2);
        t->colGap = DpiScale(8);
        t->rowGap = DpiScale(6);
        t->padding = DpiScaledInsets(8, 0, 0, 0);
        vbox->AddChild(NewVirtText({
            .s = _TRA("Grid line origin offset"),
            .font = font,
            .isRtl = isRtl,
            .padding = DpiScaledInsets(8, 0, 4, 0),
        }));
        editOffX = makeEdit();
        AddPageGridRow(t, 0, PageGridLabel(_TRA("From left:"), font, isRtl), editOffX, false);
        editOffY = makeEdit();
        AddPageGridRow(t, 1, PageGridLabel(_TRA("From bottom:"), font, isRtl), editOffY, false);
        vbox->AddChild(t);
    }

    {
        auto* t = new Table();
        t->SetSize(3, 2);
        t->colGap = DpiScale(8);
        t->rowGap = DpiScale(6);
        t->padding = DpiScaledInsets(8, 0, 0, 0);

        ddStyle = makeDropDown();
        {
            StrVec items;
            for (int i = 0; i < 3; i++) {
                items.Append(PageGridStyleName(i));
            }
            ddStyle->SetItems(items);
            ddStyle->onSelectionChanged = MkMethod0<PageGridWnd, &PageGridWnd::ApplyLive>(this);
        }
        AddPageGridRow(t, 0, PageGridLabel(_TRA("Grid style:"), font, isRtl), ddStyle);

        {
            auto* row = new HBox();
            row->alignMain = MainAxisAlign::MainStart;
            row->alignCross = CrossAxisAlign::CrossCenter;
            row->gap = DpiScale(8);
            auto* c = new VirtCustom();
            c->idealSize = {DpiScale(22), DpiScale(22)};
            c->userData = (uintptr_t)this;
            c->SetFlag(vwfFocusable, true);
            c->cursor = CursorId::Hand;
            c->onPaint = MkFunc1(PaintPageGridSwatch, c);
            c->onClick = MkMethod1<PageGridWnd, VirtMouseEvent*, &PageGridWnd::OnSwatchClick>(this);
            swatch = c;
            row->AddChild(c);
            editColor = makeEdit();
            editColor->onTextChanged = MkMethod0<PageGridWnd, &PageGridWnd::OnColorEditChanged>(this);
            row->AddChild(editColor, 1);
            AddPageGridRow(t, 1, PageGridLabel(_TRA("Color:"), font, isRtl), row);
        }

        {
            Checkbox::CreateArgs args;
            args.parent = hwnd;
            args.text = _TRA("&Show Grid");
            args.isRtl = isRtl;
            cbShow = new Checkbox();
            cbShow->Create(args);
            cbShow->onStateChanged = MkMethod0<PageGridWnd, &PageGridWnd::ApplyLive>(this);
            AddPageGridRow(t, 2, PageGridLabel({}, font, isRtl), cbShow);
        }
        vbox->AddChild(t);
    }

    {
        auto* hbox = new HBox();
        hbox->alignMain = MainAxisAlign::SpaceBetween;
        hbox->alignCross = CrossAxisAlign::CrossCenter;
        hbox->gap = font->averageCharWidth;
        auto pad = Insets{8, 0, 4, 0};

        btnReset = NewThemedButton(hwnd, _TRA("Reset to defaults"), font, false);
        btnReset->onClick = MkMethod1<PageGridWnd, VirtMouseEvent*, &PageGridWnd::OnReset>(this);
        hbox->AddChild(new Padding(btnReset, pad));

        auto* right = new HBox();
        right->alignMain = MainAxisAlign::MainEnd;
        right->alignCross = CrossAxisAlign::CrossCenter;
        right->gap = font->averageCharWidth;
        btnCancel = NewThemedButton(hwnd, _TRA("Cancel"), font, false);
        btnCancel->onClick = MkMethod1<PageGridWnd, VirtMouseEvent*, &PageGridWnd::OnCancel>(this);
        right->AddChild(new Padding(btnCancel, pad));
        btnOk = NewThemedButton(hwnd, _TRA("OK"), font, true);
        btnOk->onClick = MkMethod1<PageGridWnd, VirtMouseEvent*, &PageGridWnd::OnOk>(this);
        right->AddChild(new Padding(btnOk, pad));
        hbox->AddChild(right);
        vbox->AddChild(hbox);
    }

    auto* padding = new Padding(vbox, DpiScaledInsets(4, 8));
    layout = padding;

    int dx = DpiScale(360);
    LayoutAndSizeToContent(layout, dx, 0, hwnd);
    DoLayout(HwndClientRect(hwnd).Size());
    HwndCenterDialog(hwnd, win ? win->hwndFrame : nullptr);
    UpdateTheme();

    LoadFromPrefs();

    SetIsVisible(true);
    EditSelectAll(editWidth);
    EditSetFocus(editWidth);
    return true;
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
