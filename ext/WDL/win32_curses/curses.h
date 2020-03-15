#ifndef _CURSES_WIN32SIM_H_
#define _CURSES_WIN32SIM_H_

#if !defined(_WIN32) && !defined(MAC_NATIVE) && !defined(FORCE_WIN32_CURSES)
  #ifdef MAC
  #include <ncurses.h>
  #else
  #include <curses.h>
  #endif
#else

  #ifdef _WIN32
  #include <windows.h>
  #else
  #include "../swell/swell.h"
  #endif

#include "../wdltypes.h"


/*
** this implements a tiny subset of curses on win32.
** It creates a window (Resizeable by user), and gives you a callback to run 
** your UI. 
*/


// if you need multiple contexts, define this in your sourcefiles BEFORE including curses.h
// if you don't need multiple contexts, declare win32CursesCtx g_curses_context; in one of your source files.
#ifndef CURSES_INSTANCE
#define CURSES_INSTANCE (&g_curses_context)
#endif

#define LINES ((CURSES_INSTANCE)->lines)
#define COLS ((CURSES_INSTANCE)->cols)

//ncurses WIN32 wrapper functions

#define WDL_IS_FAKE_CURSES

#define addnstr(str,n) __addnstr(CURSES_INSTANCE,str,n)
#define addstr(str) __addnstr(CURSES_INSTANCE,str,-1)
#define addnstr_w(str,n) __addnstr_w(CURSES_INSTANCE,str,n)
#define addstr_w(str) __addnstr_w(CURSES_INSTANCE,str,-1)
#define addch(c) __addch(CURSES_INSTANCE,c)

#define mvaddstr(y,x,str) __mvaddnstr(CURSES_INSTANCE,y,x,str,-1)
#define mvaddnstr(y,x,str,n) __mvaddnstr(CURSES_INSTANCE,y,x,str,n)
#define mvaddstr_w(y,x,str) __mvaddnstr_w(CURSES_INSTANCE,y,x,str,-1)
#define mvaddnstr_w(y,x,str,n) __mvaddnstr_w(CURSES_INSTANCE,y,x,str,n)
#define clrtoeol() __clrtoeol(CURSES_INSTANCE)
#define move(y,x) __move(CURSES_INSTANCE,y,x,0)
#define attrset(a) (CURSES_INSTANCE)->m_cur_attr=(a)
#define bkgdset(a) (CURSES_INSTANCE)->m_cur_erase_attr=(a)
#define initscr() __initscr(CURSES_INSTANCE)
#define endwin() __endwin(CURSES_INSTANCE)
#define curses_erase(x) __curses_erase(x)
#define start_color()
#define init_pair(x,y,z) __init_pair((CURSES_INSTANCE),x,y,z)
#define has_colors() 1

#define A_NORMAL 0
#define A_BOLD 1
#define COLOR_PAIR(x) ((x)<<NUM_ATTRBITS)
#define COLOR_PAIRS 16
#define NUM_ATTRBITS 1

#define WIN32_CURSES_CURSOR_TYPE_VERTBAR 0
#define WIN32_CURSES_CURSOR_TYPE_HORZBAR 1
#define WIN32_CURSES_CURSOR_TYPE_BLOCK 2

typedef struct win32CursesFB {
  wchar_t c;
  unsigned char attr;
} win32CursesFB;

