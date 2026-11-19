/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's AnnotTextPopup.cpp - the read-only card with an annotation's whole
// text, opened by clicking it outside Edit PDF mode. orig hosts a borderless
// multi-line edit in a layered popup window; here the card is an absolutely
// positioned gpui element in the canvas with the same header (author on the
// left, date on the right, a rule under them) and the same colors.

#include "gui/GpuiBridge.h"

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
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "AIChatTypes.h"
#include "WindowTab.h"
#include "gui/AppShell.h"
#include "gui/DocCanvas.h"
#include "AnnotTextPopup.h"
#include "AnnotEditToolbar.h"
#include "AppHelpersCommon.h"
#include "AnnotTextPopupCommon.h"
#include "Commands.h"
#include "Toolbar.h"

// from AnnotEditToolbarCommon.h, whose kMargin / kCornerRadius clash with this
// card's (AnnotTextPopupCommon.h)
void AnnotChanged(WindowTab* tab);
Color PdfToWinColor(PdfColor c);
u8 PdfColorAlpha(PdfColor c);

// a recolor swatch on a highlight's card
constexpr int kSwatchSize = 14;
// text size when PopupFontSize is unset, and the average char width it gives
constexpr int kDefaultTextPx = 13;
constexpr int kDefaultCharDx = 8;
// hover: a pause before the card shows, so sweeping across a page shows none,
// and a grace after leaving, so the pointer can reach the card's buttons
constexpr int kHoverShowMs = 400;
constexpr int kHoverHideMs = 300;
constexpr int kHoverRefreshMs = 100;
// the page lock can be busy re-rendering the edit, so the hit test misses
constexpr int kHoverRefreshTries = 4;

struct AnnotTextPopup {
    MainWindow* win = nullptr;
    WindowTab* tab = nullptr;
    Annotation* annot = nullptr;
    Str author; // owned
    Str date;   // owned
    Str text;   // owned
    gp::Bounds measured;
    // the header's buttons, as laid out last frame
    gp::Bounds editMeasured;
    gp::Bounds closeMeasured;
    // a highlight's color, read when the card is shown (reading it takes the
    // document lock), and its row of preset swatches as laid out last frame
    PdfColor color = 0;
    bool hasSwatches = false;
    Vec<gp::Bounds> swatchMeasured;
    // shown by hovering (hides on leave), not by a click (stays until closed)
    bool byHover = false;
    bool overCard = false;
    // the hovered annotation waiting out kHoverShowMs
    Annotation* pending = nullptr;
    int showMs = 0;
    int hideMs = 0;
    // closed while hovered: not shown again until the pointer leaves it
    Annotation* dismissed = nullptr;
    // hit-test the resting pointer again once the layout has settled
    Countdown refreshHover;
    int refreshTries = 0;

    ~AnnotTextPopup() {
        str::Free(author);
        str::Free(date);
        str::Free(text);
    }
};

bool IsAnnotationTextPopupShown(MainWindow* win) {
    AnnotTextPopup* p = win ? win->annotTextPopup : nullptr;
    return p && p->annot != nullptr;
}

bool IsAnnotationTextPopupShownFor(MainWindow* win, Annotation* annot) {
    AnnotTextPopup* p = win ? win->annotTextPopup : nullptr;
    return p && annot && p->annot == annot;
}

void HideAnnotationTextPopup(MainWindow* win) {
    AnnotTextPopup* p = win ? win->annotTextPopup : nullptr;
    if (!p || !p->annot) {
        return;
    }
    p->dismissed = p->annot;
    p->annot = nullptr;
    p->tab = nullptr;
    p->overCard = false;
    AppShellInvalidate(win);
}

void DeleteAnnotationTextPopup(MainWindow* win) {
    AnnotTextPopup* p = win ? win->annotTextPopup : nullptr;
    if (!p) {
        return;
    }
    win->annotTextPopup = nullptr;
    delete p;
}

