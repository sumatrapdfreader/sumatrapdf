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

/* Include pdfapp.h *AFTER* the UNICODE defines */
#include "pdfapp.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL 0x020A
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define MIN(x,y) ((x) < (y) ? (x) : (y))

#define ID_ABOUT	0x1000
#define ID_DOCINFO	0x1001

static HWND hwndframe = NULL;
static HWND hwndview = NULL;
static HDC hdc;
static HBRUSH bgbrush;
static BITMAPINFO *dibinf = NULL;
static HCURSOR arrowcurs, handcurs, waitcurs, caretcurs;
static LRESULT CALLBACK frameproc(HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK viewproc(HWND, UINT, WPARAM, LPARAM);
static int timer_pending = 0;
static char *password = NULL;

static int justcopied = 0;

static pdfapp_t gapp;

static wchar_t wbuf[PATH_MAX];
static char filename[PATH_MAX];

/*
 * Create registry keys to associate MuPDF with PDF and XPS files.
 */

#define OPEN_KEY(parent, name, ptr) \
	RegCreateKeyExA(parent, name, 0, 0, 0, KEY_WRITE, 0, &ptr, 0)

#define SET_KEY(parent, name, value) \
	RegSetValueExA(parent, name, 0, REG_SZ, (const BYTE *)(value), (DWORD)strlen(value) + 1)

static void install_app(char *argv0)
{
	char buf[512];
	HKEY software, classes, mupdf, dotpdf, dotxps, dotepub, dotfb2;
	HKEY shell, open, command, supported_types;
	HKEY pdf_progids, xps_progids, epub_progids, fb2_progids;

	OPEN_KEY(HKEY_CURRENT_USER, "Software", software);
	OPEN_KEY(software, "Classes", classes);
	OPEN_KEY(classes, ".pdf", dotpdf);
	OPEN_KEY(dotpdf, "OpenWithProgids", pdf_progids);
	OPEN_KEY(classes, ".xps", dotxps);
	OPEN_KEY(dotxps, "OpenWithProgids", xps_progids);
	OPEN_KEY(classes, ".epub", dotepub);
	OPEN_KEY(dotepub, "OpenWithProgids", epub_progids);
	OPEN_KEY(classes, ".fb2", dotfb2);
	OPEN_KEY(dotfb2, "OpenWithProgids", fb2_progids);
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
	SET_KEY(supported_types, ".epub", "");
	SET_KEY(pdf_progids, "MuPDF", "");
	SET_KEY(xps_progids, "MuPDF", "");
	SET_KEY(epub_progids, "MuPDF", "");
	SET_KEY(fb2_progids, "MuPDF", "");

	RegCloseKey(dotfb2);
	RegCloseKey(dotepub);
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

void winalert(pdfapp_t *app, pdf_alert_event *alert)
{
	int buttons = MB_OK;
	int icon = MB_ICONWARNING;
	int pressed = PDF_ALERT_BUTTON_NONE;

	switch (alert->icon_type)
	{
	case PDF_ALERT_ICON_ERROR:
		icon = MB_ICONERROR;
		break;
	case PDF_ALERT_ICON_WARNING:
		icon = MB_ICONWARNING;
		break;
	case PDF_ALERT_ICON_QUESTION:
		icon = MB_ICONQUESTION;
		break;
	case PDF_ALERT_ICON_STATUS:
		icon = MB_ICONINFORMATION;
		break;
	}

	switch (alert->button_group_type)
	{
	case PDF_ALERT_BUTTON_GROUP_OK:
		buttons = MB_OK;
		break;
	case PDF_ALERT_BUTTON_GROUP_OK_CANCEL:
		buttons = MB_OKCANCEL;
		break;
	case PDF_ALERT_BUTTON_GROUP_YES_NO:
		buttons = MB_YESNO;
		break;
	case PDF_ALERT_BUTTON_GROUP_YES_NO_CANCEL:
		buttons = MB_YESNOCANCEL;
		break;
	}

	pressed = MessageBoxA(hwndframe, alert->message, alert->title, icon|buttons);

	switch (pressed)
	{
	case IDOK:
		alert->button_pressed = PDF_ALERT_BUTTON_OK;
		break;
	case IDCANCEL:
		alert->button_pressed = PDF_ALERT_BUTTON_CANCEL;
		break;
	case IDNO:
		alert->button_pressed = PDF_ALERT_BUTTON_NO;
		break;
	case IDYES:
		alert->button_pressed = PDF_ALERT_BUTTON_YES;
	}
}

void winprint(pdfapp_t *app)
{
	MessageBoxA(hwndframe, "The MuPDF library supports printing, but this application currently does not", "Print document", MB_ICONWARNING);
}

int winsavequery(pdfapp_t *app)
{
	switch(MessageBoxA(hwndframe, "File has unsaved changes. Do you want to save", "MuPDF", MB_YESNOCANCEL))
	{
	case IDYES: return SAVE;
	case IDNO: return DISCARD;
	default: return CANCEL;
	}
}

int winquery(pdfapp_t *app, const char *query)
{
	switch(MessageBoxA(hwndframe, query, "MuPDF", MB_YESNOCANCEL))
	{
	case IDYES: return QUERY_YES;
	case IDNO:
	default: return QUERY_NO;
	}
}

static int winfilename(wchar_t *buf, int len)
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
	ofn.lpstrFilter = L"Documents (*.pdf;*.xps;*.cbz;*.epub;*.fb2;*.zip;*.png;*.jpeg;*.tiff)\0*.zip;*.cbz;*.xps;*.epub;*.fb2;*.pdf;*.jpe;*.jpg;*.jpeg;*.jfif;*.tif;*.tiff\0PDF Files (*.pdf)\0*.pdf\0XPS Files (*.xps)\0*.xps\0CBZ Files (*.cbz;*.zip)\0*.zip;*.cbz\0EPUB Files (*.epub)\0*.epub\0FictionBook 2 Files (*.fb2)\0*.fb2\0Image Files (*.png;*.jpeg;*.tiff)\0*.png;*.jpg;*.jpe;*.jpeg;*.jfif;*.tif;*.tiff\0All Files\0*\0\0";
	ofn.Flags = OFN_FILEMUSTEXIST|OFN_HIDEREADONLY;
	return GetOpenFileNameW(&ofn);
}

int wingetcertpath(pdfapp_t *app, char *buf, int len)
{
	wchar_t twbuf[PATH_MAX] = {0};
	OPENFILENAME ofn;
	buf[0] = 0;
	memset(&ofn, 0, sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hwndframe;
	ofn.lpstrFile = twbuf;
	ofn.nMaxFile = PATH_MAX;
	ofn.lpstrInitialDir = NULL;
	ofn.lpstrTitle = L"MuPDF: Select certificate file";
	ofn.lpstrFilter = L"Certificates (*.pfx)\0*.pfx\0All files\0*\0\0";
	ofn.Flags = OFN_FILEMUSTEXIST;
	if (GetOpenFileNameW(&ofn))
	{
		int code = WideCharToMultiByte(CP_UTF8, 0, twbuf, -1, buf, MIN(PATH_MAX, len), NULL, NULL);
		if (code == 0)
		{
			pdfapp_error(app, "cannot convert filename to utf-8");
			return 0;
		}

		return 1;
	}
	else
	{
		return 0;
	}
}

int wingetsavepath(pdfapp_t *app, char *buf, int len)
{
	wchar_t twbuf[PATH_MAX];
	OPENFILENAME ofn;

	wcscpy(twbuf, wbuf);
	memset(&ofn, 0, sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hwndframe;
	ofn.lpstrFile = twbuf;
	ofn.nMaxFile = PATH_MAX;
	ofn.lpstrInitialDir = NULL;
	ofn.lpstrTitle = L"MuPDF: Save PDF file";
	ofn.lpstrFilter = L"PDF Documents (*.pdf)\0*.pdf\0All Files\0*\0\0";
	ofn.Flags = OFN_HIDEREADONLY;
	if (GetSaveFileName(&ofn))
	{
		int code = WideCharToMultiByte(CP_UTF8, 0, twbuf, -1, buf, MIN(PATH_MAX, len), NULL, NULL);
		if (code == 0)
		{
			pdfapp_error(app, "cannot convert filename to utf-8");
			return 0;
		}

		wcscpy(wbuf, twbuf);
		strcpy(filename, buf);
		return 1;
	}
	else
	{
		return 0;
	}
}

void winreplacefile(pdfapp_t *app, char *source, char *target)
{
	wchar_t wsource[PATH_MAX];
	wchar_t wtarget[PATH_MAX];

	int sz = MultiByteToWideChar(CP_UTF8, 0, source, -1, wsource, PATH_MAX);
	if (sz == 0)
	{
		pdfapp_error(app, "cannot convert filename to Unicode");
		return;
	}

	sz = MultiByteToWideChar(CP_UTF8, 0, target, -1, wtarget, PATH_MAX);
	if (sz == 0)
	{
		pdfapp_error(app, "cannot convert filename to Unicode");
		return;
	}

#if (_WIN32_WINNT >= 0x0500)
	ReplaceFile(wtarget, wsource, NULL, REPLACEFILE_IGNORE_MERGE_ERRORS, NULL, NULL);
#else
	DeleteFile(wtarget);
	MoveFile(wsource, wtarget);
#endif
}

void wincopyfile(pdfapp_t *app, char *source, char *target)
{
	wchar_t wsource[PATH_MAX];
	wchar_t wtarget[PATH_MAX];

	int sz = MultiByteToWideChar(CP_UTF8, 0, source, -1, wsource, PATH_MAX);
	if (sz == 0)
	{
		pdfapp_error(app, "cannot convert filename to Unicode");
		return;
	}

	sz = MultiByteToWideChar(CP_UTF8, 0, target, -1, wtarget, PATH_MAX);
	if (sz == 0)
	{
		pdfapp_error(app, "cannot convert filename to Unicode");
		return;
	}

	CopyFile(wsource, wtarget, FALSE);
}

static char pd_filename[256] = "The file is encrypted.";
static char pd_password[256] = "";
static wchar_t pd_passwordw[256] = {0};
static char td_textinput[1024] = "";
static int td_retry = 0;
static int cd_nopts;
static int *cd_nvals;
static const char **cd_opts;
static const char **cd_vals;
static int pd_okay = 0;

static INT_PTR CALLBACK
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
			GetDlgItemTextW(hwnd, 3, pd_passwordw, nelem(pd_passwordw));
			EndDialog(hwnd, 1);
			WideCharToMultiByte(CP_UTF8, 0, pd_passwordw, -1, pd_password, sizeof pd_password, NULL, NULL);
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

static INT_PTR CALLBACK
dlogtextproc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message)
	{
	case WM_INITDIALOG:
		SetDlgItemTextA(hwnd, 3, td_textinput);
		if (!td_retry)
			ShowWindow(GetDlgItem(hwnd, 4), SW_HIDE);
		return TRUE;
	case WM_COMMAND:
		switch(wParam)
		{
		case 1:
			pd_okay = 1;
			GetDlgItemTextA(hwnd, 3, td_textinput, sizeof td_textinput);
			EndDialog(hwnd, 1);
			return TRUE;
		case 2:
			pd_okay = 0;
			EndDialog(hwnd, 1);
			return TRUE;
		}
		break;
	case WM_CTLCOLORSTATIC:
		if ((HWND)lParam == GetDlgItem(hwnd, 4))
		{
			SetTextColor((HDC)wParam, RGB(255,0,0));
			SetBkMode((HDC)wParam, TRANSPARENT);

			return (INT_PTR)GetStockObject(NULL_BRUSH);
		}
		break;
	}
	return FALSE;
}

static INT_PTR CALLBACK
dlogchoiceproc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	HWND listbox;
	int i;
	int item;
	int sel;
	switch(message)
	{
	case WM_INITDIALOG:
		listbox = GetDlgItem(hwnd, 3);
		for (i = 0; i < cd_nopts; i++)
			SendMessageA(listbox, LB_ADDSTRING, 0, (LPARAM)cd_opts[i]);

		/* FIXME: handle multiple select */
		if (*cd_nvals > 0)
		{
			item = SendMessageA(listbox, LB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)cd_vals[0]);
			if (item != LB_ERR)
				SendMessageA(listbox, LB_SETCURSEL, item, 0);
		}
		return TRUE;
	case WM_COMMAND:
		switch(wParam)
		{
		case 1:
			listbox = GetDlgItem(hwnd, 3);
			*cd_nvals = 0;
			for (i = 0; i < cd_nopts; i++)
			{
				item = SendMessageA(listbox, LB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)cd_opts[i]);
				sel = SendMessageA(listbox, LB_GETSEL, item, 0);
				if (sel && sel != LB_ERR)
					cd_vals[(*cd_nvals)++] = cd_opts[i];
			}
			pd_okay = 1;
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

	if (password)
	{
		char *p = password;
		password = NULL;
		return p;
	}

	strcpy(buf, filename);
	s = buf;
	if (strrchr(s, '\\')) s = strrchr(s, '\\') + 1;
	if (strrchr(s, '/')) s = strrchr(s, '/') + 1;
	if (strlen(s) > 32)
		strcpy(s + 30, "...");
	sprintf(pd_filename, "The file \"%s\" is encrypted.", s);
	code = DialogBoxW(NULL, L"IDD_DLOGPASS", hwndframe, dlogpassproc);
	if (code <= 0)
		pdfapp_error(app, "cannot create password dialog");
	if (pd_okay)
		return pd_password;
	return NULL;
}

