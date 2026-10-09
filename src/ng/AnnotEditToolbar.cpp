/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's AnnotEditToolbar.cpp - the compact property row that appears under
// the selected annotation in Edit PDF mode, plus the annotation-selection
// plumbing every other file calls (SetSelectedAnnotation, RefreshAnnotationLists,
// DetachAnnotationFromUI...). orig builds the row out of virtual controls in a
// layered VirtHost window and opens win32 popup menus from its chips; here the
// row is an absolutely positioned gpui card inside the canvas and a chip's
// drop-down is another card drawn beside it. The chip set, their order, what
// each one changes and the colors are orig's.

#include "gui/GpuiBridge.h"
#include "VirtKeys.h"
#include "base/File.h"
#include "base/UITask.h"

#include "gui/Dpi.h"
#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DisplayMode.h"
#include "Annotation.h"
#include "PdfDate.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "Theme.h"
#include "Translations.h"
#include "Commands.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Toolbar.h"
#include "SvgIcons.h"
#include "CommandPalette.h"
#include "SumatraDialogs.h"
#include "gui/AppShell.h"
#include "gui/DocCanvas.h"
#include "gui/WasmBridge.h"
#include "AnnotFilterToolbar.h"
#include "AnnotTextPopup.h"
#include "AnnotPlacement.h"
#include "FormFields.h"
#include "SystemFonts.h"
#include "gui/DialogWidgets.h"
#include "AnnotEditToolbar.h"

// mupdf's glyphs for the Text / FileAttachment / Sound icons, as PDF path
// operators (m l c re h cm f) in an 8x8 box
#include "../../ext/mupdf/source/pdf/annotation-icons.h"

#include "SumatraLog.h"

constexpr int kBtnPadX = 8;
constexpr int kBtnPadY = 4;
constexpr int kMargin = 5;
constexpr int kBtnGap = 2;
constexpr int kCornerRadius = 10;
constexpr int kButtonRadius = 6;
// ranges of the number sliders; widths and sizes are in PDF points
constexpr int kBorderWidthMax = 12;
constexpr int kFreeTextSizeMin = 6;
constexpr int kFreeTextSizeMax = 72;
constexpr int kOpacityPercentMin = 10;
// at most this many chips; the paint state of each one has to keep its address
constexpr int kMaxChips = 24;

enum class AnnotEditKind {
    Color,
    InteriorColor,
    Opacity,
    Border,
    FontName,
    Bold,
    Italic,
    Underline,
    TextColor,
    TextSize,
    Alignment,
    Icon,
    Contents,
    LineStart,
    LineEnd,
    AttachFile,
    SaveAttachment,
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
    // the icon is one of mupdf's glyphs; a stamp has only its name
    bool mupdfIcon = false;
};

// which kind of drop-down a chip opened
enum class AnnotPopupKind {
    None = 0,
    Colors,
    Slider,
    List,
    // every installed font: what orig's "Other Font" hands to the Windows
    // font dialog
    FontList,
};

// what a custom-painted chip draws; its address is handed to gpui, so these
// live in a fixed array in the toolbar
struct AnnotChipPaint {
    AnnotEditKind kind = AnnotEditKind::Color;
    PdfColor color = 0;
    int number = 0;
    bool lineIsStart = false;
    Color fg = 0;
};

struct AnnotEditToolbar {
    MainWindow* win = nullptr;
    WindowTab* tab = nullptr;
    Annotation* annot = nullptr;
    Vec<AnnotEditItem> items;
    // where gpui put the card and each chip last frame (one frame late)
    gp::Bounds measured;
    gp::Bounds chipBounds[kMaxChips];
    AnnotChipPaint chipPaint[kMaxChips];
    int nChips = 0;

    // the open drop-down
    AnnotPopupKind popupKind = AnnotPopupKind::None;
    AnnotEditKind popupChip = AnnotEditKind::Color;
    int popupChipIdx = -1;
    Str popupLabel; // owned
    StrVec popupNames;
    int popupCurrent = -1;
    // the list's rows start with the icon's glyph
    bool popupGlyphs = false;
    // the font list: the row of a font that is not one of the base 14 and the
    // "Other Font" row under a separator, -1 without
    int popupCurrentFontIdx = -1;
    int popupOtherFontIdx = -1;
    float fontScrollY = 0;
    Vec<Color> popupColors;
    Color popupColorNow = kColorUnset;
    // swatch rects from the last frame, in window dips. [0] is "none" when shown
    gp::Bounds swatchBounds[32];
    Color swatchColor[32];
    bool swatchNone[32];
    int nSwatches = 0;
    // the pencil at the end of the swatch row, in window dips
    gp::Bounds editBounds{};
    bool popupWithNone = false;
    // >= 0: the drop-down also carries a thickness slider
    int popupThickness = -1;
    int popupThicknessMin = 1;
    Str popupThicknessLabel; // owned
    int sliderMin = 0;
    int sliderMax = 100;
    int sliderValue = 0;

    // the contents editor: a multi-line text box inside the card
    bool editingContents = false;
    gp::InputState* contentsEdit = nullptr;

    ~AnnotEditToolbar();
};

AnnotEditToolbar::~AnnotEditToolbar() {
    str::Free(popupLabel);
    str::Free(popupThicknessLabel);
    delete contentsEdit;
}

// a click that ends a contents edit must not also deselect the annotation
static bool gContentsEditJustEnded = false;
// where the click that selected a text markup annotation landed (page units),
// so the row starts at the click and not at the multi-line bounds
static Annotation* gClickAnchorAnnot = nullptr;
static PointF gClickAnchor;

// --- colors (orig's) --------------------------------------------------------

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

static Color BarHoverBg() {
    if (BarIsDark()) {
        return AccentColor(ThemeWindowControlBackgroundColor(), 15);
    }
    return AccentColor(BarBg(), 10);
}

static Color BarActiveBg() {
    if (BarIsDark()) {
        return AccentColor(ThemeWindowControlBackgroundColor(), 35);
    }
    return AccentColor(BarBg(), 22);
}

// --- PdfColor helpers (orig's) ----------------------------------------------

static Color PdfToWinColor(PdfColor c) {
    u8 r, g, b, a;
    UnpackPdfColor(c, r, g, b, a);
    return MkRgb(r, g, b);
}

static u8 PdfColorAlpha(PdfColor c) {
    u8 r, g, b, a;
    UnpackPdfColor(c, r, g, b, a);
    return a;
}

static Color PdfToWinColorWithAlpha(PdfColor c) {
    u8 r, g, b, a;
    UnpackPdfColor(c, r, g, b, a);
    return MkRgba(r, g, b, a);
}

static PdfColor WinToPdfColor(Color c) {
    u8 r, g, b, a;
    UnpackColor(c, r, g, b, a);
    // in a Color alpha 0 means opaque, in a PdfColor it means transparent
    return MkPdfColor(r, g, b, a == 0 ? 0xff : a);
}

// the same color, made fully opaque
static PdfColor OpaquePdfColor(PdfColor c) {
    if (c == 0) {
        return 0;
    }
    u8 r, g, b, a;
    UnpackPdfColor(c, r, g, b, a);
    return MkPdfColor(r, g, b, 0xff);
}

// a chip shows a color the way the page does: with the annotation's opacity
static PdfColor ColorWithOpacity(PdfColor c, Annotation* annot, bool withOpacity) {
    if (c == 0 || !withOpacity) {
        return c;
    }
    u8 r, g, b, a;
    UnpackPdfColor(c, r, g, b, a);
    return MkPdfColor(r, g, b, (u8)Opacity(annot));
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
        case AnnotEditKind::Bold:
            return StrL("bold");
        case AnnotEditKind::Italic:
            return StrL("italic");
        case AnnotEditKind::Underline:
            return StrL("underline");
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
        case AnnotEditKind::AttachFile:
            return StrL("attachFile");
        case AnnotEditKind::SaveAttachment:
            return StrL("saveAttachment");
        case AnnotEditKind::Delete:
            return StrL("delete");
    }
    return StrL("?");
}

// --- which chips an annotation gets (orig's CollectItems) --------------------

// annotation types whose GetColor() is a background, not the ink color
static bool AnnotationColorIsBackground(AnnotationType tp) {
    return tp == AnnotationType::FreeText || tp == AnnotationType::Text;
}

// shapes whose color is their border's: the color chip's drop-down also sets
// the border's width, so they have no Border Width chip
static bool AnnotationBorderInColorChip(AnnotationType tp) {
    return tp == AnnotationType::Square || tp == AnnotationType::Circle || tp == AnnotationType::Polygon;
}

static Str DefaultAnnotIconName(AnnotationType type) {
    switch (type) {
        case AnnotationType::Text:
            return StrL("Note");
        case AnnotationType::FileAttachment:
            return StrL("PushPin");
        case AnnotationType::Sound:
            return StrL("Speaker");
        case AnnotationType::Stamp:
            return StrL("Draft");
        default:
            return {};
    }
}

// a name from the type's list, so the chip can paint it after the temp arena resets
static Str ResolvedAnnotIconName(Annotation* annot) {
    SeqStrings icons = AnnotationIconNames(annot);
    if (!icons) {
        return {};
    }
    int idx = SeqStrIndexIS(icons, IconName(annot));
    if (idx < 0) {
        idx = SeqStrIndexIS(icons, DefaultAnnotIconName(Type(annot)));
    }
    if (idx < 0) {
        idx = 0;
    }
    return SeqStrByIndex(icons, idx);
}

// clang-format off
// in gBase14FontFamilies order
static SeqStrings gBase14ReadableNames = "Courier\0Helvetica\0TimesRoman\0";
// clang-format on

static int StyleBitForKind(AnnotEditKind kind) {
    switch (kind) {
        case AnnotEditKind::Bold:
            return kFreeTextBold;
        case AnnotEditKind::Italic:
            return kFreeTextItalic;
        case AnnotEditKind::Underline:
            return kFreeTextUnderline;
        default:
            return 0;
    }
}

// chips paint after the temp arena resets, so a family they show must outlive it
static Str InternFontFamily(Str family) {
    static Vec<Str> families;
    for (Str f : families) {
        if (str::Eq(f, family)) {
            return f;
        }
    }
    Str dup = str::Dup(family);
    VecAppend(families, dup);
    return dup;
}

static Str FontFamilyLabel(Str family) {
    int idx = SeqStrIndexIS(gBase14FontFamilies, family);
    if (idx >= 0) {
        return SeqStrByIndex(gBase14ReadableNames, idx);
    }
    return InternFontFamily(family);
}

static void AppendStyleToggle(Vec<AnnotEditItem>& out, AnnotEditKind kind, int style, Str tooltip) {
    AnnotEditItem it;
    it.kind = kind;
    it.number = (style & StyleBitForKind(kind)) ? 1 : 0;
    it.tooltip = tooltip;
    VecAppend(out, it);
}

static void CollectItems(Annotation* annot, Vec<AnnotEditItem>& out) {
    VecReset(out);
    if (!AnnotationIsLive(annot)) {
        return;
    }
    AnnotationType type = Type(annot);
    bool isFreeText = type == AnnotationType::FreeText;

    if (type == AnnotationType::FileAttachment) {
        Str fileName;
        bool hasFile = HasEmbeddedFile(annot);
        if (hasFile) {
            fileName = EmbeddedFileNameTemp(annot);
            if (fileName) {
                fileName = path::GetBaseNameTemp(fileName);
            }
            if (len(fileName) == 0) {
                fileName = StrL("file");
            }
        }
        {
            AnnotEditItem it;
            it.kind = AnnotEditKind::AttachFile;
            it.tooltip = hasFile ? fmt(Tr("Replace %s with...").s, fileName) : Tr("Attach File");
            VecAppend(out, it);
        }
        if (hasFile) {
            AnnotEditItem it;
            it.kind = AnnotEditKind::SaveAttachment;
            it.tooltip = fmt(Tr("Save %s to disk").s, fileName);
            VecAppend(out, it);
        }
    }

    // free text is about its text, so the text's color leads
    if (isFreeText) {
        AnnotEditItem it;
        it.kind = AnnotEditKind::TextColor;
        it.color = ColorWithOpacity(DefaultAppearanceTextColor(annot), annot, AnnotationSupportsOpacity(type));
        it.tooltip = Tr("Text Color");
        VecAppend(out, it);
    }
    // a color chip is also the opacity chip: it shows the color as it looks on
    // the page and picking one sets the annotation's opacity too, so there is
    // no separate Opacity chip when there is a color to carry it
    bool colorCarriesOpacity = AnnotationSupportsOpacity(type) && AnnotationSupportsColor(type);
    if (AnnotationSupportsColor(type)) {
        AnnotEditItem it;
        it.kind = AnnotEditKind::Color;
        it.color = ColorWithOpacity(GetColor(annot), annot, colorCarriesOpacity);
        it.tooltip = AnnotationColorIsBackground(type) ? Tr("Background Color") : Tr("Color");
        if (AnnotationBorderInColorChip(type)) {
            it.tooltip = Tr("Border Color and Width");
        }
        VecAppend(out, it);
    }
    if (AnnotationSupportsInteriorColor(type)) {
        AnnotEditItem it;
        it.kind = AnnotEditKind::InteriorColor;
        it.color = ColorWithOpacity(InteriorColor(annot), annot, colorCarriesOpacity);
        it.tooltip = Tr("Interior Color");
        VecAppend(out, it);
    }
    if (AnnotationSupportsOpacity(type) && !colorCarriesOpacity) {
        AnnotEditItem it;
        it.kind = AnnotEditKind::Opacity;
        it.number = Opacity(annot);
        it.tooltip = Tr("Opacity");
        VecAppend(out, it);
    }
    // ink has no Border Width chip of its own: the stroke's width is the
    // Thickness slider of its color chip's drop-down. Same for the shapes.
    if (AnnotationSupportsBorder(type) && type != AnnotationType::Ink && !AnnotationBorderInColorChip(type)) {
        AnnotEditItem it;
        it.kind = AnnotEditKind::Border;
        it.number = BorderWidth(annot);
        it.tooltip = Tr("Border Width");
        VecAppend(out, it);
    }
    if (isFreeText) {
        {
            AnnotEditItem it;
            it.kind = AnnotEditKind::FontName;
            it.text = FontFamilyLabel(FreeTextFontFamily(annot));
            it.tooltip = Tr("Font");
            VecAppend(out, it);
        }
        int style = FreeTextFontStyle(annot);
        AppendStyleToggle(out, AnnotEditKind::Bold, style, Tr("Bold"));
        AppendStyleToggle(out, AnnotEditKind::Italic, style, Tr("Italic"));
        AppendStyleToggle(out, AnnotEditKind::Underline, style, Tr("Underline"));
        {
            AnnotEditItem it;
            it.kind = AnnotEditKind::TextSize;
            it.number = DefaultAppearanceTextSize(annot);
            it.tooltip = Tr("Text Size");
            VecAppend(out, it);
        }
        {
            AnnotEditItem it;
            it.kind = AnnotEditKind::Alignment;
            it.number = Quadding(annot);
            it.tooltip = Tr("Text Alignment");
            VecAppend(out, it);
        }
    }
    SeqStrings icons = AnnotationIconNames(annot);
    if (icons) {
        AnnotEditItem it;
        it.kind = AnnotEditKind::Icon;
        it.iconName = ResolvedAnnotIconName(annot);
        it.mupdfIcon = type != AnnotationType::Stamp;
        it.tooltip = Tr("Icon");
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
            it.tooltip = Tr("Line Start");
            VecAppend(out, it);
        }
        {
            AnnotEditItem it;
            it.kind = AnnotEditKind::LineEnd;
            it.lineEnding = end;
            it.lineIsStart = false;
            it.tooltip = Tr("Line End");
            VecAppend(out, it);
        }
    }
    if (type != AnnotationType::Widget) {
        AnnotEditItem it;
        it.kind = AnnotEditKind::Contents;
        it.tooltip = isFreeText ? Tr("Edit text") : Tr("Edit Note");
        VecAppend(out, it);
    }
    if (type != AnnotationType::Widget) {
        // last, so a mis-aimed click lands on a harmless chip, not on delete
        AnnotEditItem it;
        it.kind = AnnotEditKind::Delete;
        it.tooltip = Tr("Delete");
        VecAppend(out, it);
    }
}

