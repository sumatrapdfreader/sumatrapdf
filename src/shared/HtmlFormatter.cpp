/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Dict.h"
#include "base/HtmlTags.h"
#include "base/Pixmap.h"
#include "base/CssParser.h"

#include "GumboHtmlParser.h"
#include "ImageReader.h"

#include "gui/PlatformFont.h"
#include "gui/PlatformText.h"

#if OS_WIN
#include "base/GdiPlusUtil.h"
#elif OS_LINUX
#include <cairo/cairo.h>
#elif OS_DARWIN
#include "base/MacTypesHide.h"
#include <CoreGraphics/CoreGraphics.h>
#include "base/MacTypesShow.h"
#endif
#include "HtmlFormatter.h"

// Measure each line's items before positioning them for justification.
// Flush completed lines into pages as they overflow.

bool ValidReparseIdx(ptrdiff_t idx, GumboHtmlParser* parser) {
    return !((idx < 0) || (idx > (int)parser->Len()));
}

// helper constructors for instructions that need additional arguments
DrawInstr DrawInstr::Text(::Str s, RectF bbox, bool rtl) {
    return DrawInstr(rtl ? DrawInstrType::RtlString : DrawInstrType::String, bbox, s);
}

DrawInstr DrawInstr::SetFont(PlatformFont* font) {
    DrawInstr di(DrawInstrType::SetFont);
    di.font = font;
    return di;
}

DrawInstr DrawInstr::FixedSpace(float dx) {
    DrawInstr di(DrawInstrType::FixedSpace);
    di.bbox.dx = dx;
    return di;
}

DrawInstr DrawInstr::Image(Str img, RectF bbox) {
    return DrawInstr(DrawInstrType::Image, bbox, img);
}

DrawInstr DrawInstr::LinkStart(::Str s) {
    return DrawInstr(DrawInstrType::LinkStart, {}, s);
}

DrawInstr DrawInstr::Anchor(::Str s, RectF bbox) {
    return DrawInstr(DrawInstrType::Anchor, bbox, s);
}

DrawInstr DrawInstr::PageMarkerAnchor(::Str s, RectF bbox) {
    return DrawInstr(DrawInstrType::PageMarkerAnchor, bbox, s);
}

// parses size in the form "1em", "3pt" or "15px"
void ParseSizeWithUnit(Str s, float* size, StyleRule::Unit* unit) {
    Str suffix = str::Parse(s, "%f", size);
    if (str::StartsWith(suffix, StrL("in"))) {
        constexpr float kPointsPerInch = 72;
        *size *= kPointsPerInch;
        suffix = StrL("pt");
    }
    *unit = str::StartsWith(suffix, StrL("em"))   ? StyleRule::em
            : str::StartsWith(suffix, StrL("pt")) ? StyleRule::pt
            : str::StartsWith(suffix, StrL("px")) ? StyleRule::px
                                                  : StyleRule::inherit;
}

StyleRule StyleRule::Parse(CssPullParser* parser) {
    StyleRule rule;
    const CssProperty* prop;
    while ((prop = parser->NextProperty()) != nullptr) {
        if (prop->type == Css_Text_Align) {
            rule.textAlign = FindAlignAttr(prop->s);
        } else if (prop->type == Css_Text_Indent || prop->type == Css_Padding_Left) {
            ParseSizeWithUnit(prop->s, &rule.textIndent, &rule.textIndentUnit);
        }
    }
    return rule;
}

StyleRule StyleRule::Parse(Str s) {
    CssPullParser parser(s);
    return Parse(&parser);
}

void StyleRule::Merge(StyleRule& source) {
    if (source.textAlign != AlignAttr::NotFound) {
        textAlign = source.textAlign;
    }
    if (source.textIndentUnit != StyleRule::inherit) {
        textIndent = source.textIndent;
        textIndentUnit = source.textIndentUnit;
    }
}

HtmlFormatter::HtmlFormatter(HtmlFormatterArgs* args)
    : pageDx(args->pageDx), pageDy(args->pageDy), textAllocator(args->textAllocator) {
    currReparseIdx = args->reparseIdx;
    htmlParser = new GumboHtmlParser(args->htmlStr);
    htmlParser->SetCurrPosOff(currReparseIdx);
    ReportIf(!ValidReparseIdx(currReparseIdx, htmlParser));

    textMeasure = CreatePlatformTextRender(PlatformTextMeasureMethod::Gdi);
    defaultFontName = str::Dup(ToUtf8Temp(args->GetFontName()));
    defaultFontSize = args->fontSize;
    overrideFontName = args->overrideFontName;

    // pre-size each font's measured-text cache from the size of the text: text
    // runs are mostly words (avg. ~6 bytes of html) and many repeat, so guess
    // one distinct text entry per 16 bytes of html (shared across fonts)
    measureCacheInitialSize = limitValue(len(args->htmlStr) / 16, 1024, 64 * 1024);

    DrawStyle style;
    style.font = GetPlatformFont(defaultFontName, defaultFontSize, PlatformFontStyle::Regular);
    style.align = AlignAttr::Justify;
    style.dirRtl = false;
    VecAppend(styleStack, style);
    nextPageStyle = VecLast(styleStack);

    textMeasure->SetFont(CurrFont());

    lineSpacing = textMeasure->GetCurrFontLineSpacing();
    spaceDx = CurrFont()->GetSize() / 2.5f; // note: a heuristic
    float spaceDx2 = textMeasure->GetSpaceDx();
    if (spaceDx2 < spaceDx) {
        spaceDx = spaceDx2;
    }

    EmitNewPage();
}

HtmlFormatter::~HtmlFormatter() {
    // delete all pages that were not consumed by the caller
    DeleteVecMembers(pagesToSend);
    delete currPage;
    delete textMeasure;
    delete htmlParser;
    for (int i = 0; i < nMeasureCaches; i++) {
        delete measureCaches[i].keys;
    }
    str::Free(defaultFontName);
}

// find (or lazily create) the per-font measured-text cache for the current
// font. Returns null once we've seen more than kMaxMeasureCacheFonts fonts,
// in which case the caller measures uncached.
HtmlFormatter::MeasureCache* HtmlFormatter::GetMeasureCacheForCurrFont() {
    PlatformFont* font = CurrFont();
    // fast path: same font as last measurement (the common case)
    if (lastMeasureCache && lastMeasureCache->font == font) {
        return lastMeasureCache;
    }
    for (int i = 0; i < nMeasureCaches; i++) {
        if (measureCaches[i].font == font) {
            lastMeasureCache = &measureCaches[i];
            return lastMeasureCache;
        }
    }
    if (nMeasureCaches >= kMaxMeasureCacheFonts) {
        return nullptr; // too many fonts, measure uncached
    }
    MeasureCache* mc = &measureCaches[nMeasureCaches++];
    mc->font = font;
    mc->keys = new dict::MapStrToInt(measureCacheInitialSize);
    lastMeasureCache = mc;
    return mc;
}

