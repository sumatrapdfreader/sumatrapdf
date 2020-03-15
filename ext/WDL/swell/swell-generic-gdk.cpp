/* Cockos SWELL (Simple/Small Win32 Emulation Layer for Linux/OSX)
   Copyright (C) 2006 and later, Cockos, Inc.

    This software is provided 'as-is', without any express or implied
    warranty.  In no event will the authors be held liable for any damages
    arising from the use of this software.

    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not
       claim that you wrote the original software. If you use this software
       in a product, an acknowledgment in the product documentation would be
       appreciated but is not required.
    2. Altered source versions must be plainly marked as such, and must not be
       misrepresented as being the original software.
    3. This notice may not be removed or altered from any source distribution.
  

  */


#ifndef SWELL_PROVIDED_BY_APP

#include "swell.h"

//#define SWELL_GDK_IMPROVE_WINDOWRECT // does not work yet (gdk_window_get_frame_extents() does not seem to be sufficiently reliable)

#ifdef SWELL_PRELOAD
#define STR(x) #x
#define STR2(x) STR(x)
extern "C" {
  char __attribute__ ((visibility ("default"))) SWELL_WANT_LOAD_LIBRARY[] = STR2(SWELL_PRELOAD);
};
#undef STR
#undef STR2
#endif

#ifdef SWELL_TARGET_GDK

#include "swell-internal.h"
#include "swell-dlggen.h"
#include "../wdlcstring.h"
#include "../wdlutf8.h"


#if !defined(SWELL_TARGET_GDK_NO_CURSOR_HACK)
  #define SWELL_TARGET_GDK_CURSORHACK
  #include <X11/extensions/XInput2.h>
#endif

#include <X11/Xatom.h>

static void (*_gdk_drag_drop_done)(GdkDragContext *, gboolean); // may not always be available

static guint32 _gdk_x11_window_get_desktop(GdkWindow *window)
{
  Atom type;
  gint format;
  gulong nitems=0, bytes_after; 
  guchar *data;

  if (!window || !gdk_x11_screen_supports_net_wm_hint(gdk_window_get_screen(window), 
                                           gdk_atom_intern_static_string("_NET_WM_DESKTOP"))) 
    return 0;

  XGetWindowProperty(GDK_WINDOW_XDISPLAY(window), GDK_WINDOW_XID(window), 
      gdk_x11_get_xatom_by_name_for_display(gdk_window_get_display(window), "_NET_WM_DESKTOP"),
                        0, G_MAXLONG, false, XA_CARDINAL, &type, &format, &nitems, &bytes_after, &data);
  if (type != XA_CARDINAL || nitems<1) return 0;
  nitems = *(gulong *)data;
  XFree(data);
  return (guint32) nitems;
}

static void _gdk_x11_window_move_to_desktop(GdkWindow *window, guint32 desktop)
{
  XClientMessageEvent xclient;

  if (!window || !gdk_x11_screen_supports_net_wm_hint(gdk_window_get_screen(window), 
                                           gdk_atom_intern_static_string("_NET_WM_DESKTOP"))) 
    return;

  memset (&xclient, 0, sizeof (xclient));
  xclient.type = ClientMessage;
  xclient.send_event = true;
  xclient.window = GDK_WINDOW_XID(window);
  xclient.message_type = gdk_x11_get_xatom_by_name_for_display(gdk_window_get_display(window), "_NET_WM_DESKTOP");
  xclient.format = 32;
  xclient.data.l[0] = desktop;
  xclient.data.l[1] = 1;

  XSendEvent(GDK_WINDOW_XDISPLAY(window), gdk_x11_get_default_root_xwindow(), false,
            SubstructureRedirectMask | SubstructureNotifyMask, (XEvent *)&xclient);
}

// for m_oswindow_private
#define PRIVATE_NEEDSHOW 1 

#ifndef SWELL_WINDOWSKEY_GDK_MASK
#define  SWELL_WINDOWSKEY_GDK_MASK GDK_MOD4_MASK
#endif

static int SWELL_gdk_active;
static GdkEvent *s_cur_evt;
static GList *s_program_icon_list;

static SWELL_OSWINDOW swell_dragsrc_osw;
static DWORD swell_dragsrc_timeout;
static HWND swell_dragsrc_hwnd;
static DWORD swell_lastMessagePos;
static int gdk_options;
#define OPTION_KEEP_OWNED_ABOVE 1
#define OPTION_OWNED_TASKLIST 2
#define OPTION_BORDERLESS_OVERRIDEREDIRECT 4
#define OPTION_BORDERLESS_DIALOG 8

static HWND s_ddrop_hwnd;
static POINT s_ddrop_pt;

static SWELL_CursorResourceIndex *SWELL_curmodule_cursorresource_head;

static int s_cursor_vis_cnt;

static HCURSOR s_last_cursor;
static HCURSOR s_last_setcursor;
static SWELL_OSWINDOW s_last_setcursor_oswnd;

static void *g_swell_touchptr; // last GDK touch sequence
static void *g_swell_touchptr_wnd; // last window of touch sequence, for forcing end of sequence on destroy

static bool g_swell_mouse_relmode;
static int g_swell_mouse_relmode_curpos_x;
static int g_swell_mouse_relmode_curpos_y;

static HANDLE s_clipboard_getstate, s_clipboard_setstate;
static GdkAtom s_clipboard_getstate_fmt, s_clipboard_setstate_fmt;

static WDL_IntKeyedArray<HANDLE> m_clip_recs(GlobalFree);
static WDL_PtrList<char> m_clip_curfmts;
static HWND s_clip_hwnd;

static void swell_gdkEventHandler(GdkEvent *event, gpointer data);

static int s_last_desktop;
static UINT_PTR s_deactivate_timer;
static guint32 s_force_window_time;

static void on_activate(guint32 ftime)
{
  s_force_window_time = ftime;
  swell_app_is_inactive=false;
  HWND h = SWELL_topwindows; 
  while (h)
  {
    if (h->m_oswindow)
    {
      if (h->m_israised)
        gdk_window_set_keep_above(h->m_oswindow,TRUE);

      if (!h->m_enabled) 
        gdk_window_set_accept_focus(h->m_oswindow,FALSE);
    }

    PostMessage(h,WM_ACTIVATEAPP,1,0);
    h=h->m_next;
  }
  s_last_desktop=0;
  s_force_window_time = 0;
}

void swell_gdk_reactivate_app(void)
{
  if (swell_app_is_inactive)
  {
    SWELL_focused_oswindow=NULL;
    on_activate(GDK_CURRENT_TIME);
  }
}

static void on_deactivate()
{
  swell_app_is_inactive=true;
  HWND lf = swell_oswindow_to_hwnd(SWELL_focused_oswindow);
  s_last_desktop = lf && lf->m_oswindow ? _gdk_x11_window_get_desktop(lf->m_oswindow)+1 : 0;

  HWND h = SWELL_topwindows; 
  while (h)
  {
    if (h->m_oswindow)
    {
      if (h->m_israised)
        gdk_window_set_keep_above(h->m_oswindow,FALSE);
      if (!h->m_enabled) 
        gdk_window_set_accept_focus(h->m_oswindow,TRUE); // allow the user to activate app by clicking
    }
    PostMessage(h,WM_ACTIVATEAPP,0,0);
    h=h->m_next;
  }
  DestroyPopupMenus();
}

void swell_oswindow_destroy(HWND hwnd)
{
  if (hwnd && hwnd->m_oswindow)
  {
    if (SWELL_focused_oswindow == hwnd->m_oswindow) SWELL_focused_oswindow = NULL;
    if (g_swell_touchptr && g_swell_touchptr_wnd == hwnd->m_oswindow)
      g_swell_touchptr = NULL;
    gdk_window_destroy(hwnd->m_oswindow);
    hwnd->m_oswindow=NULL;
#ifdef SWELL_LICE_GDI
    delete hwnd->m_backingstore;
    hwnd->m_backingstore=0;
#endif

    if (swell_app_is_inactive)
    {
      HWND h = SWELL_topwindows;
      while (h)
      {
        if (h->m_oswindow) break;
        h = h->m_next;
      }
      if (!h) on_activate(10); // arbitrary old timestamp that is nonzero
    }
  }
}
void swell_oswindow_update_text(HWND hwnd)
{
  if (hwnd && hwnd->m_oswindow)
  {
    gdk_window_set_title(hwnd->m_oswindow, (char*)hwnd->m_title.Get());
  }
}

void swell_oswindow_focus(HWND hwnd)
{
  if (!hwnd)
  {
    SWELL_focused_oswindow = NULL;
    return;
  }

  while (hwnd && !hwnd->m_oswindow) hwnd=hwnd->m_parent;
  if (hwnd && !swell_app_is_inactive)
  {
    gdk_window_raise(hwnd->m_oswindow);
    if (hwnd->m_oswindow != SWELL_focused_oswindow)
    {
      SWELL_focused_oswindow = hwnd->m_oswindow;
      gdk_window_focus(hwnd->m_oswindow,GDK_CURRENT_TIME);
    }
  }
}

void swell_recalcMinMaxInfo(HWND hwnd)
{
  if (!hwnd || !hwnd->m_oswindow || !(hwnd->m_style & WS_CAPTION)) return;

  MINMAXINFO mmi;
  memset(&mmi,0,sizeof(mmi));
  if (hwnd->m_style & WS_THICKFRAME)
  {
    mmi.ptMinTrackSize.x = 20;
    mmi.ptMaxSize.x = mmi.ptMaxTrackSize.x = 16384;
    mmi.ptMinTrackSize.y = 20;
    mmi.ptMaxSize.y = mmi.ptMaxTrackSize.y = 16384;
    SendMessage(hwnd,WM_GETMINMAXINFO,0,(LPARAM)&mmi);
  }
  else
  {
    RECT r=hwnd->m_position;
    mmi.ptMinTrackSize.x = mmi.ptMaxSize.x = mmi.ptMaxTrackSize.x = r.right-r.left;
    mmi.ptMinTrackSize.y = mmi.ptMaxSize.y = mmi.ptMaxTrackSize.y = r.bottom-r.top;
  }

  GdkGeometry h;
  memset(&h,0,sizeof(h));
  h.max_width= mmi.ptMaxSize.x;
  h.max_height= mmi.ptMaxSize.y;
  h.min_width= mmi.ptMinTrackSize.x;
  h.min_height= mmi.ptMinTrackSize.y;
  gdk_window_set_geometry_hints(hwnd->m_oswindow,&h,(GdkWindowHints) (GDK_HINT_POS | GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE));
}

void SWELL_initargs(int *argc, char ***argv) 
{
  if (!SWELL_gdk_active) 
  {
    XInitThreads();
#if SWELL_TARGET_GDK == 3
    void (*_gdk_set_allowed_backends)(const char *);

    *(void **)&_gdk_drag_drop_done = dlsym(RTLD_DEFAULT,"gdk_drag_drop_done");
    *(void **)&_gdk_set_allowed_backends = dlsym(RTLD_DEFAULT,"gdk_set_allowed_backends");

    if (_gdk_set_allowed_backends)
      _gdk_set_allowed_backends("x11");
#endif

#ifdef SWELL_SUPPORT_GTK
    SWELL_gdk_active = gtk_init_check(argc,argv) ? 1 : -1;
#else
    SWELL_gdk_active = gdk_init_check(argc,argv) ? 1 : -1;
#endif
    if (SWELL_gdk_active > 0)
    {
      char buf[1024];
      GetModuleFileName(NULL,buf,sizeof(buf));
      WDL_remove_filepart(buf);
      lstrcatn(buf,"/Resources/main.png",sizeof(buf));
      GdkPixbuf *pb = gdk_pixbuf_new_from_file(buf,NULL);
      if (!pb)
      {
        strcpy(buf+strlen(buf)-3,"ico");
        pb = gdk_pixbuf_new_from_file(buf,NULL);
      }
      if (pb) s_program_icon_list = g_list_append(s_program_icon_list,pb);

      gdk_event_handler_set(swell_gdkEventHandler,NULL,NULL);
    }
  }
}

