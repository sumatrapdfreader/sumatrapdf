// Fb2Test.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "resource.h"
#include "../../include/crengine.h"
#include "../../include/wolutil.h"
#include "../../include/lvpagesplitter.h"
#include "../../include/hyphman.h"

// define FIXED_JINKE_SIZE to fix window size
//#define FIXED_JINKE_SIZE 1

void Export( HWND hWnd, bool flgGray  );

#define LVCHECKPOINT(msg) \
        MessageBox( NULL, msg, "CR Engine :: checkpoint", MB_OK);

#define MAX_LOADSTRING 100

HWND g_hWnd = NULL;

// Global Variables:
HINSTANCE hInst;								// current instance
LVDocView * text_view;


void testFormatting()
{
    //
    class Tester {
    public:
        LFormattedText txt;
        void addLine( const lChar16 * str, int flags, LVFontRef font )
        {
            lString16 s( str );
            txt.AddSourceLine(
               s.c_str(),        /* pointer to unicode text string */
               s.length(),         /* number of chars in text, 0 for auto(strlen) */
               0x000000,       /* text color */
               0xFFFFFF,     /* background color */
               font.get(),        /* font to draw string */
               flags,
               16,    /* interline space, *16 (16=single, 32=double) */
               30,    /* first line margin */
               NULL,
               0
            );
        }
    };
    LVFontRef font1 = fontMan->GetFont(20, 300, false, css_ff_sans_serif, lString8("Arial") );
    LVFontRef font2 = fontMan->GetFont(20, 300, false, css_ff_serif, lString8("Times New Roman") );
    Tester t;
    t.addLine( L"Testing simple paragraph formatting. Just a test. ", LTEXT_ALIGN_WIDTH|LTEXT_FLAG_OWNTEXT, font1 );
    t.addLine( L"Another fragment of text. ", LTEXT_FLAG_OWNTEXT, font1 );
    t.addLine( L"And the last one written with another font", LTEXT_FLAG_OWNTEXT, font2 );
    t.addLine( L"Next paragraph: left-aligned. ", LTEXT_ALIGN_LEFT|LTEXT_FLAG_OWNTEXT, font1 );
    t.addLine( L"One more sentence. Second sentence.", LTEXT_FLAG_OWNTEXT, font1 );
    t.addLine( L"One more sentence. Second sentence.", LTEXT_FLAG_OWNTEXT, font1 );
    t.addLine( L" One more sentence. Second sentence.", LTEXT_FLAG_OWNTEXT, font1 );
    t.addLine( L"Testing simple paragraph formatting. Just a test. ", LTEXT_ALIGN_WIDTH|LTEXT_FLAG_OWNTEXT, font1 );
    t.addLine( L" One more sentence. Second sentence.", LTEXT_FLAG_OWNTEXT, font1 );
    t.addLine( L" Word", LTEXT_FLAG_OWNTEXT, font1 );
    t.addLine( L" Word", LTEXT_FLAG_OWNTEXT, font1 );
    t.addLine( L" Word", LTEXT_FLAG_OWNTEXT, font2 );
    t.addLine( L" Word", LTEXT_FLAG_OWNTEXT, font1 );
    t.addLine( L" Word", LTEXT_FLAG_OWNTEXT, font1 );
    t.addLine( L" Word", LTEXT_FLAG_OWNTEXT, font2 );
    t.addLine( L" Word", LTEXT_FLAG_OWNTEXT, font1 );
    t.addLine( L" One more sentence. Second sentence.", LTEXT_FLAG_OWNTEXT, font1 );
    t.addLine( L" One more sentence. Second sentence.", LTEXT_FLAG_OWNTEXT, font1 );
    int N = 100000;
    int i;
    time_t start1 = time((time_t*)0);
    for ( i=0; i<N; i++ )
        t.txt.FormatNew( 400, 300 );
    time_t end1 = time((time_t*)0);
    time_t start2 = time((time_t*)0);
    for ( i=0; i<N; i++ )
        t.txt.FormatOld( 400, 300 );
    time_t end2 = time((time_t*)0);
    int t1 = (int)(end1-start1);
    int t2 = (int)(end2-start2);
    int delta = (t2-t1) * 100 / t2;
    CRLog::debug("new formatter performance gain is %d%%", delta);
}


