/* Copyright 2011-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "PageLayout.h"
#include "StrUtil.h"
#include "HtmlPullParser.h"
#include "Mui.h"
#include "StrUtil.h"
#include "HtmlPullParser.h"
#include "Scoped.h"

using namespace Gdiplus;
#include "GdiPlusUtil.h"

/*
TODO: instead of generating list of DrawInstr objects, we could add neccessary
support to mui and use list of Control objects instead (especially if we slim down
Control objects further to make allocating hundreds of them cheaper).
*/

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

class PageLayout
{
    enum TextJustification {
        Left, Right, Center, Both
    };

public:
    PageLayout();
    ~PageLayout();

    void Start(LayoutInfo* layoutInfo);
    PageData *Next();
#if 0
    size_t PageCount() const {
        return pageInstrOffset.Count();
    }

    DrawInstr *GetInstructionsForPage(size_t pageNo, DrawInstr *& endInstr) const {
        CrashIf(pageNo >= PageCount());
        size_t start = pageInstrOffset.At(pageNo);
        size_t end = instructions.Count(); // if the last page
        if (pageNo < PageCount() - 1)
            end = pageInstrOffset.At(pageNo + 1);
        CrashIf(end < start);
        size_t len = end - start;
        DrawInstr *ret = &instructions.At(start);
        endInstr = ret + len;
        return ret;
    }
#endif

private:
    void HandleHtmlTag(HtmlToken *t);
    void EmitText(HtmlToken *t);

    REAL GetCurrentLineDx();
    void LayoutLeftStartingAt(REAL offX);
    void JustifyLineLeft();
    void JustifyLineRight();
    void JustifyLineCenter();
    void JustifyLineBoth();
    void JustifyLine(TextJustification mode);

    TextJustification AlignAttrToJustification(AlignAttr align);

    void StartLayout(LayoutInfo* layoutInfo);
    void StartNewPage();
    void StartNewLine(bool isParagraphBreak);

    void AddSetFontInstr(Font *font);

    void AddHr();
    void AddWord(WordInfo *wi);

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
    INewPageObserver *  pageObserver;
    REAL                pageDx;
    REAL                pageDy;
    REAL                lineSpacing;
    REAL                spaceDx;
    Graphics *          gfx; // for measuring text
    ScopedMem<WCHAR>    fontName;
    float               fontSize;

    // temporary state during layout process
    FontStyle           currFontStyle;
    Font *              currFont;

    TextJustification   currJustification;
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
};

PageLayout::PageLayout() : currPage(NULL), gfx(NULL)
{
}

PageLayout::~PageLayout()
{
    mui::FreeGraphicsForMeasureText(gfx);
}

void PageLayout::SetCurrentFont(FontStyle fs)
{
    currFontStyle = fs;
    currFont = mui::GetCachedFont(fontName.Get(), fontSize, fs);
}

static bool ValidFontStyleForChangeFont(FontStyle fs)
{
    if ((FontStyleBold == fs) ||
        (FontStyleItalic == fs) ||
        (FontStyleUnderline == fs) ||
        (FontStyleStrikeout == fs)) {
            return true;
    }
    return false;
}

// change the current font by adding (if addStyle is true) or removing
// a given font style from current font style
// TODO: it doesn't support the case where the same style is nested
// like "<b>fo<b>oo</b>bar</b>" - "bar" should still be bold but wont
// We would have to maintain counts for each style to do it fully right
void PageLayout::ChangeFont(FontStyle fs, bool addStyle)
{
    CrashAlwaysIf(!ValidFontStyleForChangeFont(fs));
    FontStyle newFontStyle = currFontStyle;
    if (addStyle)
        newFontStyle = (FontStyle) (newFontStyle | fs);
    else
        newFontStyle = (FontStyle) (newFontStyle & ~fs);

    if (newFontStyle == currFontStyle)
        return; // a no-op
    SetCurrentFont(newFontStyle);
    AddSetFontInstr(currFont);
}

void PageLayout::StartLayout(LayoutInfo* layoutInfo)
{
    pageObserver = layoutInfo->observer;
    pageDx = (REAL)layoutInfo->pageDx;
    pageDy = (REAL)layoutInfo->pageDy;

    CrashIf(gfx);
    gfx = mui::AllocGraphicsForMeasureText();
    fontName.Set(str::Dup(layoutInfo->fontName));
    fontSize = layoutInfo->fontSize;
    htmlParser = new HtmlPullParser(layoutInfo->htmlStr, layoutInfo->htmlStrLen);

    finishedParsing = false;
    currJustification = Both;
    SetCurrentFont(FontStyleRegular);

    CrashIf(currPage);
    lineSpacing = currFont->GetHeight(gfx);
    // note: this is heuristic that seems to work better than
    //  GetSpaceDx(gfx, currFont) (which seems way too big and is
    // bigger than what Kindle app uses)
    spaceDx = fontSize / 2.5f;
    StartNewPage();
}

