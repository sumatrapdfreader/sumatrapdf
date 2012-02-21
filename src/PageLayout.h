/* Copyright 2011-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef PageLayout_h
#define PageLayout_h

#include "BaseUtil.h"
#include "HtmlPullParser.h"
#include "Scoped.h"
#include "Vec.h"

using namespace Gdiplus;
#include "GdiPlusUtil.h"

// Layout information for a given page is a list of
// draw instructions that define what to draw and where.
enum DrawInstrType {
    InstrTypeString = 0,
    InstrTypeLine,
    InstrTypeSetFont
};

struct InstrString {
    const char *        s;
    size_t              len;
};

struct InstrSetFont {
    Font *              font;
};

struct DrawInstr {
    DrawInstrType       type;
    union {
        // info specific to a given instruction
        InstrString     str;
        InstrSetFont    setFont;
    };
    RectF bbox; // common to most instructions

    DrawInstr() { }

    DrawInstr(DrawInstrType t, RectF bbox = RectF()) :
        type(t), bbox(bbox)
    {
    }

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
};

struct PageData {
    Vec<DrawInstr>  drawInstructions;

    void Append(DrawInstr& di) {
        drawInstructions.Append(di);
    }
    size_t Count() const {
        return drawInstructions.Count();
    }
};

// just to pack args to LayoutHtml
class LayoutInfo {
public:
    LayoutInfo() :
      pageDx(0), pageDy(0), fontName(NULL), fontSize(0),
      textAllocator(NULL), htmlStr(0), htmlStrLen(0)
    {
    }

    int             pageDx;
    int             pageDy;

    const WCHAR *   fontName;
    float           fontSize;

    /* Most of the time string DrawInstr point to original html text
       that is read-only and outlives us. Sometimes (e.g. when resolving
       html entities) we need a modified text. This allocator is
       used to allocate this text. */
    Allocator *     textAllocator;

    const char *    htmlStr;
    size_t          htmlStrLen;
};

class PageLayout
{
public:
    PageLayout();
    ~PageLayout();

    PageData *IterStart(LayoutInfo* layoutInfo);
    PageData *IterNext();

private:
    void HandleHtmlTag(HtmlToken *t);
    void EmitText(HtmlToken *t);

    REAL GetCurrentLineDx();
    void LayoutLeftStartingAt(REAL offX);
    void JustifyLineLeft();
    void JustifyLineBoth();
    void JustifyLine(AlignAttr mode);

    void StartNewPage();
    void StartNewLine(bool isParagraphBreak);

    void AddSetFontInstr(Font *font);

    void AddHr();
    void EmitTextRune(const char *s, const char *end);

    void SetCurrentFont(FontStyle fs);
    void ChangeFont(FontStyle fs, bool isStart);

    DrawInstr *GetInstructionsForCurrentLine(DrawInstr *& endInst) const {
        size_t len = currPage->Count() - currLineInstrOffset;
        DrawInstr *ret = &currPage->drawInstructions.At(currLineInstrOffset);
        endInst = ret + len;
        return ret;
    }

    bool IsCurrentLineEmpty() const {
        return currLineInstrOffset == currPage->Count();
    }

    // constant during layout process
    REAL                pageDx;
    REAL                pageDy;
    SizeT<REAL>         pageSize;
    REAL                lineSpacing;
    REAL                spaceDx;
    Graphics *          gfx; // for measuring text
    ScopedMem<WCHAR>    fontName;
    float               fontSize;
    Allocator *         textAllocator;

    // temporary state during layout process
    FontStyle           currFontStyle;
    Font *              currFont;

    AlignAttr           currJustification;
    // current position in a page
    REAL                currX, currY;
    // number of consecutive newlines
    int                 newLinesCount;
    // indicates if the last instruction was text
    // consisting of just spaces
    bool                hadSpaceBefore;

    PageData *          currPage;

    // for iterative parsing
    HtmlPullParser *    htmlParser;
    // list of pages constructed
    Vec<PageData*>      pagesToSend;
    bool                finishedParsing;

    // current nesting of html tree during html parsing
    Vec<HtmlTag>        tagNesting;

    size_t              currLineInstrOffset;
    WCHAR               buf[512];

    FontMetricsCache    fontMetrics;
};

Vec<PageData*> *LayoutHtml(LayoutInfo* li);

void DrawPageLayout(Graphics *g, Vec<DrawInstr> *drawInstructions, REAL offX, REAL offY, bool showBbox);

#endif