// Foward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

lString8 OpenFileDialog( HWND hWndOwner )
{
    lString8 fn;
    OPENFILENAMEA ofn;
    char str[MAX_PATH] = "";
    memset( &ofn, 0, sizeof(ofn) );
    ofn.lStructSize = sizeof( ofn );
    ofn.hwndOwner = hWndOwner;
    ofn.lpstrFilter = "All supported files (*.fb2;*.txt;*.rtf;*.zip;*.tcr;*.html;*.epub)\0*.fb2;*.txt;*.rtf;*.zip;*.htm;*.html;*.tcr;*.epub\0FictionBook2 files (*.fb2)\0*.fb2\0ZIP files (*.zip)\0*.zip\0All Files (*.*)\0*.*\0\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = str;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Please select book to open";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = "fb2";
//    ofn.FlagsEx = OFN_EX_NOPLACESBAR;
    if ( GetOpenFileNameA( &ofn ) )
    {
        fn = str;
    }
    return fn;
}

lString8 SaveFileDialog( HWND hWndOwner )
{
    lString8 fn;
    OPENFILENAMEA ofn;
    char str[MAX_PATH] = "";
    memset( &ofn, 0, sizeof(ofn) );
    ofn.lStructSize = sizeof( ofn );
    ofn.hwndOwner = hWndOwner;
    ofn.lpstrFilter = "WOL files (*.wol)\0*.wol\0All Files (*.*)\0*.*\0\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = str;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Please select book to open";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = "wol";
//    ofn.FlagsEx = OFN_EX_NOPLACESBAR;
    if ( GetSaveFileNameA( &ofn ) )
    {
        fn = str;
    }
    return fn;
}

lString8 readFileToString( const char * fname )
{
    lString8 buf;
    LVStreamRef stream = LVOpenFileStream(fname, LVOM_READ);
    if (!stream)
        return buf;
    int sz = stream->GetSize();
    if (sz>0)
    {
        buf.insert( 0, sz, ' ' );
        stream->Read( buf.modify(), sz, NULL );
    }
    return buf;
}

