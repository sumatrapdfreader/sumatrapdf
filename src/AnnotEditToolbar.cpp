/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Pixmap.h"
#include "base/Win.h"
#include "base/UITask.h"
#include "gui/Dpi.h"

extern "C" {
#include <mupdf/pdf.h>
}

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
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "EngineMupdf.h"
#include "DisplayModel.h"
#include "Theme.h"
#include "Translations.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Canvas.h"
#include "Commands.h"
#include "Toolbar.h"
#include "AppSettings.h"
#include "FormFields.h"
#include "FilterHighlightDraw.h"
#include "AnnotFilterToolbar.h"
#include "SvgIcons.h"

#include "AnnotEditToolbar.h"

// Compact property row under the selected annotation in Edit PDF mode.
// Same floating-card look as the text-selection toolbar.

constexpr const WCHAR* kAnnotEditToolbarClassName = L"SumatraAnnotEditToolbar";

constexpr int kBtnPadX = 8;
constexpr int kBtnPadY = 4;
constexpr int kMargin = 5;
constexpr int kBtnGap = 2;
// breathing room above and below the Accept / Cancel row of the contents editor
constexpr int kContentsButtonsRowPad = 4;
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
    Delete,
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

static void StartContentsEdit(AnnotEditToolbar*);
static void EndContentsEdit(AnnotEditToolbar*, bool accept);
static void DestroyContentsEditor(AnnotEditToolbar*);
static void PostedStartContentsEdit(MainWindow*);
static void PostedDeleteSelectedAnnotation(MainWindow*);
static bool FreeTextInPlaceEditJustEnded();

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
    // non-owning, for tests; the layout tree owns them and is replaced on relayout
    Vec<AnnotEditChip*> chips;
    Func1List<MainWindow*> onWindowMoved;
    bool editingContents = false;
    bool contentsEditClosing = false;
    Edit* contentsEdit = nullptr;
};

// Native multiline edit hosted in the VirtHost; its HWND is positioned from
// the layout slot so the card can keep the floating rounded-rect look.
struct ContentsEditSlot : VirtCustom {
    Edit* edit = nullptr;
    Size GetIdealSize() override {
        if (!edit) {
            return idealSize;
        }
        Size s = edit->GetIdealSize();
        s.dx = std::max(s.dx, idealSize.dx);
        s.dy = std::max(s.dy, idealSize.dy);
        return s;
    }
    void SetBounds(Rect r) override {
        VirtCtrl::SetBounds(r);
        if (edit) {
            edit->SetBounds(r);
        }
    }
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
    u8 r;
    u8 g;
    u8 b;
    u8 a;
    UnpackPdfColor(c, r, g, b, a);
    return MkRgb(r, g, b);
}

static bool PdfColorIsTransparent(PdfColor c) {
    u8 r;
    u8 g;
    u8 b;
    u8 a;
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
        case AnnotEditKind::Delete:
            return StrL("delete");
    }
    return StrL("?");
}

// annotation types whose GetColor() is a background, not the ink color
static bool AnnotationColorIsBackground(AnnotationType tp) {
    return tp == AnnotationType::FreeText;
}

static void CollectItems(Annotation* annot, Vec<AnnotEditItem>& out) {
    VecReset(out);
    if (!AnnotationIsLive(annot)) {
        return;
    }
    AnnotationType type = Type(annot);
    bool isFreeText = type == AnnotationType::FreeText;

    // free text is about its text, so the text's color leads
    if (isFreeText) {
        AnnotEditItem it;
        it.kind = AnnotEditKind::TextColor;
        it.color = DefaultAppearanceTextColor(annot);
        it.tooltip = _TRA("Text Color");
        VecAppend(out, it);
    }
    if (AnnotationSupportsColor(type)) {
        AnnotEditItem it;
        it.kind = AnnotEditKind::Color;
        it.color = GetColor(annot);
        it.tooltip = AnnotationColorIsBackground(type) ? _TRA("Background Color") : _TRA("Color");
        VecAppend(out, it);
    }
    if (AnnotationSupportsInteriorColor(type)) {
        AnnotEditItem it;
        it.kind = AnnotEditKind::InteriorColor;
        it.color = InteriorColor(annot);
        it.tooltip = _TRA("Interior Color");
        VecAppend(out, it);
    }
    if (AnnotationSupportsOpacity(type)) {
        AnnotEditItem it;
        it.kind = AnnotEditKind::Opacity;
        it.number = Opacity(annot);
        // for free text the whole appearance is the text, background aside
        it.tooltip = isFreeText ? _TRA("Text Opacity") : _TRA("Opacity");
        VecAppend(out, it);
    }
    if (AnnotationSupportsBorder(type)) {
        AnnotEditItem it;
        it.kind = AnnotEditKind::Border;
        it.number = BorderWidth(annot);
        it.tooltip = _TRA("Border Width");
        VecAppend(out, it);
    }
    if (isFreeText) {
        {
            AnnotEditItem it;
            it.kind = AnnotEditKind::FontName;
            Str pdfName = DefaultAppearanceTextFont(annot);
            int idx = SeqStrIndex(AnnotEditorFontNames(), pdfName);
            it.number = idx;
            it.text = idx >= 0 ? SeqStrByIndex(AnnotEditorFontReadableNames(), idx) : pdfName;
            it.tooltip = _TRA("Font");
            VecAppend(out, it);
        }
        {
            AnnotEditItem it;
            it.kind = AnnotEditKind::TextSize;
            it.number = DefaultAppearanceTextSize(annot);
            it.tooltip = _TRA("Text Size");
            VecAppend(out, it);
        }
        {
            AnnotEditItem it;
            it.kind = AnnotEditKind::Alignment;
            it.number = Quadding(annot);
            it.tooltip = _TRA("Text Alignment");
            VecAppend(out, it);
        }
    }
    SeqStrings icons = AnnotationIconNames(annot);
    if (icons) {
        AnnotEditItem it;
        it.kind = AnnotEditKind::Icon;
        it.iconName = IconName(annot);
        it.tooltip = _TRA("Icon");
        VecAppend(out, it);
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
            VecAppend(out, it);
        }
        {
            AnnotEditItem it;
            it.kind = AnnotEditKind::LineEnd;
            it.lineEnding = end;
            it.lineIsStart = false;
            it.tooltip = _TRA("Line End");
            VecAppend(out, it);
        }
    }
    if (type != AnnotationType::Widget) {
        AnnotEditItem it;
        it.kind = AnnotEditKind::Contents;
        it.tooltip = _TRA("Edit text");
        VecAppend(out, it);
    }
    if (type != AnnotationType::Widget) {
        // last, so a mis-aimed click lands on a harmless chip, not on delete
        AnnotEditItem it;
        it.kind = AnnotEditKind::Delete;
        it.tooltip = _TRA("Delete Annotation");
        VecAppend(out, it);
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
    int pad = DpiScale(6);
    Rect inner = r;
    inner.Inflate(-pad, -pad);
    if (inner.dx < 6 || inner.dy < 8) {
        inner = r;
        inner.Inflate(-2, -2);
    }
    int lineH = std::max(DpiScale(2), 1);
    int gap = std::max((inner.dy - 3 * lineH) / 2, 1);
    int blockDy = 3 * lineH + 2 * gap;
    int y = inner.y + (inner.dy - blockDy) / 2;
    int full = inner.dx;
    int shortDx = std::max((full * 2) / 3, 4);
    for (int i = 0; i < 3; i++) {
        int dx = (i == 1) ? shortDx : full;
        int x = inner.x;
        if (quadding == kQuaddingCenter) {
            x = inner.x + (inner.dx - dx) / 2;
        } else if (quadding == kQuaddingRight) {
            x = inner.x + inner.dx - dx;
        }
        gfx->FillRect({x, y, dx, lineH}, col);
        y += lineH + gap;
    }
}

static void PaintSvgChip(Gfx* gfx, Rect r, const char* svg, Color fg, Color bg) {
    int pad = DpiScale(3);
    int sz = std::min(r.dx, r.dy) - 2 * pad;
    if (sz < 8) {
        sz = std::min(r.dx, r.dy);
    }
    if (sz < 1) {
        return;
    }
    Pixmap* px = GetCachedPixmapForSvg(Str(svg), sz, sz, fg, bg);
    if (!px) {
        return;
    }
    int x = r.x + (r.dx - px->width) / 2;
    int y = r.y + (r.dy - px->height) / 2;
    gfx->DrawPixmap(px, {x, y, px->width, px->height});
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

static TempStr ChipLabelTemp(const AnnotEditItem& item) {
    switch (item.kind) {
        case AnnotEditKind::Opacity:
            return fmt("%d%%", (item.number * 100 + 127) / 255);
        case AnnotEditKind::Border:
        case AnnotEditKind::TextSize:
            return fmt("%d", item.number);
        default:
            return item.text;
    }
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
            PaintSvgChip(ctx.gfx, r, gIconAnnotText, textCol, BarBg());
            break;
        case AnnotEditKind::Delete:
            PaintSvgChip(ctx.gfx, r, gIconTrash, textCol, BarBg());
            break;
        case AnnotEditKind::Opacity:
        case AnnotEditKind::Border:
        case AnnotEditKind::FontName:
        case AnnotEditKind::TextSize: {
            Str label = ChipLabelTemp(item);
            if (tb && tb->font && label) {
                ctx.gfx->DrawText(label, r, gfxTextCenter | gfxTextVCenter, tb->font, textCol);
            }
            break;
        }
    }
}

static void AnnotChanged(WindowTab* tab) {
    if (!tab || !tab->win) {
        return;
    }
    NotifyAnnotationsChanged(tab);
    ToolbarUpdateStateForWindow(tab->win, false);
    MainWindowRerender(tab->win);
    UpdateAnnotEditToolbar(tab->win);
    UpdateAnnotFilterToolbar(tab->win);
}

