/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern "C" {
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
}

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"

#include "TreeModel.h"
#include "Annotation.h"
#include "EngineBase.h"
#include "EngineFzUtil.h"

// spot checks the definitions are the same
static_assert((int)AnnotationType::Link == (int)PDF_ANNOT_LINK);
static_assert((int)AnnotationType::ThreeD == (int)PDF_ANNOT_3D);
static_assert((int)AnnotationType::Sound == (int)PDF_ANNOT_SOUND);
static_assert((int)AnnotationType::Unknown == (int)PDF_ANNOT_UNKNOWN);

AnnotationType AnnotationTypeFromPdfAnnot(enum pdf_annot_type tp) {
    return (AnnotationType)tp;
}

// clang format-off
// must match the order of enum class AnnotationType
static const char* gAnnotNames =
    "Text\0Link\0FreeText\0Line\0Square\0Circle\0Polygon\0PolyLine\0"
    "Highlight\0Underline\0Squiggly\0StrikeOut\0Redact\0Stamp\0"
    "Caret\0Ink\0Popup\0FileAttachment\0Sound\0Movie\0Widget\0"
    "Screen\0PrinterMark\0TrapNet\0Watermark\03D\0";
static const char* gAnnotReadableNames =
    "Text\0Link\0Free Text\0Line\0Square\0Circle\0Polygon\0Poly Line\0"
    "Highlight\0Underline\0Squiggly\0StrikeOut\0Redact\0Stamp\0"
    "Caret\0Ink\0Popup\0File Attachment\0Sound\0Movie\0Widget\0"
    "Screen\0Printer Mark\0Trap Net\0Watermark\03D\0";
// clang format-on

std::string_view AnnotationName(AnnotationType tp) {
    int n = (int)tp;
    CrashIf(n < -1 || n > (int)AnnotationType::ThreeD);
    if (n < 0) {
        return "Unknown";
    }
    const char* s = seqstrings::IdxToStr(gAnnotNames, n);
    return {s};
}

std::string_view AnnotationReadableName(AnnotationType tp) {
    int n = (int)tp;
    if (n < 0) {
        return "Unknown";
    }
    const char* s = seqstrings::IdxToStr(gAnnotReadableNames, n);
    return {s};
}

struct AnnotationPdf {
    // set if constructed from mupdf annotation
    fz_context* ctx = nullptr;
    pdf_page* page = nullptr;
    pdf_annot* annot = nullptr;
};

bool IsAnnotationEq(Annotation* a1, Annotation* a2) {
    if (a1 == a2) {
        return true;
    }
    if (a1->pdf && a2->pdf) {
        return a1->pdf->annot == a2->pdf->annot;
    }
    CrashIf(false);
    return false;
}

Annotation::~Annotation() {
    delete pdf;
}

void DeleteVecAnnotations(Vec<Annotation*>* annots) {
    if (!annots) {
        return;
    }
    DeleteVecMembers(*annots);
    delete annots;
}

AnnotationType Annotation::Type() const {
    CrashIf((int)type < 0);
    return type;
}

int Annotation::PageNo() const {
    CrashIf(pageNo < 0);
    return pageNo;
}

RectFl Annotation::Rect() const {
    fz_rect rc = pdf_annot_rect(pdf->ctx, pdf->annot);
    auto rect = ToRectFl(rc);
    return rect;
}

void Annotation::SetRect(RectFl r) {
    fz_rect rc = To_fz_rect(r);
    pdf_set_annot_rect(pdf->ctx, pdf->annot, rc);
    pdf_update_appearance(pdf->ctx, pdf->annot);
    isChanged = true;
}

std::string_view Annotation::Author() {
    const char* s = pdf_annot_author(pdf->ctx, pdf->annot);
    if (str::IsStringEmptyOrWhiteSpaceOnly(s)) {
        return {};
    }
    return s;
}

int Annotation::Quadding() {
    return pdf_annot_quadding(pdf->ctx, pdf->annot);
}

static bool IsValidQuadding(int i) {
    return i >= 0 && i <= 2;
}

// return true if changed
bool Annotation::SetQuadding(int newQuadding) {
    CrashIf(!IsValidQuadding(newQuadding));
    bool didChange = Quadding() != newQuadding;
    if (!didChange) {
        return false;
    }
    pdf_set_annot_quadding(pdf->ctx, pdf->annot, newQuadding);
    pdf_update_appearance(pdf->ctx, pdf->annot);
    isChanged = true;
    return true;
}

void Annotation::SetQuadPointsAsRect(const Vec<RectFl>& rects) {
    fz_quad quads[512];
    int n = rects.isize();
    if (n == 0) {
        return;
    }
    constexpr int kMaxQuads = (int)dimof(quads);
    for (int i = 0; i < n && i < kMaxQuads; i++) {
        RectFl rect = rects[i];
        fz_rect r = To_fz_rect(rect);
        fz_quad q = fz_quad_from_rect(r);
        quads[i] = q;
    }
    pdf_clear_annot_quad_points(pdf->ctx, pdf->annot);
    pdf_set_annot_quad_points(pdf->ctx, pdf->annot, n, quads);
    pdf_update_appearance(pdf->ctx, pdf->annot);
    isChanged = true;
}

