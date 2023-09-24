/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern "C" {
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
}

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"

#include "wingui/UIModels.h"

#include "Annotation.h"
#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineMupdf.h"
#include "GlobalPrefs.h"

#include "utils/Log.h"

/*
void SetLineEndingStyles(Annotation*, int start, int end);

Vec<RectF> GetQuadPointsAsRect(Annotation*);
time_t CreationDate(Annotation*);

const char* AnnotationName(AnnotationType);
*/

// spot checks the definitions are the same
static_assert((int)AnnotationType::Link == (int)PDF_ANNOT_LINK);
static_assert((int)AnnotationType::ThreeD == (int)PDF_ANNOT_3D);
static_assert((int)AnnotationType::Sound == (int)PDF_ANNOT_SOUND);
static_assert((int)AnnotationType::Unknown == (int)PDF_ANNOT_UNKNOWN);

// clang-format off
const char* gAnnotationTextIcons = "Comment\0Help\0Insert\0Key\0NewParagraph\0Note\0Paragraph\0";
// clang-format on

// clang format-off

#if 0
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
#endif

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
const char* AnnotationName(AnnotationType tp) {
    int n = (int)tp;
    CrashIf(n < -1 || n > (int)AnnotationType::ThreeD);
    if (n < 0) {
        return "Unknown";
    }
    const char* s = seqstrings::IdxToStr(gAnnotNames, n);
    CrashIf(!s);
    return s;
}
*/

static bool gDebugAnnotDestructor = false;
Annotation::~Annotation() {
    if (gDebugAnnotDestructor) {
        logf("deleting an annotation\n");
    }
}

TempStr AnnotationReadableNameTemp(AnnotationType tp) {
    int n = (int)tp;
    if (n < 0) {
        return (char*)"Unknown";
    }
    char* s = (char*)seqstrings::IdxToStr(gAnnotReadableNames, n);
    CrashIf(!s);
    return s;
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
    CrashIf(annot->pageNo < 1);
    return annot->pageNo;
}

RectF GetBounds(Annotation* annot) {
    EngineMupdf* e = annot->engine;
    auto ctx = e->ctx;
    ScopedCritSec cs(e->ctxAccess);
    fz_rect rc = {};

    fz_try(ctx) {
        rc = pdf_bound_annot(ctx, annot->pdfannot);
    }
    fz_catch(ctx) {
        logf("GetBounds(): pdf_bound_annot() failed\n");
    }
    annot->bounds = ToRectF(rc);
    return annot->bounds;
}

RectF GetRect(Annotation* annot) {
    return annot->bounds;
}

void SetRect(Annotation* annot, RectF r) {
    EngineMupdf* e = annot->engine;
    bool failed = false;
    {
        auto ctx = e->ctx;
        ScopedCritSec cs(e->ctxAccess);
        fz_rect rc = ToFzRect(r);
        fz_try(ctx) {
            pdf_set_annot_rect(ctx, annot->pdfannot, rc);
            pdf_update_annot(ctx, annot->pdfannot);
        }
        fz_catch(ctx) {
            // can happen for non-moveable annotations
            failed = true;
            logf("SetRect(): pdf_set_annot_rect() or pdf_update_annot() failed\n");
        }
    }
    ReportIf(failed);
    if (failed) {
        return;
    }
    annot->bounds = r;
    MarkNotificationAsModified(e, annot);
}

const char* Author(Annotation* annot) {
    EngineMupdf* e = annot->engine;
    auto ctx = e->ctx;
    ScopedCritSec cs(e->ctxAccess);

    const char* s = nullptr;

    fz_var(s);
    fz_try(ctx) {
        s = pdf_annot_author(ctx, annot->pdfannot);
    }
    fz_catch(ctx) {
        s = nullptr;
    }
    if (!s || str::EmptyOrWhiteSpaceOnly(s)) {
        return {};
    }
    return s;
}

int Quadding(Annotation* annot) {
    EngineMupdf* e = annot->engine;
    auto ctx = e->ctx;
    ScopedCritSec cs(e->ctxAccess);
    int res = 0;
    fz_try(ctx) {
        res = pdf_annot_quadding(ctx, annot->pdfannot);
    }
    fz_catch(ctx) {
        logf("Quadding(): pdf_annot_quadding() failed\n");
    }
    return res;
}

