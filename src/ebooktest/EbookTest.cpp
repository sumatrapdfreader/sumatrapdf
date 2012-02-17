/* Copyright 2010-2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

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
#include "ThreadUtil.h"
#include "Timer.h"
#include "EbookUiMsg.h"
#include "UiMsg.h"
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

class ControlEbook : public Control
{
    static const int CircleR = 10;

public:
    MobiDoc *       mb;
    const char *    html;

    Vec<PageData*>* pages;
    int             currPageNo; // within pages
    int             pageDx, pageDy; // size of the page for which pages was generated

    int             cursorX, cursorY;

    ControlEbook();
    virtual ~ControlEbook();

    virtual void Paint(Graphics *gfx, int offX, int offY);

    virtual void NotifyMouseMove(int x, int y);

    void SetHtml(const char *html);
    void LoadMobi(const TCHAR *fileName);

    void SetStatusText() const;

    void DeletePages();

    void PageLayout(int dx, int dy);
    //void StopPageLayoutThread();

    void MobiFinishedLoading(UiMsg *msg);

    void AdvancePage(int dist);
    void SetPage(int newPageNo);
};

ControlEbook::ControlEbook()
{
    //mobiLoadThread = NULL;
    //pageLayoutThread = NULL;
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
#if 0
    // TODO: I think that the problem here is that while the thread might
    // be finished, there still might be in-flight messages sent
    // to this object. Do we need a way to cancel messages directed
    // to a given object from the queue? Ref-count the objects so that
    // their lifetime is managed correctly?
    StopMobiLoadThread();
    StopPageLayoutThread();
#endif

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

#if 0
void ControlEbook::DeleteNewPages()
{
    if (!newPages)
        return;
    DeleteVecMembers<PageData*>(*newPages);
    delete newPages;
    newPages = NULL;
}
#endif

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

#if 0
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
#endif

#if 0
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
#endif

#if 0
// called on a background thread
void ControlEbook::NewPage(PageData *pageData)
{
    gMessageLoopUI->PostDelayedTask(base::Bind(&ControlEbook::NewPageUIThread,
                                        base::Unretained(this), pageData), 100);
}
#endif

#if 0
// called on a background thread
void ControlEbook::PageLayoutBackground(LayoutInfo *li)
{
    Vec<PageData*> *newPages = LayoutHtml(li);
    delete newPages;
    gMessageLoopUI->PostTask(base::Bind(&ControlEbook::PageLayoutFinished,
                                        base::Unretained(this)));
    delete li;
}
#endif

void ControlEbook::PageLayout(int dx, int dy)
{
    lf("ControlEbook::PageLayout: (%d,%d)", dx, dy);
    if ((dx == pageDx) && (dy == pageDy) && pages)
        return;

    if (pages)
        return;

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

#if 0
void ControlEbook::StopPageLayoutThread()
{
    if (!pageLayoutThread)
        return;
    l("ControlEbook::StopPageLayoutThread()");
    pageLayoutThread->Stop();
    delete pageLayoutThread;
    pageLayoutThread = NULL;
}
#endif

void ControlEbook::SetHtml(const char *newHtml)
{
    html = newHtml;
}

void ControlEbook::MobiFinishedLoading(UiMsg *msg)
{
    CrashIf(UiMsg::FinishedMobiLoading != msg->type);
    delete mb;
    html = NULL;
    if (NULL == msg->finishedMobiLoading.mobiDoc) {
        l("ControlEbook::MobiFinishedLoading(): failed to load");
        // TODO: a better way to notify about this
        ScopedMem<TCHAR> s(str::Format(_T("Failed to load %s!"), msg->finishedMobiLoading.fileName));
        status->SetText(s.Get());
    } else {
        delete pages;
        pages = NULL;
        mb = msg->finishedMobiLoading.mobiDoc;
        msg->finishedMobiLoading.mobiDoc = NULL;
        RequestLayout(this);
    }
}

// TODO: embed ControlEbook object to notify when finished, we use the current one now.
class ThreadLoadMobi : public ThreadBase {
public:
    TCHAR *     fileName; // thread owns this memory

    ThreadLoadMobi(const TCHAR *fileName);
    virtual ~ThreadLoadMobi() { free(fileName); }

    // ThreadBase
    virtual void Run();
};

ThreadLoadMobi::ThreadLoadMobi(const TCHAR *fn)
{
    autoDeleteSelf = true;
    fileName = str::Dup(fn);
}

void ThreadLoadMobi::Run()
{
    lf(_T("ControlEbook::LoadMobiBackground(%s)"), fileName);
    Timer t(true);
    MobiDoc *mobiDoc = MobiDoc::ParseFile(fileName);
    double loadingTimeMs = t.GetCurrTimeInMs();
    lf(_T("Loaded %s in %.2f ms"), fileName, t.GetCurrTimeInMs());

    UiMsg *msg = new UiMsg(UiMsg::FinishedMobiLoading);
    msg->finishedMobiLoading.mobiDoc = mobiDoc;
    msg->finishedMobiLoading.fileName = fileName;
    fileName = NULL;
    uimsg::Post(msg);
}

void ControlEbook::LoadMobi(const TCHAR *fileName)
{
    // note: ThreadLoadMobi object will get automatically deleted, so no
    // need to keep it aroun
    ThreadLoadMobi *loadThread = new ThreadLoadMobi(fileName);
    loadThread->Start();

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
    nextDefault->Set(Prop::AllocAlign(PropVertAlign, ElAlignTop));
    //nextDefault->Set(Prop::AllocPadding(4, 8, 12, 16));

    nextMouseOver = new Style(gStyleButtonMouseOver);
    nextMouseOver->SetBorderWidth(0.f);
    //nextMouseOver->Set(Prop::AllocPadding(12, 8, 4, 16));
    nextMouseOver->Set(Prop::AllocPadding(1, 1, 1, 4));
    nextMouseOver->Set(Prop::AllocWidth(PropStrokeWidth, 0.f));
    nextMouseOver->Set(Prop::AllocColorSolid(PropFill, "black"));
    nextMouseOver->Set(Prop::AllocColorSolid(PropBgColor, "transparent"));
    nextMouseOver->Set(Prop::AllocAlign(PropVertAlign, ElAlignBottom));

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
}

static void CreateLayout()
{
    HorizontalLayout *topPart = new HorizontalLayout();
    DirectionalLayoutData ld;
    ld.Set(prev, SizeSelf, 1.f, GetElAlignCenter());
    topPart->Add(ld);
    ld.Set(ebook, 1.f, 1.f, GetElAlignTop());
    topPart->Add(ld);
    ld.Set(next, SizeSelf, 1.f, GetElAlignBottom());
    topPart->Add(ld);

    VerticalLayout *l = new VerticalLayout();
    ld.Set(topPart, 1.f, 1.f, GetElAlignTop());
    l->Add(ld, true);
    ld.Set(horizProgress, SizeSelf, .5f, GetElAlignRight());
    l->Add(ld);
    ld.Set(status, SizeSelf, .5f, GetElAlignLeft());
    l->Add(ld);
    gMainWnd->layout = l;
}

static void OnCreateWindow(HWND hwnd)
{
    HMENU menu = BuildMenu();
    SetMenu(hwnd, menu);
    CreateWindows(hwnd);
    CreateLayout();
    ebookController = new EbookController();
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

void DispatchUiMsg(UiMsg *msg)
{
    if (UiMsg::FinishedMobiLoading == msg->type) {
        ebook->MobiFinishedLoading(msg);
    } else {
        CrashIf(true);
    }
    delete msg;
}

void DrainUiMsgQueu()
{
    for (UiMsg *msg = uimsg::RetriveNext(); msg; msg = uimsg::RetriveNext()) {
        delete msg;
    }
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
            for (UiMsg *msg = uimsg::RetriveNext(); msg; msg = uimsg::RetriveNext()) {
                DispatchUiMsg(msg);
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

#if 0
    // start per-thread MessageLoop, this one is for our UI thread
    // You can use it via static MessageLoop::current()
    MessageLoop uiMsgLoop(MessageLoop::TYPE_UI);
    gMessageLoopUI = MessageLoop::current();
    CrashIf(gMessageLoopUI != &uiMsgLoop);
#endif

    //ParseCommandLine(GetCommandLine());
    if (!RegisterWinClass(hInstance))
        goto Exit;

    if (!InstanceInit(hInstance, nCmdShow))
        goto Exit;

    uimsg::Initialize();
    //MessageLoopForUI::current()->RunWithDispatcher(NULL);
    ret = RunApp();

    delete ebookController;
    delete gMainWnd;
Exit:
    DeleteStyles();
    mui::Destroy();

    DrainUiMsgQueu();
    uimsg::Destroy();
    return ret;
}
