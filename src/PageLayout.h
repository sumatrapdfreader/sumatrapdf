/* Copyright 2011-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef PageLayout_h
#define PageLayout_h

#include <stdint.h>
#include "BaseUtil.h"
#include "Vec.h"
#include "StrUtil.h"

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
    const uint8_t *     s;
    size_t              len;
};

struct InstrSetFont {
    size_t              fontNo;
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

    DrawInstr(DrawInstrType t)
    {
        type = t;
        bbox.X = 0; bbox.Y = 0;
        bbox.Width = 0; bbox.Height = 0;
    }
};

class PageLayout
{
    enum TextJustification {
        Left, Right, Center, Both
    };

public:
    PageLayout(int dx, int dy) {
        pageDx = (REAL)dx; pageDy = (REAL)dy;
        lineSpacing = 0; spaceDx = 0;
        currPageInstrOffset = 0;
        x = y = 0;
    }

    //Vec<Page *> *LayoutText(Graphics *graphics, Font *defaultFnt, const char *s);
    bool LayoutInternal(Graphics *graphics, Font *defaultFnt, const uint8_t *s, size_t sLen);

    size_t PageCount() const {
        return pageInstrOffset.Count();
    }

    // TODO: instead of instrCount, return DrawInstr *& end
    DrawInstr *GetInstructionsForPage(size_t pageNo, size_t *instrCount) const {
        CrashAlwaysIf(pageNo >= PageCount());
        size_t start = pageInstrOffset.At(pageNo);
        size_t end = instructions.Count(); // if the last page
        if (pageNo < PageCount() - 1)
            end = pageInstrOffset.At(pageNo + 1);
        CrashAlwaysIf(end < start);
        *instrCount = end - start;
        return &instructions.At(start);
    }

    // TODO: instead of instrCount, return DrawInstr *& end
    DrawInstr *GetInstructionsForCurrentLine(size_t *instrCount) const {
        *instrCount = instructions.Count() - currLineInstrOffset;
        return &instructions.At(currLineInstrOffset);
    }

    bool IsCurrentLineEmpty() const {
        return currLineInstrOffset == instructions.Count();
    }


private:
    REAL GetTotalLineDx();
    void LayoutLeftStartingAt(REAL offX);
    void JustifyLineLeft();
    void JustifyLineRight();
    void JustifyLineCenter();
    void JustifyLineBoth();
    void JustifyLine(TextJustification mode);

    void StartLayout();
    void StartNewPage();
    void StartNewLine(bool isParagraphBreak);
    void RemoveLastPageIfEmpty();
    void AddWord(WordInfo *wi);
    void AddHr();

    // constant during layout process
    REAL        pageDx, pageDy;
    REAL        lineSpacing;
    REAL        spaceDx;
    Graphics *  gfx;
    Font *      defaultFont;

    // temporary state during layout process
    TextJustification   currJustification;
    Font *              currFont;
    // current position in a page
    REAL                x, y; 
    // number of consecutive newlines
    int                 newLinesCount;

    // drawing instructions for all pages
    Vec<DrawInstr>      instructions;

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
