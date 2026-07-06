/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern "C" {
#include <mupdf/pdf.h>
}

#include "base/Base.h"
#include "base/ScopedWin.h"

#include "wingui/UIModels.h"

#include "Annotation.h"
#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineMupdf.h"
#include "GlobalPrefs.h"
#include "Commands.h"

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

static SeqStrings gAnnotReadableNames =
    "Text\0"
    "Link\0"
    "Free Text\0"
    "Line\0"
    "Square\0"
    "Circle\0"
    "Polygon\0"
    "Poly Line\0"
    "Highlight\0"
    "Underline\0"
    "Squiggly\0"
    "StrikeOut\0"
    "Redact\0"
    "Stamp\0"
    "Caret\0"
    "Ink\0"
    "Popup\0"
    "File Attachment\0"
    "Sound\0"
    "Movie\0"
    "RichMedia\0"
    "Widget\0"
    "Screen\0"
    "Printer Mark\0"
    "Trap Net\0"
    "Watermark\0"
    "3D\0"
    "Projection\0";
Str AnnotationReadableNameTemp(AnnotationType tp) {
    int n = (int)tp;
    if (n < 0) {
        return StrL("Unknown");
    }
    Str s = SeqStrByIndex(gAnnotReadableNames, n);
    ReportIf(!s);
    return s;
}

AnnotationType Type(Annotation* annot) {
    ReportIf((int)annot->type < 0);
    return annot->type;
}

int PageNo(Annotation* annot) {
    ReportIf(annot->pageNo < 1);
    return annot->pageNo;
}

RectF GetBounds(Annotation* annot) {
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    auto ctx = e->Ctx();
    ScopedMutex cs(&e->docLock);
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
    return annot->bounds;
}

