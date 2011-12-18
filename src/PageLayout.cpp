/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include <Windows.h>
#include <GdiPlus.h>

#include "PageLayout.h"
#include "GdiPlusUtil.h"
#include "StrUtil.h"

using namespace Gdiplus;

const char *WordsIter::NewLineStr = "\n";

static void SkipCharInStr(const char *& s, size_t& left, char c)
{
    while ((left > 0) && (*s == c)) {
        ++s; --left;
    }
}

static bool IsWordBreak(char c)
{
    return (c == ' ') || (c == '\n') || (c == '\r');
}

static void SkipNonWordBreak(const char *& s, size_t& left)
{
    while ((left > 0) && !IsWordBreak(*s)) {
        ++s; --left;
    }
}

// return true if s points to "\n", "\r" or "\r\n"
// and advance s/left to skip it
// We don't want to collapse multiple consequitive newlines into
// one as we want to be able to detect paragraph breaks (i.e. empty
// newlines i.e. a newline following another newline)
static bool IsNewlineSkip(const char *& s, size_t& left)
{
    if (0 == left)
        return false;
    if ('\r' == *s) {
        --left; ++s;
        if ((left > 0) && ('\n' == *s)) {
            --left; ++s;
        }
        return true;
    }
    if ('\n' == *s) {
        --left; ++s;
        return true;
    }
    return false;
}

// iterates words in a string e.g. "foo bar\n" returns "foo", "bar" and "\n"
// also unifies line endings i.e. "\r" and "\r\n" are turned into a single "\n"
// returning NULL means end of iterations
WordInfo *WordsIter::Next()
{
    SkipCharInStr(curr, left, ' ');
    if (0 == left)
        return NULL;
    assert(*curr != 0);
    if (IsNewlineSkip(curr, left)) {
        wi.len = 1;
        wi.s = NewLineStr;
        return &wi;
    }
    wi.s = curr;
    SkipNonWordBreak(curr, left);
    wi.len = curr - wi.s;
    assert(wi.len > 0);
    return &wi;
}

void PageLayout::StartLayout()
{
    j = Both;
    pages = new Vec<Page*>();
    lineSpacing = f->GetHeight(g);
    spaceDx = GetSpaceDx(g, f);
    StartNewPage();
}

void PageLayout::StartNewPage()
{
    x = y = 0;
    newLinesCount = 0;
    p = new Page((int)pageDx, (int)pageDy);
    pages->Append(p);
}

REAL PageLayout::GetTotalLineDx()
{
    REAL dx = -spaceDx;
    for (size_t i = 0; i < lineStringsDx.Count(); i++) {
        StrDx sdx = lineStringsDx.At(i);
        dx += sdx.dx;
        dx += spaceDx;
    }
    return dx;
}

void PageLayout::LayoutLeftStartingAt(REAL offX)
{
    x = offX;
    for (size_t i = 0; i < lineStringsDx.Count(); i++) {
        StrDx sdx = lineStringsDx.At(i);
        RectF bb(x, y, sdx.dx, sdx.dy);
        StringPos sp(sdx.s, sdx.len, bb);
        p->strings->Append(sp);
        x += (sdx.dx + spaceDx);
    }
}

void PageLayout::JustifyLineLeft()
{
    LayoutLeftStartingAt(0);
}

void PageLayout::JustifyLineRight()
{
    REAL margin = pageDx - GetTotalLineDx();
    LayoutLeftStartingAt(margin);
}

void PageLayout::JustifyLineCenter()
{
    REAL margin = (pageDx - GetTotalLineDx());
    LayoutLeftStartingAt(margin / 2.f);
}

void PageLayout::JustifyLineBoth()
{
    REAL extraDxSpace = (pageDx - GetTotalLineDx()) / (REAL)(lineStringsDx.Count() - 1);
    size_t middleString = lineStringsDx.Count() / 2;

    // first half of strings are laid out starting from left
    x = 0;
    for (size_t i = 0; i <= middleString; i++) {
        StrDx sdx = lineStringsDx.At(i);
        RectF bb(x, y, sdx.dx, sdx.dy);
        StringPos sp(sdx.s, sdx.len, bb);
        p->strings->Append(sp);
        x += (sdx.dx + spaceDx);
    }

    // second half of strings are laid out from right
    x = pageDx;
    for (size_t i = lineStringsDx.Count() - 1; i > middleString; i--) {
        StrDx sdx = lineStringsDx.At(i);
        x -= sdx.dx;
        RectF bb(x, y, sdx.dx, sdx.dy);
        StringPos sp(sdx.s, sdx.len, bb);
        p->strings->Append(sp);
        x -= (spaceDx + extraDxSpace);
    }
}

void PageLayout::JustifyLine()
{
    if (0 == lineStringsDx.Count())
        return; // nothing to do
    switch (j) {
        case Left:
            JustifyLineLeft();
            break;
        case Right:
            JustifyLineRight();
            break;
        case Center:
            JustifyLineCenter();
            break;
        case Both:
            JustifyLineBoth();
            break;
        default:
            assert(0);
            break;
    }
    lineStringsDx.Reset();
}

void PageLayout::StartNewLine(bool isParagraphBreak)
{
    if (isParagraphBreak && Both == j)
        JustifyLineLeft();
    else
        JustifyLine();

    x = 0;
    y += lineSpacing;
    lineStringsDx.Reset();
    if (y > pageDy)
        StartNewPage();
}

void PageLayout::AddWord(WordInfo *wi)
{
    RectF bb;
    if (wi->IsNewline()) {
        // a single newline is considered "soft" and ignored
        // two or more consequitive newlines are considered a
        // single paragraph break
        newLinesCount++;
        if (2 == newLinesCount) {
            bool needsTwo = (x != 0);
            StartNewLine(true);
            if (needsTwo)
                StartNewLine(true);
        }
        return;
    }
    newLinesCount = 0;
    str::Utf8ToWcharBuf(wi->s, wi->len, buf, dimof(buf));
    bb = MeasureText(g, f, buf, wi->len);
    // TODO: handle a case where a single word is bigger than the whole
    // line, in which case it must be split into multiple lines
    REAL dx = bb.Width;
    if (x + dx > pageDx) {
        // start new line if the new text would exceed the line length
        StartNewLine(false);
    }
    StrDx sdx(wi->s, wi->len, dx, bb.Height);
    lineStringsDx.Append(sdx);
    x += (dx + spaceDx);
}

void PageLayout::RemoveLastPageIfEmpty()
{
    // TODO: write me
}

// How layout works: 
// * measure the strings
// * remember a line's worth of widths
// * when we fill a line we calculate the position of strings in
//   a line for a given justification setting (left, right, center, both)
Vec<Page*> *PageLayout::Layout(Graphics *g, Font *f, const char *s)
{
    this->g = g;
    this->f = f;
    StartLayout();
    WordsIter iter(s);
    for (;;) {
        WordInfo *wi = iter.Next();
        if (NULL == wi)
            break;
        AddWord(wi);
    }
    StartNewLine(true);
    RemoveLastPageIfEmpty();
    Vec<Page*> *ret = pages;
    pages = NULL;
    return ret;
}
