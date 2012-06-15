#include "fitz.h"
#include "mupdf.h"
#include "muxps.h"
#include "mucbz.h"
#include "pdfapp.h"

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>

#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL 0x020A
#endif

#define ID_ABOUT	0x1000
#define ID_DOCINFO	0x1001

static HWND hwndframe = NULL;
static HWND hwndview = NULL;
static HDC hdc;
static HBRUSH bgbrush;
static HBRUSH shbrush;
static BITMAPINFO *dibinf;
static HCURSOR arrowcurs, handcurs, waitcurs;
static LRESULT CALLBACK frameproc(HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK viewproc(HWND, UINT, WPARAM, LPARAM);

static int justcopied = 0;

static pdfapp_t gapp;

static wchar_t wbuf[1024];
static char filename[1024];

/*
 * Create registry keys to associate MuPDF with PDF and XPS files.
 */

#define OPEN_KEY(parent, name, ptr) \
	RegCreateKeyExA(parent, name, 0, 0, 0, KEY_WRITE, 0, &ptr, 0)

#define SET_KEY(parent, name, value) \
	RegSetValueExA(parent, name, 0, REG_SZ, value, strlen(value) + 1)

void install_app(char *argv0)
{
	char buf[512];
	HKEY software, classes, mupdf, dotpdf, dotxps;
	HKEY shell, open, command, supported_types;
	HKEY pdf_progids, xps_progids;

	OPEN_KEY(HKEY_CURRENT_USER, "Software", software);
	OPEN_KEY(software, "Classes", classes);
	OPEN_KEY(classes, ".pdf", dotpdf);
	OPEN_KEY(dotpdf, "OpenWithProgids", pdf_progids);
	OPEN_KEY(classes, ".xps", dotxps);
	OPEN_KEY(dotxps, "OpenWithProgids", xps_progids);
	OPEN_KEY(classes, "MuPDF", mupdf);
	OPEN_KEY(mupdf, "SupportedTypes", supported_types);
	OPEN_KEY(mupdf, "shell", shell);
	OPEN_KEY(shell, "open", open);
	OPEN_KEY(open, "command", command);

	sprintf(buf, "\"%s\" \"%%1\"", argv0);

	SET_KEY(open, "FriendlyAppName", "MuPDF");
	SET_KEY(command, "", buf);
	SET_KEY(supported_types, ".pdf", "");
	SET_KEY(supported_types, ".xps", "");
	SET_KEY(pdf_progids, "MuPDF", "");
	SET_KEY(xps_progids, "MuPDF", "");

	RegCloseKey(dotxps);
	RegCloseKey(dotpdf);
	RegCloseKey(mupdf);
	RegCloseKey(classes);
	RegCloseKey(software);
}

/*
 * Dialog boxes
 */

void winwarn(pdfapp_t *app, char *msg)
{
	MessageBoxA(hwndframe, msg, "MuPDF: Warning", MB_ICONWARNING);
}

void winerror(pdfapp_t *app, char *msg)
{
	MessageBoxA(hwndframe, msg, "MuPDF: Error", MB_ICONERROR);
	exit(1);
}

int winfilename(wchar_t *buf, int len)
{
	OPENFILENAME ofn;
	buf[0] = 0;
	memset(&ofn, 0, sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hwndframe;
	ofn.lpstrFile = buf;
	ofn.nMaxFile = len;
	ofn.lpstrInitialDir = NULL;
	ofn.lpstrTitle = L"MuPDF: Open PDF file";
	ofn.lpstrFilter = L"Documents (*.pdf;*.xps;*.cbz;*.zip)\0*.zip;*.cbz;*.xps;*.pdf\0PDF Files (*.pdf)\0*.pdf\0XPS Files (*.xps)\0*.xps\0CBZ Files (*.cbz;*.zip)\0*.zip;*.cbz\0All Files\0*\0\0";
	ofn.Flags = OFN_FILEMUSTEXIST|OFN_HIDEREADONLY;
	return GetOpenFileNameW(&ofn);
}

static char pd_filename[256] = "The file is encrypted.";
static char pd_password[256] = "";
static int pd_okay = 0;

INT CALLBACK
dlogpassproc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message)
	{
	case WM_INITDIALOG:
		SetDlgItemTextA(hwnd, 4, pd_filename);
		return TRUE;
	case WM_COMMAND:
		switch(wParam)
		{
		case 1:
			pd_okay = 1;
			GetDlgItemTextA(hwnd, 3, pd_password, sizeof pd_password);
			EndDialog(hwnd, 1);
			return TRUE;
		case 2:
			pd_okay = 0;
			EndDialog(hwnd, 1);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

char *winpassword(pdfapp_t *app, char *filename)
{
	char buf[1024], *s;
	int code;
	strcpy(buf, filename);
	s = buf;
	if (strrchr(s, '\\')) s = strrchr(s, '\\') + 1;
	if (strrchr(s, '/')) s = strrchr(s, '/') + 1;
	if (strlen(s) > 32)
		strcpy(s + 30, "...");
	sprintf(pd_filename, "The file \"%s\" is encrypted.", s);
	code = DialogBoxW(NULL, L"IDD_DLOGPASS", hwndframe, dlogpassproc);
	if (code <= 0)
		winerror(app, "cannot create password dialog");
	if (pd_okay)
		return pd_password;
	return NULL;
}

INT CALLBACK
dloginfoproc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	char buf[256];
	fz_document *doc = gapp.doc;

	switch(message)
	{
	case WM_INITDIALOG:

		SetDlgItemTextW(hwnd, 0x10, wbuf);

		if (fz_meta(doc, FZ_META_FORMAT_INFO, buf, 256) < 0)
		{
			SetDlgItemTextA(hwnd, 0x11, "Unknown");
			SetDlgItemTextA(hwnd, 0x12, "None");
			SetDlgItemTextA(hwnd, 0x13, "n/a");
			return TRUE;
		}

		SetDlgItemTextA(hwnd, 0x11, buf);

		if (fz_meta(doc, FZ_META_CRYPT_INFO, buf, 256) == 0)
		{
			SetDlgItemTextA(hwnd, 0x12, buf);
		}
		else
		{
			SetDlgItemTextA(hwnd, 0x12, "None");
		}
		buf[0] = 0;
		if (fz_meta(doc, FZ_META_HAS_PERMISSION, NULL, FZ_PERMISSION_PRINT) == 0)
			strcat(buf, "print, ");
		if (fz_meta(doc, FZ_META_HAS_PERMISSION, NULL, FZ_PERMISSION_CHANGE) == 0)
			strcat(buf, "modify, ");
		if (fz_meta(doc, FZ_META_HAS_PERMISSION, NULL, FZ_PERMISSION_COPY) == 0)
			strcat(buf, "copy, ");
		if (fz_meta(doc, FZ_META_HAS_PERMISSION, NULL, FZ_PERMISSION_NOTES) == 0)
			strcat(buf, "annotate, ");
		if (strlen(buf) > 2)
			buf[strlen(buf)-2] = 0;
		else
			strcpy(buf, "None");
		SetDlgItemTextA(hwnd, 0x13, buf);

#define SETUTF8(ID, STRING) \
		{ \
			*(char **)buf = STRING; \
			if (fz_meta(doc, FZ_META_INFO, buf, 256) <= 0) \
				buf[0] = 0; \
			SetDlgItemTextA(hwnd, ID, buf); \
		}

		SETUTF8(0x20, "Title");
		SETUTF8(0x21, "Author");
		SETUTF8(0x22, "Subject");
		SETUTF8(0x23, "Keywords");
		SETUTF8(0x24, "Creator");
		SETUTF8(0x25, "Producer");
		SETUTF8(0x26, "CreationDate");
		SETUTF8(0x27, "ModDate");
		return TRUE;

	case WM_COMMAND:
		EndDialog(hwnd, 1);
		return TRUE;
	}
	return FALSE;
}

void info()
{
	int code = DialogBoxW(NULL, L"IDD_DLOGINFO", hwndframe, dloginfoproc);
	if (code <= 0)
		winerror(&gapp, "cannot create info dialog");
}

INT CALLBACK
dlogaboutproc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message)
	{
	case WM_INITDIALOG:
		SetDlgItemTextA(hwnd, 2, pdfapp_version(&gapp));
		SetDlgItemTextA(hwnd, 3, pdfapp_usage(&gapp));
		return TRUE;
	case WM_COMMAND:
		EndDialog(hwnd, 1);
		return TRUE;
	}
	return FALSE;
}

