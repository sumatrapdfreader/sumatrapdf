/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
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
    "Text\0"
    "Link\0"
    "FreeText\0"
    "Line\0"
    "Square\0"
    "Circle\0"
    "Polygon\0"
    "PolyLine\0"
    "Highlight\0"
    "Underline\0"
    "Squiggly\0"
    "StrikeOut\0"
    "Redact\0"
    "Stamp\0"
    "Caret\0"
    "Ink\0"
    "Popup\0"
    "FileAttachment\0"
    "Sound\0"
    "Movie\0"
    "RichMedia\0"
    "Widget\0"
    "Screen\0"
    "PrinterMark\0"
    "TrapNet\0"
    "Watermark\0"
    "3D\0"
    "Projection\0";

static const char* gAnnotReadableNames =
    "Text\0"
    "Link\0"
    "Free Text\0"
    "Line\0"
    "Square\0"
    "Circle\0"
    "Polygon\0"
    "Poly Line\0"
    "Highlight\0"
    "Underline\0"
    "Squiggly\0"
    "StrikeOut\0"
    "Redact\0"
    "Stamp\0"
    "Caret\0"
    "Ink\0"
    "Popup\0"
    "File Attachment\0"
    "Sound\0"
    "Movie\0"
    "RichMedia\0"
    "Widget\0"
    "Screen\0"
    "Printer Mark\0"
    "Trap Net\0"
    "Watermark\0"
    "3D\0"
    "Projection\0";
// clang format-on

std::string_view AnnotationName(AnnotationType tp) {
    int n = (int)tp;
    CrashIf(n < -1 || n > (int)AnnotationType::ThreeD);
    if (n < 0) {
        return "Unknown";
    }
    const char* s = seqstrings::IdxToStr(gAnnotNames, n);
    CrashIf(!s);
    return {s};
}

std::string_view AnnotationReadableName(AnnotationType tp) {
    int n = (int)tp;
    if (n < 0) {
        return "Unknown";
    }
    const char* s = seqstrings::IdxToStr(gAnnotReadableNames, n);
    CrashIf(!s);
    return {s};
}

