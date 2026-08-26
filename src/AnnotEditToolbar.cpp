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
#include "gui/GuiColors.h"
#include "gui/VirtCtrl.h"
#include "gui/VirtHost.h"

#include "Settings.h"
#include "Annotation.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "Theme.h"
#include "Translations.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Canvas.h"
#include "Toolbar.h"
#include "AppSettings.h"
#include "EditAnnotations.h"

#include "AnnotEditToolbar.h"

// Compact property row under the selected annotation in Edit PDF mode.
// Same floating-card look as the text-selection toolbar.

constexpr const WCHAR* kAnnotEditToolbarClassName = L"SumatraAnnotEditToolbar";

constexpr int kBtnPadX = 8;
constexpr int kBtnPadY = 4;
constexpr int kMargin = 5;
constexpr int kBtnGap = 2;
constexpr int kCornerRadius = 10;
constexpr int kButtonRadius = 6;
constexpr int kToolbarFontPct = 108;

enum class AnnotEditKind {
    Color,
    InteriorColor,
    Opacity,
    Border,
    FontName,
    TextColor,
    TextSize,
    Alignment,
    Icon,
    Contents,
    LineStart,
    LineEnd,
};

struct AnnotEditItem {
    AnnotEditKind kind = AnnotEditKind::Color;
    Str tooltip;
    Str text;
    PdfColor color = 0;
    int number = 0;
    int lineEnding = 0;
    bool lineIsStart = false;
    Str iconName;
};

struct AnnotEditToolbar;

struct AnnotEditChip : VirtCustom {
    AnnotEditToolbar* tb = nullptr;
    AnnotEditItem item;
    Color hoverBg = kColorUnset;
    Size GetIdealSize() override { return idealSize; }
    void Paint(VirtPaintCtx&) override;
};

struct AnnotEditToolbar {
    MainWindow* win = nullptr;
    WindowTab* tab = nullptr;
    Annotation* annot = nullptr;
    VirtHost* host = nullptr;
    PlatformFont* font = nullptr;
    Size size;
    Rect lastPlaced;
    Rect lastAnnotBounds;
    Vec<AnnotEditKind> kinds;
    Func1List<MainWindow*> onWindowMoved;
};

static bool BarIsDark() {
    return !IsLightColor(ThemeWindowBackgroundColor());
}

static Color BarBg() {
    if (BarIsDark()) {
        return ThemeWindowBackgroundColor();
    }
    Color contentBg;
    ThemePageRenderColors(contentBg);
    return AccentColor(contentBg, 12);
}

static Color BarBorderColor() {
    if (BarIsDark()) {
        return AccentColor(ThemeWindowControlBackgroundColor(), 35);
    }
    return AccentColor(BarBg(), 8);
}

static Color BarTextColor() {
    if (BarIsDark()) {
        return ThemeWindowTextColor();
    }
    return MkRgb(27, 29, 33);
}

static Color BarMutedTextColor() {
    if (BarIsDark()) {
        return ThemeWindowTextDisabledColor();
    }
    return MkRgb(92, 96, 104);
}

static Color BarHoverBg(Color bg) {
    if (BarIsDark()) {
        return AccentColor(ThemeWindowControlBackgroundColor(), 15);
    }
    return AccentColor(bg, 10);
}

static Color PdfToWinColor(PdfColor c) {
    u8 r, g, b, a;
    UnpackPdfColor(c, r, g, b, a);
    return MkRgb(r, g, b);
}

static bool PdfColorIsTransparent(PdfColor c) {
    u8 r, g, b, a;
    UnpackPdfColor(c, r, g, b, a);
    return a == 0;
}

static Str KindName(AnnotEditKind kind) {
    switch (kind) {
        case AnnotEditKind::Color:
            return StrL("color");
        case AnnotEditKind::InteriorColor:
            return StrL("interiorColor");
        case AnnotEditKind::Opacity:
            return StrL("opacity");
        case AnnotEditKind::Border:
            return StrL("border");
        case AnnotEditKind::FontName:
            return StrL("font");
        case AnnotEditKind::TextColor:
            return StrL("textColor");
        case AnnotEditKind::TextSize:
            return StrL("textSize");
        case AnnotEditKind::Alignment:
            return StrL("alignment");
        case AnnotEditKind::Icon:
            return StrL("icon");
        case AnnotEditKind::Contents:
            return StrL("contents");
        case AnnotEditKind::LineStart:
            return StrL("lineStart");
        case AnnotEditKind::LineEnd:
            return StrL("lineEnd");
    }
    return StrL("?");
}

