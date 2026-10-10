/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "../ext/mupdf/source/pdf/annotation-icons.h"
#include "base/File.h"
#include "base/UITask.h"
#include "gui/Dpi.h"
#include "gui/UIModels.h"
#include "gui/PlatformFont.h"
#include "Settings.h"
#include "Annotation.h"
#include "PdfDate.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "Theme.h"
#include "Translations.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Commands.h"
#include "Toolbar.h"
#include "AppSettings.h"
#include "FormFields.h"
#include "AnnotFilterToolbar.h"
#include "SvgIcons.h"
#include "CommandPalette.h"
#include "AnnotEditToolbar.h"
#include "AnnotEditToolbarCommon.h"

bool BarIsDark() {
    return !IsLightColor(ThemeWindowBackgroundColor());
}

Color BarBg() {
    if (BarIsDark()) {
        return ThemeWindowBackgroundColor();
    }
    Color contentBg;
    ThemePageRenderColors(contentBg);
    return AccentColor(contentBg, 12);
}

Color BarBorderColor() {
    if (BarIsDark()) {
        return AccentColor(ThemeWindowControlBackgroundColor(), 35);
    }
    return AccentColor(BarBg(), 8);
}

Color BarTextColor() {
    if (BarIsDark()) {
        return ThemeWindowTextColor();
    }
    return MkRgb(27, 29, 33);
}

Color BarMutedTextColor() {
    if (BarIsDark()) {
        return ThemeWindowTextDisabledColor();
    }
    return MkRgb(92, 96, 104);
}

