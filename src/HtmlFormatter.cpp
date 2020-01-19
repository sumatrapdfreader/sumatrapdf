/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/GdiPlusUtil.h"
#include "utils/HtmlParserLookup.h"
#include "utils/CssParser.h"
#include "utils/HtmlPullParser.h"
#include "utils/Log.h"
#include "mui/Mui.h"
#include "utils/Timer.h"

// rendering engines
#include "EbookBase.h"
#include "HtmlFormatter.h"

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
that page's nextPageStyle.font, etc.
The information that we need to remember:
* font name (if different from default font name, nullptr otherwise)
* font size scale i.e. 1.f means "default font size". This is to allow the user to change
  default font size and allow us to relayout from arbitrary page
* font style (bold/italic etc.)
* a link url if we're carrying over a text for a link (nullptr if no link)
* text color (when/if we support changing text color)
* more ?

TODO: fix http://code.google.com/p/sumatrapdf/issues/detail?id=2183

TODO: HtmlFormatter could be split into DrawInstrBuilder which knows pageDx, pageDy
and generates DrawInstr and splits them into pages and a better named class that
does the parsing of the document builds pages by invoking methods on DrawInstrBuilders.

TODO: support <figure> and <figcaption> as e.g in http://ebookarchitects.com/files/BookOfTexas.mobi

TODO: instead of generating list of DrawInstr objects, we could add neccessary
support to mui and use list of Control objects instead (especially if we slim down
Control objects further to make allocating hundreds of them cheaper or introduce some
other base element(s) with less functionality and less overhead).
*/

bool ValidReparseIdx(ptrdiff_t idx, HtmlPullParser* parser) {
    if ((idx < 0) || (idx > (int)parser->Len()))
        return false;
    return true;
}

DrawInstr DrawInstr::Str(const char* s, size_t len, RectF bbox, bool rtl) {
    DrawInstr di(rtl ? DrawInstrType::RtlString : DrawInstrType::String, bbox);
    di.str.s = s;
    di.str.len = len;
    return di;
}

DrawInstr DrawInstr::SetFont(mui::CachedFont* font) {
    DrawInstr di(DrawInstrType::SetFont);
    di.font = font;
    return di;
}

DrawInstr DrawInstr::FixedSpace(float dx) {
    DrawInstr di(DrawInstrType::FixedSpace);
    di.bbox.Width = dx;
    return di;
}

DrawInstr DrawInstr::Image(char* data, size_t len, RectF bbox) {
    DrawInstr di(DrawInstrType::Image);
    di.img.data = data;
    di.img.len = len;
    di.bbox = bbox;
    return di;
}

DrawInstr DrawInstr::LinkStart(const char* s, size_t len) {
    DrawInstr di(DrawInstrType::LinkStart);
    di.str.s = s;
    di.str.len = len;
    return di;
}

DrawInstr DrawInstr::Anchor(const char* s, size_t len, RectF bbox) {
    DrawInstr di(DrawInstrType::Anchor);
    di.str.s = s;
    di.str.len = len;
    di.bbox = bbox;
    return di;
}

StyleRule::StyleRule() : tag(Tag_NotFound), textIndentUnit(inherit), textAlign(Align_NotFound) {
}

// parses size in the form "1em", "3pt" or "15px"
static void ParseSizeWithUnit(const char* s, size_t len, float* size, StyleRule::Unit* unit) {
    if (str::Parse(s, len, "%fem", size)) {
        *unit = StyleRule::em;
    } else if (str::Parse(s, len, "%fin", size)) {
        *unit = StyleRule::pt;
        *size *= 72; // 1 inch is 72 points
    } else if (str::Parse(s, len, "%fpt", size)) {
        *unit = StyleRule::pt;
    } else if (str::Parse(s, len, "%fpx", size)) {
        *unit = StyleRule::px;
    } else {
        *unit = StyleRule::inherit;
    }
}

StyleRule StyleRule::Parse(CssPullParser* parser) {
    StyleRule rule;
    const CssProperty* prop;
    while ((prop = parser->NextProperty()) != nullptr) {
        switch (prop->type) {
            case Css_Text_Align:
                rule.textAlign = FindAlignAttr(prop->s, prop->sLen);
                break;
            // TODO: some documents use Css_Padding_Left for indentation
            case Css_Text_Indent:
                ParseSizeWithUnit(prop->s, prop->sLen, &rule.textIndent, &rule.textIndentUnit);
                break;
        }
    }
    return rule;
}

StyleRule StyleRule::Parse(const char* s, size_t len) {
    CssPullParser parser(s, len);
    return Parse(&parser);
}

void StyleRule::Merge(StyleRule& source) {
    if (source.textAlign != Align_NotFound)
        textAlign = source.textAlign;
    if (source.textIndentUnit != StyleRule::inherit) {
        textIndent = source.textIndent;
        textIndentUnit = source.textIndentUnit;
    }
}

HtmlFormatter::HtmlFormatter(HtmlFormatterArgs* args)
    : pageDx(args->pageDx),
      pageDy(args->pageDy),
      textAllocator(args->textAllocator),
      currLineReparseIdx(0),
      currX(0),
      currY(0),
      currLineTopPadding(0),
      currLinkIdx(0),
      listDepth(0),
      preFormatted(false),
      dirRtl(false),
      currPage(nullptr),
      finishedParsing(false),
      pageCount(0),
      keepTagNesting(false) {
    currReparseIdx = args->reparseIdx;
    htmlParser = new HtmlPullParser(args->htmlStr.data(), args->htmlStr.size());
    htmlParser->SetCurrPosOff(currReparseIdx);
    CrashIf(!ValidReparseIdx(currReparseIdx, htmlParser));

    gfx = mui::AllocGraphicsForMeasureText();
    textMeasure = CreateTextRender(args->textRenderMethod, gfx, 10, 10);
    defaultFontName.SetCopy(args->GetFontName());
    defaultFontSize = args->fontSize;

    DrawStyle style;
    style.font = mui::GetCachedFont(defaultFontName, defaultFontSize, FontStyleRegular);
    style.align = Align_Justify;
    style.dirRtl = false;
    styleStack.Append(style);
    nextPageStyle = styleStack.Last();

    textMeasure->SetFont(CurrFont());

    lineSpacing = textMeasure->GetCurrFontLineSpacing();
    spaceDx = CurrFont()->GetSize() / 2.5f; // note: a heuristic
    float spaceDx2 = GetSpaceDx(textMeasure);
    if (spaceDx2 < spaceDx)
        spaceDx = spaceDx2;

    EmitNewPage();
}

