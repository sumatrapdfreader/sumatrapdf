#ifdef _WIN32
#include <windows.h>
#else
#include "../swell/swell.h"
#endif
#define CURSES_INSTANCE ___ERRROR_____

#include "../wdltypes.h"
#include "../wdlutf8.h"

#include "curses.h"

#include <ctype.h>
#include <stdio.h>

#define CURSOR_BLINK_TIMER_MS 400
#define CURSOR_BLINK_TIMER 2
#define CURSOR_BLINK_TIMER_ZEROEVERY 3

#define WIN32CURSES_CLASS_NAME "WDLCursesWindow"

static void doFontCalc(win32CursesCtx*, HDC);
static void reInitializeContext(win32CursesCtx *ctx);

static void m_InvalidateArea(win32CursesCtx *ctx, int sx, int sy, int ex, int ey)
{
  if (!ctx) return;

  doFontCalc(ctx,NULL);

  if (!ctx->m_hwnd || (ctx->need_redraw&4)) return;

  RECT r;
  r.left=sx*ctx->m_font_w;
  r.top=sy*ctx->m_font_h;
  r.right=ex*ctx->m_font_w;
  r.bottom=ey*ctx->m_font_h;
  InvalidateRect(ctx->m_hwnd,&r,FALSE);
}

void __curses_invalidatefull(win32CursesCtx *inst, bool finish)
{
  if (inst && inst->m_hwnd)
  {
    if (finish)
    {
      if (inst->need_redraw&4)
      {
        inst->need_redraw&=~4;
        InvalidateRect(inst->m_hwnd,NULL,FALSE);
      }
    }
    else
      inst->need_redraw|=4;
  }
}

void __addnstr(win32CursesCtx *ctx, const char *str,int n)
{
  if (!ctx||n==0) return;

  const int sx=ctx->m_cursor_x, sy=ctx->m_cursor_y, cols=ctx->cols;
  if (!ctx->m_framebuffer || sy < 0 || sy >= ctx->lines || sx < 0 || sx >= cols) return;
  win32CursesFB *p=ctx->m_framebuffer + (sx + sy*cols);

  const unsigned char attr = ctx->m_cur_attr;
  while (n && *str)
  {
    int c,sz=wdl_utf8_parsechar(str,&c);
    p->c=(wchar_t)c;
    p->attr=attr;
    p++;
    str+=sz;
    if (n > 0 && (n-=sz)<0) n = 0;

	  if (++ctx->m_cursor_x >= cols) break;
  }
  m_InvalidateArea(ctx,sx,sy,sy < ctx->m_cursor_y ? cols : ctx->m_cursor_x+1,ctx->m_cursor_y+1);
}

void __addnstr_w(win32CursesCtx *ctx, const wchar_t *str,int n)
{
  if (!ctx||n==0) return;

  const int sx=ctx->m_cursor_x, sy=ctx->m_cursor_y, cols=ctx->cols;
  if (!ctx->m_framebuffer || sy < 0 || sy >= ctx->lines || sx < 0 || sx >= cols) return;
  win32CursesFB *p=ctx->m_framebuffer + (sx + sy*cols);

  const unsigned char attr = ctx->m_cur_attr;
  while (n-- && *str)
  {
    p->c=*str++;
    p->attr=attr;
    p++;
	  if (++ctx->m_cursor_x >= cols)  break;
  }
  m_InvalidateArea(ctx,sx,sy,sy < ctx->m_cursor_y ? cols : ctx->m_cursor_x+1,ctx->m_cursor_y+1);
}

void __clrtoeol(win32CursesCtx *ctx)
{
  if (!ctx) return;

  if (ctx->m_cursor_x<0)ctx->m_cursor_x=0;
  int n = ctx->cols - ctx->m_cursor_x;
  if (!ctx->m_framebuffer || ctx->m_cursor_y < 0 || ctx->m_cursor_y >= ctx->lines || n < 1) return;
  win32CursesFB *p=ctx->m_framebuffer + (ctx->m_cursor_x + ctx->m_cursor_y*ctx->cols);
  int sx=ctx->m_cursor_x;
  while (n--)
  {
    p->c=0;
    p->attr=ctx->m_cur_erase_attr;
    p++;
  }
  m_InvalidateArea(ctx,sx,ctx->m_cursor_y,ctx->cols,ctx->m_cursor_y+1);
}

void __curses_erase(win32CursesCtx *ctx)
{
  if (!ctx) return;

  ctx->m_cur_attr=0;
  ctx->m_cur_erase_attr=0;
  if (ctx->m_framebuffer) memset(ctx->m_framebuffer,0,ctx->cols*ctx->lines*sizeof(*ctx->m_framebuffer));
  ctx->m_cursor_x=0;
  ctx->m_cursor_y=0;
  m_InvalidateArea(ctx,0,0,ctx->cols,ctx->lines);
}