void winhelp(pdfapp_t*app)
{
	int code = DialogBoxW(NULL, L"IDD_DLOGABOUT", hwndframe, dlogaboutproc);
	if (code <= 0)
		winerror(&gapp, "cannot create help dialog");
}

/*
 * Main window
 */

void winopen()
{
	WNDCLASS wc;
	HMENU menu;
	RECT r;
	ATOM a;

	/* Create and register window frame class */
	memset(&wc, 0, sizeof(wc));
	wc.style = 0;
	wc.lpfnWndProc = frameproc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = GetModuleHandle(NULL);
	wc.hIcon = LoadIconA(wc.hInstance, "IDI_ICONAPP");
	wc.hCursor = NULL; //LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = L"FrameWindow";
	a = RegisterClassW(&wc);
	if (!a)
		winerror(&gapp, "cannot register frame window class");

	/* Create and register window view class */
	memset(&wc, 0, sizeof(wc));
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = viewproc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = GetModuleHandle(NULL);
	wc.hIcon = NULL;
	wc.hCursor = NULL;
	wc.hbrBackground = NULL;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = L"ViewWindow";
	a = RegisterClassW(&wc);
	if (!a)
		winerror(&gapp, "cannot register view window class");

	/* Get screen size */
	SystemParametersInfo(SPI_GETWORKAREA, 0, &r, 0);
	gapp.scrw = r.right - r.left;
	gapp.scrh = r.bottom - r.top;

	/* Create cursors */
	arrowcurs = LoadCursor(NULL, IDC_ARROW);
	handcurs = LoadCursor(NULL, IDC_HAND);
	waitcurs = LoadCursor(NULL, IDC_WAIT);

	/* And a background color */
	bgbrush = CreateSolidBrush(RGB(0x70,0x70,0x70));
	shbrush = CreateSolidBrush(RGB(0x40,0x40,0x40));

	/* Init DIB info for buffer */
	dibinf = malloc(sizeof(BITMAPINFO) + 12);
	assert(dibinf);
	dibinf->bmiHeader.biSize = sizeof(dibinf->bmiHeader);
	dibinf->bmiHeader.biPlanes = 1;
	dibinf->bmiHeader.biBitCount = 32;
	dibinf->bmiHeader.biCompression = BI_RGB;
	dibinf->bmiHeader.biXPelsPerMeter = 2834;
	dibinf->bmiHeader.biYPelsPerMeter = 2834;
	dibinf->bmiHeader.biClrUsed = 0;
	dibinf->bmiHeader.biClrImportant = 0;
	dibinf->bmiHeader.biClrUsed = 0;

	/* Create window */
	hwndframe = CreateWindowW(L"FrameWindow", // window class name
	NULL, // window caption
	WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
	CW_USEDEFAULT, CW_USEDEFAULT, // initial position
	300, // initial x size
	300, // initial y size
	0, // parent window handle
	0, // window menu handle
	0, // program instance handle
	0); // creation parameters
	if (!hwndframe)
		winerror(&gapp, "cannot create frame");

	hwndview = CreateWindowW(L"ViewWindow", // window class name
	NULL,
	WS_VISIBLE | WS_CHILD,
	CW_USEDEFAULT, CW_USEDEFAULT,
	CW_USEDEFAULT, CW_USEDEFAULT,
	hwndframe, 0, 0, 0);
	if (!hwndview)
		winerror(&gapp, "cannot create view");

	hdc = NULL;

	SetWindowTextW(hwndframe, L"MuPDF");

	menu = GetSystemMenu(hwndframe, 0);
	AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
	AppendMenuW(menu, MF_STRING, ID_ABOUT, L"About MuPDF...");
	AppendMenuW(menu, MF_STRING, ID_DOCINFO, L"Document Properties...");

	SetCursor(arrowcurs);
}

