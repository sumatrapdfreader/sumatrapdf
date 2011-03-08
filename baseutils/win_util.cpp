/* Written by Krzysztof Kowalczyk (http://blog.kowalczyk.info)
   The author disclaims copyright to this source code. */
#include "BaseUtil.h"
#include "win_util.h"
#include "tstr_util.h"

int win_get_text_len(HWND hwnd)
{
    return (int)SendMessage(hwnd, WM_GETTEXTLENGTH, 0, 0);
}

void win_set_text(HWND hwnd, const TCHAR *txt)
{
    SendMessage(hwnd, WM_SETTEXT, (WPARAM)0, (LPARAM)txt);
}

/* return a text in edit control represented by hwnd
   return NULL in case of error (couldn't allocate memory)
   caller needs to free() the text */
TCHAR *win_get_text(HWND hwnd)
{
    int     cchTxtLen = win_get_text_len(hwnd);
    TCHAR * txt = (TCHAR*)calloc((size_t)cchTxtLen + 1, sizeof(TCHAR));

    if (NULL == txt)
        return NULL;

    SendMessage(hwnd, WM_GETTEXT, cchTxtLen + 1, (LPARAM)txt);
    txt[cchTxtLen] = 0;
    return txt;
}

void launch_url(const TCHAR *url)
{
    SHELLEXECUTEINFO sei;
    BOOL             res;

    if (NULL == url)
        return;

    ZeroMemory(&sei, sizeof(sei));
    sei.cbSize  = sizeof(sei);
    sei.fMask   = SEE_MASK_FLAG_NO_UI;
    sei.lpVerb  = TEXT("open");
    sei.lpFile  = url;
    sei.nShow   = SW_SHOWNORMAL;

    res = ShellExecuteEx(&sei);
    return;
}

void exec_with_params(const TCHAR *exe, const TCHAR *params, BOOL hidden)
{
    SHELLEXECUTEINFO sei;
    BOOL             res;

    if (NULL == exe)
        return;

    ZeroMemory(&sei, sizeof(sei));
    sei.cbSize  = sizeof(sei);
    sei.fMask   = SEE_MASK_FLAG_NO_UI;
    sei.lpVerb  = NULL;
    sei.lpFile  = exe;
    sei.lpParameters = params;
    if (hidden)
        sei.nShow = SW_HIDE;
    else
        sei.nShow   = SW_SHOWNORMAL;
    res = ShellExecuteEx(&sei);
}

/* On windows those are defined as:
#define CSIDL_PROGRAMS           0x0002
#define CSIDL_PERSONAL           0x0005
#define CSIDL_APPDATA            0x001a
 see shlobj.h for more */

#ifdef CSIDL_APPDATA
/* this doesn't seem to be defined on sm 2002 */
#define SPECIAL_FOLDER_PATH CSIDL_APPDATA
#endif

#ifdef CSIDL_PERSONAL
/* this is defined on sm 2002 and goes to "\My Documents".
   Not sure if I should use it */
 #ifndef SPECIAL_FOLDER_PATH
  #define SPECIAL_FOLDER_PATH CSIDL_PERSONAL
 #endif
#endif

/* see http://www.opennetcf.org/Forums/post.asp?method=TopicQuote&TOPIC_ID=95&FORUM_ID=12 
   for more possibilities
   return false on failure, true if ok. Even if returns false, it'll return root ("\")
   directory so that clients can ignore failures from this function
*/
TCHAR *get_app_data_folder_path(BOOL f_create)
{
#ifdef SPECIAL_FOLDER_PATH
    BOOL        f_ok;
    TCHAR       path[MAX_PATH];

    f_ok = SHGetSpecialFolderPath(NULL, path, SPECIAL_FOLDER_PATH, f_create);
    if (f_ok)
        return tstr_dup(path);
    else
        return tstr_dup(_T(""));
#else
    /* if all else fails, just use root ("\") directory */
    return tstr_dup(_T(""));
#endif
}

int screen_get_dx(void)
{
    return (int)GetSystemMetrics(SM_CXSCREEN);
}

int screen_get_dy(void)
{
    return (int)GetSystemMetrics(SM_CYSCREEN);
}

int screen_get_menu_dy(void)
{
    return GetSystemMetrics(SM_CYMENU);
}

int screen_get_caption_dy(void)
{
    return GetSystemMetrics(SM_CYCAPTION);
}

/* Ensure that the rectangle is at least partially in the work area on a
   monitor. The rectangle is shifted into the work area if necessary. */