static bool swell_initwindowsys()
{
  if (!SWELL_gdk_active) 
  {
   // maybe make the main app call this with real parms
    int argc=1;
    char buf[32];
    strcpy(buf,"blah");
    char *argv[2];
    argv[0] = buf;
    argv[1] = buf;
    char **pargv = argv;
    SWELL_initargs(&argc,&pargv);
  }
  
  return SWELL_gdk_active>0;
}

#ifdef SWELL_LICE_GDI
class LICE_CairoBitmap : public LICE_IBitmap
{
  public:
    LICE_CairoBitmap() 
    {
      m_fb = NULL; 
      m_allocsize = m_width = m_height = m_span = 0;
      m_surf = NULL;
    }
    virtual ~LICE_CairoBitmap() 
    { 
      if (m_surf) cairo_surface_destroy(m_surf);
      free(m_fb);
    }

    // LICE_IBitmap interface
    virtual LICE_pixel *getBits() 
    { 
      const UINT_PTR extra=LICE_MEMBITMAP_ALIGNAMT;
      return (LICE_pixel *) (((UINT_PTR)m_fb + extra)&~extra);
    }

    virtual int getWidth() { return m_width; }
    virtual int getHeight() { return m_height; }
    virtual int getRowSpan() { return m_span; }
    virtual bool resize(int w, int h)
    {
      if (w<0) w=0; 
      if (h<0) h=0;
      if (w == m_width && h == m_height) return false;

      if (m_surf) cairo_surface_destroy(m_surf);
      m_surf = NULL;

      m_span = w ? cairo_format_stride_for_width(CAIRO_FORMAT_RGB24,w)/4 : 0;
      const int sz = h * m_span * 4 + LICE_MEMBITMAP_ALIGNAMT;
      if (!m_fb || m_allocsize < sz || sz < m_allocsize/4)
      {
        const int newalloc = m_allocsize<sz ? (sz*3)/2 : sz;
        void *p = realloc(m_fb,newalloc);
        if (!p) return false;

        m_fb = (LICE_pixel *)p;
        m_allocsize = newalloc;
      }

      m_width=w && h ? w :0;
      m_height=w && h ? h : 0;
      return true;
    }
    virtual INT_PTR Extended(int id, void* data) 
    { 
      if (id == 0xca140) 
      {
        if (data) 
        {
          // in case we want to release surface
          return 0;
        }

        if (!m_surf) 
          m_surf = cairo_image_surface_create_for_data((guchar*)getBits(), CAIRO_FORMAT_RGB24, 
                                                       getWidth(),getHeight(), getRowSpan()*4);
        return (INT_PTR)m_surf;
      }
      return 0; 
    }

private:
  LICE_pixel *m_fb;
  int m_width, m_height, m_span;
  int m_allocsize;
  cairo_surface_t *m_surf;
};
#endif

static int swell_gdk_option(const char *name, const char *defstr, int defv)
{
  char buf[64];
  GetPrivateProfileString(".swell",name,"",buf,sizeof(buf),"");
  if (!buf[0]) WritePrivateProfileString(".swell",name,defstr,"");
  if (buf[0] >= '0' && buf[0] <= '9') return atoi(buf);
  return defv;
}

static void init_options()
{
  if (!gdk_options)
  {
    //const char *wmname = gdk_x11_screen_get_window_manager_name(gdk_screen_get_default ());

    gdk_options = 0x40000000;

    if (swell_gdk_option("gdk_owned_windows_keep_above", "auto (default is 1)",1))
      gdk_options|=OPTION_KEEP_OWNED_ABOVE;

    if (swell_gdk_option("gdk_owned_windows_in_tasklist", "auto (default is 0)",0))
      gdk_options|=OPTION_OWNED_TASKLIST;

    switch (swell_gdk_option("gdk_borderless_window_mode", "auto (default is 1=dialog hint. 2=override redirect. 0=normal hint)", 1))
    {
      case 1: gdk_options|=OPTION_BORDERLESS_DIALOG; break;
      case 2: gdk_options|=OPTION_BORDERLESS_OVERRIDEREDIRECT; break;
      default: break;
    }
  }
  
}

bool IsModalDialogBox(HWND);

void swell_oswindow_manage(HWND hwnd, bool wantfocus)
{
  if (!hwnd) return;

  bool isVis = hwnd->m_oswindow != NULL;
  bool wantVis = !hwnd->m_parent && hwnd->m_visible;

  if (isVis != wantVis)
  {
    if (!wantVis) 
    {
      RECT r;
      GetWindowRect(hwnd,&r);
      swell_oswindow_destroy(hwnd);
      hwnd->m_position = r;
    }
    else 
    {
      if (swell_initwindowsys())
      {
        init_options();

        SWELL_OSWINDOW transient_for=NULL;
        if (hwnd->m_owner && (gdk_options&OPTION_KEEP_OWNED_ABOVE))
        {
          HWND own = hwnd->m_owner;
          while (own->m_parent && !own->m_oswindow) own=own->m_parent;

          if (!own->m_oswindow)
          { 
            if (!IsModalDialogBox(hwnd)) return; // defer

            // if a modal window, parent to any owner up the chain
            while (own->m_owner && !own->m_oswindow)
            {
              own = own->m_owner;
              while (own->m_parent && !own->m_oswindow) own=own->m_parent;
            }
          }
          transient_for = own->m_oswindow;
        }

        RECT r = hwnd->m_position;
        GdkWindowAttr attr={0,};
        attr.title = (char *)hwnd->m_title.Get();
        attr.event_mask = GDK_ALL_EVENTS_MASK|GDK_EXPOSURE_MASK;
        attr.x = r.left;
        attr.y = r.top;
        attr.width = r.right-r.left;
        attr.height = r.bottom-r.top;
        attr.wclass = GDK_INPUT_OUTPUT;
        const char *appname = g_swell_appname;
        attr.wmclass_name = (gchar*)appname;
        attr.wmclass_class = (gchar*)appname;
        attr.window_type = GDK_WINDOW_TOPLEVEL;
        hwnd->m_oswindow = gdk_window_new(NULL,&attr,GDK_WA_X|GDK_WA_Y|(appname?GDK_WA_WMCLASS:0));
 
        if (hwnd->m_oswindow) 
        {
          bool override_redirect=false;
          const bool modal = DialogBoxIsActive() == hwnd;

          if (!(hwnd->m_style & WS_CAPTION)) 
          {
            if (hwnd->m_style != WS_CHILD && !(gdk_options&OPTION_BORDERLESS_OVERRIDEREDIRECT))
            {
              if (transient_for)
                gdk_window_set_transient_for(hwnd->m_oswindow,transient_for);
              gdk_window_set_type_hint(hwnd->m_oswindow, (gdk_options&OPTION_BORDERLESS_DIALOG) ? GDK_WINDOW_TYPE_HINT_DIALOG : GDK_WINDOW_TYPE_HINT_NORMAL);
              gdk_window_set_decorations(hwnd->m_oswindow,(GdkWMDecoration) 0);
            }
            else
            {
              gdk_window_set_override_redirect(hwnd->m_oswindow,true);
              override_redirect=true;
            }
            if (!SWELL_topwindows || 
                (SWELL_topwindows==hwnd && !hwnd->m_next)) wantfocus=true;
          }
          else 
          {
            GdkWindowTypeHint type_hint = GDK_WINDOW_TYPE_HINT_NORMAL;
            GdkWMDecoration decor = (GdkWMDecoration) (GDK_DECOR_ALL | GDK_DECOR_MENU);

            if (!(hwnd->m_style&WS_THICKFRAME))
              decor = (GdkWMDecoration) (GDK_DECOR_BORDER|GDK_DECOR_TITLE|GDK_DECOR_MINIMIZE);

            if (transient_for)
            {
              gdk_window_set_transient_for(hwnd->m_oswindow,transient_for);
              if (modal)
                gdk_window_set_modal_hint(hwnd->m_oswindow,true);
            }

            if (modal) type_hint = GDK_WINDOW_TYPE_HINT_DIALOG;

            gdk_window_set_type_hint(hwnd->m_oswindow,type_hint);
            gdk_window_set_decorations(hwnd->m_oswindow,decor);
          }

          if (s_force_window_time)
            gdk_x11_window_set_user_time(hwnd->m_oswindow,s_force_window_time);

          if (!wantfocus || swell_app_is_inactive)
            gdk_window_set_focus_on_map(hwnd->m_oswindow,false);

#ifdef SWELL_LICE_GDI
          if (!hwnd->m_backingstore) hwnd->m_backingstore = new LICE_CairoBitmap;
#endif
          if (!override_redirect)
          {
            if (s_program_icon_list) 
              gdk_window_set_icon_list(hwnd->m_oswindow,s_program_icon_list);
          }
          if (hwnd->m_owner && !(gdk_options&OPTION_OWNED_TASKLIST) && !override_redirect)
          {
            gdk_window_set_skip_taskbar_hint(hwnd->m_oswindow,true);
          }
          else if (hwnd->m_style == WS_CHILD) 
          {
            // hack: parentless visible window with WS_CHILD set will 
            // not appear in taskbar
            gdk_window_set_skip_taskbar_hint(hwnd->m_oswindow,true);
          }

          if (hwnd->m_israised && !swell_app_is_inactive)
            gdk_window_set_keep_above(hwnd->m_oswindow,TRUE);

          gdk_window_register_dnd(hwnd->m_oswindow);

          if (hwnd->m_oswindow_fullscreen)
            gdk_window_fullscreen(hwnd->m_oswindow);

          if (!swell_app_is_inactive && !s_force_window_time)
            gdk_window_show(hwnd->m_oswindow);
          else
            gdk_window_show_unraised(hwnd->m_oswindow);

          if (s_last_desktop>0)
            _gdk_x11_window_move_to_desktop(hwnd->m_oswindow,s_last_desktop-1);

          if (!hwnd->m_oswindow_fullscreen)
          {
            swell_oswindow_resize(hwnd->m_oswindow,hwnd->m_has_had_position?3:2,r);
          }

          if ((gdk_options&OPTION_KEEP_OWNED_ABOVE) && hwnd->m_owned_list)
          {
            HWND l = SWELL_topwindows;
            while (l)  
            {
              if (!l->m_oswindow && l->m_owner == hwnd && l->m_visible)
                swell_oswindow_manage(l,false);
              l = l->m_next;
            }
          }
        }
      }
    }
  }
  if (wantVis) swell_oswindow_update_text(hwnd);
}

void swell_oswindow_updatetoscreen(HWND hwnd, RECT *rect)
{
#ifdef SWELL_LICE_GDI
  if (hwnd && hwnd->m_backingstore && hwnd->m_oswindow)
  {
    LICE_IBitmap *bm = hwnd->m_backingstore;
    LICE_SubBitmap tmpbm(bm,rect->left,rect->top,rect->right-rect->left,rect->bottom-rect->top);

    GdkRectangle rrr={rect->left,rect->top,rect->right-rect->left,rect->bottom-rect->top};
    gdk_window_begin_paint_rect(hwnd->m_oswindow, &rrr);

    cairo_t * crc = gdk_cairo_create (hwnd->m_oswindow);
    cairo_surface_t *temp_surface = (cairo_surface_t*)bm->Extended(0xca140,NULL);
    if (temp_surface) cairo_set_source_surface(crc, temp_surface, 0,0);
    cairo_paint(crc);
    cairo_destroy(crc);

    gdk_window_end_paint(hwnd->m_oswindow);

    if (temp_surface) bm->Extended(0xca140,temp_surface); // release

  }
#endif
}

