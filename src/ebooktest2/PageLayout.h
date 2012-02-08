/* Copyright 2011-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef PageLayout_h
#define PageLayout_h

#include "BaseEbookDoc.h"
#include "BaseUtil.h"
#include "Vec.h"
#include "GeomUtil.h"

using namespace Gdiplus;

struct WordInfo {
    const char *    s;
    size_t          len;

    bool IsNewline() { return len == 1 && *s == '\n'; }
};

struct DrawInstr {
    // Layout information for a given page is a list of
    // draw instructions that define what to draw and where.
    enum Type { TypeString, TypeLine, TypeSetFont, TypeImage };

    Type                type;
    union {
        // info specific to a given instruction
        WordInfo        str;
        Font *          font;
        ImageData2 *    img;
    };
    RectF bbox; // common to most instructions

    DrawInstr() { }
    DrawInstr(Type t, RectF bbox=RectF()) : type(t), bbox(bbox) { }

    static DrawInstr Str(const char *s, size_t len, RectF bbox) {
        DrawInstr di(TypeString, bbox);
        di.str.s = s;
        di.str.len = len;
        return di;
    }

    static DrawInstr SetFont(Font *font) {
        DrawInstr di(TypeSetFont);
        di.font = font;
        return di;
    }

    static DrawInstr Line(RectF bbox) {
        DrawInstr di(TypeLine, bbox);
        return di;
    }

    static DrawInstr Image(ImageData2 *data, RectF bbox) {
        DrawInstr di(TypeImage, bbox);
        di.img = data;
        return di;
    }
};

class PageData {
    Vec<DrawInstr> drawInstructions;

public:
    DrawInstr& Instr(size_t idx) { return drawInstructions.At(idx); }
    void Append(DrawInstr di) { drawInstructions.Append(di); }
    size_t Count() const { return drawInstructions.Count(); }
};

// Called by LayoutHtml with instructions for each page. Caller
// must remember them as LayoutHtml doesn't retain them.
class INewPageObserver {
public:
    virtual void NewPage(PageData *pageData) = 0;
};

// just to pack args to LayoutHtml
class LayoutInfo {
public:
    LayoutInfo() : doc(NULL), htmlStr(0), htmlStrLen(0) { }

    // BaseEbookDoc is required for accessing file images
    BaseEbookDoc *  doc;
    // pageSize indicates the maximum size of a single page (without margins)
    SizeI           pageSize;
    // usually: htmlStr = doc->GetBookHtmlData(li.htmlStrLen);
    const char *    htmlStr;
    size_t          htmlStrLen;
};

class FontCache {
    struct Entry{
        WCHAR *     name;
        float       size;
        FontStyle   style;
        Font *      font;

        bool operator==(Entry& other){
            return size == other.size && style == other.style && str::Eq(name, other.name);
        }
    };

    Vec<Entry> cache;

public:
    FontCache() { }
    ~FontCache();

    Font *GetFont(const WCHAR *name, float size, FontStyle style);
};

// the fontCache must outlive any PageData the INewPageObserver receives
void LayoutHtml(LayoutInfo li, FontCache *fontCache, INewPageObserver *pageObserver=NULL);
void DrawPageLayout(Graphics *g, PageData *pageData, REAL offX, REAL offY, bool debugBboxes=false);

#endif