char *wintextinput(pdfapp_t *app, char *inittext, int retry)
{
	int code;
	td_retry = retry;
	fz_strlcpy(td_textinput, inittext ? inittext : "", sizeof td_textinput);
	code = DialogBoxW(NULL, L"IDD_DLOGTEXT", hwndframe, dlogtextproc);
	if (code <= 0)
		pdfapp_error(app, "cannot create text input dialog");
	if (pd_okay)
		return td_textinput;
	return NULL;
}

int winchoiceinput(pdfapp_t *app, int nopts, const char *opts[], int *nvals, const char *vals[])
{
	int code;
	cd_nopts = nopts;
	cd_nvals = nvals;
	cd_opts = opts;
	cd_vals = vals;
	code = DialogBoxW(NULL, L"IDD_DLOGLIST", hwndframe, dlogchoiceproc);
	if (code <= 0)
		pdfapp_error(app, "cannot create text input dialog");
	return pd_okay;
}

static INT_PTR CALLBACK
dloginfoproc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	char buf[256];
	wchar_t bufx[256];
	fz_context *ctx = gapp.ctx;
	fz_document *doc = gapp.doc;

	switch(message)
	{
	case WM_INITDIALOG:

		SetDlgItemTextW(hwnd, 0x10, wbuf);

		if (fz_lookup_metadata(ctx, doc, FZ_META_FORMAT, buf, sizeof buf) >= 0)
		{
			SetDlgItemTextA(hwnd, 0x11, buf);
		}
		else
		{
			SetDlgItemTextA(hwnd, 0x11, "Unknown");
			SetDlgItemTextA(hwnd, 0x12, "None");
			SetDlgItemTextA(hwnd, 0x13, "n/a");
			return TRUE;
		}

		if (fz_lookup_metadata(ctx, doc, FZ_META_ENCRYPTION, buf, sizeof buf) >= 0)
		{
			SetDlgItemTextA(hwnd, 0x12, buf);
		}
		else
		{
			SetDlgItemTextA(hwnd, 0x12, "None");
		}

		buf[0] = 0;
		if (fz_has_permission(ctx, doc, FZ_PERMISSION_PRINT))
			strcat(buf, "print, ");
		if (fz_has_permission(ctx, doc, FZ_PERMISSION_COPY))
			strcat(buf, "copy, ");
		if (fz_has_permission(ctx, doc, FZ_PERMISSION_EDIT))
			strcat(buf, "edit, ");
		if (fz_has_permission(ctx, doc, FZ_PERMISSION_ANNOTATE))
			strcat(buf, "annotate, ");
		if (strlen(buf) > 2)
			buf[strlen(buf)-2] = 0;
		else
			strcpy(buf, "none");
		SetDlgItemTextA(hwnd, 0x13, buf);

#define SETUTF8(ID, STRING) \
		if (fz_lookup_metadata(ctx, doc, "info:" STRING, buf, sizeof buf) >= 0) \
		{ \
			MultiByteToWideChar(CP_UTF8, 0, buf, -1, bufx, nelem(bufx)); \
			SetDlgItemTextW(hwnd, ID, bufx); \
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

static void info()
{
	int code = DialogBoxW(NULL, L"IDD_DLOGINFO", hwndframe, dloginfoproc);
	if (code <= 0)
		pdfapp_error(&gapp, "cannot create info dialog");
}

static INT_PTR CALLBACK
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
		pdfapp_error(&gapp, "cannot create help dialog");
}

/*
 * Main window
 */

static void winopen()
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
		pdfapp_error(&gapp, "cannot register frame window class");

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
		pdfapp_error(&gapp, "cannot register view window class");

	/* Get screen size */
	SystemParametersInfo(SPI_GETWORKAREA, 0, &r, 0);
	gapp.scrw = r.right - r.left;
	gapp.scrh = r.bottom - r.top;

	/* Create cursors */
	arrowcurs = LoadCursor(NULL, IDC_ARROW);
	handcurs = LoadCursor(NULL, IDC_HAND);
	waitcurs = LoadCursor(NULL, IDC_WAIT);
	caretcurs = LoadCursor(NULL, IDC_IBEAM);

	/* And a background color */
	bgbrush = CreateSolidBrush(RGB(0x70,0x70,0x70));

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
		pdfapp_error(&gapp, "cannot create frame");

	hwndview = CreateWindowW(L"ViewWindow", // window class name
	NULL,
	WS_VISIBLE | WS_CHILD,
	CW_USEDEFAULT, CW_USEDEFAULT,
	CW_USEDEFAULT, CW_USEDEFAULT,
	hwndframe, 0, 0, 0);
	if (!hwndview)
		pdfapp_error(&gapp, "cannot create view");

	hdc = NULL;

	SetWindowTextW(hwndframe, L"MuPDF");

	menu = GetSystemMenu(hwndframe, 0);
	AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
	AppendMenuW(menu, MF_STRING, ID_ABOUT, L"About MuPDF...");
	AppendMenuW(menu, MF_STRING, ID_DOCINFO, L"Document Properties...");

	SetCursor(arrowcurs);
}

