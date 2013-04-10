/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "EbookControls.h"
#include "BitManip.h"
#include "HtmlFormatter.h"
#include "MuiEbookPageDef.h"
#include "resource.h"
#include "WinUtil.h"

#include "DebugLog.h"

static Style *      styleMainWnd = NULL;
static HCURSOR      gCursorHand = NULL;

static ParsedMui    ebookMuiDef;

#if 0
static Rect RectForCircle(int x, int y, int r)
{
    return Rect(x - r, y - r, r * 2, r * 2);
}
#endif

PageControl::PageControl() : page(NULL), cursorX(-1), cursorY(-1)
{
    bit::Set(wantedInputBits, WantsMouseMoveBit);
}

void PageControl::SetPage(HtmlPage *newPage)
{
    page = newPage;
    RequestRepaint(this);
}

// This is just to test mouse move handling
void PageControl::NotifyMouseMove(int x, int y)
{
#if 0
    Rect r1 = RectForCircle(cursorX, cursorY, 10);
    Rect r2 = RectForCircle(x, y, 10);
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
    CrashIf(!IsVisible());

#if 0
    // for testing mouse move, paint a blue circle at current cursor position
    if ((-1 != cursorX) && (-1 != cursorY)) {
        SolidBrush br(Color(180, 0, 0, 255));
        int x = offX + cursorX;
        int y = offY + cursorY;
        Rect r(RectForCircle(x, y, 10));
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

    // TODO: support changing the text color to gRenderCache.colorRange[0]
    //       or GetSysColor(COLOR_WINDOWTEXT) if gGlobalPrefs.useSysColors
    DrawHtmlPage(gfx, &page->instructions, (REAL)r.X, (REAL)r.Y, IsDebugPaint());
    gfx->SetClip(&origClipRegion, CombineModeReplace);
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

static void CreateEbookStyles()
{
    CrashIf(styleMainWnd); // only call me once

    // TODO: support changing this color to gRenderCache.colorRange[1]
    //       or GetSysColor(COLOR_WINDOW) if gGlobalPrefs.useSysColors

    styleMainWnd = StyleByName("styleMainWnd");
    CrashIf(!styleMainWnd);
}

static void CreateLayout(EbookControls *ctrls)
{
    ctrls->topPart = new HorizontalLayout();
    DirectionalLayoutData ld;
    ld.Set(ctrls->prev, SizeSelf, 1.f, GetElAlignCenter());
    ctrls->topPart->Add(ld);
    ld.Set(ctrls->page, 1.f, 1.f, GetElAlignTop());
    ctrls->topPart->Add(ld);
    ld.Set(ctrls->next, SizeSelf, 1.f, GetElAlignBottom());
    ctrls->topPart->Add(ld);

    VerticalLayout *l = new VerticalLayout();
    ld.Set(ctrls->topPart, 1.f, 1.f, GetElAlignTop());
    l->Add(ld);
    ld.Set(ctrls->progress, SizeSelf, 1.f, GetElAlignCenter());
    l->Add(ld);
    ld.Set(ctrls->status, SizeSelf, 1.f, GetElAlignCenter());
    l->Add(ld);
    ctrls->mainWnd->layout = l;
}

static void CreateControls(EbookControls *ctrls, ParsedMui& muiInfo)
{
    ctrls->next = FindButtonVectorNamed(ebookMuiDef, "nextButton");
    CrashIf(!ctrls->next);
    ctrls->prev = FindButtonVectorNamed(ebookMuiDef, "prevButton");
    CrashIf(!ctrls->prev);
    ctrls->status = FindButtonNamed(ebookMuiDef, "statusButton");
    CrashIf(!ctrls->status);
    ctrls->progress = FindScrollBarNamed(ebookMuiDef, "progressScrollBar");
    CrashIf(!ctrls->progress);
    ctrls->progress->hCursor = gCursorHand;
    ctrls->page = (PageControl*)FindControlNamed(ebookMuiDef, "page");
    CrashIf(!ctrls->page);
}

EbookControls *CreateEbookControls(HWND hwnd)
{
    if (!gCursorHand) {
        RegisterControlCreatorFor("EbookPage", &CreatePageControl);
        gCursorHand  = LoadCursor(NULL, IDC_HAND);
        char *s = LoadTextResource(IDD_EBOOK_WIN_DESC);
        MuiFromText(s, ebookMuiDef);
        free(s);
        CreateEbookStyles();
    }

    EbookControls *ctrls = new EbookControls;

    CreateControls(ctrls, ebookMuiDef);

    ctrls->mainWnd = new HwndWrapper(hwnd);
    ctrls->mainWnd->SetMinSize(Size(320, 200));
    ctrls->mainWnd->SetStyle(styleMainWnd);

    for (size_t i = 0; i < ebookMuiDef.allControls.Count(); i++) {
        Control *c = ebookMuiDef.allControls.At(i);
        ctrls->mainWnd->AddChild(c);
    }
    CreateLayout(ctrls);
    return ctrls;
}

void DestroyEbookControls(EbookControls* ctrls)
{
    delete ctrls->mainWnd;
    delete ctrls->topPart;
    delete ctrls;
}