static bool IsValidQuadding(int i) {
    return i >= 0 && i <= 2;
}

// return true if changed
bool SetQuadding(Annotation* annot, int newQuadding) {
    EngineMupdf* e = annot->engine;
    {
        auto ctx = e->ctx;
        ScopedCritSec cs(e->ctxAccess);
        CrashIf(!IsValidQuadding(newQuadding));
        bool didChange = Quadding(annot) != newQuadding;
        if (!didChange) {
            return false;
        }
        fz_try(ctx) {
            pdf_set_annot_quadding(ctx, annot->pdfannot, newQuadding);
            pdf_update_annot(ctx, annot->pdfannot);
        }
        fz_catch(ctx) {
            logf("SetQuadding(): pdf_set_annot_quadding or pdf_update_annot() failed\n");
        }
    }
    MarkNotificationAsModified(e, annot);
    return true;
}

void SetQuadPointsAsRect(Annotation* annot, const Vec<RectF>& rects) {
    EngineMupdf* e = annot->engine;
    {
        auto ctx = e->ctx;
        ScopedCritSec cs(e->ctxAccess);
        fz_quad quads[512];
        int n = rects.isize();
        if (n == 0) {
            return;
        }
        constexpr int kMaxQuads = (int)dimof(quads);
        for (int i = 0; i < n && i < kMaxQuads; i++) {
            RectF rect = rects[i];
            fz_rect r = ToFzRect(rect);
            fz_quad q = fz_quad_from_rect(r);
            quads[i] = q;
        }
        fz_try(ctx) {
            pdf_clear_annot_quad_points(ctx, annot->pdfannot);
            pdf_set_annot_quad_points(ctx, annot->pdfannot, n, quads);
            pdf_update_annot(ctx, annot->pdfannot);
        }
        fz_catch(ctx) {
            logf("SetQuadPointsAsRect(): mupdf calls failed\n");
        }
    }
    MarkNotificationAsModified(e, annot);
}

/*
Vec<RectF> GetQuadPointsAsRect(Annotation* annot) {
    EngineMupdf* e = annot->engine;
    auto ctx = e->ctx;
    auto pdf = annot->pdf;
    ScopedCritSec cs(e->ctxAccess);
    Vec<RectF> res;
    int n = pdf_annot_quad_point_count(ctx, annot->pdfannot);
    for (int i = 0; i < n; i++) {
        fz_quad q{};
        fz_rect r{};
        fz_try(ctx)
        {
            q = pdf_annot_quad_point(ctx, annot->pdfannot, i);
            r = fz_rect_from_quad(q);
        }
        fz_catch(ctx) {
        }
        RectF rect = ToRectF(r);
        res.Append(rect);
    }
    return res;
}
*/

TempStr Contents(Annotation* annot) {
    EngineMupdf* e = annot->engine;
    auto ctx = e->ctx;
    ScopedCritSec cs(e->ctxAccess);
    const char* s = nullptr;
    fz_try(ctx) {
        s = pdf_annot_contents(ctx, annot->pdfannot);
    }
    fz_catch(ctx) {
        s = nullptr;
        logf("Contents(): pdf_annot_contents()\n");
    }
    return (TempStr)s;
}

bool SetContents(Annotation* annot, const char* sv) {
    EngineMupdf* e = annot->engine;
    const char* currValue = Contents(annot);
    if (str::Eq(sv, currValue)) {
        return false;
    }
    {
        auto ctx = e->ctx;
        ScopedCritSec cs(e->ctxAccess);
        fz_try(ctx) {
            pdf_set_annot_contents(ctx, annot->pdfannot, sv);
            pdf_update_annot(ctx, annot->pdfannot);
        }
        fz_catch(ctx) {
        }
    }
    MarkNotificationAsModified(e, annot);
    return true;
}

void DeleteAnnotation(Annotation* annot) {
    if (!annot) {
        return;
    }
    EngineMupdf* e = annot->engine;
    {
        auto ctx = e->ctx;
        ScopedCritSec cs(e->ctxAccess);
        bool failed = false;
        pdf_page* page = nullptr;
        fz_try(ctx) {
            page = pdf_annot_page(ctx, annot->pdfannot);
            pdf_delete_annot(ctx, page, annot->pdfannot);
        }
        fz_catch(ctx) {
            failed = true;
        }
        if (failed) {
            logf("failed to delete annotation on page %d\n", annot->pageNo);
            return;
        }
    }
    MarkNotificationAsModified(e, annot, AnnotationChange::Remove);
}

