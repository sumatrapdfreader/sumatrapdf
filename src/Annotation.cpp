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
