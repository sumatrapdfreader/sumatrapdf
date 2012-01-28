/* Copyright 2011-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef PageLayout_h
#define PageLayout_h

#include "BaseUtil.h"
#include "Vec.h"
#include "StrUtil.h"
#include "HtmlPullParser.h"

using namespace Gdiplus;

struct WordInfo;

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
    size_t              fontIdx;
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

    static DrawInstr SetFont(size_t fontIdx) {
        DrawInstr di(InstrTypeSetFont);
        di.setFont.fontIdx = fontIdx;
        return di;
    }

    static DrawInstr Line(RectF bbox) {
        DrawInstr di(InstrTypeLine, bbox);
        return di;
    }
};

class PageLayout
{
    enum TextJustification {
        Left, Right, Center, Both
    };

    struct FontInfo {
        FontStyle   style;
        Font *      font;
    };

public:
    PageLayout(int dx, int dy) {
        pageDx = (REAL)dx; pageDy = (REAL)dy;
    }

    ~PageLayout();

    //Vec<Page *> *LayoutText(Graphics *graphics, Font *defaultFnt, const char *s);

    void HandleHtmlTag(HtmlToken *t);
    void EmitText(HtmlToken *t);

    bool LayoutHtml(WCHAR *fontName, float fontSize, const char *s, size_t sLen);

    size_t PageCount() const {
        return pageInstrOffset.Count();
    }

    DrawInstr *GetInstructionsForPage(size_t pageNo, DrawInstr *& endInstr) const {
        CrashAlwaysIf(pageNo >= PageCount());
        size_t start = pageInstrOffset.At(pageNo);
        size_t end = instructions.Count(); // if the last page
        if (pageNo < PageCount() - 1)
            end = pageInstrOffset.At(pageNo + 1);
        CrashAlwaysIf(end < start);
        size_t len = end - start;
        DrawInstr *ret = &instructions.At(start);
        endInstr = ret + len;
        return ret;
    }

    DrawInstr *GetInstructionsForCurrentLine(DrawInstr *& endInst) const {
        size_t len = instructions.Count() - currLineInstrOffset;
        DrawInstr *ret = &instructions.At(currLineInstrOffset);
        endInst = ret + len;
        return ret;
    }

    bool IsCurrentLineEmpty() const {
        return currLineInstrOffset == instructions.Count();
    }

    Font *GetFontByIdx(size_t idx) {
        CrashAlwaysIf(idx >= fontCache.Size());
        FontInfo fi = fontCache.At(idx);
        CrashAlwaysIf(NULL == fi.font);
        return fi.font;
    }

    // constant during layout process
    REAL        pageDx, pageDy;

private:
    REAL GetCurrentLineDx();
    void LayoutLeftStartingAt(REAL offX);
    void JustifyLineLeft();
    void JustifyLineRight();
    void JustifyLineCenter();
    void JustifyLineBoth();
    void JustifyLine(TextJustification mode);

    TextJustification AlignAttrToJustification(AlignAttr align);

    void StartLayout();
    void StartNewPage();
    void StartNewLine(bool isParagraphBreak);
    void RemoveLastPageIfEmpty();

    void AddSetFontInstr(size_t fontIdx);

    void AddHr();
    void AddWord(WordInfo *wi);

    void ClearFontCache();
    void SetCurrentFont(FontStyle fs);
    void ChangeFont(FontStyle fs, bool isStart);
    FontStyle           currFontStyle;
    Vec<FontInfo>       fontCache;
    ScopedMem<WCHAR>    fontName;
    float               fontSize;
    Font *              currFont;
    size_t              currFontIdx; // within fontcache

    // constant during layout process
    REAL        lineSpacing;
    REAL        spaceDx;
    Graphics *  gfx;

    // temporary state during layout process
    TextJustification   currJustification;
    // current position in a page
    REAL                currX, currY; 
    // number of consecutive newlines
    int                 newLinesCount;

    // drawing instructions for all pages
    Vec<DrawInstr>      instructions;

    // current nesting of html tree during html parsing
    Vec<HtmlTag>        tagNesting;

    // a page is fully described by list of drawing instructions
    // This is an array of offsets into instructions array
    // for each page. The length can be calculating by
    // substracting this from the offset of the next page
    Vec<size_t>         pageInstrOffset;

    size_t              currPageInstrOffset;
    size_t              currLineInstrOffset;

    WCHAR               buf[512];
};

#endif