void TestWol()
{
	{
		LVStreamRef stream = LVOpenFileStream("woltest.wol", LVOM_WRITE);
		if (!stream)
			return;
		LVArray<lUInt32> m(10, 0xbad);
		m.clear();
		WOLWriter wol(stream.get());
		wol.addTitle(
			lString8("book title"),
			lString8("subj."),
			lString8("John Smith"),
			lString8("adapter"),
			lString8("translator"),
			lString8("publisher"),
			lString8("2006-11-01"),
			lString8("This is introduction."),
			lString8("ISBN")
		);
		LVGrayDrawBuf cover(600, 800);
		cover.FillRect(20, 20, 50, 50, 1);
		cover.FillRect(40, 70, 120, 190, 2);
		cover.FillRect(60, 80, 150, 290, 3);
		LVGrayDrawBuf page1(600, 800, 2);
		page1.FillRect(0, 0, 150, 150, 1);
		page1.FillRect(70, 70, 140, 140, 2);
		page1.FillRect(130, 130, 180, 180, 3);
		page1.FillRect(400, 400, 550, 750, 1);
		page1.FillRect(420, 420, 530, 730, 2);
		page1.FillRect(440, 440, 510, 710, 3);
		LVGrayDrawBuf page2(600, 800, 2);
		page2.FillRect(120, 20, 150, 50, 1);
		page2.FillRect(140, 70, 220, 190, 2);
		page2.FillRect(160, 80, 250, 290, 3);
		LVGrayDrawBuf page3(600, 800, 2);
		page3.FillRect(120, 120, 30, 20, 1);
		page3.FillRect(10, 10, 120, 120, 2);
		page3.FillRect(300, 300, 300, 700, 3);
		page3.FillRect(400, 400, 550, 750, 1);
		page3.FillRect(420, 420, 530, 730, 2);
		page3.FillRect(440, 440, 510, 710, 3);
		wol.addCoverImage(cover);
		wol.addImage(page1);
		wol.addImage(page2);
		wol.addImage(page3);
		SaveBitmapToFile("page1.bmp", &page1);
		SaveBitmapToFile("page2.bmp", &page2);
		SaveBitmapToFile("page3.bmp", &page3);
	}

	{
		//LVStream * stream = LVOpenFileStream("Biblia.wol", LVOM_READ);
		LVStreamRef stream = LVOpenFileStream("woltest.wol", LVOM_READ);
		//LVStream * stream = LVOpenFileStream("info2.wol", LVOM_READ);
		if (!stream)
			return;
		WOLReader rd(stream.get());
		if (!rd.readHeader())
			return;
		LVStreamRef cover = LVOpenFileStream("cover2.bin", LVOM_WRITE);
		LVStreamRef log = LVOpenFileStream("woltest.log", LVOM_WRITE);
		LVArray<lUInt8> * rdcover = rd.getBookCover();
		LVGrayDrawBuf * rdimg1 = rd.getImage(0);
		*cover << *rdcover;
		//int imgsz = (rdimg1->GetWidth()*2+7)/8*rdimg1->GetHeight();
		//LVArray<lUInt8> imgbuf(imgsz, 0);
		//memcpy(imgbuf.ptr(), rdimg1->GetScanLine(0), imgsz );
		if (rdimg1)
			SaveBitmapToFile( "test.bmp", rdimg1 );
		//*img << imgbuf;
		*log << rd.getBookTitle();
		*log << "\r\nimages found: " << lString8::itoa(rd.getImageCount());
		delete rdimg1;
	}
}

void Export( HWND hWnd, bool flgGray )
{
	lString8 fn = SaveFileDialog( hWnd );
	if (fn.empty())
		return;

	HCURSOR newCursor = ::LoadCursor( NULL, IDC_WAIT );
	HCURSOR oldCursor = ::SetCursor( newCursor );
	text_view->exportWolFile(fn.c_str(), flgGray, 3);
	::SetCursor( oldCursor );
}


void initHyph(const char * fname)
{
	HyphMan hyphman;
	LVStreamRef stream = LVOpenFileStream( fname, LVOM_READ);
	if (!stream)
		return;
	HyphMan::Open( stream.get() );
}