void winclose(pdfapp_t *app)
{
	pdfapp_close(app);
	exit(0);
}

void wincursor(pdfapp_t *app, int curs)
{
	if (curs == ARROW)
		SetCursor(arrowcurs);
	if (curs == HAND)
		SetCursor(handcurs);
	if (curs == WAIT)
		SetCursor(waitcurs);
}

void wintitle(pdfapp_t *app, char *title)
{
	wchar_t wide[256], *dp;
	char *sp;
	int rune;

	dp = wide;
	sp = title;
	while (*sp && dp < wide + 255)
	{
		sp += fz_chartorune(&rune, sp);
		*dp++ = rune;
	}
	*dp = 0;

	SetWindowTextW(hwndframe, wide);
}

void windrawrect(pdfapp_t *app, int x0, int y0, int x1, int y1)
{
	RECT r;
	r.left = x0;
	r.top = y0;
	r.right = x1;
	r.bottom = y1;
	FillRect(hdc, &r, (HBRUSH)GetStockObject(WHITE_BRUSH));
}

void windrawstring(pdfapp_t *app, int x, int y, char *s)
{
	HFONT font = (HFONT)GetStockObject(ANSI_FIXED_FONT);
	SelectObject(hdc, font);
	TextOutA(hdc, x, y - 12, s, strlen(s));
}

