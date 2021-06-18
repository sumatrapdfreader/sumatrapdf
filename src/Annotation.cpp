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
    fz_context* ctx{nullptr};
    // must protect mupdf calls because we might be e.g. rendering
    // a page in a separate thread
    CRITICAL_SECTION* ctxAccess{nullptr};
    pdf_annot* annot{nullptr};
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

AnnotationType Type(Annotation* annot) {
    CrashIf((int)annot->type < 0);
    return annot->type;
}

int PageNo(Annotation* annot) {
    CrashIf(annot->pageNo < 0);
    return annot->pageNo;
}

RectF GetRect(Annotation* annot) {
    auto pdf = annot->pdf;
    ScopedCritSec cs(pdf->ctxAccess);

    fz_rect rc = pdf_annot_rect(pdf->ctx, pdf->annot);
    auto rect = ToRectFl(rc);
    return rect;
}

void SetRect(Annotation* a, RectF r) {
    auto pdf = a->pdf;
    ScopedCritSec cs(pdf->ctxAccess);

    fz_rect rc = To_fz_rect(r);
    pdf_set_annot_rect(pdf->ctx, pdf->annot, rc);
    pdf_update_appearance(pdf->ctx, pdf->annot);
    a->isChanged = true;
}

std::string_view Author(Annotation* annot) {
    auto pdf = annot->pdf;
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

int Quadding(Annotation* annot) {
    auto pdf = annot->pdf;
    ScopedCritSec cs(pdf->ctxAccess);
    return pdf_annot_quadding(pdf->ctx, pdf->annot);
}

static bool IsValidQuadding(int i) {
    return i >= 0 && i <= 2;
}

// return true if changed
bool SetQuadding(Annotation* annot, int newQuadding) {
    auto pdf = annot->pdf;
    ScopedCritSec cs(pdf->ctxAccess);
    CrashIf(!IsValidQuadding(newQuadding));
    bool didChange = Quadding(annot) != newQuadding;
    if (!didChange) {
        return false;
    }
    pdf_set_annot_quadding(pdf->ctx, pdf->annot, newQuadding);
    pdf_update_appearance(pdf->ctx, pdf->annot);
    annot->isChanged = true;
    return true;
}

void SetQuadPointsAsRect(Annotation* annot, const Vec<RectF>& rects) {
    auto pdf = annot->pdf;
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
    annot->isChanged = true;
}

Vec<RectF> GetQuadPointsAsRect(Annotation* annot) {
    auto pdf = annot->pdf;
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

std::string_view Contents(Annotation* annot) {
    auto pdf = annot->pdf;
    ScopedCritSec cs(pdf->ctxAccess);
    const char* s = pdf_annot_contents(pdf->ctx, pdf->annot);
    return s;
}

bool SetContents(Annotation* annot, std::string_view sv) {
    auto pdf = annot->pdf;
    std::string_view currValue = Contents(annot);
    if (str::Eq(sv, currValue.data())) {
        return false;
    }
    ScopedCritSec cs(pdf->ctxAccess);
    pdf_set_annot_contents(pdf->ctx, pdf->annot, sv.data());
    pdf_update_appearance(pdf->ctx, pdf->annot);
    annot->isChanged = true;
    return true;
}

void Delete(Annotation* annot) {
    auto pdf = annot->pdf;
    CrashIf(annot->isDeleted);
    ScopedCritSec cs(pdf->ctxAccess);
    pdf_page* page = pdf_annot_page(pdf->ctx, pdf->annot);
    pdf_delete_annot(pdf->ctx, page, pdf->annot);
    annot->isDeleted = true;
    annot->isChanged = true; // TODO: not sure I need this
}

// -1 if not exist
int PopupId(Annotation* annot) {
    auto pdf = annot->pdf;
    ScopedCritSec cs(pdf->ctxAccess);
    pdf_obj* obj = pdf_dict_get(pdf->ctx, pdf_annot_obj(pdf->ctx, pdf->annot), PDF_NAME(Popup));
    if (!obj) {
        return -1;
    }
    int res = pdf_to_num(pdf->ctx, obj);
    return res;
}

time_t CreationDate(Annotation* annot) {
    auto pdf = annot->pdf;
    ScopedCritSec cs(pdf->ctxAccess);
    auto res = pdf_annot_creation_date(pdf->ctx, pdf->annot);
    return res;
}

time_t ModificationDate(Annotation* annot) {
    auto pdf = annot->pdf;
    ScopedCritSec cs(pdf->ctxAccess);
    auto res = pdf_annot_modification_date(pdf->ctx, pdf->annot);
    return res;
}

// return empty() if no icon
std::string_view IconName(Annotation* annot) {
    auto pdf = annot->pdf;
    ScopedCritSec cs(pdf->ctxAccess);
    bool hasIcon = pdf_annot_has_icon_name(pdf->ctx, pdf->annot);
    if (!hasIcon) {
        return {};
    }
    // can only call if pdf_annot_has_icon_name() returned true
    const char* iconName = pdf_annot_icon_name(pdf->ctx, pdf->annot);
    return {iconName};
}

void SetIconName(Annotation* annot, std::string_view iconName) {
    auto pdf = annot->pdf;
    ScopedCritSec cs(pdf->ctxAccess);
    pdf_set_annot_icon_name(pdf->ctx, pdf->annot, iconName.data());
    pdf_update_appearance(pdf->ctx, pdf->annot);
    // TODO: only if the value changed
    annot->isChanged = true;
}

// ColorUnset if no color
COLORREF GetColor(Annotation* annot) {
    auto pdf = annot->pdf;
    ScopedCritSec cs(pdf->ctxAccess);
    float color[4];
    int n;
    pdf_annot_color(pdf->ctx, pdf->annot, &n, color);
    COLORREF res = FromPdfColor(pdf->ctx, n, color);
    return res;
}

// return true if color changed
bool SetColor(Annotation* annot, COLORREF c) {
    auto pdf = annot->pdf;
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
        annot->isChanged = true;
    }
    return didChange;
}

// ColorUnset if no color
COLORREF InteriorColor(Annotation* annot) {
    auto pdf = annot->pdf;
    ScopedCritSec cs(pdf->ctxAccess);
    float color[4];
    int n;
    pdf_annot_interior_color(pdf->ctx, pdf->annot, &n, color);
    COLORREF res = FromPdfColor(pdf->ctx, n, color);
    return res;
}

bool SetInteriorColor(Annotation* annot, COLORREF c) {
    auto pdf = annot->pdf;
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
        annot->isChanged = true;
    }
    return didChange;
}