#if SWELL_TARGET_GDK == 2
  #define DEF_GKY(x) GDK_##x
#else
  #define DEF_GKY(x) GDK_KEY_##x
#endif

static guint swell_gdkConvertKey(guint key)
{
  //gdk key to VK_ conversion
  switch(key)
  {
  case DEF_GKY(KP_Home):
  case DEF_GKY(Home): return VK_HOME;
  case DEF_GKY(KP_End):
  case DEF_GKY(End): return VK_END;
  case DEF_GKY(KP_Up):
  case DEF_GKY(Up): return VK_UP;
  case DEF_GKY(KP_Down):
  case DEF_GKY(Down): return VK_DOWN;
  case DEF_GKY(KP_Left):
  case DEF_GKY(Left): return VK_LEFT;
  case DEF_GKY(KP_Right):
  case DEF_GKY(Right): return VK_RIGHT;
  case DEF_GKY(KP_Page_Up):
  case DEF_GKY(Page_Up): return VK_PRIOR;
  case DEF_GKY(KP_Page_Down):
  case DEF_GKY(Page_Down): return VK_NEXT;
  case DEF_GKY(KP_Insert):
  case DEF_GKY(Insert): return VK_INSERT;
  case DEF_GKY(KP_Delete):
  case DEF_GKY(Delete): return VK_DELETE;
  case DEF_GKY(Escape): return VK_ESCAPE;
  case DEF_GKY(BackSpace): return VK_BACK;
  case DEF_GKY(KP_Enter):
  case DEF_GKY(Return): return VK_RETURN;
  case DEF_GKY(ISO_Left_Tab):
  case DEF_GKY(Tab): return VK_TAB;
  case DEF_GKY(F1): return VK_F1;
  case DEF_GKY(F2): return VK_F2;
  case DEF_GKY(F3): return VK_F3;
  case DEF_GKY(F4): return VK_F4;
  case DEF_GKY(F5): return VK_F5;
  case DEF_GKY(F6): return VK_F6;
  case DEF_GKY(F7): return VK_F7;
  case DEF_GKY(F8): return VK_F8;
  case DEF_GKY(F9): return VK_F9;
  case DEF_GKY(F10): return VK_F10;
  case DEF_GKY(F11): return VK_F11;
  case DEF_GKY(F12): return VK_F12;
  case DEF_GKY(KP_0): return VK_NUMPAD0;
  case DEF_GKY(KP_1): return VK_NUMPAD1;
  case DEF_GKY(KP_2): return VK_NUMPAD2;
  case DEF_GKY(KP_3): return VK_NUMPAD3;
  case DEF_GKY(KP_4): return VK_NUMPAD4;
  case DEF_GKY(KP_5): return VK_NUMPAD5;
  case DEF_GKY(KP_6): return VK_NUMPAD6;
  case DEF_GKY(KP_7): return VK_NUMPAD7;
  case DEF_GKY(KP_8): return VK_NUMPAD8;
  case DEF_GKY(KP_9): return VK_NUMPAD9;
  case DEF_GKY(KP_Multiply): return VK_MULTIPLY;
  case DEF_GKY(KP_Add): return VK_ADD;
  case DEF_GKY(KP_Separator): return VK_SEPARATOR;
  case DEF_GKY(KP_Subtract): return VK_SUBTRACT;
  case DEF_GKY(KP_Decimal): return VK_DECIMAL;
  case DEF_GKY(KP_Divide): return VK_DIVIDE;
  case DEF_GKY(Num_Lock): return VK_NUMLOCK;
  }
  return 0;
}

static LRESULT SendMouseMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  if (!hwnd || !hwnd->m_wndproc) return -1;
  if (!IsWindowEnabled(hwnd)) 
  {
    if (msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN || msg == WM_MBUTTONDOWN ||
        msg == WM_LBUTTONDBLCLK || msg == WM_RBUTTONDBLCLK || msg == WM_MBUTTONDBLCLK)
    {
      HWND h = DialogBoxIsActive();
      if (h) SetForegroundWindow(h);
    }
    return -1;
  }

  LRESULT htc=0;
  if (msg != WM_MOUSEWHEEL && !GetCapture())
  {
    DWORD p=GetMessagePos(); 

    htc=hwnd->m_wndproc(hwnd,WM_NCHITTEST,0,p); 
    if (hwnd->m_hashaddestroy||!hwnd->m_wndproc) 
    {
      return -1; // if somehow WM_NCHITTEST destroyed us, bail
    }
     
    if (htc!=HTCLIENT || swell_window_wants_all_input() == hwnd)
    { 
      if (msg==WM_MOUSEMOVE) return hwnd->m_wndproc(hwnd,WM_NCMOUSEMOVE,htc,p); 
//      if (msg==WM_MOUSEWHEEL) return hwnd->m_wndproc(hwnd,WM_NCMOUSEWHEEL,htc,p); 
//      if (msg==WM_MOUSEHWHEEL) return hwnd->m_wndproc(hwnd,WM_NCMOUSEHWHEEL,htc,p); 
      if (msg==WM_LBUTTONUP) return hwnd->m_wndproc(hwnd,WM_NCLBUTTONUP,htc,p); 
      if (msg==WM_LBUTTONDOWN) return hwnd->m_wndproc(hwnd,WM_NCLBUTTONDOWN,htc,p); 
      if (msg==WM_LBUTTONDBLCLK) return hwnd->m_wndproc(hwnd,WM_NCLBUTTONDBLCLK,htc,p); 
      if (msg==WM_RBUTTONUP) return hwnd->m_wndproc(hwnd,WM_NCRBUTTONUP,htc,p); 
      if (msg==WM_RBUTTONDOWN) return hwnd->m_wndproc(hwnd,WM_NCRBUTTONDOWN,htc,p); 
      if (msg==WM_RBUTTONDBLCLK) return hwnd->m_wndproc(hwnd,WM_NCRBUTTONDBLCLK,htc,p); 
      if (msg==WM_MBUTTONUP) return hwnd->m_wndproc(hwnd,WM_NCMBUTTONUP,htc,p); 
      if (msg==WM_MBUTTONDOWN) return hwnd->m_wndproc(hwnd,WM_NCMBUTTONDOWN,htc,p); 
      if (msg==WM_MBUTTONDBLCLK) return hwnd->m_wndproc(hwnd,WM_NCMBUTTONDBLCLK,htc,p); 
    } 
  }


  LRESULT ret=hwnd->m_wndproc(hwnd,msg,wParam,lParam);

  if (msg==WM_LBUTTONUP || msg==WM_RBUTTONUP || msg==WM_MOUSEMOVE || msg==WM_MBUTTONUP) 
  {
    if (!GetCapture() && (hwnd->m_hashaddestroy || !hwnd->m_wndproc || !hwnd->m_wndproc(hwnd,WM_SETCURSOR,(WPARAM)hwnd,htc | (msg<<16))))    
    {
      SetCursor(SWELL_LoadCursor(IDC_ARROW));
    }
  }

  return ret;
}

static int hex_parse(char c)
{
  if (c >= '0' && c <= '9') return c-'0';
  if (c >= 'A' && c <= 'F') return 10+c-'A';
  if (c >= 'a' && c <= 'f') return 10+c-'a';
  return -1;
}

static GdkAtom utf8atom() 
{
  static GdkAtom tmp;
  if (!tmp) tmp = gdk_atom_intern_static_string("UTF8_STRING");
  return tmp;
}
static GdkAtom tgtatom() 
{
  static GdkAtom tmp;
  if (!tmp) tmp = gdk_atom_intern_static_string("TARGETS");
  return tmp;
}
static GdkAtom urilistatom() 
{
  static GdkAtom tmp;
  if (!tmp) tmp = gdk_atom_intern_static_string("text/uri-list");
  return tmp;
}


static void OnSelectionRequestEvent(GdkEventSelection *b)
{
  //printf("got sel req %s\n",gdk_atom_name(b->target));
  GdkAtom prop=GDK_NONE;

  if (swell_dragsrc_osw && b->window == swell_dragsrc_osw)
  {
    if (swell_dragsrc_hwnd)
    {
      if (b->target == tgtatom())
      {
        prop = b->property;
        GdkAtom list[] = { urilistatom() };
#if SWELL_TARGET_GDK == 2
        GdkWindow *pw = gdk_window_lookup(b->requestor);
        if (!pw) pw = gdk_window_foreign_new(b->requestor);
#else
        GdkWindow *pw = b->requestor;
#endif
        if (pw)
          gdk_property_change(pw,prop,GDK_SELECTION_TYPE_ATOM,32, GDK_PROP_MODE_REPLACE,(guchar*)list,(int) (sizeof(list)/sizeof(list[0])));
      }
      SendMessage(swell_dragsrc_hwnd,WM_USER+100,(WPARAM)b,(LPARAM)&prop);
    }
  }
  else if (s_clipboard_setstate)
  {
    if (b->target == tgtatom())
    {
      if (s_clipboard_setstate_fmt)
      {
        prop = b->property;
        GdkAtom list[] = { s_clipboard_setstate_fmt };
#if SWELL_TARGET_GDK == 2
        GdkWindow *pw = gdk_window_lookup(b->requestor);
        if (!pw) pw = gdk_window_foreign_new(b->requestor);
#else
        GdkWindow *pw = b->requestor;
#endif
        if (pw)
          gdk_property_change(pw,prop,GDK_SELECTION_TYPE_ATOM,32, GDK_PROP_MODE_REPLACE,(guchar*)list,(int) (sizeof(list)/sizeof(list[0])));
      }
    }
    else 
    {
      if (b->target == s_clipboard_setstate_fmt || 
          (b->target == GDK_TARGET_STRING && s_clipboard_setstate_fmt == utf8atom())
         )
      {
        prop = b->property;
        int len = GlobalSize(s_clipboard_setstate);
        guchar *ptr = (guchar*)s_clipboard_setstate;

        WDL_FastString str;
        if (s_clipboard_setstate_fmt == utf8atom())
        {
          const char *rd = (const char *)s_clipboard_setstate;
          while (*rd)
          {
            if (!strncmp(rd,"\r\n",2))
            {
              str.Append("\n");
              rd+=2;
            }
            else
              str.Append(rd++,1);
          }
          ptr = (guchar *)str.Get();
          len = str.GetLength();
        }
#if SWELL_TARGET_GDK == 2
        GdkWindow *pw = gdk_window_lookup(b->requestor);
        if (!pw) pw = gdk_window_foreign_new(b->requestor);
#else
        GdkWindow *pw = b->requestor;
#endif
        if (pw)
          gdk_property_change(pw,prop,b->target,8, GDK_PROP_MODE_REPLACE,ptr,len);
      }
    }
  }
  gdk_selection_send_notify(b->requestor,b->selection,b->target,prop,GDK_CURRENT_TIME);
}