// --- the names each list drop-down offers -----------------------------------

// clang-format off
static SeqStrings gFileAttachmentUcons = "Graph\0Paperclip\0PushPin\0Tag\0";
static SeqStrings gSoundIcons = "Speaker\0Mic\0";
static SeqStrings gStampIcons =
    "Approved\0AsIs\0Confidential\0Departmental\0Draft\0Experimental\0Expired\0Final\0ForComment\0ForPublicRelease\0NotApproved\0NotForPublicRelease\0Sold\0TopSecret\0";
// those are in order of pdf_line_ending enum in annot.h
static SeqStrings gLineEndingStyles =
    "None\0Square\0Circle\0Diamond\0OpenArrow\0ClosedArrow\0Butt\0ROpenArrow\0RClosedArrow\0Slash\0";
// clang-format on

SeqStrings AnnotEditorLineEndingStyles() {
    return gLineEndingStyles;
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

// orig paints the annotation list's rows itself (type on the left, contents in
// a muted color, page on the right). gpui's list rows are text, so the three
// columns become one string.
TempStr AnnotationListRowTextTemp(Annotation* annot) {
    if (!annot) {
        return {};
    }
    Str typeName = AnnotationReadableNameTemp(annot->type);
    Str contents = Contents(annot);
    if (len(contents) == 0) {
        return fmt("%s", typeName);
    }
    TempStr oneLine = str::NormalizeWSTemp(contents);
    return fmt("%s  %s", typeName, oneLine);
}

// --- the toolbar ------------------------------------------------------------

static AnnotEditToolbar* GetOrCreateToolbar(MainWindow* win) {
    if (!win->annotEditToolbar) {
        auto* tb = new AnnotEditToolbar();
        tb->win = win;
        win->annotEditToolbar = tb;
    }
    return win->annotEditToolbar;
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

void SetAnnotEditToolbarClickPos(Annotation* annot, PointF pagePt) {
    gClickAnchorAnnot = annot;
    gClickAnchor = pagePt;
}

// the annotation's bounds in canvas coordinates; false when it is off-screen
static bool GetAnnotScreenBounds(MainWindow* win, Annotation* annot, Rect& out) {
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    if (!dm || !AnnotationIsLive(annot) || !dm->PageVisible(PageNo(annot))) {
        return false;
    }
    Rect r = dm->CvtToScreen(PageNo(annot), GetRect(annot));
    if (r.IsEmpty()) {
        return false;
    }
    // orig anchors the row at the click for a multi-line text markup annotation
    if (annot == gClickAnchorAnnot && AnnotationIsTextMarkup(Type(annot))) {
        Point p = dm->CvtToScreen(PageNo(annot), gClickAnchor);
        if (r.Contains(p)) {
            r.x = p.x;
            r.dx = 1;
        }
    }
    out = r;
    return true;
}

static void ClosePopup(AnnotEditToolbar* tb) {
    tb->popupKind = AnnotPopupKind::None;
    tb->popupChipIdx = -1;
    tb->popupNames.Reset();
    tb->popupGlyphs = false;
    tb->popupCurrentFontIdx = -1;
    tb->popupOtherFontIdx = -1;
    VecReset(tb->popupColors);
    tb->editBounds = {};
}

// defined with the color chips: the pencil's dialog
static void OpenAnnotColorsDialog(AnnotEditToolbar* tb);

void HideAnnotEditToolbar(MainWindow* win) {
    AnnotEditToolbar* tb = win ? win->annotEditToolbar : nullptr;
    if (!tb) {
        return;
    }
    if (tb->editingContents) {
        EndAnnotContentsEdit(true);
    }
    ClosePopup(tb);
    tb->tab = nullptr;
    tb->annot = nullptr;
    VecReset(tb->items);
    tb->nChips = 0;
    gClickAnchorAnnot = nullptr;
    AppShellInvalidate(win);
}

void UpdateAnnotEditToolbar(MainWindow* win) {
    if (!win) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    Annotation* annot = tab ? tab->selectedAnnotation : nullptr;
    AnnotEditToolbar* tb = win->annotEditToolbar;
    if (tb && tb->editingContents &&
        (!win->pdfAnnotationsToolbarEnabled || !AnnotationIsLive(annot) || annot != tb->annot)) {
        EndAnnotContentsEdit(true);
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
    Vec<AnnotEditItem> items;
    CollectItems(annot, items);
    if (len(items) == 0) {
        HideAnnotEditToolbar(win);
        return;
    }
    if (tb->annot != annot) {
        ClosePopup(tb);
    }
    tb->tab = tab;
    tb->annot = annot;
    tb->items = items;
    AppShellInvalidate(win);
}

void RefreshAnnotEditToolbar(MainWindow* win) {
    AnnotEditToolbar* tb = win ? win->annotEditToolbar : nullptr;
    if (!tb || !tb->annot) {
        return;
    }
    UpdateAnnotEditToolbar(win);
}

void DeleteAnnotEditToolbar(MainWindow* win) {
    AnnotEditToolbar* tb = win ? win->annotEditToolbar : nullptr;
    if (!tb) {
        return;
    }
    win->annotEditToolbar = nullptr;
    delete tb;
}

// the annotation changed: repaint the page, the row, the list and the toolbar
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

// ng: for the platforms whose picker answers later (AppShellPickFileAsync)
static void OnAttachPicked(MainWindow* win, Str path) {
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    AnnotEditToolbar* tb = win->annotEditToolbar;
    Annotation* annot = LiveToolbarAnnot(tb);
    if (annot && SetEmbeddedFileFromPath(annot, path)) {
        AnnotChanged(win->CurrentTab());
    }
}

// --- the contents editor ----------------------------------------------------

bool IsEditingAnnotContents(MainWindow* win) {
    AnnotEditToolbar* tb = win ? win->annotEditToolbar : nullptr;
    return tb && tb->editingContents;
}

bool AnnotContentsEditJustEnded() {
    bool res = gContentsEditJustEnded;
    gContentsEditJustEnded = false;
    return res;
}

static AnnotEditToolbar* gEditingTb = nullptr;

void EndAnnotContentsEdit(bool accept) {
    AnnotEditToolbar* tb = gEditingTb;
    if (!tb || !tb->editingContents) {
        return;
    }
    tb->editingContents = false;
    gEditingTb = nullptr;
    gContentsEditJustEnded = true;
    Annotation* annot = LiveToolbarAnnot(tb);
    if (accept && annot && tb->contentsEdit) {
        Str now = FromGpui(gp::InputValue(tb->contentsEdit));
        if (!str::Eq(now, Contents(annot))) {
            SetContents(annot, now);
            AnnotChanged(tb->tab);
            return;
        }
    }
    AppShellInvalidate(tb->win);
}

static bool FreeTextInPlaceEditJustEnded();

// Contents editor on the property row (openedit after create, Contents chip).
void StartSelectedAnnotContentsEdit(MainWindow* win) {
    if (!win || !win->gpuiWin) {
        return;
    }
    // free text is edited on the page, in the annotation's own box, and the
    // same button ends it
    if (IsEditingFreeTextInPlace(win)) {
        EndFreeTextInPlaceEdit(true);
        return;
    }
    // clicking the button took the box away, which already ended the edit;
    // this click means "done", not "start again"
    if (FreeTextInPlaceEditJustEnded()) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    Annotation* annot = tab ? tab->selectedAnnotation : nullptr;
    if (!AnnotationIsLive(annot) || Type(annot) == AnnotationType::Widget) {
        return;
    }
    if (Type(annot) == AnnotationType::FreeText && StartFreeTextInPlaceEdit(win, annot)) {
        return;
    }
    EnablePdfAnnotationsToolbar(win);
    UpdateAnnotEditToolbar(win);
    AnnotEditToolbar* tb = win->annotEditToolbar;
    if (!tb || tb->annot != annot) {
        return;
    }
    if (!tb->contentsEdit) {
        tb->contentsEdit = new gp::InputState();
        tb->contentsEdit->kind = gp::InputKind::Textarea;
        tb->contentsEdit->focus = gp::FocusHandleNew(win->gpuiWin->app);
    }
    Str contents = Contents(annot);
    gp::InputSetValue(tb->contentsEdit, ToGpui(contents));
    // caret at the end, nothing selected: this is editing what is there
    gp::InputMoveTo(tb->contentsEdit, win->gpuiWin->app, win->gpuiWin, len(contents));
    tb->editingContents = true;
    gEditingTb = tb;
    ClosePopup(tb);
    AppShellInvalidate(win);
}

// --- editing free text on the page ------------------------------------------

// Double-clicking a free text annotation (or creating one, or its Edit text
// button) puts a text box exactly over it, in the annotation's text size and
// color, so the text is edited where it is shown. The box is opaque: it hides
// the rendered annotation and whatever is underneath. Enter makes a new line;
// Ctrl+Enter, Esc or clicking away ends it and the rendered annotation comes
// back.
// ng: orig's box is a win32 edit that does not wrap and grows with its text;
// this one is a gpui text area as wide as the annotation that wraps where
// mupdf will and grows downwards
struct FreeTextInPlaceEdit {
    MainWindow* win = nullptr;
    WindowTab* tab = nullptr;
    Annotation* annot = nullptr;
    // kept for the life of the process: gpui still holds it for the rest of
    // the event that ends the edit
    gp::InputState* edit = nullptr;
    // where gpui put the box last frame, in dips
    gp::Bounds box;
    bool wantFocus = false;
};

static FreeTextInPlaceEdit gInPlace;
static double gInPlaceEndedAt = 0;
// orig's kContentsEditJustEndedMs
constexpr double kInPlaceJustEndedSecs = 0.4;

static bool FreeTextInPlaceEditJustEnded() {
    return gInPlaceEndedAt != 0 && (gp::TimeNow() - gInPlaceEndedAt) < kInPlaceJustEndedSecs;
}

bool IsEditingFreeTextInPlace(MainWindow* win) {
    if (!gInPlace.annot) {
        return false;
    }
    return !win || gInPlace.win == win;
}

// The tests set the win32 edit with WM_SETTEXT and commit with WM_CHAR LF.
// ng's box is a gpui textarea on the frame, so those messages land here.
bool FreeTextInPlaceSetText(MainWindow* win, const WCHAR* text) {
    if (!IsEditingFreeTextInPlace(win) || !gInPlace.edit || !text) {
        return false;
    }
    gp::InputSetValue(gInPlace.edit, ToGpui(ToUtf8Temp(text)));
    AppShellInvalidate(win);
    return true;
}

bool FreeTextInPlaceCommitOnChar(MainWindow* win, int ch) {
    if (ch != '\n' || !IsEditingFreeTextInPlace(win)) {
        return false;
    }
    EndFreeTextInPlaceEdit(true);
    return true;
}

TempStr FreeTextInPlaceEditStateTemp(MainWindow* win) {
    if (!IsEditingFreeTextInPlace(win) || !gInPlace.edit) {
        return StrL("freeTextEdit active=0 rect=0,0,0,0 text=\n");
    }
    Rect r{};
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    Annotation* annot = gInPlace.annot;
    if (dm && annot) {
        r = dm->CvtToScreen(PageNo(annot), GetRect(annot));
        float s = CanvasScale(win);
        if (s <= 0.f) {
            s = 1.f;
        }
        r.x += (int)((float)win->canvasRc.x / s);
        r.y += (int)((float)win->canvasRc.y / s);
    }
    TempStr text = str::DupTemp(FromGpui(gp::InputValue(gInPlace.edit)));
    text = str::ReplaceTemp(text, StrL("\r\n"), StrL("|"));
    text = str::ReplaceTemp(text, StrL("\n"), StrL("|"));
    return fmt("freeTextEdit active=1 rect=%d,%d,%d,%d text=%s\n", r.x, r.y, r.dx, r.dy, text);
}

void EndFreeTextInPlaceEdit(bool accept) {
    if (!gInPlace.annot) {
        return;
    }
    MainWindow* win = gInPlace.win;
    WindowTab* tab = gInPlace.tab;
    Annotation* annot = gInPlace.annot;
    gp::Bounds box = gInPlace.box;
    TempStr text = str::DupTemp(FromGpui(gp::InputValue(gInPlace.edit)));
    gInPlace.win = nullptr;
    gInPlace.tab = nullptr;
    gInPlace.annot = nullptr;
    gInPlace.box = {};
    gContentsEditJustEnded = true;

    bool winOk = win && IsMainWindowValidAndNotClosing(win);
    if (accept && AnnotationIsLive(annot)) {
        // the box may be enlarged and the text set: one undo step
        EngineBase* engine = tab ? tab->GetEngine() : nullptr;
        EngineMupdfBeginOperation(engine, "Edit text");
        defer {
            EngineMupdfEndOperation(engine);
        };
        DisplayModel* dm = winOk ? win->AsFixed() : nullptr;
        int pageNo = PageNo(annot);
        if (dm && dm->ValidPageNo(pageNo) && box.h > 0) {
            // the box grew with the text while typing; keep that room so
            // MuPDF doesn't clip what was just written. Compare in screen
            // pixels: converting the box back to page coordinates biases it
            // half a pixel up and to the left, so a page-space union always
            // looks bigger and would grow the annotation on every edit.
            RectF cur = GetRect(annot);
            Rect curScreen = dm->CvtToScreen(pageNo, cur);
            float k = CanvasScale(win);
            Rect editRect{curScreen.x, curScreen.y, (int)lroundf(box.w / k), (int)lroundf(box.h / k)};
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
    if (winOk) {
        AppShellFocusFrame(win);
        AppShellInvalidate(win);
    }
}

bool StartFreeTextInPlaceEdit(MainWindow* win, Annotation* annot) {
    if (!win || !win->gpuiWin || !AnnotationIsLive(annot)) {
        return false;
    }
    if (Type(annot) != AnnotationType::FreeText) {
        return false;
    }
    if (gInPlace.annot == annot) {
        return true;
    }
    EndFreeTextInPlaceEdit(true);
    WindowTab* tab = win->CurrentTab();
    DisplayModel* dm = win->AsFixed();
    int pageNo = PageNo(annot);
    if (!tab || !dm || !dm->ValidPageNo(pageNo) || !dm->PageVisible(pageNo)) {
        return false;
    }
    if (dm->CvtToScreen(pageNo, GetRect(annot)).IsEmpty()) {
        return false;
    }
    SetSelectedAnnotation(tab, annot);
    EnablePdfAnnotationsToolbar(win);
    if (!gInPlace.edit) {
        gInPlace.edit = new gp::InputState();
        gInPlace.edit->kind = gp::InputKind::Textarea;
        gInPlace.edit->focus = gp::FocusHandleNew(win->gpuiWin->app);
    }
    constexpr int kMaxInPlaceRows = 200;
    gp::TextareaSetAutoGrow(gInPlace.edit, 1, kMaxInPlaceRows);
    Str text = Contents(annot);
    gp::InputSetValue(gInPlace.edit, ToGpui(text));
    // caret at the end, nothing selected: this is editing what is there, not
    // replacing it
    gp::InputMoveTo(gInPlace.edit, win->gpuiWin->app, win->gpuiWin, len(text));
    gInPlace.win = win;
    gInPlace.tab = tab;
    gInPlace.annot = annot;
    gInPlace.box = {};
    gInPlace.wantFocus = true;
    AppShellInvalidate(win);
    return true;
}

// a double-click on free text edits its text where it sits on the page
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

// --- what the chips do ------------------------------------------------------

static void ChipColorPicked(AnnotEditToolbar* tb, Color col) {
    WindowTab* tab = tb->tab;
    Annotation* annot = tab ? tab->selectedAnnotation : nullptr;
    if (!AnnotationIsLive(annot) || annot != tb->annot) {
        return;
    }
    AnnotationType type = Type(annot);
    bool isNone = (col == kColorUnset);
    PdfColor pdfCol = isNone ? 0 : WinToPdfColor(col);
    u8 opacity = isNone ? 0 : PdfColorAlpha(pdfCol);
    bool setsOpacity = !isNone && AnnotationSupportsOpacity(type);
    // Color plus opacity is one undo step.
    EngineBase* engine = tab->GetEngine();
    EngineMupdfBeginOperation(engine, "Set color");
    defer {
        EngineMupdfEndOperation(engine);
    };
    switch (tb->popupChip) {
        case AnnotEditKind::Color:
            // SetColor() takes the opacity from the color's alpha
            SetColor(annot, setsOpacity ? pdfCol : OpaquePdfColor(pdfCol));
            break;
        case AnnotEditKind::InteriorColor:
            // /IC has no alpha of its own, the annotation's opacity covers it
            SetInteriorColor(annot, OpaquePdfColor(pdfCol));
            if (setsOpacity) {
                SetOpacity(annot, opacity);
            }
            break;
        case AnnotEditKind::TextColor:
            SetDefaultAppearanceTextColor(annot, OpaquePdfColor(pdfCol));
            if (setsOpacity) {
                SetOpacity(annot, opacity);
            }
            break;
        default:
            return;
    }
    AnnotChanged(tab);
}

// the dialog the pencil opens. cmdId 0 is the annotation preset list, not a
// toolbar button's (orig's ShowAnnotColorsDialog).
struct AnnotColorDlgTarget {
    AnnotEditToolbar* tb = nullptr;
};

static void AnnotColorDlgPicked(AnnotColorDlgTarget* target, ChangeColorsArgs* args) {
    if (args->colorsChanged && gSettings) {
        str::ReplaceWithCopy(&gSettings->annotations.presetColors, SerializeColorList(args->colors));
        ScheduleSaveSettings();
    }
    if (args->didSelect && args->color != kColorUnset && target->tb) {
        ChipColorPicked(target->tb, args->color);
    }
    delete target;
}

static void OpenAnnotColorsDialog(AnnotEditToolbar* tb) {
    MainWindow* win = tb->win;
    Color current = tb->popupColorNow;
    ClosePopup(tb);
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    auto* target = new AnnotColorDlgTarget();
    target->tb = tb;

    auto* args = new ChangeColorsArgs();
    args->win = win;
    args->title = Tr("Annotation Colors");
    args->color = current;
    args->withOpacity = true;
    AnnotPresetColors(0, args->colors);
    args->onClose = MkFunc1(AnnotColorDlgPicked, target);
    ShowChangeColorsDialog(args);
    AppShellInvalidate(win);
}

// how wide the stroke of an ink annotation is, from the Thickness slider of
// its color drop-down
static void ChipThicknessPicked(AnnotEditToolbar* tb, int width) {
    WindowTab* tab = tb->tab;
    Annotation* annot = tab ? tab->selectedAnnotation : nullptr;
    if (!AnnotationIsLive(annot) || annot != tb->annot || BorderWidth(annot) == width) {
        return;
    }
    SetBorderWidth(annot, width);
    AnnotChanged(tab);
}

static void ChipOpacityPicked(AnnotEditToolbar* tb, int percent) {
    WindowTab* tab = tb->tab;
    Annotation* annot = tab ? tab->selectedAnnotation : nullptr;
    if (!AnnotationIsLive(annot) || annot != tb->annot) {
        return;
    }
    int opacity = ((percent * 255) + 50) / 100;
    if (Opacity(annot) == opacity) {
        return;
    }
    SetOpacity(annot, opacity);
    AnnotChanged(tab);
}

static void ChipTextSizePicked(AnnotEditToolbar* tb, int size) {
    WindowTab* tab = tb->tab;
    Annotation* annot = tab ? tab->selectedAnnotation : nullptr;
    if (!AnnotationIsLive(annot) || annot != tb->annot || DefaultAppearanceTextSize(annot) == size) {
        return;
    }
    SetDefaultAppearanceTextSize(annot, size);
    AnnotChanged(tab);
}

static void SliderValuePicked(AnnotEditToolbar* tb, int value) {
    switch (tb->popupChip) {
        case AnnotEditKind::Opacity:
            ChipOpacityPicked(tb, value);
            break;
        case AnnotEditKind::Border:
            ChipThicknessPicked(tb, value);
            break;
        case AnnotEditKind::TextSize:
            ChipTextSizePicked(tb, value);
            break;
        default:
            break;
    }
}

static void ListValuePicked(AnnotEditToolbar* tb, int idx) {
    WindowTab* tab = tb->tab;
    Annotation* annot = tab ? tab->selectedAnnotation : nullptr;
    if (!AnnotationIsLive(annot) || annot != tb->annot || idx < 0) {
        return;
    }
    switch (tb->popupChip) {
        case AnnotEditKind::Alignment:
            SetQuadding(annot, idx);
            break;
        case AnnotEditKind::Icon: {
            SeqStrings icons = AnnotationIconNames(annot);
            SetIconName(annot, SeqStrByIndex(icons, idx));
            break;
        }
        case AnnotEditKind::LineStart:
            SetLineStartStyles(annot, idx);
            break;
        case AnnotEditKind::LineEnd:
            SetLineEndStyles(annot, idx);
            break;
        case AnnotEditKind::FontName: {
            Str family = SeqStrByIndex(gBase14FontFamilies, idx);
            SetFreeTextFont(annot, family, FreeTextFontStyle(annot));
            break;
        }
        default:
            return;
    }
    AnnotChanged(tab);
}

// the font an embedding prompt is about
struct EmbedFontCtx {
    MainWindow* win = nullptr;
    WindowTab* tab = nullptr;
    Annotation* annot = nullptr;
    Str family; // owned
};

static void ApplyOtherFont(MainWindow* win, WindowTab* tab, Annotation* annot, Str family) {
    // the prompt ran a few frames, in which the annotation could have gone
    if (!IsMainWindowValidAndNotClosing(win) || win->CurrentTab() != tab) {
        return;
    }
    if (!AnnotationIsLive(annot) || tab->selectedAnnotation != annot) {
        return;
    }
    SetFreeTextFont(annot, family, FreeTextFontStyle(annot));
    AnnotChanged(tab);
}

static void OnEmbedFontAnswer(EmbedFontCtx* c, int res) {
    if (res == MbRetOk) {
        ApplyOtherFont(c->win, c->tab, c->annot, c->family);
    }
    str::Free(c->family);
    delete c;
}

// orig's kFontMenuOther branch of PickFreeTextFont, with the list of installed
// fonts standing in for the Windows font dialog. ng: that dialog also picks
// bold / italic; here the style stays what the B / I chips say
static void OtherFontPicked(AnnotEditToolbar* tb, Str family) {
    WindowTab* tab = tb->tab;
    Annotation* annot = tab ? tab->selectedAnnotation : nullptr;
    if (!AnnotationIsLive(annot) || annot != tb->annot || len(family) == 0) {
        return;
    }
    Str prevFamily = FreeTextFontFamily(annot);
    // warn only when a font starts being embedded
    bool newlyEmbedded = !IsBase14FontFamily(family) && !str::EqI(family, prevFamily);
    if (!newlyEmbedded) {
        ApplyOtherFont(tb->win, tab, annot, family);
        return;
    }
    auto* c = new EmbedFontCtx{tb->win, tab, annot, str::Dup(family)};
    Str msg = Tr("Embedding this font can add hundreds of kilobytes to the PDF.");
    MsgBox(tb->win, msg, Tr("Embed Font"), MbOkCancel | MbIconWarning,
           MkFunc1<EmbedFontCtx, int>(OnEmbedFontAnswer, c));
}

// every installed font, sorted, with the annotation's own marked
static void OpenFontListPopup(AnnotEditToolbar* tb, int chipIdx, Str current) {
    tb->popupNames.Reset();
    GetInstalledFontNames(tb->popupNames);
    SortNoCase(&tb->popupNames);
    tb->popupKind = AnnotPopupKind::FontList;
    tb->popupChipIdx = chipIdx;
    tb->popupChip = AnnotEditKind::FontName;
    tb->popupCurrent = tb->popupNames.FindI(current);
    tb->fontScrollY = 0;
    str::ReplaceWithCopy(&tb->popupLabel, Tr("Font"));
}

static void OpenListPopup(AnnotEditToolbar* tb, int chipIdx, SeqStrings names, int current, Str label) {
    tb->popupNames.Reset();
    for (Str name = SeqStrFirst(names); len(name) > 0; name = SeqStrNext(name)) {
        tb->popupNames.Append(name);
    }
    tb->popupKind = AnnotPopupKind::List;
    tb->popupChipIdx = chipIdx;
    tb->popupCurrent = current;
    str::ReplaceWithCopy(&tb->popupLabel, label);
}

static void OpenSliderPopup(AnnotEditToolbar* tb, int chipIdx, Str label, int value, int minVal, int maxVal) {
    tb->popupKind = AnnotPopupKind::Slider;
    tb->popupChipIdx = chipIdx;
    str::ReplaceWithCopy(&tb->popupLabel, label);
    tb->sliderMin = minVal;
    tb->sliderMax = maxVal;
    tb->sliderValue = limitValue(value, minVal, maxVal);
}

static void OnChipClick(AnnotEditToolbar* tb, int chipIdx) {
    WindowTab* tab = tb->tab;
    Annotation* annot = tab ? tab->selectedAnnotation : nullptr;
    if (!AnnotationIsLive(annot) || annot != tb->annot || !VecIsValidIndex(tb->items, chipIdx)) {
        return;
    }
    AnnotEditItem item = tb->items[chipIdx];
    AnnotEditKind kind = item.kind;
    // a second click on the chip that opened the drop-down closes it
    if (tb->popupKind != AnnotPopupKind::None && tb->popupChipIdx == chipIdx) {
        ClosePopup(tb);
        return;
    }
    ClosePopup(tb);
    tb->popupChip = kind;
    switch (kind) {
        case AnnotEditKind::Color:
        case AnnotEditKind::InteriorColor:
        case AnnotEditKind::TextColor: {
            PdfColor col = item.color;
            tb->popupColorNow = (col == 0) ? kColorUnset : PdfToWinColorWithAlpha(col);
            // a text markup annotation without a color isn't invisible, mupdf
            // draws it in a default one, so offering "none" there is a trap
            tb->popupWithNone = !AnnotationIsTextMarkup(Type(annot));
            bool isColor = kind == AnnotEditKind::Color;
            bool isInk = (Type(annot) == AnnotationType::Ink) && isColor;
            bool isBorder = AnnotationBorderInColorChip(Type(annot)) && isColor;
            tb->popupThickness = (isInk || isBorder) ? std::max(BorderWidth(annot), 0) : -1;
            tb->popupThicknessMin = 1;
            bool isNoteColor = (Type(annot) == AnnotationType::Text) && isColor;
            Str label = isNoteColor ? Tr("Background Color") : Tr("Color");
            Str thicknessLabel = Tr("Thickness");
            if (isBorder) {
                label = Tr("Border Color");
                thicknessLabel = Tr("Border Width");
                // a shape can do without a border
                tb->popupThicknessMin = 0;
            }
            str::ReplaceWithCopy(&tb->popupLabel, label);
            str::ReplaceWithCopy(&tb->popupThicknessLabel, thicknessLabel);
            VecReset(tb->popupColors);
            AnnotPresetColors(0, tb->popupColors);
            tb->popupKind = AnnotPopupKind::Colors;
            tb->popupChipIdx = chipIdx;
            break;
        }
        case AnnotEditKind::Opacity: {
            // in percent, as the chip shows it; fully transparent would lose it
            int percent = ((item.number * 100) + 127) / 255;
            OpenSliderPopup(tb, chipIdx, Tr("Opacity"), percent, kOpacityPercentMin, 100);
            break;
        }
        case AnnotEditKind::Border:
            OpenSliderPopup(tb, chipIdx, Tr("Border Width"), std::max(BorderWidth(annot), 0), 0, kBorderWidthMax);
            break;
        case AnnotEditKind::TextSize:
            OpenSliderPopup(tb, chipIdx, Tr("Text Size"), item.number, kFreeTextSizeMin, kFreeTextSizeMax);
            break;
        case AnnotEditKind::FontName: {
            // orig's PickFreeTextFont: the base 14 fonts, the annotation's own
            // font if it is another one, and any installed font
            Str family = FreeTextFontFamily(annot);
            int cur = SeqStrIndexIS(gBase14FontFamilies, family);
            OpenListPopup(tb, chipIdx, gBase14ReadableNames, cur, Tr("Font"));
            if (!IsBase14FontFamily(family)) {
                tb->popupCurrentFontIdx = len(tb->popupNames);
                tb->popupCurrent = tb->popupCurrentFontIdx;
                tb->popupNames.Append(family);
            }
            tb->popupOtherFontIdx = len(tb->popupNames);
            tb->popupNames.Append(Tr("Other Font (Embedded)..."));
            break;
        }
        case AnnotEditKind::Bold:
        case AnnotEditKind::Italic:
        case AnnotEditKind::Underline:
            SetFreeTextFont(annot, FreeTextFontFamily(annot), FreeTextFontStyle(annot) ^ StyleBitForKind(kind));
            AnnotChanged(tab);
            break;
        case AnnotEditKind::Alignment:
            OpenListPopup(tb, chipIdx, gQuaddingNames, item.number, Tr("Text Alignment"));
            break;
        case AnnotEditKind::Icon: {
            SeqStrings icons = AnnotationIconNames(annot);
            OpenListPopup(tb, chipIdx, icons, SeqStrIndex(icons, item.iconName), Tr("Icon"));
            tb->popupGlyphs = item.mupdfIcon;
            break;
        }
        case AnnotEditKind::LineStart:
        case AnnotEditKind::LineEnd:
            OpenListPopup(tb, chipIdx, gLineEndingStyles, item.lineEnding,
                          kind == AnnotEditKind::LineStart ? Tr("Line Start") : Tr("Line End"));
            break;
        case AnnotEditKind::AttachFile: {
            if (!CanAccessDisk()) {
                break;
            }
            if (AppShellPickFileAsync(tb->win, Tr("Open"), {}, MkFunc1(OnAttachPicked, tb->win))) {
                break;
            }
            TempStr path = AppShellPromptForPathTemp(tb->win, Tr("Open"), {});
            if (len(path) > 0 && SetEmbeddedFileFromPath(annot, path)) {
                AnnotChanged(tab);
            }
            break;
        }
        case AnnotEditKind::SaveAttachment: {
            if (!HasEmbeddedFile(annot)) {
                break;
            }
            Str fileName = EmbeddedFileNameTemp(annot);
            if (len(fileName) == 0) {
                fileName = StrL("attachment");
            }
            SaveEmbeddedFileAs(tb->win, annot, path::GetBaseNameTemp(fileName));
            break;
        }
        case AnnotEditKind::Contents:
            uitask::Post(MkFunc0(StartSelectedAnnotContentsEdit, tb->win), "StartAnnotContentsEdit");
            break;
        case AnnotEditKind::Delete:
            // deleting takes the whole row (and this chip) down with it
            uitask::Post(MkFunc0(DeleteSelectedAnnotation, tb->win), "DeleteSelectedAnnot");
            break;
    }
    AppShellInvalidate(tb->win);
}

// --- painting the chips that are not text or an SVG icon --------------------

static void PaintChecker(gp::PaintCtx* ctx, gp::Bounds r) {
    gp::CanvasFillRect(ctx, r.x, r.y, r.w, r.h, ToGpui(MkRgb(240, 240, 240)));
    float s = (float)std::max(DpiScale(3), 2);
    gp::Rgba dark = ToGpui(MkRgb(200, 200, 200));
    for (float y = 0; y < r.h; y += s) {
        for (float x = 0; x < r.w; x += s) {
            if (((int)(x / s) + (int)(y / s)) & 1) {
                float w = std::min(s, r.w - x);
                float h = std::min(s, r.h - y);
                gp::CanvasFillRect(ctx, r.x + x, r.y + y, w, h, dark);
            }
        }
    }
}

static void PaintSwatch(gp::PaintCtx* ctx, gp::Bounds r, PdfColor col, Color border) {
    float inset = (float)DpiScale(4);
    gp::Bounds sw{r.x + inset, r.y + inset, r.w - 2 * inset, r.h - 2 * inset};
    if (sw.w < 4 || sw.h < 4) {
        sw = gp::Bounds{r.x + 2, r.y + 2, r.w - 4, r.h - 4};
    }
    u8 alpha = PdfColorAlpha(col);
    float radius = (float)DpiScale(3);
    if (alpha == 0) {
        PaintChecker(ctx, sw);
    } else if (alpha < 0xff) {
        // a translucent color over the checker, so the opacity can be seen
        PaintChecker(ctx, sw);
        Color c = PdfToWinColor(col);
        gp::Rgba rgba = ToGpui(c);
        rgba.a = alpha;
        gp::CanvasFillRect(ctx, sw.x, sw.y, sw.w, sw.h, rgba);
    } else {
        gp::CanvasFillRound(ctx, sw.x, sw.y, sw.w, sw.h, radius, ToGpui(PdfToWinColor(col)));
    }
    gp::CanvasStrokeRound(ctx, sw.x, sw.y, sw.w, sw.h, radius, 1, ToGpui(border));
}

static void PaintAlignment(gp::PaintCtx* ctx, gp::Bounds r, int quadding, Color col) {
    float pad = (float)DpiScale(6);
    gp::Bounds inner{r.x + pad, r.y + pad, r.w - 2 * pad, r.h - 2 * pad};
    if (inner.w < 6 || inner.h < 8) {
        inner = gp::Bounds{r.x + 2, r.y + 2, r.w - 4, r.h - 4};
    }
    float lineH = (float)std::max(DpiScale(2), 1);
    float gap = std::max((inner.h - (3 * lineH)) / 2, 1.f);
    float blockDy = (3 * lineH) + (2 * gap);
    float y = inner.y + ((inner.h - blockDy) / 2);
    float full = inner.w;
    float shortDx = std::max((full * 2) / 3, 4.f);
    gp::Rgba c = ToGpui(col);
    for (int i = 0; i < 3; i++) {
        float dx = (i == 1) ? shortDx : full;
        float x = inner.x;
        if (quadding == kQuaddingCenter) {
            x = inner.x + ((inner.w - dx) / 2);
        } else if (quadding == kQuaddingRight) {
            x = inner.x + inner.w - dx;
        }
        gp::CanvasFillRect(ctx, x, y, dx, lineH, c);
        y += lineH + gap;
    }
}

// a point in canvas dips; gpui's Point is a text cursor position
struct FPt {
    float x = 0;
    float y = 0;
};

// orig's PaintLineEndingMark, in floats
static void PaintLineEndingMark(gp::PaintCtx* ctx, FPt tip, FPt along, gp::Rgba col, int style, float size) {
    float dx = along.x;
    float dy = along.y;
    auto perp = [&](float s) { return FPt{tip.x - (dy * s / size), tip.y + (dx * s / size)}; };
    auto back = [&](float s) { return FPt{tip.x - (dx * s / size), tip.y - (dy * s / size)}; };
    auto line = [&](FPt a, FPt b) { gp::CanvasLine(ctx, a.x, a.y, b.x, b.y, 1.5f, col); };
    switch (style) {
        case 1: { // Square
            FPt p = back(size);
            gp::CanvasStrokeRound(ctx, p.x - (size / 2), p.y - (size / 2), size, size, 0, 1, col);
            break;
        }
        case 2: { // Circle
            FPt p = back(size / 2);
            gp::CanvasFillRound(ctx, p.x - (size / 2), p.y - (size / 2), size, size, size / 2, col);
            break;
        }
        case 3: { // Diamond
            FPt l = perp(size / 2);
            FPt r = perp(-size / 2);
            FPt b = back(size);
            line(tip, l);
            line(tip, r);
            line(l, b);
            line(r, b);
            break;
        }
        case 4:   // OpenArrow
        case 7: { // ROpenArrow
            FPt wing = (style == 7) ? FPt{-dx, -dy} : along;
            FPt t = (style == 7) ? back(size) : tip;
            FPt base = (style == 7) ? tip : back(size);
            line(t, FPt{base.x - (wing.y / 2), base.y + (wing.x / 2)});
            line(t, FPt{base.x + (wing.y / 2), base.y - (wing.x / 2)});
            break;
        }
        case 5:   // ClosedArrow
        case 8: { // RClosedArrow
            FPt t = (style == 8) ? back(size) : tip;
            FPt base = (style == 8) ? tip : back(size);
            FPt l{base.x - (dy / 2), base.y + (dx / 2)};
            FPt r{base.x + (dy / 2), base.y - (dx / 2)};
            line(t, l);
            line(t, r);
            line(l, r);
            break;
        }
        case 6: // Butt
            line(perp(size / 2), perp(-size / 2));
            break;
        case 9: { // Slash
            line(FPt{tip.x - (size / 2), tip.y - (size / 2)}, FPt{tip.x + (size / 2), tip.y + (size / 2)});
            break;
        }
        default:
            break;
    }
}

static void PaintLineEnding(gp::PaintCtx* ctx, gp::Bounds r, int style, bool isStart, Color col) {
    float pad = (float)DpiScale(5);
    float y = r.y + (r.h / 2);
    FPt left{r.x + pad, y};
    FPt right{r.x + r.w - pad, y};
    gp::Rgba c = ToGpui(col);
    gp::CanvasLine(ctx, left.x, left.y, right.x, right.y, 1.5f, c);
    float size = (float)DpiScale(8);
    if (isStart) {
        PaintLineEndingMark(ctx, left, FPt{-size, 0}, c, style, size);
    } else {
        PaintLineEndingMark(ctx, right, FPt{size, 0}, c, style, size);
    }
}

static const char* MupdfIconStream(Str name) {
    if (str::EqI(name, StrL("Comment"))) {
        return icon_comment;
    }
    if (str::EqI(name, StrL("Key"))) {
        return icon_key;
    }
    if (str::EqI(name, StrL("Note"))) {
        return icon_note;
    }
    if (str::EqI(name, StrL("Help"))) {
        return icon_help;
    }
    if (str::EqI(name, StrL("NewParagraph"))) {
        return icon_new_paragraph;
    }
    if (str::EqI(name, StrL("Paragraph"))) {
        return icon_paragraph;
    }
    if (str::EqI(name, StrL("Insert"))) {
        return icon_insert;
    }
    if (str::EqI(name, StrL("Graph"))) {
        return icon_graph;
    }
    if (str::EqI(name, StrL("PushPin"))) {
        return icon_push_pin;
    }
    if (str::EqI(name, StrL("Paperclip"))) {
        return icon_paperclip;
    }
    if (str::EqI(name, StrL("Tag"))) {
        return icon_tag;
    }
    if (str::EqI(name, StrL("Speaker"))) {
        return icon_speaker;
    }
    if (str::EqI(name, StrL("Mic"))) {
        return icon_mic;
    }
    return icon_star;
}

static bool IsPdfPathOpChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '*';
}

static void SkipPdfPathWs(const char*& p) {
    while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') {
        p++;
    }
}

static float PdfPathPop(float* stk, int& top) {
    if (top <= 0) {
        return 0;
    }
    return stk[--top];
}

// a path being turned into an SVG `d` attribute, and the box around it
struct IconSvgPath {
    str::Builder d;
    // the current transform (a b c d e f), as a PDF `cm` leaves it
    float m[6] = {1, 0, 0, 1, 0, 0};
    float x0 = 1e9f;
    float y0 = 1e9f;
    float x1 = -1e9f;
    float y1 = -1e9f;
    PointF cur{};

    PointF Xf(float x, float y) const { return {x * m[0] + y * m[2] + m[4], x * m[1] + y * m[3] + m[5]}; }
    void Grow(PointF p) {
        x0 = std::min(x0, p.x);
        y0 = std::min(y0, p.y);
        x1 = std::max(x1, p.x);
        y1 = std::max(y1, p.y);
    }
};

// ng: orig parses a pdf_write_icon_appearance glyph stream (m/l/c/re/h/cm/f)
// into an fz_path and has mupdf fill it into a pixmap (ParseMupdfIconPath,
// RenderMupdfAnnotIcon). gpui draws an SVG at any size, so the same parse
// writes an SVG path here
static void ParseMupdfIconPath(IconSvgPath& out, const char* s) {
    float stk[32];
    int top = 0;
    const char* p = s;
    for (;;) {
        SkipPdfPathWs(p);
        if (*p == 0) {
            break;
        }
        if (*p == '.' || *p == '-' || *p == '+' || (*p >= '0' && *p <= '9')) {
            char* end = nullptr;
            float v = strtof(p, &end);
            if (end == p || top >= dimofi(stk)) {
                break;
            }
            stk[top++] = v;
            p = end;
            continue;
        }
        if (!IsPdfPathOpChar(*p)) {
            p++;
            continue;
        }
        const char* op = p;
        while (IsPdfPathOpChar(*p)) {
            p++;
        }
        int nOp = (int)(p - op);
        if (nOp == 1 && (op[0] == 'm' || op[0] == 'l') && top >= 2) {
            float y = PdfPathPop(stk, top);
            float x = PdfPathPop(stk, top);
            PointF pt = out.Xf(x, y);
            out.d.Append(fmt("%c%g %g", op[0] == 'm' ? 'M' : 'L', pt.x, pt.y));
            out.Grow(pt);
            out.cur = pt;
        } else if (nOp == 1 && op[0] == 'c' && top >= 6) {
            float y3 = PdfPathPop(stk, top);
            float x3 = PdfPathPop(stk, top);
            float y2 = PdfPathPop(stk, top);
            float x2 = PdfPathPop(stk, top);
            float y1 = PdfPathPop(stk, top);
            float x1 = PdfPathPop(stk, top);
            PointF p0 = out.cur;
            PointF p1 = out.Xf(x1, y1);
            PointF p2 = out.Xf(x2, y2);
            PointF p3 = out.Xf(x3, y3);
            out.d.Append(fmt("C%g %g %g %g %g %g", p1.x, p1.y, p2.x, p2.y, p3.x, p3.y));
            // the box of the curve itself, not of its control points
            constexpr int kSteps = 8;
            for (int i = 1; i <= kSteps; i++) {
                float t = (float)i / (float)kSteps;
                float u = 1 - t;
                float b0 = u * u * u;
                float b1 = 3 * u * u * t;
                float b2 = 3 * u * t * t;
                float b3 = t * t * t;
                out.Grow(
                    {b0 * p0.x + b1 * p1.x + b2 * p2.x + b3 * p3.x, b0 * p0.y + b1 * p1.y + b2 * p2.y + b3 * p3.y});
            }
            out.cur = p3;
        } else if (nOp == 2 && op[0] == 'r' && op[1] == 'e' && top >= 4) {
            float h = PdfPathPop(stk, top);
            float w = PdfPathPop(stk, top);
            float y = PdfPathPop(stk, top);
            float x = PdfPathPop(stk, top);
            PointF a = out.Xf(x, y);
            PointF b = out.Xf(x + w, y);
            PointF c = out.Xf(x + w, y + h);
            PointF e = out.Xf(x, y + h);
            out.d.Append(fmt("M%g %g L%g %g L%g %g L%g %gZ", a.x, a.y, b.x, b.y, c.x, c.y, e.x, e.y));
            out.Grow(a);
            out.Grow(b);
            out.Grow(c);
            out.Grow(e);
            out.cur = a;
        } else if (nOp == 1 && op[0] == 'h') {
            out.d.Append(StrL("Z"));
        } else if (nOp == 2 && op[0] == 'c' && op[1] == 'm' && top >= 6) {
            float f = PdfPathPop(stk, top);
            float e = PdfPathPop(stk, top);
            float d = PdfPathPop(stk, top);
            float c = PdfPathPop(stk, top);
            float b = PdfPathPop(stk, top);
            float a = PdfPathPop(stk, top);
            // the new matrix applies first, then the one in force
            float* m = out.m;
            float r[6] = {a * m[0] + b * m[2], a * m[1] + b * m[3],        c * m[0] + d * m[2],
                          c * m[1] + d * m[3], e * m[0] + f * m[2] + m[4], e * m[1] + f * m[3] + m[5]};
            memcpy(m, r, sizeof(r));
        } else {
            // 'f' and unknown ops: drop the operands, keep the path
            top = 0;
        }
    }
}

struct IconSvgEntry {
    const char* stream = nullptr;
    Str svg; // owned, never freed
};

// the icon as an SVG document filled with the current color. The streams are
// y-down (mupdf's appearance cm flips them for PDF), which is SVG's direction
// too (issue #6112). Kept for the life of the process: gpui holds the pointer
static Str MupdfIconSvg(Str name) {
    static Vec<IconSvgEntry> cache;
    if (len(name) == 0) {
        name = StrL("Note");
    }
    const char* stream = MupdfIconStream(name);
    for (const IconSvgEntry& e : cache) {
        if (e.stream == stream) {
            return e.svg;
        }
    }
    IconSvgPath path;
    ParseMupdfIconPath(path, stream);
    float bw = path.x1 - path.x0;
    float bh = path.y1 - path.y0;
    if (bw < 0.5f) {
        bw = 8;
    }
    if (bh < 0.5f) {
        bh = 8;
    }
    // orig fits the glyph's box plus this much air into a square
    float pad = 0.4f;
    float side = std::max(bw, bh) + 2 * pad;
    TempStr svg =
        fmt("<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"%g %g %g %g\" fill=\"currentColor\" "
            "stroke=\"none\"><path d=\"%s\"/></svg>",
            path.x0 - pad, path.y0 - pad, side, side, ToStr(path.d));
    IconSvgEntry e;
    e.stream = stream;
    e.svg = str::Dup(svg);
    VecAppend(cache, e);
    return e.svg;
}

static gp::El* MupdfIconEl(gp::Ctx* cx, Str name, Color fg, int size) {
    return gpc::Icon::New(cx, gp::IconName::None)
        ->Data(ToGpui(MupdfIconSvg(name)))
        ->Size((float)size)
        ->Color(ToGpui(fg))
        ->IntoEl();
}

static void PaintChip(gp::PaintCtx* ctx, gp::El* e, void* user) {
    auto* p = (AnnotChipPaint*)user;
    gp::Bounds b = e->Bounds();
    switch (p->kind) {
        case AnnotEditKind::Color:
        case AnnotEditKind::InteriorColor:
        case AnnotEditKind::TextColor:
            PaintSwatch(ctx, b, p->color, BarMutedTextColor());
            break;
        case AnnotEditKind::Alignment:
            PaintAlignment(ctx, b, p->number, p->fg);
            break;
        case AnnotEditKind::LineStart:
        case AnnotEditKind::LineEnd:
            PaintLineEnding(ctx, b, p->number, p->lineIsStart, p->fg);
            break;
        default:
            break;
    }
}

// --- the gpui card ----------------------------------------------------------

struct AnnotEditView {
    MainWindow* win = nullptr;

    static void OnChip(AnnotEditView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t idx);
    static void OnChipHover(AnnotEditView* self, gp::Ctx* cx, const gp::HoverEvent* ev, int64_t idx);
    static void OnSwatch(AnnotEditView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t idx);
    static void OnEditColors(AnnotEditView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnListItem(AnnotEditView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t idx);
    static void OnFontItem(AnnotEditView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t idx);
    static void OnFontScroll(AnnotEditView* self, gp::Ctx* cx, const gp::ScrollEvent* ev);
    static void OnSliderStep(AnnotEditView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t step);
    static void OnContentsKey(AnnotEditView* self, gp::Ctx* cx, const gp::KeyEvent* ev);
    static void OnInPlaceKey(AnnotEditView* self, gp::Ctx* cx, const gp::KeyEvent* ev);
    static void OnInPlaceDownOut(AnnotEditView* self, gp::Ctx* cx, const gp::MouseDownEvent* ev);
    static void OnContentsOk(AnnotEditView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnContentsCancel(AnnotEditView* self, gp::Ctx* cx, const gp::ClickEvent*);
};

static gp::Entity<AnnotEditView> gAnnotEditView;

void AnnotEditView::OnChip(AnnotEditView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t idx) {
    AnnotEditToolbar* tb = self->win ? self->win->annotEditToolbar : nullptr;
    if (tb) {
        OnChipClick(tb, (int)idx);
    }
    gp::Notify(cx);
}

// ng: gpui attaches a tooltip to a component, not to a plain Div, so a chip
// asks for one from its hover listener (as the tab strip does)
void AnnotEditView::OnChipHover(AnnotEditView* self, gp::Ctx* cx, const gp::HoverEvent* ev, int64_t idx) {
    AnnotEditToolbar* tb = self->win ? self->win->annotEditToolbar : nullptr;
    if (!tb || !VecIsValidIndex(tb->items, (int)idx) || idx >= kMaxChips) {
        return;
    }
    if (!ev->hovered) {
        HoverTooltipHide(cx);
        return;
    }
    Str tip = tb->items[(int)idx].tooltip;
    if (len(tip) > 0) {
        HoverTooltipShow(cx, tip, tb->chipBounds[(int)idx]);
    }
}

void AnnotEditView::OnSwatch(AnnotEditView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t idx) {
    AnnotEditToolbar* tb = self->win ? self->win->annotEditToolbar : nullptr;
    if (!tb) {
        return;
    }
    // -1 is the "none" swatch
    Color col = kColorUnset;
    if (idx >= 0 && VecIsValidIndex(tb->popupColors, (int)idx)) {
        col = tb->popupColors[(int)idx];
    }
    ClosePopup(tb);
    ChipColorPicked(tb, col);
    gp::Notify(cx);
}

// orig's OnAnnotColorPopupEdit: the pencil opens the color dialog on this
// annotation's presets. The drop-down goes first, as on orig, so it cannot
// cover the dialog.
void AnnotEditView::OnEditColors(AnnotEditView* self, gp::Ctx* cx, const gp::ClickEvent*) {
    AnnotEditToolbar* tb = self->win ? self->win->annotEditToolbar : nullptr;
    if (!tb || tb->popupKind != AnnotPopupKind::Colors) {
        return;
    }
    OpenAnnotColorsDialog(tb);
    gp::Notify(cx);
    AppShellInvalidate(self->win);
}

void AnnotEditView::OnListItem(AnnotEditView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t idx) {
    AnnotEditToolbar* tb = self->win ? self->win->annotEditToolbar : nullptr;
    if (!tb) {
        return;
    }
    if (tb->popupChip == AnnotEditKind::FontName && tb->popupOtherFontIdx >= 0) {
        int chipIdx = tb->popupChipIdx;
        int otherIdx = tb->popupOtherFontIdx;
        int currentIdx = tb->popupCurrentFontIdx;
        if (idx == otherIdx) {
            Annotation* annot = LiveToolbarAnnot(tb);
            Str family = annot ? FreeTextFontFamily(annot) : Str{};
            ClosePopup(tb);
            OpenFontListPopup(tb, chipIdx, family);
            AppShellInvalidate(self->win);
            return;
        }
        if (idx == currentIdx) {
            // the font it already has
            ClosePopup(tb);
            AppShellInvalidate(self->win);
            return;
        }
    }
    ClosePopup(tb);
    ListValuePicked(tb, (int)idx);
    gp::Notify(cx);
}

void AnnotEditView::OnFontItem(AnnotEditView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t idx) {
    AnnotEditToolbar* tb = self->win ? self->win->annotEditToolbar : nullptr;
    if (!tb || tb->popupKind != AnnotPopupKind::FontList || idx < 0 || idx >= len(tb->popupNames)) {
        return;
    }
    TempStr family = str::DupTemp(tb->popupNames[(int)idx]);
    ClosePopup(tb);
    OtherFontPicked(tb, family);
    AppShellInvalidate(self->win);
    gp::Notify(cx);
}

void AnnotEditView::OnFontScroll(AnnotEditView* self, gp::Ctx* cx, const gp::ScrollEvent* ev) {
    AnnotEditToolbar* tb = self->win ? self->win->annotEditToolbar : nullptr;
    if (!tb) {
        return;
    }
    tb->fontScrollY = ev->offsetY;
    AppShellInvalidate(self->win);
    gp::Notify(cx);
}

void AnnotEditView::OnSliderStep(AnnotEditView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t step) {
    AnnotEditToolbar* tb = self->win ? self->win->annotEditToolbar : nullptr;
    if (!tb) {
        return;
    }
    if (tb->popupKind == AnnotPopupKind::Colors) {
        int v = limitValue(tb->popupThickness + (int)step, tb->popupThicknessMin, kBorderWidthMax);
        tb->popupThickness = v;
        ChipThicknessPicked(tb, v);
        gp::Notify(cx);
        return;
    }
    int v = limitValue(tb->sliderValue + (int)step, tb->sliderMin, tb->sliderMax);
    tb->sliderValue = v;
    SliderValuePicked(tb, v);
    gp::Notify(cx);
}

// orig's OnContentsEditWndProc: Ctrl+Enter is "done", Enter a new line. The
// text box takes Enter before the shell's key handler sees it, so this one
// runs in the capture phase
void AnnotEditView::OnContentsKey(AnnotEditView* self, gp::Ctx* cx, const gp::KeyEvent* ev) {
    // The text box takes Enter and Escape before the shell sees them.
    bool accept = ev->vk == VK_RETURN && ev->ctrl;
    bool cancel = ev->vk == VK_ESCAPE;
    if (!accept && !cancel) {
        return;
    }
    const_cast<gp::KeyEvent*>(ev)->propagate = false;
    EndAnnotContentsEdit(accept);
    AppShellFocusFrame(self->win);
    AppShellInvalidate(self->win);
    gp::Notify(cx);
}

// orig's WndProcFreeTextInPlaceEdit: Esc cancels, Ctrl+Enter is "done"
void AnnotEditView::OnInPlaceKey(AnnotEditView* self, gp::Ctx* cx, const gp::KeyEvent* ev) {
    bool cancel = ev->vk == VK_ESCAPE;
    bool accept = ev->vk == VK_RETURN && ev->ctrl;
    if (!cancel && !accept) {
        return;
    }
    const_cast<gp::KeyEvent*>(ev)->propagate = false;
    EndFreeTextInPlaceEdit(accept);
    gp::Notify(cx);
}

// orig's WM_KILLFOCUS: a click elsewhere is "done". If that click was on the
// Edit text button it must not start the edit again
void AnnotEditView::OnInPlaceDownOut(AnnotEditView* self, gp::Ctx* cx, const gp::MouseDownEvent*) {
    if (!IsEditingFreeTextInPlace(self->win)) {
        return;
    }
    EndFreeTextInPlaceEdit(true);
    gInPlaceEndedAt = gp::TimeNow();
    gp::Notify(cx);
}

void AnnotEditView::OnContentsOk(AnnotEditView*, gp::Ctx* cx, const gp::ClickEvent*) {
    EndAnnotContentsEdit(true);
    gp::Notify(cx);
}

void AnnotEditView::OnContentsCancel(AnnotEditView*, gp::Ctx* cx, const gp::ClickEvent*) {
    EndAnnotContentsEdit(false);
    gp::Notify(cx);
}

static TempStr ChipLabelTemp(const AnnotEditItem& item) {
    switch (item.kind) {
        case AnnotEditKind::Opacity:
            return fmt("%d%%", ((item.number * 100) + 127) / 255);
        case AnnotEditKind::Border:
        case AnnotEditKind::TextSize:
            return fmt("%d", item.number);
        case AnnotEditKind::Bold:
            return str::DupTemp(StrL("B"));
        case AnnotEditKind::Italic:
            return str::DupTemp(StrL("I"));
        case AnnotEditKind::Underline:
            return str::DupTemp(StrL("U"));
        case AnnotEditKind::Icon:
            return str::DupTemp(item.iconName);
        default:
            return str::DupTemp(item.text);
    }
}

static const char* ChipSvgIcon(AnnotEditKind kind) {
    switch (kind) {
        case AnnotEditKind::Contents:
            return gIconAnnotText;
        case AnnotEditKind::AttachFile:
            return gIconFileOpen;
        case AnnotEditKind::SaveAttachment:
            return gIconSave;
        case AnnotEditKind::Delete:
            return gIconTrash;
        default:
            return nullptr;
    }
}

static bool ChipIsCustomPainted(AnnotEditKind kind) {
    switch (kind) {
        case AnnotEditKind::Color:
        case AnnotEditKind::InteriorColor:
        case AnnotEditKind::TextColor:
        case AnnotEditKind::Alignment:
        case AnnotEditKind::LineStart:
        case AnnotEditKind::LineEnd:
            return true;
        default:
            return false;
    }
}

static gp::El* BuildChip(AnnotEditToolbar* tb, gp::Ctx* cx, int idx, int rowDy) {
    const AnnotEditItem& item = tb->items[idx];
    Color fg = BarTextColor();
    float w = (float)rowDy;
    if (item.kind == AnnotEditKind::LineStart || item.kind == AnnotEditKind::LineEnd) {
        w = (float)(rowDy * 2);
    }
    TempStr label = ChipLabelTemp(item);
    bool custom = ChipIsCustomPainted(item.kind);
    const char* svg = ChipSvgIcon(item.kind);
    bool glyph = item.kind == AnnotEditKind::Icon && item.mupdfIcon;
    if (!custom && !svg && !glyph && len(label) > 0) {
        w = (float)(len(label) * 8 + 2 * DpiScale(kBtnPadX));
    }
    TempStr id = fmt("annot-chip-%d", idx);
    gp::El* chip = gp::Div(cx->a)
                       ->FlexRow()
                       ->ItemsCenter()
                       ->JustifyCenter()
                       ->W(w)
                       ->H((float)rowDy)
                       ->Shrink0()
                       ->Radius((float)DpiScale(kButtonRadius))
                       ->HoverBg(ToGpui(BarHoverBg()))
                       ->Cursor(gp::CursorKind::Pointer)
                       ->BoundsOut(&tb->chipBounds[idx])
                       ->PathClick(GpuiDup(cx->a, id))
                       ->OnHover(gp::ListenTo(gAnnotEditView, &AnnotEditView::OnChipHover, (intptr_t)idx))
                       ->OnClick(gp::ListenTo(gAnnotEditView, &AnnotEditView::OnChip, (intptr_t)idx));
    bool isStyleToggle =
        item.kind == AnnotEditKind::Bold || item.kind == AnnotEditKind::Italic || item.kind == AnnotEditKind::Underline;
    if (isStyleToggle && item.number != 0) {
        chip->Bg(ToGpui(BarActiveBg()));
    }
    if (custom) {
        AnnotChipPaint* p = &tb->chipPaint[idx];
        p->kind = item.kind;
        p->color = item.color;
        p->number = item.kind == AnnotEditKind::Alignment ? item.number : item.lineEnding;
        p->lineIsStart = item.lineIsStart;
        p->fg = fg;
        chip->customPaint = &PaintChip;
        chip->customUser = p;
        return chip;
    }
    if (glyph) {
        // orig's PaintMupdfAnnotIcon: the glyph, 3 px inside the chip
        chip->Child(MupdfIconEl(cx, item.iconName, fg, rowDy - 2 * DpiScale(3)));
        return chip;
    }
    if (svg) {
        chip->Child(gpc::Icon::New(cx, gp::IconName::None)
                        ->Data(ToGpui(Str(svg)))
                        ->Size((float)DpiScale(16))
                        ->Color(ToGpui(fg))
                        ->IntoEl());
        return chip;
    }
    if (len(label) > 0) {
        gp::El* t = gp::TextEl(cx->a, GpuiDup(cx->a, label))->Font(13)->Fg(ToGpui(fg));
        if (item.kind == AnnotEditKind::Bold) {
            t->Bold();
        }
        chip->Child(t);
    }
    return chip;
}

// "Accept  Ctrl + Enter": orig's ButtonWithKbd, a label and its shortcut
static gp::El* ContentsButton(gp::Ctx* cx, Str id, Str label, Str shortcut, bool isDefault, gp::Listener onClick) {
    gp::El* body = gp::Div(cx->a)->FlexRow()->ItemsCenter()->Gap(6);
    body->Child(gp::TextEl(cx->a, GpuiDup(cx->a, label))->Font(13));
    body->Child(gpc::Kbd::New(cx, GpuiDup(cx->a, shortcut))->IntoEl());
    gpc::Button* b =
        gpc::Button::New(cx, GpuiDup(cx->a, id))->Child(body)->WithSize(gp::UiSize::Small)->OnClick(onClick);
    if (isDefault) {
        b->Primary();
    }
    return b->IntoEl();
}

// orig's StartContentsEdit / LayoutContentsEditor: a framed multi-line edit of
// five lines, as wide as the annotation but at least 320 and never wider than
// the canvas, over an Accept and a Cancel button. Enter is a new line;
// Ctrl+Enter, Esc and a click elsewhere end the edit
static gp::El* BuildContentsEditor(AnnotEditToolbar* tb, gp::Ctx* cx, int annotDx) {
    constexpr int kContentsLines = 5;
    int canvasDx = (int)((float)tb->win->canvasRc.dx * CanvasScale(tb->win));
    int wantDx = std::max(annotDx, DpiScale(320));
    wantDx = std::min(wantDx, std::max(canvasDx - DpiScale(24), DpiScale(200)));

    gp::El* col = gp::Div(cx->a)
                      ->FlexCol()
                      ->Gap((float)DpiScale(kBtnGap))
                      ->W((float)wantDx)
                      ->CaptureKeyDown(gp::ListenTo(gAnnotEditView, &AnnotEditView::OnContentsKey));
    col->Child(gpc::Textarea::New(cx, GStrL("annot-contents"), tb->contentsEdit)->Rows(kContentsLines)->IntoEl());
    gp::El* row = gp::Div(cx->a)->FlexRow()->Gap((float)DpiScale(kBtnGap))->JustifyStart()->W(gp::kFill);
    row->Child(ContentsButton(cx, StrL("annot-contents-ok"), Tr("Accept"), StrL("Ctrl + Enter"), true,
                              gp::ListenTo(gAnnotEditView, &AnnotEditView::OnContentsOk)));
    row->Child(ContentsButton(cx, StrL("annot-contents-cancel"), Tr("Cancel"), StrL("Esc"), false,
                              gp::ListenTo(gAnnotEditView, &AnnotEditView::OnContentsCancel)));
    col->Child(row);
    return col;
}

// the color grid / slider / list a chip opened, drawn under the row
static gp::El* BuildPopup(AnnotEditToolbar* tb, gp::Ctx* cx) {
    tb->nSwatches = 0;
    tb->editBounds = {};
    if (tb->popupKind == AnnotPopupKind::None) {
        return nullptr;
    }
    Color fg = BarTextColor();
    gp::El* card = gp::Div(cx->a)
                       ->FlexCol()
                       ->Gap(4)
                       ->Pad((float)DpiScale(6))
                       ->Radius((float)DpiScale(kCornerRadius))
                       ->Bg(ToGpui(BarBg()))
                       ->Border(1, ToGpui(BarBorderColor()));
    if (len(tb->popupLabel) > 0) {
        card->Child(gp::TextEl(cx->a, GpuiDup(cx->a, tb->popupLabel))->Font(12)->Fg(ToGpui(BarMutedTextColor())));
    }
    if (tb->popupKind == AnnotPopupKind::Colors) {
        gp::El* grid = gp::Div(cx->a)->FlexRow()->Wrap()->Gap(4)->MaxW((float)DpiScale(220));
        float d = (float)DpiScale(20);
        auto addSwatch = [&](bool none, Color c, int clickIdx) {
            if (tb->nSwatches >= (int)dimof(tb->swatchBounds)) {
                return;
            }
            int slot = tb->nSwatches++;
            tb->swatchNone[slot] = none;
            tb->swatchColor[slot] = c;
            TempStr id = none ? StrL("annot-col-none") : fmt("annot-col-%d", clickIdx);
            gp::El* dot = gp::Div(cx->a)
                              ->W(d)
                              ->H(d)
                              ->Radius(d / 2)
                              ->Border(1, ToGpui(BarMutedTextColor()))
                              ->Cursor(gp::CursorKind::Pointer)
                              ->BoundsOut(&tb->swatchBounds[slot])
                              ->PathClick(GpuiDup(cx->a, id))
                              ->OnClick(gp::ListenTo(gAnnotEditView, &AnnotEditView::OnSwatch, (intptr_t)clickIdx));
            if (!none) {
                dot->Bg(ToGpui(c));
            }
            grid->Child(dot);
        };
        if (tb->popupWithNone) {
            addSwatch(true, kColorUnset, -1);
        }
        for (int i = 0; i < len(tb->popupColors); i++) {
            addSwatch(false, tb->popupColors[i], i);
        }
        // orig's pencil, to the right of the swatches
        constexpr int kAnnotEditPad = 5;
        float pad = (float)DpiScale(kAnnotEditPad);
        gp::El* editBtn = gp::Div(cx->a)
                              ->Pad(pad)
                              ->HoverBg(ToGpui(BarHoverBg()))
                              ->Cursor(gp::CursorKind::Pointer)
                              ->Tip(ToGpui(Tr("Edit colors")))
                              ->BoundsOut(&tb->editBounds)
                              ->PathClick(GStrL("annot-col-edit"))
                              ->OnClick(gp::ListenTo(gAnnotEditView, &AnnotEditView::OnEditColors));
        editBtn->Child(gpc::Icon::New(cx, gp::IconName::None)
                           ->Data(ToGpui(Str(gIconEditAnnotations)))
                           ->Size((float)DpiScale(16))
                           ->Color(ToGpui(fg))
                           ->IntoEl());
        grid->Child(editBtn);
        card->Child(grid);
    }
    // a slider is "- value +": gpui's Slider reports through an entity
    // subscription a per-use popup cannot hold (see "gpui gaps")
    bool hasSlider = tb->popupKind == AnnotPopupKind::Slider || tb->popupThickness >= 0;
    if (hasSlider) {
        bool thickness = tb->popupKind == AnnotPopupKind::Colors;
        int value = thickness ? tb->popupThickness : tb->sliderValue;
        Str label = thickness ? tb->popupThicknessLabel : Str{};
        gp::El* row = gp::Div(cx->a)->FlexRow()->ItemsCenter()->Gap(6);
        if (len(label) > 0) {
            row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, label))->Font(12)->Fg(ToGpui(BarMutedTextColor())));
        }
        row->Child(gpc::Button::New(cx, GStrL("annot-slider-dn"))
                       ->Label(GStrL("-"))
                       ->WithSize(gp::UiSize::Small)
                       ->OnClick(gp::ListenTo(gAnnotEditView, &AnnotEditView::OnSliderStep, (intptr_t)-1))
                       ->IntoEl());
        row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, fmt("%d", value)))->Font(13)->Fg(ToGpui(fg))->W(32));
        row->Child(gpc::Button::New(cx, GStrL("annot-slider-up"))
                       ->Label(GStrL("+"))
                       ->WithSize(gp::UiSize::Small)
                       ->OnClick(gp::ListenTo(gAnnotEditView, &AnnotEditView::OnSliderStep, (intptr_t)1))
                       ->IntoEl());
        card->Child(row);
    }
    if (tb->popupKind == AnnotPopupKind::List) {
        gp::El* list = gp::Div(cx->a)->FlexCol()->Gap(1)->MaxH((float)DpiScale(360));
        for (int i = 0; i < len(tb->popupNames); i++) {
            TempStr id = fmt("annot-li-%d", i);
            gp::El* row = gp::Div(cx->a)
                              ->FlexRow()
                              ->ItemsCenter()
                              ->PadX(6)
                              ->PadY(2)
                              ->Radius(4)
                              ->HoverBg(ToGpui(BarHoverBg()))
                              ->Cursor(gp::CursorKind::Pointer)
                              ->PathClick(GpuiDup(cx->a, id))
                              ->OnClick(gp::ListenTo(gAnnotEditView, &AnnotEditView::OnListItem, (intptr_t)i));
            if (i == tb->popupCurrent) {
                row->Bg(ToGpui(BarActiveBg()));
            }
            if (tb->popupGlyphs) {
                // orig's PopupPickGlyphs: an 18 px glyph in front of the name
                row->Gap(6)->Child(MupdfIconEl(cx, tb->popupNames[i], fg, DpiScale(18)));
            }
            if (i == tb->popupOtherFontIdx) {
                list->Child(gp::Div(cx->a)->W(gp::kFill)->H(1)->Shrink0()->Bg(ToGpui(BarBorderColor())));
            }
            row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, tb->popupNames[i]))->Font(13)->Fg(ToGpui(fg)));
            list->Child(row);
        }
        card->Child(list);
    }
    if (tb->popupKind == AnnotPopupKind::FontList) {
        // only the rows in view are built, between two spacers that stand in
        // for the rest
        constexpr int kFontRows = 12;
        constexpr int kOverscan = 4;
        float rowDy = (float)DpiScale(22);
        int n = len(tb->popupNames);
        float viewDy = rowDy * (float)std::min(n, kFontRows);
        float maxScroll = std::max(0.f, (float)n * rowDy - viewDy);
        tb->fontScrollY = std::max(0.f, std::min(tb->fontScrollY, maxScroll));
        int first = std::max((int)(tb->fontScrollY / rowDy) - kOverscan, 0);
        int last = std::min(first + kFontRows + 1 + 2 * kOverscan, n);
        gp::El* rows = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->PadR((float)DpiScale(10));
        if (first > 0) {
            rows->Child(gp::Div(cx->a)->W(gp::kFill)->H((float)first * rowDy)->Shrink0());
        }
        for (int i = first; i < last; i++) {
            TempStr id = fmt("annot-font-%d", i);
            gp::El* row = gp::Div(cx->a)
                              ->FlexRow()
                              ->ItemsCenter()
                              ->W(gp::kFill)
                              ->H(rowDy)
                              ->Shrink0()
                              ->PadX(6)
                              ->Radius(4)
                              ->HoverBg(ToGpui(BarHoverBg()))
                              ->Cursor(gp::CursorKind::Pointer)
                              ->PathClick(GpuiDup(cx->a, id))
                              ->OnClick(gp::ListenTo(gAnnotEditView, &AnnotEditView::OnFontItem, (intptr_t)i));
            if (i == tb->popupCurrent) {
                row->Bg(ToGpui(BarActiveBg()));
            }
            row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, tb->popupNames[i]))
                           ->Font(13)
                           ->Fg(ToGpui(fg))
                           ->Flex1()
                           ->MinW(0)
                           ->Truncate());
            rows->Child(row);
        }
        if (last < n) {
            rows->Child(gp::Div(cx->a)->W(gp::kFill)->H((float)(n - last) * rowDy)->Shrink0());
        }
        card->Child(gp::Div(cx->a)
                        ->Id(GStrL("annot-font-list"))
                        ->FlexCol()
                        ->W(gp::kFill)
                        ->MinW((float)DpiScale(240))
                        ->H(viewDy)
                        ->Shrink0()
                        ->ScrollY(tb->fontScrollY)
                        ->ScrollFromPath()
                        ->OnScroll(gp::ListenTo(gAnnotEditView, &AnnotEditView::OnFontScroll))
                        ->Child(rows));
    }
    return card;
}

