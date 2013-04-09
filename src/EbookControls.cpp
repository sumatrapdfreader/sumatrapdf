/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "EbookControls.h"

#include "BitManip.h"
#include "HtmlFormatter.h"
#include "resource.h"
#include "SvgPath.h"
#include "TxtParser.h"

#include "DebugLog.h"

static Style *   styleMainWnd = NULL;
static Style *   stylePage = NULL;
static Style *   styleStatus = NULL;
static Style *   styleProgress = NULL;
static Style *   styleBtnNextPrevDefault = NULL;
static Style *   styleBtnNextPrevMouseOver = NULL;
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

static char *gWinDesc = NULL;
static size_t gWinDescSize;
static TxtParser *gParser = NULL;

static TxtNode *GetRootArray(TxtParser* parser)
{
    TxtNode *root = parser->nodes.At(0);
    CrashIf(!root->IsArray());
    return root;
}

static Vec<TxtNode*> *GetStructsWithName(TxtNode *root, const char *name)
{
    size_t nameLen = str::Len(name);
    CrashIf(!root->IsArray());
    Vec<TxtNode*> *res = NULL;
    TxtNode **n;
    for (n = root->children->IterStart(); n; n = root->children->IterNext()) {
        TxtNode *node = *n;
        if (node->IsStructWithName(name, nameLen)) {
            if (!res)
                res = new Vec<TxtNode*>();
            res->Append(node);
        }
    }
    return res;
}

float ParseFloat(const char *s)
{
    char *end = (char*)s;
    return (float)strtod(s, &end);
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

static void AddStyleProp(Style *style, TxtNode *prop)
{
    if (prop->IsTextWithKey("name")) {
        ScopedMem<char> tmp(prop->ValDup());
        style->SetName(tmp);
        return;
    }

    if (prop->IsTextWithKey("bg_col")) {
        ScopedMem<char> tmp(prop->ValDup());
        style->Set(Prop::AllocColorSolid(PropBgColor, tmp));
        return;
    }

    if (prop->IsTextWithKey("col")) {
        ScopedMem<char> tmp(prop->ValDup());
        style->Set(Prop::AllocColorSolid(PropColor, tmp));
        return;
    }

    if (prop->IsTextWithKey("parent")) {
        ScopedMem<char> tmp(prop->ValDup());
        Style *parentStyle = StyleByName(tmp);
        CrashIf(!parentStyle);
        style->SetInheritsFrom(parentStyle);
        return;
    }

    if (prop->IsTextWithKey("border_width")) {
        ScopedMem<char> tmp(prop->ValDup());
        style->SetBorderWidth(ParseFloat(tmp));
        return;
    }

    if (prop->IsTextWithKey("padding")) {
        ScopedMem<char> tmp(prop->ValDup());
        ParsedPadding padding = { 0 };
        ParsePadding(tmp, padding);
        style->SetPadding(padding.top, padding.right, padding.bottom, padding.left);
        return;
    }

    if (prop->IsTextWithKey("stroke_width")) {
        ScopedMem<char> tmp(prop->ValDup());
        style->Set(Prop::AllocWidth(PropStrokeWidth, ParseFloat(tmp)));
        return;
    }

    if (prop->IsTextWithKey("fill")) {
        ScopedMem<char> tmp(prop->ValDup());
        style->Set(Prop::AllocColorSolid(PropFill, tmp));
        return;
    }

    if (prop->IsTextWithKey("vert_align")) {
        ScopedMem<char> tmp(prop->ValDup());
        style->Set(Prop::AllocAlign(PropVertAlign, ParseElAlign(tmp)));
        return;
    }

    if (prop->IsTextWithKey("text_align")) {
        ScopedMem<char> tmp(prop->ValDup());
        style->Set(Prop::AllocTextAlign(ParseAlignAttr(tmp)));
        return;
    }

    if (prop->IsTextWithKey("font_size")) {
        ScopedMem<char> tmp(prop->ValDup());
        style->Set(Prop::AllocFontSize(ParseFloat(tmp)));
        return;
    }

    
    if (prop->IsTextWithKey("font_weight")) {
        ScopedMem<char> tmp(prop->ValDup());
        style->Set(Prop::AllocFontWeight(ParseFontWeight(tmp)));
        return;
    }

    CrashIf(true);
}

static Style* StyleFromStruct(TxtNode* styleStruct)
{
    CrashIf(!styleStruct->IsStructWithName("style"));
    Style *style = new Style();
    size_t n = styleStruct->children->Count();
    for (size_t i = 0; i < n; i++) {
        TxtNode *n = styleStruct->children->At(i);
        CrashIf(!n->IsText());
        AddStyleProp(style, n);
    }
    CacheStyle(style);
    return style;
}

static Vec<Style*> *StylesFromStyleStructs(Vec<TxtNode*> *styleNodes)
{
    size_t n = styleNodes->Count();
    Vec<Style*> *res = new Vec<Style*>(n);
    for (size_t i = 0; i < n; i++) {
        res->Append(StyleFromStruct(styleNodes->At(i)));
    }
    return res;
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
    *sizeOut = size;
    UnlockResource(res);
    return s;
}

static bool LoadAndParseWinDesc()
{
    CrashIf(gWinDesc);
    gWinDesc = LoadTextResource(IDD_EBOOK_WIN_DESC, &gWinDescSize);
    gParser = new TxtParser();
    gParser->SetToParse(gWinDesc, gWinDescSize);
    bool ok = ParseTxt(*gParser);
    CrashIf(!ok);
    return ok;
}

// should only be called once at the end of the program
extern "C" static void DeleteEbookStyles()
{
    delete gParser;
    free(gWinDesc);
}

static void CreateEbookStyles()
{
    CrashIf(styleMainWnd); // only call me once

    Vec<TxtNode*> *styleNodes = GetStructsWithName(GetRootArray(gParser), "Style");
    CrashIf(!styleNodes);

    Vec<Style*> *styles = StylesFromStyleStructs(styleNodes);

    // TODO: support changing this color to gRenderCache.colorRange[1]
    //       or GetSysColor(COLOR_WINDOW) if gGlobalPrefs.useSysColors

    styleMainWnd = StyleByName("styleMainWnd");
    CrashIf(!styleMainWnd);
    stylePage = StyleByName("stylePage");
    CrashIf(!stylePage);
    styleBtnNextPrevDefault = StyleByName("styleNextDefault");
    CrashIf(!styleBtnNextPrevDefault);
    styleBtnNextPrevMouseOver = StyleByName("styleNextMouseOver");
    CrashIf(!styleBtnNextPrevMouseOver);
    styleStatus = StyleByName("styleStatus");
    CrashIf(!styleStatus);
    styleProgress = StyleByName("styleProgress");
    CrashIf(!styleProgress);

    delete styles;
    delete styleNodes;
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

EbookControls *CreateEbookControls(HWND hwnd)
{
    if (!gCursorHand)
        gCursorHand  = LoadCursor(NULL, IDC_HAND);

    if (!gWinDesc) {
        LoadAndParseWinDesc();
        CreateEbookStyles();
    }

    EbookControls *ctrls = new EbookControls;

    ctrls->next = new ButtonVector(svg::GraphicsPathFromPathData("M0 0  L10 13 L0 ,26 Z"));
    ctrls->next->SetStyles(styleBtnNextPrevDefault, styleBtnNextPrevMouseOver);

    ctrls->prev = new ButtonVector(svg::GraphicsPathFromPathData("M10 0 L0,  13 L10 26 z"));
    ctrls->prev->SetStyles(styleBtnNextPrevDefault, styleBtnNextPrevMouseOver);

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
