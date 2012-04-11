/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "HtmlFormatter.h"
#include "EpubDoc.h"
using namespace Gdiplus;
#include "GdiPlusUtil.h"
#include "MobiDoc.h"
#include "Mui.h"

#include "DebugLog.h"

/*
Given size of a page, we format html into a set of pages. We handle only a small
subset of html commonly present in ebooks.

Formatting is a delayed affair, divided into 2 stages.

1. We gather elements and their sizes for the current line. When we detect that
adding another element would overflow current line, we position elements in
current line (stage 2) and start a new line. When we detect that adding a new
line would overflow current page, we start a new page.

2. When we position elements in current line, we calculate their x/y positions.

Delaying this calculation until we have all elements of the line is necessary
to implement e.g. justification. It's also simpler to have formatting logic in
2 simpler phases than a single, more complicated step. We still need to make sure
that both stages use the same logic for determining line/page overflow, otherwise
elements will be drawn outside page bounds. This shouldn't be hard because only
stage 1 calculates the sizes of elements.
*/

/*
TODO: Instead of inserting explicit SetFont, StartLink, etc. instructions
at the beginning of every page, DrawHtmlPage could always start with
that page's styleStack.Last().font, etc.
The information that we need to remember:
* font name (if different from default font name, NULL otherwise)
* font size scale i.e. 1.f means "default font size". This is to allow the user to change
  default font size and allow us to relayout from arbitrary page
* font style (bold/italic etc.)
* a link url if we're carrying over a text for a link (NULL if no link)
* text color (when/if we support changing text color)
* more ?

TODO: reuse styleStack, listDepth, preFormatted from HtmlPage when restarting
layout from a reparseIdx

TODO: HtmlFormatter could be split into DrawInstrBuilder which knows pageDx, pageDy
and generates DrawInstr and splits them into pages and a better named class that
does the parsing of the document builds pages by invoking methods on DrawInstrBuilders.

TODO: support <figure> and <figcaption> as e.g in http://ebookarchitects.com/files/BookOfTexas.mobi

TODO: instead of generating list of DrawInstr objects, we could add neccessary
support to mui and use list of Control objects instead (especially if we slim down
Control objects further to make allocating hundreds of them cheaper or introduce some
other base element(s) with less functionality and less overhead).
*/

static bool ValidReparseIdx(int idx, HtmlPullParser *parser)
{
    // note: not the most compact version on purpose, to allow
    // setting a breakpoint on the path returning false
    if ((idx < 0) || (idx > (int)parser->Len()))
        return false;
    return true;
}

DrawInstr DrawInstr::Str(const char *s, size_t len, RectF bbox)
{
    DrawInstr di(InstrString, bbox);
    di.str.s = s;
    di.str.len = len;
    return di;
}

DrawInstr DrawInstr::SetFont(Font *font)
{
    DrawInstr di(InstrSetFont);
    di.font = font;
    return di;
}

DrawInstr DrawInstr::FixedSpace(float dx)
{
    DrawInstr di(InstrFixedSpace);
    di.bbox.Width = dx;
    return di;
}

DrawInstr DrawInstr::Image(char *data, size_t len, RectF bbox)
{
    DrawInstr di(InstrImage);
    di.img.data = data;
    di.img.len = len;
    di.bbox = bbox;
    return di;
}

DrawInstr DrawInstr::LinkStart(const char *s, size_t len)
{
    DrawInstr di(InstrLinkStart);
    di.str.s = s;
    di.str.len = len;
    return di;
}

DrawInstr DrawInstr::Anchor(const char *s, size_t len, RectF bbox)
{
    DrawInstr di(InstrAnchor);
    di.str.s = s;
    di.str.len = len;
    di.bbox = bbox;
    return di;
}

HtmlFormatter::HtmlFormatter(HtmlFormatterArgs *args) :
    pageDx(args->pageDx), pageDy(args->pageDy),
    textAllocator(args->textAllocator), currLineReparseIdx(NULL),
    currX(0), currY(0), currLineTopPadding(0), currLinkIdx(0),
    listDepth(0), preFormatted(false), currPage(NULL),
    finishedParsing(false), pageCount(0), measureAlgo(args->measureAlgo)
{
    currReparseIdx = args->reparseIdx;
    htmlParser = new HtmlPullParser(args->htmlStr, args->htmlStrLen);
    htmlParser->SetCurrPosOff(currReparseIdx);
    CrashIf(!ValidReparseIdx(currReparseIdx, htmlParser));

    gfx = mui::AllocGraphicsForMeasureText();
    defaultFontName.Set(str::Dup(args->fontName));
    defaultFontSize = args->fontSize;
    DrawStyle style;
    style.font = mui::GetCachedFont(defaultFontName, defaultFontSize, FontStyleRegular);
    style.align = Align_Justify;
    styleStack.Append(style);

    lineSpacing = CurrFont()->GetHeight(gfx);
    spaceDx = CurrFont()->GetSize() / 2.5f; // note: a heuristic
    float spaceDx2 = GetSpaceDx(gfx, CurrFont(), measureAlgo);
    if (spaceDx2 < spaceDx)
        spaceDx = spaceDx2;

    EmitNewPage();
}

HtmlFormatter::~HtmlFormatter()
{
    // delete all pages that were not consumed by the caller
    DeleteVecMembers(pagesToSend);
    delete currPage;
    delete htmlParser;
    mui::FreeGraphicsForMeasureText(gfx);
}

void HtmlFormatter::AppendInstr(DrawInstr di)
{
    currLineInstr.Append(di);
    if (-1 == currLineReparseIdx) {
        currLineReparseIdx = currReparseIdx;
        CrashIf(!ValidReparseIdx(currReparseIdx, htmlParser));
    }
}