typedef struct win32CursesCtx
{
  HWND m_hwnd;
  int lines, cols;

  int want_scrollbar;
  int scrollbar_topmargin,scrollbar_botmargin;
  int drew_scrollbar[2];
  int offs_y[2];  
  int div_y, tot_y;
  int scroll_y[2], scroll_h[2];

  int m_cursor_x, m_cursor_y;
  int cursor_state_lx,cursor_state_ly; // used to detect changes and reset cursor_state

  win32CursesFB *m_framebuffer;
  HFONT mOurFont;
  int *fontsize_ptr;
  
  int m_font_w, m_font_h;

  int colortab[COLOR_PAIRS << NUM_ATTRBITS][2];

  int m_kb_queue[64];
  unsigned char m_kb_queue_valid;
  unsigned char m_kb_queue_pos;

  char need_redraw; // &2 = need font calculation, &1 = need redraw, &4=full paint pending, no need to keep invalidating
  char cursor_state; // blinky cycle

  char m_cur_attr;
  char m_cur_erase_attr;

  // callbacks/config available for user
  char want_getch_runmsgpump; // set to 1 to cause getch() to run the message pump, 2 to cause it to be blocking (waiting for keychar)
  char cursor_type; // set to WIN32_CURSES_CURSOR_TYPE_VERTBAR etc

  void (*do_update)(win32CursesCtx *ctx); // called on resize/etc, to avoid flicker. NULL will use default behavior

  void *user_data;
  LRESULT (*onMouseMessage)(void *user_data, HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

  int *user_colortab; // cycle the high byte of the first entry to force an update of colortab
  int user_colortab_lastfirstval;
} win32CursesCtx;

extern win32CursesCtx g_curses_context; // declare this if you need it
extern int *curses_win32_global_user_colortab;
void init_user_colortab(win32CursesCtx *ctx); // if you're in a hurry, otherwise blinking cursor detects

void curses_setWindowContext(HWND hwnd, win32CursesCtx *ctx);
void curses_unregisterChildClass(HINSTANCE hInstance);
void curses_registerChildClass(HINSTANCE hInstance);
HWND curses_CreateWindow(HINSTANCE hInstance, win32CursesCtx *ctx, const char *title);


void __addnstr(win32CursesCtx *inst, const char *str,int n);
void __addnstr_w(win32CursesCtx *inst, const wchar_t *str,int n);
void __move(win32CursesCtx *inst, int y, int x, int noupdest);
static inline void __addch(win32CursesCtx *inst, wchar_t c) { __addnstr_w(inst,&c,1); }
static inline void __mvaddnstr(win32CursesCtx *inst, int x, int y, const char *str, int n) { __move(inst,x,y,1); __addnstr(inst,str,n); }
static inline void __mvaddnstr_w(win32CursesCtx *inst, int x, int y, const wchar_t *str, int n) { __move(inst,x,y,1); __addnstr_w(inst,str,n); }


void __clrtoeol(win32CursesCtx *inst);
void __initscr(win32CursesCtx *inst);
void __endwin(win32CursesCtx *inst);
void __curses_erase(win32CursesCtx *inst);
void __curses_invalidatefull(win32CursesCtx *inst, bool finish); // use around a block with a lot of drawing to prevent excessive invalidaterects

int curses_getch(win32CursesCtx *inst);

#if defined(_WIN32) || defined(MAC_NATIVE) || defined(FORCE_WIN32_CURSES)
#define getch() curses_getch(CURSES_INSTANCE)
#define erase() curses_erase(CURSES_INSTANCE)
#endif


#define wrefresh(x)
#define cbreak()
#define noecho()
#define nonl()
#define intrflush(x,y)
#define keypad(x,y)
#define nodelay(x,y)
#define raw()
#define refresh()
#define sync()



#define COLOR_WHITE RGB(192,192,192)
#define COLOR_BLACK RGB(0,0,0)
#define COLOR_BLUE  RGB(0,0,192)
#define COLOR_RED   RGB(192,0,0)
#define COLOR_CYAN  RGB(0,192,192)
#define COLOR_BLUE_DIM  RGB(0,0,56)
#define COLOR_RED_DIM    RGB(56,0,0)
#define COLOR_CYAN_DIM  RGB(0,56,56)

#define ERR -1

enum
{
  KEY_DOWN=4096,
  KEY_UP,
  KEY_PPAGE,
  KEY_NPAGE,
  KEY_RIGHT,
  KEY_LEFT,
  KEY_HOME,
  KEY_END,
  KEY_IC,
  KEY_DC,
  KEY_F1,
  KEY_F2,
  KEY_F3,
  KEY_F4,
  KEY_F5,
  KEY_F6,
  KEY_F7,
  KEY_F8,
  KEY_F9,
  KEY_F10,
  KEY_F11,
  KEY_F12,
};

#define KEY_BACKSPACE '\b'

#define KEY_F(x) (KEY_F1 + (x) - 1)


void __init_pair(win32CursesCtx *ctx, int p, int b, int f);

#endif

#endif