// measuring text is expensive and text runs (mostly words) repeat a lot
// within a document, so cache the measured size per font, keyed by text.
RectF HtmlFormatter::MeasureTextCached(Str s) {
    textMeasure->SetFont(CurrFont());
    MeasureCache* mc = GetMeasureCacheForCurrFont();
    // MapStrToInt keys are utf-8, which is what we measure, so s is the key
    int idx = 0;
    if (mc && !mc->keys->Insert(s, len(mc->vals), &idx)) {
        return mc->vals[idx];
    }
    RectF bbox = textMeasure->Measure(s);
    if (mc) {
        VecAppend(mc->vals, bbox);
    }
    return bbox;
}

void HtmlFormatter::AppendInstr(const DrawInstr& di) {
    VecAppend(currLineInstr, di);
    if (-1 == currLineReparseIdx) {
        currLineReparseIdx = currReparseIdx;
        ReportIf(!ValidReparseIdx(currReparseIdx, htmlParser));
    }
}

void HtmlFormatter::SetFont(Str fontName, PlatformFontStyle fs, float fontSize) {
    if (fontSize < 0) {
        fontSize = CurrFont()->GetSize();
    }
    PlatformFont* newFont = GetPlatformFont(fontName, fontSize, fs);
    if (CurrFont() != newFont) {
        AppendInstr(DrawInstr::SetFont(newFont));
    }

    DrawStyle style = VecLast(styleStack);
    style.font = newFont;
    VecAppend(styleStack, style);
}

void HtmlFormatter::SetFontBasedOn(PlatformFont* font, PlatformFontStyle fs, float fontSize) {
    Str fontName = font->GetName();
    if (len(fontName) == 0) {
        fontName = defaultFontName;
    }
    SetFont(fontName, fs, fontSize);
}

// change the current font by adding (if addStyle is true) or removing
// a given font style from current font style
// TODO: it doesn't corrctly support the case where a style is wrongly nested
// like "<b>fo<i>oo</b>bar</i>" - "bar" should be italic but will be bold
void HtmlFormatter::ChangeFontStyle(PlatformFontStyle fs, bool addStyle) {
    if (addStyle) {
        SetFontBasedOn(CurrFont(), fs | CurrFont()->GetStyle());
    } else {
        RevertStyleChange();
    }
}

void HtmlFormatter::SetAlignment(AlignAttr align) {
    DrawStyle style = VecLast(styleStack);
    style.align = align;
    VecAppend(styleStack, style);
}

void HtmlFormatter::RevertStyleChange() {
    if (len(styleStack) > 1) {
        DrawStyle style = VecPop(styleStack);
        if (style.font != CurrFont()) {
            AppendInstr(DrawInstr::SetFont(CurrFont()));
        }
        dirRtl = style.dirRtl;
    }
}

static bool IsTextOrImage(const DrawInstr& i) {
    return i.type == DrawInstrType::String || i.type == DrawInstrType::RtlString || i.type == DrawInstrType::Image;
}

static bool IsVisibleDrawInstr(const DrawInstr& i) {
    return IsTextOrImage(i) || i.type == DrawInstrType::Line;
}

static bool IsSpaceDrawInstr(const DrawInstr& i) {
    return i.type == DrawInstrType::FixedSpace || i.type == DrawInstrType::ElasticSpace;
}

// Keep font, link and anchor state when discarding an empty line's spaces.
static void RemoveLineSpaces(Vec<DrawInstr>& instr) {
    for (int k = len(instr); k > 0; k--) {
        if (IsSpaceDrawInstr(instr[k - 1])) {
            VecRemoveAt(instr, k - 1);
        }
    }
}

// sum of widths of all elements with a fixed size and flexible
// spaces (using minimum value for its width)
float HtmlFormatter::CurrLineDx() {
    float dx = NewLineX();
    for (DrawInstr& i : currLineInstr) {
        if (IsTextOrImage(i) || DrawInstrType::FixedSpace == i.type) {
            dx += i.bbox.dx;
        } else if (DrawInstrType::ElasticSpace == i.type) {
            dx += spaceDx;
        }
    }
    return dx;
}

// return the height of the tallest element on the line
float HtmlFormatter::CurrLineDy() {
    float dy = lineSpacing;
    for (DrawInstr& i : currLineInstr) {
        if (IsVisibleDrawInstr(i)) {
            if (i.bbox.dy > dy) {
                dy = i.bbox.dy;
            }
        }
    }
    return dy;
}

// return the width of the left margin (used for paragraph
// indentation inside lists)
float HtmlFormatter::NewLineX() const {
    // TODO: indent based on font size instead?
    float x = 15.f * (float)listDepth;
    if (x < pageDx - 20.f) {
        return x;
    }
    if (pageDx < 20.f) {
        return 0.f;
    }
    return pageDx - 20.f;
}

// When this is called, Width and Height of each element is already set
// We set position x of each visible element
void HtmlFormatter::LayoutLeftStartingAt(float offX) {
    DrawInstr* lastInstr = nullptr;
    int instrCount = 0;

    float x = offX + NewLineX();
    for (DrawInstr& i : currLineInstr) {
        if (IsTextOrImage(i)) {
            i.bbox.x = x;
            x += i.bbox.dx;
            lastInstr = &i;
            instrCount++;
        } else if (DrawInstrType::ElasticSpace == i.type) {
            x += spaceDx;
        } else if (DrawInstrType::FixedSpace == i.type) {
            x += i.bbox.dx;
        }
    }

    // center a single image
    if (instrCount == 1 && DrawInstrType::Image == lastInstr->type) {
        lastInstr->bbox.x = (pageDx - lastInstr->bbox.dx) / 2.f;
    }
}

// TODO: if elements are of different sizes (e.g. texts using different fonts)
// we should align them according to the baseline (which we would first need to
// record for each element)
static void SetYPos(Vec<DrawInstr>& instr, float y) {
    for (DrawInstr& i : instr) {
        if (IsVisibleDrawInstr(i)) {
            i.bbox.y = y;
        }
    }
}

// Redistribute extra space in the line equally among the spaces
void HtmlFormatter::JustifyLineBoth() {
    float extraSpaceDxTotal = pageDx - currX;
    ReportIf(extraSpaceDxTotal < 0.f);

    LayoutLeftStartingAt(0.f);
    size_t spaces = 0;
    bool endsWithSpace = false;
    for (DrawInstr& i : currLineInstr) {
        if (DrawInstrType::ElasticSpace == i.type) {
            ++spaces;
            endsWithSpace = true;
        } else if (IsTextOrImage(i)) {
            endsWithSpace = false;
        }
    }
    // don't take a space at the end of the line into account
    // (the last word is explicitly right-aligned below)
    if (endsWithSpace) {
        spaces--;
    }
    if (0 == spaces) {
        return;
    }
    // redistribute extra dx space among elastic spaces
    float extraSpaceDx = extraSpaceDxTotal / (float)spaces;
    float offX = 0.f;
    DrawInstr* lastStr = nullptr;
    for (DrawInstr& i : currLineInstr) {
        if (DrawInstrType::ElasticSpace == i.type) {
            offX += extraSpaceDx;
        } else if (IsTextOrImage(i)) {
            i.bbox.x += offX;
            lastStr = &i;
        }
    }
    // align the last element perfectly against the right edge in case
    // we've accumulated rounding errors
    if (lastStr) {
        lastStr->bbox.x = pageDx - lastStr->bbox.dx;
    }
}

