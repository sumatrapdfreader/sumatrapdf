/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// TODO: not quite happy how those functions are split among
// Annotation.cpp, EngineMupdf.cpp and EditAnnotations.cpp

struct Pixmap;

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
    Last = Projection,
    Unknown = -1
};

enum class AnnotationChange {
    Add,
    Remove,
    Modify,
};

class EngineMupdf;
extern "C" struct pdf_annot;

SeqStrings AnnotationTextIcons();

// an user annotation on page
// It abstracts over pdf_annot so that we don't have to
// inlude mupdf to include Annotation
struct Annotation {
    // common to both smx and pdf
    AnnotationType type = AnnotationType::Unknown;
    int pageNo = -1;

    // in page coordinates
    RectF bounds;

    EngineMupdf* engine = nullptr;
    pdf_annot* pdfannot = nullptr; // not owned

    Annotation() = default;
    ~Annotation() = default;
};

struct AnnotCreateArgs {
    AnnotationType annotType = AnnotationType::Unknown;
    // the following are set depending on type of the annotation
    ParsedColor col;
    // bgCol for free text
    ParsedColor bgCol;
    // interior color (fill) for shapes like Square, Circle, Line
    ParsedColor interiorCol;
    // opacity for free text, 0-100, 0-fully transparent (invisible), 100-fully opaque
    // if 100 we don't actually set it (it's the default)
    int opacity = 100;
    bool copyToClipboard = false;
    // for free text, < 0 means not given
    int textSize = -1;
    // for free text, < 0 means not given
    int borderWidth = -1;
    // for free text, PDF /Q text alignment, < 0 means not given (MuPDF's left)
    int quadding = -1;
    bool setContentToSelection = false;
    Str content;
    Pixmap* stampImage = nullptr;
};

int PageNo(Annotation*);
RectF GetBounds(Annotation*);
RectF GetRect(Annotation*);
void SetRect(Annotation*, RectF);
void SetQuadPointsAsRect(Annotation*, const Vec<RectF>&);

Str Author(Annotation*);
time_t ModificationDate(Annotation*);
int PopupId(Annotation*); // -1 if not exist
Str AnnotationReadableNameTemp(AnnotationType tp);
AnnotationType Type(Annotation*);

Str DefaultAppearanceTextFont(Annotation*);
PdfColor DefaultAppearanceTextColor(Annotation*);
int DefaultAppearanceTextSize(Annotation*);
Str Contents(Annotation*);
PdfColor GetColor(Annotation*);      // kColorUnset if no color
PdfColor InteriorColor(Annotation*); // kColorUnset if no color
// PDF /Q: how free text is aligned in its box ("Text Alignment" in the
// annotation editor). Right is also what right-to-left scripts want.
constexpr int kQuaddingLeft = 0;
constexpr int kQuaddingCenter = 1;
constexpr int kQuaddingRight = 2;
extern SeqStrings gQuaddingNames; // "Left\0Center\0Right\0"

int QuaddingFromName(Str);
int Quadding(Annotation*);
int BorderWidth(Annotation*);
Str IconName(Annotation*); // empty if no icon
int Opacity(Annotation*);
void GetLineEndingStyles(Annotation*, int* start, int* end);

void SetDefaultAppearanceTextFont(Annotation*, Str);
void SetDefaultAppearanceTextSize(Annotation*, int);
void SetDefaultAppearanceTextColor(Annotation*, PdfColor);
bool SetContents(Annotation*, Str);
bool SetColor(Annotation*, PdfColor);
bool SetInteriorColor(Annotation*, PdfColor);
bool SetQuadding(Annotation*, int);
void SetBorderWidth(Annotation*, int);
void SetOpacity(Annotation*, int);
void SetIconName(Annotation*, Str);
void SetLineEndStyles(Annotation*, int end);
void SetLineStartStyles(Annotation*, int start);

int GetWidgetType(Annotation*);
// which mouse cursor a form field warrants on hover. None for non-widgets and
// read-only fields, Text (I-beam) for text/combo/listbox, Button (hand) for
// checkbox/radio.
enum class WidgetCursorKind {
    None,
    Text,
    Button
};
WidgetCursorKind GetWidgetCursorKind(Annotation*);
int GetWidgetFieldFlags(Annotation*);
Str GetWidgetValue(Annotation*);
float GetWidgetFontSize(Annotation*);
int GetWidgetMaxLen(Annotation*);
bool SetWidgetTextValue(Annotation*, Str value);
void GetWidgetChoiceOptions(Annotation*, StrVec& out);
bool SetWidgetChoiceValue(Annotation*, Str value);
bool ToggleFormButton(Annotation*);

bool AnnotationIsLive(Annotation*);

void DeleteAnnotation(Annotation*);
bool AnnotationCanBeMoved(AnnotationType);
bool AnnotationCanBeResized(AnnotationType);
bool AnnotationSupportsColor(AnnotationType);
bool AnnotationSupportsBorder(AnnotationType);
bool AnnotationSupportsInteriorColor(AnnotationType);

AnnotationType CmdIdToAnnotationType(int cmdId);
