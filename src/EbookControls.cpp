/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "BitManip.h"
#include "StrSlice.h"
#include "WinUtil.h"
#include "Timer.h"
#include "HtmlParserLookup.h"
#include "EbookBase.h"
#include "Mui.h"
#include "BaseEngine.h"
#include "EbookControls.h"
#include "SettingsStructs.h"
#include "Controller.h"
#include "AppPrefs.h"
#include "HtmlFormatter.h"
#include "SerializeTxt.h"
#include "MuiEbookPageDef.h"
#include "PagesLayoutDef.h"
#include "resource.h"
#include "TrivialHtmlParser.h"
#include "TxtParser.h"
#define NOLOG 1
#include "DebugLog.h"

PageControl::PageControl() : page(NULL), cursorX(-1), cursorY(-1)
{
    bit::Set(wantedInputBits, WantsMouseMoveBit, WantsMouseClickBit);
}

PageControl::~PageControl()
{
    if (toolTip) {
        // TODO: make Control's destructor clear the tooltip?
        Control::NotifyMouseLeave();
    }
}

void PageControl::SetPage(HtmlPage *newPage)
{
    page = newPage;
    RequestRepaint(this);
}

DrawInstr *PageControl::GetLinkAt(int x, int y) const
{
    if (!page)
        return NULL;

    PointF pt((REAL)(x - cachedStyle->padding.left), (REAL)(y - cachedStyle->padding.top));
    for (DrawInstr& i : page->instructions) {
        if (InstrLinkStart == i.type && !i.bbox.IsEmptyArea() && i.bbox.Contains(pt)) {
            return &i;
        }
    }
    return NULL;
}

void PageControl::NotifyMouseMove(int x, int y)
{
    DrawInstr *link = GetLinkAt(x, y);
    if (!link) {
        SetCursor(IDC_ARROW);
        if (toolTip) {
            Control::NotifyMouseLeave();
            str::ReplacePtr(&toolTip, NULL);
        }
        return;
    }

    SetCursor(IDC_HAND);
    ScopedMem<WCHAR> url(str::conv::FromHtmlUtf8(link->str.s, link->str.len));
    if (toolTip && (!url::IsAbsolute(url) || !str::Eq(toolTip, url))) {
        Control::NotifyMouseLeave();
        str::ReplacePtr(&toolTip, NULL);
    }
    if (!toolTip && url::IsAbsolute(url)) {
        toolTip = url.StealData();
        Control::NotifyMouseEnter();
    }
}

// size of the drawable area i.e. size minus padding
Size PageControl::GetDrawableSize() const
{
    Size s;
    pos.GetSize(&s);
    Padding pad = cachedStyle->padding;
    s.Width  -= (pad.left + pad.right);
    s.Height -= (pad.top + pad.bottom);
    if ((s.Width <= 0) || (s.Height <= 0))
        return Size();
    return s;
}

void PageControl::Paint(Graphics *gfx, int offX, int offY)
{
    CrashIf(!IsVisible());

    Timer timerAll;

    CachedStyle *s = cachedStyle;
    Timer timerFill;
    Rect r(offX, offY, pos.Width, pos.Height);
    if (!s->bgColor->IsTransparent()) {
        Brush *br = BrushFromColorData(s->bgColor, r);
        gfx->FillRectangle(br, r);
    }
    double durFill = timerFill.Stop();

    if (!page)
        return;

    // during resize the page we currently show might be bigger than
    // our area. To avoid drawing outside our area we clip
    Region origClipRegion;
    gfx->GetClip(&origClipRegion);
    r.X += s->padding.left;
    r.Y += s->padding.top;
    r.Width  -= (s->padding.left + s->padding.right);
    r.Height -= (s->padding.top  + s->padding.bottom);
    r.Inflate(1,0);
    gfx->SetClip(r, CombineModeReplace);

    COLORREF txtCol, bgCol;
    GetEbookColors(&txtCol, &bgCol);
    Color textColor, bgColor;
    textColor.SetFromCOLORREF(txtCol);

    ITextRender *textRender = CreateTextRender(GetTextRenderMethod(), gfx, pos.Width, pos.Height);
    //ITextRender *textRender = CreateTextRender(TextRenderMethodHdc, gfx, pos.Width, pos.Height);

    bgColor.SetFromCOLORREF(bgCol);
    textRender->SetTextBgColor(bgColor);

    Timer timerDrawHtml;
    DrawHtmlPage(gfx, textRender, &page->instructions, (REAL)r.X, (REAL)r.Y, IsDebugPaint(), textColor);
    double durDraw = timerDrawHtml.Stop();
    gfx->SetClip(&origClipRegion, CombineModeReplace);
    delete textRender;

    double durAll = timerAll.Stop();
    plogf("all: %.2f, fill: %.2f, draw html: %.2f", durAll, durFill, durDraw);
}

Control *CreatePageControl(TxtNode *structDef)
{
    CrashIf(!structDef->IsStructWithName("EbookPage"));
    EbookPageDef *def = DeserializeEbookPageDef(structDef);
    PageControl *c = new PageControl();
    Style *style = StyleByName(def->style);
    c->SetStyle(style);

    if (def->name)
        c->SetName(def->name);

    FreeEbookPageDef(def);
    return c;
}

