/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/GuessFileType.h"
#include "base/Pixmap.h"
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
#include "DisplayMode.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "MainWindow.h"
#include "AIChatTypes.h"
#include "WindowTab.h"
#include "Theme.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "SidebarPanel.h"
#include "LayersUI.h"

constexpr int kLayerRowDy = 24;
constexpr int kLayerPadDx = 8;
constexpr int kLayerIndentDx = 16;
constexpr int kLayerBoxDx = 13;

static EngineBase* TabEngine(WindowTab* tab) {
    DisplayModel* dm = tab ? tab->AsFixed() : nullptr;
    return dm ? dm->GetEngine() : nullptr;
}

bool TabHasLayers(WindowTab* tab) {
    return EngineMupdfHasLayers(TabEngine(tab));
}

//--- the sidebar view

struct LayerRowsModel : ListBoxModel {
    Vec<PdfLayerRow*> rows;

    ~LayerRowsModel() override { DeleteVecMembers(rows); }
    int ItemsCount() override { return len(rows); }
    Str Item(int i) override { return rows[i]->text; }
};

struct LayersCtrl : VirtListBox {
    MainWindow* win = nullptr;
    LayerRowsModel* rows = nullptr;

    void DrawRow(DrawItemEvent* ev);
    void OnLayersMouseDown(VirtMouseEvent* ev);
    void OnLayersKeyDown(VirtKeyEvent* ev);
};

// re-read from the engine: rows follow the current tab and what toggling changed
static void ReloadRows(LayersCtrl* c) {
    DeleteVecMembers(c->rows->rows);
    EngineMupdfGetLayerRows(TabEngine(c->win->CurrentTab()), c->rows->rows);
    c->SetModel(c->rows);
    c->Invalidate();
}

static void DrawCheckbox(Gfx* gfx, Rect box, bool on, Color col) {
    gfx->DrawRect(box, col);
    if (!on) {
        return;
    }
    // a check mark: down to the lower third, then up to the top right
    Point p1 = {box.x + (box.dx / 5), box.y + (box.dy / 2)};
    Point p2 = {box.x + (box.dx * 2 / 5), box.y + (box.dy * 3 / 4)};
    Point p3 = {box.x + (box.dx * 4 / 5), box.y + (box.dy / 4)};
    float thick = (float)std::max(2, box.dx / 6);
    gfx->DrawLineAA(p1, p2, col, thick);
    gfx->DrawLineAA(p2, p3, col, thick);
}

static void DrawRadio(Gfx* gfx, Rect box, bool on, Color col, Color bg) {
    gfx->FillEllipse(box, col);
    Rect inner = box;
    inner.Inflate(-1, -1);
    gfx->FillEllipse(inner, bg);
    if (!on) {
        return;
    }
    Rect dot = box;
    int inset = box.dx / 4;
    dot.Inflate(-inset, -inset);
    gfx->FillEllipse(dot, col);
}

//  [indent][box] text      a label has no box; a locked layer is dimmed
void LayersCtrl::DrawRow(DrawItemEvent* ev) {
    if (ev->itemIndex < 0 || ev->itemIndex >= len(rows->rows)) {
        return;
    }
    const PdfLayerRow* row = rows->rows[ev->itemIndex];
    Gfx* gfx = ev->gfx;
    Rect r = ev->itemRect;
    Color bg = ev->selected ? GetColor(kColListSel) : GetColor(kColListBg);
    if (ev->selected) {
        gfx->FillRect(r, bg);
    }
    Color col = GetColor(kColListText);
    if (row->locked) {
        col = AccentColor(col, 60);
    }
    int pad = DpiScaleByDpi(dpi, kLayerPadDx);
    int x = r.x + pad + (row->depth * DpiScaleByDpi(dpi, kLayerIndentDx));
    if (row->kind != PdfLayerKind::Label) {
        int boxDx = DpiScaleByDpi(dpi, kLayerBoxDx);
        Rect box = {x, r.y + ((r.dy - boxDx) / 2), boxDx, boxDx};
        if (row->kind == PdfLayerKind::Radio) {
            DrawRadio(gfx, box, row->on, col, bg);
        } else {
            DrawCheckbox(gfx, box, row->on, col);
        }
        x = box.Right() + pad;
    }
    Rect rcText = {x, r.y, r.Right() - pad - x, r.dy};
    gfx->DrawText(row->text, rcText, gfxTextEllipsis | gfxTextVCenter, font, col);
}

// a click on a row turns its layer on or off
void LayersCtrl::OnLayersMouseDown(VirtMouseEvent* ev) {
    int idx = ev->button == 0 ? ItemFromPoint(ev->pt) : -1;
    OnMouseDown(ev);
    if (idx >= 0) {
        ToggleLayerRow(win, idx);
    }
}

// Space turns the selected row's layer on or off; the rest moves the selection
void LayersCtrl::OnLayersKeyDown(VirtKeyEvent* ev) {
    if (ev->vkey == VK_SPACE) {
        ToggleLayerRow(win, GetCurrentSelection());
        ev->didHandle = true;
        return;
    }
    OnKeyDown(ev);
}

