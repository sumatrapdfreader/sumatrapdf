/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "Annotation.h"

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

const char* annotNames = "None\0Highlight\0Underline\0StrikeOut\0Squiggly\0Text\0FreeText\0Line\0";
std::string_view AnnotationName(AnnotationType tp) {
    int n = (int)tp;
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
