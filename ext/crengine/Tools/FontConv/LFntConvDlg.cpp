// LFntConvDlg.cpp : implementation file
//

#include "stdafx.h"
#include "LFntConv.h"
#include "LFntConvDlg.h"
#include "../../include/crengine.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(CAboutDlg)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
		// No message handlers
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CLFntConvDlg dialog

CLFntConvDlg::CLFntConvDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CLFntConvDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CLFntConvDlg)
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
   m_logfont_selected = false;
   m_pfont = NULL;
   m_bmpfont = NULL;
   m_frmtext = NULL;
   memset( &m_logfont, 0, sizeof(m_logfont) );

   BITMAPINFO bmi;
   memset( &bmi, 0, sizeof(bmi) );
   bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
   bmi.bmiHeader.biWidth = 128;
   bmi.bmiHeader.biHeight = 128;
   bmi.bmiHeader.biPlanes = 1;
   bmi.bmiHeader.biBitCount = 32;
   bmi.bmiHeader.biCompression = BI_RGB;
   bmi.bmiHeader.biSizeImage = 0;
   bmi.bmiHeader.biXPelsPerMeter = 1024;
   bmi.bmiHeader.biYPelsPerMeter = 1024;
   bmi.bmiHeader.biClrUsed = 0;
   bmi.bmiHeader.biClrImportant = 0;

   HBITMAP hbmp = CreateDIBSection( NULL, &bmi, DIB_RGB_COLORS, (void**)(&m_drawpixels), NULL, 0 );
   m_drawbmp.Attach(hbmp);
   m_drawdc.CreateCompatibleDC(NULL);
   m_drawdc.SelectObject(&m_drawbmp);

}

bool CLFntConvDlg::DrawChar( int code, int & dx, int & dy )
{
   m_drawdc.SetBkColor(RGB(255,255,255));
   m_drawdc.SetBkMode(OPAQUE);
   m_drawdc.SetTextColor(RGB(0,0,0));
   RECT rc;
   rc.left = 0;
   rc.top = 0;
   rc.right = 128;
   rc.bottom = 128;
   m_drawdc.FillSolidRect(&rc, RGB(255,255,255));

   if (!m_pfont)
      return false;

   wchar_t s[2];
   s[0]=code;
   s[1]=0;
   CFont * oldfont = (CFont * )m_drawdc.SelectObject(m_pfont);
   SIZE sz;
   GetTextExtentPoint32W( m_drawdc.m_hDC, s, 1, &sz );
   dx = sz.cx;
   dy = sz.cy;
   TextOutW(m_drawdc.m_hDC, 0, 0, s, 1);
   m_drawdc.SelectObject(oldfont);
   return true;
}

void CLFntConvDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CLFntConvDlg)
	DDX_Control(pDX, IDC_ED_COPYRIGHT, m_edCopyright);
	DDX_Control(pDX, IDC_CB_ASCII, m_cbAscii);
	DDX_Control(pDX, IDC_CB_WESTERN, m_cbLatin);
	DDX_Control(pDX, IDC_CB_CYRILLIC, m_cbCyrillic);
	DDX_Control(pDX, IDC_CB_CHINEEZE, m_cbChineeze);
	DDX_Control(pDX, IDC_LBL_FONTNAME, m_lblFontName);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CLFntConvDlg, CDialog)
	//{{AFX_MSG_MAP(CLFntConvDlg)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BTN_CHOOSE_FONT, OnBtnChooseFont)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()



/////////////////////////////////////////////////////////////////////////////
// CLFntConvDlg message handlers

