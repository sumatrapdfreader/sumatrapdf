/* Written by Krzysztof Kowalczyk (http://blog.kowalczyk.info)
   The author disclaims copyright to this source code. */
#ifndef WIN_UTIL_H_
#define WIN_UTIL_H_
#include <commctrl.h>
#include <windowsx.h>

/* Utilities to help in common windows programming tasks */

/* constant to make it easier to return proper LRESULT values when handling
   various windows messages */
#define WM_KILLFOCUS_HANDLED 0
#define WM_SETFOCUS_HANDLED 0
#define WM_KEYDOWN_HANDLED 0
#define WM_KEYUP_HANDLED 0
#define WM_LBUTTONDOWN_HANDLED 0
#define WM_LBUTTONUP_HANDLED 0
#define WM_PAINT_HANDLED 0
#define WM_DRAWITEM_HANDLED TRUE
#define WM_MEASUREITEM_HANDLED TRUE
#define WM_SIZE_HANDLED 0
#define LVN_ITEMACTIVATE_HANDLED 0
#define WM_VKEYTOITEM_HANDLED_FULLY -2
#define WM_VKEYTOITEM_NOT_HANDLED -1
#define WM_NCPAINT_HANDLED 0
#define WM_VSCROLL_HANDLED 0
#define WM_HSCROLL_HANDLED 0
#define WM_CREATE_FAILED -1
#define WM_CREATE_OK 0

#define WIN_COL_RED     RGB(255,0,0)
#define WIN_COL_WHITE   RGB(255,255,255)
#define WIN_COL_BLACK   RGB(0,0,0)
#define WIN_COL_BLUE    RGB(0,0,255)
#define WIN_COL_GREEN   RGB(0,255,0)
#define WIN_COL_GRAY    RGB(215,215,215)

#define DRAGQUERY_NUMFILES 0xFFFFFFFF

int     win_get_text_len(HWND hwnd);
TCHAR * win_get_text(HWND hwnd);
void    win_set_text(HWND hwnd, const TCHAR *txt);

#define Edit_SelectAll(hwnd) Edit_SetSel(hwnd, 0, -1)
#define ListBox_AppendString_NoSort(hwnd, txt) ListBox_InsertString(hwnd, -1, txt)
#define Window_SetFont(hwnd, font) SetWindowFont(hwnd, font, TRUE)

int     screen_get_dx(void);
int     screen_get_dy(void);
int     screen_get_menu_dy(void);
int     screen_get_caption_dy(void);
void    rect_shift_to_work_area(RECT *rect, BOOL bFully);

void    launch_url(const TCHAR *url);
void    exec_with_params(const TCHAR *exe, const TCHAR *params, BOOL hidden);

TCHAR * get_app_data_folder_path(BOOL f_create);

void    paint_round_rect_around_hwnd(HDC hdc, HWND hwnd_edit_parent, HWND hwnd_edit, COLORREF col);
void    paint_rect(HDC hdc, RECT * rect);
void    draw_centered_text(HDC hdc, RECT *r, const TCHAR *txt);

BOOL    IsCursorOverWindow(HWND hwnd);
void    CenterDialog(HWND hDlg);

#endif