void winblitsearch()
{
	if (gapp.isediting)
	{
		char buf[sizeof(gapp.search) + 50];
		sprintf(buf, "Search: %s", gapp.search);
		windrawrect(&gapp, 0, 0, gapp.winw, 30);
		windrawstring(&gapp, 10, 20, buf);
	}
}

void winblit()
{
	int image_w = fz_pixmap_width(gapp.ctx, gapp.image);
	int image_h = fz_pixmap_height(gapp.ctx, gapp.image);
	int image_n = fz_pixmap_components(gapp.ctx, gapp.image);
	unsigned char *samples = fz_pixmap_samples(gapp.ctx, gapp.image);
	int x0 = gapp.panx;
	int y0 = gapp.pany;
	int x1 = gapp.panx + image_w;
	int y1 = gapp.pany + image_h;
	RECT r;

	if (gapp.image)
	{
		if (gapp.iscopying || justcopied)
		{
			pdfapp_invert(&gapp, gapp.selr);
			justcopied = 1;
		}

		pdfapp_inverthit(&gapp);

		dibinf->bmiHeader.biWidth = image_w;
		dibinf->bmiHeader.biHeight = -image_h;
		dibinf->bmiHeader.biSizeImage = image_h * 4;

		if (image_n == 2)
		{
			int i = image_w * image_h;
			unsigned char *color = malloc(i*4);
			unsigned char *s = samples;
			unsigned char *d = color;
			for (; i > 0 ; i--)
			{
				d[2] = d[1] = d[0] = *s++;
				d[3] = *s++;
				d += 4;
			}
			SetDIBitsToDevice(hdc,
				gapp.panx, gapp.pany, image_w, image_h,
				0, 0, 0, image_h, color,
				dibinf, DIB_RGB_COLORS);
			free(color);
		}
		if (image_n == 4)
		{
			SetDIBitsToDevice(hdc,
				gapp.panx, gapp.pany, image_w, image_h,
				0, 0, 0, image_h, samples,
				dibinf, DIB_RGB_COLORS);
		}

		pdfapp_inverthit(&gapp);

		if (gapp.iscopying || justcopied)
		{
			pdfapp_invert(&gapp, gapp.selr);
			justcopied = 1;
		}
	}

	/* Grey background */
	r.top = 0; r.bottom = gapp.winh;
	r.left = 0; r.right = x0;
	FillRect(hdc, &r, bgbrush);
	r.left = x1; r.right = gapp.winw;
	FillRect(hdc, &r, bgbrush);
	r.left = 0; r.right = gapp.winw;
	r.top = 0; r.bottom = y0;
	FillRect(hdc, &r, bgbrush);
	r.top = y1; r.bottom = gapp.winh;
	FillRect(hdc, &r, bgbrush);

	/* Drop shadow */
	r.left = x0 + 2;
	r.right = x1 + 2;
	r.top = y1;
	r.bottom = y1 + 2;
	FillRect(hdc, &r, shbrush);
	r.left = x1;
	r.right = x1 + 2;
	r.top = y0 + 2;
	r.bottom = y1;
	FillRect(hdc, &r, shbrush);

	winblitsearch();
}

void winresize(pdfapp_t *app, int w, int h)
{
	ShowWindow(hwndframe, SW_SHOWDEFAULT);
	w += GetSystemMetrics(SM_CXFRAME) * 2;
	h += GetSystemMetrics(SM_CYFRAME) * 2;
	h += GetSystemMetrics(SM_CYCAPTION);
	SetWindowPos(hwndframe, 0, 0, 0, w, h, SWP_NOZORDER | SWP_NOMOVE);
}