static void CollectItems(Annotation* annot, Vec<AnnotEditItem>& out) {
    out.Reset();
    if (!AnnotationIsLive(annot)) {
        return;
    }
    AnnotationType type = Type(annot);

    if (AnnotationSupportsColor(type)) {
        AnnotEditItem it;
        it.kind = AnnotEditKind::Color;
        it.color = GetColor(annot);
        it.tooltip = _TRA("Color");
        out.Append(it);
    }
    if (AnnotationSupportsInteriorColor(type)) {
        AnnotEditItem it;
        it.kind = AnnotEditKind::InteriorColor;
        it.color = InteriorColor(annot);
        it.tooltip = _TRA("Interior Color");
        out.Append(it);
    }
    if (type == AnnotationType::Highlight) {
        AnnotEditItem it;
        it.kind = AnnotEditKind::Opacity;
        it.number = Opacity(annot);
        it.text = fmt("%d%%", (it.number * 100 + 127) / 255);
        it.tooltip = _TRA("Opacity");
        out.Append(it);
    }
    if (AnnotationSupportsBorder(type)) {
        AnnotEditItem it;
        it.kind = AnnotEditKind::Border;
        it.number = BorderWidth(annot);
        it.text = fmt("%d", it.number);
        it.tooltip = _TRA("Border");
        out.Append(it);
    }
    if (type == AnnotationType::FreeText) {
        {
            AnnotEditItem it;
            it.kind = AnnotEditKind::FontName;
            Str pdfName = DefaultAppearanceTextFont(annot);
            int idx = SeqStrIndex(AnnotEditorFontNames(), pdfName);
            it.number = idx;
            it.text = idx >= 0 ? SeqStrByIndex(AnnotEditorFontReadableNames(), idx) : pdfName;
            it.tooltip = _TRA("Font");
            out.Append(it);
        }
        {
            AnnotEditItem it;
            it.kind = AnnotEditKind::TextColor;
            it.color = DefaultAppearanceTextColor(annot);
            it.tooltip = _TRA("Text Color");
            out.Append(it);
        }
        {
            AnnotEditItem it;
            it.kind = AnnotEditKind::TextSize;
            it.number = DefaultAppearanceTextSize(annot);
            it.text = fmt("%d", it.number);
            it.tooltip = _TRA("Text Size");
            out.Append(it);
        }
        {
            AnnotEditItem it;
            it.kind = AnnotEditKind::Alignment;
            it.number = Quadding(annot);
            it.tooltip = _TRA("Text Alignment");
            out.Append(it);
        }
    }
    SeqStrings icons = AnnotationIconNames(annot);
    if (icons) {
        AnnotEditItem it;
        it.kind = AnnotEditKind::Icon;
        it.iconName = IconName(annot);
        it.tooltip = _TRA("Icon");
        out.Append(it);
    }
    if (type == AnnotationType::Line) {
        int start = 0;
        int end = 0;
        GetLineEndingStyles(annot, &start, &end);
        {
            AnnotEditItem it;
            it.kind = AnnotEditKind::LineStart;
            it.lineEnding = start;
            it.lineIsStart = true;
            it.tooltip = _TRA("Line Start");
            out.Append(it);
        }
        {
            AnnotEditItem it;
            it.kind = AnnotEditKind::LineEnd;
            it.lineEnding = end;
            it.lineIsStart = false;
            it.tooltip = _TRA("Line End");
            out.Append(it);
        }
    }
    if (type != AnnotationType::Widget) {
        AnnotEditItem it;
        it.kind = AnnotEditKind::Contents;
        it.text = StrL("C");
        it.tooltip = _TRA("Contents");
        out.Append(it);
    }
}

static void PaintChecker(Gfx* gfx, Rect r) {
    gfx->FillRect(r, MkRgb(240, 240, 240));
    int s = std::max(DpiScale(3), 2);
    Color dark = MkRgb(200, 200, 200);
    for (int y = 0; y < r.dy; y += s) {
        for (int x = 0; x < r.dx; x += s) {
            if (((x / s) + (y / s)) & 1) {
                int dx = std::min(s, r.dx - x);
                int dy = std::min(s, r.dy - y);
                gfx->FillRect({r.x + x, r.y + y, dx, dy}, dark);
            }
        }
    }
}

