/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/GdiPlus.h"
#include "base/Archive.h"
#include "base/HtmlTags.h"
#include "GumboHtmlParser.h"
#include "mui/Mui.h"

#include "wingui/UIModels.h"

#include "DocProperties.h"
#include "DocController.h"
#include "EbookBase.h"
#include "EbookDoc.h"
#include "PalmDbReader.h"
#include "MobiDoc.h"
#include "HtmlFormatter.h"
#include "EbookFormatter.h"

/* Mobi-specific formatting methods */

MobiFormatter::MobiFormatter(HtmlFormatterArgs* args, MobiDoc* doc) : HtmlFormatter(args), doc(doc) {
    bool fromBeginning = (0 == args->reparseIdx);
    if (!doc || !fromBeginning) {
        return;
    }

    Str img = doc->GetCoverImage();
    if (!img) {
        return;
    }

    // TODO: vertically center the cover image?
    EmitImage(img);
    // only add a new page if the image isn't broken
    if (len(currLineInstr) > 0) {
        ForceNewPage();
    }
}

// parses size in the form "1em" or "3pt". To interpret ems we need emInPoints
// to be passed by the caller
static float ParseSizeAsPixels(Str s, float emInPoints) {
    float sizeInPoints = 0;
    if (!str::IsNull(str::Parse(s, "%fem", &sizeInPoints))) {
        sizeInPoints *= emInPoints;
    } else if (!str::IsNull(str::Parse(s, "%fin", &sizeInPoints))) {
        sizeInPoints *= 72;
    } else if (!str::IsNull(str::Parse(s, "%fpt", &sizeInPoints))) {
        // no conversion needed
    } else if (!str::IsNull(str::Parse(s, "%fpx", &sizeInPoints))) {
        return sizeInPoints;
    } else {
        return 0;
    }
    // TODO: take dpi into account
    float sizeInPixels = sizeInPoints;
    return sizeInPixels;
}

void MobiFormatter::HandleSpacing_Mobi(HtmlToken* t) {
    if (!t->IsStartTag()) {
        return;
    }

    // best I can tell, in mobi <p width="1em" height="3pt> means that
    // the first line of the paragrap is indented by 1em and there's
    // 3pt top padding (the same seems to apply for <blockquote>)
    AttrInfo* attr = t->GetAttrByName(StrL("width"));
    if (attr) {
        float lineIndent = ParseSizeAsPixels(attr->val, CurrFont()->GetSize());
        // there are files with negative width which produces partially invisible
        // text, so don't allow that
        if (lineIndent > 0) {
            // this should replace the previously emitted paragraph/quote block
            EmitParagraph(lineIndent);
        }
    }
    attr = t->GetAttrByName(StrL("height"));
    if (attr) {
        // for use it in FlushCurrLine()
        currLineTopPadding = ParseSizeAsPixels(attr->val, CurrFont()->GetSize());
    }
}

// mobi format has image tags in the form:
// <img recindex="0000n" alt=""/>
// where recindex is the record number of pdb record
// that holds the image (within image record array, not a
// global record)
void MobiFormatter::HandleTagImg(HtmlToken* t) {
    // we allow formatting raw html which can't require doc
    if (!doc) {
        return;
    }
    bool needAlt = true;
    AttrInfo* attr = t->GetAttrByName(StrL("recindex"));
    if (attr) {
        int n;
        if (!str::IsNull(str::Parse(attr->val, "%d", &n))) {
            Str img = doc->GetImage(n);
            needAlt = !img || !EmitImage(img);
        }
    }
    if (needAlt && (attr = t->GetAttrByName(StrL("alt"))) != nullptr) {
        HandleText(str::Dup(textAllocator, attr->val));
    }
}