void HtmlFormatter::SetFont(const WCHAR *fontName, FontStyle fs, float fontSize)
{
    if (fontSize < 0)
        fontSize = CurrFont()->GetSize();
    Font *newFont = mui::GetCachedFont(fontName, fontSize, fs);
    if (CurrFont() != newFont)
        AppendInstr(DrawInstr::SetFont(newFont));

    styleStack.Append(styleStack.Last());
    CurrStyle()->font = newFont;
}

void HtmlFormatter::SetFont(Font *font, FontStyle fs, float fontSize)
{
    LOGFONTW lfw;
    Status ok = CurrFont()->GetLogFontW(gfx, &lfw);
    const WCHAR *fontName = ok == Ok ? lfw.lfFaceName : defaultFontName;
    SetFont(fontName, fs, fontSize);
}

static bool ValidStyleForChangeFontStyle(FontStyle fs)
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
// TODO: it doesn't corrctly support the case where a style is wrongly nested
// like "<b>fo<i>oo</b>bar</i>" - "bar" should be italic but will be bold
void HtmlFormatter::ChangeFontStyle(FontStyle fs, bool addStyle)
{
    CrashAlwaysIf(!ValidStyleForChangeFontStyle(fs));
    if (addStyle)
        SetFont(CurrFont(), (FontStyle)(fs | CurrFont()->GetStyle()));
    else
        RevertStyleChange();
}

void HtmlFormatter::SetAlignment(AlignAttr align)
{
    styleStack.Append(styleStack.Last());
    CurrStyle()->align = align;
}

void HtmlFormatter::RevertStyleChange()
{
    if (styleStack.Count() > 1) {
        DrawStyle style = styleStack.Pop();
        if (style.font != CurrFont())
            AppendInstr(DrawInstr::SetFont(CurrFont()));
    }
}

static bool IsVisibleDrawInstr(DrawInstr *i)
{
    switch (i->type) {
        case InstrString:
        case InstrLine:
        case InstrImage:
            return true;
    }
    return false;
}

// sum of widths of all elements with a fixed size and flexible
// spaces (using minimum value for its width)
REAL HtmlFormatter::CurrLineDx()
{
    REAL dx = NewLineX();
    for (DrawInstr *i = currLineInstr.IterStart(); i; i = currLineInstr.IterNext()) {
        if (InstrString == i->type || InstrImage == i->type) {
            dx += i->bbox.Width;
        } else if (InstrElasticSpace == i->type) {
            dx += spaceDx;
        } else if (InstrFixedSpace == i->type) {
            dx += i->bbox.Width;
        }
    }
    return dx;
}

// return the height of the tallest element on the line
float HtmlFormatter::CurrLineDy()
{
    float dy = lineSpacing;
    for (DrawInstr *i = currLineInstr.IterStart(); i; i = currLineInstr.IterNext()) {
        if (IsVisibleDrawInstr(i)) {
            if (i->bbox.Height > dy)
                dy = i->bbox.Height;
        }
    }
    return dy;
}

// return the width of the left margin (used for paragraph
// indentation inside lists)
float HtmlFormatter::NewLineX()
{
    // TODO: indent based on font size instead?
    float x = 15.f * listDepth;
    if (x < pageDx - 20.f)
        return x;
    if (pageDx < 20.f)
        return 0.f;
    return pageDx - 20.f;
}

// When this is called, Width and Height of each element is already set
// We set position x of each visible element
void HtmlFormatter::LayoutLeftStartingAt(REAL offX)
{
    DrawInstr *lastInstr = NULL;
    int instrCount = 0;

    REAL x = offX + NewLineX();
    for (DrawInstr *i = currLineInstr.IterStart(); i; i = currLineInstr.IterNext()) {
        if (InstrString == i->type || InstrImage == i->type) {
            i->bbox.X = x;
            x += i->bbox.Width;
            lastInstr = i;
            instrCount++;
        } else if (InstrElasticSpace == i->type) {
            x += spaceDx;
        } else if (InstrFixedSpace == i->type) {
            x += i->bbox.Width;
        }
    }

    // center a single image
    if (instrCount == 1 && InstrImage == lastInstr->type)
        lastInstr->bbox.X = (pageDx - lastInstr->bbox.Width) / 2.f;
}

// TODO: if elements are of different sizes (e.g. texts using different fonts)
// we should align them according to the baseline (which we would first need to
// record for each element)
static void SetYPos(Vec<DrawInstr>& instr, float y)
{
    for (DrawInstr *i = instr.IterStart(); i; i = instr.IterNext()) {
        if (IsVisibleDrawInstr(i))
            i->bbox.Y = y;
    }
}

// Redistribute extra space in the line equally among the spaces
void HtmlFormatter::JustifyLineBoth()
{
    REAL extraSpaceDxTotal = pageDx - CurrLineDx();
    CrashIf(extraSpaceDxTotal < 0.f);

    LayoutLeftStartingAt(0.f);
    size_t spaces = 0;
    bool endsWithSpace = false;
    for (DrawInstr *i = currLineInstr.IterStart(); i; i = currLineInstr.IterNext()) {
        if (InstrElasticSpace == i->type) {
            ++spaces;
            endsWithSpace = true;
        }
        else if (InstrString == i->type || InstrImage == i->type)
            endsWithSpace = false;
    }
    // don't take a space at the end of the line into account 
    // (the last word is explicitly right-aligned below)
    if (endsWithSpace)
        spaces--;
    if (0 == spaces)
        return;
    // redistribute extra dx space among elastic spaces
    REAL extraSpaceDx = extraSpaceDxTotal / (float)spaces;
    float offX = 0.f;
    DrawInstr *lastStr = NULL;
    for (DrawInstr *i = currLineInstr.IterStart(); i; i = currLineInstr.IterNext()) {
        if (InstrElasticSpace == i->type)
            offX += extraSpaceDx;
        else if (InstrString == i->type || InstrImage == i->type) {
            i->bbox.X += offX;
            lastStr = i;
        }
    }
    // align the last element perfectly against the right edge in case
    // we've accumulated rounding errors
    if (lastStr)
        lastStr->bbox.X = pageDx - lastStr->bbox.Width;
}