static void
do_close(pdfapp_t *app)
{
	fz_context *ctx = app->ctx;
	pdfapp_close(app);
	free(dibinf);
	fz_drop_context(ctx);
}

void winclose(pdfapp_t *app)
{
	if (pdfapp_preclose(app))
	{
		do_close(app);
		exit(0);
	}
}

void wincursor(pdfapp_t *app, int curs)
{
	if (curs == ARROW)
		SetCursor(arrowcurs);
	if (curs == HAND)
		SetCursor(handcurs);
	if (curs == WAIT)
		SetCursor(waitcurs);
	if (curs == CARET)
		SetCursor(caretcurs);
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

static void windrawrect(pdfapp_t *app, int x0, int y0, int x1, int y1)
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
	TextOutA(hdc, x, y - 12, s, (int)strlen(s));
}

static void winblitsearch()
{
	if (gapp.issearching)
	{
		char buf[sizeof(gapp.search) + 50];
		sprintf(buf, "Search: %s", gapp.search);
		windrawrect(&gapp, 0, 0, gapp.winw, 30);
		windrawstring(&gapp, 10, 20, buf);
	}
}

static void winblit()
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
	HBRUSH brush;

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
			size_t i = image_w * (size_t)image_h;
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

	if (gapp.invert)
		brush = (HBRUSH)GetStockObject(BLACK_BRUSH);
	else
		brush = bgbrush;

	/* Grey background */
	r.top = 0; r.bottom = gapp.winh;
	r.left = 0; r.right = x0;
	FillRect(hdc, &r, brush);
	r.left = x1; r.right = gapp.winw;
	FillRect(hdc, &r, brush);
	r.left = 0; r.right = gapp.winw;
	r.top = 0; r.bottom = y0;
	FillRect(hdc, &r, brush);
	r.top = y1; r.bottom = gapp.winh;
	FillRect(hdc, &r, brush);

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