// MuPDF pads free text by twice the border width (pdf_write_free_text_appearance)
constexpr float kFreeTextPadPerBorder = 2.f;

gp::El* FreeTextInPlaceEditBuild(MainWindow* win, gp::Ctx* cx) {
    if (!IsEditingFreeTextInPlace(win)) {
        return nullptr;
    }
    Annotation* annot = gInPlace.annot;
    DisplayModel* dm = win->AsFixed();
    WindowTab* tab = win->CurrentTab();
    if (!dm || !AnnotationIsLive(annot) || tab != gInPlace.tab || tab->selectedAnnotation != annot) {
        // the annotation or its tab went away under the box
        EndFreeTextInPlaceEdit(AnnotationIsLive(annot));
        return nullptr;
    }
    int pageNo = PageNo(annot);
    if (!dm->PageVisible(pageNo)) {
        return nullptr;
    }
    if (!gAnnotEditView.IsValid()) {
        gAnnotEditView = gp::EntityNewState<AnnotEditView>(cx->app);
    }
    auto* view = (AnnotEditView*)gp::EntityGet(cx->app, gAnnotEditView.id);
    view->win = win;

    RectF pageRect = GetRect(annot);
    Rect rc = dm->CvtToScreen(pageNo, pageRect);
    float k = CanvasScale(win);
    // screen pixels per PDF point, so the box matches the rendered text
    float scale = pageRect.dy > 0 ? ((float)rc.dy / pageRect.dy) : 1.f;
    int textSize = DefaultAppearanceTextSize(annot);
    if (textSize <= 0) {
        textSize = 12;
    }
    float fontPx = std::max(6.f, roundf((float)textSize * scale)) * k;
    float pad = kFreeTextPadPerBorder * (float)std::max(BorderWidth(annot), 0) * scale * k;

    // the annotation's text color on white, as orig's FreeTextInPlaceEditCtlColor
    Color textCol = kColBlack;
    PdfColor pdfTextCol = DefaultAppearanceTextColor(annot);
    if (pdfTextCol != kColorUnset) {
        u8 r, g, b, a;
        UnpackPdfColor(pdfTextCol, r, g, b, a);
        textCol = MkRgb(r, g, b);
    }
    gp::InputEditorStyle style;
    style.fontSize = fontPx;
    style.foreground = ToGpui(textCol);
    style.caret = ToGpui(textCol);
    style.background = ToGpui(kColWhite);

    gp::El* box = gp::Div(cx->a)
                      ->Absolute()
                      ->Left((float)rc.x * k)
                      ->Top((float)rc.y * k)
                      ->W((float)rc.dx * k)
                      ->MinH((float)rc.dy * k)
                      ->Pad(pad)
                      ->Bg(ToGpui(kColWhite))
                      // a frame in the annotation's text color
                      ->Border(1, ToGpui(textCol))
                      ->BoundsOut(&gInPlace.box)
                      ->CaptureKeyDown(gp::ListenTo(gAnnotEditView, &AnnotEditView::OnInPlaceKey))
                      ->OnMouseDownOut(gp::ListenTo(gAnnotEditView, &AnnotEditView::OnInPlaceDownOut));
    box->Child(gp::Textarea::New(cx, gInPlace.edit, style));
    if (gInPlace.wantFocus) {
        gInPlace.wantFocus = false;
        gp::InputFocus(gInPlace.edit, cx->app, cx->win);
    }
    return box;
}