HtmlFormatter::~HtmlFormatter() {
    // delete all pages that were not consumed by the caller
    DeleteVecMembers(pagesToSend);
    delete currPage;
    delete textMeasure;
    mui::FreeGraphicsForMeasureText(gfx);
    delete htmlParser;
}

void HtmlFormatter::AppendInstr(DrawInstr di) {
    currLineInstr.Append(di);
    if (-1 == currLineReparseIdx) {
        currLineReparseIdx = currReparseIdx;
        CrashIf(!ValidReparseIdx(currReparseIdx, htmlParser));
    }
}

void HtmlFormatter::SetFont(const WCHAR* fontName, FontStyle fs, float fontSize) {
    if (fontSize < 0) {
        fontSize = CurrFont()->GetSize();
    }
    mui::CachedFont* newFont = mui::GetCachedFont(fontName, fontSize, fs);
    if (CurrFont() != newFont) {
        AppendInstr(DrawInstr::SetFont(newFont));
    }

    DrawStyle style = styleStack.Last();
    style.font = newFont;
    styleStack.Append(style);
}

void HtmlFormatter::SetFontBasedOn(mui::CachedFont* font, FontStyle fs, float fontSize) {
    const WCHAR* fontName = font->GetName();
    if (nullptr == fontName)
        fontName = defaultFontName;
    SetFont(fontName, fs, fontSize);
}

bool ValidStyleForChangeFontStyle(FontStyle fs) {
    if ((FontStyleBold == fs) || (FontStyleItalic == fs) || (FontStyleUnderline == fs) || (FontStyleStrikeout == fs)) {
        return true;
    }
    return false;
}

// change the current font by adding (if addStyle is true) or removing
// a given font style from current font style
// TODO: it doesn't corrctly support the case where a style is wrongly nested
// like "<b>fo<i>oo</b>bar</i>" - "bar" should be italic but will be bold
void HtmlFormatter::ChangeFontStyle(FontStyle fs, bool addStyle) {
    CrashIf(!ValidStyleForChangeFontStyle(fs));
    if (addStyle)
        SetFontBasedOn(CurrFont(), (FontStyle)(fs | CurrFont()->GetStyle()));
    else
        RevertStyleChange();
}

void HtmlFormatter::SetAlignment(AlignAttr align) {
    DrawStyle style = styleStack.Last();
    style.align = align;
    styleStack.Append(style);
}

void HtmlFormatter::RevertStyleChange() {
    if (styleStack.size() > 1) {
        DrawStyle style = styleStack.Pop();
        if (style.font != CurrFont())
            AppendInstr(DrawInstr::SetFont(CurrFont()));
        dirRtl = style.dirRtl;
    }
}

static bool IsVisibleDrawInstr(DrawInstr& i) {
    switch (i.type) {
        case DrawInstrType::String:
        case DrawInstrType::RtlString:
        case DrawInstrType::Line:
        case DrawInstrType::Image:
            return true;
    }
    return false;
}

// sum of widths of all elements with a fixed size and flexible
// spaces (using minimum value for its width)
REAL HtmlFormatter::CurrLineDx() {
    REAL dx = NewLineX();
    for (DrawInstr& i : currLineInstr) {
        if (DrawInstrType::String == i.type || DrawInstrType::RtlString == i.type) {
            dx += i.bbox.Width;
        } else if (DrawInstrType::Image == i.type) {
            dx += i.bbox.Width;
        } else if (DrawInstrType::ElasticSpace == i.type) {
            dx += spaceDx;
        } else if (DrawInstrType::FixedSpace == i.type) {
            dx += i.bbox.Width;
        }
    }
    return dx;
}

// return the height of the tallest element on the line
float HtmlFormatter::CurrLineDy() {
    float dy = lineSpacing;
    for (DrawInstr& i : currLineInstr) {
        if (IsVisibleDrawInstr(i)) {
            if (i.bbox.Height > dy)
                dy = i.bbox.Height;
        }
    }
    return dy;
}