void winreloadpage(pdfapp_t *app)
{
	SendMessage(hwndview, WM_APP, 0, 0);
}

void winopenuri(pdfapp_t *app, char *buf)
{
	ShellExecuteA(hwndframe, "open", buf, 0, 0, SW_SHOWNORMAL);
}

#define OUR_TIMER_ID 1

void winadvancetimer(pdfapp_t *app, float delay)
{
	timer_pending = 1;
	SetTimer(hwndview, OUR_TIMER_ID, (unsigned int)(1000*delay), NULL);
}

static void killtimer(pdfapp_t *app)
{
	timer_pending = 0;
}

static void handlekey(int c)
{
	int modifier = (GetAsyncKeyState(VK_SHIFT) < 0);
	modifier |= ((GetAsyncKeyState(VK_CONTROL) < 0)<<2);

	if (timer_pending)
		killtimer(&gapp);

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
		case VK_ESCAPE: c = '\033'; break;
		case VK_DOWN: c = 'j'; break;
		case VK_UP: c = 'k'; break;
		case VK_LEFT: c = 'h'; break;
		case VK_RIGHT: c = 'l'; break;
		case VK_PRIOR: c = ','; break;
		case VK_NEXT: c = '.'; break;
		}
	}

	pdfapp_onkey(&gapp, c, modifier);
	winrepaint(&gapp);
}

