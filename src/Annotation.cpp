/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Pixmap.h"
#include "base/ScopedWin.h"

extern "C" {
#include <mupdf/pdf.h>
}

#include "gui/UIModels.h"

#include "Annotation.h"
#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineMupdf.h"
#include "GlobalPrefs.h"
#include "Commands.h"
#include "Translations.h"

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
    int pageIdx = pageNo - 1;
    if (pageIdx < 0 || pageIdx >= len(e->pages)) {
        return false;
    }
    ScopedRecursiveMutex scope(&e->pagesLock);
    FzPageInfo* pageInfo = e->pages[pageIdx];
    if (!pageInfo) {
        return false;
    }
    return pageInfo->annotations.Contains(annot) || pageInfo->widgets.Contains(annot);
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
    {
        auto* ctx = e->Ctx();
        ScopedRecursiveMutex cs(&e->docLock);
        fz_rect rc = ToFzRect(r);
        fz_try(ctx) {
            if (annot->type == AnnotationType::Line) {
                // line annotation doesn't have a rect but a line position
                // TODO: not sure this is the right place for this
                fz_point p1 = {rc.x0, rc.y0}, p2 = {rc.x1, rc.y1};
                pdf_set_annot_line(ctx, a, p1, p2);
            } else {
                pdf_set_annot_rect(ctx, a, rc);
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
    annot->bounds = r;
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
    if (!s || str::IsEmptyOrWhiteSpace(s)) {
        return {};
    }
    return str::DupTemp(Str(s));
}

// EditAnnotations.cpp
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
                out.Append(opts[i] ? opts[i] : "");
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

/*
Vec<RectF> GetQuadPointsAsRect(Annotation* annot) {
    EngineMupdf* e = annot->engine;
    auto ctx = e->Ctx();
    auto pdf = annot->pdf;
    ScopedRecursiveMutex cs(&e->docLock);
    Vec<RectF> res;
    int n = pdf_annot_quad_point_count(ctx, annot->pdfannot);
    for (int i = 0; i < n; i++) {
        fz_quad q{};
        fz_rect r{};
        fz_try(ctx)
        {
            q = pdf_annot_quad_point(ctx, annot->pdfannot, i);
            r = fz_rect_from_quad(q);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
        }
        RectF rect = ToRectF(r);
        res.Append(rect);
    }
    return res;
}
*/

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
    u8 r, g, b, a;
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
                // TODO: set opacity to 1?
                // pdf_set_annot_opacity(ctx, a, 1.f);
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
        ReportIf(newOpacity < 0 || newOpacity > 255);
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
    if (!u) {
        u = Str(getenv("USERNAME"));
    }
    if (!u) {
        return StrL("user");
    }
    return u;
}

static TempStr GetAnnotationTextIconTemp() {
    TempStr s = str::DupTemp(gGlobalPrefs->annotations.textIconType);
    // this way user can use "new paragraph" and we'll match "NewParagraph"
    str::RemoveCharsInPlace(s, " ");
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

// matches rect_subtypes in pdf-annot.c + Line (because special case it in SetRect())
// TODO: should include AnnotationType::ThreeD but mupdf doesn't
static AnnotationType moveableAnnotations[] = {
    AnnotationType::Text,           AnnotationType::FreeText, AnnotationType::Square, AnnotationType::Circle,
    AnnotationType::Redact,         AnnotationType::Stamp,    AnnotationType::Caret,  AnnotationType::Popup,
    AnnotationType::FileAttachment, AnnotationType::Sound,    AnnotationType::Movie,  AnnotationType::Widget,
    AnnotationType::Line,
};

static AnnotationType supportsBorder[] = {
    AnnotationType::FreeText, AnnotationType::Ink,     AnnotationType::Line,     AnnotationType::Square,
    AnnotationType::Circle,   AnnotationType::Polygon, AnnotationType::PolyLine,
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

bool AnnotationCanBeResized(AnnotationType tp) {
    if (tp == AnnotationType::Text) {
        // TODO: for now don't allow resizing text annotation because it's just an icon
        // would have to figure out how to change the size of the icon
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

Annotation* EngineMupdfCreateAnnotation(EngineBase* engine, int pageNo, PointF pos, AnnotCreateArgs* args) {
    static const float black[3] = {0, 0, 0};

    EngineMupdf* epdf = AsEngineMupdf(engine);
    fz_context* ctx = epdf->Ctx();

    auto* pageInfo = epdf->GetFzPageInfo(pageNo, true);
    pdf_annot* annot = nullptr;
    pdf_page* page = nullptr;
    auto typ = args->annotType;
    auto col = args->col;
    auto bgCol = args->bgCol;
    auto interiorCol = args->interiorCol;
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
                Str defAuthor = gGlobalPrefs->annotations.defaultAuthor;
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
                case AnnotationType::Polygon:
                case AnnotationType::Redact:
                case AnnotationType::Ink:
                case AnnotationType::Popup:
                case AnnotationType::PolyLine:
                case AnnotationType::Unknown:
                case AnnotationType::FileAttachment:
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
                    if (!str::IsEmptyOrWhiteSpace(content)) {
                        pdf_set_annot_contents(ctx, annot, content);
                    }
                } break;
                case AnnotationType::Text:
                case AnnotationType::FreeText:
                case AnnotationType::Stamp:
                case AnnotationType::Square:
                case AnnotationType::Circle: {
                    fz_rect trect = pdf_annot_rect(ctx, annot);
                    float dx = trect.x1 - trect.x0;
                    trect.x0 = pos.x;
                    trect.x1 = trect.x0 + dx;
                    float dy = trect.y1 - trect.y0;
                    trect.y0 = pos.y;
                    trect.y1 = trect.y0 + dy;
                    pdf_set_annot_rect(ctx, annot, trect);
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
                    fz_point b{pos.x + 100, pos.y + 50};
                    pdf_set_annot_line(ctx, annot, a, b);
                } break;
            }
            if (typ == AnnotationType::Stamp && args->stampImage) {
                // image stamp (e.g. pasted from the clipboard): embed the image
                // and size the rect to the image's natural size, anchored at pos
                Pixmap* stamp = args->stampImage;
                fz_image* img = nullptr;
                fz_pixmap* pix = nullptr;
                fz_var(img);
                fz_var(pix);
                fz_try(ctx) {
                    int alpha = stamp->format == PixmapFormat::BGR8 ? 0 : 1;
                    fz_colorspace* colorSpace =
                        stamp->format == PixmapFormat::RGBA8 ? fz_device_rgb(ctx) : fz_device_bgr(ctx);
                    pix = fz_new_pixmap_with_data(ctx, colorSpace, stamp->width, stamp->height, nullptr, alpha,
                                                  stamp->stride, stamp->data);
                    pix->xres = (int)stamp->xres;
                    pix->yres = (int)stamp->yres;
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
                if (!str::IsEmptyOrWhiteSpace(content)) {
                    pdf_set_annot_contents(ctx, annot, content);
                } else {
                    pdf_set_annot_contents(ctx, annot, "This is a text...");
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
                    pdf_set_annot_color(ctx, annot, 3, bgColor);
                }
                // 100 is fuly opaque, the default
                if (args->opacity < 100) {
                    float fop = (float)args->opacity / 100.0f;
                    pdf_set_annot_opacity(ctx, annot, fop);
                }
            }

            if (interiorCol.parsedOk && AnnotationSupportsInteriorColor(typ)) {
                float interiorColor[3]{};
                PdfColorToFloat(interiorCol.pdfCol, interiorColor);
                pdf_set_annot_interior_color(ctx, annot, 3, interiorColor);
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
        if (!str::EqI(iconName.s, StrL("Note"))) {
            SetIconName(res, iconName);
        }
    }
    if (col.parsedOk) {
        // FreeText: text color via pdf_set_annot_default_appearance; SetColor is bg
        if (typ != AnnotationType::FreeText) {
            SetColor(res, col.pdfCol);
        }
    }
    pdf_drop_annot(ctx, annot);
    return res;
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
        case CmdCreateAnnotPopup:          return AnnotationType::Popup;
        case CmdCreateAnnotFileAttachment: return AnnotationType::FileAttachment;
    }
    // clang-format on
    return AnnotationType::Unknown;
}
