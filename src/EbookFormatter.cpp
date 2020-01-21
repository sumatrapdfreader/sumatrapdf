/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/GdiPlusUtil.h"
#include "utils/WinUtil.h"

#include "utils/Archive.h"
#include "utils/HtmlParserLookup.h"
#include "utils/HtmlPullParser.h"
#include "mui/Mui.h"

#include "wingui/TreeModel.h"
#include "EngineBase.h"
#include "EbookBase.h"
#include "EbookDoc.h"
#include "MobiDoc.h"
#include "HtmlFormatter.h"
#include "EbookFormatter.h"

/* Mobi-specific formatting methods */

MobiFormatter::MobiFormatter(HtmlFormatterArgs* args, MobiDoc* doc) : HtmlFormatter(args), doc(doc) {
    bool fromBeginning = (0 == args->reparseIdx);
    if (!doc || !fromBeginning)
        return;

    ImageData* img = doc->GetCoverImage();
    if (!img)
        return;

    // TODO: vertically center the cover image?
    EmitImage(img);
    // only add a new page if the image isn't broken
    if (currLineInstr.size() > 0)
        ForceNewPage();
}

// parses size in the form "1em" or "3pt". To interpret ems we need emInPoints
// to be passed by the caller
static float ParseSizeAsPixels(const char* s, size_t len, float emInPoints) {
    float sizeInPoints = 0;
    if (str::Parse(s, len, "%fem", &sizeInPoints)) {
        sizeInPoints *= emInPoints;
    } else if (str::Parse(s, len, "%fin", &sizeInPoints)) {
        sizeInPoints *= 72;
    } else if (str::Parse(s, len, "%fpt", &sizeInPoints)) {
        // no conversion needed
    } else if (str::Parse(s, len, "%fpx", &sizeInPoints)) {
        return sizeInPoints;
    } else {
        return 0;
    }
    // TODO: take dpi into account
    float sizeInPixels = sizeInPoints;
    return sizeInPixels;
}