static void OnExposeEvent(GdkEventExpose *exp)
{
  HWND hwnd = swell_oswindow_to_hwnd(exp->window);
  if (!hwnd) return;

#ifdef SWELL_LICE_GDI
  RECT r,cr;

  // don't use GetClientRect(),since we're getting it pre-NCCALCSIZE etc

  cr.left=cr.top=0;
  cr.right = hwnd->m_position.right - hwnd->m_position.left;
  cr.bottom = hwnd->m_position.bottom - hwnd->m_position.top;

  r.left = exp->area.x; 
  r.top=exp->area.y; 
  r.bottom=r.top+exp->area.height; 
  r.right=r.left+exp->area.width;

  if (!hwnd->m_backingstore) hwnd->m_backingstore = new LICE_CairoBitmap;

  bool forceref = hwnd->m_backingstore->resize(cr.right-cr.left,cr.bottom-cr.top);
  if (forceref) r = cr;

  LICE_SubBitmap tmpbm(hwnd->m_backingstore,r.left,r.top,r.right-r.left,r.bottom-r.top);

  if (tmpbm.getWidth()>0 && tmpbm.getHeight()>0) 
  {
    void SWELL_internalLICEpaint(HWND hwnd, LICE_IBitmap *bmout, int bmout_xpos, int bmout_ypos, bool forceref);
    SWELL_internalLICEpaint(hwnd, &tmpbm, r.left, r.top, forceref);

    GdkRectangle rrr={r.left,r.top,r.right-r.left,r.bottom-r.top};
    gdk_window_begin_paint_rect(exp->window, &rrr);

    cairo_t *crc = gdk_cairo_create (exp->window);
    LICE_IBitmap *bm = hwnd->m_backingstore;
    cairo_surface_t *temp_surface = (cairo_surface_t*)bm->Extended(0xca140,NULL);
    if (temp_surface) cairo_set_source_surface(crc, temp_surface, 0,0);
    cairo_paint(crc);
    cairo_destroy(crc);
    if (temp_surface) bm->Extended(0xca140,temp_surface); // release

    gdk_window_end_paint(exp->window);
  }
#endif
}

static void OnConfigureEvent(GdkEventConfigure *cfg)
{
  HWND hwnd = swell_oswindow_to_hwnd(cfg->window);
  if (!hwnd) return;
  int flag=0;
  if (cfg->x != hwnd->m_position.left || 
      cfg->y != hwnd->m_position.top || 
      !hwnd->m_has_had_position)
  {
    flag|=1;
    hwnd->m_has_had_position = true;
  }
  if (cfg->width != hwnd->m_position.right-hwnd->m_position.left || 
      cfg->height != hwnd->m_position.bottom - hwnd->m_position.top) flag|=2;
  hwnd->m_position.left = cfg->x;
  hwnd->m_position.top = cfg->y;
  hwnd->m_position.right = cfg->x + cfg->width;
  hwnd->m_position.bottom = cfg->y + cfg->height;
  if (flag&1) SendMessage(hwnd,WM_MOVE,0,0);
  if (flag&2) SendMessage(hwnd,WM_SIZE,0,0);
  if (!hwnd->m_hashaddestroy && hwnd->m_oswindow) swell_recalcMinMaxInfo(hwnd);
}

static void OnKeyEvent(GdkEventKey *k)
{
  HWND hwnd = swell_oswindow_to_hwnd(k->window);
  if (!hwnd) return;

  int modifiers = 0;
  if (k->state&GDK_CONTROL_MASK) modifiers|=FCONTROL;
  if (k->state&GDK_MOD1_MASK) modifiers|=FALT;
  if (k->state&SWELL_WINDOWSKEY_GDK_MASK) modifiers|=FLWIN;
  if (k->state&GDK_SHIFT_MASK) modifiers|=FSHIFT;

  UINT msgtype = k->type == GDK_KEY_PRESS ? WM_KEYDOWN : WM_KEYUP;

  guint kv = swell_gdkConvertKey(k->keyval);
  if (kv) 
  {
    modifiers |= FVIRTKEY;
  }
  else 
  {
    kv = k->keyval;
    if (swell_is_virtkey_char(kv))
    {
      if (kv >= 'a' && kv <= 'z') 
      {
        kv += 'A'-'a';
        swell_is_likely_capslock = (modifiers&FSHIFT)!=0;
      }
      else if (kv >= 'A' && kv <= 'Z') 
      {
        swell_is_likely_capslock = (modifiers&FSHIFT)==0;
      }
      modifiers |= FVIRTKEY;
    }
    else 
    {
      if (kv >= DEF_GKY(Shift_L) ||
          (kv >= DEF_GKY(ISO_Lock) &&
           kv <= DEF_GKY(ISO_Last_Group_Lock))
         )
      {
        if (kv == DEF_GKY(Shift_L) || kv == DEF_GKY(Shift_R)) kv = VK_SHIFT;
        else if (kv == DEF_GKY(Control_L) || kv == DEF_GKY(Control_R)) kv = VK_CONTROL;
        else if (kv == DEF_GKY(Alt_L) || kv == DEF_GKY(Alt_R)) kv = VK_MENU;
        else if (kv == DEF_GKY(Super_L) || kv == DEF_GKY(Super_R)) kv = VK_LWIN;
        else return; // unknown modifie key

        msgtype = k->type == GDK_KEY_PRESS ? WM_SYSKEYDOWN : WM_SYSKEYUP;
        modifiers|=FVIRTKEY;
      }
      else if (kv > 255) 
      {
        guint v = gdk_keyval_to_unicode(kv);
        if (v) kv=v;
      }
      else
      {
        // treat as ASCII, clear shift flag (?)
        modifiers &= ~FSHIFT;
      }
    }
  }

  HWND foc = GetFocusIncludeMenus();
  if (foc && IsChild(hwnd,foc)) hwnd=foc;
  else if (foc && foc->m_oswindow && !(foc->m_style&WS_CAPTION)) hwnd=foc; // for menus, event sent to other window due to gdk_window_set_override_redirect()

  MSG msg = { hwnd, msgtype, kv, modifiers, };
  INT_PTR extra_flags = 0;
  if (DialogBoxIsActive()) extra_flags |= 1;
  if (SWELLAppMain(SWELLAPP_PROCESSMESSAGE,(INT_PTR)&msg,extra_flags)<=0)
    SendMessage(hwnd, msg.message, kv, modifiers);
}

static HWND getMouseTarget(SWELL_OSWINDOW osw, POINT p, const HWND *hwnd_has_osw)
{
  HWND hwnd = GetCapture();
  if (hwnd) return hwnd;
  hwnd = hwnd_has_osw ? *hwnd_has_osw : swell_oswindow_to_hwnd(osw);
  if (!hwnd || swell_window_wants_all_input() == hwnd) return hwnd;
  return ChildWindowFromPoint(hwnd,p);
}

static void OnMotionEvent(GdkEventMotion *m)
{
  swell_lastMessagePos = MAKELONG(((int)m->x_root&0xffff),((int)m->y_root&0xffff));
  POINT p={(int)m->x, (int)m->y};
  HWND hwnd = getMouseTarget(m->window,p,NULL);

  if (hwnd)
  {
    POINT p2={(int)m->x_root, (int)m->y_root};
    ScreenToClient(hwnd, &p2);
    if (hwnd) hwnd->Retain();
    SendMouseMessage(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(p2.x, p2.y));
    if (hwnd) hwnd->Release();
  }
}

static void OnScrollEvent(GdkEventScroll *b)
{
  swell_lastMessagePos = MAKELONG(((int)b->x_root&0xffff),((int)b->y_root&0xffff));
  POINT p={(int)b->x, (int)b->y};

  HWND hwnd = getMouseTarget(b->window,p,NULL);
  if (hwnd)
  {
    POINT p2={(int)b->x_root, (int)b->y_root};
    // p2 is screen coordinates for WM_MOUSEWHEEL

    int msg=(b->direction == GDK_SCROLL_UP || b->direction == GDK_SCROLL_DOWN) ? WM_MOUSEWHEEL :
            (b->direction == GDK_SCROLL_LEFT || b->direction == GDK_SCROLL_RIGHT) ? WM_MOUSEHWHEEL : 0;
  
    if (msg) 
    {
      int v = (b->direction == GDK_SCROLL_UP || b->direction == GDK_SCROLL_LEFT) ? 120 : -120;

      if (hwnd) hwnd->Retain();
      SendMouseMessage(hwnd, msg, (v<<16), MAKELPARAM(p2.x, p2.y));
      if (hwnd) hwnd->Release();
    }
  }
}

static DWORD s_last_focus_change_time;

static void OnButtonEvent(GdkEventButton *b)
{
  HWND hwnd = swell_oswindow_to_hwnd(b->window);
  if (!hwnd) return;
  swell_lastMessagePos = MAKELONG(((int)b->x_root&0xffff),((int)b->y_root&0xffff));
  POINT p={(int)b->x, (int)b->y};
  HWND hwnd2 = getMouseTarget(b->window,p,&hwnd);

  POINT p2={(int)b->x_root, (int)b->y_root};
  ScreenToClient(hwnd2, &p2);

  int msg=WM_LBUTTONDOWN;
  if (b->button==2) msg=WM_MBUTTONDOWN;
  else if (b->button==3) msg=WM_RBUTTONDOWN;

  if (hwnd2) hwnd2->Retain();

  if (b->type == GDK_BUTTON_PRESS)
  {
    DWORD now = GetTickCount();;
    HWND oldFocus=GetFocus();
    if (!oldFocus || 
        oldFocus != hwnd2 ||
       (now >= s_last_focus_change_time && now < (s_last_focus_change_time+500)))
    {
      if (IsWindowEnabled(hwnd2))
        SendMessage(hwnd2,WM_MOUSEACTIVATE,0,0);
    }
  }

  if (hwnd && hwnd->m_oswindow && SWELL_focused_oswindow != hwnd->m_oswindow)
  {
    // this should not be necessary, focus is sent via separate events
    // (the only time I've ever seen this is when launching a popup menu via the mousedown handler, on the mouseup
    // the menu has not yet been focused but the mouse event goes to the popup menu)
    SWELL_focused_oswindow = hwnd->m_oswindow;
  }


  // for doubleclicks, GDK actually seems to send:
  //   GDK_BUTTON_PRESS, GDK_BUTTON_RELEASE, 
  //   GDK_BUTTON_PRESS, GDK_2BUTTON_PRESS, GDK_BUTTON_RELEASE
  // win32 expects:
  //   WM_LBUTTONDOWN, WM_LBUTTONUP, WM_LBUTTONDBLCLK, WM_LBUTTONUP
  // what we send:
  //   WM_LBUTTONDOWN, WM_LBUTTONUP, WM_LBUTTONDOWN, WM_LBUTTONUP, 
  //   WM_LBUTTONDBLCLK, WM_LBUTTONUP
  // there is an extra down/up pair, but it should behave fine with most code
  // (one hopes)

  if(b->type == GDK_BUTTON_RELEASE)
  {
   msg++; // convert WM_xBUTTONDOWN to WM_xBUTTONUP
  }
  else if(b->type == GDK_2BUTTON_PRESS) 
  {
    msg++; // convert WM_xBUTTONDOWN to WM_xBUTTONUP
    SendMouseMessage(hwnd2, msg, 0, MAKELPARAM(p2.x, p2.y));
    msg++; // convert WM_xBUTTONUP to WM_xBUTTONDBLCLK
  }

  SendMouseMessage(hwnd2, msg, 0, MAKELPARAM(p2.x, p2.y));
  if (hwnd2) hwnd2->Release();
}


