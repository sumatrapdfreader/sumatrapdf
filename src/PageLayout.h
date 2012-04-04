/* Copyright 2011-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef PageLayout_h
#define PageLayout_h

#include "BaseUtil.h"
#include "EbookBase.h"
#include "HtmlPullParser.h"
#include "Scoped.h"
#include "Vec.h"

using namespace Gdiplus;

// Layout information for a given page is a list of
// draw instructions that define what to draw and where.
enum DrawInstrType {
    // a piece of text
    InstrString = 0,
    // elastic space takes at least spaceDx pixels but can take more
    // if a line is justified
    InstrElasticSpace,
    // a fixed space takes a fixed amount of pixels. It's used e.g.
    // to implement paragraph indentation
    InstrFixedSpace,
    // a vertical line
    InstrLine,
    // change current font
    InstrSetFont,
    InstrImage,
    // marks the beginning of a link (<a> tag)
    InstrLinkStart,
    // marks end of the link (must have matching InstrLinkStart)
    InstrLinkEnd,
    // marks an anchor an internal link might refer to
    InstrAnchor,
};

struct DrawInstr {
    DrawInstrType       type;
    union {
        // info specific to a given instruction
        struct {
            const char *s;
            size_t      len;
        }                   str;          // InstrString, InstrLinkStart, InstrAnchor
        Font *              font;         // InstrSetFont
        ImageData           img;
    };
    RectF bbox; // common to most instructions

    DrawInstr() { }

    DrawInstr(DrawInstrType t, RectF bbox = RectF()) : type(t), bbox(bbox) { }

    // helper constructors for instructions that need additional arguments
    static DrawInstr Str(const char *s, size_t len, RectF bbox);
    static DrawInstr Image(char *data, size_t len, RectF bbox);
    static DrawInstr SetFont(Font *font);
    static DrawInstr FixedSpace(float dx);
    static DrawInstr LinkStart(const char *s, size_t len);
    static DrawInstr Anchor(const char *s, size_t len, RectF bbox);
};

struct DrawStyle {
    Font *font;
    AlignAttr align;
};

class PageData {
public:
    PageData() : reparseIdx(0), listDepth(0), preFormatted(false) { }

    Vec<DrawInstr>  instructions;
    // if we start parsing html again from reparseIdx, we should
    // get the same instructions. reparseIdx is an offset within
    // html data
    int             reparseIdx;
    // a copy of the current style stack, so that styling
    // doesn't change on a relayout from reparseIdx
    Vec<DrawStyle>  styleStack;
    // further information that is required for reliable relayouting
    int listDepth;
    bool preFormatted;
};

// just to pack args to HtmlFormatter
class LayoutInfo {
public:
    LayoutInfo() :
      pageDx(0), pageDy(0), fontName(NULL), fontSize(0),
      textAllocator(NULL), htmlStr(0), htmlStrLen(0),
      reparseIdx(0)
    { }

    int             pageDx;
    int             pageDy;

    const WCHAR *   fontName;
    float           fontSize;

    /* Most of the time string DrawInstr point to original html text
       that is read-only and outlives us. Sometimes (e.g. when resolving
       html entities) we need a modified text. This allocator is
       used to allocate this text. */
    Allocator *     textAllocator;

    const char *    htmlStr;
    size_t          htmlStrLen;

    // we start parsing from htmlStr + reparseIdx
    int             reparseIdx;
};

class HtmlFormatter
{
protected:
    void HandleAnchorTag(HtmlToken *t, bool idsOnly=false);
    void HandleTagBr();
    void HandleTagP(HtmlToken *t);
    void HandleTagFont(HtmlToken *t);
    bool HandleTagA(HtmlToken *t, const char *linkAttr="href");
    void HandleTagHx(HtmlToken *t);
    void HandleTagList(HtmlToken *t);
    void HandleTagPre(HtmlToken *t);
    void HandleHtmlTag(HtmlToken *t);
    void HandleText(HtmlToken *t);

    float CurrLineDx();
    float CurrLineDy();
    float NewLineX();
    void  LayoutLeftStartingAt(REAL offX);
    void  JustifyLineBoth();
    void  JustifyCurrLine(AlignAttr align);
    bool  FlushCurrLine(bool isParagraphBreak);
    void  UpdateLinkBboxes(PageData *page);

    void  EmitImage(ImageData *img);
    void  EmitHr();
    void  EmitTextRun(const char *s, const char *end);
    void  EmitElasticSpace();
    void  EmitParagraph(float indent);
    void  EmitEmptyLine(float lineDy);
    void  EmitNewPage();
    void  ForceNewPage();
    bool  EnsureDx(float dx);

    DrawStyle *CurrStyle() { return &styleStack.Last(); }
    Font *CurrFont() { return CurrStyle()->font; }
    void  SetFont(const WCHAR *fontName, FontStyle fs, float fontSize=-1);
    void  SetFont(Font *origFont, FontStyle fs, float fontSize=-1);
    void  ChangeFontStyle(FontStyle fs, bool isStart);
    void  SetAlignment(AlignAttr align);
    void  RevertStyleChange();

    void  AppendInstr(DrawInstr di);
    bool  IsCurrLineEmpty();
    bool  IgnoreText();

    // constant during layout process
    LayoutInfo *        layoutInfo;
    float               pageDx;
    float               pageDy;
    float               lineSpacing;
    float               spaceDx;
    Graphics *          gfx; // for measuring text
    ScopedMem<WCHAR>    defaultFontName;
    float               defaultFontSize;
    Allocator *         textAllocator;

    Vec<DrawStyle>      styleStack;
    // current position in a page
    float               currX, currY;
    // remembered when we start a new line, used when we actually
    // layout a line
    float               currLineTopPadding;
    // number of nested lists for indenting whole paragraphs
    int                 listDepth;
    // set if newlines are not to be ignored
    bool                preFormatted;

    // isntructions for the current line
    Vec<DrawInstr>      currLineInstr;
    // reparse point of the first instructions in a current line
    int                 currLineReparseIdx;
    PageData *          currPage;

    // for tracking whether we're currently inside <a> tag
    size_t              currLinkIdx;

    // reparse point for the current HtmlToken
    int                 currReparseIdx;

    HtmlPullParser *    htmlParser;

    // list of pages that we've created but haven't yet sent to client
    Vec<PageData*>      pagesToSend;

    WCHAR               buf[512];

public:
    HtmlFormatter(LayoutInfo *li);
    virtual ~HtmlFormatter();

    virtual PageData *Next() { CrashIf(true); return NULL; }
};

#include "MobiDoc.h"

class MobiFormatter : public HtmlFormatter {
    // accessor to images (and other format-specific data)
    MobiDoc *           doc;
    // remember cover image if we've generated one, so that we
    // can avoid adding the same image twice if it's early in
    // the book
    ImageData *         coverImage;
    // number of pages generated so far, approximate. Only used
    // for detection of cover image duplicates
    int                 pageCount;

    bool                finishedParsing;

    void HandleSpacing_Mobi(HtmlToken *t);
    void HandleTagImg_Mobi(HtmlToken *t);
    void HandleHtmlTag_Mobi(HtmlToken *t);

public:
    MobiFormatter(LayoutInfo *li, MobiDoc *doc);

    virtual PageData *Next();
    Vec<PageData*> *FormatAllPages();
};

void DrawPageLayout(Graphics *g, Vec<DrawInstr> *drawInstructions, REAL offX, REAL offY, bool showBbox, Color *textColor=NULL);

#endif