void CreateLayersCtrl(MainWindow* win) {
    auto* c = new LayersCtrl();
    c->win = win;
    c->rows = new LayerRowsModel();
    c->SetModel(c->rows);
    c->onDrawItem = MkMethod1<LayersCtrl, VirtListBox::DrawItemEvent*, &LayersCtrl::DrawRow>(c);
    c->onMouseDown = MkMethod1<LayersCtrl, VirtMouseEvent*, &LayersCtrl::OnLayersMouseDown>(c);
    c->onKeyDown = MkMethod1<LayersCtrl, VirtKeyEvent*, &LayersCtrl::OnLayersKeyDown>(c);
    c->SetIsVisible(false);
    win->layersCtrl = c;
    LayersCtrlColorsAndDpi(win, DpiGetForHwnd(win->hwndFrame));
}

VirtCtrl* LayersView(MainWindow* win) {
    return win->layersCtrl;
}

void DeleteLayersCtrl(MainWindow* win) {
    delete win->layersCtrl;
    win->layersCtrl = nullptr;
}

void LayersCtrlColorsAndDpi(MainWindow* win, int dpi) {
    LayersCtrl* c = win->layersCtrl;
    if (!c) {
        return;
    }
    c->font = GetAppFont();
    c->dpi = dpi;
    c->itemDy = DpiScaleByDpi(dpi, kLayerRowDy);
    c->SetColor(kColListText, ThemeWindowTextColor());
    c->SetColor(kColListBg, ThemeControlBackgroundColor());
    c->Invalidate();
}

// the list follows the current tab
void UpdateLayersCtrl(MainWindow* win) {
    LayersCtrl* c = win->layersCtrl;
    if (!c) {
        return;
    }
    c->SetCurrentSelection(-1);
    ReloadRows(c);
}

// Turns the layer of a Layers view row on or off and shows the pages with it.
// Returns false if nothing changed (a label, a locked layer)
bool ToggleLayerRow(MainWindow* win, int row) {
    EngineBase* engine = TabEngine(win->CurrentTab());
    if (row < 0 || !EngineMupdfToggleLayerRow(engine, row)) {
        return false;
    }
    if (LayersCtrl* c = win->layersCtrl) {
        int sel = c->GetCurrentSelection();
        ReloadRows(c);
        c->SetCurrentSelection(sel);
    }
    MainWindowRerender(win);
    return true;
}

// for tests: how many red and blue pixels page 1 has, rendered at 100% the way
// the window renders it (the cached display list)
static TempStr PageRedBlueTemp(EngineBase* engine) {
    if (!engine) {
        return {};
    }
    RenderPageArgs args(1, 1.f, 0, nullptr, RenderTarget::View);
    Pixmap* bmp = engine->RenderPage(args);
    Pixmap* rgb = bmp ? PixmapCopyAs32bppDIB(bmp) : nullptr;
    int red = 0;
    int blue = 0;
    for (int y = 0; rgb && rgb->data && y < rgb->height; y++) {
        const u8* row = rgb->data + ((size_t)y * (size_t)rgb->stride);
        for (int x = 0; x < rgb->width; x++) {
            const u8* px = row + ((size_t)x * 4);
            int b = px[0];
            int g = px[1];
            int r = px[2];
            red += (r > 180 && g < 80 && b < 80) ? 1 : 0;
            blue += (b > 180 && r < 80 && g < 80) ? 1 : 0;
        }
    }
    FreePixmap(rgb);
    FreePixmap(bmp);
    return fmt("red=%d blue=%d\n", red, blue);
}

// for tests: "toggle <row>", "show" or "colors", then one "<depth> <kind> <on>
// <text>" line per row, kind being label, checkbox or radio
TempStr LayersStateTemp(MainWindow* win, Str action) {
    if (str::StartsWith(action, StrL("toggle "))) {
        int i = ParseInt(Str(action.s + 7, len(action) - 7));
        ToggleLayerRow(win, i);
    } else if (str::Eq(action, StrL("show"))) {
        ShowSidebarView(win, SidebarView::Layers);
    }
    str::Builder out;
    if (str::Eq(action, StrL("colors"))) {
        out.Append(PageRedBlueTemp(TabEngine(win->CurrentTab())));
    }
    bool panelShown = SidebarPanelShowing(win, SidebarView::Layers) != nullptr;
    out.Append(fmt("panel=%d\n", panelShown ? 1 : 0));
    out.Append(fmt("states=%s\n", EngineMupdfChangedLayerStatesTemp(TabEngine(win->CurrentTab()))));
    LayersCtrl* c = win->layersCtrl;
    int n = c ? len(c->rows->rows) : 0;
    for (int i = 0; i < n; i++) {
        const PdfLayerRow* row = c->rows->rows[i];
        Str kind = StrL("checkbox");
        if (row->kind == PdfLayerKind::Label) {
            kind = StrL("label");
        } else if (row->kind == PdfLayerKind::Radio) {
            kind = StrL("radio");
        }
        out.Append(fmt("%d %s %d %s\n", row->depth, kind, row->on ? 1 : 0, row->text));
    }
    return ToStrTemp(out);
}
