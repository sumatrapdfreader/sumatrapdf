/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern "C" struct pdf_annot;
extern "C" struct fz_context;

// for fast conversions, must match the order of the above enum
// (from mupdf annot.h)
enum class AnnotationType {
    Text,
    Link,
    FreeText,
    Line,
    Square,
    Circle,
    Polygon,
    PolyLine,
    Highlight,
    Underline,
    Squiggly,
    StrikeOut,
    Redact,
    Stamp,
    Caret,
    Ink,
    Popup,
    FileAttachment,
    Sound,
    Movie,
    Widget,
    Screen,
    PrinterMark,
    TrapNet,
    Watermark,
    ThreeD,
    Unknown = -1
};

struct AnnotationSmx;
struct AnnotationPdf;

// an user annotation on page
struct Annotation {
    // common to both smx and pdf
    AnnotationType type = AnnotationType::Unknown;
    int pageNo = -1;

    // either new annotation or has been modified
    bool isChanged = false;
    // deleted are not shown but can be undeleted
    bool isDeleted = false;

    // only one of them must be set
    AnnotationSmx* smx = nullptr;
    AnnotationPdf* pdf = nullptr;

    Annotation() = default;
    ~Annotation();

    AnnotationType Type() const;
    int PageNo() const;
    RectD Rect() const;
    COLORREF Color();
    std::string_view Author();
    std::string_view Contents();
    time_t CreationDate();
    time_t ModificationDate();
};

Annotation* MakeAnnotationSmx(AnnotationType, int pageNo, RectD, COLORREF);

std::string_view AnnotationName(AnnotationType);
bool IsAnnotationEq(Annotation* a1, Annotation* a2);

void DeleteVecAnnotations(Vec<Annotation*>* annots);
Vec<Annotation*> FilterAnnotationsForPage(Vec<Annotation*>* annots, int pageNo);
AnnotationType AnnotationTypeFromPdfAnnot(enum pdf_annot_type tp);
