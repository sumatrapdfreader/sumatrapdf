/* Copyright 2011-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef PageLayout_h
#define PageLayout_h

#include "BaseEbookDoc.h"
#include "BaseUtil.h"
#include "Vec.h"
#include "GeomUtil.h"

using namespace Gdiplus;

// Layout information for a given page is a list of
// draw instructions that define what to draw and where.
enum DrawInstrType {
    InstrTypeString = 0,
    InstrTypeLine,
    InstrTypeSetFont,
    InstrTypeImage,
};

struct InstrString {
    const char *        s;
    size_t              len;
};

struct InstrSetFont {
    Font *              font;
};

struct InstrImage {
    ImageData2 *        data;
};

struct DrawInstr {
    DrawInstrType       type;
    union {
        // info specific to a given instruction
        InstrString     str;
        InstrSetFont    setFont;
        InstrImage      img;
    };
    RectF bbox; // common to most instructions

    DrawInstr() { }
    DrawInstr(DrawInstrType t, RectF bbox=RectF()) : type(t), bbox(bbox) { }

    static DrawInstr Str(const char *s, size_t len, RectF bbox) {
        DrawInstr di(InstrTypeString, bbox);
        di.str.s = s;
        di.str.len = len;
        return di;
    }

    static DrawInstr SetFont(Font *font) {
        DrawInstr di(InstrTypeSetFont);
        di.setFont.font = font;
        return di;
    }

    static DrawInstr Line(RectF bbox) {
        DrawInstr di(InstrTypeLine, bbox);
        return di;
    }

    static DrawInstr Image(ImageData2 *data, RectF bbox) {
        DrawInstr di(InstrTypeImage, bbox);
        di.img.data = data;
        return di;
    }
};

struct LayoutState {
    const char *htmlStart;
    const char *htmlEnd;
};

struct PageData {
    Vec<DrawInstr>  drawInstructions;

    /* layoutState at the beginning of this page. It allows us to
       re-do layout starting exactly where this page starts, which
       is needed when handling resizing */
    LayoutState    layoutState;

    void Append(DrawInstr& di) {
        drawInstructions.Append(di);
    }
    size_t Count() const {
        return drawInstructions.Count();
    }
};

// Called by LayoutHtml with instructions for each page. Caller
// must remember them as LayoutHtml doesn't retain them.
class INewPageObserver {
public:
    virtual ~INewPageObserver() { }
    virtual void NewPage(PageData *pageData) = 0;
};

// just to pack args to LayoutHtml
class LayoutInfo {
public:
    LayoutInfo() : doc(NULL), fontName(NULL), fontSize(0), htmlStr(0), htmlStrLen(0) { }

    BaseEbookDoc *  doc;

    SizeI           pageSize;

    const WCHAR *   fontName;
    float           fontSize;

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

void LayoutHtml(LayoutInfo* li, FontCache *fontCache, INewPageObserver *pageObserver=NULL);
void DrawPageLayout(Graphics *g, PageData *pageData, REAL offX, REAL offY, bool showBbox);
void InitGraphicsMode(Graphics *g);

#endif
