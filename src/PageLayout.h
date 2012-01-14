/* Copyright 2011-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef PageLayout_h
#define PageLayout_h

#include <stdint.h>
#include "BaseUtil.h"
#include "Vec.h"

using namespace Gdiplus;

struct WordInfo;

enum StrSpecial {
    Str_Hr = 0,
};

struct StringPos {
    StringPos() : s(NULL), len(0) {
    }
    StringPos(const char *s, size_t len, RectF bbox) : s(s), len(len), bbox(bbox) {
    }
    // is either a pointer to a string or one of the StrSpecial
    // enumerations
    const char *s;
    size_t len;
    RectF bbox;
};

class Page {
public:
    Page() : dx(0), dy(0), strings(NULL) {
    }
    Page(int dx, int dy) : dx(dx), dy(dy) {
        strings = new Vec<StringPos>();
    }
    ~Page() {
        delete strings;
    }
    int dx, dy; // used during layout
    // TODO: replace with binary blob that encodes data in a way
    // similar to MobiHtmlToDisplay(), because we have many different
    // types of data other than StringPos
    Vec<StringPos> *strings;
};

class PageLayout
{
    enum TextJustification {
        Left, Right, Center, Both
    };

    struct StrDx {
        StrDx() : s(NULL), len(0), dx(0), dy(0) {
        }
        StrDx(const char *s, size_t len, REAL dx, REAL dy) : s(s), len(len), dx(dx), dy(dy) {
        }
        const char *s;
        size_t len;
        REAL dx, dy;
    };

public:
    PageLayout(int dx, int dy) {
        pageDx = (REAL)dx; pageDy = (REAL)dy;
        lineSpacing = 0; spaceDx = 0;
        pages = NULL; currPage = NULL;
        x = y = 0;
    }

    Vec<Page *> *LayoutText(Graphics *graphics, Font *defaultFnt, const char *s);
    Vec<Page *> *LayoutInternal(Graphics *graphics, Font *defaultFnt, const uint8_t *s, size_t sLen);

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
    REAL pageDx, pageDy;
    REAL lineSpacing;
    REAL spaceDx;
    Graphics *gfx;
    Font *defaultFont;
    Font *currFont;

    // temporary state during layout process
    TextJustification justification;
    Vec<Page *> *pages;
    Page *currPage; // current page
    REAL x, y; // current position in a page
    WCHAR buf[512];
    int newLinesCount; // consecutive newlines
    Vec<StrDx> lineStringsDx;
};

#endif
