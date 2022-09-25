/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// TODO: not quite happy how those functions are split among
// Annotation.cpp, EngineMupdf.cpp and EditAnnotations.cpp

// for fast conversions, must match the order of pdf_annot_type enum in annot.h
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
    RichMedia,
    Widget,
    Screen,
    PrinterMark,
    TrapNet,
    Watermark,
    ThreeD,
    Projection,
    Unknown = -1
};

class EngineMupdf;
extern "C" struct pdf_annot;

extern const char* gAnnotationTextIcons;

// an user annotation on page
// It abstracts over pdf_annot so that we don't have to
// inlude mupdf to include Annotation
struct Annotation {
    // common to both smx and pdf
    AnnotationType type{AnnotationType::Unknown};
    int pageNo = -1;

    // either new annotation or has been modified
    bool isChanged = false;
    // deleted are not shown but can be undeleted
    bool isDeleted = false;

    EngineMupdf* engine = nullptr;
    pdf_annot* pdfannot = nullptr;

    Annotation() = default;
    ~Annotation() = default;
};

int PageNo(Annotation*);
RectF GetBounds(Annotation*);
RectF GetRect(Annotation*);
void SetRect(Annotation*, RectF);
void SetQuadPointsAsRect(Annotation*, const Vec<RectF>&);
// Vec<Annotation*> FilterAnnotationsForPage(Vec<Annotation*>* annots, int pageNo);

// EditAnnotations.cpp
const char* Author(Annotation*);
time_t ModificationDate(Annotation*);
int PopupId(Annotation*); // -1 if not exist
const char* AnnotationReadableName(AnnotationType);
AnnotationType Type(Annotation*);
const char* DefaultAppearanceTextFont(Annotation*);
PdfColor DefaultAppearanceTextColor(Annotation*);
int DefaultAppearanceTextSize(Annotation*);
void SetDefaultAppearanceTextFont(Annotation*, const char*);
void SetDefaultAppearanceTextSize(Annotation*, int);
void SetDefaultAppearanceTextColor(Annotation*, PdfColor);
const char* Contents(Annotation*);
int Quadding(Annotation*);
bool SetQuadding(Annotation*, int);
int BorderWidth(Annotation*);
void SetBorderWidth(Annotation*, int);
void GetLineEndingStyles(Annotation*, int* start, int* end);
const char* IconName(Annotation*); // empty() if no icon
void SetIconName(Annotation*, const char*);
PdfColor GetColor(Annotation*); // ColorUnset if no color
bool SetColor(Annotation*, PdfColor);
PdfColor InteriorColor(Annotation*); // ColorUnset if no color
bool SetInteriorColor(Annotation*, PdfColor);
int Opacity(Annotation*);
void SetOpacity(Annotation*, int);
bool SetContents(Annotation*, const char*);
bool IsAnnotationEq(Annotation* a1, Annotation* a2);

void DeleteAnnotation(Annotation*);

// EngineMupdf.cpp
Annotation* MakeAnnotationPdf(EngineMupdf*, pdf_annot*, int pageNo);
