/* Copyright 2011-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "PageLayout.h"
#include "GdiPlusUtil.h"
#include "StrUtil.h"
#include "MobiHtmlParse.h"
#include "HtmlPullParser.h"

using namespace Gdiplus;

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
    justification = Both;
    assert(!pages);
    pages = new Vec<Page*>();
    lineSpacing = currFont->GetHeight(gfx);
    spaceDx = GetSpaceDx(gfx, currFont);
    StartNewPage();
}

void PageLayout::StartNewPage()
{
    x = y = 0;
    newLinesCount = 0;
    currPage = new Page((int)pageDx, (int)pageDy);
    pages->Append(currPage);
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
        RectF bbox(x, y, sdx.dx, sdx.dy);
        StringPos sp(sdx.s, sdx.len, bbox);
        currPage->strings->Append(sp);
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
    REAL margin = pageDx - GetTotalLineDx();
    size_t prevCount = currPage->strings->Count();
    LayoutLeftStartingAt(0);

    // move all words proportionally to the right so that the
    // spacing remains uniform and the last word touches the
    // right page border
    size_t count = currPage->strings->Count() - prevCount;
    REAL extraSpaceDx = count > 1 ? margin / (count - 1) : margin;
    for (size_t i = 1; i < count; i++) {
        currPage->strings->At(prevCount + i).bbox.X += i * extraSpaceDx;
    }
}

void PageLayout::JustifyLine(TextJustification mode)
{
    if (0 == lineStringsDx.Count())
        return; // nothing to do
    switch (mode) {
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
    if (isParagraphBreak && Both == justification)
        JustifyLine(Left);
    else
        JustifyLine(justification);

    x = 0;
    y += lineSpacing;
    lineStringsDx.Reset();
    if (y + lineSpacing > pageDy)
        StartNewPage();
}

// add horizontal line (<hr> in html terms)
void PageLayout::AddHr()
{
    // hr creates an implicit paragraph break
    StartNewLine(true);
    x = 0;
    // height of hr is lineSpacing. If drawing it a current position
    // would exceede page bounds, go to another page
    if (y + lineSpacing > pageDy)
        StartNewPage();

    RectF bbox(x, y, pageDx, lineSpacing);
    StringPos sp((const char*)Str_Hr, 0, bbox);

    currPage->strings->Append(sp);
    y += lineSpacing;
    StartNewLine(true);
}

void PageLayout::AddWord(WordInfo *wi)
{
    RectF bbox;
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
    size_t strLen = str::Utf8ToWcharBuf(wi->s, wi->len, buf, dimof(buf));
    bbox = MeasureText(gfx, currFont, buf, strLen);
    // TODO: handle a case where a single word is bigger than the whole
    // line, in which case it must be split into multiple lines
    REAL dx = bbox.Width;
    if (x + dx > pageDx) {
        // start new line if the new text would exceed the line length
        StartNewLine(false);
    }
    StrDx sdx(wi->s, wi->len, dx, bbox.Height);
    lineStringsDx.Append(sdx);
    x += (dx + spaceDx);
}

void PageLayout::RemoveLastPageIfEmpty()
{
    while (pages->Count() > 1 && pages->Last()->strings->Count() == 0)
        delete pages->Pop();
}

// How layout works: 
// * measure the strings
// * remember a line's worth of widths
// * when we fill a line we calculate the position of strings in
//   a line for a given justification setting (left, right, center, both)
Vec<Page*> *PageLayout::LayoutText(Graphics *graphics, Font *defaultFnt, const char *s)
{
    gfx = graphics;
    defaultFont = defaultFnt;
    currFont = defaultFnt;
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

// Describes a html attribute decoded from our internal format
// Attribute value is either represented by valEnum or arbitrary string
// described by val/valLen. Could have used a union for valEnum/{val/valLen}
// but there's not much win in that.
struct DecodedAttr {
    HtmlAttr        attr;
    int             valEnum;
    const uint8_t * val;
    size_t          valLen;
};

static void DecodeAttributes(Vec<DecodedAttr> *attrs, const uint8_t* &s, size_t& sLen)
{
    DecodedAttr attr;
    ParsedElement *parsedEl;
    CrashAlwaysIf(sLen < 1);

    const uint8_t *end = s + sLen;
    parsedEl = DecodeNextParsedElement(s, end);
    CrashAlwaysIf(ParsedElInt != parsedEl->type);
    int attrCount = parsedEl->n;
    attrs->Reset();
    for (int i = 0; i < attrCount; i++) {
        parsedEl = DecodeNextParsedElement(s, end);
        CrashAlwaysIf(ParsedElInt != parsedEl->type);
        attr.attr = parsedEl->attr;
        parsedEl = DecodeNextParsedElement(s, end);
        if (ParsedElInt == parsedEl->type) {
            CrashAlwaysIf(!AttrHasEnumVal(attr.attr));
            attr.valEnum = parsedEl->n;
        } else {
            attr.valLen = parsedEl->sLen;
            attr.val = parsedEl->s;
        }
        attrs->Append(attr);
    }
}

static DecodedAttr *FindAttr(Vec<DecodedAttr> *attrs, HtmlAttr attr)
{
    for (size_t i = 0; i < attrs->Count(); i++) {
        DecodedAttr a = attrs->At(i);
        if (a.attr == attr)
            return &attrs->At(i);
    }
    return NULL;
}

Vec<Page *> *PageLayout::LayoutInternal(Graphics *graphics, Font *defaultFnt, const uint8_t *s, size_t sLen)
{
    gfx = graphics;
    defaultFont = defaultFnt;
    currFont = defaultFnt;
    StartLayout();
    WordInfo wi;

    Vec<DecodedAttr> attrs;

    const uint8_t *end = s + sLen;
    // perf: pre-allocate lineStringsDx vector
    size_t estimatedStrings = sLen  / 4;
    lineStringsDx.EnsureCap(estimatedStrings);
    ParsedElement *parsedEl;
    for (;;) {
        parsedEl = DecodeNextParsedElement(s, end);
        if (!parsedEl)
            break;
        if (ParsedElString == parsedEl->type) {
            wi.s = (char*)parsedEl->s;
            wi.len = parsedEl->sLen;
            AddWord(&wi);
        } else {
            CrashAlwaysIf(ParsedElInt != parsedEl->type);
            HtmlTag tag = parsedEl->tag;
            uint8_t b = *s++;
            bool isEndTag = (IS_END_TAG_MASK == (b & IS_END_TAG_MASK));
            bool hasAttr  = (HAS_ATTR_MASK == (b & HAS_ATTR_MASK));
            if (hasAttr) {
                size_t sLen = end - s;
                DecodeAttributes(&attrs, s, sLen);
            }
            if ((Tag_P == tag) && isEndTag) {
                // TODO: collapse multiple line breaks into one
                // i.e. don't start a new paragraph if we're already
                // at the paragraph start (including the beginning)
                // This is visible in Kafka's Trial.
                StartNewLine(true);
            } else if (Tag_Hr == tag) {
                CrashAlwaysIf(isEndTag);
                AddHr();
            }
            // TODO: handle more codes
        }
    }
    StartNewLine(true);
    RemoveLastPageIfEmpty();
    Vec<Page*> *ret = pages;
    pages = NULL;
    return ret;
}