void MobiFormatter::HandleHtmlTag(HtmlToken* t) {
    ReportIf(!t->IsTag());

    if (Tag_P == t->tag || Tag_Blockquote == t->tag) {
        HtmlFormatter::HandleHtmlTag(t);
        HandleSpacing_Mobi(t);
    } else if (Tag_Mbp_Pagebreak == t->tag) {
        ForceNewPage();
    } else if (Tag_A == t->tag) {
        HandleAnchorAttr(t);
        // handle internal and external links (prefer internal ones)
        if (!HandleTagA(t, "filepos")) {
            HandleTagA(t);
        }
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

EpubFormatter::~EpubFormatter() {
    str::Free(pagePath);
}

void EpubFormatter::HandleTagImg(HtmlToken* t) {
    ReportIf(!epubDoc);
    if (t->IsEndTag()) {
        return;
    }
    bool needAlt = true;
    AttrInfo* attr = t->GetAttrByName(StrL("src"));
    if (attr) {
        TempStr src = str::DupTemp(attr->val);
        url::DecodeInPlace(src);
        Str img = epubDoc->GetImageData(src, pagePath);
        needAlt = !img || !EmitImage(img);
    }
    if (needAlt && (attr = t->GetAttrByName(StrL("alt"))) != nullptr) {
        HandleText(str::Dup(textAllocator, attr->val));
    }
}

void EpubFormatter::HandleTagPagebreak(HtmlToken* t) {
    AttrInfo* attr = t->GetAttrByName(StrL("page_path"));
    if (!attr || len(pagePath) > 0) {
        ForceNewPage();
    }
    if (attr) {
        Gdiplus::RectF bbox(0, currY, pageDx, 0);
        // attr->val is owned by the gumbo parse tree which doesn't outlive
        // the formatter, so copy it into textAllocator
        currPage->instructions.Append(DrawInstr::PageMarkerAnchor(str::Dup(textAllocator, attr->val), bbox));
        str::ReplaceWithCopy(&pagePath, attr->val);
        // reset CSS style rules for the new document
        styleRules.Reset();
    }
}

void EpubFormatter::HandleTagLink(HtmlToken* t) {
    ReportIf(!epubDoc);
    if (t->IsEndTag()) {
        return;
    }
    AttrInfo* attr = t->GetAttrByName(StrL("rel"));
    if (!attr || !attr->ValIs("stylesheet")) {
        return;
    }
    attr = t->GetAttrByName(StrL("type"));
    if (attr && !attr->ValIs("text/css")) {
        return;
    }
    attr = t->GetAttrByName(StrL("href"));
    if (!attr) {
        return;
    }

    TempStr src = str::DupTemp(attr->val);
    url::DecodeInPlace(src);
    Str data = epubDoc->GetFileData(src, pagePath);
    if (data) {
        ParseStyleSheet(data);
        str::Free(data);
    }
}

void EpubFormatter::HandleTagSvgImage(HtmlToken* t) {
    ReportIf(!epubDoc);
    if (t->IsEndTag()) {
        return;
    }
    if (!tagNesting.Contains(Tag_Svg) && Tag_Svg_Image != t->tag) {
        return;
    }
    AttrInfo* attr = t->GetAttrByNameNS(StrL("href"), StrL("http://www.w3.org/1999/xlink"));
    if (!attr) {
        return;
    }
    TempStr src = str::DupTemp(attr->val);
    url::DecodeInPlace(src);
    Str img = epubDoc->GetImageData(src, pagePath);
    if (img) {
        EmitImage(img);
    }
}

void EpubFormatter::HandleHtmlTag(HtmlToken* t) {
    ReportIf(!t->IsTag());
    if (hiddenDepth && t->IsEndTag() && len(tagNesting) == hiddenDepth && t->tag == tagNesting.Last()) {
        hiddenDepth = 0;
        UpdateTagNesting(t);
        return;
    }
    if (0 == hiddenDepth && t->IsStartTag() && t->GetAttrByName(StrL("hidden"))) {
        hiddenDepth = len(tagNesting) + 1;
    }
    if (hiddenDepth > 0) {
        UpdateTagNesting(t);
    } else if (Tag_Image == t->tag || Tag_Svg_Image == t->tag) {
        HandleTagSvgImage(t);
    } else {
        HtmlFormatter::HandleHtmlTag(t);
    }
}

bool EpubFormatter::IgnoreText() {
    return hiddenDepth > 0 || HtmlFormatter::IgnoreText();
}

/* FictionBook-specific formatting methods */

Fb2Formatter::Fb2Formatter(HtmlFormatterArgs* args, Fb2Doc* doc)
    : HtmlFormatter(args), section(1), fb2Doc(doc), titleCount(0) {
    if (args->reparseIdx != 0) {
        return;
    }
    Str cover = doc->GetCoverImage();
    if (!cover) {
        return;
    }
    EmitImage(cover);
    // render larger images alone on the cover page,
    // smaller images just separated by a horizontal line
    if (0 == len(currLineInstr)) {
        /* the image was broken */;
    } else if (currLineInstr.Last().bbox.dy > args->pageDy / 2) {
        ForceNewPage();
    } else {
        EmitHr();
    }
}

void Fb2Formatter::HandleTagImg(HtmlToken* t) {
    ReportIf(!fb2Doc);
    if (t->IsEndTag()) {
        return;
    }
    Str img;
    AttrInfo* attr = t->GetAttrByNameNS(StrL("href"), StrL("http://www.w3.org/1999/xlink"));
    if (attr) {
        TempStr src = str::DupTemp(attr->val);
        url::DecodeInPlace(src);
        img = fb2Doc->GetImageData(src);
    }
    if (img) {
        EmitImage(img);
    }
}

void Fb2Formatter::HandleTagAsHtml(HtmlToken* t, Str name) {
    HtmlToken tok;
    tok.SetTag(t->type, name);
    HtmlFormatter::HandleHtmlTag(&tok);
}

// the name doesn't quite fit: this handles FB2 tags
void Fb2Formatter::HandleHtmlTag(HtmlToken* t) {
    if (Tag_Title == t->tag || Tag_Subtitle == t->tag) {
        bool isSubtitle = Tag_Subtitle == t->tag;
        TempStr name = fmt("h%d", section + (isSubtitle ? 1 : 0));
        HtmlToken tok;
        tok.SetTag(t->type, name);
        HandleTagHx(&tok);
        HandleAnchorAttr(t);
        if (!isSubtitle && t->IsStartTag()) {
            // the anchor must outlive the formatter, so not a TempStr
            Str link = str::Dup(textAllocator, fmt(FB2_TOC_ENTRY_MARK "%d", ++titleCount));
            currPage->instructions.Append(DrawInstr::Anchor(link, Gdiplus::RectF(0, currY, pageDx, 0)));
        }
    } else if (Tag_Section == t->tag) {
        if (t->IsStartTag()) {
            section++;
        } else if (t->IsEndTag() && section > 1) {
            section--;
        }
        FlushCurrLine(true);
        HandleAnchorAttr(t);
    } else if (Tag_P == t->tag) {
        if (!tagNesting.Contains(Tag_Title)) {
            HtmlFormatter::HandleHtmlTag(t);
        }
    } else if (Tag_Image == t->tag) {
        HandleTagImg(t);
        HandleAnchorAttr(t);
    } else if (Tag_A == t->tag) {
        HandleTagA(t, "href", "http://www.w3.org/1999/xlink");
        HandleAnchorAttr(t, true);
    } else if (Tag_Pagebreak == t->tag) {
        ForceNewPage();
    } else if (Tag_Strong == t->tag) {
        HandleTagAsHtml(t, "b");
    } else if (t->NameIs(StrL("emphasis"))) {
        HandleTagAsHtml(t, "i");
    } else if (t->NameIs(StrL("epigraph"))) {
        HandleTagAsHtml(t, "blockquote");
    } else if (t->NameIs(StrL("empty-line"))) {
        if (!t->IsEndTag()) {
            EmitParagraph(0);
        }
    } else if (t->NameIs(StrL("stylesheet"))) {
        HandleTagAsHtml(t, "style");
    }
}

/* standalone HTML-specific formatting methods */

void HtmlFileFormatter::HandleTagImg(HtmlToken* t) {
    ReportIf(!htmlDoc);
    if (t->IsEndTag()) {
        return;
    }
    bool needAlt = true;
    AttrInfo* attr = t->GetAttrByName(StrL("src"));
    if (attr) {
        TempStr src = str::DupTemp(attr->val);
        url::DecodeInPlace(src);
        Str img = htmlDoc->GetImageData(src);
        needAlt = !img || !EmitImage(img);
    }
    if (needAlt && (attr = t->GetAttrByName(StrL("alt"))) != nullptr) {
        HandleText(str::Dup(textAllocator, attr->val));
    }
}

void HtmlFileFormatter::HandleTagLink(HtmlToken* t) {
    ReportIf(!htmlDoc);
    if (t->IsEndTag()) {
        return;
    }
    AttrInfo* attr = t->GetAttrByName(StrL("rel"));
    if (!attr || !attr->ValIs("stylesheet")) {
        return;
    }
    attr = t->GetAttrByName(StrL("type"));
    if (attr && !attr->ValIs("text/css")) {
        return;
    }
    attr = t->GetAttrByName(StrL("href"));
    if (!attr) {
        return;
    }

    TempStr src = str::DupTemp(attr->val);
    url::DecodeInPlace(src);
    Str data = htmlDoc->GetFileData(src);
    if (data) {
        ParseStyleSheet(data);
    }
    str::Free(data);
}