std::string_view DefaultAppearanceTextFont(Annotation* annot) {
    auto pdf = annot->pdf;
    ScopedCritSec cs(pdf->ctxAccess);
    const char* fontName;
    float sizeF{0.0};
    int n{0};
    float textColor[3];
    pdf_annot_default_appearance(pdf->ctx, pdf->annot, &fontName, &sizeF, &n, textColor);
    return fontName;
}

void SetDefaultAppearanceTextFont(Annotation* annot, std::string_view sv) {
    auto pdf = annot->pdf;
    ScopedCritSec cs(pdf->ctxAccess);
    const char* fontName{nullptr};
    float sizeF{0.0};
    int n{0};
    float textColor[3];
    pdf_annot_default_appearance(pdf->ctx, pdf->annot, &fontName, &sizeF, &n, textColor);
    pdf_set_annot_default_appearance(pdf->ctx, pdf->annot, sv.data(), sizeF, n, textColor);
    pdf_update_appearance(pdf->ctx, pdf->annot);
    annot->isChanged = true;
}

int DefaultAppearanceTextSize(Annotation* annot) {
    auto pdf = annot->pdf;
    ScopedCritSec cs(pdf->ctxAccess);
    const char* fontName{nullptr};
    float sizeF{0.0};
    int n{0};
    float textColor[3];
    pdf_annot_default_appearance(pdf->ctx, pdf->annot, &fontName, &sizeF, &n, textColor);
    return (int)sizeF;
}

void SetDefaultAppearanceTextSize(Annotation* annot, int textSize) {
    auto pdf = annot->pdf;
    ScopedCritSec cs(pdf->ctxAccess);
    const char* fontName{nullptr};
    float sizeF{0.0};
    int n{0};
    float textColor[3];
    pdf_annot_default_appearance(pdf->ctx, pdf->annot, &fontName, &sizeF, &n, textColor);
    pdf_set_annot_default_appearance(pdf->ctx, pdf->annot, fontName, (float)textSize, n, textColor);
    pdf_update_appearance(pdf->ctx, pdf->annot);
    annot->isChanged = true;
}