gp::El* AnnotEditToolbarBuild(MainWindow* win, gp::Ctx* cx) {
    AnnotEditToolbar* tb = win ? win->annotEditToolbar : nullptr;
    if (!tb || !win->pdfAnnotationsToolbarEnabled) {
        return nullptr;
    }
    Annotation* annot = LiveToolbarAnnot(tb);
    Rect bounds;
    if (!annot || len(tb->items) == 0 || !GetAnnotScreenBounds(win, annot, bounds)) {
        return nullptr;
    }
    if (!gAnnotEditView.IsValid()) {
        gAnnotEditView = gp::EntityNewState<AnnotEditView>(cx->app);
    }
    auto* view = (AnnotEditView*)gp::EntityGet(cx->app, gAnnotEditView.id);
    view->win = win;

    int rowDy = DpiScale(24);
    float margin = (float)DpiScale(kMargin);
    gp::El* card = gp::Div(cx->a)
                       ->FlexCol()
                       ->Gap(4)
                       ->Pad(margin)
                       ->Radius((float)DpiScale(kCornerRadius))
                       ->Bg(ToGpui(BarBg()))
                       ->Border(1, ToGpui(BarBorderColor()));
    gp::El* row = gp::Div(cx->a)->FlexRow()->ItemsCenter()->Gap((float)DpiScale(kBtnGap));
    int n = std::min(len(tb->items), kMaxChips);
    tb->nChips = n;
    for (int i = 0; i < n; i++) {
        row->Child(BuildChip(tb, cx, i, rowDy));
    }
    card->Child(row);
    if (tb->editingContents && tb->contentsEdit) {
        card->Child(BuildContentsEditor(tb, cx, (int)((float)bounds.dx * CanvasScale(win))));
    }
    if (gp::El* popup = BuildPopup(tb, cx)) {
        card->Child(popup);
    }
    card->BoundsOut(&tb->measured);

    // orig places the row under the annotation, above it when there is no room
    float k = CanvasScale(win);
    float w = tb->measured.w > 0 ? tb->measured.w : (float)(n * (rowDy + 2) + 2 * (int)margin);
    float h = tb->measured.h > 0 ? tb->measured.h : (float)(rowDy + 2 * (int)margin);
    float gap = (float)DpiScale(6);
    float canvasW = (float)win->canvasRc.dx;
    float canvasH = (float)win->canvasRc.dy;
    float x = ((float)bounds.x + (float)bounds.dx / 2) * k - (w / 2);
    float y = (float)(bounds.y + bounds.dy) * k + gap;
    if (y + h > canvasH) {
        y = (float)bounds.y * k - gap - h;
    }
    x = std::max(std::min(x, canvasW - w), 0.f);
    y = std::max(std::min(y, canvasH - h), 0.f);
    card->Absolute()->Left(x)->Top(y);

    if (tb->editingContents && tb->contentsEdit) {
        gp::InputFocus(tb->contentsEdit, cx->app, cx->win);
    }
    return card;
}

