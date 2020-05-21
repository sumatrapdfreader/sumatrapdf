/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern "C" {
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
}

#include "utils/BaseUtil.h"
#include "Annotation.h"

// spot checks the definitions are the same
static_assert((int)AnnotationType::Link == (int)PDF_ANNOT_LINK);
static_assert((int)AnnotationType::ThreeD == (int)PDF_ANNOT_3D);
static_assert((int)AnnotationType::Sound == (int)PDF_ANNOT_SOUND);
static_assert((int)AnnotationType::Unknown == (int)PDF_ANNOT_UNKNOWN);

AnnotationType AnnotationTypeFromPdfAnnot(enum pdf_annot_type tp) {
    return (AnnotationType)tp;
}

Annotation::Annotation(AnnotationType type, int pageNo, RectD rect, COLORREF color) {
    this->type = type;
    this->pageNo = pageNo;
    this->rect = rect;
    this->color = color;
}

bool IsAnnotationEq(Annotation* a1, Annotation* a2) {
    if (a1 == a2) {
        return true;
    }
    if (a1->type != a2->type) {
        return false;
    }
    if (a1->pageNo != a2->pageNo) {
        return false;
    }
    if (a1->color != a2->color) {
        return false;
    }
    if (a1->rect != a2->rect) {
        return false;
    }
    return true;
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

void DeleteVecAnnotations(Vec<Annotation*>* annots) {
    if (!annots) {
        return;
    }
    DeleteVecMembers(*annots);
    delete annots;
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
        if (annot->pageNo != pageNo) {
            continue;
        }
        // include all annotations for pageNo that can be rendered by fz_run_user_annots
        switch (annot->type) {
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