struct AnnotationPdf {
    fz_context* ctx = nullptr;
    // must protect mupdf calls because we might be e.g. rendering
    // a page in a separate thread
    CRITICAL_SECTION* ctxAccess = nullptr;
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

RectF Annotation::Rect() const {
    ScopedCritSec cs(pdf->ctxAccess);

    fz_rect rc = pdf_annot_rect(pdf->ctx, pdf->annot);
    auto rect = ToRectFl(rc);
    return rect;
}

void Annotation::SetRect(RectF r) {
    ScopedCritSec cs(pdf->ctxAccess);

    fz_rect rc = To_fz_rect(r);
    pdf_set_annot_rect(pdf->ctx, pdf->annot, rc);
    pdf_update_appearance(pdf->ctx, pdf->annot);
    isChanged = true;
}

std::string_view Annotation::Author() {
    ScopedCritSec cs(pdf->ctxAccess);

    const char* s = nullptr;

    fz_var(s);
    fz_try(pdf->ctx) {
        s = pdf_annot_author(pdf->ctx, pdf->annot);
    }
    fz_catch(pdf->ctx) {
        s = nullptr;
    }
    if (!s || str::IsStringEmptyOrWhiteSpaceOnly(s)) {
        return {};
    }
    return s;
}

int Annotation::Quadding() {
    ScopedCritSec cs(pdf->ctxAccess);
    return pdf_annot_quadding(pdf->ctx, pdf->annot);
}

static bool IsValidQuadding(int i) {
    return i >= 0 && i <= 2;
}

// return true if changed
bool Annotation::SetQuadding(int newQuadding) {
    ScopedCritSec cs(pdf->ctxAccess);
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

void Annotation::SetQuadPointsAsRect(const Vec<RectF>& rects) {
    ScopedCritSec cs(pdf->ctxAccess);
    fz_quad quads[512];
    int n = rects.isize();
    if (n == 0) {
        return;
    }
    constexpr int kMaxQuads = (int)dimof(quads);
    for (int i = 0; i < n && i < kMaxQuads; i++) {
        RectF rect = rects[i];
        fz_rect r = To_fz_rect(rect);
        fz_quad q = fz_quad_from_rect(r);
        quads[i] = q;
    }
    pdf_clear_annot_quad_points(pdf->ctx, pdf->annot);
    pdf_set_annot_quad_points(pdf->ctx, pdf->annot, n, quads);
    pdf_update_appearance(pdf->ctx, pdf->annot);
    isChanged = true;
}

Vec<RectF> Annotation::GetQuadPointsAsRect() {
    ScopedCritSec cs(pdf->ctxAccess);
    Vec<RectF> res;
    int n = pdf_annot_quad_point_count(pdf->ctx, pdf->annot);
    for (int i = 0; i < n; i++) {
        fz_quad q = pdf_annot_quad_point(pdf->ctx, pdf->annot, i);
        fz_rect r = fz_rect_from_quad(q);
        RectF rect = ToRectFl(r);
        res.Append(rect);
    }
    return res;
}

std::string_view Annotation::Contents() {
    ScopedCritSec cs(pdf->ctxAccess);
    const char* s = pdf_annot_contents(pdf->ctx, pdf->annot);
    return s;
}

bool Annotation::SetContents(std::string_view sv) {
    std::string_view currValue = Contents();
    if (str::Eq(sv, currValue.data())) {
        return false;
    }
    isChanged = true;
    ScopedCritSec cs(pdf->ctxAccess);
    pdf_set_annot_contents(pdf->ctx, pdf->annot, sv.data());
    pdf_update_appearance(pdf->ctx, pdf->annot);
    return true;
}

void Annotation::Delete() {
    CrashIf(isDeleted);
    ScopedCritSec cs(pdf->ctxAccess);
    pdf_delete_annot(pdf->ctx, pdf->page, pdf->annot);
    isDeleted = true;
    isChanged = true; // TODO: not sure I need this
}

// -1 if not exist
int Annotation::PopupId() {
    ScopedCritSec cs(pdf->ctxAccess);
    pdf_obj* obj = pdf_dict_get(pdf->ctx, pdf->annot->obj, PDF_NAME(Popup));
    if (!obj) {
        return -1;
    }
    int res = pdf_to_num(pdf->ctx, obj);
    return res;
}

time_t Annotation::CreationDate() {
    ScopedCritSec cs(pdf->ctxAccess);
    auto res = pdf_annot_creation_date(pdf->ctx, pdf->annot);
    return res;
}

time_t Annotation::ModificationDate() {
    ScopedCritSec cs(pdf->ctxAccess);
    auto res = pdf_annot_modification_date(pdf->ctx, pdf->annot);
    return res;
}

// return empty() if no icon
std::string_view Annotation::IconName() {
    ScopedCritSec cs(pdf->ctxAccess);
    bool hasIcon = pdf_annot_has_icon_name(pdf->ctx, pdf->annot);
    if (!hasIcon) {
        return {};
    }
    // can only call if pdf_annot_has_icon_name() returned true
    const char* iconName = pdf_annot_icon_name(pdf->ctx, pdf->annot);
    return {iconName};
}

void Annotation::SetIconName(std::string_view iconName) {
    ScopedCritSec cs(pdf->ctxAccess);
    pdf_set_annot_icon_name(pdf->ctx, pdf->annot, iconName.data());
    pdf_update_appearance(pdf->ctx, pdf->annot);
    // TODO: only if the value changed
    isChanged = true;
}

// ColorUnset if no color
COLORREF Annotation::Color() {
    ScopedCritSec cs(pdf->ctxAccess);
    float color[4];
    int n;
    pdf_annot_color(pdf->ctx, pdf->annot, &n, color);
    COLORREF res = FromPdfColor(pdf->ctx, n, color);
    return res;
}

// return true if color changed
bool Annotation::SetColor(COLORREF c) {
    ScopedCritSec cs(pdf->ctxAccess);
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
    ScopedCritSec cs(pdf->ctxAccess);
    float color[4];
    int n;
    pdf_annot_interior_color(pdf->ctx, pdf->annot, &n, color);
    COLORREF res = FromPdfColor(pdf->ctx, n, color);
    return res;
}

bool Annotation::SetInteriorColor(COLORREF c) {
    ScopedCritSec cs(pdf->ctxAccess);
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
    ScopedCritSec cs(pdf->ctxAccess);
    const char* fontName;
    float sizeF;
    float textColor[3];
    pdf_annot_default_appearance(pdf->ctx, pdf->annot, &fontName, &sizeF, textColor);
    return fontName;
}

void Annotation::SetDefaultAppearanceTextFont(std::string_view sv) {
    ScopedCritSec cs(pdf->ctxAccess);
    const char* fontName;
    float sizeF;
    float textColor[3];
    pdf_annot_default_appearance(pdf->ctx, pdf->annot, &fontName, &sizeF, textColor);
    pdf_set_annot_default_appearance(pdf->ctx, pdf->annot, sv.data(), sizeF, textColor);
    pdf_update_appearance(pdf->ctx, pdf->annot);
    isChanged = true;
}

int Annotation::DefaultAppearanceTextSize() {
    ScopedCritSec cs(pdf->ctxAccess);
    const char* fontName;
    float sizeF;
    float textColor[3];
    pdf_annot_default_appearance(pdf->ctx, pdf->annot, &fontName, &sizeF, textColor);
    return (int)sizeF;
}

void Annotation::SetDefaultAppearanceTextSize(int textSize) {
    ScopedCritSec cs(pdf->ctxAccess);
    const char* fontName;
    float sizeF;
    float textColor[3];
    pdf_annot_default_appearance(pdf->ctx, pdf->annot, &fontName, &sizeF, textColor);
    pdf_set_annot_default_appearance(pdf->ctx, pdf->annot, fontName, (float)textSize, textColor);
    pdf_update_appearance(pdf->ctx, pdf->annot);
    isChanged = true;
}

COLORREF Annotation::DefaultAppearanceTextColor() {
    ScopedCritSec cs(pdf->ctxAccess);
    const char* fontName;
    float sizeF;
    float textColor[3];
    pdf_annot_default_appearance(pdf->ctx, pdf->annot, &fontName, &sizeF, textColor);
    COLORREF res = FromPdfColor(pdf->ctx, 3, textColor);
    return res;
}

void Annotation::SetDefaultAppearanceTextColor(COLORREF col) {
    ScopedCritSec cs(pdf->ctxAccess);
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
    ScopedCritSec cs(pdf->ctxAccess);
    pdf_line_ending leStart = PDF_ANNOT_LE_NONE;
    pdf_line_ending leEnd = PDF_ANNOT_LE_NONE;
    pdf_annot_line_ending_styles(pdf->ctx, pdf->annot, &leStart, &leEnd);
    *start = (int)leStart;
    *end = (int)leEnd;
}

void Annotation::SetLineEndingStyles(int start, int end) {
    ScopedCritSec cs(pdf->ctxAccess);
    pdf_line_ending leStart = (pdf_line_ending)start;
    pdf_line_ending leEnd = (pdf_line_ending)end;
    pdf_set_annot_line_ending_styles(pdf->ctx, pdf->annot, leStart, leEnd);
    pdf_update_appearance(pdf->ctx, pdf->annot);
    isChanged = true;
}

int Annotation::BorderWidth() {
    ScopedCritSec cs(pdf->ctxAccess);
    float res = pdf_annot_border(pdf->ctx, pdf->annot);
    return (int)res;
}

void Annotation::SetBorderWidth(int newWidth) {
    ScopedCritSec cs(pdf->ctxAccess);
    pdf_set_annot_border(pdf->ctx, pdf->annot, (float)newWidth);
    pdf_update_appearance(pdf->ctx, pdf->annot);
    isChanged = true;
}

int Annotation::Opacity() {
    ScopedCritSec cs(pdf->ctxAccess);
    float fopacity = pdf_annot_opacity(pdf->ctx, pdf->annot);
    int res = (int)(fopacity * 255.f);
    return res;
}

void Annotation::SetOpacity(int newOpacity) {
    ScopedCritSec cs(pdf->ctxAccess);
    CrashIf(newOpacity < 0 || newOpacity > 255);
    newOpacity = std::clamp(newOpacity, 0, 255);
    float fopacity = (float)newOpacity / 255.f;

    pdf_set_annot_opacity(pdf->ctx, pdf->annot, fopacity);
    pdf_update_appearance(pdf->ctx, pdf->annot);
    isChanged = true;
}

Annotation* MakeAnnotationPdf(CRITICAL_SECTION* ctxAccess, fz_context* ctx, pdf_page* page, pdf_annot* annot,
                              int pageNo) {
    ScopedCritSec cs(ctxAccess);

    auto tp = pdf_annot_type(ctx, annot);
    AnnotationType typ = AnnotationTypeFromPdfAnnot(tp);
    if (typ == AnnotationType::Unknown) {
        // unsupported type
        return nullptr;
    }
    AnnotationPdf* apdf = new AnnotationPdf();
    apdf->ctxAccess = ctxAccess;
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