bool HtmlFormatter::IsCurrLineEmpty()
{
    for (DrawInstr *i = currLineInstr.IterStart(); i; i = currLineInstr.IterNext()) {
        if (IsVisibleDrawInstr(i))
            return false;
    }
    return true;
}

void HtmlFormatter::JustifyCurrLine(AlignAttr align)
{
    switch (align) {
        case Align_Left:
            LayoutLeftStartingAt(0.f);
            break;
        case Align_Right:
            LayoutLeftStartingAt(pageDx - CurrLineDx());
            break;
        case Align_Center:
            LayoutLeftStartingAt((pageDx - CurrLineDx()) / 2.f);
            break;
        case Align_Justify:
            JustifyLineBoth();
            break;
        default:
            CrashIf(true);
            break;
    }
}

static RectF RectFUnion(RectF& r1, RectF& r2)
{
    if (r2.IsEmptyArea())
        return r1;
    if (r1.IsEmptyArea())
        return r2;
    RectF ru;
    ru.Union(ru, r1, r2);
    return ru;
}

void HtmlFormatter::UpdateLinkBboxes(HtmlPage *page)
{
    for (DrawInstr *i = page->instructions.IterStart(); i; i = page->instructions.IterNext()) {
        if (InstrLinkStart != i->type)
            continue;
        for (DrawInstr *i2 = i + 1; i2->type != InstrLinkEnd; i2++) {
            if (IsVisibleDrawInstr(i2))
                i->bbox = RectFUnion(i->bbox, i2->bbox);
        }
    }
}

void HtmlFormatter::ForceNewPage()
{
    bool createdNewPage = FlushCurrLine(true);
    if (createdNewPage)
        return;
    UpdateLinkBboxes(currPage);
    pagesToSend.Append(currPage);

    EmitNewPage();
    currX = NewLineX();
    currLineTopPadding = 0.f;
}

// returns true if created a new page
bool HtmlFormatter::FlushCurrLine(bool isParagraphBreak)
{
    if (IsCurrLineEmpty()) {
        currX = NewLineX();
        currLineTopPadding = 0;
        // remove all spaces (only keep SetFont, LinkStart and Anchor instructions)
        for (size_t k = currLineInstr.Count(); k > 0; k--) {
            DrawInstr *i = &currLineInstr.At(k-1);
            if (InstrFixedSpace == i->type || InstrElasticSpace == i->type)
                currLineInstr.RemoveAt(k-1);
        }
        return false;
    }
    AlignAttr align = CurrStyle()->align;
    if (isParagraphBreak && (Align_Justify == align))
        align = Align_Left;
    JustifyCurrLine(align);

    // create a new page if necessary
    float totalLineDy = CurrLineDy() + currLineTopPadding;
    bool createdPage = false;
    if (currY + totalLineDy > pageDy) {
        // current line too big to fit in current page,
        // so need to start another page
        UpdateLinkBboxes(currPage);
        pagesToSend.Append(currPage);
        // instructions for each page need to be self-contained
        // so we have to carry over some state (like current font)
        CrashIf(!CurrFont());
        EmitNewPage();
        currPage->reparseIdx = currLineReparseIdx;
        createdPage = true;
    }
    SetYPos(currLineInstr, currY + currLineTopPadding);
    currY += totalLineDy;

    DrawInstr link;
    if (currLinkIdx) {
        link = currLineInstr.At(currLinkIdx - 1);
        // TODO: this occasionally leads to empty links
        AppendInstr(DrawInstr(InstrLinkEnd));
    }
    currPage->instructions.Append(currLineInstr.LendData(), currLineInstr.Count());
    currLineInstr.Reset();
    currLineReparseIdx = -1; // mark as not set
    currLineTopPadding = 0;
    currX = NewLineX();
    if (currLinkIdx) {
        AppendInstr(DrawInstr::LinkStart(link.str.s, link.str.len));
        currLinkIdx = currLineInstr.Count();
    }
    return createdPage;
}

static DrawInstr *FindLastSetFontInstr(HtmlPage *page)
{
    if (!page)
        return NULL;
    Vec<DrawInstr> *els = &page->instructions;
    size_t n = els->Count();
    for (size_t i = 0; i < n; i++) {
        DrawInstr *di = &els->At(n - i - 1);
        if (InstrSetFont == di->type)
            return di;
    }
    return NULL;
}

void HtmlFormatter::EmitNewPage()
{
    HtmlPage *prevPage = currPage;
    currPage = new HtmlPage();
    currPage->reparseIdx = currReparseIdx;
    currPage->styleStack = styleStack;
    currPage->listDepth = listDepth;
    currPage->preFormatted = preFormatted;
    DrawInstr *lastSetFont = FindLastSetFontInstr(prevPage);
    if (lastSetFont)
        currPage->instructions.Append(*lastSetFont);
    else
        currPage->instructions.Append(DrawInstr::SetFont(CurrFont()));
    currY = 0.f;
}