void SetRect(Annotation* annot, RectF r) {
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    if (!a) {
        // pdfannot is nulled out by DeleteAnnotation; a stale reference
        // (e.g. annotationBeingDragged after a reload) must not reach mupdf
        return;
    }
    bool failed = false;
    {
        auto ctx = e->Ctx();
        ScopedMutex cs(&e->docLock);
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

Str Author(Annotation* annot) {
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    auto ctx = e->Ctx();
    ScopedMutex cs(&e->docLock);

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

int Quadding(Annotation* annot) {
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    auto ctx = e->Ctx();
    ScopedMutex cs(&e->docLock);
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
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    {
        auto ctx = e->Ctx();
        ScopedMutex cs(&e->docLock);
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
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    {
        auto ctx = e->Ctx();
        ScopedMutex cs(&e->docLock);
        fz_quad quads[512];
        int n = len(rects);
        if (n == 0) {
            return;
        }
        constexpr int kMaxQuads = (int)dimof(quads);
        for (int i = 0; i < n && i < kMaxQuads; i++) {
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

int GetWidgetType(Annotation* annot) {
    if (!annot || annot->type != AnnotationType::Widget) {
        return PDF_WIDGET_TYPE_UNKNOWN;
    }
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    auto ctx = e->Ctx();
    ScopedMutex cs(&e->docLock);
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
    if (!annot || annot->type != AnnotationType::Widget) {
        return WidgetCursorKind::None;
    }
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    auto ctx = e->Ctx();
    ScopedMutex cs(&e->docLock);
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

bool ToggleFormButton(Annotation* annot) {
    if (!annot || annot->type != AnnotationType::Widget) {
        return false;
    }
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    bool changed = false;
    {
        // BaseCtx(), not a Ctx() clone: toggling regenerates the appearance,
        // which runs the button's format/calculate JS; mupdf executes (and
        // rethrows errors) on _ctx, so the fz_try must be on that context.
        auto ctx = e->BaseCtx();
        ScopedMutex cs(&e->docLock);
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

int GetWidgetFieldFlags(Annotation* annot) {
    if (!annot || annot->type != AnnotationType::Widget) {
        return 0;
    }
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    auto ctx = e->Ctx();
    ScopedMutex cs(&e->docLock);
    int flags = 0;
    fz_try(ctx) {
        flags = pdf_annot_field_flags(ctx, a);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }
    return flags;
}

Str GetWidgetValue(Annotation* annot) {
    if (!annot || annot->type != AnnotationType::Widget) {
        return Str();
    }
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    auto ctx = e->Ctx();
    ScopedMutex cs(&e->docLock);
    Str res;
    fz_try(ctx) {
        res = MupdfCStrTemp(pdf_annot_field_value(ctx, a));
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }
    return res;
}

float GetWidgetFontSize(Annotation* annot) {
    if (!annot || annot->type != AnnotationType::Widget) {
        return 0;
    }
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    auto ctx = e->Ctx();
    ScopedMutex cs(&e->docLock);
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

int GetWidgetMaxLen(Annotation* annot) {
    if (!annot || annot->type != AnnotationType::Widget) {
        return 0;
    }
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    auto ctx = e->Ctx();
    ScopedMutex cs(&e->docLock);
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

bool SetWidgetTextValue(Annotation* annot, Str value) {
    if (!annot || annot->type != AnnotationType::Widget) {
        return false;
    }
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    bool ok = false;
    TempStr valueZ = str::DupTemp(value);
    {
        // BaseCtx(), not a Ctx() clone: regenerating the appearance runs the
        // field's format/calculate JS, which mupdf executes (and rethrows
        // errors) on _ctx -- the fz_try must be on that same context.
        auto ctx = e->BaseCtx();
        ScopedMutex cs(&e->docLock);
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

void GetWidgetChoiceOptions(Annotation* annot, StrVec& out) {
    if (!annot || annot->type != AnnotationType::Widget) {
        return;
    }
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    auto ctx = e->Ctx();
    ScopedMutex cs(&e->docLock);
    fz_try(ctx) {
        int n = pdf_choice_widget_options(ctx, a, 0, nullptr);
        if (n > 0) {
            const char** opts = (const char**)fz_malloc(ctx, n * sizeof(char*));
            pdf_choice_widget_options(ctx, a, 0, opts);
            for (int i = 0; i < n; i++) {
                out.Append(opts[i] ? opts[i] : "");
            }
            fz_free(ctx, opts);
        }
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }
}

bool SetWidgetChoiceValue(Annotation* annot, Str value) {
    if (!annot || annot->type != AnnotationType::Widget) {
        return false;
    }
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    bool ok = false;
    TempStr valueZ = str::DupTemp(value);
    {
        // BaseCtx(), not a Ctx() clone: regenerating the appearance runs the
        // field's format/calculate JS, which mupdf executes (and rethrows
        // errors) on _ctx -- the fz_try must be on that same context.
        auto ctx = e->BaseCtx();
        ScopedMutex cs(&e->docLock);
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
    ScopedMutex cs(&e->docLock);
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
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    auto ctx = e->Ctx();
    ScopedMutex cs(&e->docLock);
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
    if (!annot) {
        return false;
    }
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    Str currValue = Contents(annot);
    if (str::Eq(sv, currValue)) {
        return false;
    }
    TempStr valueZ = str::DupTemp(sv);
    {
        auto ctx = e->Ctx();
        ScopedMutex cs(&e->docLock);
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

static bool IsAnnotationInEngine(EngineMupdf* e, Annotation* annot) {
    int pageNo = annot->pageNo;
    int pageIdx = pageNo - 1;
    if (pageIdx < 0 || pageIdx >= len(e->pages)) {
        return false;
    }
    ScopedMutex scope(&e->pagesLock);
    FzPageInfo* pageInfo = e->pages[pageIdx];
    return pageInfo->annotations.Contains(annot);
}

void DeleteAnnotation(Annotation* annot) {
    ReportIf(!annot);
    if (!annot) {
        return;
    }
    EngineMupdf* e = annot->engine;
    if (!e) {
        return;
    }
    auto a = annot->pdfannot;
    if (!a) {
        return;
    }
    if (!IsAnnotationInEngine(e, annot)) {
        logf("DeleteAnnotation: annotation not found in engine, skipping\n");
        return;
    }
    bool failed = false;
    {
        auto ctx = e->Ctx();
        ScopedMutex cs(&e->docLock);
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
    MarkNotificationAsModified(e, annot, AnnotationChange::Remove);
}

// -1 if not exist
int PopupId(Annotation* annot) {
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    auto ctx = e->Ctx();
    ScopedMutex cs(&e->docLock);
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
    ScopedMutex cs(&e->docLock);
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
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    auto ctx = e->Ctx();
    ScopedMutex cs(&e->docLock);
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
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    auto ctx = e->Ctx();
    ScopedMutex cs(&e->docLock);
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
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    TempStr nameZ = str::DupTemp(iconName);
    {
        auto ctx = e->Ctx();
        ScopedMutex cs(&e->docLock);
        fz_try(ctx) {
            pdf_set_annot_icon_name(ctx, a, len(nameZ) == 0 ? "" : nameZ.s);
            pdf_update_annot(ctx, a);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
        }
    }
    // TODO: only if the value changed
    MarkNotificationAsModified(e, annot);
}

void SetLineEndStyles(Annotation* annot, int end) {
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    {
        auto ctx = e->Ctx();
        ScopedMutex cs(&e->docLock);
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
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    {
        auto ctx = e->Ctx();
        ScopedMutex cs(&e->docLock);
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
    return alpha / 255.0f;
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
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    auto ctx = e->Ctx();
    ScopedMutex cs(&e->docLock);
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
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    {
        auto ctx = e->Ctx();
        ScopedMutex cs(&e->docLock);
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
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    auto ctx = e->Ctx();
    ScopedMutex cs(&e->docLock);
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
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    {
        auto ctx = e->Ctx();
        ScopedMutex cs(&e->docLock);
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
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    auto ctx = e->Ctx();
    ScopedMutex cs(&e->docLock);
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
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    TempStr fontZ = str::DupTemp(sv);
    {
        auto ctx = e->Ctx();
        ScopedMutex cs(&e->docLock);
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
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    auto ctx = e->Ctx();
    ScopedMutex cs(&e->docLock);
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
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    {
        auto ctx = e->Ctx();
        ScopedMutex cs(&e->docLock);
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
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    auto ctx = e->Ctx();
    ScopedMutex cs(&e->docLock);
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
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    {
        auto ctx = e->Ctx();
        ScopedMutex cs(&e->docLock);
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
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    auto ctx = e->Ctx();
    ScopedMutex cs(&e->docLock);
    pdf_line_ending leStart = PDF_ANNOT_LE_NONE;
    pdf_line_ending leEnd = PDF_ANNOT_LE_NONE;
    fz_try(ctx) {
        pdf_annot_line_ending_styles(ctx, a, &leStart, &leEnd);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        logf("GetLineEndingStyles: pdf_annot_line_ending_styles() failed\n");
    }
    *start = (int)leStart;
    *end = (int)leEnd;
}

int BorderWidth(Annotation* annot) {
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    auto ctx = e->Ctx();
    ScopedMutex cs(&e->docLock);
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
    if (!annot) {
        return;
    }
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    {
        auto ctx = e->Ctx();
        ScopedMutex cs(&e->docLock);
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
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    auto ctx = e->Ctx();
    ScopedMutex cs(&e->docLock);
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
    EngineMupdf* e = annot->engine;
    auto a = annot->pdfannot;
    {
        auto ctx = e->Ctx();
        ScopedMutex cs(&e->docLock);
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
    switch (tp) {
        // TODO: for now don't allow resizing text annotation because it's just an icon
        // would have to figure out how to change the size of the icon
        case AnnotationType::Text:
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

    auto pageInfo = epdf->GetFzPageInfo(pageNo, true);
    pdf_annot* annot = nullptr;
    auto typ = args->annotType;
    auto col = args->col;
    auto bgCol = args->bgCol;
    auto interiorCol = args->interiorCol;
    {
        ScopedMutex cs(&epdf->docLock);

        fz_try(ctx) {
            auto page = pdf_page_from_fz_page(ctx, pageInfo->page);
            enum pdf_annot_type atyp = (enum pdf_annot_type)typ;

            annot = pdf_create_annot(ctx, page, atyp);

            pdf_set_annot_modification_date(ctx, annot, time(nullptr));
            if (pdf_annot_has_author(ctx, annot)) {
                Str defAuthor = gGlobalPrefs->annotations.defaultAuthor;
                // if "(none)" we don't set it
                if (!str::Eq(defAuthor, "(none)")) {
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
                    trect.y0 = pos.y - dy / 2;
                    trect.y1 = trect.y0 + dy;
                    pdf_set_annot_rect(ctx, annot, trect);
                } break;
                case AnnotationType::Line: {
                    fz_point a{pos.x, pos.y};
                    fz_point b{pos.x + 100, pos.y + 50};
                    pdf_set_annot_line(ctx, annot, a, b);
                } break;
            }
            if (typ == AnnotationType::Stamp && len(args->stampImage) > 0) {
                // image stamp (e.g. pasted from the clipboard): embed the image
                // and size the rect to the image's natural size, anchored at pos
                fz_image* img = nullptr;
                fz_buffer* buf = nullptr;
                fz_var(img);
                fz_var(buf);
                fz_try(ctx) {
                    buf = fz_new_buffer_from_copied_data(ctx, (u8*)args->stampImage.s, (size_t)args->stampImage.len);
                    img = fz_new_image_from_buffer(ctx, buf);
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
                    fz_drop_buffer(ctx, buf);
                }
                fz_catch(ctx) {
                    fz_rethrow(ctx);
                }
            }
            if (typ == AnnotationType::FreeText) {
                if (args->borderWidth >= 0) {
                    pdf_set_annot_border_width(ctx, annot, (float)args->borderWidth);
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
                pdf_drop_annot(ctx, annot);
            }
        }
        if (!annot) {
            return nullptr;
        }
    }

    auto res = MakeAnnotationWrapper(epdf, annot, pageNo);
    MarkNotificationAsModified(epdf, res, AnnotationChange::Add);

    if (typ == AnnotationType::Text) {
        TempStr iconName = GetAnnotationTextIconTemp();
        if (!str::EqI(iconName.s, "Note")) {
            SetIconName(res, iconName);
        }
    }
    if (col.parsedOk) {
        switch (typ) {
            case AnnotationType::FreeText:
                // do nothing. for free text we set text color via pdf_set_annot_default_appearance
                // and SetColor() sets background color
                break;
            default:
                SetColor(res, col.pdfCol);
                break;
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
