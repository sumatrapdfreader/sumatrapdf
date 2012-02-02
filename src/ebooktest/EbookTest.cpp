/* Copyright 2010-2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Hack: we need NOMINMAX to make chrome code compile but we also need
// min/max for gdi+ headers, so we import min/max from stl
#include <algorithm>
using std::min;
using std::max;

// include anything that might (re)define operator new before BaseUtil.h
#include "base/threading/thread.h"
#include "base/bind.h"

#include "Resource.h"
#include "NoFreeAllocator.h"
#include "StrUtil.h"
#include "FileUtil.h"
#include "WinUtil.h"
#include "Version.h"
#include "Vec.h"
#include "mui.h"
#include "CmdLineParser.h"
#include "FrameTimeoutCalculator.h"
#include "FileTransactions.h"
#include "Scoped.h"
#include "PageLayout.h"
#include "MobiDoc.h"
#include "EbookTestMenu.h"

/*
TODO: doing layout on a background thread needs to be more sophisticated.
Technically it works but user experience is bad.

The key to supporting fluid resizing is to limit the amount of work tha's done.
While the window is being resized, we should only calculate a few pages ahead.

Only when the user turns the pages (or, additionally, after some delay since
the last resize) we should layout everything so that we can show
number of pages.

Also, we must do the Kindle trick, where resizing preserves the top of
current page, but on going back it resyncs to show a page the way it
would be if we came there from the first page. 

One way to do it would be to add another layout which creates a new
stream of pages based on another stream of pages.

Another option, probably better, is to turn PageLayout into an iterator
that can give us pages incrementally and from an arbitrary position
in the html.

TODO: by hooking into mouse move events in HorizontalProgress control, we
could show a window telling the user which page would we go to if he was
to click there.
*/

using namespace Gdiplus;
using namespace mui;

class ControlEbook;

#define l(s) OutputDebugStringA(s)

#define ET_FRAME_CLASS_NAME    _T("ET_FRAME")

#define FONT_NAME              L"Georgia"
#define FONT_SIZE              12

#define WIN_DX    640
#define WIN_DY    480

static HINSTANCE        ghinst;
static HWND             gHwndFrame = NULL;
static ControlEbook *   gControlFrame = NULL;
static HCURSOR          gCursorHand = NULL;

// for convenience so that we don't have to pass it around
static MessageLoop *    gMessageLoopUI = NULL;

static bool gShowTextBoundingBoxes = false;

// A sample text to display if we don't show an actual mobi file
static const char *gSampleHtml = "<html><p align=justify>ClearType is <b>dependent</b> on the <i>orientation &amp; ordering</i> of the LCD stripes.</p> <p align='right'><em>Currently</em>, ClearType is implemented <hr> only for vertical stripes that are ordered RGB.</p> <p align=center>This might be a concern if you are using a tablet PC.</p> <p>Where the display can be oriented in any direction, or if you are using a screen that can be turned from landscape to portrait. The <strike>following example</strike> draws text with two <u>different quality</u> settings.</p> On to the <b>next<mbp:pagebreak>page</b></html>";

static float PercFromInt(int total, int n)
{
    CrashIf(n > total);
    if (0 == total)
        return 0.f;
    return (float)n / (float)total;
}

int IntFromPerc(int total, float perc) {
    return (int)(total * perc);
}

/* The layout is:
___________________
|                 |
| next       prev |
|                 |
|[ position bar  ]|
|[    status     ]|
___________________
*/

// Horizontal progress bar is a horizontal rectangle that visually
// represents percentage progress of some activity.
// It can also report clicks within itself.
// The background is drawn with PropBgColor and the part that represents
// percentage with PropColor.
// It has a fixed height, provided by a caller, but the width can vary
// depending on layout.
// For a bit of an extra effect, it has 2 heights: one when mouse is over
// the control and another when mouse is not over.
// For the purpose of layout, we use the bigger of the to as the real
// height of the control
// TODO: we don't take padding int account yet
// Clients can register fo IClickEvent events for this window to implement
// interactivity
class HorizontalProgressBar : public Control
{
    int     onOverDy;    // when mouse is over
    int     inactiveDy;  // when mouse is not over

