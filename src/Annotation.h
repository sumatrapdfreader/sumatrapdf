/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

enum class AnnotationType {
    None,
    Highlight,
    Underline,
    StrikeOut,
    Squiggly,
};

// an user annotation on page
struct Annotation {
    AnnotationType type = AnnotationType::None;
    int pageNo = -1;
    RectD rect = {};
    COLORREF color = 0;
    // either new annotation or has been modified
    bool isChanged = false;
    // deleted are not shown but can be undeleted
    bool isDeleted = false;

    Annotation() = default;
    Annotation(AnnotationType type, int pageNo, RectD rect, COLORREF color);
};

bool IsAnnotationEq(Annotation* a1, Annotation* a2);

void DeleteVecAnnotations(Vec<Annotation*>* annots);
Vec<Annotation*> GetAnnotationsForPage(Vec<Annotation*>* annots, int pageNo);