void __move(win32CursesCtx *ctx, int y, int x, int noupdest)
{
  if (!ctx) return;

  m_InvalidateArea(ctx,ctx->m_cursor_x,ctx->m_cursor_y,ctx->m_cursor_x+1,ctx->m_cursor_y+1);
  ctx->m_cursor_x=wdl_max(x,0);
  ctx->m_cursor_y=wdl_max(y,0);
  if (!noupdest) m_InvalidateArea(ctx,ctx->m_cursor_x,ctx->m_cursor_y,ctx->m_cursor_x+1,ctx->m_cursor_y+1);
}


void __init_pair(win32CursesCtx *ctx, int pair, int fcolor, int bcolor)
{
  if (!ctx || pair < 0 || pair >= COLOR_PAIRS) return;

  pair=COLOR_PAIR(pair);
  fcolor &= RGB(255,255,255);
  bcolor &= RGB(255,255,255);

  ctx->colortab[pair][0]=fcolor;
  ctx->colortab[pair][1]=bcolor;

  if (fcolor & 0xff) fcolor|=0xff;
  if (fcolor & 0xff00) fcolor|=0xff00;
  if (fcolor & 0xff0000) fcolor|=0xff0000;
  ctx->colortab[pair|A_BOLD][0]=fcolor;
  ctx->colortab[pair|A_BOLD][1]=bcolor;
}

int *curses_win32_global_user_colortab;

static LRESULT xlateKey(int msg, WPARAM wParam, LPARAM lParam)
{
  if (msg == WM_KEYDOWN)
  {
#ifndef _WIN32
    if (lParam & FVIRTKEY)
#endif
    switch (wParam)
	  {
	    case VK_HOME: return KEY_HOME;
	    case VK_UP: return KEY_UP;
	    case VK_PRIOR: return KEY_PPAGE;
	    case VK_LEFT: return KEY_LEFT;
	    case VK_RIGHT: return KEY_RIGHT;
	    case VK_END: return KEY_END;
	    case VK_DOWN: return KEY_DOWN;
	    case VK_NEXT: return KEY_NPAGE;
	    case VK_INSERT: return KEY_IC;
	    case VK_DELETE: return KEY_DC;
	    case VK_F1: return KEY_F1;
	    case VK_F2: return KEY_F2;
	    case VK_F3: return KEY_F3;
	    case VK_F4: return KEY_F4;
	    case VK_F5: return KEY_F5;
	    case VK_F6: return KEY_F6;
	    case VK_F7: return KEY_F7;
	    case VK_F8: return KEY_F8;
	    case VK_F9: return KEY_F9;
	    case VK_F10: return KEY_F10;
	    case VK_F11: return KEY_F11;
	    case VK_F12: return KEY_F12;
#ifndef _WIN32
            case VK_SUBTRACT: return '-'; // numpad -
            case VK_ADD: return '+';
            case VK_MULTIPLY: return '*';
            case VK_DIVIDE: return '/';
            case VK_DECIMAL: return '.';
            case VK_NUMPAD0: return '0';
            case VK_NUMPAD1: return '1';
            case VK_NUMPAD2: return '2';
            case VK_NUMPAD3: return '3';
            case VK_NUMPAD4: return '4';
            case VK_NUMPAD5: return '5';
            case VK_NUMPAD6: return '6';
            case VK_NUMPAD7: return '7';
            case VK_NUMPAD8: return '8';
            case VK_NUMPAD9: return '9';
            case (32768|VK_RETURN): return VK_RETURN;
#endif
    }
    
    switch (wParam)
    {
      case VK_RETURN: case VK_BACK: case VK_TAB: case VK_ESCAPE: return wParam;
      case VK_CONTROL: break;
    
      default:
        if(GetAsyncKeyState(VK_CONTROL)&0x8000)
        {
          if (wParam>='a' && wParam<='z') 
          {
            wParam += 1-'a';
            return wParam;
          }
          if (wParam>='A' && wParam<='Z') 
          {
            wParam += 1-'A';
            return wParam;
          }
          if ((wParam&~0x80) == '[') return 27;
          if ((wParam&~0x80) == ']') return 29;
        }
    }
  }
    
#ifdef _WIN32 // todo : fix for nonwin32
  if (msg == WM_CHAR)
  {
    if(wParam>=32) return wParam;
  }  
#else
  //osx/linux
  if (wParam >= 32)
  {
    if (!(GetAsyncKeyState(VK_SHIFT)&0x8000))
    {
      if (wParam>='A' && wParam<='Z') 
      {
        if ((GetAsyncKeyState(VK_LWIN)&0x8000)) wParam -= 'A'-1;
        else
          wParam += 'a'-'A';
      }
    }
    return wParam;
  }
      
#endif
  return ERR;
}