TempStr AnnotEditToolbarStateTemp(MainWindow* win) {
    AnnotEditToolbar* tb = win ? win->annotEditToolbar : nullptr;
    bool visible = tb && tb->annot && win->pdfAnnotationsToolbarEnabled && len(tb->items) > 0;
    if (!visible) {
        return fmt("annotEditToolbar visible=0 n=0 items= editing=0\n");
    }
    str::Builder items;
    for (int i = 0; i < len(tb->items); i++) {
        if (i > 0) {
            items.AppendChar(',');
        }
        items.Append(KindName(tb->items[i].kind));
    }
    Annotation* annot = LiveToolbarAnnot(tb);
    gp::Bounds b = tb->measured;
    str::Builder chips;
    int nChips = std::min(tb->nChips, (int)dimof(tb->chipBounds));
    for (int i = 0; i < nChips; i++) {
        if (i > 0) {
            chips.AppendChar(';');
        }
        gp::Bounds c = tb->chipBounds[i];
        Str name = i < len(tb->items) ? KindName(tb->items[i].kind) : StrL("?");
        chips.Append(fmt("%s:%d,%d,%d,%d:", name, (int)c.x, (int)c.y, (int)c.w, (int)c.h));
    }
    return fmt(
        "annotEditToolbar visible=1 n=%d items=%s placed=%d,%d,%d,%d editing=%d popup=%d "
        "fontStyle=%d font=%s chips=%s\n",
        len(tb->items), ToStrTemp(items), (int)b.x, (int)b.y, (int)b.w, (int)b.h, tb->editingContents ? 1 : 0,
        (int)tb->popupKind, FreeTextFontStyle(annot), FreeTextFontFamily(annot), ToStrTemp(chips));
}