// -1 if not exist
int PopupId(Annotation* annot) {
    EngineMupdf* e = annot->engine;
    auto ctx = e->ctx;
    ScopedCritSec cs(e->ctxAccess);
    pdf_obj* obj = nullptr;
    int res = -1;
    fz_try(ctx) {
        obj = pdf_dict_get(ctx, pdf_annot_obj(ctx, annot->pdfannot), PDF_NAME(Popup));
        if (obj) {
            res = pdf_to_num(ctx, obj);
        }
    }
    fz_catch(ctx) {
    }
    return res;
}

/*
time_t CreationDate(Annotation* annot) {
    EngineMupdf* e = annot->engine;
    auto ctx = e->ctx;
    auto pdf = annot->pdf;
    ScopedCritSec cs(e->ctxAccess);
    int64_t res = 0;
    fz_try(ctx)
    {
        res = pdf_annot_creation_date(ctx, annot->pdfannot);
    }
    fz_catch(ctx) {
    }
    return res;
}
*/

time_t ModificationDate(Annotation* annot) {
    EngineMupdf* e = annot->engine;
    auto ctx = e->ctx;
    ScopedCritSec cs(e->ctxAccess);
    int64_t res = 0;
    fz_try(ctx) {
        res = pdf_annot_modification_date(ctx, annot->pdfannot);
    }
    fz_catch(ctx) {
    }
    return res;
}

// return empty() if no icon
const char* IconName(Annotation* annot) {
    EngineMupdf* e = annot->engine;
    auto ctx = e->ctx;
    ScopedCritSec cs(e->ctxAccess);
    bool hasIcon = false;
    const char* iconName = nullptr;
    fz_try(ctx) {
        hasIcon = pdf_annot_has_icon_name(ctx, annot->pdfannot);
        if (hasIcon) {
            // can only call if pdf_annot_has_icon_name() returned true
            iconName = pdf_annot_icon_name(ctx, annot->pdfannot);
        }
    }
    fz_catch(ctx) {
    }
    return iconName;
}

void SetIconName(Annotation* annot, const char* iconName) {
    EngineMupdf* e = annot->engine;
    {
        auto ctx = e->ctx;
        ScopedCritSec cs(e->ctxAccess);
        fz_try(ctx) {
            pdf_set_annot_icon_name(ctx, annot->pdfannot, iconName);
            pdf_update_annot(ctx, annot->pdfannot);
        }
        fz_catch(ctx) {
        }
    }
    // TODO: only if the value changed
    MarkNotificationAsModified(e, annot);
}

void PdfColorToFloat(PdfColor c, float rgb[3]) {
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
        return 0; // transparent
    }
    if (n == 1) {
        return MkPdfColorFromFloat(color[0], color[0], color[0]);
    }
    if (n == 3) {
        return MkPdfColorFromFloat(color[0], color[1], color[2]);
    }
    if (n == 4) {
        float rgb[4]{};
        fz_try(ctx) {
            fz_convert_color(ctx, fz_device_cmyk(ctx), color, fz_device_rgb(ctx), rgb, nullptr,
                             fz_default_color_params);
        }
        fz_catch(ctx) {
        }
        return MkPdfColorFromFloat(rgb[0], rgb[1], rgb[2]);
    }
    CrashIf(true);
    return 0;
}

PdfColor GetColor(Annotation* annot) {
    EngineMupdf* e = annot->engine;
    auto ctx = e->ctx;
    ScopedCritSec cs(e->ctxAccess);
    float color[4]{};
    int n = -1;
    fz_try(ctx) {
        pdf_annot_color(ctx, annot->pdfannot, &n, color);
    }
    fz_catch(ctx) {
        n = -1;
    }
    if (n == -1) {
        return 0;
    }
    PdfColor res = PdfColorFromFloat(ctx, n, color);
    return res;
}