static void m_reinit_framebuffer(win32CursesCtx *ctx)
{
  if (!ctx) return;

    doFontCalc(ctx,NULL);
    RECT r;

    GetClientRect(ctx->m_hwnd,&r);
    
    ctx->lines=r.bottom / ctx->m_font_h;
    ctx->cols=r.right / ctx->m_font_w;
    if (ctx->lines<1) ctx->lines=1;
    if (ctx->cols<1) ctx->cols=1;
    ctx->m_cursor_x=0;
    ctx->m_cursor_y=0;
    free(ctx->m_framebuffer);
    ctx->m_framebuffer=(win32CursesFB *)malloc(sizeof(win32CursesFB)*ctx->lines*ctx->cols);
    if (ctx->m_framebuffer) memset(ctx->m_framebuffer, 0,sizeof(win32CursesFB)*ctx->lines*ctx->cols);

    const int *tab = ctx->user_colortab ? ctx->user_colortab : curses_win32_global_user_colortab;
    if (tab)
    {
      ctx->user_colortab_lastfirstval=tab[0];
      for (int x=0;x<COLOR_PAIRS;x++) __init_pair(ctx,x,tab[x*2],tab[x*2+1]);
    }
}
#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL 0x20A
#endif

LRESULT CALLBACK cursesWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) 
{
  win32CursesCtx *ctx = (win32CursesCtx*)GetWindowLongPtr(hwnd,GWLP_USERDATA);

#ifdef _WIN32

  static int Scroll_Message;
  if (!Scroll_Message)
  {
    Scroll_Message = (int)RegisterWindowMessage("MSWHEEL_ROLLMSG");
    if (!Scroll_Message) Scroll_Message=-1;
  }
  if (Scroll_Message > 0 && uMsg == (UINT)Scroll_Message)
  {
    uMsg=WM_MOUSEWHEEL;
    wParam<<=16; 
  }
#endif

  if (ctx) switch (uMsg)
  {
	case WM_DESTROY:
		ctx->m_hwnd=0;
	return 0;
  case WM_CHAR: case WM_KEYDOWN: 

#ifdef __APPLE__
        {
          int f=0;
          wParam = SWELL_MacKeyToWindowsKeyEx(NULL,&f,1);
          lParam=f;
        }
#endif

    {
      const int a=(int)xlateKey(uMsg,wParam,lParam);
      if (a != ERR)
      {
        const int qsize = sizeof(ctx->m_kb_queue)/sizeof(ctx->m_kb_queue[0]);
        if (ctx->m_kb_queue_valid>=qsize) // queue full, dump an old event!
        {
          ctx->m_kb_queue_valid--;
          ctx->m_kb_queue_pos++;
        }

        ctx->m_kb_queue[(ctx->m_kb_queue_pos + ctx->m_kb_queue_valid++) & (qsize-1)] = a;
      }
    }
  case WM_KEYUP:
  return 0;
	case WM_GETMINMAXINFO:
	      {
	        LPMINMAXINFO p=(LPMINMAXINFO)lParam;
	        p->ptMinTrackSize.x = 160;
	        p->ptMinTrackSize.y = 120;
	      }
	return 0;
	case WM_SIZE:
		if (wParam != SIZE_MINIMIZED)
		{
			m_reinit_framebuffer(ctx);
      if (ctx->do_update) ctx->do_update(ctx);
			else ctx->need_redraw|=1;
		}
	return 0;
  case WM_RBUTTONDOWN:
  case WM_LBUTTONDOWN:
    SetFocus(hwnd);
  case WM_LBUTTONUP:
  case WM_RBUTTONUP:
  case WM_CAPTURECHANGED:
  case WM_MOUSEMOVE:
  case WM_MOUSEWHEEL:
  case WM_LBUTTONDBLCLK:
  case WM_RBUTTONDBLCLK:
  case WM_MBUTTONDBLCLK:
    if (ctx && ctx->fontsize_ptr && uMsg == WM_MOUSEWHEEL && (GetAsyncKeyState(VK_CONTROL)&0x8000))
    {
      int a = (int)(short)HIWORD(wParam);
      if (a<0 && *ctx->fontsize_ptr > 4) (*ctx->fontsize_ptr)--;
      else if (a>=0 && *ctx->fontsize_ptr < 64) (*ctx->fontsize_ptr)++;
      else return 1;

      if (ctx->mOurFont) 
      {
        DeleteObject(ctx->mOurFont);
        ctx->mOurFont=NULL;
      }
      reInitializeContext(ctx);
      m_reinit_framebuffer(ctx);
      if (ctx->do_update) ctx->do_update(ctx);
      else ctx->need_redraw|=1;

      return 1;
    }
    if (ctx && ctx->onMouseMessage) return ctx->onMouseMessage(ctx->user_data,hwnd,uMsg,wParam,lParam);
  return 0;

  case WM_SETCURSOR:
    if (ctx->m_font_w && ctx->m_font_h)
    {
      POINT p;
      GetCursorPos(&p);
      ScreenToClient(hwnd, &p);
      p.x /= ctx->m_font_w;
      p.y /= ctx->m_font_h;

      const int topmarg=ctx->scrollbar_topmargin;
      const int bottmarg=ctx->scrollbar_botmargin;
      int paney[2] = { topmarg, ctx->div_y+topmarg+1 };
      int paneh[2] = { ctx->div_y, ctx->lines-ctx->div_y-topmarg-bottmarg-1 };
      bool has_panes=(ctx->div_y < ctx->lines-topmarg-bottmarg-1);
      int scrollw[2] = { ctx->cols-ctx->drew_scrollbar[0], ctx->cols-ctx->drew_scrollbar[1] };

      if (has_panes && p.y >= ctx->div_y+1 && p.y < ctx->div_y+2) SetCursor(LoadCursor(NULL, IDC_SIZENS));
      else if (p.y < 1 || p.y >= ctx->lines-1) SetCursor(LoadCursor(NULL, IDC_ARROW));
      else if (p.y >= paney[0] && p.y < paney[0]+paneh[0] && p.x >= scrollw[0]) SetCursor(LoadCursor(NULL, IDC_ARROW));
      else if (p.y >= paney[1] && p.y < paney[1]+paneh[1] && p.x >= scrollw[1]) SetCursor(LoadCursor(NULL, IDC_ARROW));
      else SetCursor(LoadCursor(NULL, IDC_IBEAM));   
    return TRUE;     
  }

#ifdef _WIN32
  case WM_GETDLGCODE:
    if (GetParent(hwnd))
    {
      return DLGC_WANTALLKEYS;
    }
  return 0;
#endif
  case WM_TIMER:
    if (wParam==CURSOR_BLINK_TIMER && ctx)
    {
      const char la = ctx->cursor_state;
      ctx->cursor_state = (ctx->cursor_state+1)%CURSOR_BLINK_TIMER_ZEROEVERY;
      if (!ctx->cursor_state && GetFocus() != hwnd) ctx->cursor_state=1;

      const int *tab = ctx->user_colortab ? ctx->user_colortab : curses_win32_global_user_colortab;
      if (tab && tab[0] != ctx->user_colortab_lastfirstval)
      {
	m_reinit_framebuffer(ctx);
        if (ctx->do_update) ctx->do_update(ctx);
	else ctx->need_redraw|=1;
      }
      else if (!!ctx->cursor_state != !!la)
      {
        __move(ctx,ctx->m_cursor_y,ctx->m_cursor_x,1);// refresh cursor
      }
    }
  return 0;
    case WM_CREATE:

      // this only is called on osx or from standalone, it seems, since on win32 ctx isnt set up yet
      ctx->m_hwnd=hwnd;
      #ifndef _WIN32
	m_reinit_framebuffer(ctx);
	ctx->need_redraw|=1;
      #endif
      SetTimer(hwnd,CURSOR_BLINK_TIMER,CURSOR_BLINK_TIMER_MS,NULL);
    return 0;
    case WM_ERASEBKGND:
    return 1;
    case WM_PAINT:
      {
        {
          PAINTSTRUCT ps;
          HDC hdc=BeginPaint(hwnd,&ps);
          if (hdc)
          {
            const int topmarg=ctx->scrollbar_topmargin;
            const int bottmarg=ctx->scrollbar_botmargin;
            int paney[2] = { topmarg, ctx->div_y+topmarg+1 };
            int paneh[2] = { ctx->div_y, ctx->lines-ctx->div_y-topmarg-bottmarg-1 };
            bool has_panes=(ctx->div_y < ctx->lines-topmarg-bottmarg-1);
            if (!has_panes) paneh[0]++;

            ctx->drew_scrollbar[0]=ctx->drew_scrollbar[1]=0;
            if (ctx->want_scrollbar > 0)
            {
              RECT cr;
              GetClientRect(hwnd, &cr);
              double cf=(double)cr.right/(double)ctx->m_font_w-(double)ctx->cols;
              int ws=ctx->want_scrollbar;
              if (cf < 0.5) ++ws;

              int i;
              for (i=0; i < 2; ++i)
              {
                ctx->scroll_y[i]=ctx->scroll_h[i]=0;
                if (paneh[i] > 0 && ctx->tot_y > paneh[i])
                {
                  ctx->drew_scrollbar[i]=ws;
                  int ey=paneh[i]*ctx->m_font_h;
                  ctx->scroll_h[i]=ey*paneh[i]/ctx->tot_y;
                  if (ctx->scroll_h[i] < ctx->m_font_h) ctx->scroll_h[i]=ctx->m_font_h;
                  ctx->scroll_y[i]=(ey-ctx->scroll_h[i])*ctx->offs_y[i]/(ctx->tot_y-paneh[i]);
                  if (ctx->scroll_y[i] < 0) ctx->scroll_y[i]=0;
                  if (ctx->scroll_y[i] > ey-ctx->scroll_h[i]) ctx->scroll_y[i]=ey-ctx->scroll_h[i];
                }
              }
            }

            RECT r = ps.rcPaint;
            doFontCalc(ctx,ps.hdc);
            
            HGDIOBJ oldf=SelectObject(hdc,ctx->mOurFont);
            int y,ypos;
			      int lattr=-1;
#ifdef _WIN32
            SetTextAlign(hdc,TA_TOP|TA_LEFT);
#endif
            const win32CursesFB *ptr=(const win32CursesFB*)ctx->m_framebuffer;
            RECT updr=r;

			      r.left /= ctx->m_font_w;
			      r.top /= ctx->m_font_h;
			      r.bottom += ctx->m_font_h-1;
			      r.bottom /= ctx->m_font_h;
			      r.right += ctx->m_font_w-1;
			      r.right /= ctx->m_font_w;
            
			      if (r.top < 0) r.top=0;
			      if (r.bottom > ctx->lines) r.bottom=ctx->lines;
			      if (r.left < 0) r.left=0;
                              if (r.right > ctx->cols) r.right=ctx->cols;

			      ypos = r.top * ctx->m_font_h;
			      ptr += (r.top * ctx->cols);


            HBRUSH bgbrushes[COLOR_PAIRS << NUM_ATTRBITS];
            for(y=0;y<sizeof(bgbrushes)/sizeof(bgbrushes[0]);y++) bgbrushes[y] = CreateSolidBrush(ctx->colortab[y][1]);

            char cstate=ctx->cursor_state;
            if (ctx->m_cursor_y != ctx->cursor_state_ly || ctx->m_cursor_x != ctx->cursor_state_lx)
            {
              ctx->cursor_state_lx=ctx->m_cursor_x;
              ctx->cursor_state_ly=ctx->m_cursor_y;
              ctx->cursor_state=0;
              cstate=1;
            }
                    
            if (ctx->m_framebuffer) for (y = r.top; y < r.bottom; y ++, ypos+=ctx->m_font_h, ptr += ctx->cols)
            {
              int x = r.left,xpos = r.left * ctx->m_font_w;

				      const win32CursesFB *p = ptr + r.left;

              int defer_blanks=0;

              int right=r.right;              
              if (y >= paney[0] && y < paney[0]+paneh[0])
              {
                right=wdl_min(right, ctx->cols-ctx->drew_scrollbar[0]);
              }
              else if (y >= paney[1] && y < paney[1]+paneh[1]) 
              {
                right=wdl_min(right,  ctx->cols-ctx->drew_scrollbar[1]);
              }
              
              for (;; x ++, xpos+=ctx->m_font_w, p ++)
              {
                wchar_t c=' ';
                int attr=0; 
                
                if (x < right)
                {
                  c=p->c;
                  attr=p->attr;
                }

                const bool isCursor = cstate && y == ctx->m_cursor_y && x == ctx->m_cursor_x;
                const bool isNotBlank = c>=128 || (isprint(c) && !isspace(c));

                if (defer_blanks > 0 && (isNotBlank || isCursor || attr != lattr || x>=right))
                {
                  RECT tr={xpos - defer_blanks*ctx->m_font_w,ypos,xpos,ypos+ctx->m_font_h};
                  FillRect(hdc,&tr,bgbrushes[lattr&((COLOR_PAIRS << NUM_ATTRBITS)-1)]);
                  defer_blanks=0;
                }

                if (x>=right) break;

						    if (isCursor && ctx->cursor_type == WIN32_CURSES_CURSOR_TYPE_BLOCK)
						    {
						      SetTextColor(hdc,ctx->colortab[attr&((COLOR_PAIRS << NUM_ATTRBITS)-1)][1]);
						      SetBkColor(hdc,ctx->colortab[attr&((COLOR_PAIRS << NUM_ATTRBITS)-1)][0]);
                  lattr = -1;
						    }
				        else 
                if (attr != lattr)
				        {
						      SetTextColor(hdc,ctx->colortab[attr&((COLOR_PAIRS << NUM_ATTRBITS)-1)][0]);
						      SetBkColor(hdc,ctx->colortab[attr&((COLOR_PAIRS << NUM_ATTRBITS)-1)][1]);
					        lattr=attr;
				        }

                if (isNotBlank||isCursor)
                {
                  #ifdef _WIN32
                    int txpos = xpos;
                    TextOutW(hdc,txpos,ypos,isNotBlank ? &c : L" ",1);
                  #else
                    const int max_charw = ctx->m_font_w, max_charh = ctx->m_font_h;
                    RECT tr={xpos,ypos,xpos+max_charw, ypos+max_charh};
                    HBRUSH br=bgbrushes[attr&((COLOR_PAIRS << NUM_ATTRBITS)-1)];
                    if (isCursor && ctx->cursor_type == WIN32_CURSES_CURSOR_TYPE_BLOCK)
                    {
                      br = CreateSolidBrush(ctx->colortab[attr&((COLOR_PAIRS << NUM_ATTRBITS)-1)][0]);
                      FillRect(hdc,&tr,br);
                      DeleteObject(br);
                    }
                    else
                    {
                      FillRect(hdc,&tr,br);
                    }
                    char tmp[16];
                    if (c >= 128)
                    {
                      WDL_MakeUTFChar(tmp,c,sizeof(tmp));
                    }
                    else
                    {
                      tmp[0]=isNotBlank ? (char)c : ' ';
                      tmp[1]=0;
                    }
                    DrawText(hdc,tmp,-1,&tr,DT_LEFT|DT_TOP|DT_NOPREFIX|DT_NOCLIP);
                  #endif

                  if (isCursor && ctx->cursor_type != WIN32_CURSES_CURSOR_TYPE_BLOCK)
                  {
                    RECT r={xpos,ypos,xpos+2,ypos+ctx->m_font_h};
                    if (ctx->cursor_type == WIN32_CURSES_CURSOR_TYPE_HORZBAR)
                    {
                      RECT tr={xpos,ypos+ctx->m_font_h-2,xpos+ctx->m_font_w,ypos+ctx->m_font_h};
                      r=tr;
                    }
                    HBRUSH br=CreateSolidBrush(ctx->colortab[attr&((COLOR_PAIRS << NUM_ATTRBITS)-1)][0]);
                    FillRect(hdc,&r,br);
                    DeleteObject(br);
                  }
                }
                else 
                {
                  defer_blanks++;
                }
              }
            }

            int ex=ctx->cols*ctx->m_font_w;
            int ey=ctx->lines*ctx->m_font_h;

            int anyscrollw=wdl_max(ctx->drew_scrollbar[0], ctx->drew_scrollbar[1]);
            if (anyscrollw && updr.right >= ex-anyscrollw*ctx->m_font_w)
            {              
              HBRUSH sb1=CreateSolidBrush(RGB(128,128,128));
              HBRUSH sb2=CreateSolidBrush(RGB(96, 96, 96));
              int i;
              for (i=0; i < 2; ++i)
              {
                if (ctx->drew_scrollbar[i])
                {
                  int scrolly=paney[i]*ctx->m_font_h+ctx->scroll_y[i];
                  int scrollh=ctx->scroll_h[i];
                  RECT tr = { ex-ctx->drew_scrollbar[i]*ctx->m_font_w, paney[i]*ctx->m_font_h, updr.right, wdl_min(scrolly, updr.bottom) };
                  if (tr.bottom > tr.top) FillRect(hdc, &tr, sb1);
                  tr.top=wdl_max(updr.top, scrolly);
                  tr.bottom=wdl_min(updr.bottom, scrolly+scrollh);
                  if (tr.bottom > tr.top) FillRect(hdc, &tr, sb2);
                  tr.top=wdl_max(updr.top,scrolly+scrollh);
                  tr.bottom=(paney[i]+paneh[i])*ctx->m_font_h;
                  if (tr.bottom > tr.top) FillRect(hdc, &tr, sb1);
                }
              }
              DeleteObject(sb1);
              DeleteObject(sb2);
            }
      
            ex -= ctx->m_font_w;
            if (updr.right >= ex)
            {
              // draw the scrollbars if they haven't been already drawn
              if (!ctx->drew_scrollbar[0] && updr.bottom > paney[0]*ctx->m_font_h && updr.top < (paney[0]+paneh[0])*ctx->m_font_h)
              {
                RECT tr = { ex, paney[0]*ctx->m_font_h, updr.right, (paney[0]+paneh[0])*ctx->m_font_h };
                FillRect(hdc, &tr, bgbrushes[0]);
              }
              if (!ctx->drew_scrollbar[1] && updr.bottom > paney[1]*ctx->m_font_h && updr.top < (paney[1]+paneh[1])*ctx->m_font_h)
              {
                RECT tr = { ex, paney[1]*ctx->m_font_h, updr.right, (paney[1]+paneh[1])*ctx->m_font_h };
                FillRect(hdc, &tr, bgbrushes[0]);
              }

              // draw line endings of special areas

              const int div1a = has_panes ? (paney[0]+paneh[0]) : 0;
              const int div1b = has_panes ? paney[1] : 0;

              int y;
              const int bm1 = ctx->lines-bottmarg;
              const int fonth = ctx->m_font_h;
              for (y = r.top; y < r.bottom; y ++)
              {
                if (y < topmarg || y>=bm1 || (y<div1b && y >= div1a))
                {
                  const int attr = ctx->m_framebuffer ? ctx->m_framebuffer[(y+1) * ctx->cols - 1].attr : 0; // last attribute of line

                  const int yp = y * fonth;
                  RECT tr = { wdl_max(ex,updr.left), wdl_max(yp,updr.top), updr.right, wdl_min(yp+fonth,updr.bottom) };
                  FillRect(hdc, &tr, bgbrushes[attr&((COLOR_PAIRS << NUM_ATTRBITS)-1)]);
                }
              }
            }

            if (updr.bottom > ey)
            {
              RECT tr= { updr.left, wdl_max(ey,updr.top), updr.right, updr.bottom };
              FillRect(hdc, &tr, bgbrushes[2]);
            }

            for(y=0;y<sizeof(bgbrushes)/sizeof(bgbrushes[0]);y++) DeleteObject(bgbrushes[y]);
            SelectObject(hdc,oldf);

            EndPaint(hwnd,&ps);
          }
        }
      }
    return 0;
  }
  return DefWindowProc(hwnd,uMsg,wParam,lParam);
}

