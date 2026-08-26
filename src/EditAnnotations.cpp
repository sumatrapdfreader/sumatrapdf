/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/File.h"
#include "base/Win.h"
#include "gui/Dpi.h"
#include "base/UITask.h"

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
#include "AppSettings.h"
#include "GlobalPrefs.h"
#include "DocController.h"
#include "Annotation.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "EngineMupdf.h"
#include "Translations.h"
#include "SumatraConfig.h"
#include "DisplayModel.h"
#include "MainWindow.h"
#include "Toolbar.h"
#include "WindowTab.h"
#include "FormFields.h"
#include "SumatraPDF.h"
#include "Commands.h"

#include "Theme.h"
#include "FilterHighlightDraw.h"
#include "AnnotEditToolbar.h"
#include "AnnotFilterToolbar.h"
#include "EditAnnotations.h"

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

// list of annotations where GetColor() returns background color
// TODO: probably incomplete;
static AnnotationType gAnnotsIsColorBackground[] = {
    AnnotationType::FreeText,
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

static bool IsAnnotationTypeInArray(AnnotationType* arr, int arrSize, AnnotationType toFind) {
    for (int i = 0; i < arrSize; i++) {
        if (toFind == arr[i]) {
            return true;
        }
    }
    return false;
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
    CancelFormFieldEditIfWidget(annot);
    for (MainWindow* win : gWindows) {
        if (win->annotationBeingDragged == annot) {
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
        HideAnnotFilterList(win);
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
        bool isBackground = IsAnnotationTypeInArray(gAnnotsIsColorBackground, dimofi(gAnnotsIsColorBackground), type);
        Str label = isBackground ? _TRA("Background Color:") : _TRA("Color:");
        rows.Add(StrL("color"), label, AnnotationColorNameTemp(GetColor(annot)));
    }
    if (AnnotationSupportsInteriorColor(type)) {
        rows.Add(StrL("interiorColor"), _TRA("Interior Color:"), AnnotationColorNameTemp(InteriorColor(annot)));
    }
    if (type == AnnotationType::Highlight) {
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
    if (pages.Contains(pageNo)) {
        return;
    }
    pages.Append(pageNo);
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
    pages.Reset();
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
    }
    return finish({}, 0);
}