    // what percentage of the rectangle is filled with PropColor
    // (the rest is filled with PropBgColor).
    // The range is from 0 to 1
    float   filledPerc;

public:
    HorizontalProgressBar(int onOverDy = 12, int inactiveDy = 5);
    ~HorizontalProgressBar() { }
    virtual void Measure(const Size availableSize);
    virtual void NotifyMouseEnter();
    virtual void NotifyMouseLeave();

    virtual void Paint(Graphics *gfx, int offX, int offY);

    void SetFilled(float perc);
    float GetPercAt(int x);
};

HorizontalProgressBar::HorizontalProgressBar(int onOverDy, int inactiveDy)
        : onOverDy(onOverDy), inactiveDy(inactiveDy)
{
    filledPerc = 0.f;
    bit::Set(wantedInputBits, WantsMouseOverBit, WantsMouseClickBit);
}

void HorizontalProgressBar::Measure(const Size availableSize)
{
    // dx is max available
    desiredSize.Width = availableSize.Width;

    // dy is bigger of inactiveDy and onHoverDy but 
    // smaller than availableSize.Height
    int dy = inactiveDy;
    if (onOverDy > dy)
        dy = onOverDy;
    if (dy > availableSize.Height)
        dy = availableSize.Height;

    desiredSize.Height = dy;
}

void HorizontalProgressBar::NotifyMouseEnter()
{
    if (inactiveDy != onOverDy)
        RequestRepaint(this);
}

void HorizontalProgressBar::NotifyMouseLeave()
{
    if (inactiveDy != onOverDy)
        RequestRepaint(this);
}

void HorizontalProgressBar::SetFilled(float perc)
{
    CrashIf((perc < 0.f) || (perc > 1.f));
    int prev = IntFromPerc(pos.Width, filledPerc);
    int curr = IntFromPerc(pos.Width, perc);
    filledPerc = perc;
    if (prev != curr)
        RequestRepaint(this);
}

float HorizontalProgressBar::GetPercAt(int x)
{
    return PercFromInt(pos.Width, x);
}

void HorizontalProgressBar::Paint(Graphics *gfx, int offX, int offY)
{
    if (!IsVisible())
        return;

    // TODO: take padding into account
    Prop **props = GetCachedProps();
    Prop *col   = props[PropColor];
    Prop *bgCol = props[PropBgColor];

    Rect r(offX, offY, pos.Width, pos.Height);
    Brush *br = BrushFromProp(bgCol, r);
    gfx->FillRectangle(br, r);
    ::delete br;

    int filledDx = IntFromPerc(pos.Width, filledPerc);
    if (0 == filledDx)
        return;

    r.Width = filledDx;
    int dy = inactiveDy;
    if (IsMouseOver())
        dy = onOverDy;

    r.Y += (r.Height - dy);
    r.Height = dy;
    br = BrushFromProp(col, r);
    gfx->FillRectangle(br, r);
    ::delete br;
}

class ControlEbook 
    : public HwndWrapper,
      public IClickHandler,
      public INewPageObserver
{
    static const int CircleR = 10;

    void AdvancePage(int dist);
    void SetPage(int newPageNo);

public:
    MobiDoc *     mb;
    const char *    html;

    Vec<PageData*>* pages;
    int             currPageNo;

    // those are temporary pages sent to us from background
    // thread during layout
    Vec<PageData*>* newPages;

    int             cursorX, cursorY;
    int             lastDx, lastDy;
    base::Thread *  mobiLoadThread;
    base::Thread *  pageLayoutThread;

    Button * next;
    Button * prev;
    HorizontalProgressBar *horizProgress;
    Button * status;
    Button * test;

    Style *         ebookDefault;
    Style *         statusDefault;
    Style *         horizProgressDefault;
    Style *         facebookButtonDefault;
    Style *         facebookButtonOver;

    // TODO: for testing
    Style *         nextDefault;
    Style *         nextMouseOver;
    Style *         prevDefault;
    Style *         prevMouseOver;

    ControlEbook(HWND hwnd);
    virtual ~ControlEbook();

    virtual void RegisterEventHandlers(EventMgr *evtMgr);
    virtual void UnRegisterEventHandlers(EventMgr *evtMgr);

    virtual void Paint(Graphics *gfx, int offX, int offY);

    virtual void NotifyMouseMove(int x, int y);

    // IClickHandler
    virtual void Clicked(Control *w, int x, int y);

    void SetHtml(const char *html);
    void LoadMobi(const TCHAR *fileName);

    void SetStatusText() const;

    void DeletePages();
    void DeleteNewPages();

    // INewPageObserver
    virtual void NewPage(PageData *pageData);

    void NewPageUIThread(PageData *pageData);
    void PageLayoutFinished();
    void PageLayoutBackground(LayoutInfo *li);

    void PageLayout(int dx, int dy);
    void StopPageLayoutThread();

    void MobiLoaded(MobiDoc *mb);
    void MobiFailedToLoad(const TCHAR *fileName);
    void LoadMobiBackground(const TCHAR *fileName);
    void StopMobiLoadThread();
};