// return true if color changed
bool SetColor(Annotation* annot, PdfColor c) {
    EngineMupdf* e = annot->engine;
    {
        auto ctx = e->ctx;
        ScopedCritSec cs(e->ctxAccess);
        bool didChange = false;
        float color[4]{};
        int n = -1;
        float oldOpacity = 0;
        fz_try(ctx) {
            pdf_annot_color(ctx, annot->pdfannot, &n, color);
            oldOpacity = pdf_annot_opacity(ctx, annot->pdfannot);
        }
        fz_catch(ctx) {
            n = -1;
        }
        if (n == -1) {
            return false;
        }
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
        fz_try(ctx) {
            if (c == 0) {
                pdf_set_annot_color(ctx, annot->pdfannot, 0, newColor);
                // TODO: set opacity to 1?
                // pdf_set_annot_opacity(ctx, annot->pdfannot, 1.f);
            } else {
                pdf_set_annot_color(ctx, annot->pdfannot, 3, newColor);
                if (oldOpacity != opacity) {
                    pdf_set_annot_opacity(ctx, annot->pdfannot, opacity);
                }
            }
            pdf_update_annot(ctx, annot->pdfannot);
        }
        fz_catch(ctx) {
        }
    }
    MarkNotificationAsModified(e, annot);
    return true;
}

PdfColor InteriorColor(Annotation* annot) {
    EngineMupdf* e = annot->engine;
    auto ctx = e->ctx;
    ScopedCritSec cs(e->ctxAccess);
    float color[4]{};
    int n = -1;
    fz_try(ctx) {
        pdf_annot_interior_color(ctx, annot->pdfannot, &n, color);
    }
    fz_catch(ctx) {
        n = -1;
    }
    if (n == -1) {
        return 0;
    }
    PdfColor res = PdfColorFromFloat(ctx, n, color);
    return res;
}

bool SetInteriorColor(Annotation* annot, PdfColor c) {
    EngineMupdf* e = annot->engine;
    {
        auto ctx = e->ctx;
        ScopedCritSec cs(e->ctxAccess);
        bool didChange = false;
        float color[4]{};
        int n = -1;
        fz_try(ctx) {
            pdf_annot_color(ctx, annot->pdfannot, &n, color);
        }
        fz_catch(ctx) {
            n = -1;
        }
        if (n == -1) {
            return false;
        }
        float newColor[3]{};
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
        fz_try(ctx) {
            if (c == 0) {
                pdf_set_annot_interior_color(ctx, annot->pdfannot, 0, newColor);
            } else {
                pdf_set_annot_interior_color(ctx, annot->pdfannot, 3, newColor);
            }
            pdf_update_annot(ctx, annot->pdfannot);
        }
        fz_catch(ctx) {
        }
    }
    MarkNotificationAsModified(e, annot);
    return true;
}

const char* DefaultAppearanceTextFont(Annotation* annot) {
    EngineMupdf* e = annot->engine;
    auto ctx = e->ctx;
    ScopedCritSec cs(e->ctxAccess);
    const char* fontName = nullptr;
    float sizeF{0.0};
    int n = 0;
    float textColor[4]{};
    fz_try(ctx) {
        pdf_annot_default_appearance(ctx, annot->pdfannot, &fontName, &sizeF, &n, textColor);
    }
    fz_catch(ctx) {
    }
    return fontName;
}

void SetDefaultAppearanceTextFont(Annotation* annot, const char* sv) {
    EngineMupdf* e = annot->engine;
    {
        auto ctx = e->ctx;
        ScopedCritSec cs(e->ctxAccess);
        const char* fontName = nullptr;
        float sizeF{0.0};
        int n = 0;
        float textColor[4]{};
        fz_try(ctx) {
            pdf_annot_default_appearance(ctx, annot->pdfannot, &fontName, &sizeF, &n, textColor);
            pdf_set_annot_default_appearance(ctx, annot->pdfannot, sv, sizeF, n, textColor);
            pdf_update_annot(ctx, annot->pdfannot);
        }
        fz_catch(ctx) {
        }
    }
    MarkNotificationAsModified(e, annot);
}

int DefaultAppearanceTextSize(Annotation* annot) {
    EngineMupdf* e = annot->engine;
    auto ctx = e->ctx;
    ScopedCritSec cs(e->ctxAccess);
    const char* fontName = nullptr;
    float sizeF{0.0};
    int n = 0;
    float textColor[4]{};
    fz_try(ctx) {
        pdf_annot_default_appearance(ctx, annot->pdfannot, &fontName, &sizeF, &n, textColor);
    }
    fz_catch(ctx) {
    }
    return (int)sizeF;
}