TempStr AnnotColorPopupStateTemp(MainWindow* win) {
    AnnotEditToolbar* tb = win ? win->annotEditToolbar : nullptr;
    if (!tb || tb->popupKind != AnnotPopupKind::Colors) {
        return StrL("annotColorPopup visible=0 n=0 thickness= swatches=\n");
    }
    str::Builder swatches;
    int n = 0;
    for (int i = 0; i < tb->nSwatches && i < (int)dimof(tb->swatchBounds); i++) {
        gp::Bounds b = tb->swatchBounds[i];
        if (b.w < 1.f || b.h < 1.f) {
            continue;
        }
        if (n > 0) {
            swatches.AppendChar(';');
        }
        bool none = tb->swatchNone[i];
        bool current = none ? tb->popupColorNow == kColorUnset : tb->swatchColor[i] == tb->popupColorNow;
        Str name = none ? StrL("none") : fmt("#%08x", (unsigned)tb->swatchColor[i]);
        swatches.Append(fmt("%s:%d,%d,%d,%d:%d", name, (int)b.x, (int)b.y, (int)b.w, (int)b.h, current ? 1 : 0));
        n++;
    }
    gp::Bounds e = tb->editBounds;
    return fmt("annotColorPopup visible=1 n=%d placed=0,0,0,0 thickness= edit=%d,%d,%d,%d swatches=%s\n", n, (int)e.x,
               (int)e.y, (int)e.w, (int)e.h, ToStrTemp(swatches));
}

