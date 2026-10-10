/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by AnnotEditToolbarCommon.cpp and each app's AnnotEditToolbar.cpp ---

enum class AnnotEditKind {
    Color,
    InteriorColor,
    Opacity,
    Border,
    FontName,
    Bold,
    Italic,
    Underline,
    TextColor,
    TextSize,
    Alignment,
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
Str FontFamilyLabel(Str family);
void AppendStyleToggle(Vec<AnnotEditItem>& out, AnnotEditKind kind, int style, Str tooltip);
const char* MupdfIconStream(Str name);
bool IsPdfPathOpChar(char c);
void SkipPdfPathWs(const char*& p);
float PdfPathPop(float* stk, int& top);
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
