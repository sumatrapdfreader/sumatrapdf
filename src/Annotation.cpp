/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/File.h"
#include "base/Pixmap.h"
#include "base/ScopedWin.h"

extern "C" {
#include <mupdf/pdf.h>
}

#include "gui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineMupdf.h"
#include "AppSettings.h"
#include "Commands.h"
#include "Translations.h"
#include "Annotation.h"

// spot checks the definitions are the same
static_assert((int)AnnotationType::Link == (int)PDF_ANNOT_LINK);
static_assert((int)AnnotationType::ThreeD == (int)PDF_ANNOT_3D);
static_assert((int)AnnotationType::Sound == (int)PDF_ANNOT_SOUND);
static_assert((int)AnnotationType::Unknown == (int)PDF_ANNOT_UNKNOWN);

// clang-format off
static SeqStrings gAnnotationTextIcons = "Comment\0Help\0Insert\0Key\0NewParagraph\0Note\0Paragraph\0";
// clang-format on

SeqStrings AnnotationTextIcons() {
    return gAnnotationTextIcons;
}

// clang-format off
// indexed by AnnotationType; unlike AnnotationReadableNameTemp() these are not
// translated, so they can appear in the annotation filter's ":t=" syntax
static SeqStrings gAnnotationTypeNames =
    "Text\0Link\0FreeText\0Line\0Square\0Circle\0Polygon\0PolyLine\0Highlight\0Underline\0Squiggly\0"
    "StrikeOut\0Redact\0Stamp\0Caret\0Ink\0Popup\0FileAttachment\0Sound\0Movie\0RichMedia\0Widget\0"
    "Screen\0PrinterMark\0TrapNet\0Watermark\0ThreeD\0Projection\0";
// clang-format on

SeqStrings AnnotationTypeNames() {
    return gAnnotationTypeNames;
}

// Matches case- and space-insensitively, so "free text" and "freetext" both
// name AnnotationType::FreeText. Unknown if there is no such type.
AnnotationType AnnotationTypeFromName(Str name) {
    TempStr want = str::DupTemp(name);
    want.len -= str::RemoveCharsInPlace(want, StrL(" -_"));
    if (len(want) == 0) {
        return AnnotationType::Unknown;
    }
    int idx = 0;
    SeqStrings names = gAnnotationTypeNames;
    for (Str item = SeqStrFirst(names); len(item) > 0; item = SeqStrNext(item), idx++) {
        if (str::EqI(item, want)) {
            return (AnnotationType)idx;
        }
    }
    return AnnotationType::Unknown;
}

// Translate an English annotation type label for the UI.
// When menuKey is set (e.g. "&Highlight"), use that key — those strings are
// already translated for context menus. Otherwise use english (already in
// translations.txt for Circle/Line/…). _TRN at the call site marks bare names
// for future extraction. Strip '&' access-key markers for non-menu display.
static Str TranslateAnnotTypeNameTemp(Str english, Str menuKey = {}) {
    Str key = menuKey ? menuKey : english;
    Str tr = trans::GetTranslation(key);
    if (str::Contains(tr, StrL("&"))) {
        // StrL(""), not Str{}: ReplaceTemp rejects a null replacement and
        // returns an empty string, which blanked every name with an access key
        return str::ReplaceTemp(tr, StrL("&"), StrL(""));
    }
    return tr;
}

// Human-readable annotation type names for UI (list box, hover tip, menus).
// _TRN marks strings for extraction; TranslateAnnotTypeNameTemp localizes.
// Order matches AnnotationType / pdf_annot_type.
Str AnnotationReadableNameTemp(AnnotationType tp) {
    switch (tp) {
        case AnnotationType::Text:
            return TranslateAnnotTypeNameTemp(_TRN("Text"), StrL("&Text"));
        case AnnotationType::Link:
            return TranslateAnnotTypeNameTemp(_TRN("Link"));
        case AnnotationType::FreeText:
            return TranslateAnnotTypeNameTemp(_TRN("Free Text"), StrL("&Free Text"));
        case AnnotationType::Line:
            return TranslateAnnotTypeNameTemp(_TRN("Line"));
        case AnnotationType::Square:
            return TranslateAnnotTypeNameTemp(_TRN("Square"));
        case AnnotationType::Circle:
            return TranslateAnnotTypeNameTemp(_TRN("Circle"));
        case AnnotationType::Polygon:
            return TranslateAnnotTypeNameTemp(_TRN("Polygon"));
        case AnnotationType::PolyLine:
            return TranslateAnnotTypeNameTemp(_TRN("Polyline"));
        case AnnotationType::Highlight:
            return TranslateAnnotTypeNameTemp(_TRN("Highlight"), StrL("&Highlight"));
        case AnnotationType::Underline:
            return TranslateAnnotTypeNameTemp(_TRN("Underline"), StrL("&Underline"));
        case AnnotationType::Squiggly:
            return TranslateAnnotTypeNameTemp(_TRN("Squiggly"), StrL("S&quiggly"));
        case AnnotationType::StrikeOut:
            return TranslateAnnotTypeNameTemp(_TRN("Strike Out"), StrL("&Strike Out"));
        case AnnotationType::Redact:
            return TranslateAnnotTypeNameTemp(_TRN("Redact"));
        case AnnotationType::Stamp:
            return TranslateAnnotTypeNameTemp(_TRN("Stamp"), StrL("&Stamp"));
        case AnnotationType::Caret:
            return TranslateAnnotTypeNameTemp(_TRN("Caret"), StrL("&Caret"));
        case AnnotationType::Ink:
            return TranslateAnnotTypeNameTemp(_TRN("Ink"));
        case AnnotationType::Popup:
            return TranslateAnnotTypeNameTemp(_TRN("Popup"));
        case AnnotationType::FileAttachment:
            return TranslateAnnotTypeNameTemp(_TRN("File Attachment"));
        case AnnotationType::Sound:
            return TranslateAnnotTypeNameTemp(_TRN("Sound"));
        case AnnotationType::Movie:
            return TranslateAnnotTypeNameTemp(_TRN("Movie"));
        case AnnotationType::RichMedia:
            return TranslateAnnotTypeNameTemp(_TRN("RichMedia"));
        case AnnotationType::Widget:
            return TranslateAnnotTypeNameTemp(_TRN("Widget"));
        case AnnotationType::Screen:
            return TranslateAnnotTypeNameTemp(_TRN("Screen"));
        case AnnotationType::PrinterMark:
            return TranslateAnnotTypeNameTemp(_TRN("Printer Mark"));
        case AnnotationType::TrapNet:
            return TranslateAnnotTypeNameTemp(_TRN("Trap Net"));
        case AnnotationType::Watermark:
            return TranslateAnnotTypeNameTemp(_TRN("Watermark"));
        case AnnotationType::ThreeD:
            return TranslateAnnotTypeNameTemp(_TRN("3D"));
        case AnnotationType::Projection:
            return TranslateAnnotTypeNameTemp(_TRN("Projection"));
        case AnnotationType::Unknown:
        default:
            return TranslateAnnotTypeNameTemp(_TRN("Unknown"));
    }
}

// annot is still owned by EngineMupdf (markup or form widget list).
static bool IsAnnotationInEngine(EngineMupdf* e, Annotation* annot) {
    if (!e || !annot) {
        return false;
    }
    int pageNo = annot->pageNo;
    ScopedRecursiveMutex scope(&e->pagesLock);
    FzPageInfo* pageInfo = e->PageInfoByPageNo(pageNo);
    if (!pageInfo) {
        return false;
    }
    return VecContains(pageInfo->annotations, annot) || VecContains(pageInfo->widgets, annot);
}

// Safe to call MuPDF with annot->pdfannot.
// True if annot is non-null, has a live pdf_annot*, and is still listed in
// its EngineMupdf page (markup or form widget). Use before any MuPDF call.
bool AnnotationIsLive(Annotation* annot) {
    if (!annot || !annot->engine || !annot->pdfannot) {
        return false;
    }
    return IsAnnotationInEngine(annot->engine, annot);
}

AnnotationType Type(Annotation* annot) {
    if (!annot) {
        return AnnotationType::Unknown;
    }
    ReportIf((int)annot->type < 0);
    return annot->type;
}

int PageNo(Annotation* annot) {
    if (!annot) {
        return -1;
    }
    ReportIf(annot->pageNo < 1);
    return annot->pageNo;
}

