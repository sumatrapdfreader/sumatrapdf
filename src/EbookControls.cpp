/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "EbookControls.h"
#include "PageLayout.h"
#include "SvgPath.h"

static Style *   styleMainWnd = NULL;
static Style *   stylePage = NULL;
static Style *   styleStatus = NULL;
static Style *   styleProgress = NULL;
static Style *   styleBtnNextPrevDefault = NULL;
static Style *   styleBtnNextPrevMouseOver = NULL;
static HCURSOR   gCursorHand = NULL;

#define COLOR_SEPIA         "FBF0D9"
#define COLOR_LIGHT_BLUE    "64C7EF"
#define COLOR_LIGHT_GRAY    "F0F0F0"

static Rect RectForCircle(int x, int y, int r)
{
    return Rect(x - r, y - r, r * 2, r * 2);
}

// This is just to test mouse move handling
void PageControl::NotifyMouseMove(int x, int y)
{
#if 0
    Rect r1 = RectForCircle(cursorX, cursorY, CircleR);
    Rect r2 = RectForCircle(x, y, CircleR);
    cursorX = x; cursorY = y;
    r1.Inflate(1,1); r2.Inflate(1,1);
    RequestRepaint(this, &r1, &r2);
#endif
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
#if 0
    // for testing mouse move, paint a blue circle at current cursor position
    if ((-1 != cursorX) && (-1 != cursorY)) {
        SolidBrush br(Color(180, 0, 0, 255));
        int x = offX + cursorX;
        int y = offY + cursorY;
        Rect r(RectForCircle(x, y, CircleR));
        gfx->FillEllipse(&br, r);
    }
#endif

    CachedStyle *s = cachedStyle;
    Rect r(offX, offY, pos.Width, pos.Height);
    Brush *br = BrushFromColorData(s->bgColor, r);
    gfx->FillRectangle(br, r);

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

    DrawPageLayout(gfx, &page->instructions, (REAL)r.X, (REAL)r.Y, IsDebugPaint());
    gfx->SetClip(&origClipRegion, CombineModeReplace);
}

static void CreateEbookStyles()
{
    const int pageBorderX = 16;
    const int pageBorderY = 32;

    // only create styles once
    if (styleMainWnd)
        return;

    styleMainWnd = new Style();
    styleMainWnd->Set(Prop::AllocColorSolid(PropBgColor, COLOR_SEPIA));

    stylePage = new Style();
    stylePage->Set(Prop::AllocPadding(pageBorderY, pageBorderX, pageBorderY, pageBorderX));
    stylePage->Set(Prop::AllocColorSolid(PropBgColor, "transparent"));

    styleBtnNextPrevDefault = new Style(gStyleButtonDefault);
    styleBtnNextPrevDefault->SetBorderWidth(0.f);
    //styleBtnNextPrevDefault->Set(Prop::AllocPadding(1, 1, 1, 4));
    styleBtnNextPrevDefault->Set(Prop::AllocPadding(0, 8, 0, 8));
    styleBtnNextPrevDefault->Set(Prop::AllocWidth(PropStrokeWidth, 0.f));
    styleBtnNextPrevDefault->Set(Prop::AllocColorSolid(PropFill, "gray"));
    styleBtnNextPrevDefault->Set(Prop::AllocColorSolid(PropBgColor, "transparent"));
    styleBtnNextPrevDefault->Set(Prop::AllocAlign(PropVertAlign, ElAlignCenter));

    styleBtnNextPrevMouseOver = new Style(styleBtnNextPrevDefault);
    styleBtnNextPrevMouseOver->Set(Prop::AllocColorSolid(PropFill, "black"));

    styleStatus = new Style(gStyleButtonDefault);
    styleStatus->Set(Prop::AllocColorSolid(PropBgColor, COLOR_LIGHT_GRAY));
    styleStatus->Set(Prop::AllocColorSolid(PropColor, "black"));
    styleStatus->Set(Prop::AllocFontSize(8));
    styleStatus->Set(Prop::AllocFontWeight(FontStyleRegular));
    styleStatus->Set(Prop::AllocPadding(3, 0, 3, 0));
    styleStatus->SetBorderWidth(0);
    styleStatus->Set(Prop::AllocTextAlign(Align_Center));

    styleProgress = new Style();
    styleProgress->Set(Prop::AllocColorSolid(PropBgColor, COLOR_LIGHT_GRAY));
    styleProgress->Set(Prop::AllocColorSolid(PropColor, COLOR_LIGHT_BLUE));
}

// should only be called once at the end of the program
void DeleteEbookStyles()
{
    delete styleStatus;
    delete styleBtnNextPrevDefault;
    delete styleBtnNextPrevMouseOver;
    delete stylePage;
    delete styleProgress;
    delete styleMainWnd;
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
    ld.Set(ctrls->progress, SizeSelf, 1.f, GetElAlignCenter());
    l->Add(ld);
    ld.Set(ctrls->status, SizeSelf, 1.f, GetElAlignCenter());
    l->Add(ld);
    ctrls->mainWnd->layout = l;
}

EbookControls *CreateEbookControls(HWND hwnd)
{
    CreateEbookStyles();

    if (!gCursorHand)
        gCursorHand  = LoadCursor(NULL, IDC_HAND);

    EbookControls *ctrls = new EbookControls;

    ctrls->next = new ButtonVector(svg::GraphicsPathFromPathData("M0 0  L10 13 L0 ,26 Z"));
    ctrls->next->SetStyles(styleBtnNextPrevDefault, styleBtnNextPrevMouseOver);

    ctrls->prev = new ButtonVector(svg::GraphicsPathFromPathData("M10 0 L0,  13 L10 26 z"));
    ctrls->prev->SetStyles(styleBtnNextPrevDefault, styleBtnNextPrevMouseOver);

    ctrls->progress = new ScrollBar();
    ctrls->progress->hCursor = gCursorHand;
    ctrls->progress->SetStyle(styleProgress);

    ctrls->status = new Button(L"");
    ctrls->status->SetStyles(styleStatus, styleStatus);

    ctrls->page = new PageControl();
    ctrls->page->SetStyle(stylePage);

    ctrls->mainWnd = new HwndWrapper(hwnd);
    ctrls->mainWnd->SetMaxSize(Size(1024, 800));
    ctrls->mainWnd->SetStyle(styleMainWnd);

    ctrls->mainWnd->AddChild(ctrls->next, ctrls->prev, ctrls->page);
    ctrls->mainWnd->AddChild(ctrls->progress, ctrls->status);
    CreateLayout(ctrls);
    return ctrls;
}

void DestroyEbookControls(EbookControls* ctrls)
{
    delete ctrls->mainWnd;
    delete ctrls;
}

