/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "Scoped.h"
#include "StrUtil.h"
#include "WinUtil.h"
#include "GeomUtil.h"

#include "Resource.h"
#include "Version.h"

#include "MobiDoc.h"
#include "EpubDoc.h"
#include "Fb2Doc.h"
#include "PageLayout.h"

#define ET_FRAME_CLASS_NAME    _T("ET_FRAME")
#define ET_FRAME_TITLE         _T("Ebook Test ") CURR_VERSION_STR

#define FONT_NAME              L"Georgia"
#define FONT_SIZE              12
#define PAGE_BORDER            10

#define WIN_DX    640
#define WIN_DY    480

// A sample text to display if we don't show an actual ebook
static const char *gSampleHtml = "<html><p align=justify>ClearType is <b>dependent</b> on the <i>orientation &amp; ordering</i> of the LCD stripes.</p> <p align='right'><em>Currently</em>, ClearType is implemented <hr> only for vertical stripes that are ordered RGB.</p> <p align=center>This might be a concern if you are using a tablet PC.</p> <p>Where the display can be oriented in any direction, or if you are using a screen that can be turned from landscape to portrait. The <strike>following example</strike> draws text with two <u>different quality</u> settings.</p> On to the <b>next<mbp:pagebreak>page</b>&#21;</html>";

class ControlEbook :  public INewPageObserver {
    BaseEbookDoc *  doc;
    Vec<PageData*>  pages;
    FontCache       fontCache;

    HWND            hwnd;

    int             currPageNo;
    SizeI           currDim;
    bool            showBboxes;

    void DeletePages() {
        DeleteVecMembers(pages);
        pages.Reset();
    }

public:

    ControlEbook(HWND hwnd) : hwnd(hwnd), doc(NULL), currPageNo(1), showBboxes(false) {
    }
    virtual ~ControlEbook() {
        DeletePages();
        delete doc;
    }

    void LoadDoc(const TCHAR *fileName);

    void SetStatusText(const TCHAR *text);
    void GoToPage(int newPageNo);
    void AdvancePage(int dist);

    void PageLayout(SizeI dim);
    void Repaint();
    void OnPaint();

    void ToggleShowBBoxes() { showBboxes = !showBboxes; }

    // INewPageObserver
    virtual void NewPage(PageData *pageData) { pages.Append(pageData); }
};

void ControlEbook::SetStatusText(const TCHAR *text)
{
    ScopedMem<TCHAR> s(str::Format(_T("%s - %s"), ET_FRAME_TITLE, text));
    win::SetText(hwnd, s);
}

void ControlEbook::GoToPage(int newPageNo)
{
    if (newPageNo < 1 || newPageNo > (int)pages.Count())
        return;

    currPageNo = newPageNo;
    Repaint();

    ScopedMem<TCHAR> s(str::Format(_T("Page %d out of %d"), currPageNo, (int)pages.Count()));
    SetStatusText(s);
}

void ControlEbook::AdvancePage(int dist)
{
    int newPageNo = currPageNo + dist;
    GoToPage(limitValue(newPageNo, 1, (int)pages.Count()));
}

void ControlEbook::PageLayout(SizeI dim)
{
    if (dim == currDim && pages.Count() > 0)
        return;
    currDim = dim;

    LayoutInfo li;
    if (doc) {
        li.htmlStr = doc->GetBookHtmlData(li.htmlStrLen);
    }
    else {
        li.htmlStr = gSampleHtml;
        li.htmlStrLen = strlen(gSampleHtml);
    }
    li.fontName = FONT_NAME;
    li.fontSize = FONT_SIZE;
    li.pageDx = dim.dx - 2 * PAGE_BORDER;
    li.pageDy = dim.dy - 2 * PAGE_BORDER;
    li.observer = this;

    DeletePages();
    LayoutHtml(&li, &fontCache);
}

void ControlEbook::Repaint()
{
    InvalidateRect(hwnd, NULL, TRUE);
    UpdateWindow(hwnd);
}

void ControlEbook::LoadDoc(const TCHAR *fileName)
{
    BaseEbookDoc *newDoc = NULL;
    if (EpubDoc::IsSupported(fileName))
        newDoc = EpubDoc::ParseFile(fileName);
    else if (Fb2Doc::IsSupported(fileName))
        newDoc = Fb2Doc::ParseFile(fileName);
    else if (MobiDoc2::IsSupported(fileName))
        newDoc = MobiDoc2::ParseFile(fileName);

    if (newDoc) {
        delete doc;
        doc = newDoc;
        DeletePages();

        PageLayout(ClientRect(hwnd).Size());
        GoToPage(limitValue(currPageNo, 1, (int)pages.Count()));
    } else {
        ScopedMem<TCHAR> s(str::Format(_T("Failed to load %s!"), fileName));
        SetStatusText(s);
    }
}