static void OnSelectionNotifyEvent(GdkEventSelection *b)
{
  HWND hwnd = swell_oswindow_to_hwnd(b->window);
  if (!hwnd) return;

  if (hwnd == s_ddrop_hwnd && b->target == urilistatom())
  {
    POINT p = s_ddrop_pt;
    HWND cw=hwnd;
    RECT r;
    GetWindowContentViewRect(hwnd,&r);
    if (PtInRect(&r,p))
    {
      p.x -= r.left;
      p.y -= r.top;
      cw = ChildWindowFromPoint(hwnd,p);
    }
    if (!cw) cw=hwnd;

    guchar *gptr=NULL;
    GdkAtom fmt;
    gint unitsz=0;
    gint sz=gdk_selection_property_get(b->window,&gptr,&fmt,&unitsz);

    if (sz>0 && gptr)
    {
      HANDLE gobj=GlobalAlloc(0,sz+sizeof(DROPFILES));
      if (gobj)
      {
        DROPFILES *df=(DROPFILES*)gobj;
        df->pFiles = sizeof(DROPFILES);
        df->pt = s_ddrop_pt;
        ScreenToClient(cw,&df->pt);
        df->fNC=FALSE;
        df->fWide=FALSE;
        guchar *pout = (guchar *)(df+1);
        const guchar *rd = gptr;
        const guchar *rd_end = rd + sz;
        for (;;)
        {
          while (rd < rd_end && *rd && isspace(*rd)) rd++;
          if (rd >= rd_end) break;

          if (rd+7 < rd_end && !strnicmp((const char *)rd,"file://",7))
          {
            rd += 7;
            int c=0;
            while (rd < rd_end && *rd && !isspace(*rd))
            {
              int v1,v2;
              if (*rd == '%' && rd+2 < rd_end && (v1=hex_parse(rd[1]))>=0 && (v2=hex_parse(rd[2]))>=0)
              {
                *pout++ = (v1<<4) | v2;
                rd+=3;
              }
              else
              {
                *pout++ = *rd++;
              }
              c++;
            }
            if (c) *pout++=0;
          }
          else
          {
            while (rd < rd_end && *rd && !isspace(*rd)) rd++;
          }
        }
        *pout++=0;
        *pout++=0;

        SendMessage(cw,WM_DROPFILES,(WPARAM)gobj,0);
        GlobalFree(gobj);
      }
    }

    if (gptr) g_free(gptr);
    s_ddrop_hwnd=NULL;
    return;
  }

  s_ddrop_hwnd=NULL;

  if (s_clipboard_getstate) { GlobalFree(s_clipboard_getstate); s_clipboard_getstate=NULL; }
  guchar *gptr=NULL;
  GdkAtom fmt;
  gint unitsz=0;
  gint sz=gdk_selection_property_get(b->window,&gptr,&fmt,&unitsz);
  if (sz>0 && gptr && (unitsz == 8 || unitsz == 16 || unitsz == 32))
  {
    WDL_FastString str;
    guchar *ptr = gptr;
    if (fmt == GDK_TARGET_STRING || fmt == utf8atom())
    {
      int lastc=0;
      while (sz-->0)
      {
        int c;
        if (unitsz==32) { c = *(unsigned int *)ptr; ptr+=4; }
        else if (unitsz==16)  { c = *(unsigned short *)ptr; ptr+=2; }
        else c = *ptr++;

        if (!c) break;

        if (c == '\n' && lastc != '\r') str.Append("\r",1);

        char bv[8];
        if (fmt != GDK_TARGET_STRING)
        {
          bv[0] = (char) ((unsigned char)c);
          str.Append(bv,1);
        } 
        else
        {
          WDL_MakeUTFChar(bv,c,sizeof(bv));
          str.Append(bv);
        }

        lastc=c;
      }
      ptr = (guchar*)str.Get();
      sz=str.GetLength()+1;
    }
    else if (unitsz>8) sz *= (unitsz/8);

    s_clipboard_getstate = GlobalAlloc(0,sz);
    if (s_clipboard_getstate)
    {
      memcpy(s_clipboard_getstate,ptr,sz);
      s_clipboard_getstate_fmt = fmt;
    }
  }
  if (gptr) g_free(gptr);
}

static void OnDropStartEvent(GdkEventDND *e)
{
  HWND hwnd = swell_oswindow_to_hwnd(e->window);
  if (!hwnd) return;

  GdkDragContext *ctx = e->context;
  if (ctx)
  {
    POINT p = { (int)e->x_root, (int)e->y_root };
    s_ddrop_hwnd = hwnd;
    s_ddrop_pt = p;

    GdkAtom srca = gdk_drag_get_selection(ctx);
    gdk_selection_convert(e->window,srca,urilistatom(),e->time);
    gdk_drop_finish(ctx,TRUE,e->time);
  }
}

static bool is_our_oswindow(GdkWindow *w)
{
  while (w)
  {
    HWND hwnd = swell_oswindow_to_hwnd(w);
    if (hwnd) return true;
    w = gdk_window_get_effective_parent(w);
  }
  return false;

}

static void deactivateTimer(HWND hwnd, UINT uMsg, UINT_PTR tm, DWORD dwt)
{
  KillTimer(NULL,s_deactivate_timer);
  s_deactivate_timer=0;
  if (swell_app_is_inactive) return;
  GdkWindow *window = gdk_screen_get_active_window(gdk_screen_get_default());
  if (!is_our_oswindow(window))
    on_deactivate();
}

extern SWELL_OSWINDOW swell_ignore_focus_oswindow;
extern DWORD swell_ignore_focus_oswindow_until;

static void swell_gdkEventHandler(GdkEvent *evt, gpointer data)
{
  GdkEvent *oldEvt = s_cur_evt;
  s_cur_evt = evt;

  switch (evt->type)
  {
    case GDK_FOCUS_CHANGE:
        {
          GdkEventFocus *fc = (GdkEventFocus *)evt;
          if (s_deactivate_timer) 
          {
            KillTimer(NULL,s_deactivate_timer);
            s_deactivate_timer=0;
          }
          if (fc->in && is_our_oswindow(fc->window))
          {
            s_last_focus_change_time = GetTickCount();
            swell_on_toplevel_raise(fc->window);
            if (swell_ignore_focus_oswindow != fc->window || 
                GetTickCount() > swell_ignore_focus_oswindow_until)
            {
              SWELL_focused_oswindow = fc->window;
            }
            if (swell_app_is_inactive)
            {
              on_activate(0);
            }
          }
          else if (!swell_app_is_inactive)
          {
            s_deactivate_timer = SetTimer(NULL,0,200,deactivateTimer);
          }
        }
    break;
    case GDK_SELECTION_REQUEST:
        OnSelectionRequestEvent((GdkEventSelection *)evt);
    break;

    case GDK_DELETE:
     {
       HWND hwnd = swell_oswindow_to_hwnd(((GdkEventAny*)evt)->window);
       if (hwnd && IsWindowEnabled(hwnd) && !SendMessage(hwnd,WM_CLOSE,0,0))
        SendMessage(hwnd,WM_COMMAND,IDCANCEL,0);
     }
    break;
    case GDK_EXPOSE: // paint! GdkEventExpose...
      OnExposeEvent((GdkEventExpose *)evt);
    break;
    case GDK_CONFIGURE: // size/move, GdkEventConfigure
      OnConfigureEvent((GdkEventConfigure*)evt);
    break;
    case GDK_WINDOW_STATE: /// GdkEventWindowState for min/max
          //printf("minmax\n");
    break;
    case GDK_GRAB_BROKEN:
      if (swell_oswindow_to_hwnd(((GdkEventAny*)evt)->window))
      {
        if (swell_captured_window)
        {
          SendMessage(swell_captured_window,WM_CAPTURECHANGED,0,0);
          swell_captured_window=0;
        }
      }
    break;
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      swell_dlg_destroyspare();
      OnKeyEvent((GdkEventKey *)evt);
    break;
#ifdef GDK_AVAILABLE_IN_3_4
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_UPDATE:
    case GDK_TOUCH_END:
    case GDK_TOUCH_CANCEL:
      {
        GdkEventTouch *e = (GdkEventTouch *)evt;
        static guint32 touchptr_lasttime; 
        bool doubletap = false;
        if (evt->type == GDK_TOUCH_BEGIN && !g_swell_touchptr)
        {
          DWORD now = e->time;
          doubletap = touchptr_lasttime && 
                      now >= touchptr_lasttime && 
                      now < touchptr_lasttime+350;
          touchptr_lasttime = now;
          g_swell_touchptr = e->sequence;
          g_swell_touchptr_wnd = e->window;
        }

        if (!e->sequence || e->sequence != g_swell_touchptr) 
        {
          touchptr_lasttime=0;
          break;
        }

        if (e->type == GDK_TOUCH_UPDATE)
        {
          GdkEventMotion m;
          memset(&m,0,sizeof(m));
          m.type = GDK_MOTION_NOTIFY;
          m.window = e->window;
          m.time = e->time;
          m.x = e->x;
          m.y = e->y;
          m.axes = e->axes;
          m.state = e->state;
          m.device = e->device;
          m.x_root = e->x_root;
          m.y_root = e->y_root;
          OnMotionEvent(&m);
        }
        else
        {
          GdkEventButton but;
          memset(&but,0,sizeof(but));
          if (e->type == GDK_TOUCH_BEGIN) 
          {
            but.type = doubletap ? GDK_2BUTTON_PRESS:GDK_BUTTON_PRESS;
          }
          else 
          {
            but.type = GDK_BUTTON_RELEASE;
            g_swell_touchptr = NULL;
          }
          but.window = e->window;
          but.time = e->time;
          but.x = e->x;
          but.y = e->y;
          but.axes = e->axes;
          but.state = e->state;
          but.device = e->device;
          but.button = 1;
          but.x_root = e->x_root;
          but.y_root = e->y_root;
          swell_dlg_destroyspare();
          OnButtonEvent(&but);
        } 
      }
    break;
#endif
    case GDK_MOTION_NOTIFY:
      gdk_event_request_motions((GdkEventMotion *)evt);
      OnMotionEvent((GdkEventMotion *)evt);
    break;
    case GDK_SCROLL:
      OnScrollEvent((GdkEventScroll*)evt);
    break;
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      swell_dlg_destroyspare();
      OnButtonEvent((GdkEventButton*)evt);
    break;
    case GDK_SELECTION_NOTIFY:
      OnSelectionNotifyEvent((GdkEventSelection *)evt);
    break;
    case GDK_DRAG_ENTER:
    case GDK_DRAG_MOTION:
      if (swell_oswindow_to_hwnd(((GdkEventAny*)evt)->window))
      {
        GdkEventDND *e = (GdkEventDND *)evt;
        if (e->context)
        {
          gdk_drag_status(e->context,GDK_ACTION_COPY,e->time);
          //? gdk_drop_reply(e->context,TRUE,e->time);
        }
      }
    break;
    case GDK_DRAG_LEAVE:
    case GDK_DRAG_STATUS:
    case GDK_DROP_FINISHED:
    break;
    case GDK_DROP_START:
      OnDropStartEvent((GdkEventDND *)evt);
    break;

    default:
          //printf("msg: %d\n",evt->type);
    break;
  }
#ifdef SWELL_SUPPORT_GTK
  gtk_main_do_event(evt);
#endif
  s_cur_evt = oldEvt;
}

void SWELL_RunEvents()
{
  if (SWELL_gdk_active>0) 
  {
#if 0 && defined(SWELL_SUPPORT_GTK)
    // does not seem to be necessary
    while (gtk_events_pending())
      gtk_main_iteration();
#else

#if SWELL_TARGET_GDK == 2
    gdk_window_process_all_updates();
#endif

    GMainContext *ctx=g_main_context_default();
    while (g_main_context_iteration(ctx,FALSE))
    {
      GdkEvent *evt;
      while (gdk_events_pending() && (evt = gdk_event_get()))
      {
        swell_gdkEventHandler(evt,(gpointer)1);
        gdk_event_free(evt);
      }
    }
#endif
  }
}

void swell_oswindow_update_style(HWND hwnd, LONG oldstyle)
{
  const LONG val = hwnd->m_style, ret = oldstyle;
  if (hwnd->m_oswindow && ((ret^val)& WS_CAPTION))
  {
    gdk_window_hide(hwnd->m_oswindow);
    if (val & WS_CAPTION)
    {
      if (val & WS_THICKFRAME)
        gdk_window_set_decorations(hwnd->m_oswindow,(GdkWMDecoration) (GDK_DECOR_ALL | GDK_DECOR_MENU));
      else
        gdk_window_set_decorations(hwnd->m_oswindow,(GdkWMDecoration) (GDK_DECOR_BORDER|GDK_DECOR_TITLE|GDK_DECOR_MINIMIZE));
    }
    else
    {
      gdk_window_set_decorations(hwnd->m_oswindow,(GdkWMDecoration) 0);
    }
    hwnd->m_oswindow_private |= PRIVATE_NEEDSHOW;
  }
}

