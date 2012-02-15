/* Copyright 2010-2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Hack: we need NOMINMAX to make chrome code compile but we also need
// min/max for gdi+ headers, so we import min/max from stl
#include <algorithm>
using std::min;
using std::max;

#if defined(WITH_CHROME)
// include anything that might (re)define operator new before BaseUtil.h
#include "base/threading/thread.h"
#include "base/bind.h"
#else
#include "ThreadUtil.h"
#endif

#include "CmdLineParser.h"
#include "EbookTestMenu.h"
#include "FileUtil.h"
#include "FrameTimeoutCalculator.h"
#include "MobiDoc.h"
#include "Mui.h"
#include "NoFreeAllocator.h"
#include "Resource.h"
#include "PageLayout.h"
#include "SvgPath.h"
#include "StrUtil.h"
#include "Scoped.h"
#include "Timer.h"
#include "Version.h"
#include "WinUtil.h"

#define NOLOG defined(NDEBUG)
#include "DebugLog.h"

/*
TODO: doing page layout on a background thread needs to be more sophisticated.
Technically it works but user experience is bad.

The key to supporting fluid resizing is to limit the amount of work done on UI thread.
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

using namespace mui;

class ControlEbook;
class EbookController;

#define ET_FRAME_CLASS_NAME    _T("ET_FRAME")

#define FONT_NAME              L"Georgia"
#define FONT_SIZE              12

#define WIN_DX    640
#define WIN_DY    480

static HINSTANCE        ghinst;
static HWND             gHwndFrame = NULL;
static HCURSOR          gCursorHand = NULL;

static Style *         ebookDefault = NULL;
static Style *         statusDefault = NULL;
static Style *         horizProgressDefault = NULL;
static Style *         nextDefault = NULL;
static Style *         nextMouseOver = NULL;

static HwndWrapper *   gMainWnd = NULL;
static ControlEbook *  ebook = NULL;
static ButtonVector *  next = NULL;
static ButtonVector *  prev = NULL;
static ScrollBar *     horizProgress = NULL;
static Button *        status = NULL;
static EbookController *ebookController = NULL;

#if defined(WITH_CHROME)
// for convenience so that we don't have to pass it around
static MessageLoop *    gMessageLoopUI = NULL;
#else
static UiMessageLoop *  gMessageLoopUI = NULL;
#endif

static bool gShowTextBoundingBoxes = false;

// A sample text to display if we don't show an actual mobi file
static const char *gSampleHtml = "<html><p align=justify>ClearType is <b>dependent</b> on the <i>orientation &amp; ordering</i> of the LCD stripes.</p> <p align='right'><em>Currently</em>, ClearType is implemented <hr> only for vertical stripes that are ordered RGB.</p> <p align=center>This might be a concern if you are using a tablet PC.</p> <p>Where the display can be oriented in any direction, or if you are using a screen that can be turned from landscape to portrait. The <strike>following example</strike> draws text with two <u>different quality</u> settings.</p> On to the <b>next<mbp:pagebreak>page</b></html>";

void LogProcessRunningTime()
{
    lf("EbookTest startup time: %.2f ms", GetProcessRunningTime());
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

#if 0
// Keeps laid out pages for given dimensions
struct Pages {
    int             pageDx;
    int             pageDy;
    Vec<PageData*>  pages;
};
#endif

// I'm lazy, EbookLayout uses global variables of known controls
class EbookLayout : public ILayout
{
    //HorizontalLayout top;
    Size desiredSize;
public:
    EbookLayout()
    {
        //top.Add(next).Add(prev);
    }

    virtual ~EbookLayout() {
    }

    virtual void Measure(const Size availableSize);
    virtual void Arrange(const Rect finalRect);
    virtual Size DesiredSize() {
        return desiredSize;
    }
};

void EbookLayout::Measure(const Size availableSize)
{
    Size s(availableSize);
    if (SizeInfinite == s.Width)
        s.Width = 320;
    if (SizeInfinite == s.Height)
        s.Height = 200;
    desiredSize = s;

    next->Measure(s);
    prev->Measure(s);
    // we don't need to measure ebook
    status->Measure(s);
    horizProgress->Measure(s);
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

void EbookLayout::Arrange(const Rect finalRect)
{
    int y, dx;

    int rectDy = finalRect.Height;
    int rectDx = finalRect.Width;

    // prev is on the left, y-middle
    Rect prevPos(Point(0, 0), prev->DesiredSize());
    CenterRectY(prevPos, Size(rectDx, rectDy));
    prev->Arrange(prevPos);

    // next is on the right, y-middle
    dx = next->DesiredSize().Width;
    Rect nextPos(Point(rectDx - dx, 0), next->DesiredSize());
    CenterRectY(nextPos, Size(rectDx, rectDy));
    next->Arrange(nextPos);

    // ebook is between prev and next
    Size ebookSize(rectDx - nextPos.Width - prevPos.Width, rectDy - status->DesiredSize().Height - horizProgress->DesiredSize().Height);
    Rect ebookPos(Point(prevPos.Width, 0), ebookSize);
    ((Control*)ebook)->Arrange(ebookPos);

    // status is at the bottom
    y = finalRect.Height - status->DesiredSize().Height;
    Rect statusPos(Point(0, y), status->DesiredSize());
    statusPos.Width = finalRect.Width;
    status->Arrange(statusPos);

    // horizProgress is at the bottom, right above the status
    y -= horizProgress->DesiredSize().Height;
    Rect horizPos(Point(0, y), horizProgress->DesiredSize());
    horizPos.Width = finalRect.Width;
    horizProgress->Arrange(horizPos);
}

class ControlEbook : public Control
{
    static const int CircleR = 10;

public:
    MobiDoc *       mb;
    const char *    html;

    Vec<PageData*>* pages;
    int             currPageNo; // within pages
    int             pageDx, pageDy; // size of the page for which pages was generated

    // pages 
    Vec<PageData*>* tmpPages;

    int             cursorX, cursorY;

    base::Thread *  mobiLoadThread;
    base::Thread *  pageLayoutThread;

    ControlEbook();
    virtual ~ControlEbook();

    virtual void Paint(Graphics *gfx, int offX, int offY);

    virtual void NotifyMouseMove(int x, int y);

    void SetHtml(const char *html);
    void LoadMobi(const TCHAR *fileName);

    void SetStatusText() const;

    void DeletePages();
    void DeleteNewPages();

    void NewPageUIThread(PageData *pageData);
    void PageLayoutFinished();
    void PageLayoutBackground(LayoutInfo *li);

    void PageLayout(int dx, int dy);
    void StopPageLayoutThread();

    void MobiLoaded(MobiDoc *mb);
    void MobiFailedToLoad(const TCHAR *fileName);
    void LoadMobiBackground(const TCHAR *fileName);
    void StopMobiLoadThread();

    void AdvancePage(int dist);
    void SetPage(int newPageNo);
};

ControlEbook::ControlEbook()
{
    mobiLoadThread = NULL;
    pageLayoutThread = NULL;
    mb = NULL;
    html = NULL;
    pages = NULL;
    currPageNo = 0;
    pageDx = 0; pageDy = 0;

    cursorX = -1; cursorY = -1;

    bit::Set(wantedInputBits, WantsMouseMoveBit);

    SetStatusText();
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

    DeleteNewPages();
    DeletePages();
    delete mb;
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
#if 0
    if (!newPages)
        return;
    DeleteVecMembers<PageData*>(*newPages);
    delete newPages;
    newPages = NULL;
#endif
}

void ControlEbook::SetStatusText() const
{
    if (!pages) {
        status->SetText(_T(" "));
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
#if 0
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
#endif
}

// called on a ui thread from background thread
void ControlEbook::NewPageUIThread(PageData *pageData)
{
#if 0
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
#endif
}

#if 0
// called on a background thread
void ControlEbook::NewPage(PageData *pageData)
{
    gMessageLoopUI->PostDelayedTask(base::Bind(&ControlEbook::NewPageUIThread,
                                        base::Unretained(this), pageData), 100);
}
#endif

// called on a background thread
void ControlEbook::PageLayoutBackground(LayoutInfo *li)
{
    Vec<PageData*> *newPages = LayoutHtml(li);
    delete newPages;
    gMessageLoopUI->PostTask(base::Bind(&ControlEbook::PageLayoutFinished,
                                        base::Unretained(this)));
    delete li;
}

void ControlEbook::PageLayout(int dx, int dy)
{
    lf("ControlEbook::PageLayout: (%d,%d)", dx, dy);
    if ((dx == pageDx) && (dy == pageDy) && pages)
        return;

    if (pages || pageLayoutThread)
        return;

    StopPageLayoutThread();

    DeleteNewPages();
    pageDx = dx;
    pageDy = dy;

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
    pages = LayoutHtml(li);
    delete li;
#if 0
    pageLayoutThread = new base::Thread("ControlEbook::PageLayoutBackground");
    pageLayoutThread->Start();
    pageLayoutThread->message_loop()->PostTask(base::Bind(&ControlEbook::PageLayoutBackground,
                                             base::Unretained(this), li));
#endif
}

void ControlEbook::StopPageLayoutThread()
{
    if (!pageLayoutThread)
        return;
    l("ControlEbook::StopPageLayoutThread()");
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
    l("Stopping mobi load thread");
    mobiLoadThread->Stop();
    delete mobiLoadThread;
    mobiLoadThread = NULL;
}

// called on UI thread from background thread after
// mobi file has been loaded
void ControlEbook::MobiLoaded(MobiDoc *newMobi)
{
    l("ControlEbook::MobiLoaded()");
    CrashIf(gMessageLoopUI != MessageLoop::current());
    StopMobiLoadThread();
    delete mb;
    mb = newMobi;
    html = NULL;
    delete pages;
    pages = NULL;
    RequestLayout(this);
}

// called on UI thread from backgroudn thread if we tried
// to load mobi file but failed
void ControlEbook::MobiFailedToLoad(const TCHAR *fileName)
{
    l("ControlEbook::MobiFailedToLoad()");
    CrashIf(gMessageLoopUI != MessageLoop::current());
    StopMobiLoadThread();
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
    lf(_T("ControlEbook::LoadMobiBackground(%s)"), fileName);
    Timer t(true);
    MobiDoc *mb = MobiDoc::ParseFile(fileName);
    lf(_T("Loaded %s in %.2f ms"), fileName, t.GetCurrTimeInMs());

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

    // TODO: temporary, to prevent crash below
    if (currPageNo == 0)
        return;

    offX += cachedStyle->padding.left;
    offY += cachedStyle->padding.top;

    PageData *pageData = pages->At(currPageNo - 1);
    DrawPageLayout(gfx, &pageData->drawInstructions, (REAL)offX, (REAL)offY, gShowTextBoundingBoxes);
}

// a feeble attempt at MVC split
class EbookController : public IClickHandler
{
public:
    EbookController() {
        gMainWnd->evtMgr->RegisterClickHandler(next, this);
        gMainWnd->evtMgr->RegisterClickHandler(prev, this);
        gMainWnd->evtMgr->RegisterClickHandler(horizProgress, this);
    }
    ~EbookController() {
        gMainWnd->evtMgr->UnRegisterClickHandlers(this);
    }

    // IClickHandler
    virtual void Clicked(Control *w, int x, int y);
};

// (x, y) is in the coordinates of w
void EbookController::Clicked(Control *w, int x, int y)
{
    if (w == next) {
        ebook->AdvancePage(1);
        return;
    }

    if (w == prev) {
        ebook->AdvancePage(-1);
        return;
    }

    if (w == horizProgress) {
        float perc = horizProgress->GetPercAt(x);
        if (ebook->pages) {
            int pageCount = ebook->pages->Count();
            int pageNo = IntFromPerc(pageCount, perc);
            ebook->SetPage(pageNo + 1);
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

static void CreateStyles()
{
    const int pageBorderX = 10;
    const int pageBorderY = 10;

    ebookDefault = new Style();
    ebookDefault->Set(Prop::AllocPadding(pageBorderY, pageBorderX, pageBorderY, pageBorderX));

    nextDefault = new Style(gStyleButtonDefault);
    nextDefault->SetBorderWidth(0.f);
    nextDefault->Set(Prop::AllocPadding(1, 1, 1, 4));
    nextDefault->Set(Prop::AllocWidth(PropStrokeWidth, 0.f));
    nextDefault->Set(Prop::AllocColorSolid(PropFill, "gray"));
    nextDefault->Set(Prop::AllocColorSolid(PropBgColor, "transparent"));
    //nextDefault->Set(Prop::AllocPadding(4, 8, 12, 16));
    nextMouseOver = new Style(gStyleButtonMouseOver);
    nextMouseOver->SetBorderWidth(0.f);
    //nextMouseOver->Set(Prop::AllocPadding(12, 8, 4, 16));
    nextMouseOver->Set(Prop::AllocPadding(1, 1, 1, 4));
    nextMouseOver->Set(Prop::AllocWidth(PropStrokeWidth, 0.f));
    nextMouseOver->Set(Prop::AllocColorSolid(PropFill, "black"));
    nextMouseOver->Set(Prop::AllocColorSolid(PropBgColor, "transparent"));

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

static void CreateWindows(HWND hwnd)
{
    next = new ButtonVector(svg::GraphicsPathFromPathData("M0 0  L10 13 L0 ,26 Z"));
    next->SetStyles(nextDefault, nextMouseOver);

    prev = new ButtonVector(svg::GraphicsPathFromPathData("M10 0 L0,  13 L10 26 z"));
    prev->SetStyles(nextDefault, nextMouseOver);

    horizProgress = new ScrollBar();
    horizProgress->hCursor = gCursorHand;
    horizProgress->SetCurrentStyle(horizProgressDefault, gStyleDefault);

    status = new Button(_T(""));
    status->SetStyles(statusDefault, statusDefault);

    ebook = new ControlEbook();
    ebook->SetHtml(gSampleHtml);
    ebook->SetCurrentStyle(ebookDefault, gStyleDefault);

    gMainWnd = new HwndWrapper(hwnd);
    gMainWnd->SetMinSize(Size(320, 200));
    gMainWnd->SetMaxSize(Size(1024, 800));

    gMainWnd->AddChild(next, prev, ebook);
    gMainWnd->AddChild(horizProgress, status);

    gMainWnd->layout = new EbookLayout();

    ebookController = new EbookController();
}

static void OnCreateWindow(HWND hwnd)
{
    HMENU menu = BuildMenu();
    SetMenu(hwnd, menu);
    CreateWindows(hwnd);
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
        ebook->LoadMobi(ofn.lpstrFile);
        return;
    }
    // this is just a test app, no need to support multiple files
    CrashIf(true);
}

static void OnToggleBbox(HWND hwnd)
{
    gShowTextBoundingBoxes = !gShowTextBoundingBoxes;
    SetDebugPaint(gShowTextBoundingBoxes);
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
    static bool seenWmPaint = false;

    if (gMainWnd) {
        bool wasHandled;
        LRESULT res = gMainWnd->evtMgr->OnMessage(msg, wParam, lParam, wasHandled);
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
            if (!seenWmPaint) {
                LogProcessRunningTime();
                seenWmPaint = true;
            }
            gMainWnd->OnPaint(hwnd);
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
    Timer t(true);
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
    LogProcessRunningTime();

#ifdef DEBUG
    // report memory leaks on DbgOut
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    SetErrorMode(SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS);

    ScopedCom com;
    InitAllCommonControls();
    ScopedGdiPlus gdi;

    mui::Initialize();
    CreateStyles();

    gCursorHand  = LoadCursor(NULL, IDC_HAND);

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

    delete ebookController;
    delete gMainWnd;
Exit:
    DeleteStyles();
    mui::Destroy();
    return ret;
}