void ControlEbook::OnPaint()
{
    ClientRect r(hwnd);
    DoubleBuffer buf(hwnd, r);
    FillRect(buf.GetDC(), &r.ToRECT(), GetStockBrush(WHITE_BRUSH));

    if (pages.Count() > 0) {
        if (currPageNo > (int)pages.Count())
            currPageNo = pages.Count();
        PageData *pageData = pages.At(currPageNo - 1);
        DrawPageLayout(&Graphics(buf.GetDC()), &pageData->drawInstructions,
            PAGE_BORDER, PAGE_BORDER, showBboxes);
    }

    PAINTSTRUCT ps;
    buf.Flush(BeginPaint(hwnd, &ps));
    EndPaint(hwnd, &ps);
}

static ControlEbook *gControlFrame = NULL;

static void OnCreateWindow(HWND hwnd)
{
    gControlFrame = new ControlEbook(hwnd);

    HMENU menu = CreateMenu();
    AppendMenu(menu, MF_STRING,     (UINT_PTR)IDM_OPEN,         _T("&Open\tO"));
    AppendMenu(menu, MF_STRING,     (UINT_PTR)IDM_TOGGLE_BBOX,  _T("Toggle bbox\tF1"));
    AppendMenu(menu, MF_SEPARATOR,  0, NULL);
    AppendMenu(menu, MF_STRING,     (UINT_PTR)IDM_EXIT,         _T("E&xit\tEsc"));
    HMENU mainMenu = CreateMenu();
    AppendMenu(mainMenu, MF_POPUP | MF_STRING, (UINT_PTR)menu,  _T("&File"));
    // triggers OnSize(), so must be called after we
    // have things set up to handle OnSize()
    SetMenu(hwnd, mainMenu);
}

static void OnOpen(HWND hwnd)
{
    OPENFILENAME ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = _T("All supported documents\0;*.mobi;*.awz;*.prc;*.epub;*.fb2;*.fb2.zip\0\0");
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER;
    ofn.nMaxFile = MAX_PATH;
    ScopedMem<TCHAR> file(SAZA(TCHAR, ofn.nMaxFile));
    ofn.lpstrFile = file;

    if (GetOpenFileName(&ofn))
        gControlFrame->LoadDoc(ofn.lpstrFile);
}

static void OnToggleBbox(HWND hwnd)
{
    gControlFrame->ToggleShowBBoxes();
    gControlFrame->Repaint();
}

static void OnExit(HWND hwnd)
{
    SendMessage(hwnd, WM_CLOSE, 0, 0);
}

static LRESULT OnCommand(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (LOWORD(wParam)) {
    case IDM_EXIT: OnExit(hwnd); break;
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
    case VK_SPACE: gControlFrame->AdvancePage(IsShiftPressed() ? -1 : 1); break;
    case VK_ESCAPE: case 'Q': OnExit(hwnd); break;
    case VK_F1: OnToggleBbox(hwnd); break;
    default: return DefWindowProc(hwnd, msg, key, lParam);
    }
    return 0;
}

static LRESULT OnSize(HWND hwnd, UINT msg, WPARAM key, LPARAM lParam)
{
    gControlFrame->PageLayout(SizeI(LOWORD(lParam), HIWORD(lParam)));
    gControlFrame->AdvancePage(0);
    return 0;
}

static LRESULT CALLBACK WndProcFrame(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE: OnCreateWindow(hwnd); break;
    case WM_DESTROY: PostQuitMessage(0); delete gControlFrame; break;
    // if we return 0, during WM_PAINT we can check
    // PAINTSTRUCT.fErase to see if WM_ERASEBKGND
    // was sent before WM_PAINT
    case WM_ERASEBKGND: break;
    case WM_PAINT: gControlFrame->OnPaint(); break;
    case WM_COMMAND: return OnCommand(hwnd, msg, wParam, lParam);
    case WM_KEYDOWN: return OnKeyDown(hwnd, msg, wParam, lParam);
    case WM_SIZE: return OnSize(hwnd, msg, wParam, lParam);
    default: return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

static bool InstanceInit(HINSTANCE hInstance, int nCmdShow)
{
    WNDCLASSEX wcex;
    FillWndClassEx(wcex, hInstance);
    // clear out CS_HREDRAW | CS_VREDRAW style so that resizing doesn't
    // cause the whole window redraw
    wcex.style = 0;
    wcex.lpszClassName  = ET_FRAME_CLASS_NAME;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SUMATRAPDF));
    wcex.hbrBackground  = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
    wcex.lpfnWndProc    = WndProcFrame;

    ATOM atom = RegisterClassEx(&wcex);
    if (!atom)
        return false;

    HWND hwnd = CreateWindow(
            ET_FRAME_CLASS_NAME, ET_FRAME_TITLE, WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, WIN_DX, WIN_DY,
            NULL, NULL, hInstance, NULL);
    if (!hwnd)
        return false;

    CenterDialog(hwnd);
    ShowWindow(hwnd, SW_SHOW);

    return true;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
#ifdef DEBUG
    // report memory leaks on DbgOut
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
    SetErrorMode(SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS);

    ScopedCom com;
    InitAllCommonControls();
    ScopedGdiPlus gdi;

    if (!InstanceInit(hInstance, nCmdShow))
        return 1;
    
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return msg.wParam;
}