Str KindName(AnnotEditKind kind) {
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

// annotation types whose GetColor() is a background, not the ink color
bool AnnotationColorIsBackground(AnnotationType tp) {
    return tp == AnnotationType::FreeText || tp == AnnotationType::Text;
}

// shapes whose color is their border's: the color chip's drop-down also sets
// the border's width, so they have no Border Width chip
bool AnnotationBorderInColorChip(AnnotationType tp) {
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
Str ResolvedAnnotIconName(Annotation* annot) {
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
SeqStrings gBase14ReadableNames = "Courier\0Helvetica\0TimesRoman\0";

int StyleBitForKind(AnnotEditKind kind) {
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

Str FontFamilyLabel(Str family) {
    int idx = SeqStrIndexIS(gBase14FontFamilies, family);
    if (idx >= 0) {
        return SeqStrByIndex(gBase14ReadableNames, idx);
    }
    return InternFontFamily(family);
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

void AppendStyleToggle(Vec<AnnotEditItem>& out, AnnotEditKind kind, int style, Str tooltip) {
    AnnotEditItem it;
    it.kind = kind;
    it.number = (style & StyleBitForKind(kind)) ? 1 : 0;
    it.tooltip = tooltip;
    VecAppend(out, it);
}

const char* MupdfIconStream(Str name) {
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

bool IsPdfPathOpChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '*';
}

void SkipPdfPathWs(const char*& p) {
    while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') {
        p++;
    }
}

// Build an fz_path from a pdf_write_icon_appearance glyph stream (m/l/c/re/h/cm/f).
float PdfPathPop(float* stk, int& top) {
    if (top <= 0) {
        return 0;
    }
    return stk[--top];
}

Color BarActiveBg() {
    if (BarIsDark()) {
        return AccentColor(ThemeWindowControlBackgroundColor(), 35);
    }
    return AccentColor(BarBg(), 22);
}

void AnnotChanged(WindowTab* tab) {
    if (!tab || !tab->win) {
        return;
    }
    NotifyAnnotationsChanged(tab);
    ToolbarUpdateStateForWindow(tab->win, false);
    MainWindowRerender(tab->win);
    UpdateAnnotEditToolbar(tab->win);
    UpdateAnnotFilterToolbar(tab->win);
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

// clang-format off
static SeqStrings gFileAttachmentUcons = "Graph\0Paperclip\0PushPin\0Tag\0";

static SeqStrings gSoundIcons = "Speaker\0Mic\0";

static SeqStrings gStampIcons =
    "Approved\0AsIs\0Confidential\0Departmental\0Draft\0Experimental\0Expired\0Final\0ForComment\0ForPublicRelease\0NotApproved\0NotForPublicRelease\0Sold\0TopSecret\0";

// those are in order of pdf_line_ending enum in annot.h
SeqStrings gLineEndingStyles =
    "None\0Square\0Circle\0Diamond\0OpenArrow\0ClosedArrow\0Butt\0ROpenArrow\0RClosedArrow\0Slash\0";

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

// Clear non-owning Annotation* before the old engine is destroyed.
void InvalidateEditAnnotationsOnEngineChange(WindowTab* tab) {
    CloseAnnotationUiForTab(tab);
}

void NotifyAnnotationsChanged(WindowTab* tab) {
    if (tab && tab->win) {
        UpdateAnnotFilterToolbar(tab->win);
    }
    CommandPaletteOnAnnotationsChanged();
}

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
void CollectAnnotationHoverRows(Annotation* annot, AnnotationHoverRows& rows) {
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

Color AnnotationHoverBg() {
    return ThemeNotificationsBackgroundColor();
}

Color AnnotationHoverText() {
    return ThemeNotificationsTextColor();
}

bool SameRectF(RectF a, RectF b) {
    return a.x == b.x && a.y == b.y && a.dx == b.dx && a.dy == b.dy;
}

void OnAnnotsProgress(WindowTab* tab) {
    if (!tab || !IsMainWindowValidAndNotClosing(tab->win)) {
        return;
    }
    RefreshAnnotFilterAnnotations(tab->win);
    CommandPaletteOnAnnotationsChanged();
}

// Dump selected-annotation and loaded-annot count for tests (issue-5933, issue-6023).
// no color at all would serialize like black
TempStr ColorDumpTemp(PdfColor c) {
    if (c == 0) {
        return fmt("none");
    }
    str::Builder out;
    SerializePdfColor(c, out);
    return ToStrTemp(out);
}

Color PdfToWinColor(PdfColor c) {
    u8 r;
    u8 g;
    u8 b;
    u8 a;
    UnpackPdfColor(c, r, g, b, a);
    return MkRgb(r, g, b);
}

u8 PdfColorAlpha(PdfColor c) {
    u8 r;
    u8 g;
    u8 b;
    u8 a;
    UnpackPdfColor(c, r, g, b, a);
    return a;
}

Color PdfToWinColorWithAlpha(PdfColor c) {
    u8 r;
    u8 g;
    u8 b;
    u8 a;
    UnpackPdfColor(c, r, g, b, a);
    return MkRgba(r, g, b, a);
}

PdfColor WinToPdfColor(Color c) {
    u8 r;
    u8 g;
    u8 b;
    u8 a;
    UnpackColor(c, r, g, b, a);
    // in a Color alpha 0 means opaque, in a PdfColor it means transparent
    return MkPdfColor(r, g, b, a == 0 ? 0xff : a);
}

// the same color, made fully opaque
PdfColor OpaquePdfColor(PdfColor c) {
    if (c == 0) {
        return 0;
    }
    u8 r;
    u8 g;
    u8 b;
    u8 a;
    UnpackPdfColor(c, r, g, b, a);
    return MkPdfColor(r, g, b, 0xff);
}

// a chip shows a color the way the page does: with the annotation's opacity
static PdfColor ColorWithOpacity(PdfColor c, Annotation* annot, bool withOpacity) {
    if (c == 0 || !withOpacity) {
        // no color at all; there is no opacity to show
        return c;
    }
    u8 r;
    u8 g;
    u8 b;
    u8 a;
    UnpackPdfColor(c, r, g, b, a);
    return MkPdfColor(r, g, b, (u8)Opacity(annot));
}

void CollectItems(Annotation* annot, Vec<AnnotEditItem>& out) {
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

void ScheduleShowSelectedAnnotationView(WindowTab* tab) {
    if (!tab || tab->pendingShowSelectedAnnotation) {
        return;
    }
    tab->pendingShowSelectedAnnotation = true;
    uitask::Post(MkFunc0(ShowSelectedAnnotationView, tab), "ShowSelectedAnnot");
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

// Pages the background loader should finish first: current page (toolbar page
// even when visibleRatio is still 0), every page overlapping the viewport, and
// a context-menu annotation's page.
void CollectPriorityAnnotPages(WindowTab* tab, Annotation* extra, Vec<int>& pages) {
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
