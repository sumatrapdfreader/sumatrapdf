/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "EbookControls.h"

#include "BitManip.h"
#include "HtmlFormatter.h"
#include "resource.h"
#include "SquareTreeParser.h"
#include "SvgPath.h"

#include "DebugLog.h"

static Style *   styleMainWnd = NULL;
static Style *   stylePage = NULL;
static Style *   styleStatus = NULL;
static Style *   styleProgress = NULL;
static HCURSOR   gCursorHand = NULL;

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

static SquareTree *gParser = NULL;

static float ParseFloat(const char *s)
{
    return (float)atof(s);
}

struct ParsedPadding {
    int top;
    int right;
    int bottom;
    int left;
};

// TODO: be more forgiving with whitespace
// TODO: allow 1 or 2 elements
static void ParsePadding(const char *s, ParsedPadding& p)
{
    str::Parse(s, "%d %d %d %d", &p.top, &p.right, &p.bottom, &p.left);
}

// TODO: more enums
static AlignAttr ParseAlignAttr(const char *s)
{
    if (str::EqI(s, "center"))
        return Align_Center;
    CrashIf(true);
    return Align_Left;
}

// TODO: more enums
static ElAlign ParseElAlign(const char *s)
{
    if (str::EqI(s, "center"))
        return ElAlignCenter;
    CrashIf(true);
    return ElAlignLeft;
}

#if 0
    FontStyleRegular    = 0,
    FontStyleBold       = 1,
    FontStyleItalic     = 2,
    FontStyleBoldItalic = 3,
    FontStyleUnderline  = 4,
    FontStyleStrikeout  = 8
#endif
static Gdiplus::FontStyle ParseFontWeight(const char *s)
{
    if (str::EqI(s, "regular"))
        return FontStyleRegular;
    CrashIf(true);
    // TODO: more
    return FontStyleRegular;
}

static Style *StyleFromStruct(SquareTreeNode *node)
{
    Style *style = new Style();
    const char *value;

    if ((value = node->GetValue("name")) != NULL)
        style->SetName(value);
    if ((value = node->GetValue("bg_col")) != NULL)
        style->Set(Prop::AllocColorSolid(PropBgColor, value));
    if ((value = node->GetValue("col")) != NULL)
        style->Set(Prop::AllocColorSolid(PropColor, value));
    if ((value = node->GetValue("parent")) != NULL) {
        Style *parentStyle = StyleByName(value);
        CrashIf(!parentStyle);
        style->SetInheritsFrom(parentStyle);
    }
    if ((value = node->GetValue("border_width")) != NULL)
        style->SetBorderWidth(ParseFloat(value));
    if ((value = node->GetValue("padding")) != NULL) {
        ParsedPadding padding = { 0 };
        ParsePadding(value, padding);
        style->SetPadding(padding.top, padding.right, padding.bottom, padding.left);
    }
    if ((value = node->GetValue("stroke_width")) != NULL)
        style->Set(Prop::AllocWidth(PropStrokeWidth, ParseFloat(value)));
    if ((value = node->GetValue("fill")) != NULL)
        style->Set(Prop::AllocColorSolid(PropFill, value));
    if ((value = node->GetValue("vert_align")) != NULL)
        style->Set(Prop::AllocAlign(PropVertAlign, ParseElAlign(value)));
    if ((value = node->GetValue("text_align")) != NULL)
        style->Set(Prop::AllocTextAlign(ParseAlignAttr(value)));
    if ((value = node->GetValue("font_size")) != NULL)
        style->Set(Prop::AllocFontSize(ParseFloat(value)));
    if ((value = node->GetValue("font_weight")) != NULL)
        style->Set(Prop::AllocFontWeight(ParseFontWeight(value)));

    CacheStyle(style);
    return style;
}

static ButtonVector* ButtonVectorFromStruct(SquareTreeNode *node)
{
    ButtonVector *b = new ButtonVector();
    const char *value;

    if ((value = node->GetValue("name")) != NULL)
        b->SetName(value);
    if ((value = node->GetValue("path")) != NULL)
        b->SetGraphicsPath(svg::GraphicsPathFromPathData(value));
    if ((value = node->GetValue("style_default")) != NULL) {
        Style *style = StyleByName(value);
        CrashIf(!style);
        b->SetDefaultStyle(style);
    }
    if ((value = node->GetValue("style_mouse_over")) != NULL) {
        Style *style = StyleByName(value);
        CrashIf(!style);
        b->SetMouseOverStyle(style);
    }

    return b;
}