void HtmlFormatter::EmitEmptyLine(float lineDy)
{
    CrashIf(!IsCurrLineEmpty());
    currY += lineDy;
    if (currY <= pageDy) {
        currX = NewLineX();
        return;
    }
    ForceNewPage();
}

void HtmlFormatter::EmitImage(ImageData *img)
{
    CrashIf(!img->data);
    Size imgSize = BitmapSizeFromData(img->data, img->len);
    if (imgSize.Empty())
        return;

    SizeF newSize((REAL)imgSize.Width, (REAL)imgSize.Height);
    // move overly large images to a new line
    if (!IsCurrLineEmpty() && currX + newSize.Width > pageDx)
        FlushCurrLine(false);
    // move overly large images to a new page
    if (currY > 0 && currY + newSize.Height / 2.f > pageDy)
        ForceNewPage();
    // if image is still bigger than available space, scale it down
    if ((newSize.Width > pageDx) || (currY + newSize.Height) > pageDy) {
        REAL scale = min(pageDx / newSize.Width, (pageDy - currY) / newSize.Height);
        newSize.Width *= scale;
        newSize.Height *= scale;
    }

    RectF bbox(PointF(currX, 0), newSize);
    AppendInstr(DrawInstr::Image(img->data, img->len, bbox));
    currX += bbox.Width;
}

// add horizontal line (<hr> in html terms)
void HtmlFormatter::EmitHr()
{
    // hr creates an implicit paragraph break
    FlushCurrLine(true);
    CrashIf(NewLineX() != currX);
    RectF bbox(0.f, 0.f, pageDx, lineSpacing);
    AppendInstr(DrawInstr(InstrLine, bbox));
    FlushCurrLine(true);
}

void HtmlFormatter::EmitParagraph(float indent)
{
    FlushCurrLine(true);
    CrashIf(NewLineX() != currX);
    bool needsIndent = Align_Left == CurrStyle()->align ||
                       Align_Justify == CurrStyle()->align;
    if (indent > 0 && needsIndent && EnsureDx(indent)) {
        AppendInstr(DrawInstr::FixedSpace(indent));
        currX = NewLineX() + indent;
    }
}

// ensure there is enough dx space left in the current line
// if there isn't, we start a new line
// returns false if dx is bigger than pageDx
bool HtmlFormatter::EnsureDx(float dx)
{
    if (currX + dx <= pageDx)
        return true;
    FlushCurrLine(false);
    return dx <= pageDx;
}

// don't emit multiple spaces and don't emit spaces
// at the beginning of the line
static bool CanEmitElasticSpace(float currX, float NewLineX, float maxCurrX, Vec<DrawInstr>& currLineInstr)
{
    if (NewLineX == currX)
        return false;
    // prevent elastic spaces from being flushed to the
    // beginning of the next line
    if (currX > maxCurrX)
        return false;
    DrawInstr& di = currLineInstr.Last();
    // don't add a space if only an anchor would be in between them
    if (InstrAnchor == di.type && currLineInstr.Count() > 1)
        di = currLineInstr.At(currLineInstr.Count() - 2);
    return (InstrElasticSpace != di.type) && (InstrFixedSpace != di.type);
}

void HtmlFormatter::EmitElasticSpace()
{
    if (!CanEmitElasticSpace(currX, NewLineX(), pageDx - spaceDx, currLineInstr))
        return;
    EnsureDx(spaceDx);
    currX += spaceDx;
    AppendInstr(DrawInstr(InstrElasticSpace));
}

// a text run is a string of consecutive text with uniform style
void HtmlFormatter::EmitTextRun(const char *s, const char *end)
{
    currReparseIdx = s - htmlParser->Start();
    CrashIf(!ValidReparseIdx(currReparseIdx, htmlParser));
    CrashIf(IsSpaceOnly(s, end) && !preFormatted);
    const char *tmp = ResolveHtmlEntities(s, end, textAllocator);
    bool resolved = tmp != s;
    if (resolved) {
        s = tmp;
        end = s + str::Len(s);
    }

    while (s < end) {
        // don't update the reparseIdx if s doesn't point into the original source
        if (!resolved)
            currReparseIdx = s - htmlParser->Start();

        size_t strLen = str::Utf8ToWcharBuf(s, end - s, buf, dimof(buf));
        RectF bbox = MeasureText(gfx, CurrFont(), buf, strLen, measureAlgo);
        EnsureDx(bbox.Width);
        if (bbox.Width <= pageDx - currX) {
            AppendInstr(DrawInstr::Str(s, end - s, bbox));
            currX += bbox.Width;
            break;
        }

        int lenThatFits = StringLenForWidth(gfx, CurrFont(), buf, strLen, pageDx - NewLineX(), measureAlgo);
        // try to prevent a break in the middle of a word
        if (iswalnum(buf[lenThatFits])) {
            for (int len = lenThatFits; len > 0; len--) {
                if (!iswalnum(buf[len-1])) {
                    lenThatFits = len;
                    break;
                }
            }
        }
        bbox = MeasureText(gfx, CurrFont(), buf, lenThatFits, measureAlgo);
        CrashIf(bbox.Width > pageDx);
        AppendInstr(DrawInstr::Str(s, lenThatFits, bbox));
        currX += bbox.Width;

        for (int i = 0; i < lenThatFits; i++) {
            // s is UTF-8 and buf is UTF-16, so one
            // WCHAR doesn't always equal one char
            s += buf[i] < 0x80 ? 1 : buf[i] < 0x800 ? 2 : 3;
        }
    }
}