static void PaintSwatch(Gfx* gfx, Rect r, PdfColor col, Color border) {
    int inset = DpiScale(4);
    Rect sw = r;
    sw.Inflate(-inset, -inset);
    if (sw.dx < 4 || sw.dy < 4) {
        sw = r;
        sw.Inflate(-2, -2);
    }
    if (PdfColorIsTransparent(col)) {
        PaintChecker(gfx, sw);
    } else {
        gfx->FillRoundedRect(sw, DpiScale(3), PdfToWinColor(col), border);
    }
    gfx->DrawRect(sw, border, 1);
}

static void PaintAlignment(Gfx* gfx, Rect r, int quadding, Color col) {
    int pad = DpiScale(5);
    int x0 = r.x + pad;
    int x1 = r.x + r.dx - pad;
    int mid = r.x + r.dx / 2;
    int y = r.y + pad;
    int gap = std::max((r.dy - 2 * pad) / 4, 2);
    int full = x1 - x0;
    int shortDx = std::max(full / 2, 4);
    for (int i = 0; i < 3; i++) {
        int dx = (i == 1) ? shortDx : full;
        int x = x0;
        if (quadding == kQuaddingCenter) {
            x = mid - dx / 2;
        } else if (quadding == kQuaddingRight) {
            x = x1 - dx;
        }
        gfx->FillRect({x, y, dx, 2}, col);
        y += gap;
    }
}

static void PaintLineEndingMark(Gfx* gfx, Point tip, Point along, Color col, int style, int size) {
    // along is a unit-ish direction from the shaft toward the tip
    int dx = along.x;
    int dy = along.y;
    auto perp = [&](int s) -> Point { return {tip.x - dy * s / size, tip.y + dx * s / size}; };
    auto back = [&](int s) -> Point { return {tip.x - dx * s / size, tip.y - dy * s / size}; };
    switch (style) {
        case 1: { // Square
            Point p = back(size);
            gfx->DrawRect({p.x - size / 2, p.y - size / 2, size, size}, col, 1);
            break;
        }
        case 2: { // Circle
            Point p = back(size / 2);
            gfx->FillEllipse({p.x - size / 2, p.y - size / 2, size, size}, col);
            break;
        }
        case 3: { // Diamond
            Point l = perp(size / 2);
            Point r = perp(-size / 2);
            Point b = back(size);
            gfx->DrawLineAA(tip, l, col, 1.5f);
            gfx->DrawLineAA(tip, r, col, 1.5f);
            gfx->DrawLineAA(l, b, col, 1.5f);
            gfx->DrawLineAA(r, b, col, 1.5f);
            break;
        }
        case 4:   // OpenArrow
        case 7: { // ROpenArrow
            Point wing = (style == 7) ? Point{-dx, -dy} : along;
            Point t = (style == 7) ? back(size) : tip;
            Point base = (style == 7) ? tip : back(size);
            Point l = {base.x - wing.y / 2, base.y + wing.x / 2};
            Point r = {base.x + wing.y / 2, base.y - wing.x / 2};
            gfx->DrawLineAA(t, l, col, 1.5f);
            gfx->DrawLineAA(t, r, col, 1.5f);
            break;
        }
        case 5:   // ClosedArrow
        case 8: { // RClosedArrow
            Point t = (style == 8) ? back(size) : tip;
            Point base = (style == 8) ? tip : back(size);
            Point l = {base.x - dy / 2, base.y + dx / 2};
            Point r = {base.x + dy / 2, base.y - dx / 2};
            gfx->DrawLineAA(t, l, col, 1.5f);
            gfx->DrawLineAA(t, r, col, 1.5f);
            gfx->DrawLineAA(l, r, col, 1.5f);
            break;
        }
        case 6: // Butt
            gfx->DrawLineAA(perp(size / 2), perp(-size / 2), col, 1.5f);
            break;
        case 9: { // Slash
            Point a = {tip.x - size / 2, tip.y - size / 2};
            Point b = {tip.x + size / 2, tip.y + size / 2};
            gfx->DrawLineAA(a, b, col, 1.5f);
            break;
        }
        default:
            break;
    }
}

