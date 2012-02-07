/* Copyright 2010-2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "FileUtil.h"
#include "MobiDoc.h"
#include "EpubDoc.h"
#include "Fb2Doc.h"
#include "Mui.h"
#include "Resource.h"
#include "PageLayout.h"
#include "StrUtil.h"
#include "Scoped.h"
#include "Version.h"
#include "WinUtil.h"

using namespace mui;

class ControlEbook;

#define ET_FRAME_CLASS_NAME    _T("ET_FRAME")

#define FONT_NAME              L"Georgia"
#define FONT_SIZE              12

#define WIN_DX    640
#define WIN_DY    480

static HINSTANCE        ghinst;
static HWND             gHwndFrame = NULL;
static ControlEbook *   gControlFrame = NULL;
static HCURSOR          gCursorHand = NULL;

static bool gShowTextBoundingBoxes = false;

// A sample text to display if we don't show an actual ebook
static const char *gSampleHtml = "<html><p align=justify>ClearType is <b>dependent</b> on the <i>orientation &amp; ordering</i> of the LCD stripes.</p> <p align='right'><em>Currently</em>, ClearType is implemented <hr> only for vertical stripes that are ordered RGB.</p> <p align=center>This might be a concern if you are using a tablet PC.</p> <p>Where the display can be oriented in any direction, or if you are using a screen that can be turned from landscape to portrait. The <strike>following example</strike> draws text with two <u>different quality</u> settings.</p> On to the <b>next<mbp:pagebreak>page</b>&#21;</html>";

class ControlEbook
    : public HwndWrapper,
      public IClickHandler,
      public INewPageObserver
{
public:
    BaseEbookDoc *  doc;

    Vec<PageData*>  pages;
    int             currPageNo; // within pages
    int             pageDx, pageDy; // size of the page for which pages was generated

    ScrollBar *     horizProgress;
    Button *        status;

    Style *         ebookDefault;
    Style *         statusDefault;
    Style *         horizProgressDefault;

    ControlEbook(HWND hwnd);
    virtual ~ControlEbook();

    virtual void Paint(Graphics *gfx, int offX, int offY);

    void AdvancePage(int dist);
    void SetPage(int newPageNo);

    void LoadDoc(const TCHAR *fileName);
    void SetStatusText() const;
    void DeletePages();

    void PageLayout(int dx, int dy);

    // IClickHandler
    virtual void Clicked(Control *w, int x, int y);
    // INewPageObserver
    virtual void NewPage(PageData *pageData);
};

class EbookLayout : public Layout
{
public:
    EbookLayout(Control *status, Control *horizProgress) :
        status(status), horizProgress(horizProgress) { }

    virtual ~EbookLayout() { }

    Control *status;
    Control *horizProgress;

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

void EbookLayout::Arrange(const Rect finalRect, Control *wnd)
{
    Prop **props = wnd->GetCachedProps();
    Prop *propPadding = props[PropPadding];
    int padLeft = propPadding->padding.left;
    int padRight = propPadding->padding.right;
    int padTop = propPadding->padding.top;
    int padBottom = propPadding->padding.bottom;

    int rectDy = finalRect.Height - (padTop + padBottom);
    int rectDx = finalRect.Width - (padLeft + padRight);

    // status is at the bottom
    int y = finalRect.Height - status->desiredSize.Height;
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
    doc = NULL;
    pages = NULL;
    currPageNo = 1;
    pageDx = 0; pageDy = 0;
    SetHwnd(hwnd);

    const int pageBorderX = 10;
    const int pageBorderY = 10;

    bit::Set(wantedInputBits, WantsMouseMoveBit);
    ebookDefault = new Style();
    ebookDefault->Set(Prop::AllocPadding(pageBorderY, pageBorderX, pageBorderY, pageBorderX));
    SetCurrentStyle(ebookDefault, gStyleDefault);

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

    horizProgress = new ScrollBar();
    horizProgress->hCursor = gCursorHand;
    status = new Button(_T(""));

    AddChild(horizProgress);
    AddChild(status);
    layout = new EbookLayout(status, horizProgress);

    // unfortuante sequencing issue: some things trigger a repaint which
    // only works if the control has been placed in the control tree
    // via AddChild()
    horizProgress->SetCurrentStyle(horizProgressDefault, gStyleDefault);
    status->SetStyles(statusDefault, statusDefault);
}

ControlEbook::~ControlEbook()
{
    DeletePages();
    delete doc;
    delete statusDefault;
    delete horizProgressDefault;
    delete ebookDefault;
}

void ControlEbook::DeletePages()
{
    DeleteVecMembers(pages);
    pages.Reset();
}

void ControlEbook::SetStatusText() const
{
    if (pages.Count() == 0) {
        status->SetText(_T(""));
        horizProgress->SetFilled(0.f);
        return;
    }
    ScopedMem<TCHAR> s(str::Format(_T("Page %d out of %d"), currPageNo, (int)pages.Count()));
    status->SetText(s.Get());
    horizProgress->SetFilled(PercFromInt(pages.Count(), currPageNo));
}

void ControlEbook::SetPage(int newPageNo)
{
    CrashIf((newPageNo < 1) || (newPageNo > (int)pages.Count()));
    currPageNo = newPageNo;
    SetStatusText();
    RequestRepaint(this);
}

void ControlEbook::AdvancePage(int dist)
{
    int newPageNo = currPageNo + dist;
    if (newPageNo < 1)
        return;
    if (newPageNo > (int)pages.Count())
        return;
    SetPage(newPageNo);
}

void ControlEbook::NewPage(PageData *pageData)
{
    pages.Append(pageData);
}

void ControlEbook::PageLayout(int dx, int dy)
{
    if ((dx == pageDx) && (dy == pageDy) && pages.Count() > 0)
        return;

    DeletePages();
    pageDx = dx;
    pageDy = dy;

    LayoutInfo *li = new LayoutInfo();

    if (doc) {
        li->htmlStr = doc->GetBookHtmlData(li->htmlStrLen);
    }
    else {
        li->htmlStr = gSampleHtml;
        li->htmlStrLen = strlen(gSampleHtml);
    }

    li->fontName = FONT_NAME;
    li->fontSize = FONT_SIZE;
    li->pageDx = dx;
    li->pageDy = dy;
    li->observer = this;

    Vec<PageData *> *pageData = LayoutHtml(li);
    pages.Append(pageData->Last());
    delete pageData;

    delete li;
}

void ControlEbook::LoadDoc(const TCHAR *fileName)
{
    // TODO: this message should show up in a different place,
    // reusing status for convenience
    ScopedMem<TCHAR> s(str::Format(_T("Please wait, loading %s"), fileName));
    status->SetText(s.Get());

    BaseEbookDoc *doc = NULL;
    if (EpubDoc::IsSupported(fileName))
        doc = EpubDoc::ParseFile(fileName);
    else if (Fb2Doc::IsSupported(fileName))
        doc = Fb2Doc::ParseFile(fileName);
    else // if (MobiDoc2::IsSupported(fileName))
        doc = MobiDoc2::ParseFile(fileName);

    if (!doc) {
        // TODO: this message should show up in a different place,
        // reusing status for convenience
        ScopedMem<TCHAR> s(str::Format(_T("Failed to load %s!"), fileName));
        status->SetText(s.Get());
    } else {
        delete this->doc;
        this->doc = doc;
        DeleteVecMembers(pages);
        pages.Reset();
        RequestLayout();
    }
}

// (x, y) is in the coordinates of w
void ControlEbook::Clicked(Control *w, int x, int y)
{
    if (w == horizProgress) {
        float perc = horizProgress->GetPercAt(x);
        if (pages.Count() > 0) {
            int pageNo = IntFromPerc(pages.Count(), perc);
            SetPage(pageNo + 1);
        }
    } else
        CrashAlwaysIf(true);
}

static void OnExit()
{
    SendMessage(gHwndFrame, WM_CLOSE, 0, 0);
}

void ControlEbook::Paint(Graphics *gfx, int offX, int offY)
{
    if (pages.Count() == 0)
        return;

    Prop *propPadding = GetCachedProp(PropPadding);
    offX += propPadding->padding.left;
    offY += propPadding->padding.top;

    if (currPageNo > (int)pages.Count())
        currPageNo = pages.Count();

    PageData *pageData = pages.At(currPageNo - 1);
    DrawPageLayout(gfx, &pageData->drawInstructions, (REAL)offX, (REAL)offY, gShowTextBoundingBoxes);
}

#define SEP_ITEM "-----"

struct MenuDef {
    const char *title;
    int         id;
};

MenuDef menuDefFile[] = {
    { "&Open",              IDM_OPEN },
    { "Toggle bbox",        IDM_TOGGLE_BBOX },
    { SEP_ITEM,             0,       },
    { "E&xit",              IDM_EXIT }
};

HMENU BuildMenuFromMenuDef(MenuDef menuDefs[], int menuLen, HMENU menu)
{
    assert(menu);

    for (int i = 0; i < menuLen; i++) {
        MenuDef md = menuDefs[i];
        const char *title = md.title;

        if (str::Eq(title, SEP_ITEM)) {
            AppendMenu(menu, MF_SEPARATOR, 0, NULL);
        } else {
            ScopedMem<TCHAR> tmp(str::conv::FromUtf8(title));
            AppendMenu(menu, MF_STRING, (UINT_PTR)md.id, tmp);
        }
    }

    return menu;
}

HMENU BuildMenu()
{
    HMENU mainMenu = CreateMenu();
    HMENU m;
    m = BuildMenuFromMenuDef(menuDefFile, dimof(menuDefFile), CreateMenu());
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)m, _T("&File"));
    return mainMenu;
}

static void OnCreateWindow(HWND hwnd)
{
    gControlFrame = new ControlEbook(hwnd);
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

    ofn.lpstrFilter = _T("All supported documents\0;*.mobi;*.awz;*.prc;*.epub;*.fb2;*.fb2.zip\0\0");
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
        gControlFrame->LoadDoc(ofn.lpstrFile);
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
    switch (LOWORD(wParam)) {
    case IDM_EXIT: OnExit(); break;
    case IDM_OPEN: OnOpen(hwnd); break;
    case IDM_TOGGLE_BBOX: OnToggleBbox(hwnd); break;
    default: return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

static LRESULT OnKeyDown(HWND hwnd, UINT msg, WPARAM key, LPARAM lParam)
{
    switch (key) {
    case VK_LEFT: gControlFrame->AdvancePage(-1); break;
    case VK_RIGHT: gControlFrame->AdvancePage(1); break;
    case 'O': OnOpen(hwnd); break;
    default: return DefWindowProc(hwnd, msg, key, lParam);
    }
    return 0;
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
    case WM_CREATE: OnCreateWindow(hwnd); break;
    case WM_DESTROY: PostQuitMessage(0); break;
    // if we return 0, during WM_PAINT we can check
    // PAINTSTRUCT.fErase to see if WM_ERASEBKGND
    // was sent before WM_PAINT
    case WM_ERASEBKGND: break;
    case WM_PAINT: gControlFrame->OnPaint(hwnd); break;
    case WM_COMMAND: OnCommand(hwnd, msg, wParam, lParam); break;
    case WM_KEYDOWN: return OnKeyDown(hwnd, msg, wParam, lParam);
    default: return DefWindowProc(hwnd, msg, wParam, lParam);
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

    gHwndFrame = CreateWindow(
            ET_FRAME_CLASS_NAME, _T("Ebook Test ") CURR_VERSION_STR,
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT,
            WIN_DX, WIN_DY,
            NULL, NULL,
            ghinst, NULL);

    if (!gHwndFrame)
        return FALSE;

    CenterDialog(gHwndFrame);
    ShowWindow(gHwndFrame, SW_SHOW);

    return TRUE;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
#ifdef DEBUG
    // report memory leaks on DbgOut
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    MSG msg;
    msg.wParam = 1;

    SetErrorMode(SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS);

    ScopedCom com;
    InitAllCommonControls();
    ScopedGdiPlus gdi;

    mui::Initialize();

    gCursorHand  = LoadCursor(NULL, IDC_HAND);

    if (!RegisterWinClass(hInstance))
        goto Exit;

    if (!InstanceInit(hInstance, nCmdShow))
        goto Exit;

    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    delete gControlFrame;

Exit:
    mui::Destroy();
    return msg.wParam;
}
