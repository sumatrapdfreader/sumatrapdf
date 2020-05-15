/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern "C" struct pdf_annot;
extern "C" struct fz_context;

enum class AnnotationType {
    None,
    Highlight,
    Underline,
    StrikeOut,
    Squiggly,
    Text,
    FreeText,
    Line,
    Square,
    Ink,
    Link,
    Circle,
    Polygon,
    PolyLine,
    Redact,
    Stamp,
    Caret,
    Popup,
    FileAttachment,
    Sound,
    Movie,
    Widget,
    Screen,
    PrinterMark,
    TrapNet,
    Watermark,
    ThreeD
};

// an user annotation on page
struct Annotation {
    AnnotationType type = AnnotationType::None;
    int pageNo = -1;
    RectD rect = {};
    COLORREF color = 0;

    // flags has the same meaning as mupdf annot.h
    // TODO: not sure if want to preserve it
    int flags;
    str::Str contents;
    str::Str author;

    // either new annotation or has been modified
    bool isChanged = false;
    // deleted are not shown but can be undeleted
    bool isDeleted = false;

    // set if constructed from mupdf annotation
    fz_context* ctx = nullptr;
    pdf_annot* pdf_annot = nullptr;

    Annotation() = default;
    Annotation(AnnotationType type, int pageNo, RectD rect, COLORREF color);
};

std::string_view AnnotationName(AnnotationType);

bool IsAnnotationEq(Annotation* a1, Annotation* a2);

void DeleteVecAnnotations(Vec<Annotation*>* annots);
Vec<Annotation*> FilterAnnotationsForPage(Vec<Annotation*>* annots, int pageNo);