// return the width of the left margin (used for paragraph
// indentation inside lists)
float HtmlFormatter::NewLineX() {
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
void HtmlFormatter::LayoutLeftStartingAt(REAL offX) {
    DrawInstr* lastInstr = nullptr;
    int instrCount = 0;

    REAL x = offX + NewLineX();
    for (DrawInstr& i : currLineInstr) {
        if (DrawInstrType::String == i.type || DrawInstrType::RtlString == i.type || DrawInstrType::Image == i.type) {
            i.bbox.X = x;
            x += i.bbox.Width;
            lastInstr = &i;
            instrCount++;
        } else if (DrawInstrType::ElasticSpace == i.type) {
            x += spaceDx;
        } else if (DrawInstrType::FixedSpace == i.type) {
            x += i.bbox.Width;
        }
    }

    // center a single image
    if (instrCount == 1 && DrawInstrType::Image == lastInstr->type)
        lastInstr->bbox.X = (pageDx - lastInstr->bbox.Width) / 2.f;
}

// TODO: if elements are of different sizes (e.g. texts using different fonts)
// we should align them according to the baseline (which we would first need to
// record for each element)
static void SetYPos(Vec<DrawInstr>& instr, float y) {
    for (DrawInstr& i : instr) {
        if (IsVisibleDrawInstr(i))
            i.bbox.Y = y;
    }
}

void HtmlFormatter::DumpLineDebugInfo() {
    // TODO: write me
    // like CurrLineDx() but dumps info about draw instructions to dbg out
}

// Redistribute extra space in the line equally among the spaces
void HtmlFormatter::JustifyLineBoth() {
    REAL extraSpaceDxTotal = pageDx - currX;
#ifdef DEBUG
    if (extraSpaceDxTotal < 0.f)
        DumpLineDebugInfo();
#endif
    CrashIf(extraSpaceDxTotal < 0.f);

    LayoutLeftStartingAt(0.f);
    size_t spaces = 0;
    bool endsWithSpace = false;
    for (DrawInstr& i : currLineInstr) {
        if (DrawInstrType::ElasticSpace == i.type) {
            ++spaces;
            endsWithSpace = true;
        } else if (DrawInstrType::String == i.type || DrawInstrType::RtlString == i.type)
            endsWithSpace = false;
        else if (DrawInstrType::Image == i.type)
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
    DrawInstr* lastStr = nullptr;
    for (DrawInstr& i : currLineInstr) {
        if (DrawInstrType::ElasticSpace == i.type)
            offX += extraSpaceDx;
        else if (DrawInstrType::String == i.type || DrawInstrType::RtlString == i.type ||
                 DrawInstrType::Image == i.type) {
            i.bbox.X += offX;
            lastStr = &i;
        }
    }
    // align the last element perfectly against the right edge in case
    // we've accumulated rounding errors
    if (lastStr)
        lastStr->bbox.X = pageDx - lastStr->bbox.Width;
}

bool HtmlFormatter::IsCurrLineEmpty() {
    for (DrawInstr& i : currLineInstr) {
        if (IsVisibleDrawInstr(i))
            return false;
    }
    return true;
}

void HtmlFormatter::JustifyCurrLine(AlignAttr align) {
    // TODO: is CurrLineDx needed at all?
    CrashIf(currX != CurrLineDx());

    switch (align) {
        case Align_Left:
            LayoutLeftStartingAt(0.f);
            break;
        case Align_Right:
            LayoutLeftStartingAt(pageDx - currX);
            break;
        case Align_Center:
            LayoutLeftStartingAt((pageDx - currX) / 2.f);
            break;
        case Align_Justify:
            JustifyLineBoth();
            break;
        default:
            CrashIf(true);
            break;
    }

    // when the reading direction is right-to-left, mirror the entire page
    // so that the first element on a line is the right-most, etc.
    if (dirRtl) {
        for (DrawInstr& i : currLineInstr) {
            if (IsVisibleDrawInstr(i))
                i.bbox.X = pageDx - i.bbox.X - i.bbox.Width;
        }
    }
}

static RectF RectFUnion(RectF& r1, RectF& r2) {
    if (r2.IsEmptyArea())
        return r1;
    if (r1.IsEmptyArea())
        return r2;
    RectF ru;
    ru.Union(ru, r1, r2);
    return ru;
}

void HtmlFormatter::UpdateLinkBboxes(HtmlPage* page) {
    for (DrawInstr& i : page->instructions) {
        if (DrawInstrType::LinkStart != i.type)
            continue;
        for (DrawInstr* i2 = &i + 1; i2->type != DrawInstrType::LinkEnd; i2++) {
            if (IsVisibleDrawInstr(*i2)) {
                i.bbox = RectFUnion(i.bbox, i2->bbox);
            }
        }
    }
}

void HtmlFormatter::ForceNewPage() {
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
bool HtmlFormatter::FlushCurrLine(bool isParagraphBreak) {
    if (IsCurrLineEmpty()) {
        currX = NewLineX();
        currLineTopPadding = 0;
        // remove all spaces (only keep SetFont, LinkStart and Anchor instructions)
        for (size_t k = currLineInstr.size(); k > 0; k--) {
            DrawInstr& i = currLineInstr.at(k - 1);
            if (DrawInstrType::FixedSpace == i.type || DrawInstrType::ElasticSpace == i.type)
                currLineInstr.RemoveAt(k - 1);
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
        CrashIf(currLineReparseIdx > INT_MAX);
        currPage->reparseIdx = (int)currLineReparseIdx;
        createdPage = true;
    }
    SetYPos(currLineInstr, currY + currLineTopPadding);
    currY += totalLineDy;

    DrawInstr link;
    if (currLinkIdx) {
        link = currLineInstr.at(currLinkIdx - 1);
        // TODO: this occasionally leads to empty links
        AppendInstr(DrawInstr(DrawInstrType::LinkEnd));
    }
    currPage->instructions.Append(currLineInstr.LendData(), currLineInstr.size());
    currLineInstr.Reset();
    currLineReparseIdx = -1; // mark as not set
    currLineTopPadding = 0;
    currX = NewLineX();
    if (currLinkIdx) {
        AppendInstr(DrawInstr::LinkStart(link.str.s, link.str.len));
        currLinkIdx = currLineInstr.size();
    }
    nextPageStyle = styleStack.Last();
    return createdPage;
}

void HtmlFormatter::EmitNewPage() {
    CrashIf(currReparseIdx > INT_MAX);
    currPage = new HtmlPage((int)currReparseIdx);
    currPage->instructions.Append(DrawInstr::SetFont(nextPageStyle.font));
    currY = 0.f;
}

void HtmlFormatter::EmitEmptyLine(float lineDy) {
    CrashIf(!IsCurrLineEmpty());
    currY += lineDy;
    if (currY <= pageDy) {
        currX = NewLineX();
        // remove all spaces (only keep SetFont, LinkStart and Anchor instructions)
        for (size_t k = currLineInstr.size(); k > 0; k--) {
            DrawInstr& i = currLineInstr.at(k - 1);
            if (DrawInstrType::FixedSpace == i.type || DrawInstrType::ElasticSpace == i.type)
                currLineInstr.RemoveAt(k - 1);
        }
        return;
    }
    ForceNewPage();
}

static bool HasPreviousLineSingleImage(Vec<DrawInstr>& instrs) {
    REAL imageY = -1;
    for (size_t idx = instrs.size(); idx > 0; idx--) {
        DrawInstr& i = instrs.at(idx - 1);
        if (!IsVisibleDrawInstr(i))
            continue;
        if (-1 != imageY) {
            // if another visible item precedes the image,
            // it must be completely above it (previous line)
            return i.bbox.Y + i.bbox.Height <= imageY;
        }
        if (DrawInstrType::Image != i.type)
            return false;
        imageY = i.bbox.Y;
    }
    return imageY != -1;
}

bool HtmlFormatter::EmitImage(ImageData* img) {
    CrashIf(!img->data);
    Size imgSize = BitmapSizeFromData(img->data, img->len);
    if (imgSize.Empty())
        return false;

    SizeF newSize((REAL)imgSize.Width, (REAL)imgSize.Height);
    // move overly large images to a new line (if they don't fit entirely)
    if (!IsCurrLineEmpty() && (currX + newSize.Width > pageDx || currY + newSize.Height > pageDy))
        FlushCurrLine(false);
    // move overly large images to a new page
    // (if they don't fit even when scaled down to 75%)
    REAL scalePage = std::min((pageDx - currX) / newSize.Width, pageDy / newSize.Height);
    if (currY > 0 && currY + newSize.Height * std::min(scalePage, 0.75f) > pageDy)
        ForceNewPage();
    // if image is bigger than the available space, scale it down
    if (newSize.Width > pageDx - currX || newSize.Height > pageDy - currY) {
        REAL scale = std::min(scalePage, (pageDy - currY) / newSize.Height);
        // scale down images that follow right after a line
        // containing a single image as little as possible,
        // as they might be intended to be of the same size
        if (scale < scalePage && HasPreviousLineSingleImage(currPage->instructions)) {
            ForceNewPage();
            scale = scalePage;
        }
        if (scale < 1) {
            newSize.Width = std::min(newSize.Width * scale, pageDx - currX);
            newSize.Height = std::min(newSize.Height * scale, pageDy - currY);
        }
    }

    RectF bbox(PointF(currX, 0), newSize);
    AppendInstr(DrawInstr::Image(img->data, img->len, bbox));
    currX += bbox.Width;

    return true;
}

// add horizontal line (<hr> in html terms)
void HtmlFormatter::EmitHr() {
    // hr creates an implicit paragraph break
    FlushCurrLine(true);
    CrashIf(NewLineX() != currX);
    RectF bbox(0.f, 0.f, pageDx, lineSpacing);
    AppendInstr(DrawInstr(DrawInstrType::Line, bbox));
    FlushCurrLine(true);
}

void HtmlFormatter::EmitParagraph(float indent) {
    FlushCurrLine(true);
    CrashIf(NewLineX() != currX);
    bool needsIndent = Align_Left == CurrStyle()->align || Align_Justify == CurrStyle()->align;
    if (indent > 0 && needsIndent && EnsureDx(indent)) {
        AppendInstr(DrawInstr::FixedSpace(indent));
        currX += indent;
    }
}

// ensure there is enough dx space left in the current line
// if there isn't, we start a new line
// returns false if dx is bigger than pageDx
bool HtmlFormatter::EnsureDx(float dx) {
    if (currX + dx <= pageDx)
        return true;
    FlushCurrLine(false);
    return dx <= pageDx;
}

// don't emit multiple spaces and don't emit spaces
// at the beginning of the line
static bool CanEmitElasticSpace(float currX, float NewLineX, float maxCurrX, Vec<DrawInstr>& currLineInstr) {
    if (NewLineX == currX || 0 == currLineInstr.size())
        return false;
    // prevent elastic spaces from being flushed to the
    // beginning of the next line
    if (currX > maxCurrX)
        return false;
    DrawInstr& di = currLineInstr.Last();
    // don't add a space if only an anchor would be in between them
    if (DrawInstrType::Anchor == di.type && currLineInstr.size() > 1)
        di = currLineInstr.at(currLineInstr.size() - 2);
    return (DrawInstrType::ElasticSpace != di.type) && (DrawInstrType::FixedSpace != di.type);
}

void HtmlFormatter::EmitElasticSpace() {
    if (!CanEmitElasticSpace(currX, NewLineX(), pageDx - spaceDx, currLineInstr))
        return;
    EnsureDx(spaceDx);
    currX += spaceDx;
    AppendInstr(DrawInstr(DrawInstrType::ElasticSpace));
}

// return true if we can break a word on a given character during layout
static bool CanBreakWordOnChar(WCHAR c) {
    // don't break on Chinese and Japan characters
    // https://github.com/sumatrapdfreader/sumatrapdf/issues/250
    // https://github.com/sumatrapdfreader/sumatrapdf/pull/1057
    // There are other  ranges, but far less common
    // https://stackoverflow.com/questions/1366068/whats-the-complete-range-for-chinese-characters-in-unicode
    if (c >= 0x2E80 && c <= 0xA4CF) {
        return true;
    }
    return false;
}

// a text run is a string of consecutive text with uniform style
void HtmlFormatter::EmitTextRun(const char* s, const char* end) {
    currReparseIdx = s - htmlParser->Start();
    CrashIf(!ValidReparseIdx(currReparseIdx, htmlParser));
    CrashIf(IsSpaceOnly(s, end) && !preFormatted);
    const char* tmp = ResolveHtmlEntities(s, end, textAllocator);
    bool resolved = tmp != s;
    if (resolved) {
        s = tmp;
        end = s + str::Len(s);
    }

    while (s < end) {
        // don't update the reparseIdx if s doesn't point into the original source
        if (!resolved)
            currReparseIdx = s - htmlParser->Start();

        size_t strLen = strconv::Utf8ToWcharBuf(s, end - s, buf, dimof(buf));
        // soft hyphens should not be displayed
        strLen -= str::RemoveChars(buf, L"\xad");
        if (0 == strLen)
            break;
        textMeasure->SetFont(CurrFont());
        RectF bbox = textMeasure->Measure(buf, strLen);
        if (bbox.Width <= pageDx - currX) {
            AppendInstr(DrawInstr::Str(s, end - s, bbox, dirRtl));
            currX += bbox.Width;
            break;
        }
        // get len That Fits the remaining space in the line
        size_t lenThatFits = StringLenForWidth(textMeasure, buf, strLen, pageDx - currX);
        // try to prevent a break in the middle of a word
        if (lenThatFits > 0) {
            if (!CanBreakWordOnChar(buf[lenThatFits])) {
                size_t lentmp = lenThatFits;
                for (lentmp = lenThatFits; lentmp > 0; lentmp--) {
                    if (CanBreakWordOnChar(buf[lentmp - 1])) {
                        break;
                    }
                }
                if (lentmp == 0) {
                    // make a new line if the word need to show in another line
                    if (currX != NewLineX()) {
                        FlushCurrLine(false);
                        continue;
                    }
                    // split the word (or CJK sentence) if it is too long to show in one line
                } else {
                    // renew lenThatFits
                    lenThatFits = lentmp;
                }
            }
        } else {
            // make a new line when current line is fullfilled
            FlushCurrLine(false);
            continue;
        }

        textMeasure->SetFont(CurrFont());
        bbox = textMeasure->Measure(buf, lenThatFits);
        CrashIf(bbox.Width > pageDx);
        // s is UTF-8 and buf is UTF-16, so one
        // WCHAR doesn't always equal one char
        // TODO: this usually fails for non-BMP characters (i.e. hardly ever)
        for (size_t i = lenThatFits; i > 0; i--) {
            lenThatFits += buf[i - 1] < 0x80 ? 0 : buf[i - 1] < 0x800 ? 1 : 2;
        }
        AppendInstr(DrawInstr::Str(s, lenThatFits, bbox, dirRtl));
        currX += bbox.Width;
        s += lenThatFits;
    }
}

void HtmlFormatter::HandleAnchorAttr(HtmlToken* t, bool idsOnly) {
    if (t->IsEndTag())
        return;

    AttrInfo* attr = t->GetAttrByName("id");
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

void HtmlFormatter::HandleDirAttr(HtmlToken* t) {
    // only apply reading direction changes to block elements (for now)
    if (t->IsStartTag() && !IsInlineTag(t->tag)) {
        AttrInfo* attr = t->GetAttrByName("dir");
        if (attr)
            dirRtl = CurrStyle()->dirRtl = attr->ValIs("RTL");
    }
}

void HtmlFormatter::HandleTagBr() {
    // make sure to always emit a line
    if (IsCurrLineEmpty())
        EmitEmptyLine(lineSpacing);
    else
        FlushCurrLine(true);
}

static AlignAttr GetAlignAttr(HtmlToken* t, AlignAttr defVal) {
    AttrInfo* attr = t->GetAttrByName("align");
    if (!attr)
        return defVal;
    AlignAttr align = FindAlignAttr(attr->val, attr->valLen);
    if (Align_NotFound == align)
        return defVal;
    return align;
}

void HtmlFormatter::HandleTagP(HtmlToken* t, bool isDiv) {
    if (!t->IsEndTag()) {
        AlignAttr align = CurrStyle()->align;
        float indent = 0;

        StyleRule rule = ComputeStyleRule(t);
        if (rule.textAlign != Align_NotFound)
            align = rule.textAlign;
        else if (!isDiv) {
            // prefer CSS styling to align attribute
            align = GetAlignAttr(t, align);
        }
        if (rule.textIndentUnit != StyleRule::inherit && rule.textIndent > 0) {
            float factor = rule.textIndentUnit == StyleRule::em
                               ? CurrFont()->GetSize()
                               : rule.textIndentUnit == StyleRule::pt ? 1 /* TODO: take DPI into account */ : 1;
            indent = rule.textIndent * factor;
        }

        SetAlignment(align);
        EmitParagraph(indent);
    } else {
        FlushCurrLine(true);
        RevertStyleChange();
    }
    EmitEmptyLine(0.4f * CurrFont()->GetSize());
}

void HtmlFormatter::HandleTagFont(HtmlToken* t) {
    if (t->IsEndTag()) {
        RevertStyleChange();
        return;
    }

    AttrInfo* attr = t->GetAttrByName("face");
    const WCHAR* faceName = CurrFont()->GetName();
    if (attr) {
        size_t strLen = strconv::Utf8ToWcharBuf(attr->val, attr->valLen, buf, dimof(buf));
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

bool HtmlFormatter::HandleTagA(HtmlToken* t, const char* linkAttr, const char* attrNS) {
    if (t->IsStartTag() && !currLinkIdx) {
        AttrInfo* attr = attrNS ? t->GetAttrByNameNS(linkAttr, attrNS) : t->GetAttrByName(linkAttr);
        if (attr) {
            AppendInstr(DrawInstr::LinkStart(attr->val, attr->valLen));
            currLinkIdx = currLineInstr.size();
            return true;
        }
    } else if (t->IsEndTag() && currLinkIdx) {
        AppendInstr(DrawInstr(DrawInstrType::LinkEnd));
        currLinkIdx = 0;
        return true;
    }
    return false;
}

inline bool IsTagH(HtmlTag tag) {
    switch (tag) {
        case Tag_H1:
        case Tag_H2:
        case Tag_H3:
        case Tag_H4:
        case Tag_H5:
        case Tag_H6:
            return true;
    }
    return false;
}

void HtmlFormatter::HandleTagHx(HtmlToken* t) {
    if (t->IsEndTag()) {
        FlushCurrLine(true);
        currY += CurrFont()->GetSize() / 2;
        RevertStyleChange();
    } else {
        EmitParagraph(0);
        float fontSize = defaultFontSize * pow(1.1f, '5' - t->s[1]);
        if (currY > 0)
            currY += fontSize / 2;
        SetFontBasedOn(CurrFont(), FontStyleBold, fontSize);

        StyleRule rule = ComputeStyleRule(t);
        if (Align_NotFound == rule.textAlign)
            rule.textAlign = GetAlignAttr(t, Align_Left);
        CurrStyle()->align = rule.textAlign;
    }
}

void HtmlFormatter::HandleTagList(HtmlToken* t) {
    FlushCurrLine(true);
    if (t->IsStartTag())
        listDepth++;
    else if (t->IsEndTag() && listDepth > 0)
        listDepth--;
    currX = NewLineX();
}

void HtmlFormatter::HandleTagPre(HtmlToken* t) {
    FlushCurrLine(true);
    if (t->IsStartTag()) {
        SetFont(L"Courier New", (FontStyle)CurrFont()->GetStyle());
        CurrStyle()->align = Align_Left;
        preFormatted = true;
    } else if (t->IsEndTag()) {
        RevertStyleChange();
        preFormatted = false;
    }
}

StyleRule* HtmlFormatter::FindStyleRule(HtmlTag tag, const char* clazz, size_t clazzLen) {
    uint32_t classHash = clazz ? MurmurHash2(clazz, clazzLen) : 0;
    for (size_t i = 0; i < styleRules.size(); i++) {
        StyleRule& rule = styleRules.at(i);
        if (tag == rule.tag && classHash == rule.classHash)
            return &rule;
    }
    return nullptr;
}

StyleRule HtmlFormatter::ComputeStyleRule(HtmlToken* t) {
    StyleRule rule;
    // get style rules ordered by specificity
    StyleRule* prevRule = FindStyleRule(Tag_Body, nullptr, 0);
    if (prevRule)
        rule.Merge(*prevRule);
    prevRule = FindStyleRule(Tag_Any, nullptr, 0);
    if (prevRule)
        rule.Merge(*prevRule);
    prevRule = FindStyleRule(t->tag, nullptr, 0);
    if (prevRule)
        rule.Merge(*prevRule);
    // TODO: support multiple class names
    AttrInfo* attr = t->GetAttrByName("class");
    if (attr) {
        prevRule = FindStyleRule(Tag_Any, attr->val, attr->valLen);
        if (prevRule)
            rule.Merge(*prevRule);
        prevRule = FindStyleRule(t->tag, attr->val, attr->valLen);
        if (prevRule)
            rule.Merge(*prevRule);
    }
    attr = t->GetAttrByName("style");
    if (attr) {
        StyleRule newRule = StyleRule::Parse(attr->val, attr->valLen);
        rule.Merge(newRule);
    }
    return rule;
}

void HtmlFormatter::ParseStyleSheet(const char* data, size_t len) {
    CssPullParser parser(data, len);
    while (parser.NextRule()) {
        StyleRule rule = StyleRule::Parse(&parser);
        const CssSelector* sel;
        while ((sel = parser.NextSelector()) != nullptr) {
            if (Tag_NotFound == sel->tag)
                continue;
            StyleRule* prevRule = FindStyleRule(sel->tag, sel->clazz, sel->clazzLen);
            if (prevRule) {
                prevRule->Merge(rule);
            } else {
                rule.tag = sel->tag;
                rule.classHash = sel->clazz ? MurmurHash2(sel->clazz, sel->clazzLen) : 0;
                styleRules.Append(rule);
            }
        }
    }
}

void HtmlFormatter::HandleTagStyle(HtmlToken* t) {
    if (!t->IsStartTag())
        return;
    AttrInfo* attr = t->GetAttrByName("type");
    if (attr && !attr->ValIs("text/css"))
        return;

    const char* start = t->s + t->sLen + 1;
    while (t && !t->IsError() && (!t->IsEndTag() || t->tag != Tag_Style)) {
        t = htmlParser->Next();
    }
    if (!t || !t->IsEndTag() || Tag_Style != t->tag)
        return;
    const char* end = t->s - 2;
    CrashIf(start > end);
    ParseStyleSheet(start, end - start);
    UpdateTagNesting(t);
}

// returns true if prev can't contain curr and should thus be closed
static bool AutoCloseOnOpen(HtmlTag curr, HtmlTag prev) {
    CrashIf(IsInlineTag(curr));
    // always start afresh for a new <body>
    if (Tag_Body == curr)
        return true;
    // allow <div>s to be contained within inline tags
    // (e.g. <i><div>...</div></i> from pg12.mobi)
    if (Tag_Div == curr)
        return false;

    switch (prev) {
        case Tag_Dd:
        case Tag_Dt:
            return Tag_Dd == curr || Tag_Dt == curr;
        case Tag_H1:
        case Tag_H2:
        case Tag_H3:
        case Tag_H4:
        case Tag_H5:
        case Tag_H6:
            return IsTagH(curr);
        case Tag_Lh:
        case Tag_Li:
            return Tag_Lh == curr || Tag_Li == curr;
        case Tag_P:
            return true; // <p> can't contain any block-level elements
        case Tag_Td:
        case Tag_Tr:
            return Tag_Tr == curr;
        default:
            return IsInlineTag(prev);
    }
}

void HtmlFormatter::AutoCloseTags(size_t count) {
    keepTagNesting = true; // prevent recursion
    HtmlToken tok;
    tok.type = HtmlToken::EndTag;
    tok.sLen = 0;
    // let HandleHtmlTag clean up (in reverse order)
    for (size_t i = 0; i < count; i++) {
        tok.tag = tagNesting.Pop();
        HandleHtmlTag(&tok);
    }
    keepTagNesting = false;
}

void HtmlFormatter::UpdateTagNesting(HtmlToken* t) {
    CrashIf(!t->IsTag());
    if (keepTagNesting || Tag_NotFound == t->tag || t->IsEmptyElementEndTag() || IsTagSelfClosing(t->tag)) {
        return;
    }

    size_t idx = tagNesting.size();
    bool isInline = IsInlineTag(t->tag);
    if (t->IsStartTag()) {
        if (IsInlineTag(t->tag)) {
            tagNesting.Push(t->tag);
            return;
        }
        // close all tags that can't contain this new block-level tag
        for (; idx > 0 && AutoCloseOnOpen(t->tag, tagNesting.at(idx - 1)); idx--)
            ;
    } else {
        // close all tags that were contained within the current tag
        // (for inline tags just up to the next block-level tag)
        for (; idx > 0 && (!isInline || IsInlineTag(tagNesting.at(idx - 1))) && t->tag != tagNesting.at(idx - 1); idx--)
            ;
        if (0 == idx || tagNesting.at(idx - 1) != t->tag)
            return;
    }

    AutoCloseTags(tagNesting.size() - idx);

    if (t->IsStartTag())
        tagNesting.Push(t->tag);
    else {
        CrashIf(!t->IsEndTag() || t->tag != tagNesting.Last());
        tagNesting.Pop();
    }
}

void HtmlFormatter::HandleHtmlTag(HtmlToken* t) {
    CrashIf(!t->IsTag());

    UpdateTagNesting(t);

    HtmlTag tag = t->tag;
    if (Tag_P == tag) {
        HandleTagP(t);
    } else if (Tag_Hr == tag) {
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
        HandleTagP(t, true);
    } else if (IsTagH(tag)) {
        HandleTagHx(t);
    } else if (Tag_Sup == tag) {
        // TODO: implement me
    } else if (Tag_Sub == tag) {
        // TODO: implement me
    } else if (Tag_Span == tag) {
        // TODO: implement me
    } else if (Tag_Center == tag) {
        HandleTagP(t, true);
        if (!t->IsEndTag())
            CurrStyle()->align = Align_Center;
    } else if ((Tag_Ul == tag) || (Tag_Ol == tag)) {
        HandleTagList(t);
    } else if (Tag_Li == tag) {
        // TODO: display bullet/number
        FlushCurrLine(true);
    } else if (Tag_Dt == tag) {
        FlushCurrLine(true);
        ChangeFontStyle(FontStyleBold, t->IsStartTag());
        if (t->IsStartTag())
            CurrStyle()->align = Align_Left;
    } else if (Tag_Dd == tag) {
        // TODO: separate indentation from list depth
        HandleTagList(t);
    } else if (Tag_Table == tag) {
        // TODO: implement me
        HandleTagList(t);
    } else if (Tag_Tr == tag) {
        // display tables row-by-row for now
        FlushCurrLine(true);
        if (t->IsStartTag())
            SetAlignment(Align_Left);
        else if (t->IsEndTag())
            RevertStyleChange();
    } else if (Tag_Code == tag || Tag_Tt == tag) {
        if (t->IsStartTag())
            SetFont(L"Courier New", (FontStyle)CurrFont()->GetStyle());
        else if (t->IsEndTag())
            RevertStyleChange();
    } else if (Tag_Pre == tag) {
        HandleTagPre(t);
    } else if (Tag_Img == tag) {
        HandleTagImg(t);
    } else if (Tag_Pagebreak == tag) {
        // not really a HTML tag, but many ebook
        // formats use it
        HandleTagPagebreak(t);
    } else if (Tag_Link == tag) {
        HandleTagLink(t);
    } else if (Tag_Style == tag) {
        HandleTagStyle(t);
    } else {
        // TODO: temporary debugging
        // lf("unhandled tag: %d", tag);
    }

    // any tag could contain anchor information
    HandleAnchorAttr(t);
    // any tag could contain a reading direction change
    HandleDirAttr(t);
}

void HtmlFormatter::HandleText(HtmlToken* t) {
    CrashIf(!t->IsText());
    HandleText(t->s, t->sLen);
}

void HtmlFormatter::HandleText(const char* s, size_t sLen) {
    const char* curr = s;
    const char* end = s + sLen;

    if (preFormatted) {
        // don't collapse whitespace and respect text newlines
        while (curr < end) {
            const char* text = curr;
            currReparseIdx = curr - htmlParser->Start();
            // skip to the next newline
            for (; curr < end && *curr != '\n'; curr++)
                ;
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
        bool skipped = SkipWs(curr, end);
        if (skipped)
            EmitElasticSpace();

        const char* text = curr;
        currReparseIdx = curr - htmlParser->Start();
        skipped = SkipNonWs(curr, end);
        if (skipped)
            EmitTextRun(text, curr);
    }
}

// we ignore the content of <head>, <script>, <style> and <title> tags
bool HtmlFormatter::IgnoreText() {
    for (HtmlTag& tag : tagNesting) {
        if ((Tag_Head == tag) || (Tag_Script == tag) || (Tag_Style == tag) || (Tag_Title == tag)) {
            return true;
        }
    }
    return false;
}

// empty page is one that consists of only invisible instructions
static bool IsEmptyPage(HtmlPage* p) {
    if (!p)
        return false;
    for (DrawInstr& i : p->instructions) {
        // if a page only consits of lines we consider it empty. It's different
        // than what Kindle does but I don't see the purpose of showing such
        // pages to the user
        if (DrawInstrType::Line == i.type)
            continue;
        if (IsVisibleDrawInstr(i))
            return false;
    }
    // all instructions were invisible
    return true;
}

// Return the next parsed page. Returns nullptr if finished parsing.
// For simplicity of implementation, we parse xml text node or
// xml element at a time. This might cause a creation of one
// or more pages, which we remeber and send to the caller
// if we detect accumulated pages.
HtmlPage* HtmlFormatter::Next(bool skipEmptyPages) {
    for (;;) {
        // send out all pages accumulated so far
        while (pagesToSend.size() > 0) {
            HtmlPage* ret = pagesToSend.PopAt(0);
            pageCount++;
            if (skipEmptyPages && IsEmptyPage(ret))
                delete ret;
            else
                return ret;
        }
        // we can call ourselves recursively to send outstanding
        // pages after parsing has finished so this is to detect
        // that case and really end parsing
        if (finishedParsing)
            return nullptr;
        HtmlToken* t = htmlParser->Next();
        if (!t || t->IsError())
            break;

        currReparseIdx = t->GetReparsePoint() - htmlParser->Start();
        CrashIf(!ValidReparseIdx(currReparseIdx, htmlParser));
        if (t->IsTag())
            HandleHtmlTag(t);
        else if (!IgnoreText())
            HandleText(t);
    }
    // force layout of the last line
    AutoCloseTags(tagNesting.size());
    FlushCurrLine(true);

    UpdateLinkBboxes(currPage);
    pagesToSend.Append(currPage);
    currPage = nullptr;
    // call ourselves recursively to return accumulated pages
    finishedParsing = true;
    return Next();
}

// convenience method to format the whole html
Vec<HtmlPage*>* HtmlFormatter::FormatAllPages(bool skipEmptyPages) {
    Vec<HtmlPage*>* pages = new Vec<HtmlPage*>();
    for (HtmlPage* pd = Next(skipEmptyPages); pd; pd = Next(skipEmptyPages)) {
        pages->Append(pd);
    }
    return pages;
}

// TODO: draw link in the appropriate format (blue text, underlined, should show hand cursor when
// mouse is over a link. There's a slight complication here: we only get explicit information about
// strings, not about the whitespace and we should underline the whitespace as well. Also the text
// should be underlined at a baseline
void DrawHtmlPage(Graphics* g, mui::ITextRender* textDraw, Vec<DrawInstr>* drawInstructions, REAL offX, REAL offY,
                  bool showBbox, Color textColor, bool* abortCookie) {
    Pen debugPen(Color(255, 0, 0), 1);
    // Pen linePen(Color(0, 0, 0), 2.f);
    Pen linePen(Color(0x5F, 0x4B, 0x32), 2.f);

    WCHAR buf[512];

    // GDI text rendering suffers terribly if we call GetHDC()/ReleaseHDC() around every
    // draw, so first draw text and then paint everything else
    textDraw->SetTextColor(textColor);
    Status status = Ok;
    auto t = TimeGet();
    textDraw->Lock();
    for (DrawInstr& i : *drawInstructions) {
        RectF bbox = i.bbox;
        bbox.X += offX;
        bbox.Y += offY;
        if (DrawInstrType::String == i.type || DrawInstrType::RtlString == i.type) {
            size_t strLen = strconv::Utf8ToWcharBuf(i.str.s, i.str.len, buf, dimof(buf));
            // soft hyphens should not be displayed
            strLen -= str::RemoveChars(buf, L"\xad");
            textDraw->Draw(buf, strLen, bbox, DrawInstrType::RtlString == i.type);
        } else if (DrawInstrType::SetFont == i.type) {
            textDraw->SetFont(i.font);
        }
        if (abortCookie && *abortCookie)
            break;
    }
    textDraw->Unlock();
    double dur = TimeSinceInMs(t);
    // logf("DrawHtmlPage: textDraw %.2f ms\n", dur);

    for (DrawInstr& i : *drawInstructions) {
        RectF bbox = i.bbox;
        bbox.X += offX;
        bbox.Y += offY;
        if (DrawInstrType::Line == i.type) {
            // hr is a line drawn in the middle of bounding box
            REAL y = floorf(bbox.Y + bbox.Height / 2.f + 0.5f);
            PointF p1(bbox.X, y);
            PointF p2(bbox.X + bbox.Width, y);
            if (showBbox) {
                status = g->DrawRectangle(&debugPen, bbox);
                CrashIf(status != Ok);
            }
            status = g->DrawLine(&linePen, p1, p2);
            CrashIf(status != Ok);
        } else if (DrawInstrType::Image == i.type) {
            // TODO: cache the bitmap somewhere (?)
            Bitmap* bmp = BitmapFromData(i.img.data, i.img.len);
            if (bmp) {
                status = g->DrawImage(bmp, bbox, 0, 0, (REAL)bmp->GetWidth(), (REAL)bmp->GetHeight(), UnitPixel);
                // GDI+ sometimes seems to succeed in loading an image because it lazily decodes it
                CrashIf(status != Ok && status != Win32Error);
            }
            delete bmp;
        } else if (DrawInstrType::LinkStart == i.type) {
            // TODO: set text color to blue
            REAL y = floorf(bbox.Y + bbox.Height + 0.5f);
            PointF p1(bbox.X, y);
            PointF p2(bbox.X + bbox.Width, y);
            Pen linkPen(textColor);
            status = g->DrawLine(&linkPen, p1, p2);
            CrashIf(status != Ok);
        } else if (DrawInstrType::String == i.type || DrawInstrType::RtlString == i.type) {
            if (showBbox) {
                status = g->DrawRectangle(&debugPen, bbox);
                CrashIf(status != Ok);
            }
        } else if (DrawInstrType::LinkEnd == i.type) {
            // TODO: set text color back again
        } else if ((DrawInstrType::ElasticSpace == i.type) || (DrawInstrType::FixedSpace == i.type) ||
                   (DrawInstrType::String == i.type) || (DrawInstrType::RtlString == i.type) ||
                   (DrawInstrType::SetFont == i.type) || (DrawInstrType::Anchor == i.type)) {
            // ignore
        } else {
            CrashIf(true);
        }
        if (abortCookie && *abortCookie)
            break;
    }
}

static mui::TextRenderMethod gTextRenderMethod = mui::TextRenderMethodGdi;
// static mui::TextRenderMethod gTextRenderMethod = mui::TextRenderMethodGdiplus;

mui::TextRenderMethod GetTextRenderMethod() {
    return gTextRenderMethod;
}

void SetTextRenderMethod(mui::TextRenderMethod method) {
    gTextRenderMethod = method;
}

HtmlFormatterArgs* CreateFormatterDefaultArgs(int dx, int dy, Allocator* textAllocator) {
    HtmlFormatterArgs* args = new HtmlFormatterArgs();
    args->SetFontName(L"Georgia");
    args->fontSize = 12.5f;
    args->pageDx = (REAL)dx;
    args->pageDy = (REAL)dy;
    args->textAllocator = textAllocator;
    args->textRenderMethod = GetTextRenderMethod();
    return args;
}