Vec<RectFl> Annotation::GetQuadPointsAsRect() {
    Vec<RectFl> res;
    int n = pdf_annot_quad_point_count(pdf->ctx, pdf->annot);
    for (int i = 0; i < n; i++) {
        fz_quad q = pdf_annot_quad_point(pdf->ctx, pdf->annot, i);
        fz_rect r = fz_rect_from_quad(q);
        RectFl rect = ToRectFl(r);
        res.Append(rect);
    }
    return res;
}

std::string_view Annotation::Contents() {
    const char* s = pdf_annot_contents(pdf->ctx, pdf->annot);
    return s;
}

bool Annotation::SetContents(std::string_view sv) {
    std::string_view currValue = Contents();
    if (str::Eq(sv, currValue.data())) {
        return false;
    }
    isChanged = true;
    pdf_set_annot_contents(pdf->ctx, pdf->annot, sv.data());
    pdf_update_appearance(pdf->ctx, pdf->annot);
    return true;
}

void Annotation::Delete() {
    CrashIf(isDeleted);
    pdf_delete_annot(pdf->ctx, pdf->page, pdf->annot);
    isDeleted = true;
    isChanged = true; // TODO: not sure I need this
}

// -1 if not exist
int Annotation::PopupId() {
    pdf_obj* obj = pdf_dict_get(pdf->ctx, pdf->annot->obj, PDF_NAME(Popup));
    if (!obj) {
        return -1;
    }
    int res = pdf_to_num(pdf->ctx, obj);
    return res;
}

time_t Annotation::CreationDate() {
    auto res = pdf_annot_creation_date(pdf->ctx, pdf->annot);
    return res;
}

time_t Annotation::ModificationDate() {
    auto res = pdf_annot_modification_date(pdf->ctx, pdf->annot);
    return res;
}

// return empty() if no icon
std::string_view Annotation::IconName() {
    bool hasIcon = pdf_annot_has_icon_name(pdf->ctx, pdf->annot);
    if (!hasIcon) {
        return {};
    }
    // can only call if pdf_annot_has_icon_name() returned true
    const char* iconName = pdf_annot_icon_name(pdf->ctx, pdf->annot);
    return {iconName};
}

void Annotation::SetIconName(std::string_view iconName) {
    pdf_set_annot_icon_name(pdf->ctx, pdf->annot, iconName.data());
    pdf_update_appearance(pdf->ctx, pdf->annot);
    // TODO: only if the value changed
    isChanged = true;
}

// ColorUnset if no color
COLORREF Annotation::Color() {
    float color[4];
    int n;
    pdf_annot_color(pdf->ctx, pdf->annot, &n, color);
    COLORREF res = FromPdfColor(pdf->ctx, n, color);
    return res;
}

// return true if color changed
bool Annotation::SetColor(COLORREF c) {
    bool didChange = false;
    float color[4];
    int n;
    pdf_annot_color(pdf->ctx, pdf->annot, &n, color);
    float newColor[4];
    int newN = ToPdfRgba(c, newColor);
    didChange = (n != newN);
    if (!didChange) {
        for (int i = 0; i < n; i++) {
            if (color[i] != newColor[i]) {
                didChange = true;
            }
        }
    }
    if (c == ColorUnset) {
        pdf_set_annot_color(pdf->ctx, pdf->annot, 0, newColor);
    } else {
        pdf_set_annot_color(pdf->ctx, pdf->annot, newN, newColor);
    }
    pdf_update_appearance(pdf->ctx, pdf->annot);
    if (didChange) {
        isChanged = true;
    }
    return didChange;
}

// ColorUnset if no color
COLORREF Annotation::InteriorColor() {
    float color[4];
    int n;
    pdf_annot_interior_color(pdf->ctx, pdf->annot, &n, color);
    COLORREF res = FromPdfColor(pdf->ctx, n, color);
    return res;
}

bool Annotation::SetInteriorColor(COLORREF c) {
    bool didChange = false;
    float color[4];
    int n;
    pdf_annot_interior_color(pdf->ctx, pdf->annot, &n, color);
    float newColor[4];
    int newN = ToPdfRgba(c, newColor);
    didChange = (n != newN);
    if (!didChange) {
        for (int i = 0; i < n; i++) {
            if (color[i] != newColor[i]) {
                didChange = true;
            }
        }
    }
    pdf_set_annot_interior_color(pdf->ctx, pdf->annot, newN, newColor);
    pdf_update_appearance(pdf->ctx, pdf->annot);
    if (didChange) {
        isChanged = true;
    }
    return didChange;
}

std::string_view Annotation::DefaultAppearanceTextFont() {
    const char* fontName;
    float sizeF;
    float textColor[3];
    pdf_annot_default_appearance(pdf->ctx, pdf->annot, &fontName, &sizeF, textColor);
    return fontName;
}

