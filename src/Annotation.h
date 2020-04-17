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

    Annotation() = default;
    Annotation(AnnotationType type, int pageNo, RectD rect, COLORREF color);
    bool operator==(const Annotation& other) const;
};