static void doFontCalc(win32CursesCtx *ctx, HDC hdcIn)
{
  if (!ctx || !ctx->m_hwnd || !(ctx->need_redraw&2)) return;

  HDC hdc = hdcIn;
  if (!hdc) hdc = GetDC(ctx->m_hwnd);

  if (!hdc) return;
   
  ctx->need_redraw&=~2;

  HGDIOBJ oldf=SelectObject(hdc,ctx->mOurFont);
  TEXTMETRIC tm;
  GetTextMetrics(hdc,&tm);
  ctx->m_font_h=tm.tmHeight;
  ctx->m_font_w=tm.tmAveCharWidth;
  SelectObject(hdc,oldf);
  
  if (hdc != hdcIn) ReleaseDC(ctx->m_hwnd,hdc);

}

void reInitializeContext(win32CursesCtx *ctx)
{
  if (!ctx) return;

  if (!ctx->mOurFont) ctx->mOurFont = CreateFont(
      ctx->fontsize_ptr ? *ctx->fontsize_ptr :
#ifdef _WIN32
                                                 16,
#else
                                                14,
#endif
                        0, // width
                        0, // escapement
                        0, // orientation
#ifndef __APPLE__
                        FW_NORMAL, // normal
#else
                        FW_BOLD,
#endif
                        FALSE, //italic
                        FALSE, //undelrine
                        FALSE, //strikeout
                        ANSI_CHARSET,
                        OUT_DEFAULT_PRECIS,
                        CLIP_DEFAULT_PRECIS,
                        ANTIALIASED_QUALITY, //NONANTIALIASED_QUALITY,//DEFAULT_QUALITY,
#ifdef _WIN32
                        FF_MODERN,
#else
                                                 0,
#endif
                        "Courier New");

  ctx->need_redraw|=2;
  ctx->m_font_w=8;
  ctx->m_font_h=8;
  doFontCalc(ctx,NULL);
}