bool HtmlFormatter::IsCurrLineEmpty() {
    for (DrawInstr& i : currLineInstr) {
        if (IsVisibleDrawInstr(i)) {
            return false;
        }
    }
    return true;
}

void HtmlFormatter::JustifyCurrLine(AlignAttr align) {
    // TODO: is CurrLineDx needed at all?
    ReportIf(currX != CurrLineDx());

    switch (align) {
        case AlignAttr::Left:
            LayoutLeftStartingAt(0.f);
            break;
        case AlignAttr::Right:
            LayoutLeftStartingAt(pageDx - currX);
            break;
        case AlignAttr::Center:
            LayoutLeftStartingAt((pageDx - currX) / 2.f);
            break;
        case AlignAttr::Justify:
            JustifyLineBoth();
            break;
        default:
            ReportIf(true);
            break;
    }

    // when the reading direction is right-to-left, mirror the entire page
    // so that the first element on a line is the right-most, etc.
    if (dirRtl) {
        for (DrawInstr& i : currLineInstr) {
            if (IsVisibleDrawInstr(i)) {
                i.bbox.x = pageDx - i.bbox.x - i.bbox.dx;
            }
        }
    }
}

void HtmlFormatter::ForceNewPage() {
    bool createdNewPage = FlushCurrLine(true);
    if (createdNewPage) {
        return;
    }
    VecAppend(pagesToSend, currPage);

    EmitNewPage();
    currX = NewLineX();
    currLineTopPadding = 0.f;
}

// returns true if created a new page
bool HtmlFormatter::FlushCurrLine(bool isParagraphBreak) {
    if (IsCurrLineEmpty()) {
        currX = NewLineX();
        currLineTopPadding = 0;
        RemoveLineSpaces(currLineInstr);
        return false;
    }
    AlignAttr align = CurrStyle()->align;
    if (isParagraphBreak && (AlignAttr::Justify == align)) {
        align = AlignAttr::Left;
    }
    JustifyCurrLine(align);

    // create a new page if necessary
    float totalLineDy = CurrLineDy() + currLineTopPadding;
    bool createdPage = false;
    if (currY + totalLineDy > pageDy) {
        // current line too big to fit in current page,
        // so need to start another page
        VecAppend(pagesToSend, currPage);
        // instructions for each page need to be self-contained
        // so we have to carry over some state (like current font)
        ReportIf(!CurrFont());
        EmitNewPage();
        ReportIf(currLineReparseIdx > INT_MAX);
        currPage->reparseIdx = (int)currLineReparseIdx;
        createdPage = true;
    }
    SetYPos(currLineInstr, currY + currLineTopPadding);
    currY += totalLineDy;

    DrawInstr link;
    if (currLinkIdx) {
        link = currLineInstr[(int)currLinkIdx - 1];
        // TODO: this occasionally leads to empty links
        AppendInstr(DrawInstr(DrawInstrType::LinkEnd));
    }
    VecAppendN(currPage->instructions, VecData(currLineInstr), len(currLineInstr));
    VecReset(currLineInstr);
    currLineReparseIdx = -1; // mark as not set
    currLineTopPadding = 0;
    currX = NewLineX();
    if (currLinkIdx) {
        AppendInstr(DrawInstr::LinkStart(link.str));
        currLinkIdx = len(currLineInstr);
    }
    nextPageStyle = VecLast(styleStack);
    return createdPage;
}

void HtmlFormatter::EmitNewPage() {
    ReportIf(currReparseIdx > INT_MAX);
    currPage = new HtmlPage((int)currReparseIdx);
    VecAppend(currPage->instructions, DrawInstr::SetFont(nextPageStyle.font));
    currY = 0.f;
}

void HtmlFormatter::EmitEmptyLine(float lineDy) {
    ReportIf(!IsCurrLineEmpty());
    currY += lineDy;
    if (currY <= pageDy) {
        currX = NewLineX();
        RemoveLineSpaces(currLineInstr);
        return;
    }
    ForceNewPage();
}

static bool HasPreviousLineSingleImage(Vec<DrawInstr>& instrs) {
    float imageY = -1;
    for (int idx = len(instrs); idx > 0; idx--) {
        DrawInstr& i = instrs[idx - 1];
        if (!IsVisibleDrawInstr(i)) {
            continue;
        }
        if (-1 != imageY) {
            // if another visible item precedes the image,
            // it must be completely above it (previous line)
            return i.bbox.y + i.bbox.dy <= imageY;
        }
        if (DrawInstrType::Image != i.type) {
            return false;
        }
        imageY = i.bbox.y;
    }
    return imageY != -1;
}

void HtmlFormatter::EmitImageOrAlt(HtmlToken* t, Str img) {
    if (len(img) > 0 && EmitImage(img)) {
        return;
    }
    AttrInfo alt = t->GetAttrByName(StrL("alt"));
    if (alt) {
        HandleText(str::Dup(textAllocator, alt.val));
    }
}

bool HtmlFormatter::EmitImage(Str img) {
    ReportIf(len(img) == 0);
    Pixmap* pixmap = PixmapFromData(img);
    if (!pixmap) {
        return false;
    }
    Size imgSize(pixmap->width, pixmap->height);
    FreePixmap(pixmap);
    if (imgSize.IsEmpty()) {
        return false;
    }

    SizeF newSize((float)imgSize.dx, (float)imgSize.dy);
    // move overly large images to a new line (if they don't fit entirely)
    if (!IsCurrLineEmpty() && (currX + newSize.dx > pageDx || currY + newSize.dy > pageDy)) {
        FlushCurrLine(false);
    }
    // move overly large images to a new page
    // (if they don't fit even when scaled down to 75%)
    float scalePage = std::min((pageDx - currX) / newSize.dx, pageDy / newSize.dy);
    if (currY > 0 && currY + (newSize.dy * std::min(scalePage, 0.75f)) > pageDy) {
        ForceNewPage();
    }
    // if image is bigger than the available space, scale it down
    if (newSize.dx > pageDx - currX || newSize.dy > pageDy - currY) {
        float scale = std::min(scalePage, (pageDy - currY) / newSize.dy);
        // scale down images that follow right after a line
        // containing a single image as little as possible,
        // as they might be intended to be of the same size
        if (scale < scalePage && HasPreviousLineSingleImage(currPage->instructions)) {
            ForceNewPage();
            scale = scalePage;
        }
        if (scale < 1) {
            newSize.dx = std::min(newSize.dx * scale, pageDx - currX);
            newSize.dy = std::min(newSize.dy * scale, pageDy - currY);
        }
    }

    RectF bbox(PointF(currX, 0), newSize);
    AppendInstr(DrawInstr::Image(img, bbox));
    currX += bbox.dx;

    return true;
}