class EbookLayout : public Layout
{
public:
    EbookLayout(Button *next, Control *prev, Control *status, Control *horizProgress, Control *test) :
      next(next), prev(prev), status(status), horizProgress(horizProgress), test(test)
    {
    }

    virtual ~EbookLayout() {
    }

    Control *next;
    Control *prev;
    Control *status;
    Control *horizProgress;
    Control *test;

    virtual void Measure(const Size availableSize, Control *wnd);
    virtual void Arrange(const Rect finalRect, Control *wnd);
};

void EbookLayout::Measure(const Size availableSize, Control *wnd)
{
    Size s(availableSize);
    if (SizeInfinite == s.Width)
        s.Width = 320;
    if (SizeInfinite == s.Height)
        s.Height = 200;

    wnd->MeasureChildren(s);
    wnd->desiredSize = s;
}

static Size SizeFromRect(Rect& r)
{
    return Size(r.Width, r.Height);
}

// sets y position of toCenter rectangle so that it's centered
// within container of a given size. Doesn't change x position or size.
// note: it might produce negative position and that's fine
static void CenterRectY(Rect& toCenter, Size& container)
{
    toCenter.Y = (container.Height - toCenter.Height) / 2;
}

// sets x position of toCenter rectangle so that it's centered
// within container of a given size. Doesn't change y position or size.
// note: it might produce negative position and that's fine
static void CenterRectX(Rect& toCenter, Size& container)
{
    toCenter.X = (container.Width - toCenter.Width) / 2;
}

void EbookLayout::Arrange(const Rect finalRect, Control *wnd)
{
    int y, dx, dy;

    Prop **props = wnd->GetCachedProps();
    Prop *propPadding = props[PropPadding];
    int padLeft = propPadding->padding.left;
    int padRight = propPadding->padding.right;
    int padTop = propPadding->padding.top;
    int padBottom = propPadding->padding.bottom;

    int rectDy = finalRect.Height - (padTop + padBottom);
    int rectDx = finalRect.Width - (padLeft + padRight);

    // prev is on the left, y-middle
    Rect prevPos(Point(padLeft, 0), prev->desiredSize);
    CenterRectY(prevPos, Size(rectDx, rectDy));
    prev->Arrange(prevPos);

    // next is on the right, y-middle
    dx = next->desiredSize.Width;
    Rect nextPos(Point(rectDx - dx + padLeft, 0), next->desiredSize);
    CenterRectY(nextPos, Size(rectDx, rectDy));
    next->Arrange(nextPos);

    // test is at the bottom, x-middle
    dy = test->desiredSize.Height;
    y = rectDy - dy;
    Rect testPos(Point(0, y - padBottom), test->desiredSize);
    CenterRectX(testPos, Size(rectDx, rectDy));
    test->Arrange(testPos);

    // status is at the bottom
    y = finalRect.Height - status->desiredSize.Height;
    Rect statusPos(Point(0, y), status->desiredSize);
    statusPos.Width = finalRect.Width;
    status->Arrange(statusPos);

    // horizProgress is at the bottom, right above the status
    y -= horizProgress->desiredSize.Height;
    Rect horizPos(Point(0, y), horizProgress->desiredSize);
    horizPos.Width = finalRect.Width;
    horizProgress->Arrange(horizPos);

    wnd->SetPosition(finalRect);
    ControlEbook *wndEbook = (ControlEbook*)wnd;
    rectDy -= (statusPos.Height + horizPos.Height);
    if (rectDy < 0)
        rectDy = 0;
    wndEbook->PageLayout(rectDx, rectDy);
}

