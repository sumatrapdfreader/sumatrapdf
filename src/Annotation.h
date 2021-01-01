/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern "C" struct pdf_annot;
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

    AnnotationType Type() const;
    int PageNo() const;
    // note: page no can't be changed

    RectF Rect() const;
    void SetRect(RectF);

    COLORREF Color(); // ColorUnset if no color
    bool SetColor(COLORREF);

    COLORREF InteriorColor(); // ColorUnset if no color
    bool SetInteriorColor(COLORREF);

    std::string_view Author();

    int Quadding();
    bool SetQuadding(int);

    void SetQuadPointsAsRect(const Vec<RectF>&);
    Vec<RectF> GetQuadPointsAsRect();

    std::string_view Contents();
    bool SetContents(std::string_view sv);

    int PopupId(); // -1 if not exist
    time_t CreationDate();
    time_t ModificationDate();

    std::string_view IconName(); // empty() if no icon
    void SetIconName(std::string_view);

    std::string_view DefaultAppearanceTextFont();
    void SetDefaultAppearanceTextFont(std::string_view);

    int DefaultAppearanceTextSize();
    void SetDefaultAppearanceTextSize(int);

    COLORREF DefaultAppearanceTextColor();
    void SetDefaultAppearanceTextColor(COLORREF);

    void GetLineEndingStyles(int* start, int* end);
    void SetLineEndingStyles(int start, int end);

    int Opacity();
    void SetOpacity(int);

    int BorderWidth();
    void SetBorderWidth(int);

    void Delete();
};

std::string_view AnnotationName(AnnotationType);
std::string_view AnnotationReadableName(AnnotationType);
bool IsAnnotationEq(Annotation* a1, Annotation* a2);

void DeleteVecAnnotations(Vec<Annotation*>* annots);
Vec<Annotation*> FilterAnnotationsForPage(Vec<Annotation*>* annots, int pageNo);
