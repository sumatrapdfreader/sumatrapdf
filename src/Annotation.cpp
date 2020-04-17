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

bool Annotation::operator==(const Annotation& other) const {
    if (&other == this) {
        return true;
    }
    if (other.type != type) {
        return false;
    }
    if (other.pageNo != pageNo) {
        return false;
    }
    if (other.color != color) {
        return false;
    }
    if (other.rect != rect) {
        return false;
    }
    return true;
}

void DeleteVecAnnotations(Vec<Annotation*>* annots) {
    if (!annots) {
        return;
    }
    DeleteVecMembers(*annots);
    delete annots;
}

Vec<Annotation> GetAnnotationsForPage(Vec<Annotation>& userAnnots, int pageNo) {
    Vec<Annotation> result;
    for (size_t i = 0; i < userAnnots.size(); i++) {
        Annotation& annot = userAnnots.at(i);
        if (annot.pageNo != pageNo) {
            continue;
        }
        // include all annotations for pageNo that can be rendered by fz_run_user_annots
        switch (annot.type) {
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
