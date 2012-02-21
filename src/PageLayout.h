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
    // a piece of text
    InstrString = 0,
    // space is not drawn. it's inserted during layout
    // to mark 
    InstrSpace,
    // paragraph stat is not drawn. it's inserted during layout
    // to mark indentation of first line in the paragraph
    // (but only if justification is left or justify
    InstrParagraphStart,
    // a vertical line
    InstrLine,
    // change current font
    InstrSetFont
};

struct InstrStringData {
    const char *        s;
    size_t              len;
};

struct InstrSetFontData {
    Font *              font;
};

struct DrawInstr {
    DrawInstrType       type;
    union {
        // info specific to a given instruction
        InstrStringData     str;
        InstrSetFontData    setFont;
    };
    RectF bbox; // common to most instructions

    DrawInstr() { }

    DrawInstr(DrawInstrType t, RectF bbox = RectF()) : type(t), bbox(bbox) { }

    static DrawInstr Str(const char *s, size_t len, RectF bbox);
    static DrawInstr SetFont(Font *font);
    static DrawInstr Line(RectF bbox);
    static DrawInstr Space();
    static DrawInstr ParagraphStart();
};

struct PageData {
    Vec<DrawInstr>  drawInstructions;

    void Append(DrawInstr& di) { drawInstructions.Append(di); }
    size_t Count() const { return drawInstructions.Count(); }
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

    void EmitSetFont(Font *font);
    void EmitLine();
    void EmitTextRune(const char *s, const char *end);
    void EmitSpace();
    void EmitParagraphStart();

    void SetCurrentFont(FontStyle fs);
    void ChangeFont(FontStyle fs, bool isStart);

    DrawInstr *GetInstructionsForCurrentLine(DrawInstr *& endInst) const;

    bool IsCurrentLineEmpty() const { return currLineInstrOffset == currPage->Count(); }
    bool LastInstrIsSpace() const;

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