RectF GetBounds(Annotation* annot) {
    if (!AnnotationIsLive(annot)) {
        return annot ? annot->bounds : RectF{};
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    auto* ctx = e->Ctx();
    ScopedRecursiveMutex cs(&e->docLock);
    fz_rect rc = {};

    fz_try(ctx) {
        rc = pdf_bound_annot(ctx, a);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        logf("GetBounds(): pdf_bound_annot() failed\n");
    }
    annot->bounds = ToRectF(rc);
    return annot->bounds;
}

RectF GetRect(Annotation* annot) {
    if (!annot) {
        return {};
    }
    return annot->bounds;
}

void SetRect(Annotation* annot, RectF r) {
    // Stale reference after delete/reload must not reach mupdf
    // (e.g. annotationBeingDragged).
    if (!AnnotationIsLive(annot)) {
        return;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    bool failed = false;
    // Vec lives outside fz_try: a longjmp would skip C++ destructors.
    Vec<fz_point> pts;
    Vec<int> strokeCounts;
    Vec<fz_quad> redactQuads;
    {
        auto* ctx = e->Ctx();
        ScopedRecursiveMutex cs(&e->docLock);
        float dx = r.x - annot->bounds.x;
        float dy = r.y - annot->bounds.y;
        fz_try(ctx) {
            if (annot->type == AnnotationType::Line) {
                // /L is the two endpoints, not a rect. Translate them so a
                // drag does not rewrite a top-right/bottom-left line as
                // top-left/bottom-right of the bounds.
                fz_point p1{};
                fz_point p2{};
                pdf_annot_line(ctx, a, &p1, &p2);
                p1.x += dx;
                p1.y += dy;
                p2.x += dx;
                p2.y += dy;
                pdf_set_annot_line(ctx, a, p1, p2);
            } else if (annot->type == AnnotationType::Polygon || annot->type == AnnotationType::PolyLine) {
                // /Rect is derived from Vertices; pdf_set_annot_rect rejects these.
                int n = pdf_annot_vertex_count(ctx, a);
                for (int i = 0; i < n; i++) {
                    fz_point p = pdf_annot_vertex(ctx, a, i);
                    p.x += dx;
                    p.y += dy;
                    VecAppend(pts, p);
                }
                if (n > 0) {
                    pdf_set_annot_vertices(ctx, a, n, pts.els);
                }
            } else if (annot->type == AnnotationType::Redact) {
                // Text-selection marks store coverage in QuadPoints. Translate
                // those on a move; a new rect (no quads) is an area mark.
                int n = pdf_annot_quad_point_count(ctx, a);
                if (n > 0) {
                    for (int i = 0; i < n; i++) {
                        fz_quad q = pdf_annot_quad_point(ctx, a, i);
                        q.ul.x += dx;
                        q.ul.y += dy;
                        q.ur.x += dx;
                        q.ur.y += dy;
                        q.ll.x += dx;
                        q.ll.y += dy;
                        q.lr.x += dx;
                        q.lr.y += dy;
                        VecAppend(redactQuads, q);
                    }
                    pdf_set_annot_quad_points(ctx, a, len(redactQuads), redactQuads.els);
                } else {
                    pdf_set_annot_rect(ctx, a, ToFzRect(r));
                }
            } else if (annot->type == AnnotationType::Ink) {
                // /Rect is derived from InkList; pdf_set_annot_rect rejects Ink.
                int nStrokes = pdf_annot_ink_list_count(ctx, a);
                for (int i = 0; i < nStrokes; i++) {
                    int nv = pdf_annot_ink_list_stroke_count(ctx, a, i);
                    VecAppend(strokeCounts, nv);
                    for (int k = 0; k < nv; k++) {
                        fz_point p = pdf_annot_ink_list_stroke_vertex(ctx, a, i, k);
                        p.x += dx;
                        p.y += dy;
                        VecAppend(pts, p);
                    }
                }
                if (nStrokes > 0) {
                    pdf_set_annot_ink_list(ctx, a, nStrokes, strokeCounts.els, pts.els);
                }
            } else {
                pdf_set_annot_rect(ctx, a, ToFzRect(r));
            }
            pdf_update_annot(ctx, a);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
            // can happen for non-moveable annotations
            failed = true;
            logf("SetRect(): pdf_set_annot_rect() or pdf_update_annot() failed\n");
        }
    }
    ReportIf(failed);
    if (failed) {
        return;
    }
    // pdf_update_annot can rewrite the rect (rubber stamps keep a 190x50
    // aspect). Cached bounds must match that, or resize handles and hit
    // testing cover empty space around the visible stamp (issue #5933).
    annot->bounds = GetBounds(annot);
    // must be called outside docLock to avoid deadlock with pagesLock
    MarkNotificationAsModified(e, annot);
}

static Str MupdfCStrDupTemp(const char* s) {
    if (!s) {
        return {};
    }
    return str::DupTemp(Str(s));
}

static Str MupdfCStrTemp(const char* s) {
    if (str::IsEmptyOrWhiteSpace(Str(s))) {
        return {};
    }
    return str::DupTemp(Str(s));
}

// AnnotEditToolbar.cpp
Str Author(Annotation* annot) {
    if (!AnnotationIsLive(annot)) {
        return {};
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    auto* ctx = e->Ctx();
    ScopedRecursiveMutex cs(&e->docLock);

    Str res;
    fz_try(ctx) {
        res = MupdfCStrTemp(pdf_annot_author(ctx, a));
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        res = {};
    }
    return res;
}

SeqStrings gQuaddingNames = "Left\0Center\0Right\0";

// name of a text alignment as used by the FreeTextAlignment setting and the
// `alignment` command argument -> PDF /Q value, -1 if not a known name
int QuaddingFromName(Str s) {
    return SeqStrIndexIS(gQuaddingNames, s);
}

int Quadding(Annotation* annot) {
    if (!AnnotationIsLive(annot)) {
        return 0;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    auto* ctx = e->Ctx();
    ScopedRecursiveMutex cs(&e->docLock);
    int res = 0;
    fz_try(ctx) {
        res = pdf_annot_quadding(ctx, a);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        logf("Quadding(): pdf_annot_quadding() failed\n");
    }
    return res;
}

static bool IsValidQuadding(int i) {
    return i >= 0 && i <= 2;
}

// return true if changed
bool SetQuadding(Annotation* annot, int newQuadding) {
    if (!AnnotationIsLive(annot)) {
        return false;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    {
        auto* ctx = e->Ctx();
        ScopedRecursiveMutex cs(&e->docLock);
        ReportIf(!IsValidQuadding(newQuadding));
        bool didChange = Quadding(annot) != newQuadding;
        if (!didChange) {
            return false;
        }
        fz_try(ctx) {
            pdf_set_annot_quadding(ctx, a, newQuadding);
            pdf_update_annot(ctx, a);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
            logf("SetQuadding(): pdf_set_annot_quadding or pdf_update_annot() failed\n");
        }
    }
    MarkNotificationAsModified(e, annot);
    return true;
}

void SetQuadPointsAsRect(Annotation* annot, const Vec<RectF>& rects) {
    if (!AnnotationIsLive(annot)) {
        return;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    {
        auto* ctx = e->Ctx();
        ScopedRecursiveMutex cs(&e->docLock);
        int n = len(rects);
        if (n == 0) {
            return;
        }
        fz_quad* quads = AllocArray<fz_quad>(n);
        if (!quads) {
            return;
        }
        defer {
            free(quads);
        };
        for (int i = 0; i < n; i++) {
            RectF rect = rects[i];
            fz_rect r = ToFzRect(rect);
            fz_quad q = fz_quad_from_rect(r);
            quads[i] = q;
        }
        fz_try(ctx) {
            pdf_clear_annot_quad_points(ctx, a);
            pdf_set_annot_quad_points(ctx, a, n, quads);
            pdf_update_annot(ctx, a);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
            logf("SetQuadPointsAsRect(): mupdf calls failed\n");
        }
    }
    MarkNotificationAsModified(e, annot);
}

Vec<RectF> GetQuadPointsAsRect(Annotation* annot) {
    Vec<RectF> res;
    if (!AnnotationIsLive(annot)) {
        return res;
    }
    EngineMupdf* e = annot->engine;
    auto* ctx = e->Ctx();
    ScopedRecursiveMutex cs(&e->docLock);
    fz_try(ctx) {
        int n = pdf_annot_quad_point_count(ctx, annot->pdfannot);
        for (int i = 0; i < n; i++) {
            fz_quad q = pdf_annot_quad_point(ctx, annot->pdfannot, i);
            VecAppend(res, ToRectF(fz_rect_from_quad(q)));
        }
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }
    return res;
}

// Regenerate appearance streams for the whole page after a form mutation.
// A field change can affect *other* widgets (radio-group siblings sharing the
// field value, or JS-calculated fields); each caches its resolved appearance
// until pdf_update_annot runs for it, so updating only the touched widget
// leaves siblings showing a stale appearance. Call inside the caller's fz_try,
// holding docLock.
static void UpdateFormFieldPage(fz_context* ctx, pdf_annot* a) {
    pdf_page* page = pdf_annot_page(ctx, a);
    if (page) {
        pdf_update_page(ctx, page);
    }
}

// PDF form (widget) fields. GetWidgetType returns a pdf_widget_type value
// (PDF_WIDGET_TYPE_*), or 0 (UNKNOWN) when annot isn't a form widget.
int GetWidgetType(Annotation* annot) {
    if (!AnnotationIsLive(annot) || annot->type != AnnotationType::Widget) {
        return PDF_WIDGET_TYPE_UNKNOWN;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    auto* ctx = e->Ctx();
    ScopedRecursiveMutex cs(&e->docLock);
    int wt = PDF_WIDGET_TYPE_UNKNOWN;
    fz_try(ctx) {
        wt = (int)pdf_widget_type(ctx, a);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }
    return wt;
}

WidgetCursorKind GetWidgetCursorKind(Annotation* annot) {
    if (!AnnotationIsLive(annot) || annot->type != AnnotationType::Widget) {
        return WidgetCursorKind::None;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    auto* ctx = e->Ctx();
    ScopedRecursiveMutex cs(&e->docLock);
    WidgetCursorKind kind = WidgetCursorKind::None;
    fz_try(ctx) {
        int flags = pdf_annot_field_flags(ctx, a);
        if (!(flags & PDF_FIELD_IS_READ_ONLY)) {
            int wt = pdf_widget_type(ctx, a);
            if (wt == PDF_WIDGET_TYPE_TEXT || wt == PDF_WIDGET_TYPE_COMBOBOX || wt == PDF_WIDGET_TYPE_LISTBOX) {
                kind = WidgetCursorKind::Text;
            } else if (wt == PDF_WIDGET_TYPE_CHECKBOX || wt == PDF_WIDGET_TYPE_RADIOBUTTON) {
                kind = WidgetCursorKind::Button;
            }
        }
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }
    return kind;
}

// Toggle a checkbox / radio-button form field in place. Returns true if it was
// a (non-read-only) checkbox/radio and got toggled.
bool ToggleFormButton(Annotation* annot) {
    if (!AnnotationIsLive(annot) || annot->type != AnnotationType::Widget) {
        return false;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    bool changed = false;
    {
        // BaseCtx(), not a Ctx() clone: toggling regenerates the appearance,
        // which runs the button's format/calculate JS; mupdf executes (and
        // rethrows errors) on _ctx, so the fz_try must be on that context.
        auto* ctx = e->BaseCtx();
        ScopedRecursiveMutex cs(&e->docLock);
        fz_try(ctx) {
            int wt = pdf_widget_type(ctx, a);
            int flags = pdf_annot_field_flags(ctx, a);
            bool readOnly = (flags & PDF_FIELD_IS_READ_ONLY) != 0;
            if (wt == PDF_WIDGET_TYPE_RADIOBUTTON && !readOnly) {
                // pdf_toggle_widget mishandles radio groups whose buttons have
                // distinct on-state names: it sets every sibling's /AS to the
                // selected state, leaving them all "on". Instead set the group's
                // value via pdf_set_field_value, which routes through
                // update_checkbox_selector and sets each kid's /AS correctly.
                pdf_obj* kid = pdf_annot_obj(ctx, a);
                pdf_obj* grp = kid;
                for (pdf_obj* p = pdf_dict_get(ctx, grp, PDF_NAME(Parent)); p;
                     p = pdf_dict_get(ctx, grp, PDF_NAME(Parent))) {
                    grp = p;
                }
                pdf_obj* curAS = pdf_dict_get(ctx, kid, PDF_NAME(AS));
                bool isOn = curAS && !pdf_name_eq(ctx, curAS, PDF_NAME(Off));
                bool noToggleOff = (flags & PDF_BTN_FIELD_IS_NO_TOGGLE_TO_OFF) != 0;
                Str onName = Str(pdf_to_name(ctx, pdf_button_field_on_state(ctx, kid)));
                pdf_set_field_value(ctx, e->pdfdoc, grp, CStrTemp((isOn && !noToggleOff) ? StrL("Off") : onName), 0);
                pdf_update_annot(ctx, a);
                UpdateFormFieldPage(ctx, a); // refresh all radio-group siblings
                changed = true;
            } else if (wt == PDF_WIDGET_TYPE_CHECKBOX && !readOnly) {
                pdf_toggle_widget(ctx, a);
                pdf_update_annot(ctx, a);
                UpdateFormFieldPage(ctx, a);
                changed = true;
            }
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
            logf("ToggleFormButton(): mupdf calls failed\n");
        }
    }
    if (changed) {
        // must be called outside docLock (it takes pagesLock then docLock/renderLock)
        MarkNotificationAsModified(e, annot);
    }
    return changed;
}

// pdf_annot_field_flags (PDF_FIELD_IS_*, PDF_TX_FIELD_IS_* bits), or 0.
int GetWidgetFieldFlags(Annotation* annot) {
    if (!AnnotationIsLive(annot) || annot->type != AnnotationType::Widget) {
        return 0;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    auto* ctx = e->Ctx();
    ScopedRecursiveMutex cs(&e->docLock);
    int flags = 0;
    fz_try(ctx) {
        flags = pdf_annot_field_flags(ctx, a);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }
    return flags;
}

// current text value of a form field (owned temp copy), or "" .
Str GetWidgetValue(Annotation* annot) {
    if (!AnnotationIsLive(annot) || annot->type != AnnotationType::Widget) {
        return {};
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    auto* ctx = e->Ctx();
    ScopedRecursiveMutex cs(&e->docLock);
    Str res;
    fz_try(ctx) {
        res = MupdfCStrTemp(pdf_annot_field_value(ctx, a));
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }
    return res;
}

// font size from the field's /DA (in PDF points), or 0 for auto-size.
float GetWidgetFontSize(Annotation* annot) {
    if (!AnnotationIsLive(annot) || annot->type != AnnotationType::Widget) {
        return 0;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    auto* ctx = e->Ctx();
    ScopedRecursiveMutex cs(&e->docLock);
    float size = 0;
    fz_try(ctx) {
        const char* fontZ = nullptr;
        int nColor = 0;
        float color[4] = {0};
        pdf_annot_default_appearance(ctx, a, &fontZ, &size, &nColor, color);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        size = 0;
    }
    return size;
}

// max length of a text field (chars), or 0 for unlimited.
int GetWidgetMaxLen(Annotation* annot) {
    if (!AnnotationIsLive(annot) || annot->type != AnnotationType::Widget) {
        return 0;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    auto* ctx = e->Ctx();
    ScopedRecursiveMutex cs(&e->docLock);
    int maxLen = 0;
    fz_try(ctx) {
        maxLen = pdf_text_widget_max_len(ctx, a);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        maxLen = 0;
    }
    return maxLen;
}

// set a text field's value (runs validation); returns true if accepted.
bool SetWidgetTextValue(Annotation* annot, Str value) {
    if (!AnnotationIsLive(annot) || annot->type != AnnotationType::Widget) {
        return false;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    bool ok = false;
    TempStr valueZ = str::DupTemp(value);
    {
        // BaseCtx(), not a Ctx() clone: regenerating the appearance runs the
        // field's format/calculate JS, which mupdf executes (and rethrows
        // errors) on _ctx -- the fz_try must be on that same context.
        auto* ctx = e->BaseCtx();
        ScopedRecursiveMutex cs(&e->docLock);
        fz_try(ctx) {
            ok = pdf_set_text_field_value(ctx, a, len(valueZ) == 0 ? "" : valueZ.s) != 0;
            pdf_update_annot(ctx, a);
            UpdateFormFieldPage(ctx, a); // refresh JS-calculated fields
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
            logf("SetWidgetTextValue(): mupdf calls failed\n");
        }
    }
    if (ok) {
        MarkNotificationAsModified(e, annot);
    }
    return ok;
}

// options of a combobox/listbox field (display strings), appended to `out`.
void GetWidgetChoiceOptions(Annotation* annot, StrVec& out) {
    if (!AnnotationIsLive(annot) || annot->type != AnnotationType::Widget) {
        return;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    auto* ctx = e->Ctx();
    ScopedRecursiveMutex cs(&e->docLock);
    fz_try(ctx) {
        int n = pdf_choice_widget_options(ctx, a, 0, nullptr);
        if (n > 0) {
            const char** opts = (const char**)fz_malloc(ctx, n * sizeof(char*));
            pdf_choice_widget_options(ctx, a, 0, opts);
            for (int i = 0; i < n; i++) {
                out.Append(Str(opts[i] ? opts[i] : ""));
            }
            fz_free(ctx, (void*)opts);
        }
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }
}

// set a choice field's value to one of its options; returns true if applied.
bool SetWidgetChoiceValue(Annotation* annot, Str value) {
    if (!AnnotationIsLive(annot) || annot->type != AnnotationType::Widget) {
        return false;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    bool ok = false;
    TempStr valueZ = str::DupTemp(value);
    {
        // BaseCtx(), not a Ctx() clone: regenerating the appearance runs the
        // field's format/calculate JS, which mupdf executes (and rethrows
        // errors) on _ctx -- the fz_try must be on that same context.
        auto* ctx = e->BaseCtx();
        ScopedRecursiveMutex cs(&e->docLock);
        fz_try(ctx) {
            pdf_set_choice_field_value(ctx, a, len(valueZ) == 0 ? "" : valueZ.s);
            pdf_update_annot(ctx, a);
            UpdateFormFieldPage(ctx, a); // refresh JS-calculated fields
            ok = true;
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
            logf("SetWidgetChoiceValue(): mupdf calls failed\n");
        }
    }
    if (ok) {
        MarkNotificationAsModified(e, annot);
    }
    return ok;
}

Str Contents(Annotation* annot) {
    if (!AnnotationIsLive(annot)) {
        return {};
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    auto* ctx = e->Ctx();
    ScopedRecursiveMutex cs(&e->docLock);
    Str res;
    fz_try(ctx) {
        res = MupdfCStrDupTemp(pdf_annot_contents(ctx, a));
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        res = {};
        logf("Contents(): pdf_annot_contents()\n");
    }
    return res;
}

bool SetContents(Annotation* annot, Str sv) {
    ReportIf(!annot);
    if (!AnnotationIsLive(annot)) {
        return false;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    Str currValue = Contents(annot);
    if (str::Eq(sv, currValue)) {
        return false;
    }
    TempStr valueZ = str::DupTemp(sv);
    {
        auto* ctx = e->Ctx();
        ScopedRecursiveMutex cs(&e->docLock);
        fz_try(ctx) {
            pdf_set_annot_contents(ctx, a, len(valueZ) == 0 ? "" : valueZ.s);
            pdf_update_annot(ctx, a);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
        }
    }
    MarkNotificationAsModified(e, annot);
    return true;
}

void DeleteAnnotation(Annotation* annot) {
    ReportIf(!annot);
    if (!annot) {
        return;
    }
    // Caller must have DetachAnnotationFromUI (or have no UI holders).
    EngineMupdf* e = annot->engine;
    if (!e) {
        delete annot;
        return;
    }
    auto* a = annot->pdfannot;
    if (!a) {
        // Already stripped from mupdf; drop from engine list if still present.
        if (IsAnnotationInEngine(e, annot)) {
            MarkNotificationAsModified(e, annot, AnnotationChange::Remove);
        }
        delete annot;
        return;
    }
    if (!IsAnnotationInEngine(e, annot)) {
        logf("DeleteAnnotation: annotation not found in engine, skipping\n");
        // Still free the wrapper so callers do not leak a detached Annotation*.
        annot->pdfannot = nullptr;
        delete annot;
        return;
    }
    bool failed = false;
    {
        auto* ctx = e->Ctx();
        ScopedRecursiveMutex cs(&e->docLock);
        pdf_page* page = nullptr;
        fz_try(ctx) {
            page = pdf_annot_page(ctx, a);
            pdf_delete_annot(ctx, page, a);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
            failed = true;
        }
    }
    if (failed) {
        logf("failed to delete annotation on page %d\n", annot->pageNo);
        return;
    }
    annot->pdfannot = nullptr;
    // Remove from pageInfo->annotations (must happen while annot is still valid).
    MarkNotificationAsModified(e, annot, AnnotationChange::Remove);
    delete annot;
}

// -1 if not exist
int PopupId(Annotation* annot) {
    if (!AnnotationIsLive(annot)) {
        return -1;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    auto* ctx = e->Ctx();
    ScopedRecursiveMutex cs(&e->docLock);
    pdf_obj* obj = nullptr;
    int res = -1;
    fz_try(ctx) {
        obj = pdf_dict_get(ctx, pdf_annot_obj(ctx, a), PDF_NAME(Popup));
        if (obj) {
            res = pdf_to_num(ctx, obj);
        }
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }
    return res;
}

/*
time_t CreationDate(Annotation* annot) {
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    auto ctx = e->Ctx();
    auto pdf = annot->pdf;
    ScopedRecursiveMutex cs(&e->docLock);
    int64_t res = 0;
    fz_try(ctx)
    {
        res = pdf_annot_creation_date(ctx, a);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }
    return res;
}
*/

time_t ModificationDate(Annotation* annot) {
    if (!AnnotationIsLive(annot)) {
        return 0;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    auto* ctx = e->Ctx();
    ScopedRecursiveMutex cs(&e->docLock);
    int64_t res = 0;
    fz_try(ctx) {
        res = pdf_annot_modification_date(ctx, a);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }
    return res;
}

// mupdf never touches /M on its own, so whoever changes an annotation has to
// stamp it. Used by paste, which is a brand new annotation whatever date the
// one it was copied from carried.
void SetModificationDateToNow(Annotation* annot) {
    if (!AnnotationIsLive(annot)) {
        return;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    auto* ctx = e->Ctx();
    ScopedRecursiveMutex cs(&e->docLock);
    fz_try(ctx) {
        pdf_set_annot_modification_date(ctx, a, time(nullptr));
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }
}

// return empty if no icon
Str IconName(Annotation* annot) {
    if (!AnnotationIsLive(annot)) {
        return {};
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    auto* ctx = e->Ctx();
    ScopedRecursiveMutex cs(&e->docLock);
    Str iconName;
    fz_try(ctx) {
        if (pdf_annot_has_icon_name(ctx, a)) {
            // can only call if pdf_annot_has_icon_name() returned true
            iconName = MupdfCStrDupTemp(pdf_annot_icon_name(ctx, a));
        }
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        iconName = {};
    }
    return iconName;
}

void SetIconName(Annotation* annot, Str iconName) {
    if (!AnnotationIsLive(annot)) {
        return;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    Str curr = IconName(annot);
    if (str::Eq(curr, iconName)) {
        return;
    }
    TempStr nameZ = str::DupTemp(iconName);
    {
        auto* ctx = e->Ctx();
        ScopedRecursiveMutex cs(&e->docLock);
        fz_try(ctx) {
            pdf_set_annot_icon_name(ctx, a, len(nameZ) == 0 ? "" : nameZ.s);
            pdf_update_annot(ctx, a);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
        }
    }
    MarkNotificationAsModified(e, annot);
}

static i64 FileTimeToUnixSeconds(FILETIME ft) {
    constexpr i64 kTicksFrom1601To1970 = 116444736000000000LL;
    ULARGE_INTEGER value;
    value.LowPart = ft.dwLowDateTime;
    value.HighPart = ft.dwHighDateTime;
    if (value.QuadPart < (u64)kTicksFrom1601To1970) {
        return -1;
    }
    return (i64)((value.QuadPart - kTicksFrom1601To1970) / 10000000);
}

static pdf_obj* FilespecDict(fz_context* ctx, pdf_annot* a) {
    pdf_obj* obj = pdf_annot_obj(ctx, a);
    if (!obj) {
        return nullptr;
    }
    pdf_obj* fs = pdf_dict_get(ctx, obj, PDF_NAME(FS));
    if (!fs || !pdf_is_dict(ctx, fs)) {
        return nullptr;
    }
    return fs;
}

bool HasEmbeddedFile(Annotation* annot) {
    if (!AnnotationIsLive(annot)) {
        return false;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    auto* ctx = e->Ctx();
    ScopedRecursiveMutex cs(&e->docLock);
    bool ok = false;
    fz_try(ctx) {
        pdf_obj* fs = FilespecDict(ctx, a);
        ok = fs && pdf_is_embedded_file(ctx, fs);
    }
    fz_catch(ctx) {
        fz_ignore_error(ctx);
    }
    return ok;
}

Str EmbeddedFileNameTemp(Annotation* annot) {
    if (!AnnotationIsLive(annot)) {
        return {};
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    auto* ctx = e->Ctx();
    ScopedRecursiveMutex cs(&e->docLock);
    Str name;
    fz_try(ctx) {
        pdf_obj* fs = FilespecDict(ctx, a);
        if (fs) {
            pdf_filespec_params params{};
            pdf_get_filespec_params(ctx, fs, &params);
            if (params.filename) {
                name = str::DupTemp(Str(params.filename));
            }
        }
    }
    fz_catch(ctx) {
        fz_ignore_error(ctx);
        name = {};
    }
    return name;
}

// caller owns the result (binary; may not be NUL-terminated)
Str LoadEmbeddedFile(Annotation* annot) {
    if (!AnnotationIsLive(annot)) {
        return {};
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    auto* ctx = e->Ctx();
    ScopedRecursiveMutex cs(&e->docLock);
    Str res;
    fz_try(ctx) {
        pdf_obj* fs = FilespecDict(ctx, a);
        if (fs && pdf_is_embedded_file(ctx, fs)) {
            fz_buffer* buf = pdf_load_embedded_file_contents(ctx, fs);
            if (buf) {
                res = str::Dup(Str((char*)buf->data, (int)buf->len));
                fz_drop_buffer(ctx, buf);
            }
        }
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        logf("LoadEmbeddedFile() failed\n");
    }
    return res;
}

bool SetEmbeddedFileFromPath(Annotation* annot, Str path) {
    if (!AnnotationIsLive(annot) || len(path) == 0 || !file::Exists(path)) {
        return false;
    }
    EngineMupdf* e = annot->engine;
    Str data = file::ReadFile(path);
    if (!data.s) {
        return false;
    }
    TempStr name = path::GetBaseNameTemp(path);
    TempStr mime = MimeTypeFromExtTemp(path::GetExtTemp(path));
    i64 modified = FileTimeToUnixSeconds(file::GetModificationTime(path));
    bool ok = false;
    {
        ScopedEngineOperation op(e, "Embed file attachment");
        auto* ctx = e->Ctx();
        ScopedRecursiveMutex cs(&e->docLock);
        pdf_obj* fs = nullptr;
        fz_buffer* buf = nullptr;
        fz_var(fs);
        fz_var(buf);
        fz_try(ctx) {
            buf = fz_new_buffer_from_copied_data(ctx, (const u8*)data.s, (size_t)data.len);
            fs = pdf_add_embedded_file(ctx, e->pdfdoc, CStrTemp(name), mime ? CStrTemp(mime) : nullptr, buf, modified,
                                       modified, 0);
            pdf_set_annot_filespec(ctx, annot->pdfannot, fs);
            pdf_update_annot(ctx, annot->pdfannot);
            ok = true;
        }
        fz_always(ctx) {
            fz_drop_buffer(ctx, buf);
            pdf_drop_obj(ctx, fs);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
            logf("SetEmbeddedFileFromPath() failed\n");
            ok = false;
        }
    }
    str::Free(data);
    if (ok) {
        MarkNotificationAsModified(e, annot);
    }
    return ok;
}

void SetLineEndStyles(Annotation* annot, int end) {
    if (!AnnotationIsLive(annot)) {
        return;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    {
        auto* ctx = e->Ctx();
        ScopedRecursiveMutex cs(&e->docLock);
        fz_try(ctx) {
            pdf_set_annot_line_end_style(ctx, a, (pdf_line_ending)end);
            pdf_update_annot(ctx, a);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
        }
    }
    MarkNotificationAsModified(e, annot);
}

void SetLineStartStyles(Annotation* annot, int start) {
    if (!AnnotationIsLive(annot)) {
        return;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    {
        auto* ctx = e->Ctx();
        ScopedRecursiveMutex cs(&e->docLock);
        fz_try(ctx) {
            pdf_set_annot_line_start_style(ctx, a, (pdf_line_ending)start);
            pdf_update_annot(ctx, a);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
        }
    }
    MarkNotificationAsModified(e, annot);
}

static void PdfColorToFloat(PdfColor c, float rgb[3]) {
    u8 r;
    u8 g;
    u8 b;
    u8 a;
    UnpackPdfColor(c, r, g, b, a);
    rgb[0] = (float)r / 255.0f;
    rgb[1] = (float)g / 255.0f;
    rgb[2] = (float)b / 255.0f;
}

static float GetOpacityFloat(PdfColor c) {
    u8 alpha = GetAlpha(c);
    return (float)alpha / 255.0f;
}

static PdfColor MkPdfColorFromFloat(float rf, float gf, float bf) {
    u8 r = (u8)(rf * 255.0f);
    u8 g = (u8)(gf * 255.0f);
    u8 b = (u8)(bf * 255.0f);
    return MkPdfColor(r, g, b, 0xff);
}

// n = 1 (grey), 3 (rgb) or 4 (cmyk).
static PdfColor PdfColorFromFloat(fz_context* ctx, int n, float color[4]) {
    if (n == 0) {
        return 0; // transparent
    }
    if (n == 1) {
        return MkPdfColorFromFloat(color[0], color[0], color[0]);
    }
    if (n == 3) {
        return MkPdfColorFromFloat(color[0], color[1], color[2]);
    }
    if (n == 4) {
        float rgb[4]{};
        fz_try(ctx) {
            fz_convert_color(ctx, fz_device_cmyk(ctx), color, fz_device_rgb(ctx), rgb, nullptr,
                             fz_default_color_params);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
        }
        return MkPdfColorFromFloat(rgb[0], rgb[1], rgb[2]);
    }
    ReportIf(true);
    return 0;
}

PdfColor GetColor(Annotation* annot) {
    if (!AnnotationIsLive(annot)) {
        return 0;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    auto* ctx = e->Ctx();
    ScopedRecursiveMutex cs(&e->docLock);
    float color[4]{};
    int n = -1;
    fz_try(ctx) {
        pdf_annot_color(ctx, a, &n, color);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        n = -1;
    }
    if (n == -1) {
        return 0;
    }
    PdfColor res = PdfColorFromFloat(ctx, n, color);
    return res;
}

// Highlight, Underline, StrikeOut and Squiggly: /C is the only thing drawn
static bool IsTextMarkupAnnot(AnnotationType tp) {
    switch (tp) {
        case AnnotationType::Highlight:
        case AnnotationType::Underline:
        case AnnotationType::StrikeOut:
        case AnnotationType::Squiggly:
            return true;
        default:
            return false;
    }
}

// return true if color changed
bool SetColor(Annotation* annot, PdfColor c) {
    if (!AnnotationIsLive(annot)) {
        return false;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    {
        auto* ctx = e->Ctx();
        ScopedRecursiveMutex cs(&e->docLock);
        bool didChange = false;
        float color[4]{};
        int n = -1;
        float oldOpacity = 0;
        fz_try(ctx) {
            pdf_annot_color(ctx, a, &n, color);
            oldOpacity = pdf_annot_opacity(ctx, a);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
            n = -1;
        }
        if (n == -1) {
            return false;
        }
        float newColor[3];
        PdfColorToFloat(c, newColor);
        float opacity = GetOpacityFloat(c);
        didChange = (n != 3);
        if (!didChange) {
            for (int i = 0; i < n; i++) {
                if (color[i] != newColor[i]) {
                    didChange = true;
                }
            }
        }
        if (opacity != oldOpacity) {
            didChange = true;
        }
        if (!didChange) {
            return false;
        }
        fz_try(ctx) {
            if (c == 0) {
                pdf_set_annot_color(ctx, a, 0, newColor);
                // For text markup /C is the only ink, so an empty one doesn't
                // make the annotation invisible: mupdf synthesizes Acrobat's
                // default yellow for Highlight and a black line for the rest
                // (issue #1994). Opacity 0 is what "transparent" has to mean.
                // Other types keep their opacity: a Square with a transparent
                // stroke still shows /IC, and a FreeText with a transparent
                // background still shows its text.
                if (IsTextMarkupAnnot(Type(annot))) {
                    pdf_set_annot_opacity(ctx, a, 0.f);
                }
            } else {
                pdf_set_annot_color(ctx, a, 3, newColor);
                if (oldOpacity != opacity) {
                    pdf_set_annot_opacity(ctx, a, opacity);
                }
            }
            pdf_update_annot(ctx, a);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
        }
    }
    MarkNotificationAsModified(e, annot);
    return true;
}

PdfColor InteriorColor(Annotation* annot) {
    if (!AnnotationIsLive(annot)) {
        return 0;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    auto* ctx = e->Ctx();
    ScopedRecursiveMutex cs(&e->docLock);
    float color[4]{};
    int n = -1;
    fz_try(ctx) {
        pdf_annot_interior_color(ctx, a, &n, color);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        n = -1;
    }
    if (n == -1) {
        return 0;
    }
    PdfColor res = PdfColorFromFloat(ctx, n, color);
    return res;
}

bool SetInteriorColor(Annotation* annot, PdfColor c) {
    if (!AnnotationIsLive(annot)) {
        return false;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    {
        auto* ctx = e->Ctx();
        ScopedRecursiveMutex cs(&e->docLock);
        bool didChange = false;
        float color[4]{};
        int n = -1;
        fz_try(ctx) {
            pdf_annot_interior_color(ctx, a, &n, color);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
            n = -1;
        }
        float newColor[3]{};
        PdfColorToFloat(c, newColor);
        int newN = (c == 0) ? 0 : 3;
        didChange = (n != newN);
        if (!didChange) {
            for (int i = 0; i < n; i++) {
                if (color[i] != newColor[i]) {
                    didChange = true;
                }
            }
        }
        if (!didChange) {
            return false;
        }
        fz_try(ctx) {
            pdf_set_annot_interior_color(ctx, a, newN, newColor);
            pdf_update_annot(ctx, a);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
        }
    }
    MarkNotificationAsModified(e, annot);
    return true;
}

Str DefaultAppearanceTextFont(Annotation* annot) {
    if (!AnnotationIsLive(annot)) {
        return {};
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    auto* ctx = e->Ctx();
    ScopedRecursiveMutex cs(&e->docLock);
    const char* fontNameZ = nullptr;
    float sizeF{0.0};
    int n = 0;
    float textColor[4]{};
    fz_try(ctx) {
        pdf_annot_default_appearance(ctx, a, &fontNameZ, &sizeF, &n, textColor);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }
    return MupdfCStrDupTemp(fontNameZ);
}

void SetDefaultAppearanceTextFont(Annotation* annot, Str sv) {
    if (!AnnotationIsLive(annot)) {
        return;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    TempStr fontZ = str::DupTemp(sv);
    {
        auto* ctx = e->Ctx();
        ScopedRecursiveMutex cs(&e->docLock);
        const char* fontNameZ = nullptr;
        float sizeF{0.0};
        int n = 0;
        float textColor[4]{};
        fz_try(ctx) {
            pdf_annot_default_appearance(ctx, a, &fontNameZ, &sizeF, &n, textColor);
            pdf_set_annot_default_appearance(ctx, a, len(fontZ) == 0 ? "" : fontZ.s, sizeF, n, textColor);
            pdf_update_annot(ctx, a);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
        }
    }
    MarkNotificationAsModified(e, annot);
}

int DefaultAppearanceTextSize(Annotation* annot) {
    if (!AnnotationIsLive(annot)) {
        return 0;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    auto* ctx = e->Ctx();
    ScopedRecursiveMutex cs(&e->docLock);
    const char* fontNameZ = nullptr;
    float sizeF{0.0};
    int n = 0;
    float textColor[4]{};
    fz_try(ctx) {
        pdf_annot_default_appearance(ctx, a, &fontNameZ, &sizeF, &n, textColor);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }
    return (int)sizeF;
}

void SetDefaultAppearanceTextSize(Annotation* annot, int textSize) {
    if (!AnnotationIsLive(annot)) {
        return;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    {
        auto* ctx = e->Ctx();
        ScopedRecursiveMutex cs(&e->docLock);
        const char* fontNameZ = nullptr;
        float sizeF{0.0};
        int n = 0;
        float textColor[4]{};
        fz_try(ctx) {
            pdf_annot_default_appearance(ctx, a, &fontNameZ, &sizeF, &n, textColor);
            pdf_set_annot_default_appearance(ctx, a, fontNameZ, (float)textSize, n, textColor);
            pdf_update_annot(ctx, a);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
        }
    }
    MarkNotificationAsModified(e, annot);
}

PdfColor DefaultAppearanceTextColor(Annotation* annot) {
    if (!AnnotationIsLive(annot)) {
        return 0;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    auto* ctx = e->Ctx();
    ScopedRecursiveMutex cs(&e->docLock);
    const char* fontNameZ = nullptr;
    float sizeF{0.0};
    int n = 0;
    float textColor[4]{};
    fz_try(ctx) {
        pdf_annot_default_appearance(ctx, a, &fontNameZ, &sizeF, &n, textColor);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }
    PdfColor res = PdfColorFromFloat(ctx, n, textColor);
    return res;
}

void SetDefaultAppearanceTextColor(Annotation* annot, PdfColor col) {
    if (!AnnotationIsLive(annot)) {
        return;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    {
        auto* ctx = e->Ctx();
        ScopedRecursiveMutex cs(&e->docLock);
        const char* fontNameZ = nullptr;
        float sizeF{0.0};
        int n = 0;
        float textColor[4]{}; // must be at least 4
        fz_try(ctx) {
            pdf_annot_default_appearance(ctx, a, &fontNameZ, &sizeF, &n, textColor);
            PdfColorToFloat(col, textColor);
            pdf_set_annot_default_appearance(ctx, a, fontNameZ, sizeF, 3, textColor);
            pdf_update_annot(ctx, a);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
        }
    }
    MarkNotificationAsModified(e, annot);
}

void GetLineEndingStyles(Annotation* annot, int* start, int* end) {
    if (start) {
        *start = 0;
    }
    if (end) {
        *end = 0;
    }
    if (!AnnotationIsLive(annot)) {
        return;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    auto* ctx = e->Ctx();
    ScopedRecursiveMutex cs(&e->docLock);
    pdf_line_ending leStart = PDF_ANNOT_LE_NONE;
    pdf_line_ending leEnd = PDF_ANNOT_LE_NONE;
    fz_try(ctx) {
        pdf_annot_line_ending_styles(ctx, a, &leStart, &leEnd);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        logf("GetLineEndingStyles: pdf_annot_line_ending_styles() failed\n");
    }
    if (start) {
        *start = (int)leStart;
    }
    if (end) {
        *end = (int)leEnd;
    }
}

// The /L endpoints of a Line annotation, in page coordinates.
bool GetLinePoints(Annotation* annot, PointF& start, PointF& end) {
    start = {};
    end = {};
    if (!AnnotationIsLive(annot) || annot->type != AnnotationType::Line) {
        return false;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    auto* ctx = e->Ctx();
    ScopedRecursiveMutex cs(&e->docLock);
    fz_point aPt{};
    fz_point bPt{};
    bool ok = false;
    fz_try(ctx) {
        pdf_annot_line(ctx, a, &aPt, &bPt);
        ok = true;
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        logf("GetLinePoints: pdf_annot_line() failed\n");
    }
    if (!ok) {
        return false;
    }
    start = {aPt.x, aPt.y};
    end = {bPt.x, bPt.y};
    return true;
}

// The two endpoints of a Line annotation (/L). Bounds follow from that.
void SetLinePoints(Annotation* annot, PointF start, PointF end) {
    if (!AnnotationIsLive(annot) || annot->type != AnnotationType::Line) {
        return;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    bool failed = false;
    {
        auto* ctx = e->Ctx();
        ScopedRecursiveMutex cs(&e->docLock);
        fz_try(ctx) {
            pdf_set_annot_line(ctx, a, fz_point{start.x, start.y}, fz_point{end.x, end.y});
            pdf_update_annot(ctx, a);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
            failed = true;
            logf("SetLinePoints: pdf_set_annot_line() failed\n");
        }
    }
    if (failed) {
        return;
    }
    annot->bounds = GetBounds(annot);
    MarkNotificationAsModified(e, annot);
}

// /Vertices of a PolyLine or Polygon, in page coordinates.
Vec<PointF> GetVertices(Annotation* annot) {
    Vec<PointF> res;
    if (!AnnotationIsLive(annot)) {
        return res;
    }
    if (annot->type != AnnotationType::PolyLine && annot->type != AnnotationType::Polygon) {
        return res;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    auto* ctx = e->Ctx();
    ScopedRecursiveMutex cs(&e->docLock);
    fz_try(ctx) {
        int n = pdf_annot_vertex_count(ctx, a);
        for (int i = 0; i < n; i++) {
            fz_point p = pdf_annot_vertex(ctx, a, i);
            VecAppend(res, {p.x, p.y});
        }
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        logf("GetVertices: pdf_annot_vertex() failed\n");
        VecReset(res);
    }
    return res;
}

// Replace /Vertices. Bounds follow from that.
void SetVertices(Annotation* annot, const Vec<PointF>& points) {
    if (!AnnotationIsLive(annot)) {
        return;
    }
    if (annot->type != AnnotationType::PolyLine && annot->type != AnnotationType::Polygon) {
        return;
    }
    if (len(points) < 2) {
        return;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    Vec<fz_point> pts;
    for (int i = 0; i < len(points); i++) {
        VecAppend(pts, {points[i].x, points[i].y});
    }
    bool failed = false;
    {
        auto* ctx = e->Ctx();
        ScopedRecursiveMutex cs(&e->docLock);
        fz_try(ctx) {
            pdf_set_annot_vertices(ctx, a, len(pts), pts.els);
            pdf_update_annot(ctx, a);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
            failed = true;
            logf("SetVertices: pdf_set_annot_vertices() failed\n");
        }
    }
    if (failed) {
        return;
    }
    annot->bounds = GetBounds(annot);
    MarkNotificationAsModified(e, annot);
}

void GetInkList(Annotation* annot, Vec<int>& strokeCounts, Vec<PointF>& points) {
    VecReset(strokeCounts);
    VecReset(points);
    if (!AnnotationIsLive(annot) || annot->type != AnnotationType::Ink) {
        return;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    auto* ctx = e->Ctx();
    ScopedRecursiveMutex cs(&e->docLock);
    fz_try(ctx) {
        int nStrokes = pdf_annot_ink_list_count(ctx, a);
        for (int i = 0; i < nStrokes; i++) {
            int nv = pdf_annot_ink_list_stroke_count(ctx, a, i);
            VecAppend(strokeCounts, nv);
            for (int k = 0; k < nv; k++) {
                fz_point p = pdf_annot_ink_list_stroke_vertex(ctx, a, i, k);
                VecAppend(points, {p.x, p.y});
            }
        }
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        logf("GetInkList: pdf_annot_ink_list() failed\n");
        VecReset(strokeCounts);
        VecReset(points);
    }
}

static float PointSegmentDistSq(PointF p, PointF a, PointF b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float lengthSq = dx * dx + dy * dy;
    float t = 0.f;
    if (lengthSq > 0.f) {
        t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / lengthSq;
        if (t < 0.f) {
            t = 0.f;
        } else if (t > 1.f) {
            t = 1.f;
        }
    }
    float px = a.x + t * dx;
    float py = a.y + t * dy;
    dx = p.x - px;
    dy = p.y - py;
    return dx * dx + dy * dy;
}

static bool InkStrokeHit(const Vec<PointF>& points, int start, int count, PointF pt, float radiusSq) {
    if (count == 1) {
        return PointSegmentDistSq(pt, points[start], points[start]) <= radiusSq;
    }
    for (int i = start + 1; i < start + count; i++) {
        if (PointSegmentDistSq(pt, points[i - 1], points[i]) <= radiusSq) {
            return true;
        }
    }
    return false;
}

bool EraseInkStrokes(Vec<int>& strokeCounts, Vec<PointF>& points, PointF pt, float radius) {
    int pointCount = 0;
    for (int count : strokeCounts) {
        if (count < 0) {
            return false;
        }
        pointCount += count;
    }
    if (pointCount != len(points)) {
        ReportIf(true);
        return false;
    }

    bool erased = false;
    float radiusSq = radius * radius;
    int end = len(points);
    for (int i = len(strokeCounts) - 1; i >= 0; i--) {
        int count = strokeCounts[i];
        int start = end - count;
        if (count > 0 && InkStrokeHit(points, start, count, pt, radiusSq)) {
            VecRemoveAtN(points, start, count);
            VecRemoveAt(strokeCounts, i);
            erased = true;
        }
        end = start;
    }
    return erased;
}

InkEraseResult EraseAnnotationInk(Annotation* annot, PointF pt, float radius) {
    if (!AnnotationIsLive(annot) || annot->type != AnnotationType::Ink) {
        return InkEraseResult::None;
    }

    Vec<int> strokeCounts;
    Vec<PointF> points;
    GetInkList(annot, strokeCounts, points);
    radius += (float)BorderWidth(annot) / 2.f;
    if (!EraseInkStrokes(strokeCounts, points, pt, radius)) {
        return InkEraseResult::None;
    }
    if (len(strokeCounts) == 0) {
        return InkEraseResult::Empty;
    }

    Vec<fz_point> pts;
    VecReserve(pts, len(points));
    for (PointF p : points) {
        VecAppend(pts, {p.x, p.y});
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    bool failed = false;
    {
        auto* ctx = e->Ctx();
        ScopedRecursiveMutex cs(&e->docLock);
        fz_try(ctx) {
            pdf_set_annot_ink_list(ctx, a, len(strokeCounts), strokeCounts.els, pts.els);
            pdf_update_annot(ctx, a);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
            failed = true;
            logf("EraseAnnotationInk: pdf_set_annot_ink_list() failed\n");
        }
    }
    if (failed) {
        return InkEraseResult::None;
    }
    annot->bounds = GetBounds(annot);
    MarkNotificationAsModified(e, annot);
    return InkEraseResult::Changed;
}

int BorderWidth(Annotation* annot) {
    if (!AnnotationIsLive(annot)) {
        return 0;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    auto* ctx = e->Ctx();
    ScopedRecursiveMutex cs(&e->docLock);
    float res = 0;
    fz_try(ctx) {
        res = pdf_annot_border(ctx, a);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        logf("BorderWidth: pdf_annot_border() failed\n");
    }

    return (int)res;
}

void SetBorderWidth(Annotation* annot, int newWidth) {
    ReportIf(!annot);
    if (!AnnotationIsLive(annot)) {
        return;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    {
        auto* ctx = e->Ctx();
        ScopedRecursiveMutex cs(&e->docLock);
        fz_try(ctx) {
            pdf_set_annot_border_width(ctx, a, (float)newWidth);
            pdf_update_annot(ctx, a);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
            logf("SetBorderWidth: SetBorderWidth() or pdf_update_annot() failed\n");
        }
    }
    MarkNotificationAsModified(e, annot);
}

int Opacity(Annotation* annot) {
    if (!AnnotationIsLive(annot)) {
        return 0;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    auto* ctx = e->Ctx();
    ScopedRecursiveMutex cs(&e->docLock);
    float fopacity = 0;
    fz_try(ctx) {
        fopacity = pdf_annot_opacity(ctx, a);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        logf("Opacity: pdf_annot_opacity() failed\n");
    }
    int res = (int)(fopacity * 255.f);
    return res;
}

void SetOpacity(Annotation* annot, int newOpacity) {
    if (!AnnotationIsLive(annot)) {
        return;
    }
    EngineMupdf* e = annot->engine;
    auto* a = annot->pdfannot;
    {
        auto* ctx = e->Ctx();
        ScopedRecursiveMutex cs(&e->docLock);
        ReportIf(newOpacity < 0);
        ReportIf(newOpacity > 255);
        newOpacity = setMinMax(newOpacity, 0, 255);
        float fopacity = (float)newOpacity / 255.f;

        fz_try(ctx) {
            pdf_set_annot_opacity(ctx, a, fopacity);
            pdf_update_annot(ctx, a);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
            logf("SetOpacity: pdf_set_annot_opacity() or pdf_update_annot() failed\n");
        }
    }
    MarkNotificationAsModified(e, annot);
}

static Str GetUserTemp() {
    Str u = Str(getenv("USER"));
    if (len(u) == 0) {
        u = Str(getenv("USERNAME"));
    }
    if (len(u) == 0) {
        return StrL("user");
    }
    return u;
}

static TempStr GetAnnotationTextIconTemp() {
    TempStr s = str::DupTemp(gSettings->annotations.textIconType);
    // this way user can use "new paragraph" and we'll match "NewParagraph"
    str::RemoveCharsInPlace(s, StrL(" "));
    int idx = SeqStrIndexIS(gAnnotationTextIcons, s);
    if (idx < 0) {
        return StrL("Note");
    }
    return SeqStrByIndex(gAnnotationTextIcons, idx);
}

static AnnotationType supportsInteriorColor[] = {
    AnnotationType::Circle,  AnnotationType::Line,   AnnotationType::PolyLine,
    AnnotationType::Polygon, AnnotationType::Square,
};

// The annotations whose position is theirs to change. /Rect is required on
// every annotation, but for some it is derived from other geometry; this is
// mupdf's rect_subtypes in pdf-annot.c (what pdf_annot_has_rect() answers yes
// to, and what pdf_set_annot_rect() accepts) plus Line (endpoints),
// Polygon/PolyLine (Vertices) and Ink (InkList), whose geometry SetRect()
// translates by hand.
static AnnotationType moveableAnnotations[] = {
    AnnotationType::Text,           AnnotationType::FreeText,  AnnotationType::Square, AnnotationType::Circle,
    AnnotationType::Redact,         AnnotationType::Stamp,     AnnotationType::Caret,  AnnotationType::Popup,
    AnnotationType::FileAttachment, AnnotationType::Sound,     AnnotationType::Movie,  AnnotationType::Widget,
    AnnotationType::ThreeD,         AnnotationType::RichMedia, AnnotationType::Line,   AnnotationType::Polygon,
    AnnotationType::PolyLine,       AnnotationType::Ink,
};

static AnnotationType supportsBorder[] = {
    AnnotationType::FreeText, AnnotationType::Ink,     AnnotationType::Line,     AnnotationType::Square,
    AnnotationType::Circle,   AnnotationType::Polygon, AnnotationType::PolyLine,
};

// /CA is a markup-annotation property, and only a markup annotation's
// appearance stream is generated with it (pdf_write_opacity). Mirrors mupdf's
// markup_subtypes minus the ones we can't create or edit.
static AnnotationType supportsOpacity[] = {
    AnnotationType::Text,      AnnotationType::FreeText, AnnotationType::Line,      AnnotationType::Square,
    AnnotationType::Circle,    AnnotationType::Polygon,  AnnotationType::PolyLine,  AnnotationType::Highlight,
    AnnotationType::Underline, AnnotationType::Squiggly, AnnotationType::StrikeOut, AnnotationType::Redact,
    AnnotationType::Stamp,     AnnotationType::Caret,    AnnotationType::Ink,       AnnotationType::FileAttachment,
};

static AnnotationType supportsColor[] = {
    AnnotationType::Stamp,     AnnotationType::Text,      AnnotationType::FileAttachment,
    AnnotationType::Sound,     AnnotationType::Caret,     AnnotationType::FreeText,
    AnnotationType::Ink,       AnnotationType::Line,      AnnotationType::Square,
    AnnotationType::Circle,    AnnotationType::Polygon,   AnnotationType::PolyLine,
    AnnotationType::Highlight, AnnotationType::Underline, AnnotationType::StrikeOut,
    AnnotationType::Squiggly,
};

static bool IsAnnotationInList(AnnotationType tp, AnnotationType* allowed, int nAllowed) {
    if (!allowed) {
        return true;
    }
    for (int i = 0; i < nAllowed; i++) {
        AnnotationType tp2 = allowed[i];
        if (tp2 == tp) {
            return true;
        }
    }
    return false;
}

bool AnnotationCanBeMoved(AnnotationType tp) {
    return IsAnnotationInList(tp, moveableAnnotations, dimofi(moveableAnnotations));
}

// Paste puts the copy wherever the mouse is, so copying only makes sense for
// annotations that can be moved. That rules out text markup (highlight,
// underline, squiggly, strike-out): those are anchored to the text they cover
// and a copy dropped elsewhere would mark unrelated text.
// Of the moveable types we also skip Popup, Sound, Movie and Widget, which we
// can't recreate from a snapshot, and FileAttachment, whose embedded file
// stream we don't copy (the paste would be a paperclip with no file).
bool AnnotationCanBeCopied(AnnotationType tp) {
    switch (tp) {
        case AnnotationType::Text:
        case AnnotationType::FreeText:
        case AnnotationType::Line:
        case AnnotationType::Square:
        case AnnotationType::Circle:
        case AnnotationType::Polygon:
        case AnnotationType::PolyLine:
        case AnnotationType::Redact:
        case AnnotationType::Stamp:
        case AnnotationType::Caret:
        case AnnotationType::Ink:
            return true;
        default:
            return false;
    }
}

bool AnnotationCanBeResized(AnnotationType tp) {
    // MuPDF regenerates these as fixed-size icon/caret appearances. Offering
    // resize handles only shifts the appearance within the requested rect.
    if (tp == AnnotationType::Text || tp == AnnotationType::Caret || tp == AnnotationType::FileAttachment ||
        tp == AnnotationType::Sound) {
        return false;
    }
    if (tp == AnnotationType::Ink) {
        // geometry is an ink path; stretch-to-rect is not implemented
        return false;
    }
    return AnnotationCanBeMoved(tp);
}

bool AnnotationSupportsInteriorColor(AnnotationType tp) {
    return IsAnnotationInList(tp, supportsInteriorColor, dimofi(supportsInteriorColor));
}

bool AnnotationSupportsBorder(AnnotationType tp) {
    return IsAnnotationInList(tp, supportsBorder, dimofi(supportsBorder));
}

bool AnnotationSupportsColor(AnnotationType tp) {
    return IsAnnotationInList(tp, supportsColor, dimofi(supportsColor));
}

bool AnnotationSupportsOpacity(AnnotationType tp) {
    return IsAnnotationInList(tp, supportsOpacity, dimofi(supportsOpacity));
}

Annotation* EngineMupdfCreateAnnotation(EngineBase* engine, int pageNo, PointF pos, AnnotCreateArgs* args) {
    static const float black[3] = {0, 0, 0};

    EngineMupdf* epdf = AsEngineMupdf(engine);
    fz_context* ctx = epdf->Ctx();
    // creating an annotation is a create plus a handful of property changes;
    // Undo should take all of it back in one go
    ScopedEngineOperation op(engine, "Add annotation");

    auto* pageInfo = epdf->GetFzPageInfo(pageNo, true);
    if (!pageInfo || !pageInfo->page) {
        return nullptr;
    }
    pdf_annot* annot = nullptr;
    pdf_page* page = nullptr;
    auto typ = args->annotType;
    auto col = args->col;
    auto bgCol = args->bgCol;
    auto interiorCol = args->interiorCol;
    // Keep the Vec outside fz_try: a MuPDF longjmp would skip its destructor.
    Vec<fz_point> polyLinePoints;
    if (args->polyLinePoints) {
        for (PointF point : *args->polyLinePoints) {
            VecAppend(polyLinePoints, {point.x, point.y});
        }
    }
    Vec<int> inkStrokeCounts;
    Vec<fz_point> inkPoints;
    bool hasInkList = args->inkStrokeCounts && args->inkPoints && len(*args->inkStrokeCounts) > 0;
    int nInkPoints = 0;
    if (hasInkList) {
        for (int count : *args->inkStrokeCounts) {
            if (count <= 0) {
                hasInkList = false;
                break;
            }
            VecAppend(inkStrokeCounts, count);
            nInkPoints += count;
        }
        hasInkList = hasInkList && nInkPoints == len(*args->inkPoints);
        if (hasInkList) {
            for (PointF point : *args->inkPoints) {
                VecAppend(inkPoints, {point.x, point.y});
            }
        }
    }
    {
        ScopedRecursiveMutex cs(&epdf->docLock);

        // pdf_create_annot returns a kept ref; the page list holds another.
        // On failure we must drop our keep and not fall through to the success
        // pdf_drop_annot (that would free the annot while still linked on the
        // page → UAF on the next render; crash reports show stamp create then
        // ACCESS_VIOLATION with float 30.0f as a pointer — stamp "DRAFT" uses h=30).
        fz_try(ctx) {
            page = pdf_page_from_fz_page(ctx, pageInfo->page);
            enum pdf_annot_type atyp = (enum pdf_annot_type)typ;

            annot = pdf_create_annot(ctx, page, atyp);

            pdf_set_annot_modification_date(ctx, annot, time(nullptr));
            if (pdf_annot_has_author(ctx, annot)) {
                Str defAuthor = gSettings->annotations.defaultAuthor;
                // if "(none)" we don't set it
                if (!str::Eq(defAuthor, StrL("(none)"))) {
                    Str author = GetUserTemp();
                    if (!str::IsEmptyOrWhiteSpace(defAuthor)) {
                        author = defAuthor;
                    }
                    pdf_set_annot_author(ctx, annot, CStrTemp(author));
                }
            }

            switch (typ) {
                case AnnotationType::Link:
                case AnnotationType::Popup:
                case AnnotationType::Unknown:
                case AnnotationType::Sound:
                case AnnotationType::Movie:
                case AnnotationType::RichMedia:
                case AnnotationType::Widget:
                case AnnotationType::Screen:
                case AnnotationType::PrinterMark:
                case AnnotationType::Watermark:
                case AnnotationType::TrapNet:
                case AnnotationType::ThreeD:
                case AnnotationType::Projection:
                    // do nothing
                    break;

                case AnnotationType::Highlight:
                case AnnotationType::Underline:
                case AnnotationType::Squiggly:
                case AnnotationType::StrikeOut: {
                    const char* content = CStrTemp(args->content);
                    if (!str::IsEmptyOrWhiteSpace(Str(content))) {
                        pdf_set_annot_contents(ctx, annot, content);
                    }
                } break;
                case AnnotationType::Text:
                case AnnotationType::FreeText:
                case AnnotationType::Stamp:
                case AnnotationType::FileAttachment: {
                    if (args->hasRect) {
                        // free text placed from the toolbar: the rect is the
                        // preview box the user just positioned
                        pdf_set_annot_rect(ctx, annot, ToFzRect(args->rect));
                        break;
                    }
                    fz_rect trect = pdf_annot_rect(ctx, annot);
                    float dx = trect.x1 - trect.x0;
                    trect.x0 = pos.x;
                    trect.x1 = trect.x0 + dx;
                    float dy = trect.y1 - trect.y0;
                    trect.y0 = pos.y;
                    trect.y1 = trect.y0 + dy;
                    pdf_set_annot_rect(ctx, annot, trect);
                } break;
                case AnnotationType::Square:
                case AnnotationType::Circle: {
                    if (args->hasRect) {
                        pdf_set_annot_rect(ctx, annot, ToFzRect(args->rect));
                    } else {
                        fz_rect trect = pdf_annot_rect(ctx, annot);
                        float dx = trect.x1 - trect.x0;
                        trect.x0 = pos.x;
                        trect.x1 = trect.x0 + dx;
                        float dy = trect.y1 - trect.y0;
                        trect.y0 = pos.y;
                        trect.y1 = trect.y0 + dy;
                        pdf_set_annot_rect(ctx, annot, trect);
                    }
                } break;
                case AnnotationType::Caret: {
                    // MuPDF draws the caret glyph centered in the rect, so anchor
                    // middle-left at the click point instead of top-left.
                    fz_rect trect = pdf_annot_rect(ctx, annot);
                    float dx = trect.x1 - trect.x0;
                    float dy = trect.y1 - trect.y0;
                    trect.x0 = pos.x;
                    trect.x1 = trect.x0 + dx;
                    trect.y0 = pos.y - (dy / 2);
                    trect.y1 = trect.y0 + dy;
                    pdf_set_annot_rect(ctx, annot, trect);
                } break;
                case AnnotationType::Line: {
                    fz_point a{pos.x, pos.y};
                    fz_point b = args->hasLineEnd ? fz_point{args->lineEnd.x, args->lineEnd.y}
                                                  : fz_point{pos.x + 100, pos.y + 50};
                    pdf_set_annot_line(ctx, annot, a, b);
                } break;
                case AnnotationType::Polygon:
                case AnnotationType::PolyLine: {
                    if (len(polyLinePoints) >= 2) {
                        pdf_set_annot_vertices(ctx, annot, len(polyLinePoints), polyLinePoints.els);
                    } else if (typ == AnnotationType::Polygon) {
                        fz_point points[] = {
                            {pos.x + 50, pos.y},
                            {pos.x + 100, pos.y + 50},
                            {pos.x + 50, pos.y + 90},
                            {pos.x, pos.y + 50},
                        };
                        pdf_set_annot_vertices(ctx, annot, dimof(points), points);
                    } else {
                        fz_point points[] = {
                            {pos.x, pos.y + 70},
                            {pos.x + 30, pos.y},
                            {pos.x + 65, pos.y + 55},
                            {pos.x + 100, pos.y + 10},
                        };
                        pdf_set_annot_vertices(ctx, annot, dimof(points), points);
                    }
                } break;
                case AnnotationType::Ink: {
                    if (hasInkList) {
                        pdf_set_annot_ink_list(ctx, annot, len(inkStrokeCounts), inkStrokeCounts.els, inkPoints.els);
                    } else {
                        fz_point points[] = {
                            {pos.x, pos.y + 30},      {pos.x + 15, pos.y + 5},  {pos.x + 30, pos.y + 45},
                            {pos.x + 48, pos.y + 10}, {pos.x + 65, pos.y + 40}, {pos.x + 85, pos.y + 15},
                        };
                        int count = dimof(points);
                        pdf_set_annot_ink_list(ctx, annot, 1, &count, points);
                    }
                } break;
                case AnnotationType::Redact: {
                    if (args->hasRect) {
                        pdf_set_annot_rect(ctx, annot, ToFzRect(args->rect));
                    } else {
                        fz_rect rect{pos.x, pos.y, pos.x + 100, pos.y + 50};
                        pdf_set_annot_rect(ctx, annot, rect);
                    }
                } break;
            }
            if (typ == AnnotationType::Stamp && args->stampImage) {
                // image stamp (e.g. pasted from the clipboard or Insert Image):
                // embed the image and size the rect to the image's natural size,
                // anchored at pos. PDF has no DeviceBGR, so convert BGR/BGRA to RGB.
                Pixmap* stamp = args->stampImage;
                fz_image* img = nullptr;
                fz_pixmap* pix = nullptr;
                fz_pixmap* rgbPix = nullptr;
                fz_var(img);
                fz_var(pix);
                fz_var(rgbPix);
                fz_try(ctx) {
                    bool isRgb = stamp->format == PixmapFormat::RGBA8;
                    int alpha = stamp->format == PixmapFormat::BGR8 ? 0 : 1;
                    fz_colorspace* srcCs = isRgb ? fz_device_rgb(ctx) : fz_device_bgr(ctx);
                    pix = fz_new_pixmap_with_data(ctx, srcCs, stamp->width, stamp->height, nullptr, alpha,
                                                  stamp->stride, stamp->data);
                    pix->xres = (int)stamp->xres;
                    pix->yres = (int)stamp->yres;
                    if (!isRgb) {
                        rgbPix = fz_convert_pixmap(ctx, pix, fz_device_rgb(ctx), nullptr, nullptr,
                                                   fz_default_color_params, 1);
                        fz_drop_pixmap(ctx, pix);
                        pix = rgbPix;
                        rgbPix = nullptr;
                    }
                    img = fz_new_image_from_pixmap(ctx, pix, nullptr);
                    pdf_set_annot_stamp_image(ctx, annot, img);
                    int xres = img->xres > 0 ? img->xres : 96;
                    int yres = img->yres > 0 ? img->yres : 96;
                    float wPt = (float)img->w * 72.0f / (float)xres;
                    float hPt = (float)img->h * 72.0f / (float)yres;
                    fz_rect r = {pos.x, pos.y, pos.x + wPt, pos.y + hPt};
                    pdf_set_annot_rect(ctx, annot, r);
                }
                fz_always(ctx) {
                    fz_drop_image(ctx, img);
                    fz_drop_pixmap(ctx, rgbPix);
                    fz_drop_pixmap(ctx, pix);
                }
                fz_catch(ctx) {
                    fz_rethrow(ctx);
                }
            }
            if (typ == AnnotationType::FreeText) {
                if (args->borderWidth >= 0) {
                    pdf_set_annot_border_width(ctx, annot, (float)args->borderWidth);
                }
                // left is MuPDF's default; leave /Q out of the file for it
                if (args->quadding > kQuaddingLeft) {
                    pdf_set_annot_quadding(ctx, annot, args->quadding);
                }
                const char* content = CStrTemp(args->content);
                if (!str::IsEmptyOrWhiteSpace(Str(content))) {
                    pdf_set_annot_contents(ctx, annot, content);
                } else {
                    pdf_set_annot_contents(ctx, annot, kDefaultFreeTextContent);
                }
                int fontSize = args->textSize;
                if (fontSize <= 0) {
                    fontSize = 12;
                }
                int nCol = 3;
                const float* fcol = black;
                float textColor[3]{};

                if (col.parsedOk) {
                    PdfColorToFloat(col.pdfCol, textColor);
                    fcol = textColor;
                }
                pdf_set_annot_default_appearance(ctx, annot, "Helv", (float)fontSize, nCol, fcol);
                if (bgCol.parsedOk) {
                    float bgColor[3]{};
                    PdfColorToFloat(bgCol.pdfCol, bgColor);
                    // PdfColor 0 is "no color"; asking for 3 components would
                    // give the annotation a black background instead
                    int nBgCol = (bgCol.pdfCol == 0) ? 0 : 3;
                    pdf_set_annot_color(ctx, annot, nBgCol, bgColor);
                }
                // 100 is fuly opaque, the default
                if (args->opacity < 100) {
                    float fop = (float)args->opacity / 100.0f;
                    pdf_set_annot_opacity(ctx, annot, fop);
                }
            }

            if (typ == AnnotationType::Ink) {
                // the highlighter brush is an ink stroke as wide and as
                // translucent as a marker
                if (args->borderWidth >= 0) {
                    pdf_set_annot_border_width(ctx, annot, (float)args->borderWidth);
                }
                if (args->opacity < 100) {
                    pdf_set_annot_opacity(ctx, annot, (float)args->opacity / 100.0f);
                }
            }

            if (interiorCol.parsedOk && AnnotationSupportsInteriorColor(typ)) {
                float interiorColor[3]{};
                PdfColorToFloat(interiorCol.pdfCol, interiorColor);
                // PdfColor 0 is "no fill"; 3 components would fill it black
                int nInteriorCol = (interiorCol.pdfCol == 0) ? 0 : 3;
                pdf_set_annot_interior_color(ctx, annot, nInteriorCol, interiorColor);
            }
            pdf_update_annot(ctx, annot);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
            if (annot) {
                // Unlink + drop page ref, then drop our keep from create.
                // Must not fall through to the success-path pdf_drop_annot.
                if (page) {
                    pdf_delete_annot(ctx, page, annot);
                }
                pdf_drop_annot(ctx, annot);
                annot = nullptr;
            }
        }
        if (!annot) {
            return nullptr;
        }
    }

    auto* res = MakeAnnotationWrapper(epdf, annot, pageNo);
    MarkNotificationAsModified(epdf, res, AnnotationChange::Add);

    if (typ == AnnotationType::Text) {
        TempStr iconName = GetAnnotationTextIconTemp();
        if (!str::EqI(iconName, StrL("Note"))) {
            SetIconName(res, iconName);
        }
    }
    if (col.parsedOk) {
        // FreeText: text color via pdf_set_annot_default_appearance; SetColor is bg
        if (typ != AnnotationType::FreeText) {
            SetColor(res, col.pdfCol);
        }
    }
    // SetColor copies the color's alpha (opaque for #rrggbb like HighlightColor)
    // and would wipe args->opacity set during create (highlighter is 40%).
    if (args->opacity < 100) {
        SetOpacity(res, (args->opacity * 255) / 100);
    }
    pdf_drop_annot(ctx, annot);
    return res;
}

struct AnnotationClipboard {
    bool valid = false;
    AnnotationType type = AnnotationType::Unknown;
    // the annotation's /Rect, not GetBounds(): the bound is expanded by the
    // border width, so a copy -> paste -> copy round trip would grow it
    RectF rect{};
    Str contents;
    Str iconName;
    Str fontName;
    PdfColor color = 0;
    bool hasColor = false;
    PdfColor interiorColor = 0;
    bool hasInteriorColor = false;
    PdfColor textColor = 0;
    bool hasTextColor = false;
    int opacity = 255;
    int borderWidth = -1;
    int quadding = -1;
    int textSize = -1;
    int lineStartStyle = 0;
    int lineEndStyle = 0;
    bool hasLine = false;
    PointF lineStart{};
    PointF lineEnd{};
    Vec<PointF> vertices;
    Vec<RectF> quads;
    Vec<int> inkStrokeCounts;
    Vec<PointF> inkPoints;
    Pixmap* stampImage = nullptr;
};

static AnnotationClipboard gAnnotClipboard;

static void ClearAnnotationClipboard() {
    gPendingCutAnnotation = nullptr;
    str::FreePtr(&gAnnotClipboard.contents);
    str::FreePtr(&gAnnotClipboard.iconName);
    str::FreePtr(&gAnnotClipboard.fontName);
    FreePixmap(gAnnotClipboard.stampImage);
    gAnnotClipboard.stampImage = nullptr;
    gAnnotClipboard.valid = false;
    gAnnotClipboard.type = AnnotationType::Unknown;
    gAnnotClipboard.rect = {};
    gAnnotClipboard.color = 0;
    gAnnotClipboard.hasColor = false;
    gAnnotClipboard.interiorColor = 0;
    gAnnotClipboard.hasInteriorColor = false;
    gAnnotClipboard.textColor = 0;
    gAnnotClipboard.hasTextColor = false;
    gAnnotClipboard.opacity = 255;
    gAnnotClipboard.borderWidth = -1;
    gAnnotClipboard.quadding = -1;
    gAnnotClipboard.textSize = -1;
    gAnnotClipboard.lineStartStyle = 0;
    gAnnotClipboard.lineEndStyle = 0;
    gAnnotClipboard.hasLine = false;
    gAnnotClipboard.lineStart = {};
    gAnnotClipboard.lineEnd = {};
    VecReset(gAnnotClipboard.vertices);
    VecReset(gAnnotClipboard.quads);
    VecReset(gAnnotClipboard.inkStrokeCounts);
    VecReset(gAnnotClipboard.inkPoints);
}

static Pixmap* PixmapFromRgbFzPixmap(fz_context* ctx, fz_pixmap* src) {
    if (!src || src->w <= 0 || src->h <= 0 || !src->samples) {
        return nullptr;
    }
    fz_pixmap* rgb = nullptr;
    fz_try(ctx) {
        rgb = fz_convert_pixmap(ctx, src, fz_device_rgb(ctx), nullptr, nullptr, fz_default_color_params, 1);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        rgb = nullptr;
    }
    fz_pixmap* use = rgb ? rgb : src;
    int n = use->n;
    if (n < 3 || !use->samples) {
        fz_drop_pixmap(ctx, rgb);
        return nullptr;
    }
    Pixmap* p = AllocPixmap(use->w, use->h, PixmapFormat::RGBA8);
    if (!p) {
        fz_drop_pixmap(ctx, rgb);
        return nullptr;
    }
    p->xres = (float)use->xres;
    p->yres = (float)use->yres;
    p->hasAlpha = true;
    int alphaOff = use->alpha ? n - 1 : -1;
    for (int y = 0; y < use->h; y++) {
        const u8* s = use->samples + y * use->stride;
        u8* d = p->data + y * p->stride;
        for (int x = 0; x < use->w; x++) {
            d[0] = s[0];
            d[1] = s[1];
            d[2] = s[2];
            d[3] = alphaOff >= 0 ? s[alphaOff] : 255;
            s += n;
            d += 4;
        }
    }
    fz_drop_pixmap(ctx, rgb);
    return p;
}

static Pixmap* GetStampImage(Annotation* annot) {
    if (!AnnotationIsLive(annot) || annot->type != AnnotationType::Stamp) {
        return nullptr;
    }
    EngineMupdf* e = annot->engine;
    if (!e || !e->pdfdoc) {
        return nullptr;
    }
    auto* ctx = e->Ctx();
    ScopedRecursiveMutex cs(&e->docLock);
    fz_image* img = nullptr;
    fz_pixmap* pix = nullptr;
    fz_var(img);
    fz_var(pix);
    fz_try(ctx) {
        pdf_obj* obj = pdf_annot_stamp_image_obj(ctx, annot->pdfannot);
        if (obj) {
            img = pdf_load_image(ctx, e->pdfdoc, obj);
            if (img) {
                pix = fz_get_pixmap_from_image(ctx, img, nullptr, nullptr, nullptr, nullptr);
            }
        }
    }
    fz_always(ctx) {
        fz_drop_image(ctx, img);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        fz_drop_pixmap(ctx, pix);
        pix = nullptr;
    }
    if (!pix) {
        return nullptr;
    }
    Pixmap* res = PixmapFromRgbFzPixmap(ctx, pix);
    fz_drop_pixmap(ctx, pix);
    return res;
}

bool HasCopiedAnnotation() {
    return gAnnotClipboard.valid;
}

// gAnnotClipboard owns strings, Vecs and a stamp bitmap; free them at exit
void FreeAnnotationClipboard() {
    ClearAnnotationClipboard();
}

// The annotation's /Rect. GetBounds() is pdf_bound_annot(), which is expanded
// by the border width and would grow the annotation on every copy -> paste.
static RectF GetAnnotRect(Annotation* annot) {
    if (!AnnotationIsLive(annot)) {
        return annot ? annot->bounds : RectF{};
    }
    EngineMupdf* e = annot->engine;
    auto* ctx = e->Ctx();
    ScopedRecursiveMutex cs(&e->docLock);
    fz_rect rc = {};
    bool ok = true;
    fz_try(ctx) {
        rc = pdf_annot_rect(ctx, annot->pdfannot);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        ok = false;
    }
    if (!ok) {
        return GetBounds(annot);
    }
    return ToRectF(rc);
}

// Snapshot the annotation and remember it as the one to delete on paste. A cut
// is a copy plus a delete of the original once the copy lands, so nothing is
// lost if the paste fails.
bool CutAnnotation(Annotation* annot) {
    if (!CopyAnnotation(annot)) {
        return false;
    }
    gPendingCutAnnotation = annot;
    return true;
}

// The annotation a cut is waiting to delete, if it is still around. Clears it:
// only the first paste of a cut removes the original, later ones are copies.
Annotation* TakeCutAnnotation() {
    Annotation* res = gPendingCutAnnotation;
    gPendingCutAnnotation = nullptr;
    return res;
}

// True if annot belongs to engine. Used to find the tab showing the document a
// cut annotation lives in, which can be a different one than we paste into.
bool EngineOwnsAnnotation(EngineBase* engine, Annotation* annot) {
    if (!engine || !annot) {
        return false;
    }
    return AsEngineMupdf(engine) == annot->engine;
}

// Snapshot a live annotation so PasteCopiedAnnotation can recreate it.
bool CopyAnnotation(Annotation* annot) {
    if (!AnnotationIsLive(annot) || !AnnotationCanBeCopied(annot->type)) {
        return false;
    }
    ClearAnnotationClipboard();
    gAnnotClipboard.valid = true;
    gAnnotClipboard.type = annot->type;
    gAnnotClipboard.rect = GetAnnotRect(annot);
    gAnnotClipboard.contents = str::Dup(Contents(annot));
    gAnnotClipboard.iconName = str::Dup(IconName(annot));
    gAnnotClipboard.opacity = Opacity(annot);
    if (AnnotationSupportsBorder(annot->type)) {
        gAnnotClipboard.borderWidth = BorderWidth(annot);
    }
    if (annot->type == AnnotationType::FreeText) {
        gAnnotClipboard.quadding = Quadding(annot);
        gAnnotClipboard.textSize = DefaultAppearanceTextSize(annot);
        gAnnotClipboard.fontName = str::Dup(DefaultAppearanceTextFont(annot));
        gAnnotClipboard.textColor = DefaultAppearanceTextColor(annot);
        gAnnotClipboard.hasTextColor = true;
        gAnnotClipboard.color = GetColor(annot);
        gAnnotClipboard.hasColor = true;
    } else if (AnnotationSupportsColor(annot->type)) {
        gAnnotClipboard.color = GetColor(annot);
        gAnnotClipboard.hasColor = true;
    }
    if (AnnotationSupportsInteriorColor(annot->type)) {
        gAnnotClipboard.interiorColor = InteriorColor(annot);
        gAnnotClipboard.hasInteriorColor = true;
    }
    GetLineEndingStyles(annot, &gAnnotClipboard.lineStartStyle, &gAnnotClipboard.lineEndStyle);
    gAnnotClipboard.hasLine = GetLinePoints(annot, gAnnotClipboard.lineStart, gAnnotClipboard.lineEnd);
    gAnnotClipboard.vertices = GetVertices(annot);
    gAnnotClipboard.quads = GetQuadPointsAsRect(annot);
    GetInkList(annot, gAnnotClipboard.inkStrokeCounts, gAnnotClipboard.inkPoints);
    gAnnotClipboard.stampImage = GetStampImage(annot);
    return true;
}

static void OffsetPoints(Vec<PointF>& pts, float dx, float dy) {
    for (int i = 0; i < len(pts); i++) {
        pts[i].x += dx;
        pts[i].y += dy;
    }
}

static void OffsetRects(Vec<RectF>& rects, float dx, float dy) {
    for (int i = 0; i < len(rects); i++) {
        rects[i].x += dx;
        rects[i].y += dy;
    }
}

// Recreate the copied annotation so its rect's top-left sits at topLeft.
Annotation* PasteCopiedAnnotation(EngineBase* engine, int pageNo, PointF topLeft) {
    if (!gAnnotClipboard.valid || !engine) {
        return nullptr;
    }
    ScopedEngineOperation op(engine, "Paste annotation");
    AnnotationClipboard& clip = gAnnotClipboard;
    float dx = topLeft.x - clip.rect.x;
    float dy = topLeft.y - clip.rect.y;
    RectF newRect{topLeft.x, topLeft.y, clip.rect.dx, clip.rect.dy};

    Vec<PointF> vertices = clip.vertices;
    OffsetPoints(vertices, dx, dy);
    Vec<PointF> inkPoints = clip.inkPoints;
    OffsetPoints(inkPoints, dx, dy);
    Vec<RectF> quads = clip.quads;
    OffsetRects(quads, dx, dy);

    AnnotCreateArgs args{clip.type};
    args.content = clip.contents;
    if (clip.type == AnnotationType::FreeText) {
        if (clip.hasTextColor) {
            args.col.parsedOk = true;
            args.col.pdfCol = clip.textColor;
        }
        if (clip.hasColor) {
            args.bgCol.parsedOk = true;
            args.bgCol.pdfCol = clip.color;
        }
        args.textSize = clip.textSize;
        args.borderWidth = clip.borderWidth;
        args.quadding = clip.quadding;
    } else if (clip.hasColor) {
        args.col.parsedOk = true;
        args.col.pdfCol = clip.color;
    }
    if (clip.hasInteriorColor) {
        args.interiorCol.parsedOk = true;
        args.interiorCol.pdfCol = clip.interiorColor;
    }
    args.stampImage = clip.stampImage;
    if (clip.hasLine) {
        args.hasLineEnd = true;
        args.lineEnd = {clip.lineEnd.x + dx, clip.lineEnd.y + dy};
    }
    if (len(vertices) >= 2) {
        args.polyLinePoints = &vertices;
    }
    if (len(clip.inkStrokeCounts) > 0) {
        args.inkStrokeCounts = &clip.inkStrokeCounts;
        args.inkPoints = &inkPoints;
    }
    bool useRect = clip.type == AnnotationType::Square || clip.type == AnnotationType::Circle ||
                   (clip.type == AnnotationType::Redact && len(quads) == 0);
    if (useRect) {
        args.hasRect = true;
        args.rect = newRect;
    }

    PointF pos = topLeft;
    if (clip.hasLine) {
        pos = {clip.lineStart.x + dx, clip.lineStart.y + dy};
    }
    Annotation* annot = EngineMupdfCreateAnnotation(engine, pageNo, pos, &args);
    if (!annot) {
        return nullptr;
    }
    if (clip.iconName) {
        SetIconName(annot, clip.iconName);
    }
    if (clip.fontName) {
        SetDefaultAppearanceTextFont(annot, clip.fontName);
    }
    if (clip.hasLine || clip.type == AnnotationType::PolyLine || clip.type == AnnotationType::Line) {
        SetLineStartStyles(annot, clip.lineStartStyle);
        SetLineEndStyles(annot, clip.lineEndStyle);
    }
    if (clip.type == AnnotationType::FreeText) {
        SetContents(annot, clip.contents);
        if (clip.hasTextColor) {
            SetDefaultAppearanceTextColor(annot, clip.textColor);
        }
        if (clip.textSize > 0) {
            SetDefaultAppearanceTextSize(annot, clip.textSize);
        }
        if (clip.quadding >= 0) {
            SetQuadding(annot, clip.quadding);
        }
    } else if (clip.contents) {
        SetContents(annot, clip.contents);
    }
    if (clip.opacity < 255) {
        SetOpacity(annot, clip.opacity);
    }
    if (AnnotationSupportsBorder(clip.type) && clip.borderWidth >= 0) {
        SetBorderWidth(annot, clip.borderWidth);
    }
    if (len(quads) > 0) {
        SetQuadPointsAsRect(annot, quads);
    }
    switch (clip.type) {
        case AnnotationType::Text:
        case AnnotationType::FreeText:
        case AnnotationType::Stamp:
        case AnnotationType::Caret:
        case AnnotationType::FileAttachment:
            SetRect(annot, newRect);
            break;
        default:
            break;
    }
    // a pasted annotation is new: it is dated now, not when the one it was
    // copied from was last touched. Last, so it also covers the writes above.
    SetModificationDateToNow(annot);
    return annot;
}

AnnotationType CmdIdToAnnotationType(int cmdId) {
    // clang-format off
    switch (cmdId) {
        case CmdCreateAnnotText:           return AnnotationType::Text;
        case CmdCreateAnnotLink:           return AnnotationType::Link;
        case CmdCreateAnnotFreeText:       return AnnotationType::FreeText;
        case CmdCreateAnnotLine:           return AnnotationType::Line;
        case CmdCreateAnnotSquare:         return AnnotationType::Square;
        case CmdCreateAnnotCircle:         return AnnotationType::Circle;
        case CmdCreateAnnotPolygon:        return AnnotationType::Polygon;
        case CmdCreateAnnotPolyLine:       return AnnotationType::PolyLine;
        case CmdCreateAnnotHighlight:      return AnnotationType::Highlight;
        case CmdCreateAnnotUnderline:      return AnnotationType::Underline;
        case CmdCreateAnnotSquiggly:       return AnnotationType::Squiggly;
        case CmdCreateAnnotStrikeOut:      return AnnotationType::StrikeOut;
        case CmdCreateAnnotRedact:         return AnnotationType::Redact;
        case CmdCreateAnnotStamp:          return AnnotationType::Stamp;
        case CmdCreateAnnotCaret:          return AnnotationType::Caret;
        case CmdCreateAnnotInk:            return AnnotationType::Ink;
        case CmdAnnotationHighlightBrush:  return AnnotationType::Ink;
        case CmdCreateAnnotPopup:          return AnnotationType::Popup;
        case CmdCreateAnnotFileAttachment: return AnnotationType::FileAttachment;
    }
    // clang-format on
    return AnnotationType::Unknown;
}
