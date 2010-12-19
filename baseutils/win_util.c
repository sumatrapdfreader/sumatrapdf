/* Written by Krzysztof Kowalczyk (http://blog.kowalczyk.info)
   The author disclaims copyright to this source code. */
#include "base_util.h"
#include "win_util.h"
#include "tstr_util.h"

// Hmm, why have to redefine here (?!)
#ifdef __GNUC__
#define LVM_GETSELECTIONMARK     (LVM_FIRST+66)
#define ListView_GetSelectionMark(w) (INT)SNDMSG((w),LVM_GETSELECTIONMARK,0,0)
#endif

int rect_dx(RECT *r)
{
    int dx = r->right - r->left;
    assert(dx >= 0);
    return dx;
}

int rect_dy(RECT *r)
{
    int dy = r->bottom - r->top;
    assert(dy >= 0);
    return dy;
}

void rect_set(RECT *r, int x, int y, int dx, int dy)
{
    r->left = x;
    r->top = y;
    r->right = x + dx;
    r->bottom = y + dy;
}

void win_set_font(HWND hwnd, HFONT font)
{
    SendMessage(hwnd, WM_SETFONT, (WPARAM)font, 0);
}

int win_get_text_len(HWND hwnd)
{
    return (int)SendMessage(hwnd, WM_GETTEXTLENGTH, 0, 0);
}

void win_set_texta(HWND hwnd, const char *txt)
{
    SendMessageA(hwnd, WM_SETTEXT, (WPARAM)0, (LPARAM)txt);
}

void win_set_textw(HWND hwnd, const WCHAR *txt)
{
    SendMessageW(hwnd, WM_SETTEXT, (WPARAM)0, (LPARAM)txt);
}

/* return a text in edit control represented by hwnd
   return NULL in case of error (couldn't allocate memory)
   caller needs to free() the text */
TCHAR *win_get_text(HWND hwnd)
{
    int     cchTxtLen = win_get_text_len(hwnd);
    TCHAR * txt = (TCHAR*)malloc((cchTxtLen+1)*sizeof(TCHAR));

    if (NULL == txt)
        return NULL;

    SendMessage(hwnd, WM_GETTEXT, cchTxtLen + 1, (LPARAM)txt);
    txt[cchTxtLen] = 0;
    return txt;
}

WCHAR *win_get_textw(HWND hwnd)
{
    int     cchTxtLen = win_get_text_len(hwnd);
    WCHAR * txt = (WCHAR*)malloc((cchTxtLen+1)*sizeof(WCHAR));

    if (NULL == txt)
        return NULL;

    SendMessageW(hwnd, WM_GETTEXT, cchTxtLen + 1, (LPARAM)txt);
    txt[cchTxtLen] = 0;
    return txt;
}

void win_edit_set_selection(HWND hwnd, DWORD selStart, DWORD selEnd)
{
   SendMessage(hwnd, EM_SETSEL, (WPARAM)selStart, (WPARAM)selEnd);
}

void win_edit_select_all(HWND hwnd)
{
    win_edit_set_selection(hwnd, 0, -1);
}

LRESULT lv_delete_all_items(HWND hwnd)
{
    return SendMessage(hwnd, LVM_DELETEALLITEMS, 0, 0);
}

void lv_set_items_count(HWND hwnd, int items_count)
{
    ListView_SetItemCount(hwnd, items_count);
}

int lv_get_items_count(HWND hwnd)
{
    LRESULT count = ListView_GetItemCount(hwnd);
    if (LB_ERR == count)
        return 0;
    return (int)count;
}

LRESULT lv_insert_column(HWND hwnd, int col, LVCOLUMN *lvc)
{
    return SendMessage(hwnd, LVM_INSERTCOLUMN, col, (LPARAM)lvc);
}

LRESULT lv_set_column(HWND hwnd, int col, LVCOLUMN *lvc)
{
    return SendMessage(hwnd, LVM_SETCOLUMN, col, (LPARAM)lvc);
}

LRESULT lv_set_column_dx(HWND hwnd, int col, int dx)
{
    return ListView_SetColumnWidth(hwnd, col, dx);
}

LRESULT lv_insert_item(HWND hwnd, int row, LVITEM *lvi)
{
    lvi->iItem = row;
    lvi->iSubItem = 0;
    return SendMessage(hwnd, LVM_INSERTITEM, 0, (LPARAM)lvi);
}

LRESULT lb_delete_string(HWND hwnd, int pos)
{
    return SendMessage(hwnd, LB_DELETESTRING, pos, 0);
}