// parses size in the form "1em" or "3pt". To interpret ems we need emInPoints
// to be passed by the caller
static float ParseSizeAsPixels(const char *s, size_t len, float emInPoints)
{
    float x = 0;
    float sizeInPoints = 0;
    if (str::Parse(s, len, "%fem", &x)) {
        sizeInPoints = x * emInPoints;
    } else if (str::Parse(s, len, "%fpt", &x)) {
        sizeInPoints = x;
    }
    // TODO: take dpi into account
    float sizeInPixels = sizeInPoints;
    return sizeInPixels;
}

void HtmlFormatter::HandleAnchorTag(HtmlToken *t, bool idsOnly)
{
    if (t->IsEndTag())
        return;

    AttrInfo *attr = t->GetAttrByName("id");
    if (!attr && !idsOnly && Tag_A == t->tag)
        attr = t->GetAttrByName("name");
    if (!attr)
        return;

    // TODO: make anchors more specific than the top of the current line?
    RectF bbox(0, currY, pageDx, 0);
    // append at the start of the line to prevent the anchor
    // from being flushed to the next page (with wrong currY value)
    currPage->instructions.Append(DrawInstr::Anchor(attr->val, attr->valLen, bbox));
}

void HtmlFormatter::HandleTagBr()
{
    // make sure to always emit a line
    if (IsCurrLineEmpty())
        EmitEmptyLine(lineSpacing);
    else
        FlushCurrLine(true);
}

void HtmlFormatter::HandleTagP(HtmlToken *t)
{
    if (!t->IsEndTag()) {
        AlignAttr align = CurrStyle()->align;
        AttrInfo *attr = t->GetAttrByName("align");
        if (attr) {
            AlignAttr just = FindAlignAttr(attr->val, attr->valLen);
            if (just != Align_NotFound)
                align = just;
        }
        SetAlignment(align);
        EmitParagraph(0);
    } else {
        FlushCurrLine(true);
        RevertStyleChange();
    }
}

void HtmlFormatter::HandleTagFont(HtmlToken *t)
{
    if (t->IsEndTag()) {
        RevertStyleChange();
        return;
    }

    AttrInfo *attr = t->GetAttrByName("face");
    LOGFONTW lfw;
    CurrFont()->GetLogFontW(gfx, &lfw);
    const WCHAR *faceName = lfw.lfFaceName;
    if (attr) {
        size_t strLen = str::Utf8ToWcharBuf(t->s, t->sLen, buf, dimof(buf));
        // multiple font names can be comma separated
        if (strLen > 0 && *buf != ',') {
            str::TransChars(buf, L",", L"\0");
            faceName = buf;
        }
    }

    float fontSize = CurrFont()->GetSize();
    attr = t->GetAttrByName("size");
    if (attr) {
        // the sizes are in the range from 1 (tiny) to 7 (huge)
        int size = 3; // normal size
        str::Parse(attr->val, attr->valLen, "%d", &size);
        // sizes can also be relative to the current size
        if (attr->valLen > 0 && ('-' == *attr->val || '+' == *attr->val))
            size += 3;
        size = limitValue(size, 1, 7);
        float scale = pow(1.2f, size - 3);
        fontSize = defaultFontSize * scale;
    }

    SetFont(faceName, (FontStyle)CurrFont()->GetStyle(), fontSize);
}

bool HtmlFormatter::HandleTagA(HtmlToken *t, const char *linkAttr)
{
    if (t->IsStartTag() && !currLinkIdx) {
        AttrInfo *attr = t->GetAttrByName(linkAttr);
        if (attr) {
            AppendInstr(DrawInstr::LinkStart(attr->val, attr->valLen));
            currLinkIdx = currLineInstr.Count();
            return true;
        }
    }
    else if (t->IsEndTag() && currLinkIdx) {
        AppendInstr(DrawInstr(InstrLinkEnd));
        currLinkIdx = 0;
        return true;
    }
    return false;
}

static bool IsTagH(HtmlTag tag)
{
    switch (tag) {
        case Tag_H1:
        case Tag_H2:
        case Tag_H3:
        case Tag_H4:
        case Tag_H5:
            return true;
    }
   return false;
 }

void HtmlFormatter::HandleTagHx(HtmlToken *t)
{
    if (t->IsEndTag()) {
        FlushCurrLine(true);
        currY += CurrFont()->GetSize() / 2;
        RevertStyleChange();
    }
    else {
        EmitParagraph(0);
        float fontSize = defaultFontSize * pow(1.1f, '5' - t->s[1]);
        if (currY > 0)
            currY += fontSize / 2;
        SetFont(CurrFont(), FontStyleBold, fontSize);
        CurrStyle()->align = Align_Left;
    }
}

void HtmlFormatter::HandleTagList(HtmlToken *t)
{
    FlushCurrLine(true);
    if (t->IsStartTag())
        listDepth++;
    else if (t->IsEndTag() && listDepth > 0)
        listDepth--;
    currX = NewLineX();
}

void HtmlFormatter::HandleTagPre(HtmlToken *t)
{
    FlushCurrLine(true);
    if (t->IsStartTag()) {
        SetFont(L"Courier New", (FontStyle)CurrFont()->GetStyle());
        CurrStyle()->align = Align_Left;
        preFormatted = true;
    }
    else if (t->IsEndTag()) {
        RevertStyleChange();
        preFormatted = false;
    }
}