bool ShowAnnotationTextPopup(MainWindow* win, Annotation* annot) {
    if (!win || !win->AsFixed() || !AnnotationHasText(annot)) {
        return false;
    }
    if (!win->annotTextPopup) {
        auto* p = new AnnotTextPopup();
        p->win = win;
        win->annotTextPopup = p;
    }
    AnnotTextPopup* p = win->annotTextPopup;
    p->annot = annot;
    p->tab = win->CurrentTab();
    p->byHover = false;
    // the last card's size would place this one
    p->measured = {};
    // who wrote the comment; the annotation's type name when it has no author, so
    // the header never comes up empty (orig's PopupAuthorTemp)
    Str author = Author(annot);
    if (len(author) == 0) {
        author = AnnotationReadableNameTemp(Type(annot));
    }
    str::ReplaceWithCopy(&p->author, author);
    str::ReplaceWithCopy(&p->date, FormatPdfDateLocalTimeTemp(ModificationDate(annot)));
    str::ReplaceWithCopy(&p->text, Contents(annot));
    // text markup can be recolored from the card
    p->hasSwatches = AnnotationIsTextMarkup(Type(annot));
    p->color = p->hasSwatches ? GetColor(annot) : 0;
    AppShellInvalidate(win);
    return true;
}

// Edit PDF: the selected annotation has its property row and comment editor,
// so no hover card goes on top of them. Flag checks only.
static bool IsEditPdfCardBlocked(MainWindow* win, Annotation* annot) {
    if (!win->pdfAnnotationsToolbarEnabled) {
        return false;
    }
    WindowTab* tab = win->CurrentTab();
    return (tab && annot == tab->selectedAnnotation) || IsEditingAnnotContents(win);
}

// ng: text markup shows its comment's card on hover, in Edit PDF too (where
// it gets no metadata card). Not in presentations or while a button is down.
// Cheap: the comment itself is checked by the caller.
bool IsCommentHoverTarget(MainWindow* win, Annotation* annot) {
    if (!win || !annot || win->mouseAction != MouseAction::None || IsEditPdfCardBlocked(win, annot)) {
        return false;
    }
    return !win->InPresentation() && AnnotationIsTextMarkup(Type(annot)) && AnnotationIsLive(annot);
}

bool IsAnnotationTextPopupFromHover(MainWindow* win) {
    return IsAnnotationTextPopupShown(win) && win->annotTextPopup->byHover;
}

// the comment the card shows, for tests
Str AnnotationTextPopupText(MainWindow* win) {
    return IsAnnotationTextPopupShown(win) ? win->annotTextPopup->text : Str{};
}

// tests: the card's and its buttons' bounds as laid out last frame, in dips
TempStr AnnotTextPopupBoundsTemp(MainWindow* win) {
    if (!IsAnnotationTextPopupShown(win)) {
        return StrL("card=0\n");
    }
    AnnotTextPopup* p = win->annotTextPopup;
    gp::Bounds c = p->measured;
    gp::Bounds e = p->editMeasured;
    gp::Bounds x = p->closeMeasured;
    str::Builder out;
    out.Append(fmt("card=%g,%g,%g,%g edit=%g,%g,%g,%g close=%g,%g,%g,%g swatches=%d", c.x, c.y, c.w, c.h, e.x, e.y, e.w,
                   e.h, x.x, x.y, x.w, x.h, len(p->swatchMeasured)));
    for (gp::Bounds b : p->swatchMeasured) {
        out.Append(fmt(" sw=%g,%g,%g,%g", b.x, b.y, b.w, b.h));
    }
    out.Append(StrL("\n"));
    return ToStrTemp(out);
}

// The comment or the layout under a resting pointer changed (Edit PDF off
// moves the page): look again after a frame instead of waiting for a move.
void AnnotTextPopupRefreshHover(MainWindow* win) {
    if (!win) {
        return;
    }
    if (!win->annotTextPopup) {
        win->annotTextPopup = new AnnotTextPopup();
        win->annotTextPopup->win = win;
    }
    win->annotTextPopup->refreshHover.Arm(kHoverRefreshMs);
    win->annotTextPopup->refreshTries = kHoverRefreshTries;
    // the card was closed for the edit, not by the user
    win->annotTextPopup->dismissed = nullptr;
}

