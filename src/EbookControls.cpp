/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/BitManip.h"
#include "utils/GdiPlusUtil.h"
#include "utils/HtmlParserLookup.h"
#include "mui/Mui.h"
#include "utils/SerializeTxt.h"
#include "utils/StrSlice.h"
#include "utils/Timer.h"
#include "utils/TrivialHtmlParser.h"
#include "utils/TxtParser.h"
#include "utils/WinUtil.h"
// rendering engines
#include "EbookBase.h"
#include "HtmlFormatter.h"
// ui
#include "SumatraPDF.h"
#include "resource.h"
#include "EbookControls.h"
#include "MuiEbookPageDef.h"
#include "PagesLayoutDef.h"

constexpr const char ebookWinDesc[] = R"data(
Style [
    name: styleMainWnd
    bg_col: sepia
]

Style [
    name: stylePage
    padding: 32 16
    bg_col: transparent
]

Style [
    name: styleNextDefault
    parent: buttonDefault
    border_width: 0
    padding: 0 8
    stroke_width: 0
    fill: gray
    bg_col: transparent
    vert_align: center
]

Style [
    name: styleNextMouseOver
    parent: styleNextDefault
    fill: black
]

Style [
    name: styleStatus
    parent: buttonDefault
    bg_col: sepia
    col: black
    font_size: 8
    font_weight: regular
    padding: 3 0
    border_width: 0
    text_align: center
]

Style [
    name: styleProgress
    bg_col: light gray
    col: light blue
]

ButtonVector [
    name: nextButton
    clicked: next
    path: M0 0  L10 13 L0 ,26 Z
    style_default: styleNextDefault
    style_mouse_over: styleNextMouseOver
]

ButtonVector [
    name: prevButton
    clicked: prev
    path: M10 0 L0,  13 L10 26 z
    style_default: styleNextDefault
    style_mouse_over: styleNextMouseOver
]

Button [
    name: statusButton
    style: styleStatus
]

ScrollBar [
    name: progressScrollBar
    style: styleProgress
    cursor: hand
]

EbookPage [
    name: page1
    style: stylePage
]

EbookPage [
    name: page2
    style: stylePage
]

PagesLayout [
    name: pagesLayout
    page1: page1
    page2: page2
    spaceDx: 12
]

HorizontalLayout [
    name: top
    children [
        prevButton self 1 bottom
        pagesLayout 1 1 top
        nextButton self 1 center
    ]
]

VerticalLayout [
    name: mainLayout
    children [
        top 1 1 top
        progressScrollBar self 1 center
        statusButton self 1 center
    ]
]
)data";

PageControl::PageControl() : page(nullptr), cursorX(-1), cursorY(-1) {
    bit::Set(wantedInputBits, WantsMouseMoveBit, WantsMouseClickBit);
}

PageControl::~PageControl() {
    if (toolTip) {
        // TODO: make Control's destructor clear the tooltip?
        Control::NotifyMouseLeave();
    }
}

void PageControl::SetPage(HtmlPage* newPage) {
    page = newPage;
    RequestRepaint(this);
}

DrawInstr* PageControl::GetLinkAt(int x, int y) const {
    if (!page)
        return nullptr;

    PointF pt((REAL)(x - cachedStyle->padding.left), (REAL)(y - cachedStyle->padding.top));
    for (DrawInstr& i : page->instructions) {
        if (DrawInstrType::LinkStart == i.type && !i.bbox.IsEmptyArea() && i.bbox.Contains(pt)) {
            return &i;
        }
    }
    return nullptr;
}

void PageControl::NotifyMouseMove(int x, int y) {
    DrawInstr* link = GetLinkAt(x, y);
    if (!link) {
        SetCursor(IDC_ARROW);
        if (toolTip) {
            Control::NotifyMouseLeave();
            str::ReplacePtr(&toolTip, nullptr);
        }
        return;
    }

    SetCursor(IDC_HAND);
    AutoFreeWstr url(strconv::FromHtmlUtf8(link->str.s, link->str.len));
    if (toolTip && (!url::IsAbsolute(url) || !str::Eq(toolTip, url))) {
        Control::NotifyMouseLeave();
        str::ReplacePtr(&toolTip, nullptr);
    }
    if (!toolTip && url::IsAbsolute(url)) {
        toolTip = url.StealData();
        Control::NotifyMouseEnter();
    }
}

// size of the drawable area i.e. size minus padding
Size PageControl::GetDrawableSize() const {
    Size s;
    pos.GetSize(&s);
    Padding pad = cachedStyle->padding;
    s.Width -= (pad.left + pad.right);
    s.Height -= (pad.top + pad.bottom);
    if ((s.Width <= 0) || (s.Height <= 0))
        return Size();
    return s;
}