static void handlemouse(int x, int y, int btn, int state)
{
	int modifier = (GetAsyncKeyState(VK_SHIFT) < 0);
	modifier |= ((GetAsyncKeyState(VK_CONTROL) < 0)<<2);

	if (state != 0 && timer_pending)
		killtimer(&gapp);

	if (state != 0 && justcopied)
	{
		justcopied = 0;
		winrepaint(&gapp);
	}

	if (state == 1)
		SetCapture(hwndview);
	if (state == -1)
		ReleaseCapture();

	pdfapp_onmouse(&gapp, x, y, btn, modifier, state);
}

static LRESULT CALLBACK
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
		break;

	case WM_SIZE:
	{
		// More generally, you should use GetEffectiveClientRect
		// if you have a toolbar etc.
		RECT rect;
		GetClientRect(hwnd, &rect);
		MoveWindow(hwndview, rect.left, rect.top,
		rect.right-rect.left, rect.bottom-rect.top, TRUE);
		if (wParam == SIZE_MAXIMIZED)
			gapp.shrinkwrap = 0;
		return 0;
	}

	case WM_SIZING:
		gapp.shrinkwrap = 0;
		break;

	case WM_NOTIFY:
	case WM_COMMAND:
		return SendMessage(hwndview, message, wParam, lParam);

	case WM_CLOSE:
		if (!pdfapp_preclose(&gapp))
			return 0;
	}

	return DefWindowProc(hwnd, message, wParam, lParam);
}