static void PaintLineEnding(Gfx* gfx, Rect r, int style, bool isStart, Color col) {
    int pad = DpiScale(5);
    int y = r.y + r.dy / 2;
    Point left{r.x + pad, y};
    Point right{r.x + r.dx - pad, y};
    gfx->DrawLineAA(left, right, col, 1.5f);
    int size = DpiScale(8);
    if (isStart) {
        PaintLineEndingMark(gfx, left, {-size, 0}, col, style, size);
    } else {
        PaintLineEndingMark(gfx, right, {size, 0}, col, style, size);
    }
}

static void PaintIconGlyph(Gfx* gfx, Rect r, Str name, Color col, PlatformFont* font) {
    int pad = DpiScale(4);
    Rect inner = r;
    inner.Inflate(-pad, -pad);
    if (str::EqI(name, StrL("Note")) || str::EqI(name, StrL("Comment"))) {
        gfx->DrawRect(inner, col, 1);
        gfx->FillRect({inner.x + 2, inner.y + inner.dy / 3, inner.dx - 4, 1}, col);
        gfx->FillRect({inner.x + 2, inner.y + (2 * inner.dy) / 3, inner.dx / 2, 1}, col);
        return;
    }
    if (str::EqI(name, StrL("PushPin"))) {
        int cx = inner.x + inner.dx / 2;
        int head = std::max(inner.dx / 5, 2);
        gfx->FillEllipse({cx - head, inner.y, head * 2, head * 2}, col);
        gfx->DrawLineAA({cx, inner.y + head * 2}, {cx, inner.y + inner.dy}, col, 1.5f);
        return;
    }
    if (str::EqI(name, StrL("Paperclip"))) {
        gfx->DrawLineAA({inner.x + 2, inner.y + inner.dy - 2}, {inner.x + inner.dx - 2, inner.y + 2}, col, 1.5f);
        return;
    }
    if (str::EqI(name, StrL("Circle"))) {
        gfx->FillEllipse(inner, col);
        return;
    }
    Str label = name;
    if (len(label) > 4) {
        label = Str(name.s, 4);
    }
    gfx->DrawText(label, inner, gfxTextCenter | gfxTextVCenter | gfxTextEllipsis, font, col);
}

void AnnotEditChip::Paint(VirtPaintCtx& ctx) {
    if (IsEnabled() && HasFlag(vwfHovered) && hoverBg != kColorUnset) {
        ctx.gfx->FillRoundedRect(ctx.bounds, DpiScale(kButtonRadius), hoverBg);
    }
    Color textCol = BarTextColor();
    Color border = BarMutedTextColor();
    Rect r = ctx.bounds;
    switch (item.kind) {
        case AnnotEditKind::Color:
        case AnnotEditKind::InteriorColor:
        case AnnotEditKind::TextColor:
            PaintSwatch(ctx.gfx, r, item.color, border);
            break;
        case AnnotEditKind::Alignment:
            PaintAlignment(ctx.gfx, r, item.number, textCol);
            break;
        case AnnotEditKind::Icon:
            PaintIconGlyph(ctx.gfx, r, item.iconName, textCol, tb ? tb->font : nullptr);
            break;
        case AnnotEditKind::LineStart:
        case AnnotEditKind::LineEnd:
            PaintLineEnding(ctx.gfx, r, item.lineEnding, item.lineIsStart, textCol);
            break;
        case AnnotEditKind::Contents:
        case AnnotEditKind::Opacity:
        case AnnotEditKind::Border:
        case AnnotEditKind::FontName:
        case AnnotEditKind::TextSize:
            if (tb && tb->font && item.text) {
                ctx.gfx->DrawText(item.text, r, gfxTextCenter | gfxTextVCenter, tb->font, textCol);
            }
            break;
    }
}

static void AnnotChanged(WindowTab* tab) {
    if (!tab || !tab->win) {
        return;
    }
    NotifyAnnotationsChanged(tab->editAnnotsWindow);
    ToolbarUpdateStateForWindow(tab->win, false);
    MainWindowRerender(tab->win);
    UpdateAnnotEditToolbar(tab->win);
}