BOOL CLFntConvDlg::OnInitDialog()
{
	CDialog::OnInitDialog();


	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

    m_cbAscii.SetCheck(TRUE);
	m_cbCyrillic.SetCheck(TRUE);
	m_cbLatin.SetCheck(TRUE);
	m_edCopyright.SetWindowText( "unknown" );
	
	// TODO: Add extra initialization here
	
	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CLFntConvDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CLFntConvDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CPaintDC dc(this); // device context for painting
		CRect rect;
		GetClientRect(&rect);
        const int view_pos = 150;
        rect.top = view_pos - 5;
      dc.FillSolidRect(&rect, 0xC0C0C0);
      if (m_fontref.get())
      {
         COLORREF pal[] = {0xFFFFFF, 0xAAAAAA, 0x555555, 0x000000};
         //COLORREF pal[] = {0xF0F0F0, 0x909090, 0x505050, 0x101010};
         wchar_t text1[] = {'T','e','s','t',':',' ', 0x420, 0x443, 0x441, 0x441, 0x43A, 0x438, 0x439,
            ' ', 0x44F, 0x00AD, 0x437, 0x44B, 0x43A, 0x00AD, 0};
         wchar_t text2[] = {0x41F, 0x440, 0x43E, 0x432, 0x435, 0x440, 0x43A, 0x430, 0};

         //draw_buf_t buf;
		 LVGrayDrawBuf buf( m_frmtext->GetWidth()+10, 250 );
         //lvdrawbufAlloc( &buf, 2, m_frmtext->width+10, 250 );
         //lvdrawbufFill( &buf, 0 );
		 buf.Clear( 0 );
         m_frmtext->Draw( &buf, 5, 5 );
		 //buf.DrawFormattedText( m_frmtext, 5, 5 );
         //lvtextDraw( m_frmtext,  &buf, 5, 5);
         //lvdrawbufDrawText( &buf, 10, 3, m_bmpfont, text1, wcslen(text1), '?' );
         //lvdrawbufDrawText( &buf, 10, 3+lvfontGetHeader(m_bmpfont)->fontHeight, m_bmpfont, text2, wcslen(text2), '?' );
         DrawBuf2DC( dc, 10, view_pos, &buf, pal, 1 );
         DrawBuf2DC( dc, m_frmtext->GetWidth()+30, view_pos, &buf, pal, 2 );
         //lvdrawbufFree( &buf );
      }
		//CDialog::OnPaint();
	}
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CLFntConvDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

void CLFntConvDlg::InitFontList()
{

}

DWORD GetGlyphIndicesW(
  HDC hdc,       // handle to DC
  LPCWSTR lpstr, // string to convert
  int c,         // number of characters in string
  LPWORD pgi,    // array of glyph indices
  DWORD fl       // glyph options
);

#define SH L"\xad"
void CLFntConvDlg::FormatSampleText()
{
    LVFont * font = m_fontref.get();
    if ( m_frmtext )
        delete m_frmtext;
    m_frmtext = new LFormattedText();
    if ( !font )
        return;
    m_frmtext->AddSourceLine( L"Centered paragraph!",
        0, font, LTEXT_FLAG_OWNTEXT|LTEXT_ALIGN_CENTER, 16, 0, NULL );
    m_frmtext->AddSourceLine( L"Russian: " 
        L"\x042D\x0442\x043E \x0442\x0435\x043A\x0441\x0442 \x043D\x0430 "
        L"\x0440\x0443\x0441" SH L"\x0441\x043A\x043E\x043C \x044F\x0437\x044B" SH L"\x043A\x0435. ",
        0, font, LTEXT_FLAG_OWNTEXT|LTEXT_ALIGN_WIDTH, 16, 40, NULL );
    m_frmtext->AddSourceLine( L"New para.",
        0, font, LTEXT_FLAG_OWNTEXT|LTEXT_ALIGN_WIDTH, 16, 40, NULL );
    m_frmtext->AddSourceLine( L"This sentence should be appended to the same paragraph.",
        0, font, LTEXT_FLAG_OWNTEXT, 16, 40, NULL );
    m_frmtext->AddSourceLine( L"This is right aligned paragraph. Is it wrapped properly? Check it!",
        0, font, LTEXT_FLAG_OWNTEXT|LTEXT_ALIGN_RIGHT, 16, 0, NULL );
    m_frmtext->AddSourceLine( L"This is paragraph tests width alignment. "
        L"First line has margin.",
        0, font, LTEXT_FLAG_OWNTEXT|LTEXT_ALIGN_WIDTH, 16, 40, NULL );
    m_frmtext->Format( 340, 300 );
}