// --- selection and the annotation lists -------------------------------------

// Drop non-owning Annotation* held by UI (selection, drag, hover, form edit).
// Call before DeleteAnnotation frees the wrapper, or when the engine is about
// to die and raw Annotation* must not be used again.
void DetachAnnotationFromUI(Annotation* annot) {
    if (!annot) {
        return;
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
        }
        HideAnnotationTextPopupFor(win, annot);
        int nTabs = win->TabCount();
        for (int i = 0; i < nTabs; i++) {
            WindowTab* t = win->GetTab(i);
            if (t && t->selectedAnnotation == annot) {
                t->selectedAnnotation = nullptr;
                HideAnnotEditToolbar(win);
            }
        }
    }
    if (annot == gClickAnchorAnnot) {
        gClickAnchorAnnot = nullptr;
    }
}

// every Annotation* the UI holds points into the tab's engine; drop them all
void CloseAnnotationUiForTab(WindowTab* tab) {
    if (!tab) {
        return;
    }
    tab->selectedAnnotation = nullptr;
    MainWindow* win = tab->win;
    if (win) {
        if (win->annotationBeingDragged) {
            EndPdfEditOperation(win);
        }
        win->annotationBeingDragged = nullptr;
        win->annotationBeingResized = false;
        win->annotationUnderCursor = nullptr;
        VecReset(win->annotationVertexPreview);
        HideAnnotationTextPopup(win);
        CommitFormFieldEdit(false);
        HideAnnotEditToolbar(win);
    }
    // Not just for the current tab: by the time a tab is deleted it has already
    // been unlinked from the window, and the filter list may still be holding
    // its annotations.
    ClearAnnotFilterAnnotations(win);
}

// Clear non-owning Annotation* before the old engine is destroyed.
void InvalidateEditAnnotationsOnEngineChange(WindowTab* tab) {
    CloseAnnotationUiForTab(tab);
}

void DeleteAnnotationAndUpdateUI(WindowTab* tab, Annotation* annot) {
    if (!annot || !tab) {
        return;
    }
    Annotation* keepSelected = annot == tab->selectedAnnotation ? nullptr : tab->selectedAnnotation;

    DetachAnnotationFromUI(annot);
    DeleteAnnotation(annot);
    RefreshAnnotationLists(tab);
    SetSelectedAnnotation(tab, keepSelected);
    if (IsMainWindowValidAndNotClosing(tab->win)) {
        MainWindowRerender(tab->win);
        ToolbarUpdateStateForWindow(tab->win, true);
    }
}

void NotifyAnnotationsChanged(WindowTab* tab) {
    if (tab && tab->win) {
        UpdateAnnotFilterToolbar(tab->win);
    }
    CommandPaletteOnAnnotationsChanged();
}

// GoToPage / canvas scroll for the current selection. Posted so holding
// arrows in the annot list can keep moving the caret (issue #6009).
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
    win->RedrawAll(true);
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
    if (!tab) {
        return;
    }
    MainWindow* win = tab->win;
    if (annot == tab->selectedAnnotation) {
        if (IsMainWindowValidAndNotClosing(win)) {
            MainWindowRerender(win);
            ToolbarUpdateStateForWindow(win, false);
            UpdateAnnotEditToolbar(win);
            UpdateAnnotFilterToolbar(win);
        }
        return;
    }
    tab->selectedAnnotation = annot;
    tab->didScrollToSelectedAnnotation = false;
    ScheduleShowSelectedAnnotationView(tab);
    if (IsMainWindowValidAndNotClosing(win)) {
        UpdateAnnotEditToolbar(win);
        UpdateAnnotFilterToolbar(win);
        AppShellInvalidate(win);
    }
}

static void AddAnnotPage(Vec<int>& pages, int pageNo, int pageCount) {
    if (pageNo < 1 || pageNo > pageCount || VecContains(pages, pageNo)) {
        return;
    }
    VecAppend(pages, pageNo);
}

// Pages the background loader should finish first: current page, every page
// overlapping the viewport, and a context-menu annotation's page.
static void CollectPriorityAnnotPages(WindowTab* tab, Annotation* extra, Vec<int>& pages) {
    VecReset(pages);
    DisplayModel* dm = tab ? tab->AsFixed() : nullptr;
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
    CommandPaletteOnAnnotationsChanged();
}

void StartLoadingAnnotationsForUi(WindowTab* tab) {
    DisplayModel* dm = tab ? tab->AsFixed() : nullptr;
    if (!dm) {
        return;
    }
    EngineBase* engine = dm->GetEngine();
    if (!EngineSupportsAnnotations(engine)) {
        return;
    }
    Vec<int> firstPages;
    CollectPriorityAnnotPages(tab, tab->selectedAnnotation, firstPages);
    for (int pageNo : firstPages) {
        EngineMupdfLoadAnnotsForPage(engine, pageNo);
    }
    EngineMupdfStartLoadAllAnnotations(engine, firstPages, MkFunc0(OnAnnotsProgress, tab));
}

void RefreshAnnotationLists(WindowTab* tab) {
    if (!tab || !tab->win) {
        return;
    }
    StartLoadingAnnotationsForUi(tab);
    RefreshAnnotFilterAnnotations(tab->win);
    CommandPaletteOnAnnotationsChanged();
}

void RefreshEditAnnotationsAfterEngineChange(WindowTab* tab) {
    if (!tab || !tab->win) {
        return;
    }
    StartLoadingAnnotationsForUi(tab);
    RefreshAnnotFilterAnnotations(tab->win);
}

// tests click the frame; the document model is canvas pixels
static Rect CanvasToFramePx(MainWindow* win, Rect r) {
    float s = CanvasScale(win);
    if (s <= 0.f) {
        s = 1.f;
    }
    r.x += (int)((float)win->canvasRc.x / s + 0.5f);
    r.y += (int)((float)win->canvasRc.y / s + 0.5f);
    return r;
}

// no color at all would serialize like black
static TempStr ColorDumpTemp(PdfColor c) {
    if (c == 0) {
        return fmt("none");
    }
    str::Builder out;
    SerializePdfColor(c, out);
    return ToStrTemp(out);
}

// selected annotation and loaded-annot count (issue-5933, issue-6023)
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
                   (int)(win->annotationResizeRerenderLeftMs != 0)));
    Annotation* annot = tab->selectedAnnotation;
    DisplayModel* dm = tab->AsFixed();
    if (annot && dm) {
        Rect annotRect = CanvasToFramePx(win, dm->CvtToScreen(annot->pageNo, GetRect(annot)));
        out.Append(fmt(" annotType=%d annotRect=%d,%d,%d,%d canResize=%d", (int)annot->type, annotRect.x, annotRect.y,
                       annotRect.dx, annotRect.dy, (int)AnnotationCanBeResized(annot->type)));
        // outline the pointer is dragging; empty unless that resize is in progress
        Rect outline;
        if (win->annotationBeingResized && win->annotationResizeOutlineOnly) {
            outline = CanvasToFramePx(win, dm->CvtToScreen(annot->pageNo, win->annotationResizePreviewRect));
        }
        out.Append(fmt(" resizeOutline=%d,%d,%d,%d", outline.x, outline.y, outline.dx, outline.dy));
        out.Append(fmt(" color=%s interiorColor=%s opacity=%d", ColorDumpTemp(GetColor(annot)),
                       ColorDumpTemp(InteriorColor(annot)), Opacity(annot)));
        out.Append(fmt(" contents=%s", Contents(annot)));
    }
    return finish({}, 0);
}

