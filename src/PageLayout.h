/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef PageLayout_h
#define PageLayout_h

#include "BaseUtil.h"
#include "Vec.h"

using namespace Gdiplus;

struct StringPos {
    StringPos() : s(NULL), len(0) {
    }
    StringPos(const char *s, size_t len, RectF bb) : s(s), len(len), bb(bb) {
    }
    const char *s;
    size_t len;
    RectF bb;
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
    Vec<StringPos> *strings;
};

struct WordInfo {
    const char *s;
    size_t len;
    bool IsNewline() {
        return ((len == 1) && (s[0] == '\n'));
    }
};

class WordsIter {
public:
    WordsIter(const char *s) : s(s) {
        Reset();
    }

    void Reset() {
        curr = s;
        len = strlen(s);
        left = len;
    }

    WordInfo *Next();

private:
    WordInfo wi;

    static const char *NewLineStr;
    const char *s;
    size_t len;

    const char *curr;
    size_t left;
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
        pages = NULL; p = NULL;
        x = y = 0;
    }

    Vec<Page *> *Layout(Graphics *graphics, Font *font, const char *string);

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

    // constant during layout process
    REAL pageDx, pageDy;
    REAL lineSpacing;
    REAL spaceDx;
    Graphics *g;
    Font *f;

    // temporary state during layout process
    TextJustification j;
    Vec<Page *> *pages;
    Page *p; // current page
    REAL x, y; // current position in a page
    WCHAR buf[512];
    int newLinesCount; // consecutive newlines
    Vec<StrDx> lineStringsDx;
};

#endif