ControlEbook::ControlEbook(HWND hwnd)
{
    mobiLoadThread = NULL;
    pageLayoutThread = NULL;
    mb = NULL;
    html = NULL;
    pages = NULL;
    newPages = NULL;
    currPageNo = 0;
    lastDx = 0; lastDy = 0;
    SetHwnd(hwnd);

    cursorX = -1; cursorY = -1;

    const int pageBorderX = 10;
    const int pageBorderY = 10;

    bit::Set(wantedInputBits, WantsMouseMoveBit);
    ebookDefault = new Style();
    ebookDefault->Set(Prop::AllocPadding(pageBorderY, pageBorderX, pageBorderY, pageBorderX));
    SetCurrentStyle(ebookDefault, gStyleDefault);

    prevDefault = new Style(gStyleButtonDefault);
    prevDefault->Set(Prop::AllocPadding(12, 16, 4, 8));
    prevMouseOver = new Style(gStyleButtonMouseOver);
    prevMouseOver->Set(Prop::AllocPadding(4, 16, 4, 8));

    nextDefault = new Style(gStyleButtonDefault);
    nextDefault->Set(Prop::AllocPadding(4, 8, 12, 16));
    nextMouseOver = new Style(gStyleButtonMouseOver);
    nextMouseOver->Set(Prop::AllocPadding(12, 8, 4, 16));
    nextMouseOver->Set(Prop::AllocColorSolid(PropBgColor, "white"));

    facebookButtonDefault = new Style();
    facebookButtonDefault->Set(Prop::AllocColorSolid(PropColor, "white"));
    //facebookButtonDefault->Set(Prop::AllocColorLinearGradient(PropBgColor, LinearGradientModeVertical, "#75ae5c", "#67a54b"));
    facebookButtonDefault->Set(Prop::AllocColorLinearGradient(PropBgColor, LinearGradientModeVertical, "#647bad", "#5872a7"));
    facebookButtonDefault->Set(Prop::AllocColorSolid(PropBorderTopColor, "#29447E"));
    facebookButtonDefault->Set(Prop::AllocColorSolid(PropBorderRightColor, "#29447E"));
    facebookButtonDefault->Set(Prop::AllocColorSolid(PropBorderBottomColor, "#1A356E"));

    facebookButtonOver = new Style(facebookButtonDefault);
    facebookButtonOver->Set(Prop::AllocColorSolid(PropColor, "yellow"));

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

    next = new Button(_T("Next"));
    prev = new Button(_T("Prev"));
    horizProgress = new HorizontalProgressBar();
    horizProgress->hCursor = gCursorHand;
    status = new Button(_T(""));
    test = new Button(_T("test"));
    test->zOrder = 1;

    AddChild(next);
    AddChild(prev);
    AddChild(horizProgress);
    AddChild(status);
    AddChild(test);
    layout = new EbookLayout(next, prev, status, horizProgress, test);

    // unfortuante sequencing issue: some things trigger a repaint which
    // only works if the control has been placed in the control tree
    // via AddChild()
    next->SetStyles(nextDefault, nextMouseOver);
    prev->SetStyles(prevDefault, prevMouseOver);
    horizProgress->SetCurrentStyle(horizProgressDefault, gStyleDefault);
    status->SetStyles(statusDefault, statusDefault);
    test->SetStyles(facebookButtonDefault, facebookButtonOver);

    // special case for classes that derive from HwndWrapper
    // as they don't call this from SetParent() (like other Control derivatives)
    RegisterEventHandlers(evtMgr);
}

ControlEbook::~ControlEbook()
{
    // TODO: I think that the problem here is that while the thread might
    // be finished, there still might be in-flight messages sent
    // to this object. Do we need a way to cancel messages directed
    // to a given object from the queue? Ref-count the objects so that
    // their lifetime is managed correctly?
    StopMobiLoadThread();
    StopPageLayoutThread();

    // special case for classes that derive from HwndWrapper
    // as they don't trigger this from the destructor
    UnRegisterEventHandlers(evtMgr);

    DeleteNewPages();
    DeletePages();
    delete mb;
    delete statusDefault;
    delete facebookButtonDefault;
    delete facebookButtonOver;
    delete nextDefault;
    delete nextMouseOver;
    delete prevDefault;
    delete prevMouseOver;
    delete horizProgressDefault;
}

void ControlEbook::DeletePages()
{
    if (!pages)
        return;
    DeleteVecMembers<PageData*>(*pages);
    delete pages;
    pages = NULL;
}

void ControlEbook::DeleteNewPages()
{
    if (!newPages)
        return;
    DeleteVecMembers<PageData*>(*newPages);
    delete newPages;
    newPages = NULL;
}