// Shows the card of the comment under the cursor after a pause and hides it
// once the pointer has left both the highlight and the card. Driven by the
// shell's tick from annotationUnderCursor, which the canvas keeps current.
void AnnotTextPopupTick(MainWindow* win, int elapsedMs) {
    AnnotTextPopup* rp = win ? win->annotTextPopup : nullptr;
    if (rp && rp->refreshHover.Tick(elapsedMs)) {
        DocCanvasRefreshHover(win);
        bool found = win->annotationUnderCursor && win->annotUnderCursorHasComment;
        if (!found && --rp->refreshTries > 0) {
            rp->refreshHover.Arm(kHoverRefreshMs);
        }
    }
    Annotation* target = win ? win->annotationUnderCursor : nullptr;
    // the canvas knows from the page element; Contents would take the doc lock
    if (!target || !win->annotUnderCursorHasComment || !IsCommentHoverTarget(win, target)) {
        target = nullptr;
    }
    AnnotTextPopup* p = win ? win->annotTextPopup : nullptr;
    if (p && p->dismissed != target) {
        p->dismissed = nullptr;
    }
    if (!p && !target) {
        return;
    }
    if (target && !p) {
        p = new AnnotTextPopup();
        p->win = win;
        win->annotTextPopup = p;
    }
    bool shown = p->annot != nullptr;

    if (target) {
        p->hideMs = kHoverHideMs;
        if (p->annot == target || p->dismissed == target) {
            return;
        }
        // from one hovered comment to the next: no second pause
        if (p->pending != target) {
            p->pending = target;
            p->showMs = shown && p->byHover ? 0 : kHoverShowMs;
        } else {
            p->showMs -= elapsedMs;
        }
        if (p->showMs > 0) {
            return;
        }
        p->pending = nullptr;
        if (ShowAnnotationTextPopup(win, target)) {
            p->byHover = true;
        }
        return;
    }

    p->pending = nullptr;
    if (!shown || !p->byHover) {
        return;
    }
    // a drag, a presentation, the selection's editing UI or the metadata card
    // of another annotation hides it at once: never two cards
    bool blocked = win->mouseAction != MouseAction::None || win->InPresentation() ||
                   IsEditPdfCardBlocked(win, p->annot) || IsAnnotationHoverOverlayVisible(win);
    if (!blocked && p->overCard) {
        return;
    }
    p->hideMs -= elapsedMs;
    if (!blocked && p->hideMs > 0) {
        return;
    }
    HideAnnotationTextPopup(win);
}

struct AnnotTextPopupView {
    MainWindow* win = nullptr;