static int PopupPick(MainWindow* win, Point screen, const StrVec& names, int current) {
    HMENU menu = CreatePopupMenu();
    if (!menu) {
        return -1;
    }
    for (int i = 0; i < len(names); i++) {
        UINT flags = MF_STRING;
        if (i == current) {
            flags |= MF_CHECKED;
        }
        AppendMenuW(menu, flags, (UINT)(i + 1), ToWStrTemp(names[i]).s);
    }
    int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN, screen.x, screen.y, 0, win->hwndFrame,
                             nullptr);
    DestroyMenu(menu);
    return cmd > 0 ? cmd - 1 : -1;
}

// 32bpp PARGB: themed menus ignore 24-bit DDBs and treat a 32-bit DIB with
// alpha 0 as fully transparent, so GDI FillRect into a DIB is not enough.
static HBITMAP CreateColorSwatchBitmap(PdfColor pdfCol, int dx, int dy) {
    if (dx < 1 || dy < 1) {
        return nullptr;
    }
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = dx;
    bmi.bmiHeader.biHeight = -dy;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp || !bits) {
        DeleteObject(bmp);
        return nullptr;
    }
    u8 r, g, b, a;
    UnpackPdfColor(pdfCol, r, g, b, a);
    auto* px = (u32*)bits;
    int cell = std::max(dx / 4, 2);
    for (int y = 0; y < dy; y++) {
        for (int x = 0; x < dx; x++) {
            u8 pr, pg, pb;
            if (x == 0 || y == 0 || x == dx - 1 || y == dy - 1) {
                pr = pg = pb = 80;
            } else if (a == 0) {
                bool dark = ((x / cell) + (y / cell)) & 1;
                pr = pg = pb = dark ? 180 : 240;
            } else {
                pr = r;
                pg = g;
                pb = b;
            }
            px[y * dx + x] = 0xFF000000u | ((u32)pr << 16) | ((u32)pg << 8) | pb;
        }
    }
    return bmp;
}

static int PopupPickColors(MainWindow* win, Point screen, int current) {
    HMENU menu = CreatePopupMenu();
    if (!menu) {
        return -1;
    }
    Vec<HBITMAP> bmps;
    int n = AnnotEditorColorCount();
    int sw = DpiScale(14);
    for (int i = 0; i < n; i++) {
        HBITMAP bmp = CreateColorSwatchBitmap(AnnotEditorColorAt(i), sw, sw);
        if (bmp) {
            bmps.Append(bmp);
        }
        MENUITEMINFOW mii{};
        mii.cbSize = sizeof(mii);
        mii.fMask = MIIM_ID | MIIM_STRING | MIIM_STATE;
        if (bmp) {
            mii.fMask |= MIIM_BITMAP;
            mii.hbmpItem = bmp;
        }
        mii.wID = (UINT)(i + 1);
        Str name = AnnotEditorColorNameAt(i);
        WCHAR* ws = ToWStrTemp(name).s;
        mii.dwTypeData = ws;
        mii.cch = (UINT)len(name);
        if (i == current) {
            mii.fState = MFS_CHECKED;
        }
        InsertMenuItemW(menu, (UINT)i, TRUE, &mii);
    }
    int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN, screen.x, screen.y, 0, win->hwndFrame,
                             nullptr);
    DestroyMenu(menu);
    for (HBITMAP bmp : bmps) {
        DeleteObject(bmp);
    }
    return cmd > 0 ? cmd - 1 : -1;
}

static int PopupPickSeq(MainWindow* win, Point screen, SeqStrings names, int current) {
    StrVec items;
    for (int off = 0; SeqStrAt(names, off);) {
        items.Append(SeqStrAt(names, off));
        if (!SeqStrAdvance(names, off)) {
            break;
        }
    }
    return PopupPick(win, screen, items, current);
}