void ControlEbook::SetStatusText() const
{
    if (!pages) {
        status->SetText(_T(""));
        horizProgress->SetFilled(0.f);
        return;
    }
    size_t pageCount = pages->Count();
    ScopedMem<TCHAR> s(str::Format(_T("Page %d out of %d"), currPageNo, (int)pageCount));
    status->SetText(s.Get());
    horizProgress->SetFilled(PercFromInt(pageCount, currPageNo));
}

void ControlEbook::SetPage(int newPageNo)
{
    CrashIf((newPageNo < 1) || (newPageNo > (int)pages->Count()));
    currPageNo = newPageNo;
    SetStatusText();
    RequestRepaint(this);
}

void ControlEbook::AdvancePage(int dist)
{
    if (!pages)
        return;
    int newPageNo = currPageNo + dist;
    if (newPageNo < 1)
        return;
    if (newPageNo > (int)pages->Count())
        return;
    SetPage(newPageNo);
}

void ControlEbook::PageLayoutFinished()
{
    StopPageLayoutThread();
    if (!newPages)
        return;
    DeletePages();
    pages = newPages;
    newPages = NULL;
    if (currPageNo > (int)pages->Count())
        currPageNo = 1;
    if (0 == currPageNo)
        currPageNo = 1;
    SetStatusText();
    RequestRepaint(this);
}

// called on a ui thread from background thread
void ControlEbook::NewPageUIThread(PageData *pageData)
{
    if (!newPages)
        newPages = new Vec<PageData*>();

    newPages->Append(pageData);
    // TODO: this really starves the UI thread. Is it because per-item
    // processing is so high? Would batching things up make UI responsive
    // during layout?
    if (newPages->Count() == 1) {
        ScopedMem<TCHAR> s(str::Format(_T("Layout started. Please wait...")));
        status->SetText(s.Get());
    }
}

// called on a background thread
void ControlEbook::NewPage(PageData *pageData)
{
    gMessageLoopUI->PostDelayedTask(base::Bind(&ControlEbook::NewPageUIThread, 
                                        base::Unretained(this), pageData), 100);
}

// called on a background thread
void ControlEbook::PageLayoutBackground(LayoutInfo *li)
{
    LayoutHtml(li);
    gMessageLoopUI->PostTask(base::Bind(&ControlEbook::PageLayoutFinished, 
                                        base::Unretained(this)));
    delete li;
}

void ControlEbook::PageLayout(int dx, int dy)
{
    if ((pages || newPages || pageLayoutThread) && ((lastDx == dx) && (lastDy == dy)))
        return;
    StopPageLayoutThread();

    DeleteNewPages();
    lastDx = dx;
    lastDy = dy;

    LayoutInfo *li = new LayoutInfo();

    size_t len;
    if (html)
        len = strlen(html);
    else
        html = mb->GetBookHtmlData(len);

    li->fontName = FONT_NAME;
    li->fontSize = FONT_SIZE;
    li->htmlStr = html;
    li->htmlStrLen = len;
    li->pageDx = dx;
    li->pageDy = dy;
    li->observer = this;

    pageLayoutThread = new base::Thread("ControlEbook::PageLayoutBackground");
    pageLayoutThread->Start();
    pageLayoutThread->message_loop()->PostTask(base::Bind(&ControlEbook::PageLayoutBackground, 
                                             base::Unretained(this), li));
}

void ControlEbook::StopPageLayoutThread()
{
    if (!pageLayoutThread)
        return;
    pageLayoutThread->Stop();
    delete pageLayoutThread;
    pageLayoutThread = NULL;
}

void ControlEbook::SetHtml(const char *newHtml)
{
    html = newHtml;
}

void ControlEbook::StopMobiLoadThread()
{
    if (!mobiLoadThread)
        return;
    mobiLoadThread->Stop();
    delete mobiLoadThread;
    mobiLoadThread = NULL;
}

// called on UI thread from background thread after
// mobi file has been loaded
void ControlEbook::MobiLoaded(MobiDoc *newMobi)
{
    CrashIf(gMessageLoopUI != MessageLoop::current());
    delete mb;
    mb = newMobi;
    html = NULL;
    delete pages;
    pages = NULL;
    RequestLayout();
}