LRESULT lb_delete_all_items(HWND hwnd)
{
#if 1
    LRESULT    remaining_count;
    for (;;) {
        remaining_count = lb_delete_string(hwnd, 0);
        if ((LB_ERR == remaining_count) || (0 == remaining_count))
            break;
    }
    return 0;
#else
    LRESULT count;
    int     i;

    count = lb_get_items_count(hwnd);
    if (LB_ERR == count)
        return LB_ERR;

    for (i=count-1; i--; i>=0) {
        lb_delete_string(hwnd, i);
    }
    assert(0 == lb_get_items_count(hwnd);
#endif
}

#if 0
LRESULT lb_set_items_count(HWND hwnd, int items_count)
{
    return SendMessage(hwnd, LB_SETCOUNT, items_count, 0);
}
#endif

LRESULT lb_get_items_count(HWND hwnd)
{
    return SendMessage(hwnd, LB_GETCOUNT, 0, 0);
}

LRESULT lb_insert_item_text(HWND hwnd, int row, const TCHAR *txt)
{
    return SendMessage(hwnd, LB_INSERTSTRING, (WPARAM)row, (LPARAM)txt);
}

LRESULT lb_insert_item_textw(HWND hwnd, int row, const WCHAR *txt)
{
    return SendMessageW(hwnd, LB_INSERTSTRING, (WPARAM)row, (LPARAM)txt);
}

LRESULT lb_append_string_no_sort(HWND hwnd, const TCHAR *txt)
{
    return lb_insert_item_text(hwnd, -1, txt);
}

LRESULT lb_append_stringw_no_sort(HWND hwnd, const WCHAR *txt)
{
    return lb_insert_item_textw(hwnd, -1, txt);
}

/* lb_get_selection and lb_set_selection only work for single-selection listbox */
LRESULT lb_get_selection(HWND hwnd)
{
    return SendMessage(hwnd, LB_GETCURSEL, 0, 0);
}

LRESULT lb_set_selection(HWND hwnd, int item)
{
    assert(item >= 0);
    return SendMessage(hwnd, LB_SETCURSEL, (WPARAM)item, 0);
}

LRESULT lv_insert_item_text(HWND hwnd, int row, const TCHAR *txt)
{
    LVITEM  lvi = {0};

    assert(txt);
    if (!txt)
        return -1; /* means failure */
    lvi.mask = LVIF_TEXT;
    lvi.pszText = (LPTSTR)txt;
    return lv_insert_item(hwnd, row, &lvi);
}

/* Returns a selected item or -1 if no selection.
   Assumes that the list is single-sel */
int lv_get_selection_pos(HWND hwnd)
{
    int selection;
    int selected_count = ListView_GetSelectedCount(hwnd);
    assert(selected_count <= 1);
    if (0 == selected_count)
        return -1;
    selection = ListView_GetSelectionMark(hwnd);
    return selection;
}

int font_get_dy_from_dc(HDC hdc, HFONT font)
{
    TEXTMETRIC  tm;
    HFONT       font_prev;
    int         font_dy;

    font_prev = (HFONT)SelectObject(hdc, font);
    GetTextMetrics(hdc, &tm);
    font_dy = tm.tmAscent + tm.tmDescent;
    SelectObject(hdc, font_prev);
    return font_dy;
}

int font_get_dy(HWND hwnd, HFONT font)
{
    HDC         hdc;
    int         font_dy = 0;

    hdc = GetDC(hwnd);
    if (hdc)
        font_dy = font_get_dy_from_dc(hdc, font);
    ReleaseDC(hwnd, hdc);
    return font_dy;
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

void screen_get_dx_dy(int *dx_out, int *dy_out)
{
    if (dx_out)
        *dx_out = GetSystemMetrics(SM_CXSCREEN);
    if (dy_out)
        *dy_out = GetSystemMetrics(SM_CYSCREEN);
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

/* given a string id 'strId' from resources, get the string in a dynamically
   allocated string.
   Returns the string or NULL if error.
   Caller needs to free() the string.
   TODO: string is limited to BUF_CCH_SIZE. Could do it better by dynamically
   allocating more memory if needed */
#define BUF_CCH_SIZE 256
TCHAR *load_string_dup(int str_id)
{
    TCHAR buf[BUF_CCH_SIZE] = {0};
    LoadString(NULL, str_id, buf, BUF_CCH_SIZE);
    if (0 == tstr_len(buf))
    {
        assert(0);
        return NULL;
    }
    return tstr_dup(buf);
}

const TCHAR *load_string(int str_id)
{
    int          res;
    const TCHAR *str;
 
    /* little-known hack: when lpBuffer is NULL, LoadString() returns
       a pointer to a string, that can be cast to TCHAR * (LPCTSTR)
       requires -n option to RC (resource compiler)
       http://msdn.microsoft.com/library/default.asp?url=/library/en-us/wcesdk40/html/cerefLoadString.asp */
    res = LoadString(NULL, str_id, NULL, 0);
    if (0 == res)
        return NULL;
    str = (const TCHAR*)res;
    return str;
}

// A helper to set a string, null-terminated 'keyValue' for a given 'keyName'
// in 'keyPath'/'keyClass'
// if 'keyName' is NULL then we set the default value for 'keyPath'
// Returns false if there was any error (can be ignored)
int regkey_set_str(HKEY key_class, TCHAR *key_path, TCHAR *key_name, TCHAR *key_value)
{
    HKEY    hkey    = NULL;
    DWORD   size  = 0;
    BOOL    f_ok;

    if (ERROR_SUCCESS != RegCreateKeyEx(key_class, key_path, 0, NULL, 0, 0, NULL, &hkey, NULL))
        return FALSE;

    f_ok = TRUE;
    size = (DWORD)(tstr_len(key_value)*sizeof(TCHAR));
    if (ERROR_SUCCESS != RegSetValueEx(hkey, key_name, 0, REG_SZ, (LPBYTE)key_value, size))
        f_ok = FALSE;
    RegCloseKey(hkey);

    return f_ok;
}

int regkey_set_dword(HKEY key_class, TCHAR *key_path, TCHAR *key_name, DWORD key_value)
{
    HKEY    hkey    = NULL;
    DWORD   size  = 0;
    BOOL    f_ok;

    if (ERROR_SUCCESS != RegCreateKeyEx(key_class, key_path, 0, NULL, 0, 0, NULL, &hkey, NULL))
        return FALSE;

    f_ok = TRUE;
    size = sizeof(DWORD);
    if (ERROR_SUCCESS != RegSetValueEx(hkey, key_name, 0, REG_DWORD, (LPBYTE)&key_value, size))
        f_ok = FALSE;
    RegCloseKey(hkey);

    return f_ok;
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
    HBRUSH  br_prev;
    HPEN    pen;
    HPEN    pen_prev;
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