static void OnChipClick(AnnotEditChip* chip, VirtMouseEvent*) {
    if (!chip || !chip->tb) {
        return;
    }
    AnnotEditToolbar* tb = chip->tb;
    WindowTab* tab = tb->tab;
    Annotation* annot = tab ? tab->selectedAnnotation : nullptr;
    if (!AnnotationIsLive(annot) || annot != tb->annot) {
        return;
    }
    Rect chipScreen = tb->host->ToScreen(chip->bounds);
    Point screen{chipScreen.x, chipScreen.y + chipScreen.dy};
    AnnotEditKind kind = chip->item.kind;
    switch (kind) {
        case AnnotEditKind::Color:
        case AnnotEditKind::InteriorColor:
        case AnnotEditKind::TextColor: {
            int n = AnnotEditorColorCount();
            int current = -1;
            for (int i = 0; i < n; i++) {
                if (AnnotEditorColorAt(i) == chip->item.color) {
                    current = i;
                    break;
                }
            }
            int idx = PopupPickColors(tb->win, screen, current);
            if (idx < 0) {
                return;
            }
            PdfColor col = AnnotEditorColorAt(idx);
            if (kind == AnnotEditKind::Color) {
                SetColor(annot, col);
            } else if (kind == AnnotEditKind::InteriorColor) {
                SetInteriorColor(annot, col);
            } else {
                SetDefaultAppearanceTextColor(annot, col);
            }
            AnnotChanged(tab);
            break;
        }
        case AnnotEditKind::Opacity: {
            const int vals[] = {64, 128, 191, 255};
            StrVec names;
            int current = -1;
            for (int i = 0; i < dimofi(vals); i++) {
                names.Append(fmt("%d%%", (vals[i] * 100 + 127) / 255));
                if (abs(vals[i] - chip->item.number) < 20) {
                    current = i;
                }
            }
            int idx = PopupPick(tb->win, screen, names, current);
            if (idx < 0) {
                return;
            }
            SetOpacity(annot, vals[idx]);
            AnnotChanged(tab);
            break;
        }
        case AnnotEditKind::Border: {
            const int vals[] = {0, 1, 2, 3, 4, 6, 8, 12};
            StrVec names;
            int current = -1;
            for (int i = 0; i < dimofi(vals); i++) {
                names.Append(fmt("%d", vals[i]));
                if (vals[i] == chip->item.number) {
                    current = i;
                }
            }
            int idx = PopupPick(tb->win, screen, names, current);
            if (idx < 0) {
                return;
            }
            SetBorderWidth(annot, vals[idx]);
            AnnotChanged(tab);
            break;
        }
        case AnnotEditKind::TextSize: {
            const int vals[] = {8, 10, 12, 14, 16, 18, 24, 36};
            StrVec names;
            int current = -1;
            for (int i = 0; i < dimofi(vals); i++) {
                names.Append(fmt("%d", vals[i]));
                if (vals[i] == chip->item.number) {
                    current = i;
                }
            }
            int idx = PopupPick(tb->win, screen, names, current);
            if (idx < 0) {
                return;
            }
            SetDefaultAppearanceTextSize(annot, vals[idx]);
            AnnotChanged(tab);
            break;
        }
        case AnnotEditKind::FontName: {
            int idx = PopupPickSeq(tb->win, screen, AnnotEditorFontReadableNames(), chip->item.number);
            if (idx < 0) {
                return;
            }
            SetDefaultAppearanceTextFont(annot, SeqStrByIndex(AnnotEditorFontNames(), idx));
            AnnotChanged(tab);
            break;
        }
        case AnnotEditKind::Alignment: {
            int idx = PopupPickSeq(tb->win, screen, gQuaddingNames, chip->item.number);
            if (idx < 0) {
                return;
            }
            SetQuadding(annot, idx);
            AnnotChanged(tab);
            break;
        }
        case AnnotEditKind::Icon: {
            SeqStrings icons = AnnotationIconNames(annot);
            int current = SeqStrIndex(icons, chip->item.iconName);
            int idx = PopupPickSeq(tb->win, screen, icons, current);
            if (idx < 0) {
                return;
            }
            SetIconName(annot, SeqStrByIndex(icons, idx));
            AnnotChanged(tab);
            break;
        }
        case AnnotEditKind::LineStart:
        case AnnotEditKind::LineEnd: {
            int idx = PopupPickSeq(tb->win, screen, AnnotEditorLineEndingStyles(), chip->item.lineEnding);
            if (idx < 0) {
                return;
            }
            if (kind == AnnotEditKind::LineStart) {
                SetLineStartStyles(annot, idx);
            } else {
                SetLineEndStyles(annot, idx);
            }
            AnnotChanged(tab);
            break;
        }
        case AnnotEditKind::Contents:
            ShowEditAnnotationsWindow(tab, annot, EditAnnotFocus::Edit);
            break;
    }
}

