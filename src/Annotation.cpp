/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern "C" {
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
}

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"

#include "TreeModel.h"
#include "Annotation.h"
#include "EngineBase.h"
#include "EngineFzUtil.h"

// spot checks the definitions are the same
static_assert((int)AnnotationType::Link == (int)PDF_ANNOT_LINK);
static_assert((int)AnnotationType::ThreeD == (int)PDF_ANNOT_3D);
static_assert((int)AnnotationType::Sound == (int)PDF_ANNOT_SOUND);
static_assert((int)AnnotationType::Unknown == (int)PDF_ANNOT_UNKNOWN);

AnnotationType AnnotationTypeFromPdfAnnot(enum pdf_annot_type tp) {
    return (AnnotationType)tp;
}

// clang format-off
// must match the order of enum class AnnotationType
const char* annotNames =
    "Text\0Link\0FreeText\0Line\0Square\0Circle\0Polygon\0PolyLine\0"
    "Highlight\0Underline\0Squiggly\0StrikeOut\0Redact\0Stamp\0"
    "Caret\0Ink\0Popup\0FileAttachment\0Sound\0Movie\0Widget\0"
    "Screen\0PrinterMark\0TrapNet\0Watermark\03D\0";
// clang format-on

std::string_view AnnotationName(AnnotationType tp) {
    int n = (int)tp;
    if (n < 0) {
        return "Unknown";
    }
    const char* s = seqstrings::IdxToStr(annotNames, n);
    return {s};
}

struct AnnotationSmx {
    RectD rect = {};
    COLORREF color = ColorUnset;
    COLORREF interiorColor = ColorUnset;

    // flags has the same meaning as mupdf annot.h
    // TODO: not sure if want to preserve it
    int flags;

    str::Str contents;
    str::Str author;
    int quadding; // aka Text Alignment
    str::Str iconName;

    time_t creationDate;
    time_t modificationDate;
};

struct AnnotationPdf {
    // set if constructed from mupdf annotation
    fz_context* ctx = nullptr;
    pdf_page* page = nullptr;
    pdf_annot* annot = nullptr;
};

bool IsAnnotationEq(Annotation* a1, Annotation* a2) {
    if (a1 == a2) {
        return true;
    }
    // TODO: fix me
    CrashIf(false);
    if (a1->type != a2->type) {
        return false;
    }
    if (a1->pageNo != a2->pageNo) {
        return false;
    }
#if 0
    if (a1->color != a2->color) {
        return false;
    }
    if (a1->rect != a2->rect) {
        return false;
    }
#endif
    return false;
}

Annotation::~Annotation() {
    delete smx;
    delete pdf;
}

void DeleteVecAnnotations(Vec<Annotation*>* annots) {
    if (!annots) {
        return;
    }
    DeleteVecMembers(*annots);
    delete annots;
}

AnnotationType Annotation::Type() const {
    CrashIf((int)type < 0);
    return type;
}

int Annotation::PageNo() const {
    CrashIf(pageNo < 0);
    return pageNo;
}

RectD Annotation::Rect() const {
    if (smx) {
        return smx->rect;
    }
    // TODO: cache during creation?
    fz_rect rc = pdf_annot_rect(pdf->ctx, pdf->annot);
    auto rect = fz_rect_to_RectD(rc);
    return rect;
}

std::string_view Annotation::Author() {
    if (smx) {
        return smx->author.as_view();
    }
    const char* s = pdf_annot_author(pdf->ctx, pdf->annot);
    if (str::IsStringEmptyOrWhiteSpaceOnly(s)) {
        return {};
    }
    return s;
}

int Annotation::Quadding() {
    if (smx) {
        return smx->quadding;
    }
    return pdf_annot_quadding(pdf->ctx, pdf->annot);
}

static bool IsValidQuadding(int i) {
    return i >= 0 && i <= 2;
}

// return true if changed
bool Annotation::SetQuadding(int newQuadding) {
    CrashIf(!IsValidQuadding(newQuadding));
    bool didChange = Quadding() != newQuadding;
    if (!didChange) {
        return false;
    }
    if (smx) {
        smx->quadding = newQuadding;
    } else {
        pdf_set_annot_quadding(pdf->ctx, pdf->annot, newQuadding);
        pdf_update_appearance(pdf->ctx, pdf->annot);
    }
    isChanged = true;
    return true;
}

std::string_view Annotation::Contents() {
    if (smx) {
        return smx->contents.as_view();
    }
    // TODO: cache during creation?
    const char* s = pdf_annot_contents(pdf->ctx, pdf->annot);
    return s;
}

bool Annotation::SetContents(std::string_view sv) {
    std::string_view currValue = Contents();
    if (str::Eq(sv, currValue.data())) {
        return false;
    }
    isChanged = true;
    if (smx) {
        smx->contents.Set(sv);
    } else {
        pdf_set_annot_contents(pdf->ctx, pdf->annot, sv.data());
        pdf_update_appearance(pdf->ctx, pdf->annot);
    }
    return true;
}

void Annotation::Delete() {
    CrashIf(isDeleted);
    if (smx) {
        // no-op
    } else {
        pdf_delete_annot(pdf->ctx, pdf->page, pdf->annot);
    }
    isDeleted = true;
    isChanged = true; // TODO: not sure I need this
}