void PageControl::Paint(Graphics* gfx, int offX, int offY) {
    CrashIf(!IsVisible());

    auto timerAll = TimeGet();

    CachedStyle* s = cachedStyle;
    auto timerFill = TimeGet();
    Rect r(offX, offY, pos.Width, pos.Height);
    if (!s->bgColor->IsTransparent()) {
        Brush* br = BrushFromColorData(s->bgColor, r);
        gfx->FillRectangle(br, r);
    }
    double durFill = TimeSinceInMs(timerFill);

    if (!page)
        return;

    // during resize the page we currently show might be bigger than
    // our area. To avoid drawing outside our area we clip
    Region origClipRegion;
    gfx->GetClip(&origClipRegion);
    r.X += s->padding.left;
    r.Y += s->padding.top;
    r.Width -= (s->padding.left + s->padding.right);
    r.Height -= (s->padding.top + s->padding.bottom);
    r.Inflate(1, 0);
    gfx->SetClip(r, CombineModeReplace);

    COLORREF txtCol, bgCol;
    GetEbookUiColors(txtCol, bgCol);
    Color textColor, bgColor;
    textColor.SetFromCOLORREF(txtCol);

    ITextRender* textRender = CreateTextRender(GetTextRenderMethod(), gfx, pos.Width, pos.Height);
    // ITextRender *textRender = CreateTextRender(TextRenderMethodHdc, gfx, pos.Width, pos.Height);

    bgColor.SetFromCOLORREF(bgCol);
    textRender->SetTextBgColor(bgColor);

    auto timerDrawHtml = TimeGet();
    DrawHtmlPage(gfx, textRender, &page->instructions, (REAL)r.X, (REAL)r.Y, IsDebugPaint(), textColor);
    double durDraw = TimeSinceInMs(timerDrawHtml);
    gfx->SetClip(&origClipRegion, CombineModeReplace);
    delete textRender;

    double durAll = TimeSinceInMs(timerAll);
    // logf("all: %.2f, fill: %.2f, draw html: %.2f\n", durAll, durFill, durDraw);
}

Control* CreatePageControl(TxtNode* structDef) {
    CrashIf(!structDef->IsStructWithName("EbookPage"));
    EbookPageDef* def = DeserializeEbookPageDef(structDef);
    PageControl* c = new PageControl();
    Style* style = StyleByName(def->style);
    c->SetStyle(style);

    if (def->name)
        c->SetName(def->name);

    FreeEbookPageDef(def);
    return c;
}

ILayout* CreatePagesLayout(ParsedMui* parsedMui, TxtNode* structDef) {
    CrashIf(!structDef->IsStructWithName("PagesLayout"));
    PagesLayoutDef* def = DeserializePagesLayoutDef(structDef);
    CrashIf(!def->page1 || !def->page2);
    PageControl* page1 = static_cast<PageControl*>(FindControlNamed(*parsedMui, def->page1));
    PageControl* page2 = static_cast<PageControl*>(FindControlNamed(*parsedMui, def->page2));
    CrashIf(!page1 || !page2);
    PagesLayout* layout = new PagesLayout(page1, page2, def->spaceDx);
    if (def->name)
        layout->SetName(def->name);
    FreePagesLayoutDef(def);
    return layout;
}

void SetMainWndBgCol(EbookControls* ctrls) {
    COLORREF txtColor, bgColor;
    GetEbookUiColors(txtColor, bgColor);

    Style* styleMainWnd = StyleByName("styleMainWnd");
    CrashIf(!styleMainWnd);
    u8 r, g, b;
    UnpackRgb(bgColor, r, g, b);
    styleMainWnd->Set(Prop::AllocColorSolid(PropBgColor, r, g, b));
    ctrls->mainWnd->SetStyle(styleMainWnd);

    Style* styleStatus = StyleByName("styleStatus");
    styleStatus->Set(Prop::AllocColorSolid(PropBgColor, r, g, b));
    ctrls->status->SetStyle(styleStatus);

    // TODO: should also allow to change text color
    // TODO: also match the colors of progress bar to be based on background color
    // TODO: update tab color

    // note: callers are expected to update the background of tree control and
    // other colors that are supposed to match background color
}

EbookControls* CreateEbookControls(HWND hwnd, FrameRateWnd* frameRateWnd) {
    static bool wasRegistered = false;
    if (!wasRegistered) {
        RegisterControlCreatorFor("EbookPage", &CreatePageControl);
        RegisterLayoutCreatorFor("PagesLayout", &CreatePagesLayout);
        wasRegistered = true;
    }

    ParsedMui* muiDef = new ParsedMui();
    size_t ebookWinDescLen = static_strlen(ebookWinDesc);
    std::string_view ebookStr(ebookWinDesc, ebookWinDescLen);
    MuiFromText(*muiDef, ebookStr);

    EbookControls* ctrls = new EbookControls;
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

    for (size_t i = 0; i < muiDef->allControls.size(); i++) {
        Control* c = muiDef->allControls.at(i);
        ctrls->mainWnd->AddChild(c);
    }
    return ctrls;
}

void DestroyEbookControls(EbookControls* ctrls) {
    delete ctrls->mainWnd;
    delete ctrls->topPart;
    delete ctrls->pagesLayout;
    delete ctrls->muiDef;
    delete ctrls;
}

Size PagesLayout::Measure(const Size availableSize) {
    desiredSize = availableSize;
    return desiredSize;
}

void PagesLayout::Arrange(const Rect finalRect) {
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
            dx = dx / 2;
            CrashIf(dx < 10);
        }
    }
    Rect r = finalRect;
    r.Width = dx;
    page1->Arrange(r);
    r.X = r.X + dx + spaceDx;
    page2->Arrange(r);
}