void PageLayout::StartNewPage()
{
    if (currPage && pageObserver)
        pageObserver->NewPage(currPage);

    currPage = new PageData;
    currX = currY = 0;
    newLinesCount = 0;
    // instructions for each page need to be self-contained
    // so we have to carry over some state like the current font
    CrashIf(!currFont);
    AddSetFontInstr(currFont);
    currLineInstrOffset = currPage->Count();
}

REAL PageLayout::GetCurrentLineDx()
{
    REAL dx = -spaceDx;
    DrawInstr *end;
    DrawInstr *currInstr = GetInstructionsForCurrentLine(end);
    while (currInstr < end) {
        if (InstrTypeString == currInstr->type) {
            dx += currInstr->bbox.Width;
            dx += spaceDx;
        }
        ++currInstr;
    }
    if (dx < 0)
        dx = 0;
    return dx;
}

void PageLayout::LayoutLeftStartingAt(REAL offX)
{
    currX = offX;
    DrawInstr *end;
    DrawInstr *currInstr = GetInstructionsForCurrentLine(end);
    while (currInstr < end) {
        if (InstrTypeString == currInstr->type) {
            // currInstr Width and Height are already set
            currInstr->bbox.X = currX;
            currInstr->bbox.Y = currY;
            currX += (currInstr->bbox.Width + spaceDx);
        }
        ++currInstr;
    }
}

void PageLayout::JustifyLineLeft()
{
    LayoutLeftStartingAt(0);
}

void PageLayout::JustifyLineRight()
{
    REAL margin = pageDx - GetCurrentLineDx();
    LayoutLeftStartingAt(margin);
}

void PageLayout::JustifyLineCenter()
{
    REAL margin = (pageDx - GetCurrentLineDx());
    LayoutLeftStartingAt(margin / 2.f);
}

void PageLayout::JustifyLineBoth()
{
    // move all words proportionally to the right so that the
    // spacing remains uniform and the last word touches the
    // right page border
    REAL margin = pageDx - GetCurrentLineDx();
    LayoutLeftStartingAt(0);
    DrawInstr *end;
    DrawInstr *c = GetInstructionsForCurrentLine(end);
    size_t count = end - c;
    REAL extraSpaceDx = count > 1 ? margin / (count - 1) : margin;
    ++c;
    size_t n = 1;
    while (c < end) {
        c->bbox.X += n * extraSpaceDx;
        ++n;
        ++c;
    }
}

void PageLayout::JustifyLine(TextJustification mode)
{
    if (IsCurrentLineEmpty())
        return;

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
    currLineInstrOffset = currPage->Count();
}

void PageLayout::StartNewLine(bool isParagraphBreak)
{
    // don't put empty lines at the top of the page
    if ((0 == currY) && IsCurrentLineEmpty())
        return;

    if (isParagraphBreak && Both == currJustification)
        JustifyLine(Left);
    else
        JustifyLine(currJustification);

    currX = 0;
    currY += lineSpacing;
    currLineInstrOffset = currPage->Count();
    if (currY + lineSpacing > pageDy)
        StartNewPage();
}

struct KnownAttrInfo {
    HtmlAttr        attr;
    const char *    val;
    size_t          valLen;
};

static bool IsAllowedAttribute(HtmlAttr* allowedAttributes, HtmlAttr attr)
{
    while (Attr_NotFound != *allowedAttributes) {
        if (attr == *allowedAttributes++)
            return true;
    }
    return false;
}

static void GetKnownAttributes(HtmlToken *t, HtmlAttr *allowedAttributes, Vec<KnownAttrInfo> *out)
{
    out->Reset();
    AttrInfo *attrInfo;
    size_t tagLen = GetTagLen(t->s, t->sLen);
    const char *curr = t->s + tagLen;
    const char *end = t->s + t->sLen;
    for (;;) {
        attrInfo = GetNextAttr(curr, end);
        if (NULL == attrInfo)
            break;
        HtmlAttr attr = FindAttr(attrInfo->name, attrInfo->nameLen);
        if (!IsAllowedAttribute(allowedAttributes, attr))
            continue;
        KnownAttrInfo knownAttr = { attr, attrInfo->val, attrInfo->valLen };
        out->Append(knownAttr);
    }
}