ILayout *CreatePagesLayout(ParsedMui *parsedMui, TxtNode *structDef)
{
    CrashIf(!structDef->IsStructWithName("PagesLayout"));
    PagesLayoutDef *def = DeserializePagesLayoutDef(structDef);
    CrashIf(!def->page1 || !def->page2);
    PageControl *page1 = static_cast<PageControl*>(FindControlNamed(*parsedMui, def->page1));
    PageControl *page2 = static_cast<PageControl*>(FindControlNamed(*parsedMui, def->page2));
    CrashIf(!page1 || !page2);
    PagesLayout *layout = new PagesLayout(page1, page2, def->spaceDx);
    if (def->name)
        layout->SetName(def->name);
    FreePagesLayoutDef(def);
    return layout;
}

void SetMainWndBgCol(EbookControls *ctrls)
{
    COLORREF bgColor;
    GetEbookColors(NULL, &bgColor);

    Style *styleMainWnd = StyleByName("styleMainWnd");
    CrashIf(!styleMainWnd);
    styleMainWnd->Set(Prop::AllocColorSolid(PropBgColor, GetRValueSafe(bgColor), GetGValueSafe(bgColor), GetBValueSafe(bgColor)));
    ctrls->mainWnd->SetStyle(styleMainWnd);

    Style *styleStatus = StyleByName("styleStatus");
    styleStatus->Set(Prop::AllocColorSolid(PropBgColor, GetRValueSafe(bgColor), GetGValueSafe(bgColor), GetBValueSafe(bgColor)));
    ctrls->status->SetStyle(styleStatus);

    // TODO: should also allow to change text color
    // TODO: also match the colors of progress bar to be based on background color
    // TODO: update tab color

    // note: callers are expected to update the background of tree control and 
    // other colors that are supposed to match background color
}

EbookControls *CreateEbookControls(HWND hwnd, FrameRateWnd *frameRateWnd)
{
    static bool wasRegistered = false;
    if (!wasRegistered) {
        RegisterControlCreatorFor("EbookPage", &CreatePageControl);
        RegisterLayoutCreatorFor("PagesLayout", &CreatePagesLayout);
        wasRegistered = true;
    }

    ParsedMui *muiDef = new ParsedMui();
    char *s = LoadTextResource(IDD_EBOOK_WIN_DESC);
    MuiFromText(s, *muiDef);
    free(s);

    EbookControls *ctrls = new EbookControls;
    ctrls->muiDef = muiDef;
    CrashIf(!FindButtonVectorNamed(*muiDef, "nextButton"));
    CrashIf(!FindButtonVectorNamed(*muiDef, "prevButton"));
    ctrls->status = FindButtonNamed(*muiDef, "statusButton");
    CrashIf(!ctrls->status);
    ctrls->progress = FindScrollBarNamed(*muiDef, "progressScrollBar");
    CrashIf(!ctrls->progress);
    ctrls->progress->hCursor = GetCursor(IDC_HAND);

    ctrls->topPart = FindLayoutNamed(*muiDef, "top");
    CrashIf(!ctrls->topPart);
    ctrls->pagesLayout = static_cast<PagesLayout*>(FindLayoutNamed(*muiDef, "pagesLayout"));
    CrashIf(!ctrls->pagesLayout);

    ctrls->mainWnd = new HwndWrapper(hwnd);
    ctrls->mainWnd->frameRateWnd = frameRateWnd;
    ctrls->mainWnd->SetMinSize(Size(320, 200));

    SetMainWndBgCol(ctrls);
    ctrls->mainWnd->layout = FindLayoutNamed(*muiDef, "mainLayout");
    CrashIf(!ctrls->mainWnd->layout);

    for (size_t i = 0; i < muiDef->allControls.Count(); i++) {
        Control *c = muiDef->allControls.At(i);
        ctrls->mainWnd->AddChild(c);
    }
    return ctrls;
}

void DestroyEbookControls(EbookControls* ctrls)
{
    delete ctrls->mainWnd;
    delete ctrls->topPart;
    delete ctrls->pagesLayout;
    delete ctrls->muiDef;
    delete ctrls;
}

Size PagesLayout::Measure(const Size availableSize)
{
    desiredSize = availableSize;
    return desiredSize;
}

void PagesLayout::Arrange(const Rect finalRect)
{
    // only page2 can be hidden
    CrashIf(!page1->IsVisible());

    // if only page1 visible, give it the whole area
    if (!page2->IsVisible()) {
        page1->Arrange(finalRect);
        return;
    }

    // when both visible, give them equally sized areas
    // with spaceDx between them
    int dx = finalRect.Width;
    if (page2->IsVisible()) {
        dx = (dx / 2) - spaceDx;
        // protect against excessive spaceDx values
        if (dx <= 100) {
            spaceDx = 0;
            dx = dx /2;
            CrashIf(dx < 10);
        }
    }
    Rect r = finalRect;
    r.Width = dx;
    page1->Arrange(r);
    r.X = r.X + dx + spaceDx;
    page2->Arrange(r);
}

void GetEbookColors(COLORREF* txtColOut, COLORREF* bgColOut)
{
    if (txtColOut) {
        if (gGlobalPrefs->useSysColors)
            *txtColOut = GetSysColor(COLOR_WINDOWTEXT);
        else
            *txtColOut = gGlobalPrefs->ebookUI.textColor;
    }

    if (bgColOut) {
        if (gGlobalPrefs->useSysColors)
            *bgColOut = GetSysColor(COLOR_WINDOW);
        else
            *bgColOut = gGlobalPrefs->ebookUI.backgroundColor;
    }
}