// -1 if not exist
int Annotation::PopupId() {
    if (smx) {
        // not available for smx
        return -1;
    }
    pdf_obj* obj = pdf_dict_get(pdf->ctx, pdf->annot->obj, PDF_NAME(Popup));
    if (!obj) {
        return -1;
    }
    int res = pdf_to_num(pdf->ctx, obj);
    return res;
}

time_t Annotation::CreationDate() {
    if (smx) {
        return smx->creationDate;
    }
    auto res = pdf_annot_creation_date(pdf->ctx, pdf->annot);
    return res;
}

time_t Annotation::ModificationDate() {
    if (smx) {
        return smx->modificationDate;
    }
    auto res = pdf_annot_modification_date(pdf->ctx, pdf->annot);
    return res;
}

// return empty() if no icon
std::string_view Annotation::IconName() {
    if (smx) {
        return smx->iconName.as_view();
    }
    bool hasIcon = pdf_annot_has_icon_name(pdf->ctx, pdf->annot);
    if (!hasIcon) {
        return {};
    }
    // can only call if pdf_annot_has_icon_name() returned true
    const char* iconName = pdf_annot_icon_name(pdf->ctx, pdf->annot);
    return {iconName};
}

void Annotation::SetIconName(std::string_view iconName) {
    if (smx) {
        smx->iconName.Set(iconName);
    } else {
        pdf_set_annot_icon_name(pdf->ctx, pdf->annot, iconName.data());
        pdf_update_appearance(pdf->ctx, pdf->annot);
    }
    // TODO: only if the value changed
    isChanged = true;
}

// ColorUnset if no color
COLORREF Annotation::Color() {
    if (smx) {
        return smx->color;
    }
    float color[4];
    int n;
    pdf_annot_color(pdf->ctx, pdf->annot, &n, color);
    COLORREF res = FromPdfColor(pdf->ctx, n, color);
    return res;
}

// return true if color changed
bool Annotation::SetColor(COLORREF c) {
    bool didChange;
    if (smx) {
        didChange = smx->color != c;
        smx->color = c;
        if (didChange) {
            isChanged = true;
        }
        return didChange;
    }
    float color[4];
    int n;
    pdf_annot_color(pdf->ctx, pdf->annot, &n, color);
    float newColor[4];
    int newN = ToPdfRgba(c, newColor);
    didChange = (n != newN);
    if (!didChange) {
        for (int i = 0; i < n; i++) {
            if (color[i] != newColor[i]) {
                didChange = true;
            }
        }
    }
    pdf_set_annot_color(pdf->ctx, pdf->annot, newN, newColor);
    pdf_update_appearance(pdf->ctx, pdf->annot);
    if (didChange) {
        isChanged = true;
    }
    return didChange;
}

// ColorUnset if no color
COLORREF Annotation::InteriorColor() {
    if (smx) {
        return smx->interiorColor;
    }
    float color[4];
    int n;
    pdf_annot_interior_color(pdf->ctx, pdf->annot, &n, color);
    COLORREF res = FromPdfColor(pdf->ctx, n, color);
    return res;
}

Annotation* MakeAnnotationPdf(fz_context* ctx, pdf_page* page, pdf_annot* annot, int pageNo) {
    auto tp = pdf_annot_type(ctx, annot);
    AnnotationType typ = AnnotationTypeFromPdfAnnot(tp);
    if (typ == AnnotationType::Unknown) {
        // unsupported type
        return nullptr;
    }
    AnnotationPdf* apdf = new AnnotationPdf();
    apdf->ctx = ctx;
    apdf->annot = annot;
    apdf->page = page;

    Annotation* res = new Annotation();
    res->pageNo = pageNo;
    res->pdf = apdf;
    res->type = typ;

    // res->flags = pdf_annot_flags(ctx, annot);
    // TODO: implement those
    // pdf_annot_opacity(ctx, annot)
    // pdf_annot_border
    // pdf_annot_language
    // pdf_annot_quadding
    // pdf_annot_interior_color
    // pdf_annot_quad_point_count / pdf_annot_quad_point
    // pdf_annot_ink_list_count / pdf_annot_ink_list_stroke_count / pdf_annot_ink_list_stroke_vertex
    // pdf_annot_line_start_style, pdf_annot_line_end_style
    // pdf_annot_icon_name
    // pdf_annot_line
    // pdf_annot_vertex_count

    return res;
}

Annotation* MakeAnnotationSmx(AnnotationType type, int pageNo, RectD rect, COLORREF col) {
    AnnotationSmx* smx = new AnnotationSmx();
    smx->rect = rect;
    smx->color = col;
    Annotation* res = new Annotation();
    res->smx = smx;
    res->type = type;
    res->pageNo = pageNo;
    return res;
}

Vec<Annotation*> FilterAnnotationsForPage(Vec<Annotation*>* annots, int pageNo) {
    Vec<Annotation*> result;
    if (!annots) {
        return result;
    }
    for (auto& annot : *annots) {
        if (annot->isDeleted) {
            continue;
        }
        if (annot->PageNo() != pageNo) {
            continue;
        }
        // include all annotations for pageNo that can be rendered by fz_run_user_annots
        switch (annot->Type()) {
            case AnnotationType::Highlight:
            case AnnotationType::Underline:
            case AnnotationType::StrikeOut:
            case AnnotationType::Squiggly:
                result.Append(annot);
                break;
        }
    }
    return result;
}
