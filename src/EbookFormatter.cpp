/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "EbookFormatter.h"
#include "EbookDoc.h"
using namespace Gdiplus;
#include "GdiPlusUtil.h"
#include "HtmlPullParser.h"
#include "MobiDoc.h"

#define FONT_NAME              L"Georgia"
#define FONT_SIZE              12.5f

HtmlFormatterArgs *CreateFormatterArgsDoc(Doc doc, int dx, int dy, PoolAllocator *textAllocator)
{
    HtmlFormatterArgs *args = new HtmlFormatterArgs();
    args->htmlStr = doc.GetHtmlData(args->htmlStrLen);
    CrashIf(!args->htmlStr);
    args->fontName = FONT_NAME;
    args->fontSize = FONT_SIZE;
    args->pageDx = (REAL)dx;
    args->pageDy = (REAL)dy;
    args->textAllocator = textAllocator;
    return args;
}

HtmlFormatter *CreateFormatter(Doc doc, HtmlFormatterArgs* args)
{
    if (doc.AsMobi())
        return new MobiFormatter(args, doc.AsMobi());
    if (doc.AsMobiTest())
        return new MobiFormatter(args, NULL);
    if (doc.AsEpub())
        return new EpubFormatter(args, doc.AsEpub());
    CrashIf(true);
    return NULL;
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
void MobiFormatter::HandleTagImg(HtmlToken *t)
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

void MobiFormatter::HandleHtmlTag(HtmlToken *t)
{
    CrashIf(!t->IsTag());

    if (Tag_P == t->tag || Tag_Blockquote == t->tag) {
        HtmlFormatter::HandleHtmlTag(t);
        HandleSpacing_Mobi(t);
    } else if (Tag_Mbp_Pagebreak == t->tag) {
        ForceNewPage();
    } else if (Tag_A == t->tag) {
        HandleAnchorAttr(t);
        // handle internal and external links (prefer internal ones)
        if (!HandleTagA(t, "filepos"))
            HandleTagA(t);
    } else if (Tag_Hr == t->tag) {
        // imitating Kindle: hr is proceeded by an empty line
        FlushCurrLine(false);
        EmitEmptyLine(lineSpacing);
        EmitHr();
    } else {
        HtmlFormatter::HandleHtmlTag(t);
    }
}

/* EPUB-specific formatting methods */

void EpubFormatter::HandleTagImg(HtmlToken *t)
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

void EpubFormatter::HandleTagPagebreak(HtmlToken *t)
{
    AttrInfo *attr = t->GetAttrByName("page_path");
    if (!attr || pagePath)
        ForceNewPage();
    if (attr) {
        RectF bbox(0, currY, pageDx, 0);
        currPage->instructions.Append(DrawInstr::Anchor(attr->val, attr->valLen, bbox));
        pagePath.Set(str::DupN(attr->val, attr->valLen));
    }
}

void EpubFormatter::HandleTagSvgImage(HtmlToken *t)
{
    CrashIf(!epubDoc);
    if (t->IsEndTag())
        return;
    if (tagNesting.Find(Tag_Svg) == -1)
        return;
    AttrInfo *attr = t->GetAttrByNameNS("href", "http://www.w3.org/1999/xlink");
    if (!attr)
        return;
    ScopedMem<char> src(str::DupN(attr->val, attr->valLen));
    ImageData *img = epubDoc->GetImageData(src, pagePath);
    if (img)
        EmitImage(img);
}

void EpubFormatter::HandleHtmlTag(HtmlToken *t)
{
    CrashIf(!t->IsTag());
    if (hiddenDepth && t->IsEndTag() && tagNesting.Count() == hiddenDepth &&
        t->tag == tagNesting.Last()) {
        hiddenDepth = 0;
        UpdateTagNesting(t);
        return;
    }
    if (0 == hiddenDepth && t->IsStartTag() && t->GetAttrByName("hidden"))
        hiddenDepth = tagNesting.Count() + 1;
    if (hiddenDepth > 0)
        UpdateTagNesting(t);
    else if (Tag_Image == t->tag)
        HandleTagSvgImage(t);
    else
        HtmlFormatter::HandleHtmlTag(t);
}

bool EpubFormatter::IgnoreText()
{
    return hiddenDepth > 0 || HtmlFormatter::IgnoreText();
}

/* FictionBook-specific formatting methods */

Fb2Formatter::Fb2Formatter(HtmlFormatterArgs *args, Fb2Doc *doc) :
    HtmlFormatter(args), fb2Doc(doc), section(1), titleCount(0)
{
    if (args->reparseIdx != 0)
        return;
    ImageData *cover = doc->GetCoverImage();
    if (!cover)
        return;
    EmitImage(cover);
    // render larger images alone on the cover page,
    // smaller images just separated by a horizontal line
    if (0 == currLineInstr.Count())
        /* the image was broken */;
    else if (currLineInstr.Last().bbox.Height > args->pageDy / 2)
        ForceNewPage();
    else
        EmitHr();
}

void Fb2Formatter::HandleTagImg(HtmlToken *t)
{
    CrashIf(!fb2Doc);
    if (t->IsEndTag())
        return;
    AttrInfo *attr = t->GetAttrByNameNS("href", "http://www.w3.org/1999/xlink");
    if (!attr)
        return;
    ScopedMem<char> src(str::DupN(attr->val, attr->valLen));
    ImageData *img = fb2Doc->GetImageData(src);
    if (img)
        EmitImage(img);
}

void Fb2Formatter::HandleTagAsHtml(HtmlToken *t, const char *name)
{
    HtmlToken tok;
    tok.SetTag(t->type, name, name + str::Len(name));
    HtmlFormatter::HandleHtmlTag(&tok);
}

// the name doesn't quite fit: this handles FB2 tags
void Fb2Formatter::HandleHtmlTag(HtmlToken *t)
{
    if (Tag_Title == t->tag || Tag_Subtitle == t->tag) {
        bool isSubtitle = Tag_Subtitle == t->tag;
        ScopedMem<char> name(str::Format("h%d", section + (isSubtitle ? 1 : 0)));
        HtmlToken tok;
        tok.SetTag(t->type, name, name + str::Len(name));
        HandleTagHx(&tok);
        HandleAnchorAttr(t);
        if (!isSubtitle && t->IsStartTag()) {
            char *link = (char *)Allocator::Alloc(textAllocator, 24);
            sprintf_s(link, 24, FB2_TOC_ENTRY_MARK "%d", ++titleCount);
            currPage->instructions.Append(DrawInstr::Anchor(link, str::Len(link), RectF(0, currY, pageDx, 0)));
        }
    }
    else if (Tag_Section == t->tag) {
        if (t->IsStartTag())
            section++;
        else if (t->IsEndTag() && section > 1)
            section--;
        FlushCurrLine(true);
        HandleAnchorAttr(t);
    }
    else if (Tag_P == t->tag) {
        if (htmlParser->tagNesting.Find(Tag_Title) == -1)
            HtmlFormatter::HandleHtmlTag(t);
    }
    else if (Tag_Image == t->tag) {
        HandleTagImg(t);
        HandleAnchorAttr(t);
    }
    else if (Tag_A == t->tag) {
        HandleTagA(t, "href", "http://www.w3.org/1999/xlink");
        HandleAnchorAttr(t, true);
    }
    else if (Tag_Pagebreak == t->tag)
        ForceNewPage();
    else if (Tag_Strong == t->tag)
        HandleTagAsHtml(t, "b");
    else if (t->NameIs("emphasis"))
        HandleTagAsHtml(t, "i");
    else if (t->NameIs("epigraph"))
        HandleTagAsHtml(t, "blockquote");
    else if (t->NameIs("empty-line")) {
        if (!t->IsEndTag())
            EmitParagraph(0);
    }
}

/* PalmDOC-specific formatting methods */

void PdbFormatter::HandleTagImg(HtmlToken *t)
{
    CrashIf(!palmDoc);
    if (t->IsEndTag())
        return;
    AttrInfo *attr = t->GetAttrByName("src");
    if (!attr)
        return;
    ScopedMem<char> src(str::DupN(attr->val, attr->valLen));
    ImageData *img = palmDoc->GetImageData(src);
    if (img)
        EmitImage(img);
}

/* standalone HTML-specific formatting methods */

void HtmlFileFormatter::HandleTagImg(HtmlToken *t)
{
    CrashIf(!htmlDoc);
    if (t->IsEndTag())
        return;
    AttrInfo *attr = t->GetAttrByName("src");
    if (!attr)
        return;
    ScopedMem<char> src(str::DupN(attr->val, attr->valLen));
    ImageData *img = htmlDoc->GetImageData(src);
    if (img)
        EmitImage(img);
}