void Annotation::SetDefaultAppearanceTextFont(std::string_view sv) {
    const char* fontName;
    float sizeF;
    float textColor[3];
    pdf_annot_default_appearance(pdf->ctx, pdf->annot, &fontName, &sizeF, textColor);
    pdf_set_annot_default_appearance(pdf->ctx, pdf->annot, sv.data(), sizeF, textColor);
    pdf_update_appearance(pdf->ctx, pdf->annot);
    isChanged = true;
}

int Annotation::DefaultAppearanceTextSize() {
    const char* fontName;
    float sizeF;
    float textColor[3];
    pdf_annot_default_appearance(pdf->ctx, pdf->annot, &fontName, &sizeF, textColor);
    return (int)sizeF;
}

void Annotation::SetDefaultAppearanceTextSize(int textSize) {
    const char* fontName;
    float sizeF;
    float textColor[3];
    pdf_annot_default_appearance(pdf->ctx, pdf->annot, &fontName, &sizeF, textColor);
    pdf_set_annot_default_appearance(pdf->ctx, pdf->annot, fontName, (float)textSize, textColor);
    pdf_update_appearance(pdf->ctx, pdf->annot);
    isChanged = true;
}

COLORREF Annotation::DefaultAppearanceTextColor() {
    const char* fontName;
    float sizeF;
    float textColor[3];
    pdf_annot_default_appearance(pdf->ctx, pdf->annot, &fontName, &sizeF, textColor);
    COLORREF res = FromPdfColor(pdf->ctx, 3, textColor);
    return res;
}

void Annotation::SetDefaultAppearanceTextColor(COLORREF col) {
    const char* text_font;
    float sizeF;
    float textColor[4];
    pdf_annot_default_appearance(pdf->ctx, pdf->annot, &text_font, &sizeF, textColor);
    ToPdfRgba(col, textColor);
    pdf_set_annot_default_appearance(pdf->ctx, pdf->annot, text_font, sizeF, textColor);
    pdf_update_appearance(pdf->ctx, pdf->annot);
    isChanged = true;
}

void Annotation::GetLineEndingStyles(int* start, int* end) {
    pdf_line_ending leStart = PDF_ANNOT_LE_NONE;
    pdf_line_ending leEnd = PDF_ANNOT_LE_NONE;
    pdf_annot_line_ending_styles(pdf->ctx, pdf->annot, &leStart, &leEnd);
    *start = (int)leStart;
    *end = (int)leEnd;
}

void Annotation::SetLineEndingStyles(int start, int end) {
    pdf_line_ending leStart = (pdf_line_ending)start;
    pdf_line_ending leEnd = (pdf_line_ending)end;
    pdf_set_annot_line_ending_styles(pdf->ctx, pdf->annot, leStart, leEnd);
    pdf_update_appearance(pdf->ctx, pdf->annot);
    isChanged = true;
}

int Annotation::BorderWidth() {
    float res = pdf_annot_border(pdf->ctx, pdf->annot);
    return (int)res;
}

void Annotation::SetBorderWidth(int newWidth) {
    pdf_set_annot_border(pdf->ctx, pdf->annot, (float)newWidth);
    pdf_update_appearance(pdf->ctx, pdf->annot);
    isChanged = true;
}

int Annotation::Opacity() {
    float fopacity = pdf_annot_opacity(pdf->ctx, pdf->annot);
    int res = (int)(fopacity * 255.f);
    return res;
}

void Annotation::SetOpacity(int newOpacity) {
    CrashIf(newOpacity < 0 || newOpacity > 255);
    newOpacity = std::clamp(newOpacity, 0, 255);
    float fopacity = (float)newOpacity / 255.f;
    pdf_set_annot_opacity(pdf->ctx, pdf->annot, fopacity);
    pdf_update_appearance(pdf->ctx, pdf->annot);
    isChanged = true;
}

Annotation* MakeAnnotationPdf(fz_context* ctx, pdf_page* page, pdf_annot* annot, int pageNo) {
    auto tp = pdf_annot_type(ctx, annot);
    AnnotationType typ = AnnotationTypeFromPdfAnnot(tp);
    if (typ == AnnotationType::Unknown) {
        // unsupported type
        return nullptr;
    }
    AnnotationPdf* apdf = new AnnotationPdf();
    apdf->ctx = ctx;
    apdf->annot = annot;
    apdf->page = page;

    Annotation* res = new Annotation();
    res->pageNo = pageNo;
    res->pdf = apdf;
    res->type = typ;
    return res;
}

Vec<Annotation*> FilterAnnotationsForPage(Vec<Annotation*>* annots, int pageNo) {
    Vec<Annotation*> result;
    if (!annots) {
        return result;
    }
    for (auto& annot : *annots) {
        if (annot->isDeleted) {
            continue;
        }
        if (annot->PageNo() != pageNo) {
            continue;
        }
        // include all annotations for pageNo that can be rendered by fz_run_user_annots
        switch (annot->Type()) {
            case AnnotationType::Highlight:
            case AnnotationType::Underline:
            case AnnotationType::StrikeOut:
            case AnnotationType::Squiggly:
                result.Append(annot);
                break;
        }
    }
    return result;
}