/*                  0  1
	1 4         0  00 10   00 01
	3 2         1  11 01   10 11

                      b0 = y0
					  b1 = x0 ^ y0

                     00    01   10   11
    1 0 4 0    00  0000  1000 0011 1011
	0 5 0 8    01  1100  0100 1111 0111
	3 0 2 0    10  0010  1010 0001 1001
	0 7 0 6    11  1110  0110 1101 0101

                      b0 = x1;
					  b1 = x1 ^ y1;
					  b2 = y0
					  b3 = x0 ^ y0

                      b0 = y2;
					  b1 = x2 ^ y2;
					  b2 = x1
					  b3 = x1 ^ y1
					  b4 = y0
					  b5 = x0 ^ y0
*/
/*
void make_dither_table()
{
	int m[64];
	for (int y=0; y<8; y++) {
		for (int x=0; x<8; x++) {
			int d = 0;
			int xy = x^y;
			d |= (xy&4)>>1;
			d |= (xy&2)<<2;
			d |= (xy&1)<<5;
			d |= (y&4)>>2;
			d |= (x&2)<<1;
			d |= (y&1)<<4;
			m[x + y*8] = d;
		}
	}
	FILE * f = fopen("c:\\_dither.txt", "wt");
	if (f) {
		for (int i=0; i<64; i++) {
		    fprintf(f, "%d, ", m[i]);
			if ( (i&7)==7 )
			    fprintf(f, "\n", m[i]);
		}
		fclose(f);
	}
}
*/

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{

    CRLog::setFileLogger( "crengine.log" );
    CRLog::setLogLevel( CRLog::LL_TRACE );

 	// TODO: Place code here.
	MSG msg;
	HACCEL hAccelTable;

	//make_dither_table();

	//TestWol();
/*
    LVStreamRef zipfile = LVOpenFileStream( L"zip_test.zip", LVOM_READ );
    if (!zipfile.isNull())
    {
        LVContainerRef zip = LVOpenArchieve( zipfile );
        if (!zip.isNull())
        {
            LVStreamRef log = LVOpenFileStream("ziptest.log", LVOM_WRITE);
            for (int i=0; i<zip->GetObjectCount(); i++)
            {
                const LVContainerItemInfo * item = zip->GetObjectInfo(i);
                if (item)
                {
                    //
                    *log << UnicodeToLocal( item->GetName() );
                    *log << lString8::itoa( (int)item->GetSize() );

                    LVStreamRef unpstream = zip->OpenStream( item->GetName(), LVOM_READ );
                    if (!unpstream.isNull())
                    {
                        *log << "\n arc stream opened ok \n";
                        LVStreamRef outstream = LVOpenFileStream( item->GetName(), LVOM_WRITE );
                        if ( !outstream.isNull() )
                        {
                            int copiedBytes = (int)LVPumpStream( outstream, unpstream );
                            *log << " copied " << lString8::itoa(copiedBytes) << " bytes\n";
                        }
                        else
                        {
                            *log << " error opening out stream\n";
                        }
                    }
                }
            }
        }
    }
*/
	lString8 exe_dir;
	char exe_fn[MAX_PATH+1];
	GetModuleFileNameA( NULL, exe_fn, MAX_PATH );
	int last_slash = -1;
	int i;
	for (i=0; exe_fn[i]; i++)
		if (exe_fn[i]=='\\' || exe_fn[i]=='/')
			last_slash = i;
	if (last_slash>0)
		exe_dir = lString8( exe_fn, last_slash );

	// init hyphenation manager
	initHyph( (exe_dir + "\\russian_EnUS_hyphen_(Alan).pdb").c_str() );

    lString8 fontDir = exe_dir;
    fontDir << "\\fonts";

    // init bitmap font manager
    InitFontManager( fontDir );


    // Load font definitions into font manager
    // fonts are in files font1.lbf, font2.lbf, ... font32.lbf
#if (USE_FREETYPE==1)
        LVContainerRef dir = LVOpenDirectory( LocalToUnicode(fontDir).c_str() );
        if ( !dir.isNull() )
        for ( i=0; i<dir->GetObjectCount(); i++ ) {
            const LVContainerItemInfo * item = dir->GetObjectInfo(i);
            lString16 fileName = item->GetName();
            if ( !item->IsContainer() && fileName.length()>4 && lString16(fileName, fileName.length()-4, 4)==L".ttf" ) {
                lString8 fn = UnicodeToLocal(fileName);
                printf("loading font: %s\n", fn.c_str());
                if ( !fontMan->RegisterFont(fn) ) {
                    printf("    failed\n");
                }
            }
        }
        //fontMan->RegisterFont(lString8("arial.ttf"));
#else
#if (USE_WIN32_FONTS==0)

    #define MAX_FONT_FILE 32
    for (i=0; i<MAX_FONT_FILE; i++)
    {
        char fn[32];
        sprintf( fn, "font%d.lbf", i );
        fontMan->RegisterFont( lString8(fn) );
    }
#endif
#endif
    //LVCHECKPOINT("WinMain start");
    text_view = new LVDocView;

    // stylesheet can be placed to file fb2.css
    // if not found, default stylesheet will be used
    lString8 css = readFileToString( (exe_dir + "\\fb2.css").c_str() );
    if (css.length() > 0)
        text_view->setStyleSheet( css );

    //LVCHECKPOINT("WinMain before loads");

    if (!fontMan->GetFontCount())
    {
        //error
        char str[100];
#if (USE_FREETYPE==1)
        sprintf(str, "Cannot open font file(s) fonts/*.ttf \nCannot work without font\nPlace some TTF files to font\\ directory" );
#else
        sprintf(str, "Cannot open font file(s) font#.lbf \nCannot work without font\nUse FontConv utility to generate .lbf fonts from TTF" );
#endif
        MessageBoxA( NULL, str, "CR Engine :: Fb2Test -- fatal error!", MB_OK);
        return 1;
    }

    lString8 cmdline(lpCmdLine);
    cmdline.trim();
    if ( cmdline == "test_format" ) {
        testFormatting();
        return 1;
    }
    if (cmdline.empty())
    {
        cmdline = OpenFileDialog( NULL );
        //cmdline = "example2.fb2";
    }

    if ( cmdline.empty() )
        return 2;
    if ( !text_view->LoadDocument( cmdline.c_str() ))
    {
        //error
        char str[100];
        sprintf(str, "Cannot open document file %s", cmdline.c_str());
        MessageBoxA( NULL, str, "CR Engine :: Fb2Test -- fatal error!", MB_OK);
        return 1;
    }

    //LVCHECKPOINT("WinMain after loads");

	// Initialize global strings
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow))
	{
		return FALSE;
	}


	hAccelTable = LoadAccelerators(hInstance, (LPCTSTR)IDC_FONTTEST);

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

    delete text_view;

    ShutdownFontManager();

	return msg.wParam;
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage is only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEXW);

	wcex.style			= 0; //CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= (WNDPROC)WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, (LPCTSTR)IDI_FONTTEST);
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= NULL;
	wcex.lpszClassName	= L"CoolReader";
	wcex.hIconSm		= LoadIcon(wcex.hInstance, (LPCTSTR)IDI_SMALL);

	return RegisterClassExW(&wcex);
}