static LRESULT CALLBACK
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
		pdfapp_postblit(&gapp);
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
		if ((signed short)HIWORD(wParam) <= 0)
		{
			handlemouse(oldx, oldy, 5, 1);
			handlemouse(oldx, oldy, 5, -1);
		}
		else
		{
			handlemouse(oldx, oldy, 4, 1);
			handlemouse(oldx, oldy, 4, -1);
		}
		return 0;

	/* Timer */
	case WM_TIMER:
		if (wParam == OUR_TIMER_ID && timer_pending && gapp.presentation_mode)
		{
			timer_pending = 0;
			handlekey(VK_RIGHT + 256);
			handlemouse(oldx, oldy, 0, 0); /* update cursor */
			return 0;
		}
		break;

	/* Keyboard events */

	case WM_KEYDOWN:
		/* only handle special keys */
		switch (wParam)
		{
		case VK_F1:
			winhelp(&gapp);
			return 0;
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

	/* We use WM_APP to trigger a reload and repaint of a page */
	case WM_APP:
		pdfapp_reloadpage(&gapp);
		break;
	}

	fflush(stdout);

	/* Pass on unhandled events to Windows */
	return DefWindowProc(hwnd, message, wParam, lParam);
}

typedef BOOL (SetProcessDPIAwareFn)(void);