// The click that dismisses a TrackPopupMenu is then delivered to the chip
// under the cursor and would open the menu again. Eat it when it landed on
// the chip that opened this popup; a click on a different chip still opens
// that chip's menu.
static void EatDismissClickOverRect(Rect screenRect) {
    POINT pt;
    GetCursorPos(&pt);
    if (!screenRect.Contains(pt.x, pt.y)) {
        return;
    }
    MSG msg{};
    while (PeekMessageW(&msg, nullptr, WM_LBUTTONDOWN, WM_LBUTTONDOWN, PM_REMOVE)) {
    }
    while (PeekMessageW(&msg, nullptr, WM_LBUTTONUP, WM_LBUTTONUP, PM_REMOVE)) {
    }
}

static AnnotEditToolbar* gPopupTb = nullptr;
static AnnotEditKind gPopupKind = AnnotEditKind::Color;
static u64 gPopupDismissedAt = 0;

static bool SameChipClickDismissedPopup(AnnotEditToolbar* tb, AnnotEditKind kind) {
    if (!tb || tb != gPopupTb || kind != gPopupKind || gPopupDismissedAt == 0) {
        return false;
    }
    return (GetTickCount64() - gPopupDismissedAt) < 400;
}

static void NotePopupDismissed(AnnotEditToolbar* tb, AnnotEditKind kind) {
    gPopupTb = tb;
    gPopupKind = kind;
    gPopupDismissedAt = GetTickCount64();
}