static void UpdateScrollBar(HWND hWnd )
{
	const LVScrollInfo * lvsi = text_view->getScrollInfo();
    SCROLLINFO si;
    memset( &si, 0, sizeof(si) );
    si.cbSize = sizeof(si);
    si.fMask = SIF_DISABLENOSCROLL | SIF_PAGE | SIF_POS | SIF_RANGE;
    si.nMin = 0;
    si.nMax = lvsi->maxpos;
    si.nPos = lvsi->pos;
    si.nPage = lvsi->pagesize;
    SetScrollInfo( hWnd, SB_VERT, &si, true );
    InvalidateRect(hWnd, NULL, FALSE);
}


//
//   FUNCTION: InitInstance(HANDLE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   HWND hWnd;

   hInst = hInstance; // Store instance handle in our global variable

   int x=0;
   int y=0;
   int dx=0;
   int dy=0;
   lUInt32 flags = 0;
#ifdef FIXED_JINKE_SIZE
      flags = WS_DLGFRAME | WS_MINIMIZEBOX | WS_SYSMENU | WS_VSCROLL; //WS_OVERLAPPEDWINDOW
      dx = 600 + GetSystemMetrics(SM_CXDLGFRAME)*2
      	 + GetSystemMetrics(SM_CXVSCROLL);
      dy = 800 + GetSystemMetrics(SM_CYDLGFRAME)*2
      	 + GetSystemMetrics(SM_CYCAPTION);
#else
      flags = WS_OVERLAPPEDWINDOW | WS_VSCROLL; //
      dx = 500;
      dy = 600;
#endif

   hWnd = CreateWindowW(
	   L"CoolReader",
	   L"CREngine - Simple FB2 viewer",
      flags, //WS_OVERLAPPEDWINDOW
      x, y, dx, dy,
      NULL, NULL, hInstance, NULL);

   if (!hWnd)
   {
      return FALSE;
   }

   g_hWnd = hWnd;

   ShowWindow(hWnd, nCmdShow);

   text_view->Render();

   //UpdateScrollBar( hWnd );
   UpdateWindow(hWnd);

   return TRUE;
}

static void DoCommand( HWND hWnd, LVDocCmd cmd, int param=0 )
{
    text_view->doCommand( cmd, param );
    UpdateScrollBar( hWnd );
}