static int
get_system_dpi(void)
{
	HMODULE hUser32 = LoadLibrary(TEXT("user32.dll"));
	SetProcessDPIAwareFn *ptr;
	int hdpi, vdpi;
	HDC desktopDC;

	ptr = (SetProcessDPIAwareFn *)GetProcAddress(hUser32, "SetProcessDPIAware");
	if (ptr != NULL)
		ptr();
	FreeLibrary(hUser32);

	desktopDC = GetDC(NULL);
	hdpi = GetDeviceCaps(desktopDC, LOGPIXELSX);
	vdpi = GetDeviceCaps(desktopDC, LOGPIXELSY);
	/* hdpi,vdpi = 100 means 96dpi. */
	return ((hdpi + vdpi) * 96 + 0.5f) / 200;
}

static void usage(const char *argv0)
{
	const char *msg =
		"usage: mupdf [options] file.pdf [page]\n"
		"\t-p -\tpassword\n"
		"\t-r -\tresolution\n"
		"\t-A -\tset anti-aliasing quality in bits (0=off, 8=best)\n"
		"\t-C -\tRRGGBB (tint color in hexadecimal syntax)\n"
		"\t-W -\tpage width for EPUB layout\n"
		"\t-H -\tpage height for EPUB layout\n"
		"\t-I -\tinvert colors\n"
		"\t-S -\tfont size for EPUB layout\n"
		"\t-U -\tuser style sheet for EPUB layout\n"
		"\t-X\tdisable document styles for EPUB layout\n";
	MessageBoxA(NULL, msg, "MuPDF: Usage", MB_OK);
	exit(1);
}

int WINAPI
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	int argc;
	LPWSTR *wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
	char **argv;
	char argv0[256];
	MSG msg;
	int code;
	fz_context *ctx;
	int kbps = 0;
	int displayRes = get_system_dpi();
	int c;

	ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);
	if (!ctx)
	{
		MessageBoxA(NULL, "Cannot initialize MuPDF context.", "MuPDF: Error", MB_OK);
		exit(1);
	}
	pdfapp_init(ctx, &gapp);

	argv = fz_argv_from_wargv(argc, wargv);

	while ((c = fz_getopt(argc, argv, "Ip:r:A:C:W:H:S:U:Xb:")) != -1)
	{
		switch (c)
		{
		case 'C':
			c = strtol(fz_optarg, NULL, 16);
			gapp.tint = 1;
			gapp.tint_white = c;
			break;
		case 'p': password = fz_optarg; break;
		case 'r': displayRes = fz_atoi(fz_optarg); break;
		case 'I': gapp.invert = 1; break;
		case 'A': fz_set_aa_level(ctx, fz_atoi(fz_optarg)); break;
		case 'W': gapp.layout_w = fz_atoi(fz_optarg); break;
		case 'H': gapp.layout_h = fz_atoi(fz_optarg); break;
		case 'S': gapp.layout_em = fz_atoi(fz_optarg); break;
		case 'b': kbps = fz_atoi(fz_optarg); break;
		case 'U': gapp.layout_css = fz_optarg; break;
		case 'X': gapp.layout_use_doc_css = 0; break;
		default: usage(argv[0]);
		}
	}

	pdfapp_setresolution(&gapp, displayRes);

	GetModuleFileNameA(NULL, argv0, sizeof argv0);
	install_app(argv0);

	winopen();

	if (fz_optind < argc)
	{
		strcpy(filename, argv[fz_optind++]);
	}
	else
	{
		if (!winfilename(wbuf, nelem(wbuf)))
			exit(0);
		code = WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, filename, sizeof filename, NULL, NULL);
		if (code == 0)
			pdfapp_error(&gapp, "cannot convert filename to utf-8");
	}

	if (fz_optind < argc)
		gapp.pageno = atoi(argv[fz_optind++]);

	if (kbps)
		pdfapp_open_progressive(&gapp, filename, 0, kbps);
	else
		pdfapp_open(&gapp, filename, 0);

	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	fz_free_argv(argc, argv);

	do_close(&gapp);

	return 0;
}
