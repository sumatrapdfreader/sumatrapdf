/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by AnnotEditToolbarCommon.cpp and each app's AnnotEditToolbar.cpp ---

// what both apps' AnnotEditToolbar start with: the annotation being edited
struct AnnotEditToolbarBase {
    MainWindow* win = nullptr;
    WindowTab* tab = nullptr;
    Annotation* annot = nullptr;
};

enum class AnnotEditKind {
    Color,
    InteriorColor,
    Opacity,
    Border,
    FontName,
    Bold,
    Italic,
    Underline,
    Strikeout,
    TextColor,
    TextSize,
    Alignment,
    VAlign,
    Icon,
    Contents,
    LineStart,
    LineEnd,
    AttachFile,
    SaveAttachment,
    Delete,
};
struct AnnotEditItem {
    AnnotEditKind kind = AnnotEditKind::Color;
    Str tooltip;
    Str text;
    PdfColor color = 0;
    int number = 0;
    int lineEnding = 0;
    bool lineIsStart = false;
    Str iconName;
    bool mupdfIcon = false;
};
bool BarIsDark();
Color BarBg();
Color BarBorderColor();
Color BarTextColor();
Color BarMutedTextColor();
Str KindName(AnnotEditKind kind);
bool AnnotationColorIsBackground(AnnotationType tp);
bool AnnotationBorderInColorChip(AnnotationType tp);
Str ResolvedAnnotIconName(Annotation* annot);
extern SeqStrings gBase14ReadableNames;
int StyleBitForKind(AnnotEditKind kind);
int VAlignOfStyle(int style);
int NextVAlignStyle(int style);
Str FontFamilyLabel(Str family);
void AppendStyleToggle(Vec<AnnotEditItem>& out, AnnotEditKind kind, int style, Str tooltip);
const char* MupdfIconStream(Str name);
// receives an icon glyph's path; points are already transformed
struct MupdfIconPathSink {
    virtual void MoveTo(PointF p) = 0;
    virtual void LineTo(PointF p) = 0;
    virtual void CurveTo(PointF p1, PointF p2, PointF p3) = 0;
    // corners in order: (x,y), (x+w,y), (x+w,y+h), (x,y+h)
    virtual void AddRect(PointF a, PointF b, PointF c, PointF d) = 0;
    virtual void Close() = 0;
    virtual ~MupdfIconPathSink() = default;
};
void ParseMupdfIconPath(MupdfIconPathSink& sink, const char* s);
Color BarActiveBg();
void AnnotChanged(WindowTab* tab);
extern SeqStrings gLineEndingStyles;
struct AnnotationHoverRows {
    StrVec keys;
    StrVec labels;
    StrVec values;

    void Add(Str key, Str label, Str value) {
        keys.Append(key);
        labels.Append(label);
        values.Append(value);
    }
};
void CollectAnnotationHoverRows(Annotation* annot, AnnotationHoverRows& rows);
Color AnnotationHoverBg();
Color AnnotationHoverText();
bool SameRectF(RectF a, RectF b);
void OnAnnotsProgress(WindowTab* tab);
TempStr ColorDumpTemp(PdfColor c);

Color PdfToWinColor(PdfColor c);
u8 PdfColorAlpha(PdfColor c);
Color PdfToWinColorWithAlpha(PdfColor c);
PdfColor WinToPdfColor(Color c);
PdfColor OpaquePdfColor(PdfColor c);
void CollectItems(Annotation* annot, Vec<AnnotEditItem>& out);
TempStr ChipLabelTemp(const AnnotEditItem& item);
void ScheduleShowSelectedAnnotationView(WindowTab* tab);
void CollectPriorityAnnotPages(WindowTab* tab, Annotation* extra, Vec<int>& pages);

// implemented by each app
void ShowSelectedAnnotationView(WindowTab* tab);
Rect CanvasToFramePx(MainWindow* win, Rect r);
bool AnnotResizeRerenderPending(MainWindow* win);
void DetachAnnotationFromEditors(Annotation* annot);

void ChipThicknessPicked(AnnotEditToolbarBase* tb, int width);
void ChipOpacityPicked(AnnotEditToolbarBase* tb, int percent);
void ChipTextSizePicked(AnnotEditToolbarBase* tb, int size);
Annotation* LiveToolbarAnnot(AnnotEditToolbarBase* tb);
AnnotEditKind ChipColorKind(AnnotEditToolbarBase* tb);
void ChipColorPicked(AnnotEditToolbarBase* tb, Color col);

constexpr int kBtnPadX = 8;

constexpr int kBtnPadY = 4;

constexpr int kMargin = 5;

constexpr int kBtnGap = 2;

constexpr int kCornerRadius = 10;

constexpr int kButtonRadius = 6;

// ranges of the number sliders; widths and sizes are in PDF points
constexpr int kBorderWidthMax = 12;

constexpr int kFreeTextSizeMin = 6;

constexpr int kFreeTextSizeMax = 72;

constexpr int kOpacityPercentMin = 10;

void PostedStartContentsEdit(MainWindow* win);