void swell_oswindow_update_enable(HWND hwnd)
{
  if (hwnd->m_oswindow && !swell_app_is_inactive) 
    gdk_window_set_accept_focus(hwnd->m_oswindow,hwnd->m_enabled);
}

int SWELL_SetWindowLevel(HWND hwnd, int newlevel)
{
  int rv=0;
  if (hwnd)
  {
    rv = hwnd->m_israised ? 1 : 0;
    hwnd->m_israised = newlevel>0;
    if (hwnd->m_oswindow) gdk_window_set_keep_above(hwnd->m_oswindow,newlevel>0 && !swell_app_is_inactive);
  }
  return rv;
}

void SWELL_GetViewPort(RECT *r, const RECT *sourcerect, bool wantWork)
{
  if (swell_initwindowsys())
  {
    GdkScreen *defscr = gdk_screen_get_default();
    if (!defscr) { r->left=r->top=0; r->right=r->bottom=1024; return; }
    gint idx = sourcerect ? gdk_screen_get_monitor_at_point(defscr,
           (sourcerect->left+sourcerect->right)/2,
           (sourcerect->top+sourcerect->bottom)/2) : 0;
    GdkRectangle rc={0,0,1024,1024};
    gdk_screen_get_monitor_geometry(defscr,idx,&rc);
    r->left=rc.x; r->top = rc.y;
    r->right=rc.x+rc.width;
    r->bottom=rc.y+rc.height;
    return;
  }
  r->left=r->top=0;
  r->right=1024;
  r->bottom=768;
}


bool GetWindowRect(HWND hwnd, RECT *r)
{
  if (!hwnd) return false;
  if (hwnd->m_oswindow)
  {
#ifdef SWELL_GDK_IMPROVE_WINDOWRECT
    GdkRectangle gr;
    gdk_window_get_frame_extents(hwnd->m_oswindow,&gr);

    r->left=gr.x;
    r->top=gr.y;
    r->right=gr.x + gr.width;
    r->bottom = gr.y + gr.height;
#else
    // this is wrong (returns client rect in screen coordinates), but gdk_window_get_frame_extents() doesn't seem to work 
    gint x=hwnd->m_position.left,y=hwnd->m_position.top;
    gdk_window_get_root_origin(hwnd->m_oswindow,&x,&y);
    r->left=x;
    r->top=y;
    r->right=x + hwnd->m_position.right - hwnd->m_position.left;
    r->bottom = y + hwnd->m_position.bottom - hwnd->m_position.top;
#endif

    return true;
  }

  r->left=r->top=0; 
  ClientToScreen(hwnd,(LPPOINT)r);
  r->right = r->left + hwnd->m_position.right - hwnd->m_position.left;
  r->bottom = r->top + hwnd->m_position.bottom - hwnd->m_position.top;
  return true;
}

void swell_oswindow_begin_resize(SWELL_OSWINDOW wnd)
{
  // make sure window is resizable (hints will be re-set on upcoming CONFIGURE event)
  gdk_window_set_geometry_hints(wnd,NULL,(GdkWindowHints) 0); 
}

void swell_oswindow_resize(SWELL_OSWINDOW wnd, int reposflag, RECT f)
{
#ifdef SWELL_GDK_IMPROVE_WINDOWRECT
  if (reposflag & 2)
  {
    // increase size to include titlebars etc
    GdkRectangle gr;
    gdk_window_get_frame_extents(wnd,&gr);
    gint cw=gr.width, ch=gr.height;
    gdk_window_get_geometry(wnd,NULL,NULL,&cw,&ch);
    // when it matters, this seems to always make gr.height=ch, which is pointless
    f.right -= gr.width - cw;
    f.bottom -= gr.height - ch;
  }
#endif
  if ((reposflag&3)==3) gdk_window_move_resize(wnd,f.left,f.top,f.right-f.left,f.bottom-f.top);
  else if (reposflag&2) gdk_window_resize(wnd,f.right-f.left,f.bottom-f.top);
  else if (reposflag&1) gdk_window_move(wnd,f.left,f.top);
}

void swell_oswindow_postresize(HWND hwnd, RECT f) 
{ 
  if (hwnd->m_oswindow && (hwnd->m_oswindow_private&PRIVATE_NEEDSHOW) && !hwnd->m_oswindow_fullscreen)
  {
    gdk_window_show(hwnd->m_oswindow);
    if (hwnd->m_style & WS_CAPTION) gdk_window_unmaximize(hwnd->m_oswindow); // fixes Kwin
    swell_oswindow_resize(hwnd->m_oswindow,3,f); // fixes xfce
    hwnd->m_oswindow_private &= ~PRIVATE_NEEDSHOW;
  }
}

void UpdateWindow(HWND hwnd)
{
#if SWELL_TARGET_GDK == 2
  if (hwnd)
  {
    while (hwnd && !hwnd->m_oswindow) hwnd=hwnd->m_parent;
    if (hwnd && hwnd->m_oswindow) gdk_window_process_updates(hwnd->m_oswindow,true);
  }
#endif
}

void swell_oswindow_invalidate(HWND hwnd, const RECT *r) 
{ 
  GdkRectangle gdkr;
  if (r)
  {
    gdkr.x = r->left;
    gdkr.y = r->top;
    gdkr.width = r->right-r->left;
    gdkr.height = r->bottom-r->top;
  }

  gdk_window_invalidate_rect(hwnd->m_oswindow,r ? &gdkr : NULL,true);
}



bool OpenClipboard(HWND hwndDlg) 
{
  s_clip_hwnd=hwndDlg ? hwndDlg : SWELL_topwindows; 
  if (s_clipboard_getstate)
  {
    GlobalFree(s_clipboard_getstate);
    s_clipboard_getstate = NULL;
  }
  s_clipboard_getstate_fmt = NULL;

  return true; 
}

static HANDLE req_clipboard(GdkAtom type)
{
  if (s_clipboard_getstate_fmt == type) return s_clipboard_getstate;

  HWND h = s_clip_hwnd;
  while (h && !h->m_oswindow) h = h->m_parent;

  if (h && SWELL_gdk_active > 0)
  {
    if (s_clipboard_getstate)
    {
      GlobalFree(s_clipboard_getstate);
      s_clipboard_getstate=NULL;
    }
    gdk_selection_convert(h->m_oswindow,GDK_SELECTION_CLIPBOARD,type,GDK_CURRENT_TIME);
 
    GMainContext *ctx=g_main_context_default();
    DWORD startt = GetTickCount();
    for (;;)
    {
      while (!s_clipboard_getstate && g_main_context_iteration(ctx,FALSE))
      {
        GdkEvent *evt;
        while (!s_clipboard_getstate && gdk_events_pending() && (evt = gdk_event_get()))
        {
          if (evt->type == GDK_SELECTION_NOTIFY || evt->type == GDK_SELECTION_REQUEST)
            swell_gdkEventHandler(evt,(gpointer)1);
          gdk_event_free(evt);
        }
      }

      if (s_clipboard_getstate) 
      {
        if (s_clipboard_getstate_fmt == type) return s_clipboard_getstate;
        return NULL;
      }

      DWORD now = GetTickCount();
      if (now < startt-1000 || now > startt+500) break;
      Sleep(10);
    }
  }
  return NULL;
}

void CloseClipboard() 
{ 
  s_clip_hwnd=NULL; 
}

UINT EnumClipboardFormats(UINT lastfmt)
{
  if (!lastfmt)
  {
    // checking this causes issues (reentrancy, I suppose?)
    //if (req_clipboard(utf8atom()))
    return CF_TEXT;
  }
  if (lastfmt == CF_TEXT) lastfmt = 0;

  int x=0;
  for (;;)
  {
    int fmt=0;
    if (!m_clip_recs.Enumerate(x++,&fmt)) return 0;
    if (lastfmt == 0) return fmt;

    if ((UINT)fmt == lastfmt) return m_clip_recs.Enumerate(x++,&fmt) ? fmt : 0;
  }
}

HANDLE GetClipboardData(UINT type)
{
  if (type == CF_TEXT)
  {
    return req_clipboard(utf8atom());
  }
  return m_clip_recs.Get(type);
}


void EmptyClipboard()
{
  m_clip_recs.DeleteAll();
}

void SetClipboardData(UINT type, HANDLE h)
{
  if (type == CF_TEXT)
  {
    if (s_clipboard_setstate) { GlobalFree(s_clipboard_setstate); s_clipboard_setstate=NULL; }
    s_clipboard_setstate_fmt=NULL;
    static GdkWindow *w;
    if (!w)
    {
      GdkWindowAttr attr={0,};
      attr.title = (char *)"swell clipboard";
      attr.event_mask = GDK_ALL_EVENTS_MASK;
      attr.wclass = GDK_INPUT_ONLY;
      attr.window_type = GDK_WINDOW_TOPLEVEL;
      w = gdk_window_new(NULL,&attr,0);
    }
    if (w)
    {
      s_clipboard_setstate_fmt = utf8atom();
      s_clipboard_setstate = h;
      gdk_selection_owner_set(w,GDK_SELECTION_CLIPBOARD,GDK_CURRENT_TIME,TRUE);
    }
    return;
  }
  if (h) m_clip_recs.Insert(type,h);
  else m_clip_recs.Delete(type);
}

UINT RegisterClipboardFormat(const char *desc)
{
  if (!desc || !*desc) return 0;
  int x;
  const int n = m_clip_curfmts.GetSize();
  for(x=0;x<n;x++) 
    if (!strcmp(m_clip_curfmts.Get(x),desc)) return x + 1;
  m_clip_curfmts.Add(strdup(desc));
  return n+1;
}


void GetCursorPos(POINT *pt)
{
  pt->x=0;
  pt->y=0;
  if (SWELL_gdk_active>0)
  {
//#if SWELL_TARGET_GDK == 3
//    GdkDevice *dev=NULL;
//    if (s_cur_evt) dev = gdk_event_get_device(s_cur_evt);
//    if (!dev) dev = gdk_device_manager_get_client_pointer(gdk_display_get_device_manager(gdk_display_get_default()));
//    if (dev) gdk_device_get_position(dev,NULL,&pt->x,&pt->y);
//#else
    gdk_display_get_pointer(gdk_display_get_default(),NULL,&pt->x,&pt->y,NULL);
//#endif
  }
}


WORD GetAsyncKeyState(int key)
{
  if (SWELL_gdk_active>0)
  {
    GdkModifierType mod=(GdkModifierType)0;
    HWND h = GetFocus();
    while (h && !h->m_oswindow) h = h->m_parent;
//#if SWELL_TARGET_GDK == 3
//    GdkDevice *dev=NULL;
//    if (s_cur_evt) dev = gdk_event_get_device(s_cur_evt);
//    if (!dev) dev = gdk_device_manager_get_client_pointer(gdk_display_get_device_manager(gdk_display_get_default()));
//    if (dev) gdk_window_get_device_position(h?  h->m_oswindow : gdk_get_default_root_window(),dev, NULL, NULL,&mod);
//#else
    gdk_window_get_pointer(h?  h->m_oswindow : gdk_get_default_root_window(),NULL,NULL,&mod);
//#endif
 
    if (key == VK_LBUTTON) return ((mod&GDK_BUTTON1_MASK)||g_swell_touchptr)?0x8000:0;
    if (key == VK_MBUTTON) return (mod&GDK_BUTTON2_MASK)?0x8000:0;
    if (key == VK_RBUTTON) return (mod&GDK_BUTTON3_MASK)?0x8000:0;

    if (key == VK_CONTROL) return (mod&GDK_CONTROL_MASK)?0x8000:0;
    if (key == VK_MENU) return (mod&GDK_MOD1_MASK)?0x8000:0;
    if (key == VK_SHIFT) return (mod&GDK_SHIFT_MASK)?0x8000:0;
    if (key == VK_LWIN) return (mod&SWELL_WINDOWSKEY_GDK_MASK)?0x8000:0;
  }
  return 0;
}