// add horizontal line (<hr> in html terms)
void HtmlFormatter::EmitHr() {
    // hr creates an implicit paragraph break
    FlushCurrLine(true);
    ReportIf(NewLineX() != currX);
    RectF bbox(0.f, 0.f, pageDx, lineSpacing);
    AppendInstr(DrawInstr(DrawInstrType::Line, bbox));
    FlushCurrLine(true);
}

void HtmlFormatter::EmitParagraph(float indent) {
    FlushCurrLine(true);
    ReportIf(NewLineX() != currX);
    bool needsIndent = AlignAttr::Left == CurrStyle()->align || AlignAttr::Justify == CurrStyle()->align;
    if (indent > 0 && needsIndent && EnsureDx(indent)) {
        AppendInstr(DrawInstr::FixedSpace(indent));
        currX += indent;
    }
}

// ensure there is enough dx space left in the current line
// if there isn't, we start a new line
// returns false if dx is bigger than pageDx
bool HtmlFormatter::EnsureDx(float dx) {
    if (currX + dx <= pageDx) {
        return true;
    }
    FlushCurrLine(false);
    return dx <= pageDx;
}

// don't emit multiple spaces and don't emit spaces
// at the beginning of the line
static bool CanEmitElasticSpace(float currX, float NewLineX, float maxCurrX, Vec<DrawInstr>& currLineInstr) {
    if (NewLineX == currX || 0 == len(currLineInstr)) {
        return false;
    }
    // prevent elastic spaces from being flushed to the
    // beginning of the next line
    if (currX > maxCurrX) {
        return false;
    }
    DrawInstr& di = VecLast(currLineInstr);
    // don't add a space if only an anchor would be in between them
    if (DrawInstrType::Anchor == di.type && len(currLineInstr) > 1) {
        di = currLineInstr[len(currLineInstr) - 2];
    }
    return !IsSpaceDrawInstr(di);
}

void HtmlFormatter::EmitElasticSpace() {
    if (!CanEmitElasticSpace(currX, NewLineX(), pageDx - spaceDx, currLineInstr)) {
        return;
    }
    EnsureDx(spaceDx);
    currX += spaceDx;
    AppendInstr(DrawInstr(DrawInstrType::ElasticSpace));
}

// return true if we can break a word on a given character during layout
static bool CanBreakWordOnChar(int c) {
    // don't break on Chinese and Japan characters
    // https://github.com/sumatrapdfreader/sumatrapdf/issues/250
    // https://github.com/sumatrapdfreader/sumatrapdf/pull/1057
    // There are other  ranges, but far less common
    // https://stackoverflow.com/questions/1366068/whats-the-complete-range-for-chinese-characters-in-unicode
    return c >= 0x2E80 && c <= 0xA4CF;
}

// how much of `run` the first `bufLen` bytes of its soft-hyphen-stripped copy
// cover
static int RunLenForBufLen(Str run, int bufLen) {
    int i = 0;
    int n = 0;
    while (i < len(run) && n < bufLen) {
        if ((u8)run.s[i] == 0xC2 && (i + 1) < len(run) && (u8)run.s[i + 1] == 0xAD) {
            i += 2;
            continue;
        }
        i++;
        n++;
    }
    return i;
}

// soft hyphens (U+00AD, 0xC2 0xAD in utf-8) should not be displayed
static void RemoveSoftHyphensInPlace(Str& s) {
    char* dst = s.s;
    const char* src = s.s;
    const char* end = s.s + s.len;
    while (src < end) {
        if ((u8)src[0] == 0xC2 && (src + 1) < end && (u8)src[1] == 0xAD) {
            src += 2;
            continue;
        }
        *dst++ = *src++;
    }
    s.len = (int)(dst - s.s);
}

// a text run is a string of consecutive text with uniform style
void HtmlFormatter::EmitTextRun(Str s) {
    Str run = s;
    currReparseIdx = htmlParser->PosOf(run);
    ReportIf(!ValidReparseIdx(currReparseIdx, htmlParser));
    ReportIf(str::IsEmptyOrWhiteSpace(run) && !preFormatted);
    ::Str tmp = ResolveHtmlEntities(s, textAllocator);
    bool resolved = tmp.s != s.s;
    if (resolved) {
        run = tmp;
    }

    while (run) {
        // don't update the reparseIdx if run doesn't point into the original source
        if (!resolved) {
            currReparseIdx = htmlParser->PosOf(run);
        }

        TempStr buf = str::DupTemp(run);
        RemoveSoftHyphensInPlace(buf);
        if (len(buf) == 0) {
            break;
        }
        RectF bbox = MeasureTextCached(buf);
        if (bbox.dx <= pageDx - currX) {
            AppendInstr(DrawInstr::Text(run, bbox, dirRtl));
            currX += bbox.dx;
            break;
        }
        // get len That Fits the remaining space in the line (pass the width we
        // just measured so it isn't measured again)
        int lenThatFits = textMeasure->StringLenForWidth(buf, pageDx - currX, bbox.dx);
        if (lenThatFits <= 0) {
            FlushCurrLine(false);
            continue;
        }

        // Move an unbroken word to a new line; split it only when the line is empty.
        if (!CanBreakWordOnChar(Utf8CodepointContaining(buf, lenThatFits))) {
            int wordEnd = lenThatFits;
            while (wordEnd > 0 && !CanBreakWordOnChar(Utf8CodepointContaining(buf, wordEnd - 1))) {
                wordEnd--;
            }
            if (wordEnd > 0) {
                lenThatFits = wordEnd;
            } else if (currX != NewLineX()) {
                FlushCurrLine(false);
                continue;
            }
        }

        // never cut a utf-8 sequence in half (this used to be the utf-16
        // surrogate-pair case)
        if (lenThatFits < len(buf)) {
            lenThatFits = Utf8CodepointStartByte(buf, lenThatFits);
        }
        bbox = MeasureTextCached(Str(buf.s, lenThatFits));
        ReportIf(bbox.dx > pageDx);
        // buf is `run` with the soft hyphens removed, so a length in buf maps
        // back to a longer one in run
        int runLenThatFits = RunLenForBufLen(run, lenThatFits);
        AppendInstr(DrawInstr::Text(Str(run.s, runLenThatFits), bbox, dirRtl));
        currX += bbox.dx;
        run = Str(run.s + runLenThatFits, run.len - runLenThatFits);
    }
}

// Emits a synthetic string (e.g. a list bullet or number) at the current
// position. Unlike EmitTextRun, s isn't part of the source HTML, so it must
// stay valid for the lifetime of the page: pass a string literal or one
// allocated in textAllocator.
// emits a synthetic, persistent string (e.g. a list bullet/number)
void HtmlFormatter::EmitTextMarker(Str s) {
    if (len(s) == 0) {
        return;
    }
    RectF bbox = MeasureTextCached(s);
    AppendInstr(DrawInstr::Text(s, bbox, dirRtl));
    currX += bbox.dx;
}