void SetDefaultAppearanceTextSize(Annotation* annot, int textSize) {
    EngineMupdf* e = annot->engine;
    {
        auto ctx = e->ctx;
        ScopedCritSec cs(e->ctxAccess);
        const char* fontName = nullptr;
        float sizeF{0.0};
        int n = 0;
        float textColor[4]{};
        fz_try(ctx) {
            pdf_annot_default_appearance(ctx, annot->pdfannot, &fontName, &sizeF, &n, textColor);
            pdf_set_annot_default_appearance(ctx, annot->pdfannot, fontName, (float)textSize, n, textColor);
            pdf_update_annot(ctx, annot->pdfannot);
        }
        fz_catch(ctx) {
        }
    }
    MarkNotificationAsModified(e, annot);
}

PdfColor DefaultAppearanceTextColor(Annotation* annot) {
    EngineMupdf* e = annot->engine;
    auto ctx = e->ctx;
    ScopedCritSec cs(e->ctxAccess);
    const char* fontName = nullptr;
    float sizeF{0.0};
    int n = 0;
    float textColor[4]{};
    fz_try(ctx) {
        pdf_annot_default_appearance(ctx, annot->pdfannot, &fontName, &sizeF, &n, textColor);
    }
    fz_catch(ctx) {
    }
    PdfColor res = PdfColorFromFloat(ctx, n, textColor);
    return res;
}

void SetDefaultAppearanceTextColor(Annotation* annot, PdfColor col) {
    EngineMupdf* e = annot->engine;
    {
        auto ctx = e->ctx;
        ScopedCritSec cs(e->ctxAccess);
        const char* fontName = nullptr;
        float sizeF{0.0};
        int n = 0;
        float textColor[4]{}; // must be at least 4
        fz_try(ctx) {
            pdf_annot_default_appearance(ctx, annot->pdfannot, &fontName, &sizeF, &n, textColor);
            PdfColorToFloat(col, textColor);
            pdf_set_annot_default_appearance(ctx, annot->pdfannot, fontName, sizeF, 3, textColor);
            pdf_update_annot(ctx, annot->pdfannot);
        }
        fz_catch(ctx) {
        }
    }
    MarkNotificationAsModified(e, annot);
}

void GetLineEndingStyles(Annotation* annot, int* start, int* end) {
    EngineMupdf* e = annot->engine;
    auto ctx = e->ctx;
    ScopedCritSec cs(e->ctxAccess);
    pdf_line_ending leStart = PDF_ANNOT_LE_NONE;
    pdf_line_ending leEnd = PDF_ANNOT_LE_NONE;
    fz_try(ctx) {
        pdf_annot_line_ending_styles(ctx, annot->pdfannot, &leStart, &leEnd);
    }
    fz_catch(ctx) {
        logf("GetLineEndingStyles: pdf_annot_line_ending_styles() failed\n");
    }
    *start = (int)leStart;
    *end = (int)leEnd;
}

/*
void SetLineEndingStyles(Annotation* annot, int start, int end) {
    EngineMupdf* e = annot->engine;
        {
            auto ctx = e->ctx;
        ScopedCritSec cs(e->ctxAccess);
                fz_try(ctx)
                {
                        pdf_line_ending leStart = (pdf_line_ending)start;
                        pdf_line_ending leEnd = (pdf_line_ending)end;
                        pdf_set_annot_line_ending_styles(ctx, annot->pdfannot, leStart, leEnd);
                        pdf_update_annot(ctx, annot->pdfannot);
                }
                fz_catch(ctx) {
                        logf("SetLineEndingStyles: failure in mupdf calls\n");
                }
        }
    MarkNotificationAsModified(e, annot);
}
*/

int BorderWidth(Annotation* annot) {
    EngineMupdf* e = annot->engine;
    auto ctx = e->ctx;
    ScopedCritSec cs(e->ctxAccess);
    float res = 0;
    fz_try(ctx) {
        res = pdf_annot_border(ctx, annot->pdfannot);
    }
    fz_catch(ctx) {
        logf("BorderWidth: pdf_annot_border() failed\n");
    }

    return (int)res;
}