static void PaintToolbarBg(AnnotEditToolbar*, VirtHostPaintEvent* ev) {
    ev->gfx->FillRoundedRect(ev->clientRect, DpiScale(kCornerRadius), BarBg(), BarBorderColor());
}

static Size ChipSizeFor(const AnnotEditItem& item, PlatformFont* font, int rowDy, int padX) {
    switch (item.kind) {
        case AnnotEditKind::Color:
        case AnnotEditKind::InteriorColor:
        case AnnotEditKind::TextColor:
        case AnnotEditKind::Alignment:
        case AnnotEditKind::Icon:
        case AnnotEditKind::Contents:
            return {rowDy, rowDy};
        case AnnotEditKind::LineStart:
        case AnnotEditKind::LineEnd:
            return {rowDy * 2, rowDy};
        default: {
            Size text = PlatformFontMeasureText(font, item.text ? item.text : StrL("00"));
            return {text.dx + (2 * padX), rowDy};
        }
    }
}

static void LayoutToolbar(AnnotEditToolbar* tb, const Vec<AnnotEditItem>& items) {
    int padX = DpiScale(kBtnPadX);
    int padY = DpiScale(kBtnPadY);
    int margin = DpiScale(kMargin);
    int gap = DpiScale(kBtnGap);
    int textDy = PlatformFontMeasureText(tb->font, StrL("Mg")).dy;
    int rowDy = textDy + (2 * padY);
    Color hoverBg = BarHoverBg(BarBg());

    tb->kinds.Reset();
    auto* box = new HBox();
    box->alignCross = CrossAxisAlign::Stretch;
    bool isFirst = true;
    for (const AnnotEditItem& item : items) {
        auto* chip = new AnnotEditChip();
        chip->tb = tb;
        chip->item = item;
        chip->hoverBg = hoverBg;
        chip->idealSize = ChipSizeFor(item, tb->font, rowDy, padX);
        chip->SetTooltip(item.tooltip);
        chip->onClick = MkFunc1(OnChipClick, chip);
        tb->kinds.Append(item.kind);
        ILayout* child = chip;
        if (!isFirst) {
            child = new Padding(chip, Insets{0, 0, 0, gap});
        }
        isFirst = false;
        box->AddChild(child);
    }
    auto* content = new Padding(box, Insets{margin, margin, margin, margin});
    tb->size = tb->host->SetLayoutSizedToContent(content);
}

static bool GetAnnotScreenBounds(MainWindow* win, Annotation* annot, Rect& out) {
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    if (!dm || !AnnotationIsLive(annot) || !dm->PageVisible(PageNo(annot))) {
        return false;
    }
    Rect canvas = HwndClientRect(win->hwndCanvas);
    Rect r = dm->CvtToScreen(PageNo(annot), GetRect(annot)).Intersect(canvas);
    if (r.IsEmpty()) {
        return false;
    }
    out = r;
    return true;
}

static bool PositionToolbar(AnnotEditToolbar* tb, const Rect& annot) {
    MainWindow* win = tb->win;
    Rect canvas = HwndClientRect(win->hwndCanvas);
    int gap = DpiScale(6);
    int w = tb->size.dx;
    int h = tb->size.dy;

    int x = annot.x;
    int y = annot.y + annot.dy + gap;
    if (y + h > canvas.y + canvas.dy) {
        y = annot.y - gap - h;
    }

    int maxX = canvas.x + canvas.dx - w;
    x = std::min(x, maxX);
    x = std::max(x, canvas.x);
    int maxY = canvas.y + canvas.dy - h;
    y = std::min(y, maxY);
    y = std::max(y, canvas.y);

    Point p = HwndClientToScreen(win->hwndCanvas, Point(x, y));
    Rect placed(p.x, p.y, w, h);
    if (placed == tb->lastPlaced) {
        return false;
    }
    bool sizeChanged = tb->lastPlaced.dx != w || tb->lastPlaced.dy != h;
    tb->lastPlaced = placed;
    tb->host->SetBounds(placed);
    if (sizeChanged) {
        tb->host->ClipToRoundedRect(kCornerRadius, {w, h});
    }
    return true;
}