// --- the hover card of the annotation under the cursor (Edit PDF) -----------

// clang-format off
static SeqStrings gColors =
    "Transparent\0Aqua\0Black\0Blue\0Fuchsia\0Gray\0Green\0Lime\0Maroon\0Navy\0Olive\0Orange\0Purple\0Red\0Silver\0Teal\0White\0Yellow\0";

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

static TempStr FontDescriptionTemp(Str family, int style) {
    str::Builder s;
    s.Append(FontFamilyLabel(family));
    if (style & kFreeTextBold) {
        s.Append(fmt(" %s", Tr("Bold")));
    }
    if (style & kFreeTextItalic) {
        s.Append(fmt(" %s", Tr("Italic")));
    }
    if (style & kFreeTextUnderline) {
        s.Append(fmt(" %s", Tr("Underline")));
    }
    return ToStrTemp(s);
}

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

// ng: orig's card is a layered popup window (a VirtHost); here it is an
// element the canvas draws, so this keeps what the card shows and where
struct AnnotationHoverOverlay {
    MainWindow* win = nullptr;
    WindowTab* tab = nullptr;
    Annotation* annot = nullptr;
    bool visible = false;
    AnnotationHoverRows rows;
    // the card as gpui laid it out on the last frame
    gpui::Bounds measured{};
    Rect anchorRect;
    RectF annotBounds;
    bool isAbove = false;
    // text markup: the mouse position (page coordinates) when the card
    // appeared; the card is centered on it until it hides
    bool hasMouseAnchor = false;
    PointF mouseAnchor;
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
        rows.Add(StrL("contents"), Tr("Contents:"), ShortAnnotationContentsTemp(contents));
    }

    AnnotationType type = Type(annot);
    if (type == AnnotationType::FreeText) {
        int quadding = Quadding(annot);
        rows.Add(StrL("textAlignment"), Tr("Text Alignment:"), SeqStrByIndex(gQuaddingNames, quadding));

        rows.Add(StrL("textFont"), Tr("Text Font:"),
                 FontDescriptionTemp(FreeTextFontFamily(annot), FreeTextFontStyle(annot)));
        rows.Add(StrL("textSize"), Tr("Text Size:"), fmt("%d", DefaultAppearanceTextSize(annot)));
        rows.Add(StrL("textColor"), Tr("Text Color:"), AnnotationColorNameTemp(DefaultAppearanceTextColor(annot)));
    }

    if (type == AnnotationType::Line) {
        int start = 0;
        int end = 0;
        GetLineEndingStyles(annot, &start, &end);
        rows.Add(StrL("lineStart"), Tr("Line Start:"), SeqStrByIndex(gLineEndingStyles, start));
        rows.Add(StrL("lineEnd"), Tr("Line End:"), SeqStrByIndex(gLineEndingStyles, end));
    }

    Str icon = IconName(annot);
    if (AnnotationIconNames(annot) && icon) {
        rows.Add(StrL("icon"), Tr("Icon:"), ShortAnnotationHoverValueTemp(icon));
    }
    if (type == AnnotationType::FileAttachment) {
        Str attached = EmbeddedFileNameTemp(annot);
        if (attached) {
            rows.Add(StrL("attachedFile"), Tr("Attached File:"), ShortAnnotationHoverValueTemp(attached));
        }
    }
    if (AnnotationSupportsBorder(type)) {
        rows.Add(StrL("border"), Tr("Border:"), fmt("%d", BorderWidth(annot)));
    }
    if (AnnotationSupportsColor(type)) {
        Str label = AnnotationColorIsBackground(type) ? Tr("Background Color:") : Tr("Color:");
        rows.Add(StrL("color"), label, AnnotationColorNameTemp(GetColor(annot)));
    }
    if (AnnotationSupportsInteriorColor(type)) {
        rows.Add(StrL("interiorColor"), Tr("Interior Color:"), AnnotationColorNameTemp(InteriorColor(annot)));
    }
    if (AnnotationSupportsOpacity(type)) {
        rows.Add(StrL("opacity"), Tr("Opacity:"), fmt("%d", Opacity(annot)));
    }

    rows.Add(StrL("author"), Tr("Author:"), ShortAnnotationHoverValueTemp(Author(annot)));
    rows.Add(StrL("date"), Tr("Date:"), FormatPdfDateLocalTimeTemp(ModificationDate(annot)));
    int popupId = PopupId(annot);
    if (popupId >= 0) {
        rows.Add(StrL("popup"), Tr("Popup:"), fmt("%d 0 R", popupId));
    }
    if (gShowRect) {
        RectF rect = GetBounds(annot);
        rows.Add(StrL("rect"), Tr("Rect:"), fmt("%d-%d@%d-%d", (int)rect.dx, (int)rect.dy, (int)rect.x, (int)rect.y));
    }
}

static Color AnnotationHoverBg() {
    return ThemeNotificationsBackgroundColor();
}

static Color AnnotationHoverText() {
    return ThemeNotificationsTextColor();
}

static AnnotationHoverOverlay* GetOrCreateAnnotationHoverOverlay(MainWindow* win) {
    if (win->annotationHoverOverlay) {
        return win->annotationHoverOverlay;
    }
    auto* overlay = new AnnotationHoverOverlay();
    overlay->win = win;
    win->annotationHoverOverlay = overlay;
    return overlay;
}

static void BuildAnnotationHoverOverlay(AnnotationHoverOverlay* overlay, Annotation* annot) {
    overlay->rows.keys.Reset();
    overlay->rows.labels.Reset();
    overlay->rows.values.Reset();
    CollectAnnotationHoverRows(annot, overlay->rows);
    overlay->annot = annot;
    overlay->tab = overlay->win->CurrentTab();
    overlay->annotBounds = GetRect(annot);
    // the size is the previous card's until this one was painted
    overlay->measured = {};
}

static bool SameRectF(RectF a, RectF b) {
    return a.x == b.x && a.y == b.y && a.dx == b.dx && a.dy == b.dy;
}

void HideAnnotationHoverOverlay(MainWindow* win) {
    AnnotationHoverOverlay* overlay = win ? win->annotationHoverOverlay : nullptr;
    if (!overlay) {
        return;
    }
    if (overlay->visible) {
        AppShellInvalidate(win);
    }
    overlay->visible = false;
    overlay->annot = nullptr;
    overlay->tab = nullptr;
    overlay->anchorRect = {};
    overlay->hasMouseAnchor = false;
    overlay->annotBounds = {};
    overlay->rows.keys.Reset();
    overlay->rows.labels.Reset();
    overlay->rows.values.Reset();
}

// ng: `mousePos` is the cursor on the canvas; orig asks GetCursorPos
void UpdateAnnotationHoverOverlay(MainWindow* win, Point mousePos) {
    Annotation* annot = win ? win->annotationUnderCursor : nullptr;
    WindowTab* tab = win ? win->CurrentTab() : nullptr;
    if (!win || !win->pdfAnnotationsToolbarEnabled || win->mouseAction != MouseAction::None ||
        !AnnotationIsLive(annot) || (tab && annot == tab->selectedAnnotation)) {
        HideAnnotationHoverOverlay(win);
        return;
    }
    AnnotationHoverOverlay* overlay = GetOrCreateAnnotationHoverOverlay(win);
    RectF bounds = GetRect(annot);
    bool appearing = overlay->annot != annot || overlay->tab != win->CurrentTab() || !overlay->visible;
    if (appearing) {
        overlay->hasMouseAnchor = false;
        DisplayModel* dm = win->AsFixed();
        if (dm && AnnotationIsTextMarkup(annot->type)) {
            overlay->mouseAnchor = dm->CvtFromScreen(mousePos, PageNo(annot));
            overlay->hasMouseAnchor = true;
        }
    }
    bool rebuild = appearing || !SameRectF(bounds, overlay->annotBounds);
    if (rebuild) {
        BuildAnnotationHoverOverlay(overlay, annot);
    }
    if (rebuild || !overlay->visible) {
        overlay->visible = true;
        AppShellInvalidate(win);
    }
}

void RefreshAnnotationHoverOverlay(MainWindow* win) {
    AnnotationHoverOverlay* overlay = win ? win->annotationHoverOverlay : nullptr;
    if (!overlay || !overlay->visible || !AnnotationIsLive(overlay->annot)) {
        return;
    }
    BuildAnnotationHoverOverlay(overlay, overlay->annot);
    AppShellInvalidate(win);
}

void DeleteAnnotationHoverOverlay(MainWindow* win) {
    AnnotationHoverOverlay* overlay = win ? win->annotationHoverOverlay : nullptr;
    if (!overlay) {
        return;
    }
    win->annotationHoverOverlay = nullptr;
    delete overlay;
}

bool IsAnnotationHoverOverlayVisible(MainWindow* win) {
    AnnotationHoverOverlay* overlay = win ? win->annotationHoverOverlay : nullptr;
    return overlay && overlay->visible;
}

TempStr AnnotationHoverOverlayStateTemp(MainWindow* win) {
    AnnotationHoverOverlay* overlay = win ? win->annotationHoverOverlay : nullptr;
    if (!overlay || !overlay->visible) {
        return StrL("overlay visible=0\n");
    }
    gp::Bounds r = overlay->measured;
    Rect a = overlay->anchorRect;
    str::Builder out;
    out.Append(fmt("overlay visible=1 rows=%d above=%d rect=%d,%d,%d,%d anchor=%d,%d,%d,%d\n", len(overlay->rows.keys),
                   overlay->isAbove ? 1 : 0, (int)r.x, (int)r.y, (int)r.w, (int)r.h, a.x, a.y, a.dx, a.dy));
    for (int i = 0; i < len(overlay->rows.keys); i++) {
        out.Append(fmt("row %s=%s\n", overlay->rows.keys[i], overlay->rows.values[i]));
    }
    return ToStrTemp(out);
}

// orig's BuildAnnotationHoverOverlay layout and PositionAnnotationHoverOverlay.
// It is an informational card, not a new interaction surface: it has no
// listeners, so mouse input still reaches the annotation and canvas beneath it.
gp::El* AnnotationHoverOverlayBuild(MainWindow* win, gp::Ctx* cx) {
    AnnotationHoverOverlay* overlay = win ? win->annotationHoverOverlay : nullptr;
    if (!overlay || !overlay->visible) {
        return nullptr;
    }
    DisplayModel* dm = win->AsFixed();
    Annotation* annot = overlay->annot;
    WindowTab* tab = win->CurrentTab();
    // ng: orig hides the card from every place that ends a hover; the card is
    // rebuilt each frame here, so the conditions are checked in one place
    bool keep = dm && tab == overlay->tab && win->pdfAnnotationsToolbarEnabled &&
                win->mouseAction == MouseAction::None && AnnotationIsLive(annot) &&
                annot == win->annotationUnderCursor && annot != tab->selectedAnnotation && !IsPlacingAnnotation(win) &&
                dm->PageVisible(PageNo(annot));
    if (!keep) {
        HideAnnotationHoverOverlay(win);
        return nullptr;
    }

    float k = CanvasScale(win);
    Rect canvas = Rect(0, 0, (int)((float)win->canvasRc.dx / k), (int)((float)win->canvasRc.dy / k));
    Rect annotRect = dm->CvtToScreen(PageNo(annot), GetRect(annot));
    overlay->anchorRect = annotRect;
    if (canvas.Intersect(annotRect).IsEmpty()) {
        HideAnnotationHoverOverlay(win);
        return nullptr;
    }

    Color textColor = AnnotationHoverText();
    Color labelColor = ThemeWindowTextDisabledColor();
    gp::El* card = gp::Div(cx->a)
                       ->FlexCol()
                       ->Absolute()
                       ->PadX((float)DpiScale(10) * k)
                       ->PadY((float)DpiScale(8) * k)
                       ->Radius((float)DpiScale(6) * k)
                       ->Bg(ToGpui(AnnotationHoverBg()))
                       ->Border(1, ToGpui(ThemeEdgeColor()))
                       ->BoundsOut(&overlay->measured);
    Str title = AnnotationReadableNameTemp(Type(annot));
    card->Child(gp::TextEl(cx->a, GpuiDup(cx->a, title))->Font(12)->Bold()->Fg(ToGpui(textColor)));
    card->Child(gp::Div(cx->a)->H((float)DpiScale(5) * k));
    // orig's two-column Table: 12 px between the columns, 3 px between rows
    gp::El* labels = gp::Div(cx->a)->FlexCol()->Gap((float)DpiScale(3) * k);
    gp::El* values = gp::Div(cx->a)->FlexCol()->Gap((float)DpiScale(3) * k);
    for (int row = 0; row < len(overlay->rows.labels); row++) {
        labels->Child(gp::TextEl(cx->a, GpuiDup(cx->a, overlay->rows.labels[row]))->Font(12)->Fg(ToGpui(labelColor)));
        // ng: an empty text has no height, and the rows would go out of step
        Str value = overlay->rows.values[row];
        values->Child(
            gp::TextEl(cx->a, len(value) > 0 ? GpuiDup(cx->a, value) : GStrL(" "))->Font(12)->Fg(ToGpui(textColor)));
    }
    card->Child(gp::Div(cx->a)->FlexRow()->Gap((float)DpiScale(12) * k)->Child(labels)->Child(values));

    // ng: gpui measures while painting; until then the card is placed off the
    // canvas and another frame is asked for
    if (overlay->measured.w <= 0 || overlay->measured.h <= 0) {
        card->Left(-10000)->Top(-10000);
        AppShellInvalidate(win);
        return card;
    }

    int gap = DpiScale(6);
    int width = (int)(overlay->measured.w / k);
    int height = std::min((int)(overlay->measured.h / k), canvas.dy);
    int x = annotRect.x;
    if (overlay->hasMouseAnchor) {
        // a highlight can span many lines: center the card where the mouse entered it
        x = dm->CvtToScreen(PageNo(annot), overlay->mouseAnchor).x - (width / 2);
    }
    int y = annotRect.y + annotRect.dy + gap;
    overlay->isAbove = y + height > canvas.y + canvas.dy;
    if (overlay->isAbove) {
        y = annotRect.y - gap - height;
    }

    int maxX = canvas.x + canvas.dx - width;
    x = std::max(canvas.x, std::min(x, maxX));
    int maxY = canvas.y + canvas.dy - height;
    y = std::max(canvas.y, std::min(y, maxY));
    card->Left((float)x * k)->Top((float)y * k);
    return card;
}