void SetBorderWidth(Annotation* annot, int newWidth) {
    EngineMupdf* e = annot->engine;
    {
        auto ctx = e->ctx;
        ScopedCritSec cs(e->ctxAccess);
        fz_try(ctx) {
            pdf_set_annot_border(ctx, annot->pdfannot, (float)newWidth);
            pdf_update_annot(ctx, annot->pdfannot);
        }
        fz_catch(ctx) {
            logf("SetBorderWidth: SetBorderWidth() or pdf_update_annot() failed\n");
        }
    }
    MarkNotificationAsModified(e, annot);
}

int Opacity(Annotation* annot) {
    EngineMupdf* e = annot->engine;
    auto ctx = e->ctx;
    ScopedCritSec cs(e->ctxAccess);
    float fopacity = 0;
    fz_try(ctx) {
        fopacity = pdf_annot_opacity(ctx, annot->pdfannot);
    }
    fz_catch(ctx) {
        logf("Opacity: pdf_annot_opacity() failed\n");
    }
    int res = (int)(fopacity * 255.f);
    return res;
}

void SetOpacity(Annotation* annot, int newOpacity) {
    EngineMupdf* e = annot->engine;
    {
        auto ctx = e->ctx;
        ScopedCritSec cs(e->ctxAccess);
        CrashIf(newOpacity < 0 || newOpacity > 255);
        newOpacity = std::clamp(newOpacity, 0, 255);
        float fopacity = (float)newOpacity / 255.f;

        fz_try(ctx) {
            pdf_set_annot_opacity(ctx, annot->pdfannot, fopacity);
            pdf_update_annot(ctx, annot->pdfannot);
        }
        fz_catch(ctx) {
            logf("SetOpacity: pdf_set_annot_opacity() or pdf_update_annot() failed\n");
        }
    }
    MarkNotificationAsModified(e, annot);
}

// TODO: unused, remove
#if 0
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
#endif

static const char* getuser(void) {
    const char* u;
    u = getenv("USER");
    if (!u) {
        u = getenv("USERNAME");
    }
    if (!u) {
        u = "user";
    }
    return u;
}

static TempStr GetAnnotationTextIconTemp() {
    char* s = str::DupTemp(gGlobalPrefs->annotations.textIconType);
    // this way user can use "new paragraph" and we'll match "NewParagraph"
    str::RemoveCharsInPlace(s, " ");
    int idx = seqstrings::StrToIdxIS(gAnnotationTextIcons, s);
    if (idx < 0) {
        return (char*)"Note";
    }
    char* real = (char*)seqstrings::IdxToStr(gAnnotationTextIcons, idx);
    return real;
}

// clang-format off
static AnnotationType moveableAnnotations[] = {
    AnnotationType::Text,
    AnnotationType::Link,
    AnnotationType::FreeText,
    AnnotationType::Line,
    AnnotationType::Square,
    AnnotationType::Circle,
    AnnotationType::Polygon,
    AnnotationType::PolyLine,
    //AnnotationType::Highlight,
    //AnnotationType::Underline,
    //AnnotationType::Squiggly,
    //AnnotationType::StrikeOut,
    //AnnotationType::Redact,
    AnnotationType::Stamp,
    AnnotationType::Caret,
    //AnnotationType::Ink,
    AnnotationType::Popup,
    AnnotationType::FileAttachment,
    AnnotationType::Sound,
    AnnotationType::Movie,
    //AnnotationType::Widget, // TODO: maybe moveble?
    //AnnotationType::Screen,
    AnnotationType::PrinterMark,
    AnnotationType::TrapNet,
    AnnotationType::Watermark,
    AnnotationType::ThreeD,
    AnnotationType::Unknown,// sentinel value
};
// clang-format on

static bool IsAnnotationInList(AnnotationType tp, AnnotationType* allowed) {
    if (!allowed) {
        return true;
    }
    int i = 0;
    while (allowed[i] != AnnotationType::Unknown) {
        AnnotationType tp2 = allowed[i];
        if (tp2 == tp) {
            return true;
        }
        ++i;
    }
    return false;
}

bool IsMoveableAnnotation(AnnotationType tp) {
    return IsAnnotationInList(tp, moveableAnnotations);
}