// called on UI thread from backgroudn thread if we tried
// to load mobi file but failed
void ControlEbook::MobiFailedToLoad(const TCHAR *fileName)
{
    CrashIf(gMessageLoopUI != MessageLoop::current());
    // TODO: this message should show up in a different place, 
    // reusing status for convenience
    ScopedMem<TCHAR> s(str::Format(_T("Failed to load %s!"), fileName));
    status->SetText(s.Get());
    free((void*)fileName);
}

// Method executed on background thread that tries to load
// a given mobi file and either calls MobiLoaded() or 
// MobiFailedToLoad() on ui thread
void ControlEbook::LoadMobiBackground(const TCHAR *fileName)
{
    CrashIf(gMessageLoopUI == MessageLoop::current());
    MobiDoc *mb = MobiDoc::ParseFile(fileName);
    if (!mb)
        gMessageLoopUI->PostTask(base::Bind(&ControlEbook::MobiFailedToLoad, 
                                 base::Unretained(this), fileName));
    else
        gMessageLoopUI->PostTask(base::Bind(&ControlEbook::MobiLoaded, 
                                 base::Unretained(this), mb));
    free((void*)fileName);
}

void ControlEbook::LoadMobi(const TCHAR *fileName)
{
    // TODO: not sure if that's enough to handle user chosing
    // to load another mobi file while loading of the previous
    // hasn't finished yet
    StopMobiLoadThread();
    mobiLoadThread = new base::Thread("ControlEbook::LoadMobiBackground");
    mobiLoadThread->Start();
    // TODO: use some refcounted version of fileName
    mobiLoadThread->message_loop()->PostTask(base::Bind(&ControlEbook::LoadMobiBackground, 
                                             base::Unretained(this), str::Dup(fileName)));
    // TODO: this message should show up in a different place, 
    // reusing status for convenience
    ScopedMem<TCHAR> s(str::Format(_T("Please wait, loading %s"), fileName));
    status->SetText(s.Get());
}

static Rect RectForCircle(int x, int y, int r)
{
    return Rect(x - r, y - r, r * 2, r * 2);
}

// This is just to test mouse move handling
void ControlEbook::NotifyMouseMove(int x, int y)
{
    Rect r1 = RectForCircle(cursorX, cursorY, CircleR);
    Rect r2 = RectForCircle(x, y, CircleR);
    cursorX = x; cursorY = y;
    r1.Inflate(1,1); r2.Inflate(1,1);
    RequestRepaint(this, &r1, &r2);
}
void ControlEbook::RegisterEventHandlers(EventMgr *evtMgr) 
{
    evtMgr->RegisterClickHandler(next, this);
    evtMgr->RegisterClickHandler(prev, this);
    evtMgr->RegisterClickHandler(horizProgress, this);
    evtMgr->RegisterClickHandler(test, this);
}

void ControlEbook::UnRegisterEventHandlers(EventMgr *evtMgr)
{
    evtMgr->UnRegisterClickHandlers(this);
}

// (x, y) is in the coordinates of w
void ControlEbook::Clicked(Control *w, int x, int y)
{
    if (w == next) {
        AdvancePage(1);
        return;
    }

    if (w == prev) {
        AdvancePage(-1);
        return;
    }

    if (w == test) {
        ScopedMem<TCHAR> s(str::Join(test->text, _T("0")));
        test->SetText(s.Get());
        return;
    }

    if (w == horizProgress) {
        float perc = horizProgress->GetPercAt(x);
        if (pages) {
            int pageCount = pages->Count();
            int pageNo = IntFromPerc(pageCount, perc);
            SetPage(pageNo + 1);
        }
        return;
    }

    CrashAlwaysIf(true);
}

#define TEN_SECONDS_IN_MS 10*1000

static float gUiDPIFactor = 1.0f;
inline int dpiAdjust(int value)
{
    return (int)(value * gUiDPIFactor);
}

static void OnExit()
{
    SendMessage(gHwndFrame, WM_CLOSE, 0, 0);
}

static inline void EnableAndShow(HWND hwnd, bool enable)
{
    ShowWindow(hwnd, enable ? SW_SHOW : SW_HIDE);
    EnableWindow(hwnd, enable);
}