static char *LoadTextResource(int resId, size_t *sizeOut)
{
    HRSRC resSrc = FindResource(NULL, MAKEINTRESOURCE(resId), RT_RCDATA);
    CrashIf(!resSrc);
    HGLOBAL res = LoadResource(NULL, resSrc);
    CrashIf(!res);
    DWORD size = SizeofResource(NULL, resSrc);
    const char *resData = (const char*)LockResource(res);
    char *s = str::DupN(resData, size);
    if (sizeOut)
        *sizeOut = size;
    UnlockResource(res);
    return s;
}

static bool LoadAndParseWinDesc()
{
    CrashIf(gParser);
    ScopedMem<char> winDesc(LoadTextResource(IDD_EBOOK_WIN_DESC, NULL));
    gParser = new SquareTree(winDesc);
    CrashIf(!gParser->root || gParser->root->data.Count() == 0);
    if (!gParser->root || gParser->root->data.Count() == 0)
        gParser->root = new SquareTreeNode();
    return gParser->root != NULL && gParser->root->data.Count() != 0;
}

// should only be called once at the end of the program
extern "C" static void DeleteEbookStyles()
{
    delete gParser;
}

static void CreateEbookStyles()
{
    CrashIf(styleMainWnd); // only call me once

    CrashIf(!gParser || !gParser->root);
    size_t off = 0;
    SquareTreeNode *node;
    while ((node = gParser->root->GetChild("Style", &off)) != NULL) {
        StyleFromStruct(node);
    }

    // TODO: support changing this color to gRenderCache.colorRange[1]
    //       or GetSysColor(COLOR_WINDOW) if gGlobalPrefs.useSysColors

    styleMainWnd = StyleByName("styleMainWnd");
    CrashIf(!styleMainWnd);
    stylePage = StyleByName("stylePage");
    CrashIf(!stylePage);
    styleStatus = StyleByName("styleStatus");
    CrashIf(!styleStatus);
    styleProgress = StyleByName("styleProgress");
    CrashIf(!styleProgress);

    atexit(DeleteEbookStyles);
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

// TODO: create the rest of controls
static void CreateControls(EbookControls *ctrls)
{
    size_t off = 0;
    SquareTreeNode *node;
    while ((node = gParser->root->GetChild("ButtonVector", &off)) != NULL) {
        ButtonVector *b = ButtonVectorFromStruct(node);
        if (b->IsNamed("nextButton"))
            ctrls->next = b;
        else if (b->IsNamed("prevButton"))
            ctrls->prev = b;
        else
            CrashIf(true);
    }
    CrashIf(!ctrls->next);
    CrashIf(!ctrls->prev);
}

EbookControls *CreateEbookControls(HWND hwnd)
{
    if (!gCursorHand)
        gCursorHand  = LoadCursor(NULL, IDC_HAND);

    if (!gParser) {
        // TODO: verify return value
        LoadAndParseWinDesc();
        CreateEbookStyles();
    }

    EbookControls *ctrls = new EbookControls;

    CreateControls(ctrls);

    ctrls->progress = new ScrollBar();
    ctrls->progress->hCursor = gCursorHand;
    ctrls->progress->SetStyle(styleProgress);

    ctrls->status = new Button(NULL, styleStatus, styleStatus);

    ctrls->page = new PageControl();
    ctrls->page->SetStyle(stylePage);

    ctrls->mainWnd = new HwndWrapper(hwnd);
    ctrls->mainWnd->SetMinSize(Size(320, 200));
    ctrls->mainWnd->SetStyle(styleMainWnd);

    ctrls->mainWnd->AddChild(ctrls->next, ctrls->prev, ctrls->page);
    ctrls->mainWnd->AddChild(ctrls->progress, ctrls->status);
    CreateLayout(ctrls);
    return ctrls;
}

void DestroyEbookControls(EbookControls* ctrls)
{
    delete ctrls->mainWnd;
    delete ctrls->topPart;
    delete ctrls;
}