void CLFntConvDlg::OnBtnChooseFont() 
{
	// TODO: Add your control notification handler code here
   CFontDialog dlg(NULL, CF_TTONLY|CF_SCREENFONTS, NULL, this );
   if (dlg.DoModal()==IDOK)
   {
      //
      dlg.GetCurrentFont( &m_logfont );
      CString txt;
      txt = dlg.GetFaceName() + " ";
      CString sz;
      sz.Format("(%d)\r\n", dlg.GetSize()/10);
      txt += sz;
      m_lblFontName.SetWindowText(txt);
      if (m_pfont)
         delete m_pfont;
      m_pfont = new CFont();
      m_pfont->CreateFontIndirect(&m_logfont);
      m_lblFontName.SetFont(m_pfont);

      CFileDialog savedlg(false, "lbf", "bmpfont.lbf", OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
         "Bitmap fonts (*.lbf)|*.lbf||", this);
      if (savedlg.DoModal()==IDOK)
      {
         CString fname = savedlg.GetFileName();
         CLVFntConvertor conv(&m_logfont);
         DWORD flt = 0;
         if ( m_cbLatin.GetCheck() )
             flt |= LCHARSET_LATIN;
         if ( m_cbCyrillic.GetCheck() )
             flt |= LCHARSET_CYRILLIC;
         if ( m_cbChineeze.GetCheck() )
             flt |= LCHARSET_CHINEESE;
         conv.SetCharsetFilter( flt );
		 char c[64];
		 m_edCopyright.GetWindowText( c, 64 );
		 conv.SetCopyright( lString8(c) );
         conv.Convert( fname );
         if (m_bmpfont)
            lvfontClose(m_bmpfont);
         m_bmpfont = NULL;
         m_fontref = LoadFontFromFile( fname );
         //lvfontOpen( fname, &m_bmpfont );
         FormatSampleText();

      }
      Invalidate();

/*
      m_char_exists[0]=false;
      m_char_exists[0xFFFF]=false;
      CFont * oldfont = (CFont * )m_drawdc.SelectObject(m_pfont);
      CLFntGlyph glyph;
      int glyphcount = 0;
      for (int i=1; i<0xFFFF; i++)
      {
#define GGI_MARK_NONEXISTING_GLYPHS 1
         //GetFontUnicodeRanges();
         //GetGlyphIndicesW(m_drawdc.m_hDC, s, 1, w, GGI_MARK_NONEXISTING_GLYPHS);
         ;
         m_char_exists[i] = glyph.Init( m_drawdc.m_hDC, i );
         if (m_char_exists[i])
            glyphcount++;
      }
      m_drawdc.SelectObject(oldfont);
*/
   }
}

int CLFntConvDlg::batchConvert(const char *fn)
{
	FILE * f = fopen( fn, "rt" );
	int cntConverted = false;
	char str[2048];
	if (f)
	{
		while (fgets(str, 2047, f))
		{
			char fname[128] = "";
			char fontname[128] = "";
			int  size = 0;
			int  bold = 0;
			int  italic = 0;
			if (sscanf(str, "%s %d %d %d %s", fname, &size, &bold, &italic, fontname)!=5)
				continue;
			if (size<8 || size>52 || bold<0 || bold>1 || italic<0 || italic>1
				|| !fname[0] || !fontname[0])
				continue;
			for (int i=0; fontname[i]; i++)
				if (fontname[i]=='_')
					fontname[i]=' ';
			LOGFONT lf;
			memset(&lf, 0, sizeof(LOGFONT));
			lf.lfHeight = size;
			lf.lfWeight = bold?700:400;
			lf.lfItalic = italic;
			lf.lfCharSet = DEFAULT_CHARSET;
			lf.lfOutPrecision = OUT_TT_ONLY_PRECIS;
			lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
			lf.lfQuality = PROOF_QUALITY;
			strcpy(lf.lfFaceName, fontname);
			lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;

			 CLVFntConvertor conv(&lf);
			 conv.SetCharsetFilter( LCHARSET_LATIN | LCHARSET_CYRILLIC );
			 conv.SetCopyright( lString8("Unknown") );
			 conv.Convert( fname );

			cntConverted++;
		}
		fclose(f);
	}
	return cntConverted;
}