void PageLayout::AddSetFontInstr(Font *font)
{
    currPage->Append(DrawInstr::SetFont(font));
}

// add horizontal line (<hr> in html terms)
void PageLayout::AddHr()
{
    // hr creates an implicit paragraph break
    StartNewLine(true);
    currX = 0;
    // height of hr is lineSpacing. If drawing it a current position
    // would exceede page bounds, go to another page
    if (currY + lineSpacing > pageDy)
        StartNewPage();

    RectF bbox(currX, currY, pageDx, lineSpacing);
    currPage->Append(DrawInstr::Line(bbox));
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
            bool needsTwo = (currX != 0);
            StartNewLine(true);
            if (needsTwo)
                StartNewLine(true);
        }
        return;
    }
    newLinesCount = 0;

    // TODO: check if the string contains html entities. If it does,
    // decode the entity, create a copy of decoded string in memory
    // that will packaged with pages information

    size_t strLen = str::Utf8ToWcharBuf(wi->s, wi->len, buf, dimof(buf));
    bbox = MeasureText(gfx, currFont, buf, strLen);
    // TODO: handle a case where a single word is bigger than the whole
    // line, in which case it must be split into multiple lines
    REAL dx = bbox.Width;
    if (currX + dx > pageDx) {
        // start new line if the new text would exceed the line length
        StartNewLine(false);
    }
    bbox.Y = currY;
    currPage->Append(DrawInstr::Str(wi->s, wi->len, bbox));
    currX += (dx + spaceDx);
}

#if 0
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
#endif

#if 0
void DumpAttr(uint8 *s, size_t sLen)
{
    static Vec<char *> seen;
    char *sCopy = str::DupN((char*)s, sLen);
    bool didSee = false;
    for (size_t i = 0; i < seen.Count(); i++) {
        char *tmp = seen.At(i);
        if (str::EqI(sCopy, tmp)) {
            didSee = true;
            break;
        }
    }
    if (didSee) {
        free(sCopy);
        return;
    }
    seen.Append(sCopy);
    printf("%s\n", sCopy);
}
#endif

// tags that I want to explicitly ignore and not define
// HtmlTag enums for them
// One file has a bunch of st1:* tags (st1:city, st1:place etc.)
static bool IgnoreTag(const char *s, size_t sLen)
{
    if (sLen >= 4 && s[3] == ':' && s[0] == 's' && s[1] == 't' && s[2] == '1')
        return true;
    // no idea what "o:p" is
    if (sLen == 3 && s[1] == ':' && s[0] == 'o'  && s[2] == 'p')
        return true;
    return false;
}

PageLayout::TextJustification PageLayout::AlignAttrToJustification(AlignAttr align)
{
    switch (align) {
        case Align_Center:
            return Center;
        case Align_Justify:
            return Both;
        case Align_Left:
            return Left;
        case Align_Right:
            return Right;
        default:
            return Both;
    }
}

void PageLayout::HandleHtmlTag(HtmlToken *t)
{
    Vec<KnownAttrInfo> attrs;
    CrashAlwaysIf(!t->IsTag());

    // HtmlToken string includes potential attributes,
    // get the length of just the tag
    size_t tagLen = GetTagLen(t->s, t->sLen);
    if (IgnoreTag(t->s, tagLen))
        return;

    HtmlTag tag = FindTag((char*)t->s, tagLen);
    // TODO: ignore instead of crashing once we're satisfied we covered all the tags
    CrashIf(tag == Tag_NotFound);

    // update the current state of html tree
    if (t->IsStartTag())
        RecordStartTag(&tagNesting, tag);
    else if (t->IsEndTag())
        RecordEndTag(&tagNesting, tag);

    if (Tag_P == tag) {
        TextJustification newJustification = Both;
        if (t->IsStartTag() || t->IsEmptyElementEndTag()) {
            StartNewLine(true);
            static HtmlAttr validAttrs[] = { Attr_Align, Attr_NotFound };
            GetKnownAttributes(t, validAttrs, &attrs);
            if (attrs.Count() > 0) {
                KnownAttrInfo attr = attrs.At(0);
                AlignAttr alignAttr = FindAlignAttr(attr.val, attr.valLen);
                newJustification = AlignAttrToJustification(alignAttr);
            }
        } else if (t->IsEndTag()) {
            StartNewLine(false);
        }
        currJustification = newJustification;
        return;
    }

    if (Tag_Hr == tag) {
        AddHr();
        return;
    }

    if ((Tag_B == tag) || (Tag_Em == tag)) {
        ChangeFont(FontStyleBold, t->IsStartTag());
        return;
    }

    if (Tag_I == tag) {
        ChangeFont(FontStyleItalic, t->IsStartTag());
        return;
    }

    if (Tag_U == tag) {
        ChangeFont(FontStyleUnderline, t->IsStartTag());
        return;
    }

    if (Tag_Strike == tag) {
        ChangeFont(FontStyleStrikeout, t->IsStartTag());
        return;
    }

    if (Tag_Mbp_Pagebreak == tag) {
        JustifyLine(currJustification);
        StartNewPage();
        return;
    }
}