COLORREF DefaultAppearanceTextColor(Annotation* annot) {
    auto pdf = annot->pdf;
    ScopedCritSec cs(pdf->ctxAccess);
    const char* fontName{nullptr};
    float sizeF{0.0};
    int n{0};
    float textColor[3];
    pdf_annot_default_appearance(pdf->ctx, pdf->annot, &fontName, &sizeF, &n, textColor);
    COLORREF res = FromPdfColor(pdf->ctx, 3, textColor);
    return res;
}

void SetDefaultAppearanceTextColor(Annotation* annot, COLORREF col) {
    auto pdf = annot->pdf;
    ScopedCritSec cs(pdf->ctxAccess);
    const char* fontName{nullptr};
    float sizeF{0.0};
    int n{0};
    float textColor[4];
    pdf_annot_default_appearance(pdf->ctx, pdf->annot, &fontName, &sizeF, &n, textColor);
    ToPdfRgba(col, textColor);
    pdf_set_annot_default_appearance(pdf->ctx, pdf->annot, fontName, sizeF, n, textColor);
    pdf_update_appearance(pdf->ctx, pdf->annot);
    annot->isChanged = true;
}

void GetLineEndingStyles(Annotation* annot, int* start, int* end) {
    auto pdf = annot->pdf;
    ScopedCritSec cs(pdf->ctxAccess);
    pdf_line_ending leStart = PDF_ANNOT_LE_NONE;
    pdf_line_ending leEnd = PDF_ANNOT_LE_NONE;
    pdf_annot_line_ending_styles(pdf->ctx, pdf->annot, &leStart, &leEnd);
    *start = (int)leStart;
    *end = (int)leEnd;
}

void SetLineEndingStyles(Annotation* annot, int start, int end) {
    auto pdf = annot->pdf;
    ScopedCritSec cs(pdf->ctxAccess);
    pdf_line_ending leStart = (pdf_line_ending)start;
    pdf_line_ending leEnd = (pdf_line_ending)end;
    pdf_set_annot_line_ending_styles(pdf->ctx, pdf->annot, leStart, leEnd);
    pdf_update_appearance(pdf->ctx, pdf->annot);
    annot->isChanged = true;
}

int BorderWidth(Annotation* annot) {
    auto pdf = annot->pdf;
    ScopedCritSec cs(pdf->ctxAccess);
    float res = pdf_annot_border(pdf->ctx, pdf->annot);
    return (int)res;
}

void SetBorderWidth(Annotation* annot, int newWidth) {
    auto pdf = annot->pdf;
    ScopedCritSec cs(pdf->ctxAccess);
    pdf_set_annot_border(pdf->ctx, pdf->annot, (float)newWidth);
    pdf_update_appearance(pdf->ctx, pdf->annot);
    annot->isChanged = true;
}

int Opacity(Annotation* annot) {
    auto pdf = annot->pdf;
    ScopedCritSec cs(pdf->ctxAccess);
    float fopacity = pdf_annot_opacity(pdf->ctx, pdf->annot);
    int res = (int)(fopacity * 255.f);
    return res;
}

void SetOpacity(Annotation* annot, int newOpacity) {
    auto pdf = annot->pdf;
    ScopedCritSec cs(pdf->ctxAccess);
    CrashIf(newOpacity < 0 || newOpacity > 255);
    newOpacity = std::clamp(newOpacity, 0, 255);
    float fopacity = (float)newOpacity / 255.f;

    pdf_set_annot_opacity(pdf->ctx, pdf->annot, fopacity);
    pdf_update_appearance(pdf->ctx, pdf->annot);
    annot->isChanged = true;
}

Annotation* MakeAnnotationPdf(CRITICAL_SECTION* ctxAccess, fz_context* ctx, pdf_annot* annot, int pageNo) {
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
        if (PageNo(annot) != pageNo) {
            continue;
        }
        // include all annotations for pageNo that can be rendered by fz_run_user_annots
        switch (Type(annot)) {
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