void winrepaint(pdfapp_t *app)
{
	InvalidateRect(hwndview, NULL, 0);
}

void winrepaintsearch(pdfapp_t *app)
{
	// TODO: invalidate only search area and
	// call only search redraw routine.
	InvalidateRect(hwndview, NULL, 0);
}

void winfullscreen(pdfapp_t *app, int state)
{
	static WINDOWPLACEMENT savedplace;
	static int isfullscreen = 0;
	if (state && !isfullscreen)
	{
		GetWindowPlacement(hwndframe, &savedplace);
		SetWindowLong(hwndframe, GWL_STYLE, WS_POPUP | WS_VISIBLE);
		SetWindowPos(hwndframe, NULL, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
		ShowWindow(hwndframe, SW_SHOWMAXIMIZED);
		isfullscreen = 1;
	}
	if (!state && isfullscreen)
	{
		SetWindowLong(hwndframe, GWL_STYLE, WS_OVERLAPPEDWINDOW);
		SetWindowPos(hwndframe, NULL, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
		SetWindowPlacement(hwndframe, &savedplace);
		isfullscreen = 0;
	}
}

/*
 * Event handling
 */

void windocopy(pdfapp_t *app)
{
	HGLOBAL handle;
	unsigned short *ucsbuf;

	if (!OpenClipboard(hwndframe))
		return;
	EmptyClipboard();

	handle = GlobalAlloc(GMEM_MOVEABLE, 4096 * sizeof(unsigned short));
	if (!handle)
	{
		CloseClipboard();
		return;
	}

	ucsbuf = GlobalLock(handle);
	pdfapp_oncopy(&gapp, ucsbuf, 4096);
	GlobalUnlock(handle);

	SetClipboardData(CF_UNICODETEXT, handle);
	CloseClipboard();

	justcopied = 1;	/* keep inversion around for a while... */
}

void winreloadfile(pdfapp_t *app)
{
	pdfapp_close(app);
	pdfapp_open(app, filename, 1);
}

void winopenuri(pdfapp_t *app, char *buf)
{
	ShellExecuteA(hwndframe, "open", buf, 0, 0, SW_SHOWNORMAL);
}

void handlekey(int c)
{
	if (GetCapture() == hwndview)
		return;

	if (justcopied)
	{
		justcopied = 0;
		winrepaint(&gapp);
	}

	/* translate VK into ASCII equivalents */
	if (c > 256)
	{
		switch (c - 256)
		{
		case VK_F1: c = '?'; break;
		case VK_ESCAPE: c = '\033'; break;
		case VK_DOWN: c = 'j'; break;
		case VK_UP: c = 'k'; break;
		case VK_LEFT: c = 'b'; break;
		case VK_RIGHT: c = ' '; break;
		case VK_PRIOR: c = ','; break;
		case VK_NEXT: c = '.'; break;
		}
	}

	pdfapp_onkey(&gapp, c);
	winrepaint(&gapp);
}

void handlemouse(int x, int y, int btn, int state)
{
	if (state != 0 && justcopied)
	{
		justcopied = 0;
		winrepaint(&gapp);
	}

	if (state == 1)
		SetCapture(hwndview);
	if (state == -1)
		ReleaseCapture();

	pdfapp_onmouse(&gapp, x, y, btn, 0, state);
}

LRESULT CALLBACK
frameproc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message)
	{
	case WM_SETFOCUS:
		PostMessage(hwnd, WM_APP+5, 0, 0);
		return 0;
	case WM_APP+5:
		SetFocus(hwndview);
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	case WM_SYSCOMMAND:
		if (wParam == ID_ABOUT)
		{
			winhelp(&gapp);
			return 0;
		}
		if (wParam == ID_DOCINFO)
		{
			info();
			return 0;
		}
		if (wParam == SC_MAXIMIZE)
			gapp.shrinkwrap = 0;
		break;

	case WM_SIZE:
	{
		// More generally, you should use GetEffectiveClientRect
		// if you have a toolbar etc.
		RECT rect;
		GetClientRect(hwnd, &rect);
		MoveWindow(hwndview, rect.left, rect.top,
		rect.right-rect.left, rect.bottom-rect.top, TRUE);
		return 0;
	}

	case WM_SIZING:
		gapp.shrinkwrap = 0;
		break;

	case WM_NOTIFY:
	case WM_COMMAND:
		return SendMessage(hwndview, message, wParam, lParam);
	}

	return DefWindowProc(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK
viewproc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static int oldx = 0;
	static int oldy = 0;
	int x = (signed short) LOWORD(lParam);
	int y = (signed short) HIWORD(lParam);

	switch (message)
	{
	case WM_SIZE:
		if (wParam == SIZE_MINIMIZED)
			return 0;
		if (wParam == SIZE_MAXIMIZED)
			gapp.shrinkwrap = 0;
		pdfapp_onresize(&gapp, LOWORD(lParam), HIWORD(lParam));
		break;

	/* Paint events are low priority and automagically catenated
	 * so we don't need to do any fancy waiting to defer repainting.
	 */
	case WM_PAINT:
	{
		//puts("WM_PAINT");
		PAINTSTRUCT ps;
		hdc = BeginPaint(hwnd, &ps);
		winblit();
		hdc = NULL;
		EndPaint(hwnd, &ps);
		return 0;
	}

	case WM_ERASEBKGND:
		return 1; // well, we don't need to erase to redraw cleanly

	/* Mouse events */

	case WM_LBUTTONDOWN:
		SetFocus(hwndview);
		oldx = x; oldy = y;
		handlemouse(x, y, 1, 1);
		return 0;
	case WM_MBUTTONDOWN:
		SetFocus(hwndview);
		oldx = x; oldy = y;
		handlemouse(x, y, 2, 1);
		return 0;
	case WM_RBUTTONDOWN:
		SetFocus(hwndview);
		oldx = x; oldy = y;
		handlemouse(x, y, 3, 1);
		return 0;

	case WM_LBUTTONUP:
		oldx = x; oldy = y;
		handlemouse(x, y, 1, -1);
		return 0;
	case WM_MBUTTONUP:
		oldx = x; oldy = y;
		handlemouse(x, y, 2, -1);
		return 0;
	case WM_RBUTTONUP:
		oldx = x; oldy = y;
		handlemouse(x, y, 3, -1);
		return 0;

	case WM_MOUSEMOVE:
		oldx = x; oldy = y;
		handlemouse(x, y, 0, 0);
		return 0;

	/* Mouse wheel */

	case WM_MOUSEWHEEL:
		if ((signed short)HIWORD(wParam) > 0)
			handlekey(LOWORD(wParam) & MK_SHIFT ? '+' : 'k');
		else
			handlekey(LOWORD(wParam) & MK_SHIFT ? '-' : 'j');
		return 0;

	/* Keyboard events */

	case WM_KEYDOWN:
		/* only handle special keys */
		switch (wParam)
		{
		case VK_F1:
		case VK_LEFT:
		case VK_UP:
		case VK_PRIOR:
		case VK_RIGHT:
		case VK_DOWN:
		case VK_NEXT:
		case VK_ESCAPE:
			handlekey(wParam + 256);
			handlemouse(oldx, oldy, 0, 0);	/* update cursor */
			return 0;
		}
		return 1;

	/* unicode encoded chars, including escape, backspace etc... */
	case WM_CHAR:
		if (wParam < 256)
		{
			handlekey(wParam);
			handlemouse(oldx, oldy, 0, 0);	/* update cursor */
		}
		return 0;
	}

	fflush(stdout);

	/* Pass on unhandled events to Windows */
	return DefWindowProc(hwnd, message, wParam, lParam);
}

int WINAPI
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	int argc;
	LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	char argv0[256];
	MSG msg;
	int code;
	fz_context *ctx;

	ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);
	if (!ctx)
	{
		fprintf(stderr, "cannot initialise context\n");
		exit(1);
	}
	pdfapp_init(ctx, &gapp);

	GetModuleFileNameA(NULL, argv0, sizeof argv0);
	install_app(argv0);

	winopen();

	if (argc == 2)
	{
		wcscpy(wbuf, argv[1]);
	}
	else
	{
		if (!winfilename(wbuf, nelem(wbuf)))
			exit(0);
	}

	code = WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, filename, sizeof filename, NULL, NULL);
	if (code == 0)
		winerror(&gapp, "cannot convert filename to utf-8");

	pdfapp_open(&gapp, filename, 0);

	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	pdfapp_close(&gapp);

	return 0;
}