#define SCROLL_LINE_H 24

//
//  FUNCTION: WndProc(HWND, unsigned, WORD, LONG)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;
	TCHAR szHello[MAX_LOADSTRING];
	LoadString(hInst, IDS_HELLO, szHello, MAX_LOADSTRING);

	switch (message)
	{
		case WM_CREATE:
            {
		        LVDocImageRef img = text_view->getPageImage( 0 );
                img->getDrawBuf();
		        UpdateScrollBar(hWnd);
            }
		    break;
		case WM_ERASEBKGND:
            break;
		case WM_VSCROLL:
            {
                switch (LOWORD(wParam))
                {
                case SB_TOP:
                	DoCommand( hWnd, DCMD_BEGIN );
                    break;
                case SB_BOTTOM:
                	DoCommand( hWnd, DCMD_END );
                    break;
                case SB_LINEDOWN:
                	DoCommand( hWnd, DCMD_LINEDOWN, 1 );
                    break;
                case SB_LINEUP:
                	DoCommand( hWnd, DCMD_LINEUP, 1 );
                    break;
                case SB_PAGEDOWN:
                	DoCommand( hWnd, DCMD_PAGEDOWN, 1 );
                    break;
                case SB_PAGEUP:
                	DoCommand( hWnd, DCMD_PAGEUP, 1 );
                    break;
                case SB_THUMBPOSITION:
                	DoCommand( hWnd, DCMD_GO_POS,
                        text_view->scrollPosToDocPos( HIWORD(wParam) )
                	 );
                    break;
                case SB_THUMBTRACK:
                	DoCommand( hWnd, DCMD_GO_POS,
                        text_view->scrollPosToDocPos( HIWORD(wParam) )
                	 );
                    break;
                case SB_ENDSCROLL:
                	DoCommand( hWnd, DCMD_GO_POS,
                        text_view->GetPos()
                	 );
                    break;
                }
            }
            break;
		case WM_SIZE:
            {
				if (wParam!=SIZE_MINIMIZED)
				{
					text_view->Resize(LOWORD(lParam), HIWORD(lParam));
					UpdateScrollBar( hWnd );
				}
            }
            break;
#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL                   0x020A
#define WHEEL_DELTA                     120     /* Value for rolling one detent */
#endif
		case WM_MOUSEWHEEL:
			{
  			int delta = ((lInt16)HIWORD(wParam))/WHEEL_DELTA;
  			if (delta<0)
  				DoCommand( hWnd, DCMD_LINEDOWN, 3 );
  			else if (delta>0)
  				DoCommand( hWnd, DCMD_LINEUP, 3 );
			}
            break;
		case WM_KEYDOWN:
            {
                switch( wParam )
                {
                case VK_F3:
                   {
                        lString8 fn = OpenFileDialog( hWnd );
                        if ( !fn.empty() )
                        {
                            text_view->LoadDocument( fn.c_str() );
                            text_view->Render();
                			DoCommand( hWnd, DCMD_BEGIN );
                        }
                   }
                    break;
                case VK_F2:
           			Export(hWnd, false);
                    break;
                case VK_F5:
           			Export(hWnd, true);
                    break;
                case VK_UP:
           			DoCommand( hWnd, DCMD_LINEUP, 1 );
                    break;
                case VK_DOWN:
           			DoCommand( hWnd, DCMD_LINEDOWN, 1 );
                    break;
                case VK_NEXT:
           			DoCommand( hWnd, DCMD_PAGEDOWN, 1 );
                    break;
                case VK_PRIOR:
           			DoCommand( hWnd, DCMD_PAGEUP, 1 );
                    break;
                case VK_HOME:
           			DoCommand( hWnd, DCMD_BEGIN );
                    break;
                case VK_END:
           			DoCommand( hWnd, DCMD_END );
                    break;
                case VK_ADD:
                case VK_SUBTRACT:
                    {
                        text_view->ZoomFont( wParam==VK_ADD ? 1 : -1 );
                        UpdateScrollBar( hWnd );
                    }
                	break;
                case VK_TAB:
                    {
                        bool shift = (GetKeyState( VK_SHIFT ) & 0x8000)!=0;
                        if ( shift )
                            text_view->selectPrevPageLink( true );
                        else
                            text_view->selectNextPageLink( true );
                        UpdateScrollBar( hWnd );
                    }
                    //Update(hWnd);
                    break;
                case VK_RETURN:
                    text_view->goSelectedLink();
                    UpdateScrollBar( hWnd );
                    break;
                case VK_BACK:
                    text_view->goBack();
                    UpdateScrollBar( hWnd );
                    break;
                }
            }
            break;
        case WM_LBUTTONDOWN:
            {
                int xPos = lParam & 0xFFFF;
                int yPos = (lParam >> 16) & 0xFFFF;
                ldomXPointer ptr = text_view->getNodeByPoint( lvPoint( xPos, yPos ) );
                if ( !ptr.isNull() ) {
                    if ( ptr.getNode()->isText() ) {
                        ldomXRange * wordRange = new ldomXRange();
                        if ( ldomXRange::getWordRange( *wordRange, ptr ) ) {
                            wordRange->setFlags( 0x10000 );
                            text_view->getDocument()->getSelections().clear();
                            text_view->getDocument()->getSelections().add( wordRange );
                            text_view->updateSelections();
                            UpdateScrollBar( hWnd );
                        } else {
                            delete wordRange;
                        }
                    }
                }

            }
            break;
		case WM_COMMAND:
			wmId    = LOWORD(wParam);
			wmEvent = HIWORD(wParam);
			// Parse the menu selections:
			switch (wmId)
			{
				case IDM_ABOUT:
				   DialogBox(hInst, (LPCTSTR)IDD_ABOUTBOX, hWnd, (DLGPROC)About);
				   break;
				case IDM_EXIT:
				   DestroyWindow(hWnd);
				   break;
				case ID_ZOOM_IN:
                case ID_ZOOM_OUT:
                    {
                        text_view->ZoomFont( wParam==ID_ZOOM_IN ? 1 : -1 );
                        UpdateScrollBar( hWnd );
                    }
                    break;
				case IDM_FILE_OPEN:
                   {
                        lString8 fn = OpenFileDialog( hWnd );
                        if ( !fn.empty() )
                        {
                            text_view->LoadDocument( fn.c_str() );
                            text_view->Render();
                			DoCommand( hWnd, DCMD_BEGIN );
                        }
                   }
				   break;
				default:
				   return DefWindowProc(hWnd, message, wParam, lParam);
			}
			break;
		case WM_PAINT:
            {
			    hdc = BeginPaint(hWnd, &ps);
		        LVDocImageRef img = text_view->getPageImage( 0 );
                LVDrawBuf * drawBuf = img->getDrawBuf();
                /*
                ldomXRangeList links;
                ldomXRangeList & sel = text_view->getDocument()->getSelections();
                text_view->getCurrentPageLinks( links );
                int linkCount = links.length();
                if ( linkCount ) {
                    sel.clear();
                    sel.add( new ldomXRange(*links[0]) );
                    for ( int i=0; i<linkCount; i++ ) {
                        lString16 txt = links[i]->getRangeText();
                        lString8 txt8 = UnicodeToLocal( txt );
                        const char * s = txt8.c_str();
                        txt.clear();
                    }
                    linkCount++;
                    text_view->updateSelections();
                }
                */
                drawBuf->DrawTo( hdc, 0, 0, 0, NULL);

                //COLORREF pal[4]={0xFFFFFF, 0xAAAAAA, 0x555555, 0x000000};
                //DrawBuf2DC( hdc, 0, 0, text_view->GetDrawBuf(), pal, 1 );

                EndPaint(hWnd, &ps);
            }
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
   }
   return 0;
}

// Mesage handler for about box.
LRESULT CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_INITDIALOG:
				return TRUE;

		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
			{
				EndDialog(hDlg, LOWORD(wParam));
				return TRUE;
			}
			break;
	}
    return FALSE;
}