static int PopupPick(MainWindow* win, Point screen, const StrVec& names, int current, Rect chipScreen) {
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
    EatDismissClickOverRect(chipScreen);
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
    u8 r;
    u8 g;
    u8 b;
    u8 a;
    UnpackPdfColor(pdfCol, r, g, b, a);
    auto* px = (u32*)bits;
    int cell = std::max(dx / 4, 2);
    for (int y = 0; y < dy; y++) {
        for (int x = 0; x < dx; x++) {
            u8 pr;
            u8 pg;
            u8 pb;
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

static int PopupPickColors(MainWindow* win, Point screen, int current, Rect chipScreen) {
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
            VecAppend(bmps, bmp);
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
    EatDismissClickOverRect(chipScreen);
    return cmd > 0 ? cmd - 1 : -1;
}

static int PopupPickSeq(MainWindow* win, Point screen, SeqStrings names, int current, Rect chipScreen) {
    StrVec items;
    for (int off = 0; SeqStrAt(names, off);) {
        items.Append(SeqStrAt(names, off));
        if (!SeqStrAdvance(names, off)) {
            break;
        }
    }
    return PopupPick(win, screen, items, current, chipScreen);
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
    if (kind != AnnotEditKind::Contents && SameChipClickDismissedPopup(tb, kind)) {
        return;
    }
    auto dismissed = [&](int idx) -> bool {
        if (idx >= 0) {
            return false;
        }
        NotePopupDismissed(tb, kind);
        return true;
    };
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
            int idx = PopupPickColors(tb->win, screen, current, chipScreen);
            if (dismissed(idx)) {
                return;
            }
            PdfColor col = AnnotEditorColorAt(idx);
            if (kind == AnnotEditKind::Color) {
                // SetColor() takes the annotation's opacity from the color's
                // alpha and the palette is all-opaque, so picking a color would
                // throw away what the Opacity chip set
                if (col != 0) {
                    u8 r;
                    u8 g;
                    u8 b;
                    u8 a;
                    UnpackPdfColor(col, r, g, b, a);
                    u8 opacity = (u8)Opacity(annot);
                    if (opacity == 0) {
                        // coming back from Transparent, which set opacity to 0;
                        // keeping it would make the color picked here invisible
                        opacity = 255;
                    }
                    col = MkPdfColor(r, g, b, opacity);
                }
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
            int idx = PopupPick(tb->win, screen, names, current, chipScreen);
            if (dismissed(idx)) {
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
            int idx = PopupPick(tb->win, screen, names, current, chipScreen);
            if (dismissed(idx)) {
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
            int idx = PopupPick(tb->win, screen, names, current, chipScreen);
            if (dismissed(idx)) {
                return;
            }
            SetDefaultAppearanceTextSize(annot, vals[idx]);
            AnnotChanged(tab);
            break;
        }
        case AnnotEditKind::FontName: {
            int idx = PopupPickSeq(tb->win, screen, AnnotEditorFontReadableNames(), chip->item.number, chipScreen);
            if (dismissed(idx)) {
                return;
            }
            SetDefaultAppearanceTextFont(annot, SeqStrByIndex(AnnotEditorFontNames(), idx));
            AnnotChanged(tab);
            break;
        }
        case AnnotEditKind::Alignment: {
            int idx = PopupPickSeq(tb->win, screen, gQuaddingNames, chip->item.number, chipScreen);
            if (dismissed(idx)) {
                return;
            }
            SetQuadding(annot, idx);
            AnnotChanged(tab);
            break;
        }
        case AnnotEditKind::Icon: {
            SeqStrings icons = AnnotationIconNames(annot);
            int current = SeqStrIndex(icons, chip->item.iconName);
            int idx = PopupPickSeq(tb->win, screen, icons, current, chipScreen);
            if (dismissed(idx)) {
                return;
            }
            SetIconName(annot, SeqStrByIndex(icons, idx));
            AnnotChanged(tab);
            break;
        }
        case AnnotEditKind::LineStart:
        case AnnotEditKind::LineEnd: {
            int idx = PopupPickSeq(tb->win, screen, AnnotEditorLineEndingStyles(), chip->item.lineEnding, chipScreen);
            if (dismissed(idx)) {
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
            // the chip lives in the layout tree we replace; wait until this click
            // has finished bubbling
            uitask::Post(MkFunc0(PostedStartContentsEdit, tb->win), "StartAnnotContentsEdit");
            break;
        case AnnotEditKind::Delete:
            // deleting takes the whole toolbar (and this chip) down with it
            uitask::Post(MkFunc0(PostedDeleteSelectedAnnotation, tb->win), "DeleteSelectedAnnot");
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
        case AnnotEditKind::Delete:
            return {rowDy, rowDy};
        case AnnotEditKind::LineStart:
        case AnnotEditKind::LineEnd:
            return {rowDy * 2, rowDy};
        default: {
            Str label = ChipLabelTemp(item);
            Size text = PlatformFontMeasureText(font, label ? label : StrL("00"));
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

    VecReset(tb->kinds);
    VecReset(tb->chips);
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
        VecAppend(tb->kinds, item.kind);
        VecAppend(tb->chips, chip);
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

// tb->annot is non-owning. Save/reload frees the wrapper and only
// tab->selectedAnnotation is cleared in that path, so compare that first
// and never call AnnotationIsLive on tb->annot alone.
static Annotation* LiveToolbarAnnot(AnnotEditToolbar* tb) {
    if (!tb || !tb->win) {
        return nullptr;
    }
    WindowTab* tab = tb->win->CurrentTab();
    Annotation* annot = tab ? tab->selectedAnnotation : nullptr;
    if (!annot || annot != tb->annot || tab != tb->tab) {
        return nullptr;
    }
    if (!AnnotationIsLive(annot)) {
        return nullptr;
    }
    return annot;
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

static void SetHostNoActivate(VirtHost* host, bool noActivate) {
    if (!host || !host->native) {
        return;
    }
    host->noActivate = noActivate;
    HWND hwnd = host->native;
    LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if (noActivate) {
        ex |= WS_EX_NOACTIVATE;
    } else {
        ex &= ~WS_EX_NOACTIVATE;
    }
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, ex);
    UINT flags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED;
    if (noActivate) {
        flags |= SWP_NOACTIVATE;
    }
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, flags);
}

static void DestroyContentsEditor(AnnotEditToolbar* tb) {
    if (!tb) {
        return;
    }
    // clear the state before the delete: destroying the box takes its focus
    // away, and that WM_KILLFOCUS comes back through OnContentsEditWndProc
    Edit* edit = tb->contentsEdit;
    tb->contentsEdit = nullptr;
    tb->editingContents = false;
    tb->contentsEditClosing = false;
    delete edit;
    if (tb->host) {
        SetHostNoActivate(tb->host, true);
    }
}

static void RestoreCanvasFocus(AnnotEditToolbar* tb) {
    if (tb && tb->win && tb->win->hwndCanvas) {
        HwndSetFocus(tb->win->hwndCanvas);
    }
}

static void EndContentsEdit(AnnotEditToolbar* tb, bool accept) {
    if (!tb || !tb->editingContents) {
        return;
    }
    WindowTab* tab = tb->tab;
    Annotation* annot = LiveToolbarAnnot(tb);
    TempStr newText{};
    if (accept && tb->contentsEdit && annot) {
        newText = tb->contentsEdit->GetTextTemp();
        newText = str::ReplaceTemp(newText, StrL("\r\n"), StrL("\n"));
    }
    RestoreCanvasFocus(tb);
    DestroyContentsEditor(tb);
    if (accept && annot) {
        SetContents(annot, newText);
        AnnotChanged(tab);
        return;
    }
    if (tab && tab->win) {
        UpdateAnnotEditToolbar(tab->win);
    }
}

static void PostedAcceptContents(MainWindow* win) {
    if (!win || !win->annotEditToolbar) {
        return;
    }
    EndContentsEdit(win->annotEditToolbar, true);
}

static void PostedCancelContents(MainWindow* win) {
    if (!win || !win->annotEditToolbar) {
        return;
    }
    EndContentsEdit(win->annotEditToolbar, false);
}

static void QueueEndContentsEdit(AnnotEditToolbar* tb, bool accept) {
    if (!tb || tb->contentsEditClosing || !tb->win) {
        return;
    }
    tb->contentsEditClosing = true;
    if (accept) {
        uitask::Post(MkFunc0(PostedAcceptContents, tb->win), "AcceptAnnotContents");
    } else {
        uitask::Post(MkFunc0(PostedCancelContents, tb->win), "CancelAnnotContents");
    }
}

// A click that ends a contents edit is spent on ending it: it must not also
// deselect the annotation the text was just written to. Same idea as
// gInPlaceEndedAt for the free text editor on the page.
static u64 gContentsEditEndedAt = 0;
constexpr u64 kContentsEditJustEndedMs = 400;

static void OnAcceptContentsClick(AnnotEditToolbar* tb, VirtMouseEvent*) {
    QueueEndContentsEdit(tb, true);
}

static void OnCancelContentsClick(AnnotEditToolbar* tb, VirtMouseEvent*) {
    QueueEndContentsEdit(tb, false);
}

static void OnContentsEditWndProc(AnnotEditToolbar* tb, ControlBase::WndProcEvent* ev) {
    if (!tb || !tb->contentsEdit) {
        return;
    }
    if (ev->msg == WM_CHAR) {
        // Ctrl+Enter is LF; Esc also arrives as WM_CHAR. Eat both so they
        // don't insert text after KEYDOWN queued accept/cancel.
        if (ev->wparam == VK_ESCAPE || ev->wparam == 0x0A) {
            ev->didHandle = true;
            ev->result = 0;
            return;
        }
    }
    if (ev->msg == WM_KEYDOWN) {
        if (ev->wparam == VK_ESCAPE) {
            ev->didHandle = true;
            ev->result = 0;
            QueueEndContentsEdit(tb, false);
            return;
        }
        if (ev->wparam == VK_RETURN && IsCtrlPressed()) {
            ev->didHandle = true;
            ev->result = 0;
            QueueEndContentsEdit(tb, true);
            return;
        }
    }
    if (ev->msg == WM_KILLFOCUS) {
        // clicking away from the box is "done", the same as Ctrl+Enter
        QueueEndContentsEdit(tb, true);
        gContentsEditEndedAt = GetTickCount64();
    }
    tb->contentsEdit->WndProc(ev);
}

static void OnHostNativeMsg(AnnotEditToolbar* tb, VirtHostNativeMsg* ev) {
    if (!tb || !tb->contentsEdit || !tb->contentsEdit->hwnd) {
        return;
    }
    HWND child = nullptr;
    if (ev->msg == WM_COMMAND) {
        child = (HWND)ev->lp;
        if (child == tb->contentsEdit->hwnd) {
            tb->contentsEdit->DispatchCommand(ev->wp, ev->lp);
            ev->didHandle = true;
            ev->res = 0;
        }
        return;
    }
    if (ev->msg == WM_CTLCOLOREDIT || ev->msg == WM_CTLCOLORSTATIC) {
        child = (HWND)ev->lp;
        if (child == tb->contentsEdit->hwnd) {
            ev->res = tb->contentsEdit->DispatchMessageReflect(ev->msg, ev->wp, ev->lp);
            ev->didHandle = ev->res != 0;
        }
    }
}

// Themed button whose caption is a translated label plus a key-cap shortcut,
// using the same (Kbd/...) rendering as the keyboard-help sheet. The rich
// text is owned here (not a child) so hover and click stay on the button.
struct ButtonWithKbd : VirtButton {
    VirtRichText* kbdLabel = nullptr;

    explicit ButtonWithKbd(PlatformFont* f) : VirtButton({}, f) {}
    ~ButtonWithKbd() override;

    Size GetIdealSize() override;
    void Paint(VirtPaintCtx&) override;
};

ButtonWithKbd::~ButtonWithKbd() {
    delete kbdLabel;
}

Size ButtonWithKbd::GetIdealSize() {
    Size s2 = kbdLabel ? kbdLabel->GetIdealSize() : VirtText::GetIdealSize();
    return {s2.dx + textPadding.left + textPadding.right, s2.dy + textPadding.top + textPadding.bottom};
}

void ButtonWithKbd::Paint(VirtPaintCtx& ctx) {
    VirtButton::Paint(ctx);
    if (!kbdLabel) {
        return;
    }
    bool isEnabled = HasFlag(vwfEnabled);
    Color bg = GetColor((isEnabled && HasFlag(vwfHovered)) ? kColBtnBgHover : kColBtnBg);
    Color textCol = GetColor(kColBtnText);
    if (!isEnabled) {
        Color disabled = GetColor(kColBtnTextDisabled);
        if (disabled != kColorUnset) {
            textCol = disabled;
        }
    }
    Rect r = ctx.content;
    r.SubTB(textPadding.top, textPadding.bottom);
    r.SubLR(textPadding.left, textPadding.right);
    Size ks = kbdLabel->GetIdealSize();
    int x = r.x + std::max(0, (r.dx - ks.dx) / 2);
    int y = r.y + std::max(0, (r.dy - ks.dy) / 2);
    kbdLabel->SetColor(kColRichText, textCol);
    kbdLabel->SetColor(kColRichLink, textCol);
    kbdLabel->SetColor(kColRichBg, bg);
    kbdLabel->SetBounds({x, y, ks.dx, ks.dy});
    kbdLabel->PaintStandalone(ctx.gfx);
}

static ButtonWithKbd* NewButtonWithKbd(HWND hwndForDpi, Str label, Str shortcut, PlatformFont* font, bool isDefault) {
    DpiSetFromHwnd(hwndForDpi);
    auto* b = new ButtonWithKbd(font);
    b->SetIsDefault(isDefault);
    b->textPadding = DpiScaledInsets(2, 10);
    auto* rich = new VirtRichText();
    rich->font = font;
    ParseTipInto(rich, fmt("%s (Kbd/%s)", label, shortcut));
    b->kbdLabel = rich;
    return b;
}

static void LayoutContentsEditor(AnnotEditToolbar* tb) {
    int margin = DpiScale(kMargin);
    int gap = DpiScale(kBtnGap);
    Rect canvas = HwndClientRect(tb->win->hwndCanvas);
    int wantDx = std::max(tb->lastAnnotBounds.dx, DpiScale(320));
    wantDx = std::min(wantDx, std::max(canvas.dx - DpiScale(24), DpiScale(200)));

    tb->contentsEdit->idealDx = wantDx;
    auto* slot = new ContentsEditSlot();
    slot->edit = tb->contentsEdit;
    slot->idealSize = {wantDx, tb->contentsEdit->GetIdealSize().dy};

    auto* buttons = new HBox();
    buttons->alignMain = MainAxisAlign::MainStart;
    buttons->alignCross = CrossAxisAlign::CrossCenter;
    buttons->gap = gap;
    auto* btnAccept = NewButtonWithKbd(tb->host->native, _TRA("Accept"), StrL("Ctrl + Enter"), tb->font, true);
    btnAccept->onClick = MkFunc1(OnAcceptContentsClick, tb);
    auto* btnCancel = NewButtonWithKbd(tb->host->native, _TRA("Cancel"), StrL("Esc"), tb->font, false);
    btnCancel->onClick = MkFunc1(OnCancelContentsClick, tb);
    buttons->AddChild(btnAccept);
    buttons->AddChild(btnCancel);

    auto* vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;
    vbox->gap = gap;
    int rowPad = DpiScale(kContentsButtonsRowPad);
    vbox->AddChild(slot);
    vbox->AddChild(new Padding(buttons, Insets{rowPad, 0, rowPad, 0}));

    auto* content = new Padding(vbox, Insets{margin, margin, margin, margin});
    tb->size = tb->host->SetLayoutSizedToContent(content);
    VecReset(tb->kinds);
    // SetLayoutSizedToContent freed the property row that owned them
    VecReset(tb->chips);
}

static void PostedStartContentsEdit(MainWindow* win) {
    AnnotEditToolbar* tb = win ? win->annotEditToolbar : nullptr;
    if (!tb) {
        return;
    }
    // free text is edited on the page, in the annotation's own box, and the
    // same button ends it
    if (IsEditingFreeTextInPlace(win)) {
        EndFreeTextInPlaceEdit(true);
        return;
    }
    // clicking the button took focus off the box, which already ended the
    // edit; this click means "done", not "start again"
    if (FreeTextInPlaceEditJustEnded()) {
        return;
    }
    Annotation* annot = LiveToolbarAnnot(tb);
    if (annot && Type(annot) == AnnotationType::FreeText && StartFreeTextInPlaceEdit(win, annot)) {
        return;
    }
    StartContentsEdit(tb);
}

static void PostedDeleteSelectedAnnotation(MainWindow* win) {
    AnnotEditToolbar* tb = win ? win->annotEditToolbar : nullptr;
    Annotation* annot = LiveToolbarAnnot(tb);
    if (annot) {
        DeleteAnnotationAndUpdateUI(tb->tab, annot);
    }
}

static void StartContentsEdit(AnnotEditToolbar* tb) {
    Annotation* annot = LiveToolbarAnnot(tb);
    if (!tb || !tb->host || !annot || tb->editingContents) {
        return;
    }
    Edit::CreateArgs args;
    args.parent = tb->host->native;
    args.isMultiLine = true;
    args.withFrame = true;
    args.idealSizeLines = 5;
    args.textPadding = 3;
    args.font = tb->font;
    args.isRtl = IsUIRtl();
    auto* edit = new Edit();
    Color bg = BarIsDark() ? ThemeWindowControlBackgroundColor() : MkRgb(255, 255, 255);
    edit->SetColors(BarTextColor(), bg);
    HWND hwnd = edit->Create(args);
    if (!hwnd) {
        delete edit;
        return;
    }
    Str s = Contents(annot);
    s = str::ReplaceTemp(s, StrL("\r\n"), StrL("\n"));
    s = str::ReplaceTemp(s, StrL("\n"), StrL("\r\n"));
    edit->SetText(s);
    edit->onWndProc = MkFunc1(OnContentsEditWndProc, tb);

    tb->contentsEdit = edit;
    tb->editingContents = true;
    tb->contentsEditClosing = false;
    SetHostNoActivate(tb->host, false);
    LayoutContentsEditor(tb);
    PositionToolbar(tb, tb->lastAnnotBounds);
    tb->host->Show(true);
    tb->host->Invalidate(false);
    SetActiveWindow(tb->host->native);
    edit->SetFocus();
    EditSetCursorPosAtEnd(edit);
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
    HWND hwnd = tb->host->native;
    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    SetWindowLongPtrW(hwnd, GWL_STYLE, style | WS_CLIPCHILDREN);
    tb->host->onPaintBackground = MkFunc1(PaintToolbarBg, tb);
    tb->host->onNativeMsg = MkFunc1(OnHostNativeMsg, tb);
    tb->font = GetScaledPlatformFont(GetAppFont(), kToolbarFontPct);
    tb->onWindowMoved = MkFunc1Void(RepositionAnnotEditToolbar);
    win->RegisterOnWindowMoved(&tb->onWindowMoved);
    win->annotEditToolbar = tb;
    return tb;
}

//--- in-place free text editing

// A plain edit control sits exactly over the free text annotation while you
// type: same font and size, on a white background so the text stays readable
// whatever is underneath. Enter makes a new line; Ctrl+Enter, Esc or clicking
// away ends it and the rendered annotation comes back.
struct FreeTextInPlaceEdit {
    HWND hwnd = nullptr;
    HFONT font = nullptr;
    MainWindow* win = nullptr;
    WindowTab* tab = nullptr;
    Annotation* annot = nullptr;
    Size size;
    Size minSize;
    int padding = 0;
    // what MuPDF will lay the text out with
    int textSize = 12;
    int borderWidth = 0;
    int fontPx = 0;
    float scale = 1.f;
};

// MuPDF stacks free text lines 1.2 * the font size apart and wraps at the
// rect width less 2 * (2 * border width) (pdf_write_free_text_appearance).
constexpr float kFreeTextLineHeight = 1.2f;
// GDI's Arial is not quite MuPDF's Helvetica; leave room rather than let the
// annotation wrap a line the box showed whole
constexpr float kInPlaceWidthSlack = 1.03f;

static FreeTextInPlaceEdit gInPlace;
static WNDPROC gInPlaceDefProc = nullptr;
static bool gInPlaceEnding = false;
// so the click that closes the box by taking focus away doesn't reopen it
static u64 gInPlaceEndedAt = 0;

static bool FreeTextInPlaceEditJustEnded() {
    return gInPlaceEndedAt != 0 && (GetTickCount64() - gInPlaceEndedAt) < kContentsEditJustEndedMs;
}

bool AnnotContentsEditJustEnded() {
    if (gContentsEditEndedAt != 0 && (GetTickCount64() - gContentsEditEndedAt) < kContentsEditJustEndedMs) {
        return true;
    }
    return FreeTextInPlaceEditJustEnded();
}

bool IsEditingFreeTextInPlace(MainWindow* win) {
    if (!gInPlace.hwnd) {
        return false;
    }
    return !win || gInPlace.win == win;
}

// Arial / Courier New / Times New Roman have the metrics of the base 14 fonts
// MuPDF renders free text with.
static const WCHAR* WinFontForPdfFontName(Str pdfName) {
    if (str::EqI(pdfName, StrL("Cour"))) {
        return L"Courier New";
    }
    if (str::EqI(pdfName, StrL("TiRo"))) {
        return L"Times New Roman";
    }
    return L"Arial";
}

static Size MeasureInPlaceText(Str text) {
    if (!gInPlace.hwnd) {
        return {};
    }
    HDC hdc = GetDC(gInPlace.hwnd);
    uint format = DT_CALCRECT | DT_NOPREFIX | DT_EDITCONTROL | DT_NOCLIP;
    Size s = HdcMeasureText(hdc, text, 0, format, gInPlace.font);
    ReleaseDC(gInPlace.hwnd, hdc);
    return s;
}

// The box follows what has been typed. It never shrinks below the annotation's
// own box, so committing can only ever grow the annotation.
static void SizeInPlaceEditToText() {
    if (!gInPlace.hwnd || !gInPlace.win) {
        return;
    }
    TempStr text = HwndGetTextTemp(gInPlace.hwnd);
    Size ts = MeasureInPlaceText(text);
    int pad = gInPlace.padding;
    // room for the caret past the last glyph
    int caret = std::max(DpiScale(3), 2);
    // big enough for the edit control to show the text...
    Size want;
    want.dx = ts.dx + (2 * pad) + caret;
    want.dy = ts.dy + (2 * pad);

    // ...and big enough for MuPDF to lay it out the same way once the box is
    // gone. It measures in PDF points, so take the pixel measurement back to
    // ems and re-apply the annotation's own font size: the pixel size is a
    // rounded version of it, and that rounding alone was enough to make the
    // annotation wrap a line the box had shown whole.
    float emDx = gInPlace.fontPx > 0 ? ((float)ts.dx / (float)gInPlace.fontPx) : 0.f;
    float padPts = 4.f * (float)gInPlace.borderWidth;
    int nLines = std::max((int)SendMessageW(gInPlace.hwnd, EM_GETLINECOUNT, 0, 0), 1);
    float dxPts = (emDx * (float)gInPlace.textSize * kInPlaceWidthSlack) + padPts;
    float dyPts = ((float)nLines * kFreeTextLineHeight * (float)gInPlace.textSize) + padPts;
    want.dx = std::max(want.dx, (int)(dxPts * gInPlace.scale) + caret);
    want.dy = std::max(want.dy, (int)(dyPts * gInPlace.scale));

    want.dx = std::max(want.dx, gInPlace.minSize.dx);
    want.dy = std::max(want.dy, gInPlace.minSize.dy);
    Rect canvas = HwndClientRect(gInPlace.win->hwndCanvas);
    Rect cur = ChildPosWithinParent(gInPlace.hwnd);
    want.dx = std::min(want.dx, std::max(canvas.dx - cur.x, gInPlace.minSize.dx));
    want.dy = std::min(want.dy, std::max(canvas.dy - cur.y, gInPlace.minSize.dy));
    if (want == gInPlace.size) {
        return;
    }
    gInPlace.size = want;
    SetWindowPos(gInPlace.hwnd, nullptr, 0, 0, want.dx, want.dy, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// Scrolling and zooming move the annotation out from under the box.
void RepositionFreeTextInPlaceEdit(MainWindow* win) {
    if (!IsEditingFreeTextInPlace(win)) {
        return;
    }
    DisplayModel* dm = gInPlace.win->AsFixed();
    Annotation* annot = gInPlace.annot;
    if (!dm || !AnnotationIsLive(annot) || !dm->PageVisible(PageNo(annot))) {
        return;
    }
    Rect r = dm->CvtToScreen(PageNo(annot), GetRect(annot));
    Rect cur = ChildPosWithinParent(gInPlace.hwnd);
    if (cur.x == r.x && cur.y == r.y) {
        return;
    }
    SetWindowPos(gInPlace.hwnd, nullptr, r.x, r.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void EndFreeTextInPlaceEdit(bool accept) {
    if (!gInPlace.hwnd || gInPlaceEnding) {
        return;
    }
    gInPlaceEnding = true;
    HWND hwnd = gInPlace.hwnd;
    HFONT font = gInPlace.font;
    MainWindow* win = gInPlace.win;
    WindowTab* tab = gInPlace.tab;
    Annotation* annot = gInPlace.annot;
    Rect editRect = ChildPosWithinParent(hwnd);
    TempStr text{};
    if (accept) {
        text = str::DupTemp(HwndGetTextTemp(hwnd));
        text = str::ReplaceTemp(text, StrL("\r\n"), StrL("\n"));
    }
    // clear the state and unsubclass before destroying, so the destroy-time
    // WM_KILLFOCUS doesn't come back through the commit path
    gInPlace = {};
    SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)gInPlaceDefProc);
    DestroyWindow(hwnd);
    if (font) {
        DeleteObject(font);
    }
    bool winOk = win && IsMainWindowValidAndNotClosing(win);
    if (winOk) {
        HwndSetFocus(win->hwndCanvas);
    }
    if (accept && AnnotationIsLive(annot)) {
        DisplayModel* dm = winOk ? win->AsFixed() : nullptr;
        int pageNo = PageNo(annot);
        if (dm && dm->ValidPageNo(pageNo)) {
            // the box was sized to the text while typing; keep that room so
            // MuPDF doesn't clip what was just written. Compare in screen
            // pixels: converting the box back to page coordinates biases it
            // half a pixel up and to the left, so a page-space union always
            // looks bigger and would grow the annotation on every edit.
            RectF cur = GetRect(annot);
            Rect curScreen = dm->CvtToScreen(pageNo, cur);
            Rect wanted = curScreen.Union(editRect);
            if (wanted != curScreen) {
                SetRect(annot, cur.Union(dm->CvtFromScreen(wanted, pageNo)));
            }
        }
        SetContents(annot, text);
        AnnotChanged(tab);
    } else if (winOk) {
        MainWindowRerender(win);
    }
    gInPlaceEnding = false;
}

static LRESULT CALLBACK WndProcFreeTextInPlaceEdit(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_GETDLGCODE:
            // Enter is a new line and Esc cancels, so we want every key
            return DLGC_WANTALLKEYS;
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) {
                EndFreeTextInPlaceEdit(false);
                return 0;
            }
            if (wp == VK_RETURN && IsCtrlPressed()) {
                EndFreeTextInPlaceEdit(true);
                return 0;
            }
            break;
        case WM_CHAR:
            // Ctrl+Enter reaches an edit control as LF. That, not the key-down
            // above, is what a real keyboard delivers here.
            if (wp == 0x0A) {
                EndFreeTextInPlaceEdit(true);
                return 0;
            }
            // Esc was handled on key down; don't also insert it
            if (wp == VK_ESCAPE) {
                return 0;
            }
            break;
        case WM_KILLFOCUS:
            EndFreeTextInPlaceEdit(true);
            // only a click elsewhere gets here, and if that click was on the
            // Edit text button it means "done", not "start again"
            gInPlaceEndedAt = GetTickCount64();
            return 0;
    }
    LRESULT res = CallWindowProcW(gInPlaceDefProc, hwnd, msg, wp, lp);
    switch (msg) {
        case WM_CHAR:
        case WM_KEYDOWN:
        case WM_PASTE:
        case WM_CUT:
        case WM_CLEAR:
        case WM_UNDO:
        case WM_SETTEXT:
        case EM_REPLACESEL:
            SizeInPlaceEditToText();
            break;
    }
    return res;
}

bool StartFreeTextInPlaceEdit(MainWindow* win, Annotation* annot) {
    if (!win || !win->hwndCanvas || !AnnotationIsLive(annot)) {
        return false;
    }
    if (Type(annot) != AnnotationType::FreeText) {
        return false;
    }
    if (gInPlace.annot == annot) {
        return true;
    }
    EndFreeTextInPlaceEdit(true);
    DisplayModel* dm = win->AsFixed();
    int pageNo = PageNo(annot);
    if (!dm || !dm->ValidPageNo(pageNo) || !dm->PageVisible(pageNo)) {
        return false;
    }
    RectF pageRect = GetRect(annot);
    Rect rc = dm->CvtToScreen(pageNo, pageRect);
    if (rc.IsEmpty()) {
        return false;
    }

    // screen pixels per PDF point, so the box matches the rendered text
    float scale = pageRect.dy > 0 ? ((float)rc.dy / pageRect.dy) : 1.f;
    int textSize = DefaultAppearanceTextSize(annot);
    if (textSize <= 0) {
        textSize = 12;
    }
    int borderWidth = std::max(BorderWidth(annot), 0);
    int fontPx = std::max(6, (int)(((float)textSize * scale) + 0.5f));
    HFONT font = CreateFontW(-fontPx, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH,
                             WinFontForPdfFontName(DefaultAppearanceTextFont(annot)));
    if (!font) {
        return false;
    }

    // lines don't wrap: the box grows to fit them instead
    DWORD style = WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_WANTRETURN | ES_AUTOHSCROLL | ES_AUTOVSCROLL;
    HMODULE hmod = GetModuleHandleW(nullptr);
    HWND hwnd =
        CreateWindowExW(0, WC_EDITW, L"", style, rc.x, rc.y, rc.dx, rc.dy, win->hwndCanvas, nullptr, hmod, nullptr);
    if (!hwnd) {
        DeleteObject(font);
        return false;
    }
    SetWindowFont(hwnd, font, TRUE);
    int pad = std::max(DpiScale(2), 1);
    SendMessageW(hwnd, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(pad, pad));
    TempStr text = str::ReplaceTemp(Contents(annot), StrL("\r\n"), StrL("\n"));
    text = str::ReplaceTemp(text, StrL("\n"), StrL("\r\n"));
    HwndSetText(hwnd, text);

    gInPlaceDefProc = (WNDPROC)GetWindowLongPtrW(hwnd, GWLP_WNDPROC);
    SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)WndProcFreeTextInPlaceEdit);

    gInPlace.hwnd = hwnd;
    gInPlace.font = font;
    gInPlace.win = win;
    gInPlace.tab = win->CurrentTab();
    gInPlace.annot = annot;
    gInPlace.size = rc.Size();
    gInPlace.minSize = rc.Size();
    gInPlace.padding = pad;
    gInPlace.textSize = textSize;
    gInPlace.borderWidth = borderWidth;
    gInPlace.fontPx = fontPx;
    gInPlace.scale = scale;

    HwndSetFocus(hwnd);
    // caret at the end, nothing selected: this is editing what is there, not
    // replacing it
    int end = (int)SendMessageW(hwnd, WM_GETTEXTLENGTH, 0, 0);
    EditSelectText(hwnd, end, end);
    SizeInPlaceEditToText();
    return true;
}

// Edit the free text annotation under `pt`, if there is one and we are in
// Edit PDF mode.
bool StartFreeTextInPlaceEditAt(MainWindow* win, Point pt) {
    if (!win || !win->pdfAnnotationsToolbarEnabled) {
        return false;
    }
    WindowTab* tab = win->CurrentTab();
    DisplayModel* dm = win->AsFixed();
    if (!tab || !dm) {
        return false;
    }
    Annotation* annot = dm->GetAnnotationAtPos(pt, nullptr);
    if (!annot || Type(annot) != AnnotationType::FreeText) {
        return false;
    }
    SetSelectedAnnotation(tab, annot);
    return StartFreeTextInPlaceEdit(win, annot);
}

TempStr FreeTextInPlaceEditStateTemp(MainWindow* win) {
    if (!IsEditingFreeTextInPlace(win)) {
        return StrL("freeTextEdit active=0 rect=0,0,0,0 text=\n");
    }
    Rect r = ChildPosWithinParent(gInPlace.hwnd);
    TempStr text = HwndGetTextTemp(gInPlace.hwnd);
    text = str::ReplaceTemp(text, StrL("\r\n"), StrL("|"));
    return fmt("freeTextEdit active=1 rect=%d,%d,%d,%d text=%s\n", r.x, r.y, r.dx, r.dy, text);
}

void HideAnnotEditToolbar(MainWindow* win) {
    if (IsEditingFreeTextInPlace(win)) {
        EndFreeTextInPlaceEdit(true);
    }
    AnnotEditToolbar* tb = win ? win->annotEditToolbar : nullptr;
    if (!tb || !tb->host) {
        return;
    }
    if (tb->editingContents) {
        RestoreCanvasFocus(tb);
        DestroyContentsEditor(tb);
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
    VecReset(tb->kinds);
    VecReset(tb->chips);
}

void UpdateAnnotEditToolbar(MainWindow* win) {
    if (!win) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    Annotation* annot = tab ? tab->selectedAnnotation : nullptr;
    AnnotEditToolbar* tb = win->annotEditToolbar;
    if (tb && tb->editingContents) {
        if (!win->pdfAnnotationsToolbarEnabled || !AnnotationIsLive(annot) || annot != tb->annot) {
            RestoreCanvasFocus(tb);
            DestroyContentsEditor(tb);
        } else {
            Rect bounds;
            if (!GetAnnotScreenBounds(win, annot, bounds)) {
                HideAnnotEditToolbar(win);
                return;
            }
            tb->lastAnnotBounds = bounds;
            PositionToolbar(tb, bounds);
            return;
        }
    }
    if (!win->pdfAnnotationsToolbarEnabled || !AnnotationIsLive(annot)) {
        HideAnnotEditToolbar(win);
        return;
    }
    Rect bounds;
    if (!GetAnnotScreenBounds(win, annot, bounds)) {
        HideAnnotEditToolbar(win);
        return;
    }
    tb = GetOrCreateToolbar(win);
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
    Annotation* annot = LiveToolbarAnnot(tb);
    Rect bounds;
    if (!annot || !GetAnnotScreenBounds(win, annot, bounds)) {
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
    if (gPopupTb == tb) {
        gPopupTb = nullptr;
        gPopupDismissedAt = 0;
    }
    DestroyContentsEditor(tb);
    win->UnregisterOnWindowMoved(&tb->onWindowMoved);
    delete tb->host;
    delete tb;
    win->annotEditToolbar = nullptr;
}

TempStr AnnotEditToolbarStateTemp(MainWindow* win) {
    AnnotEditToolbar* tb = win ? win->annotEditToolbar : nullptr;
    bool visible = tb && tb->host && tb->host->IsVisible();
    if (!visible) {
        return fmt("annotEditToolbar visible=0 n=0 items= editing=0\n");
    }
    str::Builder items;
    for (int i = 0; i < len(tb->kinds); i++) {
        if (i > 0) {
            items.AppendChar(',');
        }
        items.Append(KindName(tb->kinds[i]));
    }
    Rect r = tb->host->ScreenRect();
    str::Builder chips;
    for (int i = 0; i < len(tb->chips); i++) {
        if (i > 0) {
            chips.AppendChar(';');
        }
        Rect cr = tb->chips[i]->BoundsInWindow();
        Str name = i < len(tb->kinds) ? KindName(tb->kinds[i]) : StrL("?");
        chips.Append(fmt("%s:%d,%d,%d,%d:%s", name, r.x + cr.x, r.y + cr.y, cr.dx, cr.dy, tb->chips[i]->tooltip));
    }
    return fmt("annotEditToolbar visible=1 n=%d items=%s placed=%d,%d,%d,%d editing=%d chips=%s\n", len(tb->kinds),
               ToStrTemp(items), r.x, r.y, r.dx, r.dy, tb->editingContents ? 1 : 0, ToStrTemp(chips));
}

// clang-format off
static SeqStrings gFileAttachmentUcons = "Graph\0Paperclip\0PushPin\0Tag\0";
static SeqStrings gSoundIcons = "Speaker\0Mic\0";
static SeqStrings gStampIcons =
    "Approved\0AsIs\0Confidential\0Departmental\0Draft\0Experimental\0Expired\0Final\0ForComment\0ForPublicRelease\0NotApproved\0NotForPublicRelease\0Sold\0TopSecret\0";
// those are in order of pdf_line_ending enum in annot.h
static SeqStrings gLineEndingStyles =
    "None\0Square\0Circle\0Diamond\0OpenArrow\0ClosedArrow\0Butt\0ROpenArrow\0RClosedArrow\0Slash\0";
static SeqStrings gColors =
    "Transparent\0Aqua\0Black\0Blue\0Fuchsia\0Gray\0Green\0Lime\0Maroon\0Navy\0Olive\0Orange\0Purple\0Red\0Silver\0Teal\0White\0Yellow\0";
static SeqStrings gFontNames = "Cour\0Helv\0TiRo\0";
static SeqStrings gFontReadableNames = "Courier\0Helvetica\0TimesRoman\0";

static PdfColor gColorsValues[] = {
	0x00000000, /* transparent */
	0xff00ffff, /* aqua */
	0xff000000, /* black */
	0xff0000ff, /* blue */
	0xffff00ff, /* fuchsia */
	0xff808080, /* gray */
	0xff008000, /* green */
	0xff00ff00, /* lime */
	0xff800000, /* maroon */
	0xff000080, /* navy */
	0xff808000, /* olive */
	0xffffa500, /* orange */
	0xff800080, /* purple */
	0xffff0000, /* red */
	0xffc0c0c0, /* silver */
	0xff008080, /* teal */
	0xffffffff, /* white */
	0xffffff00, /* yellow */
};

// clang-format on

static bool gShowRect = true;

static TempStr GetKnownColorNameTemp(PdfColor c) {
    int n = dimofi(gColorsValues);
    for (int i = 0; i < n; i++) {
        if (c == gColorsValues[i]) {
            return SeqStrByIndex(gColors, i);
        }
    }
    return {};
}

static void AppendPdfDate(str::Builder& s, time_t secs) {
    struct tm tm;
    gmtime_s(&tm, &secs);
    char buf[100];
    strftime(buf, sizeof buf, "%Y-%m-%d %H:%M UTC", &tm);
    s.Append(Str(buf));
}

// Drop non-owning Annotation* held by UI (selection, drag, hover, form edit).
// Call before DeleteAnnotation frees the wrapper, or when the engine is about
// to die and raw Annotation* must not be used again.
void DetachAnnotationFromUI(Annotation* annot) {
    if (!annot) {
        return;
    }
    if (gInPlace.annot == annot) {
        EndFreeTextInPlaceEdit(false);
    }
    CancelFormFieldEditIfWidget(annot);
    for (MainWindow* win : gWindows) {
        if (win->annotationBeingDragged == annot) {
            EndPdfEditOperation(win);
            win->annotationBeingDragged = nullptr;
            win->annotationBeingResized = false;
        }
        if (win->annotationUnderCursor == annot) {
            win->annotationUnderCursor = nullptr;
            HideAnnotationHoverOverlay(win);
        }
        int nTabs = win->TabCount();
        for (int i = 0; i < nTabs; i++) {
            WindowTab* t = win->GetTab(i);
            if (t && t->selectedAnnotation == annot) {
                t->selectedAnnotation = nullptr;
                HideAnnotEditToolbar(win);
            }
        }
    }
}

void CloseAnnotationUiForTab(WindowTab* tab) {
    if (!tab) {
        return;
    }
    MainWindow* win = tab->win;
    if (win && win->CurrentTab() == tab) {
        HideAnnotEditToolbar(win);
    }
}

// Clear non-owning Annotation* before the old engine is destroyed.
void InvalidateEditAnnotationsOnEngineChange(WindowTab* tab) {
    CloseAnnotationUiForTab(tab);
}

void DeleteAnnotationAndUpdateUI(WindowTab* tab, Annotation* annot) {
    if (!annot) {
        return;
    }
    Annotation* keepSelected = annot == tab->selectedAnnotation ? nullptr : tab->selectedAnnotation;

    DetachAnnotationFromUI(annot);
    DeleteAnnotation(annot);
    RefreshAnnotationLists(tab);
    SetSelectedAnnotation(tab, keepSelected);
    if (IsMainWindowValidAndNotClosing(tab->win)) {
        MainWindowRerender(tab->win);
    }
}

void NotifyAnnotationsChanged(WindowTab* tab) {
    if (tab && tab->win) {
        UpdateAnnotFilterToolbar(tab->win);
    }
}

bool AnnotMatchesFilter(Annotation* annot, const StrVec& words) {
    if (len(words) == 0) {
        return true;
    }
    return FilterMatches(Contents(annot), words);
}

// Type on the left, optional contents in muted color, page number on the right.
// Contents is clipped so it cannot paint over the page column.
void DrawAnnotationListRow(Gfx* gfx, PlatformFont* font, Rect rc, Annotation* annot, const StrVec& filterWords,
                           Vec<u8>& hlScratch, Color colBg, Color colText, bool selected) {
    if (!gfx || !annot) {
        return;
    }
    if (IsSpecialColor(colBg)) {
        colBg = GetSysColor(COLOR_WINDOW);
    }
    if (IsSpecialColor(colText)) {
        colText = GetSysColor(COLOR_WINDOWTEXT);
    }
    if (selected) {
        colBg = AccentColor(colBg, 30);
    }
    gfx->FillRect(rc, colBg);

    int pad = DpiScale(6);
    Rect rcText = rc;
    rcText.x += pad;
    rcText.dx -= 2 * pad;
    if (rcText.dx <= 0) {
        return;
    }

    TempStr pageStr = fmt("%d", annot->pageNo);
    int pageGap = DpiScale(10);
    int pageColDx = gfx->MeasureText(pageStr, font).dx;
    Rect rcPage = rcText;
    rcPage.x = std::max(rcText.x, rcText.x + rcText.dx - pageColDx);
    rcPage.dx = rcText.x + rcText.dx - rcPage.x;

    Str typeName = AnnotationReadableNameTemp(annot->type);
    int typeDx = gfx->MeasureText(typeName, font).dx;
    int typeMaxDx = std::max(0, rcPage.x - pageGap - rcText.x);
    Rect rcType = rcText;
    rcType.dx = std::min(typeDx, typeMaxDx);
    if (rcType.dx > 0) {
        gfx->DrawText(typeName, rcType, gfxTextEllipsis | gfxTextVCenter | gfxTextLeft, font, colText);
    }

    Str contents = Contents(annot);
    if (contents && rcType.dx > 0) {
        TempStr oneLine = str::NormalizeWSTemp(contents);
        if (oneLine) {
            int typeContentsGap = DpiScale(8);
            Rect rcContents = rcText;
            rcContents.x = rcType.x + rcType.dx + typeContentsGap;
            rcContents.dx = rcPage.x - pageGap - rcContents.x;
            if (rcContents.dx > 0) {
                gfx->PushClip(rcContents);
                DrawMaybeHighlightedText(gfx, rcContents, oneLine, filterWords, hlScratch, colBg, false, false,
                                         gfxTextEllipsis | gfxTextVCenter | gfxTextLeft, font,
                                         ThemeWindowTextDisabledColor());
                gfx->PopClip();
            }
        }
    }

    gfx->FillRect(rcPage, colBg);
    gfx->DrawText(pageStr, rcPage, gfxTextEllipsis | gfxTextVCenter | gfxTextRight, font, colText);
}

SeqStrings AnnotEditorColorNames() {
    return gColors;
}

int AnnotEditorColorCount() {
    return dimofi(gColorsValues);
}

PdfColor AnnotEditorColorAt(int i) {
    if (i < 0 || i >= dimofi(gColorsValues)) {
        return 0;
    }
    return gColorsValues[i];
}

Str AnnotEditorColorNameAt(int i) {
    return SeqStrByIndex(gColors, i);
}

SeqStrings AnnotEditorLineEndingStyles() {
    return gLineEndingStyles;
}

SeqStrings AnnotEditorFontNames() {
    return gFontNames;
}

SeqStrings AnnotEditorFontReadableNames() {
    return gFontReadableNames;
}

SeqStrings AnnotationIconNames(Annotation* annot) {
    SeqStrings items = nullptr;
    if (annot) {
        switch (Type(annot)) {
            case AnnotationType::Text:
                items = AnnotationTextIcons();
                break;
            case AnnotationType::FileAttachment:
                items = gFileAttachmentUcons;
                break;
            case AnnotationType::Sound:
                items = gSoundIcons;
                break;
            case AnnotationType::Stamp:
                items = gStampIcons;
                break;
            default:
                break;
        }
    }
    return items;
}

constexpr const WCHAR* kAnnotationHoverOverlayClassName = L"SumatraAnnotationHoverOverlay";

struct AnnotationHoverRows {
    StrVec keys;
    StrVec labels;
    StrVec values;

    void Add(Str key, Str label, Str value) {
        keys.Append(key);
        labels.Append(label);
        values.Append(value);
    }
};

struct AnnotationHoverOverlay {
    MainWindow* win = nullptr;
    WindowTab* tab = nullptr;
    Annotation* annot = nullptr;
    VirtHost* host = nullptr;
    PlatformFont* font = nullptr;
    Size size;
    Rect lastPlaced;
    Rect anchorRect;
    RectF annotBounds;
    Str rowsDump;
    int rowCount = 0;
    bool isAbove = false;
};

static TempStr AnnotationColorNameTemp(PdfColor color) {
    TempStr known = GetKnownColorNameTemp(color);
    if (known) {
        return known;
    }
    str::Builder value;
    SerializePdfColor(color, value);
    return ToStrTemp(value);
}

static TempStr ShortAnnotationHoverValueTemp(Str value, int maxRunes = 72) {
    if (len(value) == 0) {
        return StrL("");
    }
    TempStr oneLine = str::NormalizeWSTemp(value);
    return ShortenStringUtf8Temp(oneLine, maxRunes);
}

static TempStr ShortAnnotationContentsTemp(Str value) {
    constexpr int kMaxRunes = 32;
    TempStr oneLine = str::NormalizeWSTemp(value);
    int nRunes = utf8StrLen((const u8*)CStrTemp(oneLine));
    if (nRunes >= 0 && nRunes <= kMaxRunes) {
        return oneLine;
    }

    int bytesToKeep = std::min(kMaxRunes, len(oneLine));
    if (nRunes >= 0) {
        bytesToKeep = 0;
        for (int i = 0; i < kMaxRunes; i++) {
            int runeBytes = utf8RuneLen((const u8*)oneLine.s + bytesToKeep);
            ReportIf(runeBytes <= 0);
            if (runeBytes <= 0) {
                break;
            }
            bytesToKeep += runeBytes;
        }
    } else if (len(oneLine) <= kMaxRunes) {
        return oneLine;
    }
    return str::JoinTemp(Str(oneLine.s, bytesToKeep), StrL("..."));
}

// Keep the hover card's rows in lockstep with the compact property toolbar.
// Metadata is always present; type-specific properties use the same visibility
// predicates as that toolbar.
static void CollectAnnotationHoverRows(Annotation* annot, AnnotationHoverRows& rows) {
    Str contents = Contents(annot);
    if (len(contents) > 0) {
        rows.Add(StrL("contents"), _TRA("Contents:"), ShortAnnotationContentsTemp(contents));
    }

    AnnotationType type = Type(annot);
    if (type == AnnotationType::FreeText) {
        int quadding = Quadding(annot);
        rows.Add(StrL("textAlignment"), _TRA("Text Alignment:"), SeqStrByIndex(gQuaddingNames, quadding));

        int fontIdx = SeqStrIndex(gFontNames, DefaultAppearanceTextFont(annot));
        if (fontIdx >= 0) {
            rows.Add(StrL("textFont"), _TRA("Text Font:"), SeqStrByIndex(gFontReadableNames, fontIdx));
        }
        rows.Add(StrL("textSize"), _TRA("Text Size:"), fmt("%d", DefaultAppearanceTextSize(annot)));
        rows.Add(StrL("textColor"), _TRA("Text Color:"), AnnotationColorNameTemp(DefaultAppearanceTextColor(annot)));
    }

    if (type == AnnotationType::Line) {
        int start = 0;
        int end = 0;
        GetLineEndingStyles(annot, &start, &end);
        rows.Add(StrL("lineStart"), _TRA("Line Start:"), SeqStrByIndex(gLineEndingStyles, start));
        rows.Add(StrL("lineEnd"), _TRA("Line End:"), SeqStrByIndex(gLineEndingStyles, end));
    }

    Str icon = IconName(annot);
    if (AnnotationIconNames(annot) && icon) {
        rows.Add(StrL("icon"), _TRA("Icon:"), ShortAnnotationHoverValueTemp(icon));
    }
    if (AnnotationSupportsBorder(type)) {
        rows.Add(StrL("border"), _TRA("Border:"), fmt("%d", BorderWidth(annot)));
    }
    if (AnnotationSupportsColor(type)) {
        Str label = AnnotationColorIsBackground(type) ? _TRA("Background Color:") : _TRA("Color:");
        rows.Add(StrL("color"), label, AnnotationColorNameTemp(GetColor(annot)));
    }
    if (AnnotationSupportsInteriorColor(type)) {
        rows.Add(StrL("interiorColor"), _TRA("Interior Color:"), AnnotationColorNameTemp(InteriorColor(annot)));
    }
    if (AnnotationSupportsOpacity(type)) {
        rows.Add(StrL("opacity"), _TRA("Opacity:"), fmt("%d", Opacity(annot)));
    }

    rows.Add(StrL("author"), _TRA("Author:"), ShortAnnotationHoverValueTemp(Author(annot)));
    str::Builder date;
    if (ModificationDate(annot) != 0) {
        AppendPdfDate(date, ModificationDate(annot));
    }
    rows.Add(StrL("date"), _TRA("Date:"), ToStr(date));
    int popupId = PopupId(annot);
    if (popupId >= 0) {
        rows.Add(StrL("popup"), _TRA("Popup:"), fmt("%d 0 R", popupId));
    }
    if (gShowRect) {
        RectF rect = GetBounds(annot);
        rows.Add(StrL("rect"), _TRA("Rect:"), fmt("%d-%d@%d-%d", (int)rect.dx, (int)rect.dy, (int)rect.x, (int)rect.y));
    }
}

static Color AnnotationHoverBg() {
    return ThemeNotificationsBackgroundColor();
}

static Color AnnotationHoverText() {
    return ThemeNotificationsTextColor();
}

static void PaintAnnotationHoverOverlay(AnnotationHoverOverlay*, VirtHostPaintEvent* ev) {
    int radius = DpiScale(6);
    ev->gfx->FillRoundedRect(ev->clientRect, radius, AnnotationHoverBg(), ThemeEdgeColor());
}

static void AnnotationHoverNativeMsg(AnnotationHoverOverlay*, VirtHostNativeMsg* ev) {
    if (ev->msg == WM_NCHITTEST) {
        // This is an informational card, not a new interaction surface. Let
        // mouse input continue to reach the annotation and canvas beneath it.
        ev->didHandle = true;
        ev->res = HTTRANSPARENT;
    }
}

static AnnotationHoverOverlay* GetOrCreateAnnotationHoverOverlay(MainWindow* win) {
    if (win->annotationHoverOverlay) {
        return win->annotationHoverOverlay;
    }
    auto* overlay = new AnnotationHoverOverlay();
    overlay->win = win;
    overlay->font = GetAppFontForDpi(DpiGetForHwnd(win->hwndCanvas));

    VirtHost::CreateArgs args;
    args.parent = win->hwndFrame;
    args.className = WStr(kAnnotationHoverOverlayClassName);
    args.initialSize = {1, 1};
    args.bgColor = AnnotationHoverBg();
    args.isPopup = true;
    args.visible = false;
    args.noActivate = true;
    args.userData = overlay;
    overlay->host = VirtHost::Create(args);
    if (!overlay->host) {
        delete overlay;
        return nullptr;
    }
    overlay->host->onPaintBackground = MkFunc1(PaintAnnotationHoverOverlay, overlay);
    overlay->host->onNativeMsg = MkFunc1(AnnotationHoverNativeMsg, overlay);
    win->annotationHoverOverlay = overlay;
    return overlay;
}

static void BuildAnnotationHoverOverlay(AnnotationHoverOverlay* overlay, Annotation* annot) {
    AnnotationHoverRows rows;
    CollectAnnotationHoverRows(annot, rows);

    Color textColor = AnnotationHoverText();
    Color labelColor = ThemeWindowTextDisabledColor();
    auto* title = NewVirtText({
        .s = AnnotationReadableNameTemp(Type(annot)),
        .font = GetBoldPlatformFont(overlay->font),
        .textColor = textColor,
    });

    auto* table = new Table();
    table->SetSize(len(rows.labels), 2);
    table->colGap = DpiScale(12);
    table->rowGap = DpiScale(3);
    for (int row = 0; row < len(rows.labels); row++) {
        auto* label = NewVirtText({
            .s = rows.labels[row],
            .font = overlay->font,
            .textColor = labelColor,
        });
        auto* value = NewVirtText({
            .s = rows.values[row],
            .font = overlay->font,
            .textColor = textColor,
            .ellipsis = !str::Eq(rows.keys[row], StrL("date")) && !str::Eq(rows.keys[row], StrL("rect")),
        });
        TableCell& labelCell = table->SetCell(row, 0, label);
        labelCell.alignV = CrossAxisAlign::CrossCenter;
        TableCell& valueCell = table->SetCell(row, 1, value);
        valueCell.alignV = CrossAxisAlign::CrossCenter;
    }

    auto* column = new VBox();
    column->alignCross = CrossAxisAlign::Stretch;
    column->AddChild(title);
    column->AddChild(new Spacer(0, DpiScale(5)));
    column->AddChild(table);
    auto* content = new Padding(column, DpiScaledInsets(8, 10));
    overlay->size = overlay->host->SetLayoutSizedToContent(content);
    overlay->host->ClipToRoundedRect(6, overlay->size);

    str::Builder dump;
    for (int i = 0; i < len(rows.keys); i++) {
        dump.Append(fmt("row %s=%s\n", rows.keys[i], rows.values[i]));
    }
    str::ReplaceWithCopy(&overlay->rowsDump, ToStr(dump));
    overlay->rowCount = len(rows.keys);
    overlay->annot = annot;
    overlay->tab = overlay->win->CurrentTab();
    overlay->annotBounds = GetRect(annot);
}

static bool SameRectF(RectF a, RectF b) {
    return a.x == b.x && a.y == b.y && a.dx == b.dx && a.dy == b.dy;
}

static bool PositionAnnotationHoverOverlay(AnnotationHoverOverlay* overlay) {
    MainWindow* win = overlay ? overlay->win : nullptr;
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    Annotation* annot = overlay ? overlay->annot : nullptr;
    if (!dm || !annot || !dm->PageVisible(PageNo(annot))) {
        return false;
    }

    Rect canvas = HwndClientRect(win->hwndCanvas);
    Rect annotRect = dm->CvtToScreen(PageNo(annot), GetRect(annot));
    overlay->anchorRect = annotRect;
    if (canvas.Intersect(annotRect).IsEmpty()) {
        return false;
    }

    int gap = DpiScale(6);
    int width = overlay->size.dx;
    int height = std::min(overlay->size.dy, canvas.dy);
    int x = annotRect.x;
    int y = annotRect.y + annotRect.dy + gap;
    overlay->isAbove = y + height > canvas.y + canvas.dy;
    if (overlay->isAbove) {
        y = annotRect.y - gap - height;
    }

    int maxX = canvas.x + canvas.dx - width;
    x = std::max(canvas.x, std::min(x, maxX));
    int maxY = canvas.y + canvas.dy - height;
    y = std::max(canvas.y, std::min(y, maxY));

    Point screen = HwndClientToScreen(win->hwndCanvas, Point(x, y));
    Rect placed(screen.x, screen.y, width, height);
    if (placed != overlay->lastPlaced) {
        overlay->host->SetBounds(placed);
        overlay->lastPlaced = placed;
    }
    return true;
}

void HideAnnotationHoverOverlay(MainWindow* win) {
    AnnotationHoverOverlay* overlay = win ? win->annotationHoverOverlay : nullptr;
    if (!overlay) {
        return;
    }
    overlay->host->Show(false);
    overlay->annot = nullptr;
    overlay->tab = nullptr;
    overlay->lastPlaced = {};
    overlay->anchorRect = {};
    overlay->annotBounds = {};
    overlay->rowCount = 0;
    str::Free(overlay->rowsDump);
    overlay->rowsDump = {};
}

void UpdateAnnotationHoverOverlay(MainWindow* win) {
    Annotation* annot = win ? win->annotationUnderCursor : nullptr;
    WindowTab* tab = win ? win->CurrentTab() : nullptr;
    if (!win || !win->pdfAnnotationsToolbarEnabled || win->mouseAction != MouseAction::None ||
        !AnnotationIsLive(annot) || (tab && annot == tab->selectedAnnotation)) {
        HideAnnotationHoverOverlay(win);
        return;
    }
    AnnotationHoverOverlay* overlay = GetOrCreateAnnotationHoverOverlay(win);
    if (!overlay) {
        return;
    }
    RectF bounds = GetRect(annot);
    bool rebuild = overlay->annot != annot || overlay->tab != win->CurrentTab() ||
                   !SameRectF(bounds, overlay->annotBounds) || !overlay->host->IsVisible();
    if (rebuild) {
        BuildAnnotationHoverOverlay(overlay, annot);
    }
    if (!PositionAnnotationHoverOverlay(overlay)) {
        HideAnnotationHoverOverlay(win);
        return;
    }
    overlay->host->Show(true);
    if (rebuild) {
        overlay->host->Invalidate(false);
    }
}

void RepositionAnnotationHoverOverlay(MainWindow* win) {
    AnnotationHoverOverlay* overlay = win ? win->annotationHoverOverlay : nullptr;
    if (!overlay || !overlay->host->IsVisible()) {
        return;
    }
    UpdateAnnotationHoverOverlay(win);
}

void RefreshAnnotationHoverOverlay(MainWindow* win) {
    AnnotationHoverOverlay* overlay = win ? win->annotationHoverOverlay : nullptr;
    if (!overlay || !overlay->host->IsVisible()) {
        return;
    }
    overlay->annot = nullptr;
    UpdateAnnotationHoverOverlay(win);
}

void DeleteAnnotationHoverOverlay(MainWindow* win) {
    AnnotationHoverOverlay* overlay = win ? win->annotationHoverOverlay : nullptr;
    if (!overlay) {
        return;
    }
    win->annotationHoverOverlay = nullptr;
    str::Free(overlay->rowsDump);
    delete overlay->host;
    delete overlay;
}

TempStr AnnotationHoverOverlayStateTemp(MainWindow* win) {
    AnnotationHoverOverlay* overlay = win ? win->annotationHoverOverlay : nullptr;
    bool visible = overlay && overlay->host && overlay->host->IsVisible();
    if (!visible) {
        return StrL("overlay visible=0\n");
    }
    Rect r = overlay->host->ScreenRect();
    Rect a = overlay->anchorRect;
    str::Builder out;
    out.Append(fmt("overlay visible=1 rows=%d above=%d rect=%d,%d,%d,%d anchor=%d,%d,%d,%d\n", overlay->rowCount,
                   overlay->isAbove ? 1 : 0, r.x, r.y, r.dx, r.dy, a.x, a.y, a.dx, a.dy));
    out.Append(overlay->rowsDump);
    return ToStrTemp(out);
}

// GoToPage / canvas scroll for the current selection. Posted so holding
// arrows in the annot list can keep moving the caret (issue #6009). Find
// uses ScheduleRepaint, not MainWindowRerender, for the same reason.
static void ShowSelectedAnnotationView(WindowTab* tab) {
    if (!tab) {
        return;
    }
    tab->pendingShowSelectedAnnotation = false;
    if (!IsMainWindowValidAndNotClosing(tab->win)) {
        return;
    }
    MainWindow* win = tab->win;
    bool tabOpen = false;
    for (WindowTab* t : win->Tabs()) {
        if (t == tab) {
            tabOpen = true;
            break;
        }
    }
    if (!tabOpen) {
        return;
    }
    Annotation* annot = tab->selectedAnnotation;
    DisplayModel* dm = tab->AsFixed();
    if (AnnotationIsLive(annot) && dm) {
        int pageNo = annot->pageNo;
        int nPages = dm->PageCount();
        if (pageNo < 1 || pageNo > nPages) {
            logf("ShowSelectedAnnotationView: invalid pageNo=%d nPages=%d\n", pageNo, nPages);
        } else if (!dm->PageVisible(pageNo)) {
            dm->GoToPage(pageNo, true);
        }
    }
    ScheduleRepaint(win, 0);
    ToolbarUpdateStateForWindow(win, false);
}

static void ScheduleShowSelectedAnnotationView(WindowTab* tab) {
    if (!tab || tab->pendingShowSelectedAnnotation) {
        return;
    }
    tab->pendingShowSelectedAnnotation = true;
    uitask::Post(MkFunc0(ShowSelectedAnnotationView, tab), "ShowSelectedAnnot");
}

void SetSelectedAnnotation(WindowTab* tab, Annotation* annot) {
    MainWindow* win = tab->win;
    if (annot == tab->selectedAnnotation) {
        MainWindowRerender(win);
        ToolbarUpdateStateForWindow(win, false);
        UpdateAnnotEditToolbar(win);
        UpdateAnnotFilterToolbar(win);
        return;
    }
    tab->selectedAnnotation = annot;
    tab->didScrollToSelectedAnnotation = false;
    ScheduleShowSelectedAnnotationView(tab);
    UpdateAnnotEditToolbar(win);
    UpdateAnnotFilterToolbar(win);
}

static void AddAnnotPage(Vec<int>& pages, int pageNo, int pageCount) {
    if (pageNo < 1 || pageNo > pageCount) {
        return;
    }
    if (VecContains(pages, pageNo)) {
        return;
    }
    VecAppend(pages, pageNo);
}

// Load annot wrappers for one page (page + annot dicts, not stext).
static void LoadAnnotsForPage(EngineMupdf* e, int pageNo) {
    if (!e || pageNo < 1 || pageNo > e->pageCount) {
        return;
    }
    FzPageInfo* pi = e->GetFzPageInfo(pageNo, true);
    if (!pi) {
        logf("LoadAnnotsForPage: page %d GetFzPageInfo failed\n", pageNo);
        return;
    }
    logf("LoadAnnotsForPage: page %d n=%d loaded=%d\n", pageNo, len(pi->annotations), (int)pi->annotsLoaded);
}

// Pages the background loader should finish first: current page (toolbar page
// even when visibleRatio is still 0), every page overlapping the viewport, and
// a context-menu annotation's page.
static void CollectPriorityAnnotPages(WindowTab* tab, Annotation* extra, Vec<int>& pages) {
    VecReset(pages);
    if (!tab) {
        return;
    }
    DisplayModel* dm = tab->AsFixed();
    if (!dm) {
        return;
    }
    int n = dm->PageCount();
    int curr = dm->CurrentPageNo();
    if (curr < 1 || curr > n) {
        curr = 1;
    }
    AddAnnotPage(pages, curr, n);
    AddAnnotPage(pages, dm->FirstVisiblePageNo(), n);
    for (int i = 1; i <= n; i++) {
        if (dm->PageVisible(i)) {
            AddAnnotPage(pages, i, n);
        }
    }
    if (extra) {
        AddAnnotPage(pages, extra->pageNo, n);
    }
}

static void OnAnnotsProgress(WindowTab* tab) {
    if (!tab || !IsMainWindowValidAndNotClosing(tab->win)) {
        return;
    }
    RefreshAnnotFilterAnnotations(tab->win);
}

void StartLoadingAnnotationsForUi(WindowTab* tab) {
    if (!tab) {
        return;
    }
    DisplayModel* dm = tab->AsFixed();
    if (!dm) {
        return;
    }
    EngineMupdf* engine = AsEngineMupdf(dm->GetEngine());
    if (!engine) {
        return;
    }
    Vec<int> firstPages;
    CollectPriorityAnnotPages(tab, tab->selectedAnnotation, firstPages);
    for (int pageNo : firstPages) {
        LoadAnnotsForPage(engine, pageNo);
    }
    EngineMupdfStartLoadAllAnnotations(engine, firstPages, MkFunc0(OnAnnotsProgress, tab));
}

void RefreshAnnotationLists(WindowTab* tab) {
    if (!tab) {
        return;
    }
    if (tab->win) {
        StartLoadingAnnotationsForUi(tab);
        RefreshAnnotFilterAnnotations(tab->win);
    }
}

void RefreshEditAnnotationsAfterEngineChange(WindowTab* tab) {
    if (!tab) {
        return;
    }
    if (tab->win) {
        StartLoadingAnnotationsForUi(tab);
        RefreshAnnotFilterAnnotations(tab->win);
    }
}

// Dump selected-annotation and loaded-annot count for tests (issue-5933, issue-6023).
TempStr AnnotEditorLayoutResultTemp(int, int, int* exitCodeOut, int) {
    str::Builder out;
    auto finish = [&](Str msg, int code) -> TempStr {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };

    if (len(gWindows) == 0) {
        return finish(StrL("NOTREADY no-window"), 2);
    }
    MainWindow* win = gWindows[0];
    if (!win || !win->IsDocLoaded()) {
        return finish(StrL("NOTREADY no-doc"), 2);
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || !EngineSupportsAnnotations(tab->GetEngine())) {
        return finish(StrL("ERROR no-annot-engine"), 1);
    }

    StartLoadingAnnotationsForUi(tab);
    Vec<Annotation*> annots;
    EngineMupdfGetLoadedAnnotations(tab->GetEngine(), annots);
    int n = len(annots);
    out.Append(fmt("OK n=%d ignoreReload=%d reloadOnFocus=%d resizeRerenderPending=%d", n,
                   (int)tab->ignoreNextAutoReload, (int)tab->reloadOnFocus,
                   (int)(win->annotationResizeRerenderTimer != 0)));
    Annotation* annot = tab->selectedAnnotation;
    DisplayModel* dm = tab->AsFixed();
    if (annot && dm) {
        Rect annotRect = dm->CvtToScreen(annot->pageNo, GetRect(annot));
        out.Append(fmt(" annotType=%d annotRect=%d,%d,%d,%d canResize=%d", (int)annot->type, annotRect.x, annotRect.y,
                       annotRect.dx, annotRect.dy, (int)AnnotationCanBeResized(annot->type)));
        // the outline the pointer is dragging while the annotation itself is
        // left alone; empty unless an outline-only resize is in progress
        Rect outline;
        if (win->annotationBeingResized && win->annotationResizeOutlineOnly) {
            outline = dm->CvtToScreen(annot->pageNo, win->annotationResizePreviewRect);
        }
        out.Append(fmt(" resizeOutline=%d,%d,%d,%d", outline.x, outline.y, outline.dx, outline.dy));
        // last on the line: the contents can hold anything, including spaces
        out.Append(fmt(" contents=%s", Contents(annot)));
    }
    return finish({}, 0);
}