void HtmlFormatter::HandleAnchorAttr(HtmlToken* t, bool idsOnly) {
    if (t->IsEndTag()) {
        return;
    }

    AttrInfo attr = t->GetAttrByName(StrL("id"));
    if (!attr && !idsOnly && Tag_A == t->tag) {
        attr = t->GetAttrByName(StrL("name"));
    }
    if (!attr) {
        return;
    }

    // TODO: make anchors more specific than the top of the current line?
    RectF bbox(0, currY, pageDx, 0);
    // append at the start of the line to prevent the anchor
    // from being flushed to the next page (with wrong currY value)
    // attr.val is owned by the gumbo parse tree which doesn't outlive
    // the formatter, so copy it into textAllocator
    VecAppend(currPage->instructions, DrawInstr::Anchor(str::Dup(textAllocator, attr.val), bbox));
}

void HtmlFormatter::HandleDirAttr(HtmlToken* t) {
    // only apply reading direction changes to block elements (for now)
    if (t->IsStartTag() && !IsInlineTag(t->tag)) {
        AttrInfo attr = t->GetAttrByName(StrL("dir"));
        if (attr) {
            dirRtl = CurrStyle()->dirRtl = attr.ValIs(StrL("RTL"));
        }
    }
}

void HtmlFormatter::HandleTagBr() {
    // make sure to always emit a line
    if (IsCurrLineEmpty()) {
        EmitEmptyLine(lineSpacing);
    } else {
        FlushCurrLine(true);
    }
}

static AlignAttr GetAlignAttr(HtmlToken* t, AlignAttr defVal) {
    AlignAttr align = FindAlignAttr(t->GetAttrByName(StrL("align")).val);
    return align == AlignAttr::NotFound ? defVal : align;
}