void MobiFormatter::HandleSpacing_Mobi(HtmlToken* t) {
    if (!t->IsStartTag())
        return;

    // best I can tell, in mobi <p width="1em" height="3pt> means that
    // the first line of the paragrap is indented by 1em and there's
    // 3pt top padding (the same seems to apply for <blockquote>)
    AttrInfo* attr = t->GetAttrByName("width");
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
void MobiFormatter::HandleTagImg(HtmlToken* t) {
    // we allow formatting raw html which can't require doc
    if (!doc)
        return;
    bool needAlt = true;
    AttrInfo* attr = t->GetAttrByName("recindex");
    if (attr) {
        int n;
        if (str::Parse(attr->val, attr->valLen, "%d", &n)) {
            ImageData* img = doc->GetImage(n);
            needAlt = !img || !EmitImage(img);
        }
    }
    if (needAlt && (attr = t->GetAttrByName("alt")) != nullptr)
        HandleText(attr->val, attr->valLen);
}

void MobiFormatter::HandleHtmlTag(HtmlToken* t) {
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

void EpubFormatter::HandleTagImg(HtmlToken* t) {
    CrashIf(!epubDoc);
    if (t->IsEndTag())
        return;
    bool needAlt = true;
    AttrInfo* attr = t->GetAttrByName("src");
    if (attr) {
        AutoFree src(str::DupN(attr->val, attr->valLen));
        url::DecodeInPlace(src);
        ImageData* img = epubDoc->GetImageData(src, pagePath);
        needAlt = !img || !EmitImage(img);
    }
    if (needAlt && (attr = t->GetAttrByName("alt")) != nullptr)
        HandleText(attr->val, attr->valLen);
}

void EpubFormatter::HandleTagPagebreak(HtmlToken* t) {
    AttrInfo* attr = t->GetAttrByName("page_path");
    if (!attr || pagePath)
        ForceNewPage();
    if (attr) {
        RectF bbox(0, currY, pageDx, 0);
        currPage->instructions.Append(DrawInstr::Anchor(attr->val, attr->valLen, bbox));
        pagePath.Set(str::DupN(attr->val, attr->valLen));
        // reset CSS style rules for the new document
        styleRules.Reset();
    }
}

void EpubFormatter::HandleTagLink(HtmlToken* t) {
    CrashIf(!epubDoc);
    if (t->IsEndTag())
        return;
    AttrInfo* attr = t->GetAttrByName("rel");
    if (!attr || !attr->ValIs("stylesheet"))
        return;
    attr = t->GetAttrByName("type");
    if (attr && !attr->ValIs("text/css"))
        return;
    attr = t->GetAttrByName("href");
    if (!attr)
        return;

    AutoFree src(str::DupN(attr->val, attr->valLen));
    url::DecodeInPlace(src);
    AutoFree data(epubDoc->GetFileData(src, pagePath));
    if (data.data) {
        ParseStyleSheet(data.data, data.size());
    }
}

void EpubFormatter::HandleTagSvgImage(HtmlToken* t) {
    CrashIf(!epubDoc);
    if (t->IsEndTag()) {
        return;
    }
    if (!tagNesting.Contains(Tag_Svg) && Tag_Svg_Image != t->tag) {
        return;
    }
    AttrInfo* attr = t->GetAttrByNameNS("href", "http://www.w3.org/1999/xlink");
    if (!attr) {
        return;
    }
    AutoFree src(str::DupN(attr->val, attr->valLen));
    url::DecodeInPlace(src);
    ImageData* img = epubDoc->GetImageData(src, pagePath);
    if (img) {
        EmitImage(img);
    }
}

void EpubFormatter::HandleHtmlTag(HtmlToken* t) {
    CrashIf(!t->IsTag());
    if (hiddenDepth && t->IsEndTag() && tagNesting.size() == hiddenDepth && t->tag == tagNesting.Last()) {
        hiddenDepth = 0;
        UpdateTagNesting(t);
        return;
    }
    if (0 == hiddenDepth && t->IsStartTag() && t->GetAttrByName("hidden"))
        hiddenDepth = tagNesting.size() + 1;
    if (hiddenDepth > 0)
        UpdateTagNesting(t);
    else if (Tag_Image == t->tag || Tag_Svg_Image == t->tag)
        HandleTagSvgImage(t);
    else
        HtmlFormatter::HandleHtmlTag(t);
}

bool EpubFormatter::IgnoreText() {
    return hiddenDepth > 0 || HtmlFormatter::IgnoreText();
}

/* FictionBook-specific formatting methods */

Fb2Formatter::Fb2Formatter(HtmlFormatterArgs* args, Fb2Doc* doc)
    : HtmlFormatter(args), fb2Doc(doc), section(1), titleCount(0) {
    if (args->reparseIdx != 0)
        return;
    ImageData* cover = doc->GetCoverImage();
    if (!cover)
        return;
    EmitImage(cover);
    // render larger images alone on the cover page,
    // smaller images just separated by a horizontal line
    if (0 == currLineInstr.size())
        /* the image was broken */;
    else if (currLineInstr.Last().bbox.Height > args->pageDy / 2)
        ForceNewPage();
    else
        EmitHr();
}

void Fb2Formatter::HandleTagImg(HtmlToken* t) {
    CrashIf(!fb2Doc);
    if (t->IsEndTag())
        return;
    ImageData* img = nullptr;
    AttrInfo* attr = t->GetAttrByNameNS("href", "http://www.w3.org/1999/xlink");
    if (attr) {
        AutoFree src(str::DupN(attr->val, attr->valLen));
        url::DecodeInPlace(src);
        img = fb2Doc->GetImageData(src);
    }
    if (img)
        EmitImage(img);
}

void Fb2Formatter::HandleTagAsHtml(HtmlToken* t, const char* name) {
    HtmlToken tok;
    tok.SetTag(t->type, name, name + str::Len(name));
    HtmlFormatter::HandleHtmlTag(&tok);
}

// the name doesn't quite fit: this handles FB2 tags
void Fb2Formatter::HandleHtmlTag(HtmlToken* t) {
    if (Tag_Title == t->tag || Tag_Subtitle == t->tag) {
        bool isSubtitle = Tag_Subtitle == t->tag;
        AutoFree name(str::Format("h%d", section + (isSubtitle ? 1 : 0)));
        HtmlToken tok;
        tok.SetTag(t->type, name, name + str::Len(name));
        HandleTagHx(&tok);
        HandleAnchorAttr(t);
        if (!isSubtitle && t->IsStartTag()) {
            char* link = (char*)Allocator::Alloc(textAllocator, 24);
            sprintf_s(link, 24, FB2_TOC_ENTRY_MARK "%d", ++titleCount);
            currPage->instructions.Append(DrawInstr::Anchor(link, str::Len(link), RectF(0, currY, pageDx, 0)));
        }
    } else if (Tag_Section == t->tag) {
        if (t->IsStartTag())
            section++;
        else if (t->IsEndTag() && section > 1)
            section--;
        FlushCurrLine(true);
        HandleAnchorAttr(t);
    } else if (Tag_P == t->tag) {
        if (!tagNesting.Contains(Tag_Title))
            HtmlFormatter::HandleHtmlTag(t);
    } else if (Tag_Image == t->tag) {
        HandleTagImg(t);
        HandleAnchorAttr(t);
    } else if (Tag_A == t->tag) {
        HandleTagA(t, "href", "http://www.w3.org/1999/xlink");
        HandleAnchorAttr(t, true);
    } else if (Tag_Pagebreak == t->tag)
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
    } else if (t->NameIs("stylesheet"))
        HandleTagAsHtml(t, "style");
}

/* standalone HTML-specific formatting methods */

void HtmlFileFormatter::HandleTagImg(HtmlToken* t) {
    CrashIf(!htmlDoc);
    if (t->IsEndTag())
        return;
    bool needAlt = true;
    AttrInfo* attr = t->GetAttrByName("src");
    if (attr) {
        AutoFree src(str::DupN(attr->val, attr->valLen));
        url::DecodeInPlace(src);
        ImageData* img = htmlDoc->GetImageData(src);
        needAlt = !img || !EmitImage(img);
    }
    if (needAlt && (attr = t->GetAttrByName("alt")) != nullptr)
        HandleText(attr->val, attr->valLen);
}

void HtmlFileFormatter::HandleTagLink(HtmlToken* t) {
    CrashIf(!htmlDoc);
    if (t->IsEndTag())
        return;
    AttrInfo* attr = t->GetAttrByName("rel");
    if (!attr || !attr->ValIs("stylesheet"))
        return;
    attr = t->GetAttrByName("type");
    if (attr && !attr->ValIs("text/css"))
        return;
    attr = t->GetAttrByName("href");
    if (!attr)
        return;

    AutoFree src(str::DupN(attr->val, attr->valLen));
    url::DecodeInPlace(src);
    AutoFree data(htmlDoc->GetFileData(src));
    if (data.data) {
        ParseStyleSheet(data.data, data.size());
    }
}