DWORD GetMessagePos()
{  
  return swell_lastMessagePos;
}

struct bridgeState {
  bridgeState(bool needrep, GdkWindow *_w, Window _nw, Display *_disp);
  ~bridgeState();


  GdkWindow *w;
  Window native_w;
  Display *native_disp;

  bool lastvis;
  bool need_reparent;
  RECT lastrect;
};

static WDL_PtrList<bridgeState> filter_windows;
bridgeState::~bridgeState() 
{ 
  filter_windows.DeletePtr(this); 
  if (w) 
  {
    g_object_unref(G_OBJECT(w));
    XDestroyWindow(native_disp,native_w);
  }
}
bridgeState::bridgeState(bool needrep, GdkWindow *_w, Window _nw, Display *_disp)
{
  w=_w;
  native_w=_nw;
  native_disp=_disp;
  lastvis=false;
  need_reparent=needrep;
  memset(&lastrect,0,sizeof(lastrect));
  filter_windows.Add(this);
}

static LRESULT xbridgeProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
    case WM_DESTROY:
      if (hwnd && hwnd->m_private_data)
      {
        bridgeState *bs = (bridgeState*)hwnd->m_private_data;
        hwnd->m_private_data = 0;
        delete bs;
      }
    break;
    case WM_TIMER:
      if (wParam != 1) break;
    case WM_MOVE:
    case WM_SIZE:
      if (hwnd && hwnd->m_private_data)
      {
        bridgeState *bs = (bridgeState*)hwnd->m_private_data;
        if (bs->w)
        {
          HWND h = hwnd->m_parent;
          RECT tr = hwnd->m_position;
          while (h)
          {
            RECT cr = h->m_position;
            if (h->m_oswindow)
            {
              cr.right -= cr.left;
              cr.bottom -= cr.top;
              cr.left=cr.top=0;
            }

            if (h->m_wndproc)
            {
              NCCALCSIZE_PARAMS p = {{ cr }};
              h->m_wndproc(h,WM_NCCALCSIZE,0,(LPARAM)&p);
              cr = p.rgrc[0];
            }
            tr.left += cr.left;
            tr.top += cr.top;
            tr.right += cr.left;
            tr.bottom += cr.top;

            if (tr.left < cr.left) tr.left=cr.left;
            if (tr.top < cr.top) tr.top = cr.top;
            if (tr.right > cr.right) tr.right = cr.right;
            if (tr.bottom > cr.bottom) tr.bottom = cr.bottom;

            if (h->m_oswindow) break;
            h=h->m_parent;
          }

          // todo: need to periodically check to see if the plug-in has resized its window
          bool vis = IsWindowVisible(hwnd);
          if (vis)
          {
#if SWELL_TARGET_GDK == 2
            gint w=0,hh=0,d=0;
            gdk_window_get_geometry(bs->w,NULL,NULL,&w,&hh,&d);
#else
            gint w=0,hh=0;
            gdk_window_get_geometry(bs->w,NULL,NULL,&w,&hh);
#endif
            if (w > bs->lastrect.right-bs->lastrect.left) 
            {
              bs->lastrect.right = bs->lastrect.left + w;
              tr.right++; // workaround "bug" in GDK -- if bs->w was resized via Xlib, GDK won't resize it unless it thinks the size changed
            }
            if (hh > bs->lastrect.bottom-bs->lastrect.top) 
            {
              bs->lastrect.bottom = bs->lastrect.top + hh;
              tr.bottom++; // workaround "bug" in GDK -- if bs->w was resized via Xlib, GDK won't resize it unless it thinks the size changed
            }
          }

          if (h && (bs->need_reparent || (vis != bs->lastvis) || (vis&&memcmp(&tr,&bs->lastrect,sizeof(RECT))))) 
          {
            if (bs->lastvis && !vis)
            {
              gdk_window_hide(bs->w);
              bs->lastvis = false;
            }

            if (bs->need_reparent)
            {
              gdk_window_reparent(bs->w,h->m_oswindow,tr.left,tr.top);
              gdk_window_resize(bs->w, tr.right-tr.left,tr.bottom-tr.top);
              bs->lastrect=tr;

              bs->need_reparent=false;
            }
            else if (memcmp(&tr,&bs->lastrect,sizeof(RECT)))
            {
              bs->lastrect = tr;
              gdk_window_move_resize(bs->w,tr.left,tr.top, tr.right-tr.left, tr.bottom-tr.top);
            }
            if (vis && !bs->lastvis)
            {
              gdk_window_show(bs->w);
              gdk_window_raise(bs->w);
              bs->lastvis = true;
            }
          }
        }
      }
    break;
  }
  return DefWindowProc(hwnd,uMsg,wParam,lParam);
}

static GdkFilterReturn filterCreateShowProc(GdkXEvent *xev, GdkEvent *event, gpointer data)
{
  const XEvent *xevent = (XEvent *)xev;
  if (xevent && xevent->type == CreateNotify)
  {
    for (int x=0;x<filter_windows.GetSize(); x++)
    {
      bridgeState *bs = filter_windows.Get(x);
      if (bs && bs->native_w == xevent->xany.window && bs->native_disp == xevent->xany.display)
      {
        //gint w=0,hh=0;
        //gdk_window_get_geometry(bs->w,NULL,NULL,&w,&hh);
        XMapWindow(bs->native_disp, xevent->xcreatewindow.window);
        //XResizeWindow(bs->native_disp, xevent->xcreatewindow.window,w,hh);
        return GDK_FILTER_REMOVE;
      }
    }
  }
  return GDK_FILTER_CONTINUE;
}

HWND SWELL_CreateXBridgeWindow(HWND viewpar, void **wref, RECT *r)
{
  HWND hwnd = NULL;
  *wref = NULL;

  GdkWindow *ospar = NULL;
  HWND hpar = viewpar;
  while (hpar)
  {
    ospar = hpar->m_oswindow;
    if (ospar) break;
    hpar = hpar->m_parent;
  }

  bool need_reparent=false;

  if (!ospar)
  {
    need_reparent = true;
    ospar = gdk_screen_get_root_window(gdk_screen_get_default());
  }

  Display *disp = gdk_x11_display_get_xdisplay(gdk_window_get_display(ospar));
  Window w = XCreateWindow(disp,GDK_WINDOW_XID(ospar),0,0,r->right-r->left,r->bottom-r->top,0,CopyFromParent, InputOutput, CopyFromParent, 0, NULL);
  GdkWindow *gdkw = w ? gdk_x11_window_foreign_new_for_display(gdk_display_get_default(),w) : NULL;

  hwnd = new HWND__(viewpar,0,r,NULL, true, xbridgeProc);
  bridgeState *bs = gdkw ? new bridgeState(need_reparent,gdkw,w,disp) : NULL;
  hwnd->m_private_data = (INT_PTR) bs;
  if (gdkw)
  {
    *wref = (void *) w;

    XSelectInput(disp, w, StructureNotifyMask | SubstructureNotifyMask);

    static bool filt_add;
    if (!filt_add)
    {
      filt_add=true;
      gdk_window_add_filter(NULL, filterCreateShowProc, NULL);
    }
    SetTimer(hwnd,1,100,NULL);
    if (!need_reparent) SendMessage(hwnd,WM_SIZE,0,0);
  }
  return hwnd;
}

struct dropSourceInfo {
  dropSourceInfo() 
  { 
    srclist=NULL; srccount=0; srcfn=NULL; callback=NULL; 
    state=0;
    dragctx=NULL;
  }
  ~dropSourceInfo() 
  { 
    free(srcfn); 
    if (dragctx)
    {
      if (_gdk_drag_drop_done) _gdk_drag_drop_done(dragctx,state!=0);
      g_object_unref(dragctx);
    }
  }

  const char **srclist;
  int srccount;
  // or
  void (*callback)(const char *);
  char *srcfn;
  
  int state;

  GdkDragContext *dragctx;
};

static void encode_uri(WDL_FastString *s, const char *rd)
{
  while (*rd)
  {
    // unsure if UTF-8 chars should be urlencoded or allowed?
    if (*rd < 0 || (!isalnum(*rd) && *rd != '-' && *rd != '_' && *rd != '.' && *rd != '/'))
    {
      char buf[8];
      snprintf(buf,sizeof(buf),"%%%02x",(int)(unsigned char)*rd);
      s->Append(buf);
    }
    else s->Append(rd,1);

    rd++;
  }
}


static LRESULT WINAPI dropSourceWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  dropSourceInfo *inf = (dropSourceInfo*)hwnd->m_private_data;
  switch (msg)
  {
    case WM_CREATE:
      if (!swell_dragsrc_osw)
      {
        GdkWindowAttr attr={0,};
        attr.title = (char *)"swell drag source";
        attr.event_mask = GDK_ALL_EVENTS_MASK;
        attr.wclass = GDK_INPUT_ONLY;
        attr.window_type = GDK_WINDOW_TOPLEVEL;
        swell_dragsrc_osw = gdk_window_new(NULL,&attr,0);
      }
      if (swell_dragsrc_osw)
      {
        inf->dragctx = gdk_drag_begin(swell_dragsrc_osw, g_list_append(NULL,urilistatom()));
      }
      SetCapture(hwnd);
    break;
    case WM_MOUSEMOVE:
      if (inf->dragctx)
      {
        POINT p;
        GetCursorPos(&p);
        GdkWindow *w = NULL;
        GdkDragProtocol proto;
        gdk_drag_find_window_for_screen(inf->dragctx,NULL,gdk_screen_get_default(),p.x,p.y,&w,&proto);
        // todo: need to update gdk_drag_context_get_drag_window()
        // (or just SetCursor() a drag and drop cursor)
        if (w) 
        {
          gdk_drag_motion(inf->dragctx,w,proto,p.x,p.y,GDK_ACTION_COPY,GDK_ACTION_COPY,GDK_CURRENT_TIME);
        }
      }
    break;
    case WM_LBUTTONUP:
      if (inf->dragctx && !inf->state)
      {
        inf->state=1;
        GdkAtom sel = gdk_drag_get_selection(inf->dragctx);
        if (sel) gdk_selection_owner_set(swell_dragsrc_osw,sel,GDK_CURRENT_TIME,TRUE);
        gdk_drag_drop(inf->dragctx,GDK_CURRENT_TIME);
        if (!sel)
        {
          sel = gdk_drag_get_selection(inf->dragctx);
          if (sel) gdk_selection_owner_set(swell_dragsrc_osw,sel,GDK_CURRENT_TIME,TRUE);
        }
        swell_dragsrc_timeout = GetTickCount() + 500;
        return 0;
      }
      ReleaseCapture();
    break;
    case WM_USER+100:
    if (wParam && lParam) 
    {
      GdkAtom *aOut = (GdkAtom *)lParam;
      GdkEventSelection *evt = (GdkEventSelection*)wParam;

      if (evt->target == urilistatom())
      {
        WDL_FastString s;
        if (inf->srclist && inf->srccount)
        {
          for (int x=0;x<inf->srccount;x++)
          {
            if (x) s.Append("\n");
            s.Append("file://");
            encode_uri(&s,inf->srclist[x]);
          }
        }
        else if (inf->callback && inf->srcfn && inf->state)
        {
          inf->callback(inf->srcfn);
          s.Append("file://");
          encode_uri(&s,inf->srcfn);
        }

        if (s.GetLength())
        {
          *aOut = evt->property;
#if SWELL_TARGET_GDK == 2
          GdkWindow *pw = gdk_window_lookup(evt->requestor);
          if (!pw) pw = gdk_window_foreign_new(evt->requestor);
#else
          GdkWindow *pw = evt->requestor;
#endif
          if (pw)
            gdk_property_change(pw,*aOut,evt->target,8, GDK_PROP_MODE_REPLACE,(guchar*)s.Get(),s.GetLength());
        }
      }
       
      if (inf->state) ReleaseCapture();
    }
    break;

  }
  return DefWindowProc(hwnd,msg,wParam,lParam);
}