Annotation* EngineMupdfCreateAnnotation(EngineBase* engine, AnnotationType typ, int pageNo, PointF pos) {
    static const float black[3] = {0, 0, 0};

    EngineMupdf* epdf = AsEngineMupdf(engine);
    fz_context* ctx = epdf->ctx;

    auto pageInfo = epdf->GetFzPageInfo(pageNo, true);
    pdf_annot* annot = nullptr;

    {
        ScopedCritSec cs(epdf->ctxAccess);

        fz_try(ctx) {
            auto page = pdf_page_from_fz_page(ctx, pageInfo->page);
            enum pdf_annot_type atyp = (enum pdf_annot_type)typ;

            annot = pdf_create_annot(ctx, page, atyp);

            pdf_set_annot_modification_date(ctx, annot, time(nullptr));
            if (pdf_annot_has_author(ctx, annot)) {
                char* defAuthor = gGlobalPrefs->annotations.defaultAuthor;
                // if "(none)" we don't set it
                if (!str::Eq(defAuthor, "(none)")) {
                    const char* author = getuser();
                    if (!str::EmptyOrWhiteSpaceOnly(defAuthor)) {
                        author = defAuthor;
                    }
                    pdf_set_annot_author(ctx, annot, author);
                }
            }

            switch (typ) {
                case AnnotationType::Text:
                case AnnotationType::FreeText:
                case AnnotationType::Stamp:
                case AnnotationType::Caret:
                case AnnotationType::Square:
                case AnnotationType::Circle: {
                    fz_rect trect = pdf_annot_rect(ctx, annot);
                    float dx = trect.x1 - trect.x0;
                    trect.x0 = pos.x;
                    trect.x1 = trect.x0 + dx;
                    float dy = trect.y1 - trect.y0;
                    trect.y0 = pos.y;
                    trect.y1 = trect.y0 + dy;
                    pdf_set_annot_rect(ctx, annot, trect);
                } break;
                case AnnotationType::Line: {
                    fz_point a{pos.x, pos.y};
                    fz_point b{pos.x + 100, pos.y + 50};
                    pdf_set_annot_line(ctx, annot, a, b);
                } break;
            }
            if (typ == AnnotationType::FreeText) {
                auto& a = gGlobalPrefs->annotations;
                int borderWidth = a.freeTextBorderWidth;
                if (borderWidth < 0) {
                    borderWidth = 1; // default
                }
                pdf_set_annot_border(ctx, annot, (float)borderWidth);
                pdf_set_annot_contents(ctx, annot, "This is a text...");
                int fontSize = a.freeTextSize;
                if (fontSize <= 0) {
                    fontSize = 12;
                }
                int nCol = 3;
                const float* col = black;
                float textColor[3]{};

                auto parsedCol = GetParsedColor(a.freeTextColor, a.freeTextColorParsed);
                if (parsedCol && parsedCol->parsedOk) {
                    PdfColorToFloat(parsedCol->pdfCol, textColor);
                    col = textColor;
                }

                pdf_set_annot_default_appearance(ctx, annot, "Helv", (float)fontSize, nCol, col);
            }

            pdf_update_annot(ctx, annot);
        }
        fz_catch(ctx) {
            if (annot) {
                pdf_drop_annot(ctx, annot);
            }
        }
        if (!annot) {
            return nullptr;
        }
    }

    auto res = MakeAnnotationWrapper(epdf, annot, pageNo);
    MarkNotificationAsModified(epdf, res, AnnotationChange::Add);

    auto& a = gGlobalPrefs->annotations;
    ParsedColor* parsedCol = nullptr;

    if (typ == AnnotationType::Text) {
        TempStr iconName = GetAnnotationTextIconTemp();
        if (!str::EqI(iconName, "Note")) {
            SetIconName(res, iconName);
        }
        parsedCol = GetParsedColor(a.textIconColor, a.textIconColorParsed);
    } else if (typ == AnnotationType::Underline) {
        parsedCol = GetParsedColor(a.underlineColor, a.underlineColorParsed);
    } else if (typ == AnnotationType::Highlight) {
        parsedCol = GetParsedColor(a.highlightColor, a.highlightColorParsed);
    } else if (typ == AnnotationType::Squiggly) {
        parsedCol = GetParsedColor(a.squigglyColor, a.squigglyColorParsed);
    } else if (typ == AnnotationType::StrikeOut) {
        parsedCol = GetParsedColor(a.strikeOutColor, a.strikeOutColorParsed);
    } else if (typ == AnnotationType::FreeText) {
    }
    if (parsedCol && parsedCol->parsedOk) {
        SetColor(res, parsedCol->pdfCol);
    }

    pdf_drop_annot(ctx, annot);
    return res;
}
