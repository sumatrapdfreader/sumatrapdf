/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// dummy alias for pdf_annot, exists so that we don't have to expose mupdf internals
// struct Annot;
// typedef void* Annot;

extern "C" struct fz_context;

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

struct AnnotationPdf;

// an user annotation on page
// It abstracts over pdf_annot so that we don't have to
// inlude mupdf to include Annotation
struct Annotation {
    // common to both smx and pdf
    AnnotationType type{AnnotationType::Unknown};
    int pageNo{-1};

    // either new annotation or has been modified
    bool isChanged{false};
    // deleted are not shown but can be undeleted
    bool isDeleted{false};

    AnnotationPdf* pdf{nullptr};

    Annotation() = default;
    ~Annotation();
};

void SetRect(Annotation*, RectF);
bool SetColor(Annotation*, COLORREF);
bool SetInteriorColor(Annotation*, COLORREF);
bool SetQuadding(Annotation*, int);
void SetQuadPointsAsRect(Annotation*, const Vec<RectF>&);
void SetDefaultAppearanceTextColor(Annotation*, COLORREF);
void SetDefaultAppearanceTextSize(Annotation*, int);
void SetDefaultAppearanceTextFont(Annotation*, std::string_view);
bool SetContents(Annotation*, std::string_view sv);
void SetIconName(Annotation*, std::string_view);
void SetLineEndingStyles(Annotation*, int start, int end);
void SetOpacity(Annotation*, int);
void SetBorderWidth(Annotation*, int);

AnnotationType Type(Annotation*);
int PageNo(Annotation*);
// note: page no can't be changed
RectF GetRect(Annotation*);
COLORREF GetColor(Annotation*);      // ColorUnset if no color
COLORREF InteriorColor(Annotation*); // ColorUnset if no color
std::string_view Author(Annotation*);

int Quadding(Annotation*);
Vec<RectF> GetQuadPointsAsRect(Annotation*);
std::string_view Contents(Annotation*);
int PopupId(Annotation*); // -1 if not exist
time_t CreationDate(Annotation*);
time_t ModificationDate(Annotation*);
std::string_view IconName(Annotation*); // empty() if no icon
std::string_view DefaultAppearanceTextFont(Annotation*);
int DefaultAppearanceTextSize(Annotation*);
COLORREF DefaultAppearanceTextColor(Annotation*);
void GetLineEndingStyles(Annotation*, int* start, int* end);
int Opacity(Annotation*);
int BorderWidth(Annotation*);
void Delete(Annotation*);

std::string_view AnnotationName(AnnotationType);
std::string_view AnnotationReadableName(AnnotationType);
bool IsAnnotationEq(Annotation* a1, Annotation* a2);

Vec<Annotation*> FilterAnnotationsForPage(Vec<Annotation*>* annots, int pageNo);
