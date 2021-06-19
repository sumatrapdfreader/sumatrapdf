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
#include "EnginePdfImpl.h"

/*
void SetLineEndingStyles(Annotation*, int start, int end);

Vec<RectF> GetQuadPointsAsRect(Annotation*);
time_t CreationDate(Annotation*);

std::string_view AnnotationName(AnnotationType);
*/

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

/*
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
*/

// return argb
PdfColor MkPdfColor(u8 r, u8 g, u8 b, u8 a) {
    PdfColor b2 = (PdfColor)b;
    PdfColor g2 = (PdfColor)g << 8;
    PdfColor r2 = (PdfColor)r << 16;
    PdfColor a2 = (PdfColor)a << 24;
    return a2 | r2 | g2 | b2;
}

// argb
void UnpackPdfColor(PdfColor c, u8& r, u8& g, u8& b, u8& a) {
    b = (u8)(c & 0xff);
    c = c >> 8;
    g = (u8)(c & 0xff);
    c = c >> 8;
    r = (u8)(c & 0xff);
    c = c >> 8;
    a = (u8)(c & 0xff);
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

bool IsAnnotationEq(Annotation* a1, Annotation* a2) {
    if (a1 == a2) {
        return true;
    }
    return a1->pdfannot == a2->pdfannot;
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
    EnginePdf* e = annot->engine;
    ScopedCritSec cs(e->ctxAccess);

    fz_rect rc = pdf_annot_rect(e->ctx, annot->pdfannot);
    auto rect = ToRectFl(rc);
    return rect;
}

void SetRect(Annotation* annot, RectF r) {
    EnginePdf* e = annot->engine;
    ScopedCritSec cs(e->ctxAccess);

    fz_rect rc = To_fz_rect(r);
    pdf_set_annot_rect(e->ctx, annot->pdfannot, rc);
    pdf_update_annot(e->ctx, annot->pdfannot);
    annot->isChanged = true;
}

std::string_view Author(Annotation* annot) {
    EnginePdf* e = annot->engine;
    ScopedCritSec cs(e->ctxAccess);

    const char* s = nullptr;

    fz_var(s);
    fz_try(e->ctx) {
        s = pdf_annot_author(e->ctx, annot->pdfannot);
    }
    fz_catch(e->ctx) {
        s = nullptr;
    }
    if (!s || str::IsStringEmptyOrWhiteSpaceOnly(s)) {
        return {};
    }
    return s;
}

int Quadding(Annotation* annot) {
    EnginePdf* e = annot->engine;
    ScopedCritSec cs(e->ctxAccess);
    return pdf_annot_quadding(e->ctx, annot->pdfannot);
}

static bool IsValidQuadding(int i) {
    return i >= 0 && i <= 2;
}

// return true if changed
bool SetQuadding(Annotation* annot, int newQuadding) {
    EnginePdf* e = annot->engine;
    ScopedCritSec cs(e->ctxAccess);
    CrashIf(!IsValidQuadding(newQuadding));
    bool didChange = Quadding(annot) != newQuadding;
    if (!didChange) {
        return false;
    }
    pdf_set_annot_quadding(e->ctx, annot->pdfannot, newQuadding);
    pdf_update_annot(e->ctx, annot->pdfannot);
    annot->isChanged = true;
    return true;
}

void SetQuadPointsAsRect(Annotation* annot, const Vec<RectF>& rects) {
    EnginePdf* e = annot->engine;
    ScopedCritSec cs(e->ctxAccess);
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
    pdf_clear_annot_quad_points(e->ctx, annot->pdfannot);
    pdf_set_annot_quad_points(e->ctx, annot->pdfannot, n, quads);
    pdf_update_annot(e->ctx, annot->pdfannot);
    annot->isChanged = true;
}

/*
Vec<RectF> GetQuadPointsAsRect(Annotation* annot) {
    EnginePdf* e = annot->engine;
    auto pdf = annot->pdf;
    ScopedCritSec cs(e->ctxAccess);
    Vec<RectF> res;
    int n = pdf_annot_quad_point_count(e->ctx, annot->pdfannot);
    for (int i = 0; i < n; i++) {
        fz_quad q = pdf_annot_quad_point(e->ctx, annot->pdfannot, i);
        fz_rect r = fz_rect_from_quad(q);
        RectF rect = ToRectFl(r);
        res.Append(rect);
    }
    return res;
}
*/

std::string_view Contents(Annotation* annot) {
    EnginePdf* e = annot->engine;
    ScopedCritSec cs(e->ctxAccess);
    const char* s = pdf_annot_contents(e->ctx, annot->pdfannot);
    return s;
}

bool SetContents(Annotation* annot, std::string_view sv) {
    EnginePdf* e = annot->engine;
    std::string_view currValue = Contents(annot);
    if (str::Eq(sv, currValue.data())) {
        return false;
    }
    ScopedCritSec cs(e->ctxAccess);
    pdf_set_annot_contents(e->ctx, annot->pdfannot, sv.data());
    pdf_update_annot(e->ctx, annot->pdfannot);
    annot->isChanged = true;
    return true;
}

void Delete(Annotation* annot) {
    EnginePdf* e = annot->engine;
    CrashIf(annot->isDeleted);
    ScopedCritSec cs(e->ctxAccess);
    pdf_page* page = pdf_annot_page(e->ctx, annot->pdfannot);
    pdf_delete_annot(e->ctx, page, annot->pdfannot);
    annot->isDeleted = true;
    annot->isChanged = true; // TODO: not sure I need this
}

// -1 if not exist
int PopupId(Annotation* annot) {
    EnginePdf* e = annot->engine;
    ScopedCritSec cs(e->ctxAccess);
    pdf_obj* obj = pdf_dict_get(e->ctx, pdf_annot_obj(e->ctx, annot->pdfannot), PDF_NAME(Popup));
    if (!obj) {
        return -1;
    }
    int res = pdf_to_num(e->ctx, obj);
    return res;
}

/*
time_t CreationDate(Annotation* annot) {
    EnginePdf* e = annot->engine;
    auto pdf = annot->pdf;
    ScopedCritSec cs(e->ctxAccess);
    auto res = pdf_annot_creation_date(e->ctx, annot->pdfannot);
    return res;
}
*/

time_t ModificationDate(Annotation* annot) {
    EnginePdf* e = annot->engine;
    ScopedCritSec cs(e->ctxAccess);
    auto res = pdf_annot_modification_date(e->ctx, annot->pdfannot);
    return res;
}

// return empty() if no icon
std::string_view IconName(Annotation* annot) {
    EnginePdf* e = annot->engine;
    ScopedCritSec cs(e->ctxAccess);
    bool hasIcon = pdf_annot_has_icon_name(e->ctx, annot->pdfannot);
    if (!hasIcon) {
        return {};
    }
    // can only call if pdf_annot_has_icon_name() returned true
    const char* iconName = pdf_annot_icon_name(e->ctx, annot->pdfannot);
    return {iconName};
}

void SetIconName(Annotation* annot, std::string_view iconName) {
    EnginePdf* e = annot->engine;
    ScopedCritSec cs(e->ctxAccess);
    pdf_set_annot_icon_name(e->ctx, annot->pdfannot, iconName.data());
    pdf_update_annot(e->ctx, annot->pdfannot);
    // TODO: only if the value changed
    annot->isChanged = true;
}

static void PdfColorToFloat(PdfColor c, float rgb[3]) {
    u8 r, g, b, a;
    UnpackPdfColor(c, r, g, b, a);
    rgb[0] = (float)r / 255.0f;
    rgb[1] = (float)g / 255.0f;
    rgb[2] = (float)b / 255.0f;
}

static float GetOpacityFloat(PdfColor c) {
    u8 alpha = GetAlpha(c);
    return alpha / 255.0f;
}

static PdfColor MkPdfColorFromFloat(float rf, float gf, float bf) {
    u8 r = (u8)(rf * 255.0f);
    u8 g = (u8)(gf * 255.0f);
    u8 b = (u8)(bf * 255.0f);
    return MkPdfColor(r, g, b, 0xff);
}

// n = 1 (grey), 3 (rgb) or 4 (cmyk).
static PdfColor PdfColorFromFloat(fz_context* ctx, int n, float color[4]) {
    if (n == 0) {
        return 0;
    }
    if (n == 1) {
        return MkPdfColorFromFloat(color[0], color[0], color[0]);
    }
    if (n == 3) {
        return MkPdfColorFromFloat(color[0], color[1], color[2]);
    }
    if (n == 4) {
        float rgb[4];
        fz_convert_color(ctx, fz_device_cmyk(ctx), color, fz_device_rgb(ctx), rgb, NULL, fz_default_color_params);
        return MkPdfColorFromFloat(rgb[0], rgb[1], rgb[2]);
    }
    CrashIf(true);
    return 0;
}

PdfColor GetColor(Annotation* annot) {
    EnginePdf* e = annot->engine;
    ScopedCritSec cs(e->ctxAccess);
    float color[4];
    int n;
    pdf_annot_color(e->ctx, annot->pdfannot, &n, color);
    PdfColor res = PdfColorFromFloat(e->ctx, n, color);
    return res;
}

// return true if color changed
bool SetColor(Annotation* annot, PdfColor c) {
    EnginePdf* e = annot->engine;
    ScopedCritSec cs(e->ctxAccess);
    bool didChange = false;
    float color[4];
    int n;
    pdf_annot_color(e->ctx, annot->pdfannot, &n, color);
    float oldOpacity = pdf_annot_opacity(e->ctx, annot->pdfannot);
    float newColor[3];
    PdfColorToFloat(c, newColor);
    float opacity = GetOpacityFloat(c);
    didChange = (n != 3);
    if (!didChange) {
        for (int i = 0; i < n; i++) {
            if (color[i] != newColor[i]) {
                didChange = true;
            }
        }
    }
    if (opacity != oldOpacity) {
        didChange = true;
    }
    if (!didChange) {
        return false;
    }
    if (c == 0) {
        pdf_set_annot_color(e->ctx, annot->pdfannot, 0, newColor);
        // TODO: set opacity to 1?
        // pdf_set_annot_opacity(e->ctx, annot->pdfannot, 1.f);
    } else {
        pdf_set_annot_color(e->ctx, annot->pdfannot, 3, newColor);
        if (oldOpacity != opacity) {
            pdf_set_annot_opacity(e->ctx, annot->pdfannot, opacity);
        }
    }
    pdf_update_annot(e->ctx, annot->pdfannot);
    annot->isChanged = true;
    return true;
}

PdfColor InteriorColor(Annotation* annot) {
    EnginePdf* e = annot->engine;
    ScopedCritSec cs(e->ctxAccess);
    float color[4];
    int n;
    pdf_annot_interior_color(e->ctx, annot->pdfannot, &n, color);
    PdfColor res = PdfColorFromFloat(e->ctx, n, color);
    return res;
}

bool SetInteriorColor(Annotation* annot, PdfColor c) {
    EnginePdf* e = annot->engine;
    ScopedCritSec cs(e->ctxAccess);
    bool didChange = false;
    float color[4];
    int n;
    pdf_annot_color(e->ctx, annot->pdfannot, &n, color);
    float newColor[3];
    PdfColorToFloat(c, newColor);
    didChange = (n != 3);
    if (!didChange) {
        for (int i = 0; i < n; i++) {
            if (color[i] != newColor[i]) {
                didChange = true;
            }
        }
    }
    if (!didChange) {
        return false;
    }
    if (c == 0) {
        pdf_set_annot_interior_color(e->ctx, annot->pdfannot, 0, newColor);
    } else {
        pdf_set_annot_interior_color(e->ctx, annot->pdfannot, 3, newColor);
    }
    pdf_update_annot(e->ctx, annot->pdfannot);
    annot->isChanged = true;
    return true;
}

std::string_view DefaultAppearanceTextFont(Annotation* annot) {
    EnginePdf* e = annot->engine;
    ScopedCritSec cs(e->ctxAccess);
    const char* fontName;
    float sizeF{0.0};
    int n{0};
    float textColor[3];
    pdf_annot_default_appearance(e->ctx, annot->pdfannot, &fontName, &sizeF, &n, textColor);
    return fontName;
}

void SetDefaultAppearanceTextFont(Annotation* annot, std::string_view sv) {
    EnginePdf* e = annot->engine;
    ScopedCritSec cs(e->ctxAccess);
    const char* fontName{nullptr};
    float sizeF{0.0};
    int n{0};
    float textColor[3];
    pdf_annot_default_appearance(e->ctx, annot->pdfannot, &fontName, &sizeF, &n, textColor);
    pdf_set_annot_default_appearance(e->ctx, annot->pdfannot, sv.data(), sizeF, n, textColor);
    pdf_update_annot(e->ctx, annot->pdfannot);
    annot->isChanged = true;
}

int DefaultAppearanceTextSize(Annotation* annot) {
    EnginePdf* e = annot->engine;
    ScopedCritSec cs(e->ctxAccess);
    const char* fontName{nullptr};
    float sizeF{0.0};
    int n{0};
    float textColor[3];
    pdf_annot_default_appearance(e->ctx, annot->pdfannot, &fontName, &sizeF, &n, textColor);
    return (int)sizeF;
}

void SetDefaultAppearanceTextSize(Annotation* annot, int textSize) {
    EnginePdf* e = annot->engine;
    ScopedCritSec cs(e->ctxAccess);
    const char* fontName{nullptr};
    float sizeF{0.0};
    int n{0};
    float textColor[3];
    pdf_annot_default_appearance(e->ctx, annot->pdfannot, &fontName, &sizeF, &n, textColor);
    pdf_set_annot_default_appearance(e->ctx, annot->pdfannot, fontName, (float)textSize, n, textColor);
    pdf_update_annot(e->ctx, annot->pdfannot);
    annot->isChanged = true;
}

PdfColor DefaultAppearanceTextColor(Annotation* annot) {
    EnginePdf* e = annot->engine;
    ScopedCritSec cs(e->ctxAccess);
    const char* fontName{nullptr};
    float sizeF{0.0};
    int n{0};
    float textColor[3];
    pdf_annot_default_appearance(e->ctx, annot->pdfannot, &fontName, &sizeF, &n, textColor);
    PdfColor res = PdfColorFromFloat(e->ctx, 3, textColor);
    return res;
}

void SetDefaultAppearanceTextColor(Annotation* annot, PdfColor col) {
    EnginePdf* e = annot->engine;
    ScopedCritSec cs(e->ctxAccess);
    const char* fontName{nullptr};
    float sizeF{0.0};
    int n{0};
    float textColor[3];
    pdf_annot_default_appearance(e->ctx, annot->pdfannot, &fontName, &sizeF, &n, textColor);
    PdfColorToFloat(col, textColor);
    pdf_set_annot_default_appearance(e->ctx, annot->pdfannot, fontName, sizeF, 3, textColor);
    pdf_update_annot(e->ctx, annot->pdfannot);
    annot->isChanged = true;
}

void GetLineEndingStyles(Annotation* annot, int* start, int* end) {
    EnginePdf* e = annot->engine;
    ScopedCritSec cs(e->ctxAccess);
    pdf_line_ending leStart = PDF_ANNOT_LE_NONE;
    pdf_line_ending leEnd = PDF_ANNOT_LE_NONE;
    pdf_annot_line_ending_styles(e->ctx, annot->pdfannot, &leStart, &leEnd);
    *start = (int)leStart;
    *end = (int)leEnd;
}

/*
void SetLineEndingStyles(Annotation* annot, int start, int end) {
    EnginePdf* e = annot->engine;
    ScopedCritSec cs(e->ctxAccess);
    pdf_line_ending leStart = (pdf_line_ending)start;
    pdf_line_ending leEnd = (pdf_line_ending)end;
    pdf_set_annot_line_ending_styles(e->ctx, annot->pdfannot, leStart, leEnd);
    pdf_update_annot(e->ctx, annot->pdfannot);
    annot->isChanged = true;
}
*/

int BorderWidth(Annotation* annot) {
    EnginePdf* e = annot->engine;
    ScopedCritSec cs(e->ctxAccess);
    float res = pdf_annot_border(e->ctx, annot->pdfannot);
    return (int)res;
}

void SetBorderWidth(Annotation* annot, int newWidth) {
    EnginePdf* e = annot->engine;
    ScopedCritSec cs(e->ctxAccess);
    pdf_set_annot_border(e->ctx, annot->pdfannot, (float)newWidth);
    pdf_update_annot(e->ctx, annot->pdfannot);
    annot->isChanged = true;
}

int Opacity(Annotation* annot) {
    EnginePdf* e = annot->engine;
    ScopedCritSec cs(e->ctxAccess);
    float fopacity = pdf_annot_opacity(e->ctx, annot->pdfannot);
    int res = (int)(fopacity * 255.f);
    return res;
}

void SetOpacity(Annotation* annot, int newOpacity) {
    EnginePdf* e = annot->engine;
    ScopedCritSec cs(e->ctxAccess);
    CrashIf(newOpacity < 0 || newOpacity > 255);
    newOpacity = std::clamp(newOpacity, 0, 255);
    float fopacity = (float)newOpacity / 255.f;

    pdf_set_annot_opacity(e->ctx, annot->pdfannot, fopacity);
    pdf_update_annot(e->ctx, annot->pdfannot);
    annot->isChanged = true;
}

Annotation* MakeAnnotationPdf(EnginePdf* engine, pdf_annot* annot, int pageNo) {
    ScopedCritSec cs(engine->ctxAccess);

    auto tp = pdf_annot_type(engine->ctx, annot);
    AnnotationType typ = AnnotationTypeFromPdfAnnot(tp);
    if (typ == AnnotationType::Unknown) {
        // unsupported type
        return nullptr;
    }

    Annotation* res = new Annotation();
    res->engine = engine;
    res->pageNo = pageNo;
    res->pdfannot = annot;
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
