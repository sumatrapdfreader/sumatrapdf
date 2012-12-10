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