void PageLayout::EmitText(HtmlToken *t)
{
    CrashIf(!t->IsText());
    const char *end = t->s + t->sLen;
    const char *curr = t->s;
    SkipWs(curr, end);
    while (curr < end) {
        const char *currStart = curr;
        SkipNonWs(curr, end);
        size_t len = curr - currStart;
        if (len > 0) {
            WordInfo wi = { currStart, len };
            AddWord(&wi);
        }
        SkipWs(curr, end);
    }
}

// Return the next parsed page. Returns NULL if finished parsing.
// For simplicity of implementation, we parse xml text node or
// xml element at a time. This might cause a creation of one
// or more pages, which we remeber and send to the caller
// if we detect accumulated pages.
PageData *PageLayout::Next()
{
    for (;;)
    {
        if (pagesToSend.Count() > 0) {
            PageData *ret = pagesToSend.At(0);
            pagesToSend.RemoveAt(0);
            return ret;
        }
        if (finishedParsing)
            return NULL;
        HtmlToken *t = htmlParser->Next();
        if (!t || t->IsError())
            break;

        if (t->IsTag())
            HandleHtmlTag(t);
        else
            EmitText(t);
    }
    // force layout of the last line
    StartNewLine(true);

    finishedParsing = true;

    // only send the last page if not empty
    if (currPage && currPage->Count() > 0) {
        pagesToSend.Append(currPage);
        currPage = NULL;
        return Next();
    }
    return NULL;
}

Vec<PageData*> *LayoutHtml(LayoutInfo* li)
{
    Vec<PageData*> *pages = new Vec<PageData*>();
    PageLayout l;
    l.StartLayout(layoutInfo);
    PageData *pd;
    for (;;) {
        pd = Next();
        if (!pd)
            break;
        pages->Append(pd);
    }
    return pages;
}

void DrawPageLayout(Graphics *g, Vec<DrawInstr> *drawInstructions, REAL offX, REAL offY, bool showBbox)
{
    StringFormat sf(StringFormat::GenericTypographic());
    SolidBrush br(Color(0,0,0));
    SolidBrush br2(Color(255, 255, 255, 255));
    Pen pen(Color(255, 0, 0), 1);
    Pen blackPen(Color(0, 0, 0), 1);

    Font *font = NULL;

    WCHAR buf[512];
    PointF pos;
    for (DrawInstr *instr = drawInstructions->IterStart(); instr; instr = drawInstructions->IterNext()) {
        RectF bbox = instr->bbox;
        bbox.X += offX;
        bbox.Y += offY;
        if (InstrTypeLine == instr->type) {
            // hr is a line drawn in the middle of bounding box
            REAL y = bbox.Y + bbox.Height / 2.f;
            PointF p1(bbox.X, y);
            PointF p2(bbox.X + bbox.Width, y);
            if (showBbox) {
                //g->FillRectangle(&br, bbox);
                g->DrawRectangle(&pen, bbox);
            }
            g->DrawLine(&blackPen, p1, p2);
        } else if (InstrTypeString == instr->type) {
            size_t strLen = str::Utf8ToWcharBuf(instr->str.s, instr->str.len, buf, dimof(buf));
            bbox.GetLocation(&pos);
            if (showBbox) {
                //g->FillRectangle(&br, bbox);
                g->DrawRectangle(&pen, bbox);
            }
            g->DrawString(buf, strLen, font, pos, NULL, &br);
        } else if (InstrTypeSetFont == instr->type) {
            font = instr->setFont.font;
        }
    }
}
