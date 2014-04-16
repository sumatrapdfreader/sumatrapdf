/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "Mui.h"
#include "EbookControls.h"

#include "AppPrefs.h"
#include "BitManip.h"
#include "HtmlFormatter.h"
#include "EbookFormatter.h"
#include "MuiEbookPageDef.h"
#include "PagesLayoutDef.h"
#include "resource.h"
#include "TxtParser.h"
#include "WinUtil.h"

#include "DebugLog.h"

static HCURSOR      gCursorHand = NULL;

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

    Color textColor;
    if (gGlobalPrefs->useSysColors)
        textColor.SetFromCOLORREF(GetSysColor(COLOR_WINDOWTEXT));
    else
        textColor.SetFromCOLORREF(gGlobalPrefs->ebookUI.textColor);

    ITextRender *textRender = CreateTextRender(GetTextRenderMethod(), gfx);

    Color bgCol;
    bgCol.SetFromCOLORREF(gGlobalPrefs->ebookUI.backgroundColor);
    textRender->SetTextBgColor(bgCol);

    DrawHtmlPage(gfx, textRender, &page->instructions, (REAL)r.X, (REAL)r.Y, IsDebugPaint(), textColor);
    gfx->SetClip(&origClipRegion, CombineModeReplace);
    delete textRender;
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
    Style *styleMainWnd = StyleByName("styleMainWnd");
    CrashIf(!styleMainWnd);
    COLORREF bgColor = gGlobalPrefs->ebookUI.backgroundColor;
    if (gGlobalPrefs->useSysColors)
        bgColor = GetSysColor(COLOR_WINDOW);
    styleMainWnd->Set(Prop::AllocColorSolid(PropBgColor, GetRValueSafe(bgColor), GetGValueSafe(bgColor), GetBValueSafe(bgColor)));
    ctrls->mainWnd->SetStyle(styleMainWnd);
}

EbookControls *CreateEbookControls(HWND hwnd)
{
    if (!gCursorHand) {
        RegisterControlCreatorFor("EbookPage", &CreatePageControl);
        RegisterLayoutCreatorFor("PagesLayout", &CreatePagesLayout);
        gCursorHand  = LoadCursor(NULL, IDC_HAND);
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
    ctrls->progress->hCursor = gCursorHand;

    ctrls->topPart = FindLayoutNamed(*muiDef, "top");
    CrashIf(!ctrls->topPart);
    ctrls->pagesLayout = static_cast<PagesLayout*>(FindLayoutNamed(*muiDef, "pagesLayout"));
    CrashIf(!ctrls->pagesLayout);

    ctrls->mainWnd = new HwndWrapper(hwnd);
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
    int dx = desiredSize.Width;
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

