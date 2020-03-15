#include "curses.h"

#ifdef _WIN32
win32CursesCtx g_curses_context; // we only need the one instance
#endif

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
  g_curses_context.want_getch_runmsgpump = 1;  // non-block

  curses_registerChildClass(hInstance);
  curses_CreateWindow(hInstance,&g_curses_context,"Sample Test App");
#else
int main() {
#endif


  initscr();
  cbreak();
  noecho();
  nonl();
  intrflush(stdscr,FALSE);
  keypad(stdscr,TRUE);
  nodelay(stdscr,TRUE);
  raw();
#if !defined(_WIN32) && !defined(MAC_NATIVE)
	ESCDELAY=0; // dont wait--at least on the console this seems to work.
#endif

  if (has_colors()) // we don't use color yet, but we could
  {
	start_color();
	init_pair(1, COLOR_WHITE, COLOR_BLUE); // normal status lines
	init_pair(2, COLOR_BLACK, COLOR_CYAN); // value
  }

  erase();
  refresh();

  float xpos=0,ypos=0, xdir=0.7, ydir=1.5;
  for (;;)
  {
    int t=getch();
    if (t==27) break;
    else if (t== KEY_LEFT) xdir *=0.9;
    else if (t== KEY_RIGHT) xdir *=1.1;
    else if (t== KEY_UP) ydir *=1.1;
    else if (t== KEY_DOWN) ydir *=0.9;

    xpos+=xdir; ypos+=ydir;
    if (xpos >= COLS-1||xpos<1) { if (xpos<1)xpos=1; else xpos=COLS-1;  xdir=-xdir; }
    if (ypos >= LINES-1||ypos<1) { if (ypos<1)ypos=1; else ypos=LINES-1;  ydir=-ydir; }

    erase();
    mvaddstr(ypos,xpos,"X");


    Sleep(10);
#ifdef _WIN32
	if (!g_curses_context.m_hwnd) break;
#endif
  }
  

  erase();
  refresh();
  endwin();

#ifdef _WIN32
  if (g_curses_context.m_hwnd) DestroyWindow(g_curses_context.m_hwnd);
  curses_unregisterChildClass(hInstance);
#endif

  return 0;


}



