/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "EbookControls.h"

#include "AppPrefs.h"
#include "BitManip.h"
#include "HtmlFormatter.h"
#include "resource.h"
#include "SquareTreeParser.h"
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
    DrawHtmlPage(gfx, &page->instructions, (REAL)r.X, (REAL)r.Y, IsDebugPaint(), textColor);
    gfx->SetClip(&origClipRegion, CombineModeReplace);
}

Control *CreatePageControl(const char *name, SquareTreeNode *data)
{
    CrashIf(!str::Eq(name, "EbookPage"));
    PageControl *c = new PageControl();

    Style *style = StyleByName(data->GetValue("style"));
    CrashIf(!style);
    c->SetStyle(style);

    if ((name = data->GetValue("name")) != NULL)
        c->SetName(name);

    return c;
}

EbookControls *CreateEbookControls(HWND hwnd)
{
    if (!gCursorHand) {
        // TODO: use gCursorHand from SumatraPDF.h
        gCursorHand  = LoadCursor(NULL, IDC_HAND);
    }

    EbookControls *ctrls = AllocStruct<EbookControls>();
    ctrls->mainWnd = new HwndWrapper(hwnd);
    ctrls->mainWnd->SetMinSize(Size(320, 200));

    ScopedMem<char> s(LoadTextResource(IDD_EBOOK_WIN_DESC));
    ParsedMui *muiDef = ParsedMui::Create(s, ctrls->mainWnd, CreatePageControl);
    CrashIf(!muiDef);

    ctrls->muiDef = muiDef;
    CrashIf(!muiDef->FindControl("prevButton", "ButtonVector"));
    CrashIf(!muiDef->FindControl("nextButton", "ButtonVector"));
    ctrls->status = static_cast<Button *>(muiDef->FindControl("statusButton", "Button"));
    CrashIf(!ctrls->status);
    ctrls->progress = static_cast<ScrollBar *>(muiDef->FindControl("progressScrollBar", "ScrollBar"));
    CrashIf(!ctrls->progress);
    ctrls->progress->hCursor = gCursorHand;
    ctrls->page = static_cast<PageControl *>(muiDef->FindControl("page", "EbookPage"));
    CrashIf(!ctrls->page);
    ctrls->topPart = muiDef->FindLayout("top");
    CrashIf(!ctrls->topPart);

    Style *styleMainWnd = StyleByName("styleMainWnd");
    CrashIf(!styleMainWnd);

    // update the background color from the prefs
    CrashIf(!styleMainWnd);
    COLORREF bgColor = gGlobalPrefs->ebookUI.backgroundColor;
    if (gGlobalPrefs->useSysColors)
        bgColor = GetSysColor(COLOR_WINDOW);
    styleMainWnd->Set(Prop::AllocColorSolid(PropBgColor, GetRValue(bgColor), GetGValue(bgColor), GetBValue(bgColor)));

    ctrls->mainWnd->SetStyle(styleMainWnd);
    ctrls->mainWnd->layout = muiDef->FindLayout("mainLayout");
    CrashIf(!ctrls->mainWnd->layout);

    return ctrls;
}

void DestroyEbookControls(EbookControls* ctrls)
{
    delete ctrls->mainWnd;
    delete ctrls->topPart;
    delete ctrls->muiDef;
    free(ctrls);
}