void HtmlFormatter::HandleHtmlTag(HtmlToken *t)
{
    CrashAlwaysIf(!t->IsTag());

    HtmlTag tag = t->tag;

    if (Tag_P == tag) {
        HandleTagP(t);
    } else if (Tag_Hr == tag) {
        // imitating Kindle: hr is proceeded by an empty line
        FlushCurrLine(false);
        EmitEmptyLine(lineSpacing);
        EmitHr();
    } else if ((Tag_B == tag) || (Tag_Strong == tag)) {
        ChangeFontStyle(FontStyleBold, t->IsStartTag());
    } else if ((Tag_I == tag) || (Tag_Em == tag)) {
        ChangeFontStyle(FontStyleItalic, t->IsStartTag());
    } else if (Tag_U == tag) {
        if (!currLinkIdx)
            ChangeFontStyle(FontStyleUnderline, t->IsStartTag());
    } else if (Tag_Strike == tag) {
        ChangeFontStyle(FontStyleStrikeout, t->IsStartTag());
    } else if (Tag_Br == tag) {
        HandleTagBr();
    } else if (Tag_Font == tag) {
        HandleTagFont(t);
    } else if (Tag_A == tag) {
        HandleTagA(t);
    } else if (Tag_Blockquote == tag) {
        // TODO: implement me
        HandleTagList(t);
    } else if (Tag_Div == tag) {
        // TODO: implement me
        FlushCurrLine(true);
    } else if (IsTagH(tag)) {
        HandleTagHx(t);
    } else if (Tag_Sup == tag) {
        // TODO: implement me
    } else if (Tag_Sub == tag) {
        // TODO: implement me
    } else if (Tag_Span == tag) {
        // TODO: implement me
    } else if (Tag_Center == tag) {
        HandleTagP(t);
        if (!t->IsEndTag())
            CurrStyle()->align = Align_Center;
    } else if ((Tag_Ul == tag) || (Tag_Ol == tag)) {
        HandleTagList(t);
    } else if ((Tag_Li == tag) || (Tag_Dd == tag)) {
        // TODO: display bullet/number for Tag_Li
        FlushCurrLine(true);
    } else if (Tag_Dt == tag) {
        if (t->IsStartTag())
            EmitParagraph(15.f);
        else
            FlushCurrLine(true);
        ChangeFontStyle(FontStyleBold, t->IsStartTag());
        if (t->IsStartTag())
            CurrStyle()->align = Align_Left;
    } else if (Tag_Table == tag) {
        // TODO: implement me
        HandleTagList(t);
    } else if (Tag_Tr == tag) {
        // display tables row-by-row for now
        FlushCurrLine(true);
    } else if (Tag_Code == tag) {
        if (t->IsStartTag())
            SetFont(L"Courier New", (FontStyle)CurrFont()->GetStyle());
        else if (t->IsEndTag())
            RevertStyleChange();
    } else if (Tag_Pre == tag) {
        HandleTagPre(t);
    } else {
        // TODO: temporary debugging
        //lf("unhandled tag: %d", tag);
    }

    // any tag could contain anchor information
    HandleAnchorTag(t);
}

void HtmlFormatter::HandleText(HtmlToken *t)
{
    CrashIf(!t->IsText());
    bool skipped;
    const char *curr = t->s;
    const char *end = t->s + t->sLen;

    if (preFormatted) {
        // don't collapse whitespace and respect text newlines
        while (curr < end) {
            const char *text = curr;
            currReparseIdx = curr - htmlParser->Start();
            // skip to the next newline
            for (; curr < end && *curr != '\n'; curr++);
            if (curr < end && curr > text && *(curr - 1) == '\r')
                curr--;
            EmitTextRun(text, curr);
            if ('\n' == *curr || '\r' == *curr) {
                curr += '\r' == *curr ? 2 : 1;
                HandleTagBr();
            }
        }
        return;
    }

    // break text into runs i.e. chunks that are either all
    // whitespace or all non-whitespace
    while (curr < end) {
        // collapse multiple, consecutive white-spaces into a single space
        currReparseIdx = curr - htmlParser->Start();
        skipped = SkipWs(curr, end);
        if (skipped)
            EmitElasticSpace();

        const char *text = curr;
        currReparseIdx = curr - htmlParser->Start();
        skipped = SkipNonWs(curr, end);
        if (skipped)
            EmitTextRun(text, curr);
    }
}

// we ignore the content of <head>, <style> and <title> tags
bool HtmlFormatter::IgnoreText()
{
    for (HtmlTag *tag = htmlParser->tagNesting.IterStart(); tag; tag = htmlParser->tagNesting.IterNext()) {
        if ((Tag_Head == *tag) ||
            (Tag_Style == *tag) ||
            (Tag_Title == *tag)) {
                return true;
        }
    }
    return false;
}