void __initscr(win32CursesCtx *ctx)
{
#ifdef WDL_IS_FAKE_CURSES
  if (!curses_win32_global_user_colortab && (!ctx || !ctx->user_colortab))
#endif
  {
    __init_pair(ctx,0,RGB(192,192,192),RGB(0,0,0));
  }
}

void __endwin(win32CursesCtx *ctx)
{
  if (ctx)
  {
    if (ctx->m_hwnd)
      curses_setWindowContext(ctx->m_hwnd,0);
    ctx->m_kb_queue_valid=0;
    ctx->m_hwnd=0;
    free(ctx->m_framebuffer);
    ctx->m_framebuffer=0;
    if (ctx->mOurFont) DeleteObject(ctx->mOurFont);
    ctx->mOurFont=0;
  }
}


int curses_getch(win32CursesCtx *ctx)
{
  if (!ctx || !ctx->m_hwnd) return ERR;

#ifdef _WIN32
  if (ctx->want_getch_runmsgpump>0)
  {
    MSG msg;
    if (ctx->want_getch_runmsgpump>1)
    {
      while(!ctx->m_kb_queue_valid && GetMessage(&msg,NULL,0,0))
      {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
    }
    else while(PeekMessage(&msg,NULL,0,0,PM_REMOVE))
    {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }
#endif

  if (ctx->m_kb_queue_valid)
  {
    const int qsize = sizeof(ctx->m_kb_queue)/sizeof(ctx->m_kb_queue[0]);
    const int a = ctx->m_kb_queue[ctx->m_kb_queue_pos & (qsize-1)];
    ctx->m_kb_queue_pos++;
    ctx->m_kb_queue_valid--;
    return a;
  }
  
  if (ctx->need_redraw&1)
  {
    ctx->need_redraw&=~1;
    InvalidateRect(ctx->m_hwnd,NULL,FALSE);
    return 'L'-'A'+1;
  }

  return ERR;
}

void curses_setWindowContext(HWND hwnd, win32CursesCtx *ctx)
{
  SetWindowLongPtr(hwnd,GWLP_USERDATA,(INT_PTR)ctx);
  if (ctx)
  {
    ctx->m_hwnd=hwnd;
    ctx->m_kb_queue_valid=0;

    free(ctx->m_framebuffer);
    ctx->m_framebuffer=0;

    SetTimer(hwnd,CURSOR_BLINK_TIMER,CURSOR_BLINK_TIMER_MS,NULL);
    reInitializeContext(ctx);
    m_reinit_framebuffer(ctx);
    InvalidateRect(hwnd,NULL,FALSE);
  }
}

#ifdef _WIN32
static int m_regcnt;
#endif

void curses_unregisterChildClass(HINSTANCE hInstance)
{
#ifdef _WIN32
  if (!--m_regcnt)
    UnregisterClass(WIN32CURSES_CLASS_NAME,hInstance);
#endif
}

void curses_registerChildClass(HINSTANCE hInstance)
{
#ifdef _WIN32
  if (!m_regcnt++)
  {
	  WNDCLASS wc={CS_DBLCLKS,};	
	  wc.lpfnWndProc = cursesWindowProc;
    wc.hInstance = hInstance;	
	  wc.hCursor = LoadCursor(NULL,IDC_ARROW);
	  wc.lpszClassName = WIN32CURSES_CLASS_NAME;

    RegisterClass(&wc);
  }
#endif
}

#ifndef _WIN32
HWND curses_ControlCreator(HWND parent, const char *cname, int idx, const char *classname, int style, int x, int y, int w, int h)
{
  HWND hw=0;
  if (!strcmp(classname,WIN32CURSES_CLASS_NAME))
  {
    hw=CreateDialog(NULL,0,parent,(DLGPROC)cursesWindowProc);
  }
  
  if (hw)
  {
    SWELL_SetClassName(hw,WIN32CURSES_CLASS_NAME);
    SetWindowLong(hw,GWL_ID,idx);
    SetWindowPos(hw,HWND_TOP,x,y,w,h,SWP_NOZORDER|SWP_NOACTIVATE);
    ShowWindow(hw,SW_SHOWNA);
    return hw;
  }
  
  return 0;
}

#endif

HWND curses_CreateWindow(HINSTANCE hInstance, win32CursesCtx *ctx, const char *title)
{
  if (!ctx) return NULL;
#ifdef _WIN32
 ctx->m_hwnd = CreateWindowEx(0,WIN32CURSES_CLASS_NAME, title,WS_CAPTION|WS_MAXIMIZEBOX|WS_MINIMIZEBOX|WS_SIZEBOX|WS_SYSMENU,
					CW_USEDEFAULT,CW_USEDEFAULT,640,480,
					NULL, NULL,hInstance,NULL);
#else
  ctx->m_hwnd = CreateDialog(NULL,0,NULL,(DLGPROC)cursesWindowProc);
  
#endif
 if (ctx->m_hwnd) 
 {
   curses_setWindowContext(ctx->m_hwnd,ctx);
   ShowWindow(ctx->m_hwnd,SW_SHOW);
 }
 return ctx->m_hwnd;
}