void HtmlFormatter::HandleTagP(HtmlToken* t, bool isDiv) {
    if (!t->IsEndTag()) {
        AlignAttr align = CurrStyle()->align;
        float indent = 0;

        StyleRule rule = ComputeStyleRule(t);
        if (rule.textAlign != AlignAttr::NotFound) {
            align = rule.textAlign;
        } else if (!isDiv) {
            // prefer CSS styling to align attribute
            align = GetAlignAttr(t, align);
        }
        if (rule.textIndentUnit != StyleRule::inherit && rule.textIndent > 0) {
            float factor = rule.textIndentUnit == StyleRule::em ? CurrFont()->GetSize() : 1;
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

    AttrInfo attr = t->GetAttrByName(StrL("face"));
    Str faceName = CurrFont()->GetName();
    if (attr && !overrideFontName) {
        TempStr buf = str::DupTemp(attr.val);
        // multiple font names can be comma separated
        if (buf && buf.s[0] != ',') {
            str::TransCharsInPlace(buf, StrL(","), StrL("\0"));
            faceName = buf;
        }
    }

    float fontSize = CurrFont()->GetSize();
    attr = t->GetAttrByName(StrL("size"));
    if (attr) {
        // the sizes are in the range from 1 (tiny) to 7 (huge)
        int size = 3; // normal size
        str::Parse(attr.val, "%d", &size);
        // sizes can also be relative to the current size
        if (len(attr.val) > 0 && ('-' == attr.val.s[0] || '+' == attr.val.s[0])) {
            size += 3;
        }
        size = limitValue(size, 1, 7);
        float scale = (float)pow(1.2f, size - 3);
        fontSize = defaultFontSize * scale;
    }

    SetFont(faceName, CurrFont()->GetStyle(), fontSize);
}

bool HtmlFormatter::HandleTagA(HtmlToken* t, Str linkAttr, HtmlNameMatch match) {
    if (t->IsStartTag() && !currLinkIdx) {
        AttrInfo attr = t->GetAttrByName(linkAttr, match);
        if (attr) {
            // attr.val is owned by the gumbo parse tree which doesn't
            // outlive the formatter, so copy it into textAllocator
            AppendInstr(DrawInstr::LinkStart(str::Dup(textAllocator, attr.val)));
            currLinkIdx = len(currLineInstr);
            return true;
        }
    } else if (t->IsEndTag() && currLinkIdx) {
        AppendInstr(DrawInstr(DrawInstrType::LinkEnd));
        currLinkIdx = 0;
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
        float fontSize = defaultFontSize * (float)pow(1.1f, '5' - t->s.s[1]);
        if (currY > 0) {
            currY += fontSize / 2;
        }
        SetFontBasedOn(CurrFont(), PlatformFontStyle::Bold, fontSize);

        StyleRule rule = ComputeStyleRule(t);
        if (AlignAttr::NotFound == rule.textAlign) {
            rule.textAlign = GetAlignAttr(t, AlignAttr::Left);
        }
        CurrStyle()->align = rule.textAlign;
    }
}

void HtmlFormatter::HandleTagList(HtmlToken* t) {
    FlushCurrLine(true);
    if (t->IsStartTag()) {
        listDepth++;
    } else if (t->IsEndTag() && listDepth > 0) {
        listDepth--;
    }
    currX = NewLineX();
}

void HtmlFormatter::HandleTagPre(HtmlToken* t) {
    FlushCurrLine(true);
    if (t->IsStartTag()) {
        SetFont(StrL("Courier New"), CurrFont()->GetStyle());
        CurrStyle()->align = AlignAttr::Left;
        preFormatted = true;
    } else if (t->IsEndTag()) {
        RevertStyleChange();
        preFormatted = false;
    }
}

StyleRule* HtmlFormatter::FindStyleRule(HtmlTag tag, Str clazz) {
    u32 classHash = MurmurHash2(clazz);
    for (int i = 0; i < len(styleRules); i++) {
        StyleRule& rule = styleRules[i];
        if (tag == rule.tag && classHash == rule.classHash) {
            return &rule;
        }
    }
    return nullptr;
}

StyleRule HtmlFormatter::ComputeStyleRule(HtmlToken* t) {
    StyleRule rule;
    auto mergeRule = [&](HtmlTag tag, Str clazz) {
        if (StyleRule* prev = FindStyleRule(tag, clazz)) {
            rule.Merge(*prev);
        }
    };

    // Apply rules in specificity order, ending with the inline style.
    mergeRule(Tag_Body, {});
    mergeRule(kTagAny, {});
    mergeRule(t->tag, {});

    // TODO: support multiple class names
    AttrInfo attr = t->GetAttrByName(StrL("class"));
    if (attr) {
        mergeRule(kTagAny, attr.val);
        mergeRule(t->tag, attr.val);
    }
    attr = t->GetAttrByName(StrL("style"));
    if (attr) {
        StyleRule newRule = StyleRule::Parse(attr.val);
        rule.Merge(newRule);
    }
    return rule;
}

void HtmlFormatter::ParseStyleSheet(Str data) {
    CssPullParser parser(data);
    while (parser.NextRule()) {
        StyleRule rule = StyleRule::Parse(&parser);
        const CssSelector* sel;
        while ((sel = parser.NextSelector()) != nullptr) {
            if (Tag_NotFound == sel->tag) {
                continue;
            }
            Str clazz = sel->clazz;
            StyleRule* prevRule = FindStyleRule(sel->tag, clazz);
            if (prevRule) {
                prevRule->Merge(rule);
            } else {
                rule.tag = sel->tag;
                rule.classHash = MurmurHash2(clazz);
                VecAppend(styleRules, rule);
            }
        }
    }
}

void HtmlFormatter::HandleTagStyle(HtmlToken* t) {
    if (!t->IsStartTag()) {
        return;
    }
    AttrInfo attr = t->GetAttrByName(StrL("type"));
    if (attr && !attr.ValIs(StrL("text/css"))) {
        return;
    }

    const char* start = t->s.s + len(t->s) + 1;
    do {
        t = htmlParser->Next();
    } while (t && (!t->IsEndTag() || t->tag != Tag_Style));
    if (!t) {
        return;
    }
    const char* end = t->s.s - 2;
    ReportIf(start > end);
    ParseStyleSheet(Str(start, (int)(end - start)));
    UpdateTagNesting(t);
}

// returns true if prev can't contain curr and should thus be closed
static bool AutoCloseOnOpen(HtmlTag curr, HtmlTag prev) {
    ReportIf(IsInlineTag(curr));
    // always start afresh for a new <body>
    if (Tag_Body == curr) {
        return true;
    }
    // allow <div>s to be contained within inline tags
    // (e.g. <i><div>...</div></i> from pg12.mobi)
    if (Tag_Div == curr) {
        return false;
    }

    if (IsHeadingTag(prev)) {
        return IsHeadingTag(curr);
    }

    switch (prev) {
        case Tag_Dd:
        case Tag_Dt:
            return Tag_Dd == curr || Tag_Dt == curr;
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
    HtmlToken tok{};
    tok.type = HtmlToken::EndTag;
    tok.s = {};
    // let HandleHtmlTag clean up (in reverse order)
    for (size_t i = 0; i < count; i++) {
        tok.tag = VecPop(tagNesting);
        HandleHtmlTag(&tok);
    }
    keepTagNesting = false;
}

void HtmlFormatter::UpdateTagNesting(HtmlToken* t) {
    ReportIf(!t->IsTag());
    if (keepTagNesting || Tag_NotFound == t->tag || t->IsEmptyElementEndTag() || IsTagSelfClosing(t->tag)) {
        return;
    }

    int idx = len(tagNesting);
    bool isInline = IsInlineTag(t->tag);
    if (t->IsStartTag()) {
        if (IsInlineTag(t->tag)) {
            VecAppend(tagNesting, t->tag);
            return;
        }
        // close all tags that can't contain this new block-level tag
        for (; idx > 0 && AutoCloseOnOpen(t->tag, tagNesting[idx - 1]); idx--) {
            // no-op
        }
    } else {
        // close all tags that were contained within the current tag
        // (for inline tags just up to the next block-level tag)
        for (; idx > 0 && (!isInline || IsInlineTag(tagNesting[idx - 1])) && t->tag != tagNesting[idx - 1]; idx--) {
            // no-op
        }
        if (0 == idx || tagNesting[idx - 1] != t->tag) {
            return;
        }
    }

    AutoCloseTags(len(tagNesting) - idx);

    if (t->IsStartTag()) {
        VecAppend(tagNesting, t->tag);
    } else {
        ReportIf(!t->IsEndTag() || t->tag != VecLast(tagNesting));
        VecPop(tagNesting);
    }
}

void HtmlFormatter::HandleHtmlTag(HtmlToken* t) {
    ReportIf(!t->IsTag());

    UpdateTagNesting(t);

    HtmlTag tag = t->tag;
    if (Tag_P == tag) {
        HandleTagP(t);
    } else if (Tag_Hr == tag) {
        EmitHr();
    } else if ((Tag_B == tag) || (Tag_Strong == tag)) {
        ChangeFontStyle(PlatformFontStyle::Bold, t->IsStartTag());
    } else if ((Tag_I == tag) || (Tag_Em == tag)) {
        ChangeFontStyle(PlatformFontStyle::Italic, t->IsStartTag());
    } else if (Tag_U == tag) {
        if (!currLinkIdx) {
            ChangeFontStyle(PlatformFontStyle::Underline, t->IsStartTag());
        }
    } else if (Tag_Strike == tag) {
        ChangeFontStyle(PlatformFontStyle::Strikeout, t->IsStartTag());
    } else if (Tag_Br == tag) {
        HandleTagBr();
    } else if (Tag_Font == tag) {
        HandleTagFont(t);
    } else if (Tag_A == tag) {
        HandleTagA(t);
    } else if (Tag_Blockquote == tag || Tag_Dd == tag || Tag_Table == tag) {
        HandleTagList(t);
    } else if (Tag_Div == tag) {
        HandleTagP(t, true);
    } else if (IsHeadingTag(tag)) {
        HandleTagHx(t);
    } else if (Tag_Center == tag) {
        HandleTagP(t, true);
        if (!t->IsEndTag()) {
            CurrStyle()->align = AlignAttr::Center;
        }
    } else if ((Tag_Ul == tag) || (Tag_Ol == tag)) {
        HandleTagList(t);
        if (t->IsStartTag()) {
            ListInfo li;
            li.ordered = (Tag_Ol == tag);
            if (li.ordered) {
                // honor <ol start="N">
                AttrInfo attr = t->GetAttrByName(StrL("start"));
                if (attr) {
                    li.nextNum = ParseInt(attr.val);
                }
            }
            VecAppend(listInfos, li);
        } else if (t->IsEndTag() && len(listInfos) > 0) {
            VecRemoveLast(listInfos);
        }
    } else if (Tag_Li == tag) {
        FlushCurrLine(true);
        if (t->IsStartTag() && len(listInfos) > 0) {
            ListInfo& li = VecLast(listInfos);
            if (li.ordered) {
                Str marker = str::Dup(textAllocator, fmt("%d. ", li.nextNum));
                li.nextNum++;
                EmitTextMarker(marker);
            } else {
                EmitTextMarker(StrL("\xe2\x80\xa2  ")); // U+2022 bullet + 2 spaces
            }
        }
    } else if (Tag_Dt == tag) {
        FlushCurrLine(true);
        ChangeFontStyle(PlatformFontStyle::Bold, t->IsStartTag());
        if (t->IsStartTag()) {
            CurrStyle()->align = AlignAttr::Left;
        }
    } else if (Tag_Tr == tag) {
        // display tables row-by-row for now
        FlushCurrLine(true);
        if (t->IsStartTag()) {
            SetAlignment(AlignAttr::Left);
        } else if (t->IsEndTag()) {
            RevertStyleChange();
        }
    } else if (Tag_Code == tag || Tag_Tt == tag) {
        if (t->IsStartTag()) {
            SetFont(StrL("Courier New"), CurrFont()->GetStyle());
        } else if (t->IsEndTag()) {
            RevertStyleChange();
        }
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
    }

    // any tag could contain anchor information
    HandleAnchorAttr(t);
    // any tag could contain a reading direction change
    HandleDirAttr(t);
}

void HtmlFormatter::HandleText(Str curr) {
    if (preFormatted) {
        // don't collapse whitespace and respect text newlines
        while (curr) {
            currReparseIdx = htmlParser->PosOf(curr);
            Str text, rest;
            bool newline = str::CutChar(curr, '\n', &text, &rest);
            if (newline) {
                str::TrimSuffix(text, StrL("\r"));
            }
            EmitTextRun(text);
            if (!newline) {
                break;
            }
            curr = rest;
            HandleTagBr();
        }
        return;
    }

    // break text into runs i.e. chunks that are either all
    // whitespace or all non-whitespace
    while (curr) {
        currReparseIdx = htmlParser->PosOf(curr);
        int skipped = str::TrimWs(curr);
        if (skipped > 0) {
            EmitElasticSpace();
        }

        currReparseIdx += skipped;
        Str text = curr;
        text.len = str::TrimNonWs(curr);
        if (len(text) > 0) {
            EmitTextRun(text);
        }
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

// Rule-only pages count as empty.
static bool IsEmptyPage(HtmlPage* p) {
    if (!p) {
        return false;
    }
    for (DrawInstr& i : p->instructions) {
        if (IsTextOrImage(i)) {
            return false;
        }
    }
    return true;
}

// Return queued pages, parsing more tokens as needed.
HtmlPage* HtmlFormatter::Next(bool skipEmptyPages) {
    AtomicIntInc(&gAllowAllocFailure);
    AutoCall decAllowAlloc(AtomicIntDec, &gAllowAllocFailure);

    for (;;) {
        // send out all pages accumulated so far
        while (len(pagesToSend) > 0) {
            HtmlPage* ret = VecPopAt(pagesToSend, 0);
            if (skipEmptyPages && IsEmptyPage(ret)) {
                delete ret;
            } else {
                return ret;
            }
        }
        if (finishedParsing) {
            return nullptr;
        }
        HtmlToken* t = htmlParser->Next();
        if (!t) {
            AutoCloseTags(len(tagNesting));
            FlushCurrLine(true);
            VecAppend(pagesToSend, currPage);
            currPage = nullptr;
            finishedParsing = true;
            // Discard final empty pages after finishing the parser.
            skipEmptyPages = true;
            continue;
        }

        currReparseIdx = htmlParser->PosOf(t->reparsePoint);
        ReportIf(!ValidReparseIdx(currReparseIdx, htmlParser));
        if (t->IsTag()) {
            HandleHtmlTag(t);
        } else if (!IgnoreText()) {
            ReportIf(!t->IsText());
            HandleText(t->s);
        }
    }
}

// convenience method to format the whole html
Vec<HtmlPage*>* HtmlFormatter::FormatAllPages(bool skipEmptyPages) {
    Vec<HtmlPage*>* pages = new Vec<HtmlPage*>();
    for (HtmlPage* pd = Next(skipEmptyPages); pd; pd = Next(skipEmptyPages)) {
        VecAppend(*pages, pd);
    }
    return pages;
}

#if OS_WIN || OS_LINUX || OS_DARWIN
// Draw text in one lock before shapes; GDI's GetHDC/ReleaseHDC is expensive.
static void DrawHtmlText(PlatformTextRender* textDraw, Vec<DrawInstr>* drawInstructions, float offX, float offY,
                         Color textColor, bool* abortCookie) {
    textDraw->SetTextColor(textColor);
    textDraw->Lock();
    for (DrawInstr& i : *drawInstructions) {
        RectF bbox = i.bbox;
        bbox.Offset(offX, offY);
        if (DrawInstrType::String == i.type || DrawInstrType::RtlString == i.type) {
            TempStr buf = str::DupTemp(i.str);
            RemoveSoftHyphensInPlace(buf);
            textDraw->Draw(buf, bbox, DrawInstrType::RtlString == i.type);
        } else if (DrawInstrType::SetFont == i.type) {
            textDraw->SetFont(i.font);
        }
        if (abortCookie && *abortCookie) {
            break;
        }
    }
    textDraw->Unlock();
}
#endif

// TODO: draw link in the appropriate format (blue text, underlined, should show hand cursor when
// mouse is over a link. There's a slight complication here: we only get explicit information about
// strings, not about the whitespace and we should underline the whitespace as well. Also the text
// should be underlined at a baseline
#if OS_WIN
using Gdiplus::Bitmap;
using Gdiplus::Ok;
using Gdiplus::Pen;
using Gdiplus::Status;
using Gdiplus::UnitPixel;
using Gdiplus::Win32Error;

void DrawHtmlPage(Gdiplus::Graphics* g, PlatformTextRender* textDraw, Vec<DrawInstr>* drawInstructions, float offX,
                  float offY, bool showBbox, Color textColor, bool* abortCookie) {
    Pen debugPen(Gdiplus::Color(255, 0, 0), 1);
    Pen linePen(Gdiplus::Color(0x5F, 0x4B, 0x32), 2.f);
    Pen linkPen(textColor);

    DrawHtmlText(textDraw, drawInstructions, offX, offY, textColor, abortCookie);

    Status status;
    for (DrawInstr& i : *drawInstructions) {
        RectF bbox = i.bbox;
        bbox.x += offX;
        bbox.y += offY;
        if (DrawInstrType::Line == i.type || DrawInstrType::LinkStart == i.type) {
            bool rule = DrawInstrType::Line == i.type;
            float y = floorf(bbox.y + (rule ? bbox.dy / 2.f : bbox.dy) + 0.5f);
            Gdiplus::PointF p1(bbox.x, y);
            Gdiplus::PointF p2(bbox.x + bbox.dx, y);
            if (rule && showBbox) {
                status = g->DrawRectangle(&debugPen, ToGdipRectF(bbox));
                ReportIf(status != Ok);
            }
            status = g->DrawLine(rule ? &linePen : &linkPen, p1, p2);
            ReportIf(status != Ok);
        } else if (DrawInstrType::Image == i.type) {
            // TODO: cache the bitmap somewhere (?)
            Bitmap* bmp = NewGdiplusBitmapFromPixmap(PixmapFromData(i.GetImage()));
            if (bmp) {
                status = g->DrawImage(bmp, ToGdipRectF(bbox), 0, 0, (float)bmp->GetWidth(), (float)bmp->GetHeight(),
                                      UnitPixel);
                // GDI+ sometimes seems to succeed in loading an image because it lazily decodes it
                ReportIf(status != Ok && status != Win32Error);
            }
            delete bmp;
        } else if (DrawInstrType::String == i.type || DrawInstrType::RtlString == i.type) {
            if (showBbox) {
                status = g->DrawRectangle(&debugPen, ToGdipRectF(bbox));
                ReportIf(status != Ok);
            }
        } else if (DrawInstrType::LinkEnd == i.type) {
            // TODO: set text color back again
        } else if (IsSpaceDrawInstr(i) || (DrawInstrType::SetFont == i.type) || (DrawInstrType::Anchor == i.type) ||
                   (DrawInstrType::PageMarkerAnchor == i.type)) {
            // ignore
        } else {
            ReportIf(true);
        }
        if (abortCookie && *abortCookie) {
            break;
        }
    }
}
#endif

#if OS_LINUX || OS_DARWIN
static Pixmap* PixmapForHtml(Pixmap* src) {
    if (!src || !src->data || src->format == PixmapFormat::Native) {
        return nullptr;
    }
    Pixmap* dst = AllocPixmap(src->width, src->height, PixmapFormat::BGRA8, true);
    if (!dst) {
        return nullptr;
    }
    int bpp = PixmapBytesPerPixel(src->format);
    bool rgba = src->format == PixmapFormat::RGBA8;
    for (int y = 0; y < src->height; y++) {
        const u8* s = src->data + (size_t)y * src->stride;
        u8* d = dst->data + (size_t)y * dst->stride;
        for (int x = 0; x < src->width; x++) {
            u32 a = bpp == 4 ? s[3] : 255;
            u8 r = rgba ? s[0] : s[2];
            u8 g = s[1];
            u8 b = rgba ? s[2] : s[0];
            d[0] = src->premultiplied ? b : (u8)(((u32)b * a + 127) / 255);
            d[1] = src->premultiplied ? g : (u8)(((u32)g * a + 127) / 255);
            d[2] = src->premultiplied ? r : (u8)(((u32)r * a + 127) / 255);
            d[3] = (u8)a;
            s += bpp;
            d += 4;
        }
    }
    return dst;
}

#endif

#if OS_LINUX
using HtmlDrawContext = cairo_t*;

static void HtmlSetColor(cairo_t* cairo, Color col) {
    u8 r = 0;
    u8 g = 0;
    u8 b = 0;
    UnpackColor(col, r, g, b);
    cairo_set_source_rgb(cairo, r / 255.0, g / 255.0, b / 255.0);
}

static void HtmlDrawImage(cairo_t* cairo, Str data, RectF bbox) {
    Pixmap* decoded = PixmapFromData(data);
    Pixmap* pixmap = PixmapForHtml(decoded);
    FreePixmap(decoded);
    if (!pixmap) {
        return;
    }
    cairo_surface_t* surface = cairo_image_surface_create_for_data(pixmap->data, CAIRO_FORMAT_ARGB32, pixmap->width,
                                                                   pixmap->height, pixmap->stride);
    if (cairo_surface_status(surface) == CAIRO_STATUS_SUCCESS) {
        cairo_save(cairo);
        cairo_translate(cairo, bbox.x, bbox.y);
        cairo_scale(cairo, bbox.dx / pixmap->width, bbox.dy / pixmap->height);
        cairo_set_source_surface(cairo, surface, 0, 0);
        cairo_rectangle(cairo, 0, 0, pixmap->width, pixmap->height);
        cairo_fill(cairo);
        cairo_restore(cairo);
    }
    cairo_surface_destroy(surface);
    FreePixmap(pixmap);
}

#endif

#if OS_DARWIN
using HtmlDrawContext = CGContextRef;

static void HtmlSetColor(CGContextRef context, Color color) {
    u8 r = 0;
    u8 g = 0;
    u8 b = 0;
    UnpackColor(color, r, g, b);
    CGContextSetRGBStrokeColor(context, r / 255.0, g / 255.0, b / 255.0, 1);
}

static void HtmlDrawImage(CGContextRef context, Str data, RectF bbox) {
    Pixmap* decoded = PixmapFromData(data);
    Pixmap* pixmap = PixmapForHtml(decoded);
    FreePixmap(decoded);
    if (!pixmap) {
        return;
    }
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGBitmapInfo info = (CGBitmapInfo)((u32)kCGBitmapByteOrder32Little | (u32)kCGImageAlphaPremultipliedFirst);
    CGContextRef bitmap =
        CGBitmapContextCreate(pixmap->data, pixmap->width, pixmap->height, 8, pixmap->stride, colorSpace, info);
    CGImageRef image = bitmap ? CGBitmapContextCreateImage(bitmap) : nullptr;
    if (image) {
        CGContextSaveGState(context);
        CGContextTranslateCTM(context, bbox.x, bbox.y + bbox.dy);
        CGContextScaleCTM(context, bbox.dx / pixmap->width, -bbox.dy / pixmap->height);
        CGContextDrawImage(context, CGRectMake(0, 0, pixmap->width, pixmap->height), image);
        CGContextRestoreGState(context);
        CGImageRelease(image);
    }
    if (bitmap) {
        CGContextRelease(bitmap);
    }
    CGColorSpaceRelease(colorSpace);
    FreePixmap(pixmap);
}

#endif

#if OS_LINUX || OS_DARWIN
void DrawHtmlPage(HtmlDrawContext context, PlatformTextRender* textDraw, Vec<DrawInstr>* drawInstructions, float offX,
                  float offY, bool showBbox, Color textColor, bool* abortCookie) {
    DrawHtmlText(textDraw, drawInstructions, offX, offY, textColor, abortCookie);

    for (DrawInstr& i : *drawInstructions) {
        RectF bbox = i.bbox;
        bbox.Offset(offX, offY);
        if (DrawInstrType::Line == i.type || DrawInstrType::LinkStart == i.type) {
            bool rule = DrawInstrType::Line == i.type;
            float y = floorf(bbox.y + (rule ? bbox.dy / 2.f : bbox.dy) + 0.5f);
            HtmlSetColor(context, rule ? MkRgb(0x5f, 0x4b, 0x32) : textColor);
#if OS_LINUX
            cairo_set_line_width(context, rule ? 2 : 1);
            cairo_move_to(context, bbox.x, y);
            cairo_line_to(context, bbox.x + bbox.dx, y);
            cairo_stroke(context);
#else
            CGContextSetLineWidth(context, rule ? 2 : 1);
            CGContextMoveToPoint(context, bbox.x, y);
            CGContextAddLineToPoint(context, bbox.x + bbox.dx, y);
            CGContextStrokePath(context);
#endif
        } else if (DrawInstrType::Image == i.type) {
            HtmlDrawImage(context, i.GetImage(), bbox);
        } else if ((DrawInstrType::String == i.type || DrawInstrType::RtlString == i.type) && showBbox) {
            HtmlSetColor(context, kColRed);
#if OS_LINUX
            cairo_set_line_width(context, 1);
            cairo_rectangle(context, bbox.x, bbox.y, bbox.dx, bbox.dy);
            cairo_stroke(context);
#else
            CGContextSetLineWidth(context, 1);
            CGContextStrokeRect(context, CGRectMake(bbox.x, bbox.y, bbox.dx, bbox.dy));
#endif
        }
        if (abortCookie && *abortCookie) {
            break;
        }
    }
}
#endif

HtmlFormatterArgs* CreateFormatterDefaultArgs(int dx, int dy, Arena* textAllocator) {
    HtmlFormatterArgs* args = new HtmlFormatterArgs();
    args->SetFontName(L"Georgia");
    args->fontSize = 12.5f;
    args->pageDx = (float)dx;
    args->pageDy = (float)dy;
    args->textAllocator = textAllocator;
    return args;
}