static void DrawPageLayout(Graphics *g, Vec<DrawInstr> *drawInstructions, REAL offX, REAL offY)
{
    StringFormat sf(StringFormat::GenericTypographic());
    SolidBrush br(Color(0,0,0));
    SolidBrush br2(Color(255, 255, 255, 255));
    Pen pen(Color(255, 0, 0), 1);
    Pen blackPen(Color(0, 0, 0), 1);

    FontInfo fi = gFontCache->GetById(0);
    Font *font = fi.font;

    WCHAR buf[512];
    PointF pos;
    DrawInstr *currInstr = &drawInstructions->At(0);
    DrawInstr *end = currInstr + drawInstructions->Count();
    while (currInstr < end) {
        RectF bbox = currInstr->bbox;
        bbox.X += offX;
        bbox.Y += offY;
        if (InstrTypeLine == currInstr->type) {
            // hr is a line drawn in the middle of bounding box
            REAL y = bbox.Y + bbox.Height / 2.f;
            PointF p1(bbox.X, y);
            PointF p2(bbox.X + bbox.Width, y);
            if (gShowTextBoundingBoxes) {
                //g->FillRectangle(&br, bbox);
                g->DrawRectangle(&pen, bbox);
            }
            g->DrawLine(&blackPen, p1, p2);
        } else if (InstrTypeString == currInstr->type) {
            size_t strLen = str::Utf8ToWcharBuf((const char*)currInstr->str.s, currInstr->str.len, buf, dimof(buf));
            bbox.GetLocation(&pos);
            if (gShowTextBoundingBoxes) {
                //g->FillRectangle(&br, bbox);
                g->DrawRectangle(&pen, bbox);
            }
            g->DrawString(buf, strLen, font, pos, NULL, &br);
        } else if (InstrTypeSetFont == currInstr->type) {
            fi = gFontCache->GetById(currInstr->setFont.fontId);
            font = fi.font;
        }
        ++currInstr;
    }
}

void ControlEbook::Paint(Graphics *gfx, int offX, int offY)
{
    // for testing mouse move, paint a blue circle at current cursor position
    if ((-1 != cursorX) && (-1 != cursorY)) {
        SolidBrush br(Color(180, 0, 0, 255));
        int x = offX + cursorX;
        int y = offY + cursorY;
        Rect r(RectForCircle(x, y, CircleR));
        gfx->FillEllipse(&br, r);
    }

    if (!pages)
        return;
    Prop *propPadding = GetCachedProp(PropPadding);
    offX += propPadding->padding.left;
    offY += propPadding->padding.top;

    PageData *pageData = pages->At(currPageNo - 1);
    DrawPageLayout(gfx, &pageData->drawInstructions, (REAL)offX, (REAL)offY);
}

#if 0
static void DrawFrame2(Graphics &g, RectI r)
{
    DrawPage(&g, 0, (REAL)pageBorderX, (REAL)pageBorderY);
    if (gShowTextBoundingBoxes) {
        Pen p(Color(0,0,255), 1);
        g.DrawRectangle(&p, pageBorderX, pageBorderY, r.dx - (pageBorderX * 2), r.dy - (pageBorderY * 2));
    }
}
#endif

static void OnCreateWindow(HWND hwnd)
{
    gControlFrame = new ControlEbook(hwnd);
    gControlFrame->SetHtml(gSampleHtml);
    gControlFrame->SetMinSize(Size(320, 200));
    gControlFrame->SetMaxSize(Size(1024, 800));

    HMENU menu = BuildMenu();
    // triggers OnSize(), so must be called after we
    // have things set up to handle OnSize()
    SetMenu(hwnd, menu);
}

static void OnOpen(HWND hwnd)
{
    OPENFILENAME ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;

    str::Str<TCHAR> fileFilter;
    fileFilter.Append(_T("All supported documents"));

    ofn.lpstrFilter = _T("All supported documents\0;*.mobi;*.awz;\0\0");
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY |
                OFN_ALLOWMULTISELECT | OFN_EXPLORER;
    ofn.nMaxFile = MAX_PATH * 100;
    ScopedMem<TCHAR> file(SAZA(TCHAR, ofn.nMaxFile));
    ofn.lpstrFile = file;

    if (!GetOpenFileName(&ofn))
        return;

    TCHAR *fileName = ofn.lpstrFile + ofn.nFileOffset;
    if (*(fileName - 1)) {
        // special case: single filename without NULL separator
        gControlFrame->LoadMobi(ofn.lpstrFile);
        return;
    }
    // this is just a test app, no need to support multiple files
    CrashIf(true);
}