// TODO: draw link in the appropriate format (blue text, underlined, should show hand cursor when
// mouse is over a link. There's a slight complication here: we only get explicit information about
// strings, not about the whitespace and we should underline the whitespace as well. Also the text
// should be underlined at a baseline
void DrawHtmlPage(Graphics *g, Vec<DrawInstr> *drawInstructions, REAL offX, REAL offY, bool showBbox, Color *textColor)
{
    Color kindleTextColor(0x5F, 0x4B, 0x32); // this color matches Kindle app
    if (!textColor)
        textColor = &kindleTextColor;
    SolidBrush brText(*textColor);
    Pen debugPen(Color(255, 0, 0), 1);
    //Pen linePen(Color(0, 0, 0), 2.f);
    Pen linePen(Color(0x5F, 0x4B, 0x32), 2.f);
    Font *font = NULL;

    WCHAR buf[512];
    PointF pos;
    DrawInstr *i;
    for (i = drawInstructions->IterStart(); i; i = drawInstructions->IterNext()) {
        RectF bbox = i->bbox;
        bbox.X += offX;
        bbox.Y += offY;
        if (InstrLine == i->type) {
            // hr is a line drawn in the middle of bounding box
            REAL y = floorf(bbox.Y + bbox.Height / 2.f + 0.5f);
            PointF p1(bbox.X, y);
            PointF p2(bbox.X + bbox.Width, y);
            if (showBbox)
                g->DrawRectangle(&debugPen, bbox);
            g->DrawLine(&linePen, p1, p2);
        } else if (InstrString == i->type) {
            size_t strLen = str::Utf8ToWcharBuf(i->str.s, i->str.len, buf, dimof(buf));
            bbox.GetLocation(&pos);
            if (showBbox)
                g->DrawRectangle(&debugPen, bbox);
            g->DrawString(buf, strLen, font, pos, NULL, &brText);
        } else if (InstrSetFont == i->type) {
            font = i->font;
        } else if ((InstrElasticSpace == i->type) ||
            (InstrFixedSpace == i->type) ||
            (InstrAnchor == i->type)) {
            // ignore
        } else if (InstrImage == i->type) {
            // TODO: cache the bitmap somewhere (?)
            Bitmap *bmp = BitmapFromData(i->img.data, i->img.len);
            if (bmp)
                g->DrawImage(bmp, bbox, 0, 0, (REAL)bmp->GetWidth(), (REAL)bmp->GetHeight(), UnitPixel);
            delete bmp;
        } else if (InstrLinkStart == i->type) {
            // TODO: set text color to blue
            REAL y = floorf(bbox.Y + bbox.Height + 0.5f);
            PointF p1(bbox.X, y);
            PointF p2(bbox.X + bbox.Width, y);
            Pen linkPen(*textColor);
            g->DrawLine(&linkPen, p1, p2);
        } else if (InstrLinkEnd == i->type) {
            // TODO: set text color back again
        } else {
            CrashIf(true);
        }
    }
}

/* Mobi-specific formatting methods */

MobiFormatter::MobiFormatter(HtmlFormatterArgs* args, MobiDoc *doc) :
    HtmlFormatter(args), doc(doc), coverImage(NULL)
{
    bool fromBeginning = (0 == args->reparseIdx);
    if (!doc || !fromBeginning)
        return;

    ImageData *img = doc->GetCoverImage();
    if (!img)
        return;

    // this is a heuristic that tries to filter images that are not
    // cover images, like in http://www.sethgodin.com/sg/docs/StopStealingDreams-SethGodin.mobi
    // TODO: a better way would be to only add the image if one isn't
    // present at the beginning of html
    Size size = BitmapSizeFromData(img->data, img->len);
    if ((size.Width < 320) || (size.Height < 200))
        return;

    coverImage = img;
    // TODO: vertically center the cover image?
    EmitImage(coverImage);
    ForceNewPage();
}

void MobiFormatter::HandleSpacing_Mobi(HtmlToken *t)
{
    if (t->IsEndTag())
        return;

    // best I can tell, in mobi <p width="1em" height="3pt> means that
    // the first line of the paragrap is indented by 1em and there's
    // 3pt top padding (the same seems to apply for <blockquote>)
    AttrInfo *attr = t->GetAttrByName("width");
    if (attr) {
        float lineIndent = ParseSizeAsPixels(attr->val, attr->valLen, CurrFont()->GetSize());
        // there are files with negative width which produces partially invisible
        // text, so don't allow that
        if (lineIndent > 0) {
            // this should replace the previously emitted paragraph/quote block
            EmitParagraph(lineIndent);
        }
    }
    attr = t->GetAttrByName("height");
    if (attr) {
        // for use it in FlushCurrLine()
        currLineTopPadding = ParseSizeAsPixels(attr->val, attr->valLen, CurrFont()->GetSize());
    }
}

// mobi format has image tags in the form:
// <img recindex="0000n" alt=""/>
// where recindex is the record number of pdb record
// that holds the image (within image record array, not a
// global record)
// TODO: handle alt attribute (?)
void MobiFormatter::HandleTagImg_Mobi(HtmlToken *t)
{
    // we allow formatting raw html which can't require doc
    if (!doc)
        return;

    AttrInfo *attr = t->GetAttrByName("recindex");
    if (!attr)
        return;
    int n = 0;
    if (!str::Parse(attr->val, attr->valLen, "%d", &n))
        return;
    ImageData *img = doc->GetImage(n);
    if (!img)
        return;

    // if the image we're adding early on is the same as cover
    // image, then skip it. 5 is a heuristic
    if ((img == coverImage) && (pageCount < 5))
        return;

    EmitImage(img);
}

void MobiFormatter::HandleHtmlTag_Mobi(HtmlToken *t)
{
    CrashAlwaysIf(!t->IsTag());

    if (Tag_P == t->tag || Tag_Blockquote == t->tag) {
        HandleHtmlTag(t);
        HandleSpacing_Mobi(t);
    } else if (Tag_Mbp_Pagebreak == t->tag) {
        ForceNewPage();
    } else if (Tag_Img == t->tag) {
        HandleTagImg_Mobi(t);
        HandleAnchorTag(t);
    } else if (Tag_A == t->tag) {
        HandleAnchorTag(t);
        // handle internal and external links (prefer internal ones)
        if (!HandleTagA(t, "filepos"))
            HandleTagA(t);
    } else {
        HandleHtmlTag(t);
    }
}