    static void OnClose(AnnotTextPopupView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnEdit(AnnotTextPopupView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnHover(AnnotTextPopupView* self, gp::Ctx* cx, const gp::HoverEvent* ev);
    static void OnSwatch(AnnotTextPopupView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t idx);
};

static gp::Entity<AnnotTextPopupView> gAnnotTextPopupView;

void AnnotTextPopupView::OnClose(AnnotTextPopupView* self, gp::Ctx* cx, const gp::ClickEvent*) {
    HideAnnotationTextPopup(self->win);
    gp::Notify(cx);
}

// the comment editor of Edit PDF, on this card's annotation
void AnnotTextPopupView::OnEdit(AnnotTextPopupView* self, gp::Ctx* cx, const gp::ClickEvent*) {
    AnnotTextPopup* p = self->win ? self->win->annotTextPopup : nullptr;
    Annotation* annot = p ? p->annot : nullptr;
    if (annot && IsCurrentTab(self->win, p->tab)) {
        AddComment(self->win, annot);
    }
    gp::Notify(cx);
}

// recolor the card's highlight with a preset: one undo step through the normal
// update path (modified state, lists, re-render). Its opacity stays.
void AnnotTextPopupView::OnSwatch(AnnotTextPopupView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t idx) {
    AnnotTextPopup* p = self->win ? self->win->annotTextPopup : nullptr;
    Annotation* annot = p ? p->annot : nullptr;
    if (!annot || !IsCurrentTab(self->win, p->tab) || !AnnotationIsLive(annot)) {
        return;
    }
    Vec<Color> colors;
    AnnotPresetColors(CmdCreateAnnotHighlight, colors);
    if (!VecIsValidIndex(colors, (int)idx)) {
        return;
    }
    u8 alpha = p->color == (PdfColor)kColorUnset || p->color == 0 ? 0xff : PdfColorAlpha(p->color);
    Color c = colors[(int)idx];
    PdfColor pdfCol = MkPdfColor(GetRed(c), GetGreen(c), GetBlue(c), alpha);
    EngineBase* engine = p->tab->GetEngine();
    EngineMupdfBeginOperation(engine, "Set color");
    SetColor(annot, pdfCol);
    EngineMupdfEndOperation(engine);
    p->color = pdfCol;
    AnnotChanged(p->tab);
    gp::Notify(cx);
}

// a hover card stays while the pointer is on it
void AnnotTextPopupView::OnHover(AnnotTextPopupView* self, gp::Ctx*, const gp::HoverEvent* ev) {
    AnnotTextPopup* p = self->win ? self->win->annotTextPopup : nullptr;
    if (p && p->annot) {
        p->overCard = ev->hovered;
    }
}

gp::El* AnnotTextPopupBuild(MainWindow* win, gp::Ctx* cx) {
    AnnotTextPopup* p = win ? win->annotTextPopup : nullptr;
    if (!p || !p->annot) {
        return nullptr;
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm || !IsCurrentTab(win, p->tab) || !AnnotationIsLive(p->annot) || !dm->PageVisible(PageNo(p->annot))) {
        HideAnnotationTextPopup(win);
        return nullptr;
    }
    if (!gAnnotTextPopupView.IsValid()) {
        gAnnotTextPopupView = gp::EntityNewState<AnnotTextPopupView>(cx->app);
    }
    auto* view = (AnnotTextPopupView*)gp::EntityGet(cx->app, gAnnotTextPopupView.id);
    view->win = win;

    Color bg = PopupBg();
    Color fg = PopupText();
    float margin = (float)DpiScale(kMargin);
    // Annotations.PopupFontSize sizes the text; the header stays a px smaller
    int textPx = GetAnnotPopupFontSize();
    textPx = textPx > 0 ? textPx : kDefaultTextPx;
    int headerPx = textPx - 1;
    int charDx = kDefaultCharDx * textPx / kDefaultTextPx;
    float maxW = (float)std::max(DpiScale(kMinWidth), std::min(win->canvasRc.dx - 32, kMaxLineChars * charDx));
    float maxH = (float)(win->canvasRc.dy * kMaxHeightPercent / 100);

    // The card shrinks to a short comment, but never below its header: author,
    // date and the buttons, so none overflows it. In a narrow canvas the
    // author truncates first, then the date; the buttons never give way.
    float headerGap = 12;
    gp::PaintCtx* paint = win->gpuiWin ? &win->gpuiWin->paint : nullptr;
    float authorDx = gp::MeasureText(paint, ToGpui(p->author), (float)headerPx, 0, false, gp::kFontWeightBold).w;
    float dateDx = len(p->date) > 0 ? gp::MeasureText(paint, ToGpui(p->date), (float)headerPx, 0).w + headerGap : 0;
    // until the first layout: a label's width plus button padding, an icon button
    float editDx = p->editMeasured.w;
    if (editDx <= 0) {
        editDx = gp::MeasureText(paint, ToGpui(Tr("Edit")), (float)headerPx, 0).w + 16;
    }
    float closeDx = p->closeMeasured.w > 0 ? p->closeMeasured.w : (float)(headerPx + 10);
    float buttonsDx = editDx + closeDx + 2 * headerGap;
    float chromeDx = 2 * margin + 2;
    float minW = chromeDx + std::min(authorDx, maxW) + dateDx + buttonsDx;
    // a highlight's swatch row is never cut either
    Vec<Color> swatches;
    if (p->hasSwatches) {
        AnnotPresetColors(CmdCreateAnnotHighlight, swatches);
    }
    float swatchD = (float)DpiScale(kSwatchSize);
    float swatchGap = 4;
    float swatchesDx = (float)len(swatches) * (swatchD + swatchGap);
    buttonsDx = std::max(buttonsDx, swatchesDx);
    minW = std::max(minW, chromeDx + swatchesDx);
    float roomW = (float)win->canvasRc.dx - 16;
    minW = std::max(std::min(minW, std::max(roomW, chromeDx + buttonsDx)), chromeDx + buttonsDx);
    maxW = std::max(maxW, minW);

    gp::El* card = gp::Div(cx->a)
                       ->FlexCol()
                       ->Gap(4)
                       ->Pad(margin)
                       ->MinW(minW)
                       ->MaxW(maxW)
                       ->MaxH(maxH)
                       ->Radius((float)DpiScale(kCornerRadius))
                       ->Bg(ToGpui(bg))
                       ->Border(1, ToGpui(ThemeEdgeColor()))
                       ->OnHover(gp::ListenTo(gAnnotTextPopupView, &AnnotTextPopupView::OnHover));
    // author on the left, date on the right, with a rule underneath
    gp::El* header = gp::Div(cx->a)->FlexRow()->ItemsCenter()->Gap(headerGap)->W(gp::kFill);
    Str author = p->author;
    header->Child(gp::TextEl(cx->a, GpuiDup(cx->a, author))
                      ->Font((float)headerPx)
                      ->Bold()
                      ->Fg(ToGpui(fg))
                      ->Flex1()
                      ->MinW(0)
                      ->Truncate());
    if (len(p->date) > 0) {
        header->Child(gp::TextEl(cx->a, GpuiDup(cx->a, p->date))
                          ->Font((float)headerPx)
                          ->Fg(ToGpui(PopupMutedText()))
                          ->Shrink(1)
                          ->MinW(0)
                          ->Truncate());
    }
    header->Child(gpc::Button::New(cx, GStrL("annot-popup-edit"))
                      ->Label(ToGpui(Tr("Edit")))
                      ->Ghost()
                      ->Compact()
                      ->WithSize(gp::UiSize::XSmall)
                      ->Tooltip(ToGpui(Tr("Edit comment")))
                      ->OnClick(gp::ListenTo(gAnnotTextPopupView, &AnnotTextPopupView::OnEdit))
                      ->IntoEl()
                      ->Shrink0()
                      ->BoundsOut(&p->editMeasured));
    header->Child(gpc::Button::New(cx, GStrL("annot-popup-close"))
                      ->Icon(gp::IconName::Close)
                      ->Ghost()
                      ->Compact()
                      ->WithSize(gp::UiSize::XSmall)
                      ->Tooltip(ToGpui(Tr("Close")))
                      ->OnClick(gp::ListenTo(gAnnotTextPopupView, &AnnotTextPopupView::OnClose))
                      ->IntoEl()
                      ->Shrink0()
                      ->BoundsOut(&p->closeMeasured));
    card->Child(header);
    card->Child(gp::Div(cx->a)->W(gp::kFill)->H(1)->MarginB((float)DpiScale(kRuleGap))->Bg(ToGpui(PopupRuleColor())));
    card->Child(gp::TextEl(cx->a, GpuiDup(cx->a, p->text))->Font((float)textPx)->Fg(ToGpui(fg))->Wrap()->W(gp::kFill));
    // the presets, the highlight's color ringed; sized before pointers go out
    VecReset(p->swatchMeasured);
    VecAppendBlanks(p->swatchMeasured, len(swatches));
    gp::El* swatchRow = gp::Div(cx->a)->FlexRow()->ItemsCenter()->Gap(swatchGap)->MarginT(4);
    Color current = PdfToWinColor(p->color);
    for (int i = 0; i < len(swatches); i++) {
        p->swatchMeasured[i] = {};
        Color c = swatches[i];
        bool on = p->color != 0 && p->color != (PdfColor)kColorUnset && (c & 0xffffff) == current;
        gp::El* sw = gp::Div(cx->a)
                         ->W(swatchD)
                         ->H(swatchD)
                         ->Shrink0()
                         ->Radius(swatchD / 2)
                         ->Bg(ToGpui(c & 0xffffff))
                         ->Border(on ? 2 : 1, ToGpui(on ? fg : ThemeEdgeColor()))
                         ->Cursor(gp::CursorKind::Pointer)
                         ->BoundsOut(&p->swatchMeasured[i])
                         ->PathClick(GpuiDup(cx->a, fmt("annot-popup-col-%d", i)))
                         ->OnClick(gp::ListenTo(gAnnotTextPopupView, &AnnotTextPopupView::OnSwatch, (intptr_t)i));
        swatchRow->Child(sw);
    }
    if (len(swatches) > 0) {
        card->Child(swatchRow);
    }
    card->BoundsOut(&p->measured);

    // ng: gpui measures while painting; until then the card is placed off the
    // canvas and another frame is asked for (as the hover overlay does)
    if (p->measured.w <= 0 || p->measured.h <= 0) {
        card->Absolute()->Left(-10000)->Top(-10000);
        AppShellInvalidate(win);
        return card;
    }

    // under the annotation, above it when there is no room
    float k = CanvasScale(win);
    Rect r = dm->CvtToScreen(PageNo(p->annot), GetRect(p->annot));
    float w = p->measured.w > 0 ? p->measured.w : maxW;
    float h = p->measured.h > 0 ? p->measured.h : 80;
    float gap = (float)DpiScale(6);
    float x = ((float)r.x + (float)r.dx / 2) * k - (w / 2);
    float y = (float)(r.y + r.dy) * k + gap;
    if (y + h > (float)win->canvasRc.dy) {
        y = (float)r.y * k - gap - h;
    }
    x = std::max(std::min(x, (float)win->canvasRc.dx - w), 0.f);
    y = std::max(std::min(y, (float)win->canvasRc.dy - h), 0.f);
    card->Absolute()->Left(x)->Top(y);
    return card;
}