static void OnToggleBbox(HWND hwnd)
{
    gShowTextBoundingBoxes = !gShowTextBoundingBoxes;
    InvalidateRect(hwnd, NULL, FALSE);
}

static LRESULT OnCommand(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    int wmId = LOWORD(wParam);

    if ((IDM_EXIT == wmId) || (IDCANCEL == wmId)) {
        OnExit();
        return 0;
    }

    if (IDM_OPEN == wmId) {
        OnOpen(hwnd);
        return 0;
    }

    if (IDM_TOGGLE_BBOX == wmId) {
        OnToggleBbox(hwnd);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK WndProcFrame(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (gControlFrame) {
        bool wasHandled;
        LRESULT res = gControlFrame->evtMgr->OnMessage(msg, wParam, lParam, wasHandled);
        if (wasHandled)
            return res;
    }

    switch (msg)
    {
        case WM_CREATE:
            OnCreateWindow(hwnd);
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        // if we return 0, during WM_PAINT we can check
        // PAINTSTRUCT.fErase to see if WM_ERASEBKGND
        // was sent before WM_PAINT
        case WM_ERASEBKGND:
            return 0;

        case WM_PAINT:
            gControlFrame->OnPaint(hwnd);
            break;

        case WM_COMMAND:
            OnCommand(hwnd, msg, wParam, lParam);
            break;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return 0;
}

static BOOL RegisterWinClass(HINSTANCE hInstance)
{
    WNDCLASSEX  wcex;

    FillWndClassEx(wcex, hInstance);
    // clear out CS_HREDRAW | CS_VREDRAW style so that resizing doesn't
    // cause the whole window redraw
    wcex.style = 0;
    wcex.lpszClassName  = ET_FRAME_CLASS_NAME;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SUMATRAPDF));
    wcex.hbrBackground  = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));

    wcex.lpfnWndProc    = WndProcFrame;

    ATOM atom = RegisterClassEx(&wcex);
    return atom != NULL;
}

static BOOL InstanceInit(HINSTANCE hInstance, int nCmdShow)
{
    ghinst = hInstance;
    win::GetHwndDpi(NULL, &gUiDPIFactor);

    gHwndFrame = CreateWindow(
            ET_FRAME_CLASS_NAME, _T("Ebook Test ") CURR_VERSION_STR,
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT,
            dpiAdjust(WIN_DX), dpiAdjust(WIN_DY),
            NULL, NULL,
            ghinst, NULL);

    if (!gHwndFrame)
        return FALSE;

    CenterDialog(gHwndFrame);
    ShowWindow(gHwndFrame, SW_SHOW);

    return TRUE;
}

// inspired by http://engineering.imvu.com/2010/11/24/how-to-write-an-interactive-60-hz-desktop-application/
static int RunApp()
{
    MSG msg;
    FrameTimeoutCalculator ftc(60);
    MillisecondTimer t;
    t.Start();
    for (;;) {
        const DWORD timeout = ftc.GetTimeoutInMilliseconds();
        DWORD res = WAIT_TIMEOUT;
        if (timeout > 0) {
            res = MsgWaitForMultipleObjects(0, 0, TRUE, timeout, QS_ALLEVENTS);
        }
        if (res == WAIT_TIMEOUT) {
            //AnimStep();
            ftc.Step();
        }

        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                return (int)msg.wParam;
            }
            if (!IsDialogMessage(gHwndFrame, &msg)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    int ret = 1;

    SetErrorMode(SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS);

    ScopedCom com;
    InitAllCommonControls();
    ScopedGdiPlus gdi;

    mui::Initialize();

    gCursorHand  = LoadCursor(NULL, IDC_HAND);
    gFontCache = new ThreadSafeFontCache();

    // start per-thread MessageLoop, this one is for our UI thread
    // You can use it via static MessageLoop::current()
    MessageLoop uiMsgLoop(MessageLoop::TYPE_UI);

    gMessageLoopUI = MessageLoop::current();
    CrashIf(gMessageLoopUI != &uiMsgLoop);

    //ParseCommandLine(GetCommandLine());
    if (!RegisterWinClass(hInstance))
        goto Exit;

    if (!InstanceInit(hInstance, nCmdShow))
        goto Exit;

    MessageLoopForUI::current()->RunWithDispatcher(NULL);
    // ret = RunApp();

    delete gControlFrame;

Exit:
    delete gFontCache;
    mui::Destroy();
    return ret;
}
