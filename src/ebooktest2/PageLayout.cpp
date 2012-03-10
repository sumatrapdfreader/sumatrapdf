/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "PageLayout.h"

// reuse src/PageLayout as far as possible
#include "MiniMui.h"
#include "../PageLayout.cpp"

#define DEFAULT_FONT_NAME   L"Georgia"
#define DEFAULT_FONT_SIZE   12.f

class PageLayout2 : public PageLayout {
    void HandleTagImg2(HtmlToken *t);
    void HandleTagHeader(HtmlToken *t);
    void HandleHtmlTag2(HtmlToken *t);

    ImageData *EmitImage2(ImageData2 *img);

    bool IgnoreText();

public:
    PageLayout2(LayoutInfo *li);

    Vec<PageData*> *Layout();
};

void PageLayout2::HandleTagImg2(HtmlToken *t)
{
    if (!layoutInfo->mobiDoc)
        return;
    BaseEbookDoc *doc = (BaseEbookDoc *)layoutInfo->mobiDoc;

    AttrInfo *attr = t->GetAttrByName("src");
    if (!attr)
        attr = t->GetAttrByName("recindex");
    if (!attr)
        return;
    ScopedMem<char> src(str::DupN(attr->val, attr->valLen));
    ImageData2 *img = doc->GetImageData(src);
    if (img)
        EmitImage2(img);
}

void PageLayout2::HandleTagHeader(HtmlToken *t)
{
    if (t->IsEndTag()) {
        HandleTagP(t);
        currFontSize = defaultFontSize;
        ChangeFontStyle(FontStyleBold, false);
        currY += 10;
    }
    else {
        currJustification = Align_Left;
        HandleTagP(t);
        currFontSize = defaultFontSize * (1 + ('5' - t->s[1]) * 0.2f);
        ChangeFontStyle(FontStyleBold, true);
    }
}

void PageLayout2::HandleHtmlTag2(HtmlToken *t)
{
    HtmlTag tag = FindTag(t);
    switch (tag) {
    case Tag_Img:
        HandleTagImg2(t);
        break;
    case Tag_Pagebreak:
        ForceNewPage();
        break;
    case Tag_Ul: case Tag_Ol: case Tag_Dl:
        currJustification = Align_Left;
        break;
    case Tag_Li: case Tag_Dd: case Tag_Dt:
        FlushCurrLine(false);
        break;
    case Tag_Center:
        currJustification = Align_Center;
        break;
    case Tag_H1: case Tag_H2: case Tag_H3:
    case Tag_H4: case Tag_H5:
        HandleTagHeader(t);
        break;
    case Tag_Abbr: case Tag_Acronym: case Tag_Code:
    case Tag_Lh: case Tag_Link: case Tag_Meta:
    case Tag_Pre: case Tag_Style: case Tag_Title:
        // ignore instead of crashing in HandleHtmlTag
        break;
    default:
        HandleHtmlTag(t);
        break;
    }
}

ImageData *PageLayout2::EmitImage2(ImageData2 *img)
{
    ImageData img1;
    img1.data = img->data;
    img1.len = img->len;
    EmitImage(&img1);

    ImageData *result = &img1;
    return result; // only for comparison with coverImage!
}

PageLayout2::PageLayout2(LayoutInfo* li)
{
    CrashIf(currPage);
    finishedParsing = false;
    layoutInfo = li;
    pageDx = (REAL)layoutInfo->pageDx;
    pageDy = (REAL)layoutInfo->pageDy;
    textAllocator = layoutInfo->textAllocator;
    htmlParser = new HtmlPullParser(layoutInfo->htmlStr, layoutInfo->htmlStrLen);

    CrashIf(gfx);
    gfx = mui::AllocGraphicsForMeasureText();
    defaultFontName.Set(str::Dup(layoutInfo->fontName));
    defaultFontSize = layoutInfo->fontSize;
    SetCurrentFont(FontStyleRegular, defaultFontSize);

    coverImage = NULL;
    pageCount = 0;
    inLink = false;

    lineSpacing = currFont->GetHeight(gfx);
    spaceDx = currFontSize / 2.5f; // note: a heuristic
    float spaceDx2 = GetSpaceDx(gfx, currFont);
    if (spaceDx2 < spaceDx)
        spaceDx = spaceDx2;

    currJustification = Align_Justify;
    currX = 0; currY = 0;
    currPage = new PageData;
    currPage->reparsePoint = currReparsePoint;

    currLineTopPadding = 0;
    if (layoutInfo->mobiDoc) {
        BaseEbookDoc *doc = (BaseEbookDoc *)layoutInfo->mobiDoc;
        ImageData2 *img = doc->GetImageData((size_t)-1);
        if (img)
            coverImage = EmitImage2(img);
    }
}

bool PageLayout2::IgnoreText()
{
    // ignore the content of <head>, <style> and <title> tags
    return htmlParser->tagNesting.Find(Tag_Head) != -1 ||
           htmlParser->tagNesting.Find(Tag_Style) != -1 ||
           htmlParser->tagNesting.Find(Tag_Title) != -1;
}

Vec<PageData*> *PageLayout2::Layout()
{
    HtmlToken *t;
    while ((t = htmlParser->Next()) && !t->IsError()) {
        currReparsePoint = t->GetReparsePoint();
        if (t->IsTag())
            HandleHtmlTag2(t);
        else if (!IgnoreText())
            HandleText(t);
    }

    FlushCurrLine(true);
    pagesToSend.Append(currPage);
    currPage = NULL;

    // remove empty pages (same as PageLayout::IterNext)
    for (size_t i = 0; i < pagesToSend.Count(); i++) {
        if (IsEmptyPage(pagesToSend.At(i))) {
            delete pagesToSend.At(i);
            pagesToSend.RemoveAt(i--);
        }
    }

    Vec<PageData *> *result = new Vec<PageData *>(pagesToSend);
    pagesToSend.Reset();
    return result;
}

Vec<PageData*> *LayoutHtml2(LayoutInfo li, BaseEbookDoc *doc)
{
    li.mobiDoc = (MobiDoc *)doc; // hack to allow passing doc through PageLayout
    li.fontName = DEFAULT_FONT_NAME;
    li.fontSize = DEFAULT_FONT_SIZE * 96 / 72;

    return PageLayout2(&li).Layout();
}

void DrawPageLayout2(Graphics *g, PageData *data, PointF offset, bool showBbox)
{
    InitGraphicsMode(g);
    DrawPageLayout(g, &data->instructions, offset.X, offset.Y, showBbox);
}
