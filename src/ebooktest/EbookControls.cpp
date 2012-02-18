/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "EbookControls.h"
#include "PageLayout.h"
#include "SvgPath.h"

static Style *   ebookDefault = NULL;
static Style *   statusDefault = NULL;
static Style *   horizProgressDefault = NULL;
static Style *   nextDefault = NULL;
static Style *   nextMouseOver = NULL;
static HCURSOR   gCursorHand = NULL;

static Rect RectForCircle(int x, int y, int r)
{
    return Rect(x - r, y - r, r * 2, r * 2);
}

// This is just to test mouse move handling
void PageControl::NotifyMouseMove(int x, int y)
{
    Rect r1 = RectForCircle(cursorX, cursorY, CircleR);
    Rect r2 = RectForCircle(x, y, CircleR);
    cursorX = x; cursorY = y;
    r1.Inflate(1,1); r2.Inflate(1,1);
    RequestRepaint(this, &r1, &r2);
}

// size of the drawable area i.e. size minus padding
Size PageControl::GetDrawableSize()
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
    // for testing mouse move, paint a blue circle at current cursor position
    if ((-1 != cursorX) && (-1 != cursorY)) {
        SolidBrush br(Color(180, 0, 0, 255));
        int x = offX + cursorX;
        int y = offY + cursorY;
        Rect r(RectForCircle(x, y, CircleR));
        gfx->FillEllipse(&br, r);
    }

    if (!page)
        return;

    offX += cachedStyle->padding.left;
    offY += cachedStyle->padding.top;

    DrawPageLayout(gfx, &page->drawInstructions, (REAL)offX, (REAL)offY, IsDebugPaint());
}

static void CreateStyles()
{
    const int pageBorderX = 10;
    const int pageBorderY = 10;

    ebookDefault = new Style();
    ebookDefault->Set(Prop::AllocPadding(pageBorderY, pageBorderX, pageBorderY, pageBorderX));

    nextDefault = new Style(gStyleButtonDefault);
    nextDefault->SetBorderWidth(0.f);
    //nextDefault->Set(Prop::AllocPadding(1, 1, 1, 4));
    nextDefault->Set(Prop::AllocPadding(0, 4, 0, 4));
    nextDefault->Set(Prop::AllocWidth(PropStrokeWidth, 0.f));
    nextDefault->Set(Prop::AllocColorSolid(PropFill, "gray"));
    nextDefault->Set(Prop::AllocColorSolid(PropBgColor, "transparent"));
    nextDefault->Set(Prop::AllocAlign(PropVertAlign, ElAlignCenter));

    nextMouseOver = new Style(nextDefault);
    nextMouseOver->Set(Prop::AllocColorSolid(PropFill, "black"));
    nextMouseOver->Set(Prop::AllocColorSolid(PropBgColor, "#80FFFFFF"));

    statusDefault = new Style();
    statusDefault->Set(Prop::AllocColorSolid(PropBgColor, "white"));
    statusDefault->Set(Prop::AllocColorSolid(PropColor, "black"));
    statusDefault->Set(Prop::AllocFontSize(8));
    statusDefault->Set(Prop::AllocFontWeight(FontStyleRegular));
    statusDefault->Set(Prop::AllocPadding(2, 0, 2, 0));
    statusDefault->SetBorderWidth(0);
    statusDefault->Set(Prop::AllocTextAlign(Align_Center));

    horizProgressDefault = new Style();
    horizProgressDefault->Set(Prop::AllocColorSolid(PropBgColor, "transparent"));
    horizProgressDefault->Set(Prop::AllocColorSolid(PropColor, "yellow"));
}

static void DeleteStyles()
{
    delete statusDefault;
    delete nextDefault;
    delete nextMouseOver;
    delete ebookDefault;
    delete horizProgressDefault;
}

static void CreateLayout(EbookControls *ctrls)
{
    HorizontalLayout *topPart = new HorizontalLayout();
    DirectionalLayoutData ld;
    ld.Set(ctrls->prev, SizeSelf, 1.f, GetElAlignCenter());
    topPart->Add(ld);
    ld.Set(ctrls->page, 1.f, 1.f, GetElAlignTop());
    topPart->Add(ld);
    ld.Set(ctrls->next, SizeSelf, 1.f, GetElAlignBottom());
    topPart->Add(ld);

    VerticalLayout *l = new VerticalLayout();
    ld.Set(topPart, 1.f, 1.f, GetElAlignTop());
    l->Add(ld, true);
    ld.Set(ctrls->horizProgress, SizeSelf, 1.f, GetElAlignCenter());
    l->Add(ld);
    ld.Set(ctrls->status, SizeSelf, 1.f, GetElAlignCenter());
    l->Add(ld);
    ctrls->mainWnd->layout = l;
}

EbookControls *CreateEbookControls(HWND hwnd)
{
    CreateStyles();

    if (!gCursorHand)
        gCursorHand  = LoadCursor(NULL, IDC_HAND);

    EbookControls *ctrls = new EbookControls;

    ctrls->next = new ButtonVector(svg::GraphicsPathFromPathData("M0 0  L10 13 L0 ,26 Z"));
    ctrls->next->SetStyles(nextDefault, nextMouseOver);

    ctrls->prev = new ButtonVector(svg::GraphicsPathFromPathData("M10 0 L0,  13 L10 26 z"));
    ctrls->prev->SetStyles(nextDefault, nextMouseOver);

    ctrls->horizProgress = new ScrollBar();
    ctrls->horizProgress->hCursor = gCursorHand;
    ctrls->horizProgress->SetCurrentStyle(horizProgressDefault, gStyleDefault);

    ctrls->status = new Button(_T(""));
    ctrls->status->SetStyles(statusDefault, statusDefault);

    ctrls->page = new PageControl();
    ctrls->page->SetCurrentStyle(ebookDefault, gStyleDefault);

    ctrls->mainWnd = new HwndWrapper(hwnd);
    ctrls->mainWnd->SetMinSize(Size(320, 200));
    ctrls->mainWnd->SetMaxSize(Size(1024, 800));

    ctrls->mainWnd->AddChild(ctrls->next, ctrls->prev, ctrls->page);
    ctrls->mainWnd->AddChild(ctrls->horizProgress, ctrls->status);
    CreateLayout(ctrls);
    return ctrls;
}

void DestroyEbookControls(EbookControls* ctrls)
{
    DeleteStyles();
    delete ctrls->mainWnd;
    delete ctrls;
}