void SWELL_InitiateDragDrop(HWND hwnd, RECT* srcrect, const char* srcfn, void (*callback)(const char* dropfn))
{
  dropSourceInfo info;
  info.srcfn = strdup(srcfn);
  info.callback = callback;
  RECT r={0,};
  HWND__ *h = new HWND__(NULL,0,&r,NULL,false,NULL,dropSourceWndProc, NULL);
  swell_dragsrc_timeout = 0;
  swell_dragsrc_hwnd=h;
  h->m_private_data = (INT_PTR) &info;
  dropSourceWndProc(h,WM_CREATE,0,0);
  while (GetCapture()==h)
  {
    SWELL_RunEvents();
    Sleep(10);
    if (swell_dragsrc_timeout && GetTickCount()>swell_dragsrc_timeout) ReleaseCapture();
  }
  
  swell_dragsrc_hwnd=NULL;
  DestroyWindow(h);
}

// owner owns srclist, make copies here etc
void SWELL_InitiateDragDropOfFileList(HWND hwnd, RECT *srcrect, const char **srclist, int srccount, HICON icon)
{
  dropSourceInfo info;
  info.srclist = srclist;
  info.srccount = srccount;
  RECT r={0,};
  HWND__ *h = new HWND__(NULL,0,&r,NULL,false,NULL,dropSourceWndProc, NULL);
  swell_dragsrc_timeout = 0;
  swell_dragsrc_hwnd=h;
  h->m_private_data = (INT_PTR) &info;
  dropSourceWndProc(h,WM_CREATE,0,0);
  while (GetCapture()==h)
  {
    SWELL_RunEvents();
    Sleep(10);
    if (swell_dragsrc_timeout && GetTickCount()>swell_dragsrc_timeout) ReleaseCapture();
  }
  
  swell_dragsrc_hwnd=NULL;
  DestroyWindow(h);
}

void SWELL_FinishDragDrop() { }


bool SWELL_IsCursorVisible()
{
  return s_cursor_vis_cnt>=0;
}


void SWELL_SetCursor(HCURSOR curs)
{
  if (s_last_setcursor == curs && SWELL_focused_oswindow == s_last_setcursor_oswnd) return;

  s_last_setcursor=curs;
  s_last_setcursor_oswnd = SWELL_focused_oswindow;
  if (SWELL_focused_oswindow)
  {
    gdk_window_set_cursor(SWELL_focused_oswindow,(GdkCursor *)curs);
#ifdef SWELL_TARGET_GDK_CURSORHACK
    if (GetCapture())
    {
      // workaround for a GDK behavior:
      // gdkwindow.c, gdk_window_set_cursor_internal() has a line:
      // >>> if (_gdk_window_event_parent_of (window, pointer_info->window_under_pointer))
      // this should also allow setting the cursor if window is in a "grabbing" state
      GdkDisplay *gdkdisp = gdk_display_get_default();
#if SWELL_TARGET_GDK == 2
      if (gdkdisp && gdk_display_get_window_at_pointer(gdkdisp,NULL,NULL) != SWELL_focused_oswindow)
#else
      GdkDevice *dev = gdk_device_manager_get_client_pointer(gdk_display_get_device_manager(gdkdisp));
      if (dev && gdk_device_get_window_at_position(dev,NULL,NULL) != SWELL_focused_oswindow)
#endif
      {
        Display *disp = gdk_x11_display_get_xdisplay(gdkdisp);
        Window wn =  GDK_WINDOW_XID(SWELL_focused_oswindow);
#if SWELL_TARGET_GDK == 2
        gint devid=2; // hardcoded default pointing device
#else
        gint devid = gdk_x11_device_get_id(dev);
#endif
        if (disp && wn)
        {
          if (curs)
            XIDefineCursor(disp,devid,wn, gdk_x11_cursor_get_xcursor((GdkCursor*)curs));
          else
            XIUndefineCursor(disp,devid,wn);
        }
      }
    }
#endif // SWELL_TARGET_GDK_CURSORHACK
  }
}

HCURSOR SWELL_GetCursor()
{
  return s_last_setcursor;
}
HCURSOR SWELL_GetLastSetCursor()
{
  return s_last_setcursor;
}

int SWELL_ShowCursor(BOOL bShow)
{
  s_cursor_vis_cnt += (bShow?1:-1);
  if (s_cursor_vis_cnt==-1 && !bShow) 
  {
    gint x1, y1;
    #if SWELL_TARGET_GDK == 3
    GdkDevice *dev = gdk_device_manager_get_client_pointer (gdk_display_get_device_manager (gdk_display_get_default ()));
    gdk_device_get_position (dev, NULL, &x1, &y1);
    #else
    gdk_display_get_pointer(gdk_display_get_default(), NULL, &x1, &y1, NULL);
    #endif
    g_swell_mouse_relmode_curpos_x = x1;
    g_swell_mouse_relmode_curpos_y = y1;
    s_last_cursor = GetCursor();
    SetCursor((HCURSOR)gdk_cursor_new_for_display(gdk_display_get_default(),GDK_BLANK_CURSOR));
    //g_swell_mouse_relmode=true;
  }
  if (s_cursor_vis_cnt==0 && bShow) 
  {
    SetCursor(s_last_cursor);
    g_swell_mouse_relmode=false;
    if (!g_swell_touchptr)
    {
      #if SWELL_TARGET_GDK == 3
      gdk_device_warp(gdk_device_manager_get_client_pointer(gdk_display_get_device_manager(gdk_display_get_default())),
                     gdk_screen_get_default(),
                     g_swell_mouse_relmode_curpos_x, g_swell_mouse_relmode_curpos_y);
      #else
      gdk_display_warp_pointer(gdk_display_get_default(),gdk_screen_get_default(), g_swell_mouse_relmode_curpos_x, g_swell_mouse_relmode_curpos_y);
      #endif
    }
  }
  return s_cursor_vis_cnt;
}

BOOL SWELL_SetCursorPos(int X, int Y)
{  
  if (g_swell_mouse_relmode || g_swell_touchptr) return false;
 
  #if SWELL_TARGET_GDK == 3
  gdk_device_warp(gdk_device_manager_get_client_pointer(gdk_display_get_device_manager(gdk_display_get_default())),
                     gdk_screen_get_default(),
                     X, Y);
  #else
  gdk_display_warp_pointer(gdk_display_get_default(),gdk_screen_get_default(), X, Y);
  #endif
  return true;
}

static void getHotSpotForFile(const char *fn, POINT *pt)
{
  FILE *fp = fopen(fn,"rb");
  if (!fp) return;
  unsigned char buf[32];
  if (fread(buf,1,6,fp)==6 && !buf[0] && !buf[1] && buf[2] == 2 && buf[3] == 0 && buf[4] == 1 && buf[5] == 0)
  {
    fread(buf,1,16,fp);
    pt->x = buf[4]|(buf[5]<<8);
    pt->y = buf[6]|(buf[7]<<8);
  }
  fclose(fp);
}

HCURSOR SWELL_LoadCursorFromFile(const char *fn)
{
  GdkPixbuf *pb = gdk_pixbuf_new_from_file(fn,NULL);
  if (pb) 
  {
    POINT hs = {0,};
    getHotSpotForFile(fn,&hs);
    GdkCursor *curs = gdk_cursor_new_from_pixbuf(gdk_display_get_default(),pb,hs.x,hs.y);
    g_object_unref(pb);
    return (HCURSOR) curs;
  }
  return NULL;
}

HCURSOR SWELL_LoadCursor(const char *_idx)
{
  GdkCursorType def = GDK_LEFT_PTR;
  if (_idx == IDC_NO) def = GDK_PIRATE;
  else if (_idx == IDC_SIZENWSE) def = GDK_BOTTOM_LEFT_CORNER;
  else if (_idx == IDC_SIZENESW) def = GDK_BOTTOM_RIGHT_CORNER;
  else if (_idx == IDC_SIZEALL) def = GDK_FLEUR;
  else if (_idx == IDC_SIZEWE) def =  GDK_RIGHT_SIDE;
  else if (_idx == IDC_SIZENS) def = GDK_TOP_SIDE;
  else if (_idx == IDC_ARROW) def = GDK_LEFT_PTR;
  else if (_idx == IDC_HAND) def = GDK_HAND1;
  else if (_idx == IDC_UPARROW) def = GDK_CENTER_PTR;
  else if (_idx == IDC_IBEAM) def = GDK_XTERM;
  else 
  {
    SWELL_CursorResourceIndex *p = SWELL_curmodule_cursorresource_head;
    while (p)
    {
      if (p->resid == _idx)
      {
        if (p->cachedCursor) return p->cachedCursor;
        // todo: load from p->resname, into p->cachedCursor, p->hotspot
        char buf[1024];
        GetModuleFileName(NULL,buf,sizeof(buf));
        WDL_remove_filepart(buf);
        snprintf_append(buf,sizeof(buf),"/Resources/%s.cur",p->resname);
        GdkPixbuf *pb = gdk_pixbuf_new_from_file(buf,NULL);
        if (pb) 
        {
          getHotSpotForFile(buf,&p->hotspot);
          GdkCursor *curs = gdk_cursor_new_from_pixbuf(gdk_display_get_default(),pb,p->hotspot.x,p->hotspot.y);
          return (p->cachedCursor = (HCURSOR) curs);
        }
      }
      p=p->_next;
    }
  }

  HCURSOR hc= (HCURSOR)gdk_cursor_new_for_display(gdk_display_get_default(),def);

  return hc;
}

void SWELL_Register_Cursor_Resource(const char *idx, const char *name, int hotspot_x, int hotspot_y)
{
  SWELL_CursorResourceIndex *ri = (SWELL_CursorResourceIndex*)malloc(sizeof(SWELL_CursorResourceIndex));
  ri->hotspot.x = hotspot_x;
  ri->hotspot.y = hotspot_y;
  ri->resname=name;
  ri->cachedCursor=0;
  ri->resid = idx;
  ri->_next = SWELL_curmodule_cursorresource_head;
  SWELL_curmodule_cursorresource_head = ri;
}

int SWELL_KeyToASCII(int wParam, int lParam, int *newflags)
{
  return 0;
}

void swell_scaling_init(bool no_auto_hidpi)
{
  #if SWELL_TARGET_GDK == 3

  if (!no_auto_hidpi && g_swell_ui_scale == 256)
  {
    int (*gsf)(void*);
    void * (*gpm)(GdkDisplay *);
    *(void **)&gsf = dlsym(RTLD_DEFAULT,"gdk_monitor_get_scale_factor");
    *(void **)&gpm = dlsym(RTLD_DEFAULT,"gdk_display_get_primary_monitor");

    if (gpm && gsf)
    {
      GdkDisplay *gdkdisp = gdk_display_get_default();
      if (gdkdisp)
      {
        void *m = gpm(gdkdisp);
        if (m)
        {
          int sf = gsf(m);
          if (sf > 1 && sf < 8)
            g_swell_ui_scale = sf*256;
        }
      }
    }
  }

  if (g_swell_ui_scale != 256)
  {
    GdkDisplay *gdkdisp = gdk_display_get_default();
    if (gdkdisp)
    {
      void (*p)(GdkDisplay*, gint);
      *(void **)&p = dlsym(RTLD_DEFAULT,"gdk_x11_display_set_window_scale");
      if (p) p(gdkdisp,1);
    }
  }
  #endif
}


#endif
#endif