// empty page is one that consists of only invisible instructions
static bool IsEmptyPage(HtmlPage *p)
{
    if (!p)
        return false;
    DrawInstr *i;
    for (i = p->instructions.IterStart(); i; i = p->instructions.IterNext()) {
        // if a page only consits of lines we consider it empty. It's different
        // than what Kindle does but I don't see the purpose of showing such
        // pages to the user
        if (InstrLine == i->type)
            continue;
        if (IsVisibleDrawInstr(i))
            return false;
    }
    // all instructions were invisible
    return true;
}

// Return the next parsed page. Returns NULL if finished parsing.
// For simplicity of implementation, we parse xml text node or
// xml element at a time. This might cause a creation of one
// or more pages, which we remeber and send to the caller
// if we detect accumulated pages.
HtmlPage *MobiFormatter::Next()
{
    for (;;)
    {
        // send out all pages accumulated so far
        while (pagesToSend.Count() > 0) {
            HtmlPage *ret = pagesToSend.At(0);
            pagesToSend.RemoveAt(0);
            pageCount++;
            if (IsEmptyPage(ret))
                delete ret;
            else
                return ret;
        }
        // we can call ourselves recursively to send outstanding
        // pages after parsing has finished so this is to detect
        // that case and really end parsing
        if (finishedParsing)
            return NULL;
        HtmlToken *t = htmlParser->Next();
        if (!t || t->IsError())
            break;

        currReparseIdx = t->GetReparsePoint() - htmlParser->Start();
        CrashIf(!ValidReparseIdx(currReparseIdx, htmlParser));
        if (t->IsTag()) {
            HandleHtmlTag_Mobi(t);
        } else {
            if (!IgnoreText())
                HandleText(t);
        }
    }
    // force layout of the last line
    FlushCurrLine(true);

    UpdateLinkBboxes(currPage);
    pagesToSend.Append(currPage);
    currPage = NULL;
    // call ourselves recursively to return accumulated pages
    finishedParsing = true;
    return Next();
}

// convenience method to format the whole html
Vec<HtmlPage*> *MobiFormatter::FormatAllPages()
{
    Vec<HtmlPage *> *pages = new Vec<HtmlPage *>();
    for (HtmlPage *pd = Next(); pd; pd = Next()) {
        pages->Append(pd);
    }
    return pages;
}

void EpubFormatter::HandleTagImg_Epub(HtmlToken *t)
{
    CrashIf(!epubDoc);
    if (t->IsEndTag())
        return;
    AttrInfo *attr = t->GetAttrByName("src");
    if (!attr)
        return;
    ScopedMem<char> src(str::DupN(attr->val, attr->valLen));
    ImageData *img = epubDoc->GetImageData(src, pagePath);
    if (img)
        EmitImage(img);
}

void EpubFormatter::HandleHtmlTag_Epub(HtmlToken *t)
{
    if (Tag_Img == t->tag) {
        HandleTagImg_Epub(t);
        HandleAnchorTag(t);
    }
    else if (Tag_Pagebreak == t->tag) {
        AttrInfo *attr = t->GetAttrByName("page_path");
        if (!attr || pagePath)
            ForceNewPage();
        if (attr) {
            RectF bbox(0, currY, pageDx, 0);
            currPage->instructions.Append(DrawInstr::Anchor(attr->val, attr->valLen, bbox));
            pagePath.Set(str::DupN(attr->val, attr->valLen));
        }
    }
    else
        HandleHtmlTag(t);
}

// TODO: replace with a single HtmlFormatter::FormatAllPages()
// implemented like MobiFormatter::FormatAllPages() (uses Next())
Vec<HtmlPage*> *EpubFormatter::FormatAllPages()
{
    HtmlToken *t;
    while ((t = htmlParser->Next()) && !t->IsError()) {
        if (t->IsTag())
            HandleHtmlTag_Epub(t);
        else if (!IgnoreText())
            HandleText(t);
    }

    FlushCurrLine(true);
    UpdateLinkBboxes(currPage);
    pagesToSend.Append(currPage);
    currPage = NULL;

    Vec<HtmlPage *> *result = new Vec<HtmlPage *>(pagesToSend);
    pagesToSend.Reset();
    return result;
}

// Return the next parsed page. Returns NULL if finished parsing.
// For simplicity of implementation, we parse xml text node or
// xml element at a time. This might cause a creation of one
// or more pages, which we remeber and send to the caller
// if we detect accumulated pages.
// TODO: make this single implementation part of HtmlFormatter
// (we can make HandleHtmlTag() virtual to make this work)
HtmlPage *EpubFormatter::Next()
{
    for (;;)
    {
        // send out all pages accumulated so far
        while (pagesToSend.Count() > 0) {
            HtmlPage *ret = pagesToSend.At(0);
            pagesToSend.RemoveAt(0);
            pageCount++;
            if (IsEmptyPage(ret))
                delete ret;
            else
                return ret;
        }
        // we can call ourselves recursively to send outstanding
        // pages after parsing has finished so this is to detect
        // that case and really end parsing
        if (finishedParsing)
            return NULL;
        HtmlToken *t = htmlParser->Next();
        if (!t || t->IsError())
            break;

        currReparseIdx = t->GetReparsePoint() - htmlParser->Start();
        CrashIf(!ValidReparseIdx(currReparseIdx, htmlParser));
        if (t->IsTag()) {
            HandleHtmlTag_Epub(t);
        } else {
            if (!IgnoreText())
                HandleText(t);
        }
    }
    // force layout of the last line
    FlushCurrLine(true);

    UpdateLinkBboxes(currPage);
    pagesToSend.Append(currPage);
    currPage = NULL;
    // call ourselves recursively to return accumulated pages
    finishedParsing = true;
    return Next();
}