static AnnotEditToolbar* GetOrCreateToolbar(MainWindow* win) {
    if (win->annotEditToolbar) {
        return win->annotEditToolbar;
    }
    auto* tb = new AnnotEditToolbar();
    tb->win = win;

    VirtHost::CreateArgs args;
    args.parent = win->hwndFrame;
    args.className = WStr(kAnnotEditToolbarClassName);
    args.isPopup = true;
    args.visible = false;
    args.noActivate = true;
    args.userData = tb;

    tb->host = VirtHost::Create(args);
    if (!tb->host) {
        delete tb;
        return nullptr;
    }
    tb->host->onPaintBackground = MkFunc1(PaintToolbarBg, tb);
    tb->font = GetScaledPlatformFont(GetAppFont(), kToolbarFontPct);
    tb->onWindowMoved = MkFunc1Void(RepositionAnnotEditToolbar);
    win->RegisterOnWindowMoved(&tb->onWindowMoved);
    win->annotEditToolbar = tb;
    return tb;
}

void HideAnnotEditToolbar(MainWindow* win) {
    AnnotEditToolbar* tb = win ? win->annotEditToolbar : nullptr;
    if (!tb || !tb->host) {
        return;
    }
    if (tb->host->IsVisible()) {
        tb->host->Show(false);
    }
    if (tb->host->vroot) {
        tb->host->vroot->ClearHover();
        tb->host->vroot->ClearPressed();
    }
    tb->tab = nullptr;
    tb->annot = nullptr;
    tb->lastPlaced = {};
    tb->lastAnnotBounds = {};
    tb->kinds.Reset();
}

void UpdateAnnotEditToolbar(MainWindow* win) {
    if (!win) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    Annotation* annot = tab ? tab->selectedAnnotation : nullptr;
    if (!win->pdfAnnotationsToolbarEnabled || !AnnotationIsLive(annot)) {
        HideAnnotEditToolbar(win);
        return;
    }
    Rect bounds;
    if (!GetAnnotScreenBounds(win, annot, bounds)) {
        HideAnnotEditToolbar(win);
        return;
    }
    AnnotEditToolbar* tb = GetOrCreateToolbar(win);
    if (!tb) {
        return;
    }
    Vec<AnnotEditItem> items;
    CollectItems(annot, items);
    if (len(items) == 0) {
        HideAnnotEditToolbar(win);
        return;
    }
    tb->tab = tab;
    tb->annot = annot;
    tb->lastAnnotBounds = bounds;
    LayoutToolbar(tb, items);
    PositionToolbar(tb, bounds);
    tb->host->Show(true);
    tb->host->Invalidate(false);
}

void RepositionAnnotEditToolbar(MainWindow* win) {
    AnnotEditToolbar* tb = win ? win->annotEditToolbar : nullptr;
    if (!tb || !tb->host || !tb->host->IsVisible()) {
        return;
    }
    Rect bounds;
    if (!GetAnnotScreenBounds(win, tb->annot, bounds)) {
        HideAnnotEditToolbar(win);
        return;
    }
    tb->lastAnnotBounds = bounds;
    PositionToolbar(tb, bounds);
}

void RefreshAnnotEditToolbar(MainWindow* win) {
    AnnotEditToolbar* tb = win ? win->annotEditToolbar : nullptr;
    if (!tb || !tb->host || !tb->host->IsVisible()) {
        return;
    }
    UpdateAnnotEditToolbar(win);
}

void DeleteAnnotEditToolbar(MainWindow* win) {
    AnnotEditToolbar* tb = win ? win->annotEditToolbar : nullptr;
    if (!tb) {
        return;
    }
    win->UnregisterOnWindowMoved(&tb->onWindowMoved);
    delete tb->host;
    delete tb;
    win->annotEditToolbar = nullptr;
}

TempStr AnnotEditToolbarStateTemp(MainWindow* win) {
    AnnotEditToolbar* tb = win ? win->annotEditToolbar : nullptr;
    bool visible = tb && tb->host && tb->host->IsVisible();
    if (!visible) {
        return fmt("annotEditToolbar visible=0 n=0 items=\n");
    }
    str::Builder items;
    for (int i = 0; i < len(tb->kinds); i++) {
        if (i > 0) {
            items.AppendChar(',');
        }
        items.Append(KindName(tb->kinds[i]));
    }
    Rect r = tb->host->ScreenRect();
    return fmt("annotEditToolbar visible=1 n=%d items=%s placed=%d,%d,%d,%d\n", len(tb->kinds), ToStrTemp(items), r.x,
               r.y, r.dx, r.dy);
}