void rect_shift_to_work_area(RECT *rect, BOOL bFully)
{
    MONITORINFO mi = { 0 };
    mi.cbSize = sizeof mi;
    GetMonitorInfo(MonitorFromRect(rect, MONITOR_DEFAULTTONEAREST), &mi);
    
    if (rect->bottom <= mi.rcWork.top || bFully && rect->top < mi.rcWork.top)
        /* Rectangle is too far above work area */
        OffsetRect(rect, 0, mi.rcWork.top - rect->top);
    else if (rect->top >= mi.rcWork.bottom || bFully && rect->bottom > mi.rcWork.bottom)
        /* Rectangle is too far below */
        OffsetRect(rect, 0, mi.rcWork.bottom - rect->bottom);
    
    if (rect->right <= mi.rcWork.left || bFully && rect->left < mi.rcWork.left)
        /* Too far left */
        OffsetRect(rect, mi.rcWork.left - rect->left, 0);
    else if (rect->left >= mi.rcWork.right || bFully && rect->right > mi.rcWork.right)
        /* Right */
        OffsetRect(rect, mi.rcWork.right - rect->right, 0);
}

static void rect_client_to_screen(RECT *r, HWND hwnd)
{
    POINT   p1 = {r->left, r->top};
    POINT   p2 = {r->right, r->bottom};
    ClientToScreen(hwnd, &p1);
    ClientToScreen(hwnd, &p2);
    r->left = p1.x;
    r->top = p1.y;
    r->right = p2.x;
    r->bottom = p2.y;
}

void paint_round_rect_around_hwnd(HDC hdc, HWND hwnd_edit_parent, HWND hwnd_edit, COLORREF col)
{
    RECT    r;
    HBRUSH  br;
    HGDIOBJ br_prev;
    HGDIOBJ pen;
    HGDIOBJ pen_prev;
    GetClientRect(hwnd_edit, &r);
    br = CreateSolidBrush(col);
    if (!br) return;
    pen = CreatePen(PS_SOLID, 1, col);
    pen_prev = SelectObject(hdc, pen);
    br_prev = SelectObject(hdc, br);
    rect_client_to_screen(&r, hwnd_edit_parent);
    /* TODO: the roundness value should probably be calculated from the dy of the rect */
    /* TODO: total hack: I manually adjust rectangle to values that fit g_hwnd_edit, as
       found by experimentation. My mapping of coordinates isn't right (I think I need
       mapping from window to window but even then it wouldn't explain -3 for y axis */
    RoundRect(hdc, r.left+4, r.top-3, r.right+12, r.bottom-3, 8, 8);
    if (br_prev)
        SelectObject(hdc, br_prev);
    if (pen_prev)
        SelectObject(hdc, pen_prev);
    DeleteObject(pen);
    DeleteObject(br);
}

void paint_rect(HDC hdc, RECT * rect)
{
    MoveToEx(hdc, rect->left, rect->top, NULL);
    LineTo(hdc, rect->right - 1, rect->top);
    LineTo(hdc, rect->right - 1, rect->bottom - 1);
    LineTo(hdc, rect->left, rect->bottom - 1);
    LineTo(hdc, rect->left, rect->top);
}

void draw_centered_text(HDC hdc, RECT *r, const TCHAR *txt)
{    
    SetBkMode(hdc, TRANSPARENT);
    DrawText(hdc, txt, lstrlen(txt), r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

BOOL IsCursorOverWindow(HWND hwnd)
{
    POINT pt;
    RECT rcWnd;
    GetCursorPos(&pt);
    GetWindowRect(hwnd, &rcWnd);
    return PtInRect(&rcWnd, pt);
}

void CenterDialog(HWND hDlg)
{
    RECT rcDialog, rcOwner, rcRect;
    HWND hParent = GetParent(hDlg);

    GetWindowRect(hDlg, &rcDialog);
    OffsetRect(&rcDialog, -rcDialog.left, -rcDialog.top);
    GetWindowRect(hParent ? hParent : GetDesktopWindow(), &rcOwner);
    CopyRect(&rcRect, &rcOwner);
    OffsetRect(&rcRect, -rcRect.left, -rcRect.top);

    // center dialog on its parent window
    OffsetRect(&rcDialog, rcOwner.left + (rcRect.right - rcDialog.right) / 2, rcOwner.top + (rcRect.bottom - rcDialog.bottom) / 2);
    // ensure that the dialog is fully visible on one monitor
    rect_shift_to_work_area(&rcDialog, TRUE);

    SetWindowPos(hDlg, 0, rcDialog.left, rcDialog.top, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}
