/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// A PDF's annotations as rows (page, type, marked text, note, author, ...),
// and those rows as Markdown or CSV: `SumatraPDF cli annotations` and the
// Export Annotations command.

#include "base/Base.h"
#include "base/GuessFileType.h"
#include "gui/UIModels.h"

#include "DocProperties.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "Annotation.h"
#include "AnnotationsExport.h"

#include <time.h>

static TempStr HexColorTemp(PdfColor c) {
    if (c == 0 || c == (PdfColor)kColorUnset) {
        return {};
    }
    u8 r, g, b, a;
    UnpackPdfColor(c, r, g, b, a);
    return fmt("#%02x%02x%02x", r, g, b);
}

static TempStr IsoTimeTemp(time_t t) {
    if (t <= 0) {
        return {};
    }
    struct tm tmv;
#if OS_WIN
    gmtime_s(&tmv, &t);
#else
    gmtime_r(&t, &tmv);
#endif
    return fmt("%04d-%02d-%02dT%02d:%02d:%02dZ", tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday, tmv.tm_hour,
               tmv.tm_min, tmv.tm_sec);
}

// highlight, underline, squiggly or strike-out: marks text
bool IsTextMarkupAnnot(AnnotationType tp) {
    return tp == AnnotationType::Highlight || tp == AnnotationType::Underline || tp == AnnotationType::Squiggly ||
           tp == AnnotationType::StrikeOut;
}

// the page text a highlight (underline, ...) is over: the characters whose
// box's center is in one of its quads
static TempStr MarkedTextTemp(const PageText& pt, const Vec<RectF>& quads) {
    str::Builder out;
    const u8* p = (const u8*)pt.text.s;
    const u8* end = p + len(pt.text);
    for (int i = 0; i < pt.nCodepoints && p < end; i++) {
        int n = utf8RuneLen(p);
        if (n < 1 || p + n > end) {
            n = 1;
        }
        Rect c = pt.coords ? pt.coords[i] : Rect();
        PointF center((float)c.x + (float)c.dx / 2, (float)c.y + (float)c.dy / 2);
        for (const RectF& q : quads) {
            if (q.Contains(center)) {
                out.Append(Str((const char*)p, n));
                break;
            }
        }
        p += n;
    }
    TempStr res = ToStrTemp(out);
    return str::ReplaceTemp(res, StrL("\n"), StrL(" "));
}

// The annotations on pages, in page order. Popups (another annotation's note
// window) and widgets (form fields) are left out. The engine must stay open
// while rows are used.
void CollectAnnotRows(EngineBase* engine, const Vec<int>& pages, Vec<AnnotRow>& rows) {
    for (int pageNo : pages) {
        EngineMupdfLoadAnnotsForPage(engine, pageNo);
    }
    Vec<Annotation*> annots;
    EngineMupdfGetLoadedAnnotations(engine, annots);
    for (int pageNo : pages) {
        PageText pt{};
        bool haveText = false;
        for (Annotation* a : annots) {
            if (PageNo(a) != pageNo) {
                continue;
            }
            AnnotationType tp = Type(a);
            if (tp == AnnotationType::Popup || tp == AnnotationType::Widget) {
                continue;
            }
            AnnotRow row;
            row.pageNo = pageNo;
            row.type = tp;
            row.rect = GetRect(a);
            if (IsTextMarkupAnnot(tp)) {
                if (!haveText) {
                    pt = engine->ExtractPageText(pageNo);
                    haveText = true;
                }
                row.marked = MarkedTextTemp(pt, GetQuadPointsAsRect(a));
            }
            row.contents = Contents(a);
            row.author = Author(a);
            row.color = HexColorTemp(GetColor(a));
            row.modified = IsoTimeTemp(ModificationDate(a));
            VecAppend(rows, row);
        }
        if (haveText) {
            FreePageText(&pt);
        }
    }
}

static TempStr OneLineTemp(Str s) {
    TempStr res = str::ReplaceTemp(s, StrL("\r\n"), StrL(" "));
    return str::ReplaceTemp(res, StrL("\n"), StrL(" "));
}

// "# Annotations in doc.pdf", a "## Page N" per page, a line per annotation:
// - **Highlight**: "the marked text" note (author, 2026-11-22)
TempStr AnnotationsMarkdownTemp(const Vec<AnnotRow>& rows, Str docName) {
    str::Builder out;
    out.Append(fmt("# Annotations in %s\n", docName));
    int lastPage = 0;
    for (const AnnotRow& r : rows) {
        if (r.pageNo != lastPage) {
            out.Append(fmt("\n## Page %d\n\n", r.pageNo));
            lastPage = r.pageNo;
        }
        out.Append(fmt("- **%s**", AnnotationTypeName(r.type)));
        if (len(r.marked) > 0) {
            out.Append(fmt(": \"%s\"", r.marked));
        }
        if (len(r.contents) > 0) {
            out.Append(fmt("%s %s", len(r.marked) > 0 ? StrL("") : StrL(":"), OneLineTemp(r.contents)));
        }
        Str when = len(r.modified) >= 10 ? Str(r.modified.s, 10) : Str{};
        if (len(r.author) > 0 && len(when) > 0) {
            out.Append(fmt(" *(%s, %s)*", r.author, when));
        } else if (len(r.author) > 0 || len(when) > 0) {
            out.Append(fmt(" *(%s)*", len(r.author) > 0 ? r.author : when));
        }
        out.Append(StrL("\n"));
    }
    if (len(rows) == 0) {
        out.Append(StrL("\nNo annotations.\n"));
    }
    return ToStrTemp(out);
}

// a CSV field: quoted when it has a comma, quote or line break (RFC 4180)
static void AppendCsvField(str::Builder& out, Str s) {
    bool quote = str::ContainsChar(s, ',') || str::ContainsChar(s, '"') || str::ContainsChar(s, '\n') ||
                 str::ContainsChar(s, '\r');
    if (!quote) {
        out.Append(s);
        return;
    }
    out.AppendChar('"');
    out.Append(str::ReplaceTemp(s, StrL("\""), StrL("\"\"")));
    out.AppendChar('"');
}

// page,type,text,note,author,color,modified; UTF-8 with a BOM, so Excel
// reads it as UTF-8
TempStr AnnotationsCsvTemp(const Vec<AnnotRow>& rows) {
    str::Builder out;
    out.Append(StrL("\xEF\xBB\xBFpage,type,text,note,author,color,modified\r\n"));
    for (const AnnotRow& r : rows) {
        out.Append(fmt("%d,", r.pageNo));
        AppendCsvField(out, AnnotationTypeName(r.type));
        out.AppendChar(',');
        AppendCsvField(out, r.marked);
        out.AppendChar(',');
        AppendCsvField(out, r.contents);
        out.AppendChar(',');
        AppendCsvField(out, r.author);
        out.AppendChar(',');
        AppendCsvField(out, r.color);
        out.AppendChar(',');
        AppendCsvField(out, r.modified);
        out.Append(StrL("\r\n"));
    }
    return ToStrTemp(out);
}
