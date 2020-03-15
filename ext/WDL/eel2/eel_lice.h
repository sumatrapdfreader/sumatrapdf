#ifndef _EEL_LICE_H_
#define _EEL_LICE_H_

// #define EEL_LICE_GET_FILENAME_FOR_STRING(idx, fs, p) (((sInst*)opaque)->GetFilenameForParameter(idx,fs,p))
// #define EEL_LICE_GET_CONTEXT(opaque) (((opaque) ? (((sInst *)opaque)->m_gfx_state) : NULL)


void eel_lice_register();

#ifdef DYNAMIC_LICE
  #define LICE_IBitmap LICE_IBitmap_disabledAPI
    #include "../lice/lice.h"

  #undef LICE_IBitmap
  typedef void LICE_IBitmap; // prevent us from using LICE api directly, in case it ever changes

class LICE_IFont;

#ifdef EEL_LICE_API_ONLY
#define EEL_LICE_FUNCDEF extern
#else
#define EEL_LICE_FUNCDEF
#endif

#define LICE_FUNCTION_VALID(x) (x)

EEL_LICE_FUNCDEF LICE_IBitmap *(*__LICE_CreateBitmap)(int, int, int);
EEL_LICE_FUNCDEF void (*__LICE_Clear)(LICE_IBitmap *dest, LICE_pixel color);
EEL_LICE_FUNCDEF void (*__LICE_Line)(LICE_IBitmap *dest, int x1, int y1, int x2, int y2, LICE_pixel color, float alpha, int mode, bool aa);
EEL_LICE_FUNCDEF bool (*__LICE_ClipLine)(int* pX1, int* pY1, int* pX2, int* pY2, int xLo, int yLo, int xHi, int yHi);
EEL_LICE_FUNCDEF void (*__LICE_DrawText)(LICE_IBitmap *bm, int x, int y, const char *string, 
                   LICE_pixel color, float alpha, int mode);
EEL_LICE_FUNCDEF void (*__LICE_DrawChar)(LICE_IBitmap *bm, int x, int y, char c, 
                   LICE_pixel color, float alpha, int mode);
EEL_LICE_FUNCDEF void (*__LICE_MeasureText)(const char *string, int *w, int *h);
EEL_LICE_FUNCDEF void (*__LICE_PutPixel)(LICE_IBitmap *bm, int x, int y, LICE_pixel color, float alpha, int mode);
EEL_LICE_FUNCDEF LICE_pixel (*__LICE_GetPixel)(LICE_IBitmap *bm, int x, int y);
EEL_LICE_FUNCDEF void (*__LICE_FillRect)(LICE_IBitmap *dest, int x, int y, int w, int h, LICE_pixel color, float alpha, int mode);
EEL_LICE_FUNCDEF void (*__LICE_DrawRect)(LICE_IBitmap *dest, int x, int y, int w, int h, LICE_pixel color, float alpha, int mode);
EEL_LICE_FUNCDEF LICE_IBitmap *(*__LICE_LoadImage)(const char* filename, LICE_IBitmap* bmp, bool tryIgnoreExtension);
EEL_LICE_FUNCDEF void (*__LICE_Blur)(LICE_IBitmap *dest, LICE_IBitmap *src, int dstx, int dsty, int srcx, int srcy, int srcw, int srch); // src and dest can overlap, however it may look fudgy if they do
EEL_LICE_FUNCDEF void (*__LICE_ScaledBlit)(LICE_IBitmap *dest, LICE_IBitmap *src, int dstx, int dsty, int dstw, int dsth, 
                     float srcx, float srcy, float srcw, float srch, float alpha, int mode);
EEL_LICE_FUNCDEF void (*__LICE_Circle)(LICE_IBitmap* dest, float cx, float cy, float r, LICE_pixel color, float alpha, int mode, bool aa);
EEL_LICE_FUNCDEF void (*__LICE_FillCircle)(LICE_IBitmap* dest, float cx, float cy, float r, LICE_pixel color, float alpha, int mode, bool aa);
EEL_LICE_FUNCDEF void (*__LICE_FillTriangle)(LICE_IBitmap* dest, int x1, int y1, int x2, int y2, int x3, int y3, LICE_pixel color, float alpha, int mode);
EEL_LICE_FUNCDEF void (*__LICE_FillConvexPolygon)(LICE_IBitmap* dest, const int* x, const int* y, int npoints, LICE_pixel color, float alpha, int mode);
EEL_LICE_FUNCDEF void (*__LICE_RoundRect)(LICE_IBitmap *drawbm, float xpos, float ypos, float w, float h, int cornerradius, LICE_pixel col, float alpha, int mode, bool aa);
EEL_LICE_FUNCDEF void (*__LICE_Arc)(LICE_IBitmap* dest, float cx, float cy, float r, float minAngle, float maxAngle, LICE_pixel color, float alpha, int mode, bool aa);

// if cliptosourcerect is false, then areas outside the source rect can get in (otherwise they are not drawn)
EEL_LICE_FUNCDEF void (*__LICE_RotatedBlit)(LICE_IBitmap *dest, LICE_IBitmap *src, 
                      int dstx, int dsty, int dstw, int dsth, 
                      float srcx, float srcy, float srcw, float srch, 
                      float angle, 
                      bool cliptosourcerect, float alpha, int mode,
                      float rotxcent, float rotycent); // these coordinates are offset from the center of the image, in source pixel coordinates


EEL_LICE_FUNCDEF void (*__LICE_MultiplyAddRect)(LICE_IBitmap *dest, int x, int y, int w, int h, 
                          float rsc, float gsc, float bsc, float asc, // 0-1, or -100 .. +100 if you really are insane
                          float radd, float gadd, float badd, float aadd); // 0-255 is the normal range on these.. of course its clamped

EEL_LICE_FUNCDEF void (*__LICE_GradRect)(LICE_IBitmap *dest, int dstx, int dsty, int dstw, int dsth, 
                      float ir, float ig, float ib, float ia,
                      float drdx, float dgdx, float dbdx, float dadx,
                      float drdy, float dgdy, float dbdy, float dady,
                      int mode);

EEL_LICE_FUNCDEF void (*__LICE_TransformBlit2)(LICE_IBitmap *dest, LICE_IBitmap *src,  
                    int dstx, int dsty, int dstw, int dsth,
                    double *srcpoints, int div_w, int div_h, // srcpoints coords should be div_w*div_h*2 long, and be in source image coordinates
                    float alpha, int mode);

EEL_LICE_FUNCDEF void (*__LICE_DeltaBlit)(LICE_IBitmap *dest, LICE_IBitmap *src, 
                    int dstx, int dsty, int dstw, int dsth,                     
                    float srcx, float srcy, float srcw, float srch, 
                    double dsdx, double dtdx, double dsdy, double dtdy,         
                    double dsdxdy, double dtdxdy,
                    bool cliptosourcerect, float alpha, int mode);


#define LICE_Blur __LICE_Blur
#define LICE_Clear __LICE_Clear
#define LICE_Line __LICE_Line
#define LICE_ClipLine __LICE_ClipLine
#define LICE_FillRect __LICE_FillRect
#define LICE_DrawRect __LICE_DrawRect
#define LICE_PutPixel __LICE_PutPixel
#define LICE_GetPixel __LICE_GetPixel
#define LICE_DrawText __LICE_DrawText
#define LICE_DrawChar __LICE_DrawChar
#define LICE_MeasureText __LICE_MeasureText
#define LICE_LoadImage __LICE_LoadImage
#define LICE_RotatedBlit __LICE_RotatedBlit
#define LICE_ScaledBlit __LICE_ScaledBlit
#define LICE_MultiplyAddRect __LICE_MultiplyAddRect
#define LICE_GradRect __LICE_GradRect
#define LICE_TransformBlit2 __LICE_TransformBlit2
#define LICE_DeltaBlit __LICE_DeltaBlit
#define LICE_Circle __LICE_Circle
#define LICE_FillCircle __LICE_FillCircle
#define LICE_FillTriangle __LICE_FillTriangle
#define LICE_FillConvexPolygon __LICE_FillConvexPolygon
#define LICE_RoundRect __LICE_RoundRect
#define LICE_Arc __LICE_Arc

EEL_LICE_FUNCDEF HDC (*LICE__GetDC)(LICE_IBitmap *bm);
EEL_LICE_FUNCDEF int (*LICE__GetWidth)(LICE_IBitmap *bm);
EEL_LICE_FUNCDEF int (*LICE__GetHeight)(LICE_IBitmap *bm);
EEL_LICE_FUNCDEF void (*LICE__Destroy)(LICE_IBitmap *bm);
EEL_LICE_FUNCDEF bool (*LICE__resize)(LICE_IBitmap *bm, int w, int h);

EEL_LICE_FUNCDEF void (*LICE__DestroyFont)(LICE_IFont* font);
EEL_LICE_FUNCDEF LICE_IFont *(*LICE_CreateFont)();
EEL_LICE_FUNCDEF void (*LICE__SetFromHFont)(LICE_IFont* ifont, HFONT font, int flags);
EEL_LICE_FUNCDEF LICE_pixel (*LICE__SetTextColor)(LICE_IFont* ifont, LICE_pixel color);
EEL_LICE_FUNCDEF void (*LICE__SetTextCombineMode)(LICE_IFont* ifont, int mode, float alpha);
EEL_LICE_FUNCDEF int (*LICE__DrawText)(LICE_IFont* ifont, LICE_IBitmap *bm, const char *str, int strcnt, RECT *rect, UINT dtFlags);

#else

#include "../lice/lice.h"
#include "../lice/lice_text.h"

#define LICE_FUNCTION_VALID(x) (sizeof(int) > 0)

static HDC LICE__GetDC(LICE_IBitmap *bm)
{
  return bm->getDC();
}
static int LICE__GetWidth(LICE_IBitmap *bm)
{
  return bm->getWidth();
}
static int LICE__GetHeight(LICE_IBitmap *bm)
{
  return bm->getHeight();
}
static void LICE__Destroy(LICE_IBitmap *bm)
{
  delete bm;
}
static void LICE__SetFromHFont(LICE_IFont * ifont, HFONT font, int flags)
{
  if (ifont) ifont->SetFromHFont(font,flags);
}
static LICE_pixel LICE__SetTextColor(LICE_IFont* ifont, LICE_pixel color)
{
  if (ifont) return ifont->SetTextColor(color);
  return 0;
}
static void LICE__SetTextCombineMode(LICE_IFont* ifont, int mode, float alpha)
{
  if (ifont) ifont->SetCombineMode(mode, alpha);
}
static int LICE__DrawText(LICE_IFont* ifont, LICE_IBitmap *bm, const char *str, int strcnt, RECT *rect, UINT dtFlags)
{
  if (ifont) return ifont->DrawText(bm, str, strcnt, rect, dtFlags);
  return 0;
}


static LICE_IFont *LICE_CreateFont()
{
  return new LICE_CachedFont();
}
static void LICE__DestroyFont(LICE_IFont *bm)
{
  delete bm;
}
static bool LICE__resize(LICE_IBitmap *bm, int w, int h)
{
  return bm->resize(w,h);
}

static LICE_IBitmap *__LICE_CreateBitmap(int mode, int w, int h)
{
  if (mode==1) return new LICE_SysBitmap(w,h);
  return new LICE_MemBitmap(w,h);
}


#endif

#include "../wdlutf8.h"


class eel_lice_state
{
public:

  eel_lice_state(NSEEL_VMCTX vm, void *ctx, int image_slots, int font_slots);
  ~eel_lice_state();

  void resetVarsToStock()
  {
    if (m_gfx_a&&m_gfx_r&&m_gfx_g&&m_gfx_b) *m_gfx_r=*m_gfx_g=*m_gfx_b=*m_gfx_a=1.0;
    if (m_gfx_a2) *m_gfx_a2=1.0;
    if (m_gfx_dest) *m_gfx_dest=-1.0;
    if (m_mouse_wheel) *m_mouse_wheel=0.0;
    if (m_mouse_hwheel) *m_mouse_hwheel=0.0;
    // todo: reset others?
  }

  LICE_IBitmap *m_framebuffer, *m_framebuffer_extra;
  int m_framebuffer_dirty;
  WDL_TypedBuf<LICE_IBitmap *> m_gfx_images;
  struct gfxFontStruct {
    LICE_IFont *font;
    char last_fontname[128];
    char actual_fontname[128];
    int last_fontsize;
    int last_fontflag;

    int use_fonth;
  }; 
  WDL_TypedBuf<gfxFontStruct> m_gfx_fonts;
  int m_gfx_font_active; // -1 for default, otherwise index into gfx_fonts (NOTE: this differs from the exposed API, which defines 0 as default, 1-n)
  LICE_IFont *GetActiveFont() { return m_gfx_font_active>=0&&m_gfx_font_active<m_gfx_fonts.GetSize() && m_gfx_fonts.Get()[m_gfx_font_active].use_fonth ? m_gfx_fonts.Get()[m_gfx_font_active].font : NULL; }

  LICE_IBitmap *GetImageForIndex(EEL_F idx, const char *callername) 
  { 
    if (idx>-2.0)
    {
      if (idx < 0.0) return m_framebuffer;

      const int a = (int)idx;
      if (a >= 0 && a < m_gfx_images.GetSize()) return m_gfx_images.Get()[a];
    }
    return NULL;
  };

  void SetImageDirty(LICE_IBitmap *bm)
  {
    if (bm == m_framebuffer && !m_framebuffer_dirty)
    {
      if (m_gfx_clear && *m_gfx_clear > -1.0)
      {
        const int a=(int)*m_gfx_clear;
        if (LICE_FUNCTION_VALID(LICE_Clear)) LICE_Clear(m_framebuffer,LICE_RGBA((a&0xff),((a>>8)&0xff),((a>>16)&0xff),0));
      }
      m_framebuffer_dirty=1;
    }
  }

  // R, G, B, A, w, h, x, y, mode(1=add,0=copy)
  EEL_F *m_gfx_r, *m_gfx_g, *m_gfx_b, *m_gfx_w, *m_gfx_h, *m_gfx_a, *m_gfx_x, *m_gfx_y, *m_gfx_mode, *m_gfx_clear, *m_gfx_texth,*m_gfx_dest, *m_gfx_a2;
  EEL_F *m_mouse_x, *m_mouse_y, *m_mouse_cap, *m_mouse_wheel, *m_mouse_hwheel;
  EEL_F *m_gfx_ext_retina;

  NSEEL_VMCTX m_vmref;
  void *m_user_ctx;

  int setup_frame(HWND hwnd, RECT r, int _mouse_x=0, int _mouse_y=0, int has_dpi=0); // mouse_x/y used only if hwnd is NULL
  void finish_draw();

  void gfx_lineto(EEL_F xpos, EEL_F ypos, EEL_F aaflag);
  void gfx_rectto(EEL_F xpos, EEL_F ypos);
  void gfx_line(int np, EEL_F **parms);
  void gfx_rect(int np, EEL_F **parms);
  void gfx_roundrect(int np, EEL_F **parms);
  void gfx_arc(int np, EEL_F **parms);
  void gfx_set(int np, EEL_F **parms);
  void gfx_grad_or_muladd_rect(int mode, int np, EEL_F **parms);
  void gfx_setpixel(EEL_F r, EEL_F g, EEL_F b);
  void gfx_getpixel(EEL_F *r, EEL_F *g, EEL_F *b);
  void gfx_drawnumber(EEL_F n, EEL_F ndigits);
  void gfx_drawchar(EEL_F ch);
  void gfx_getimgdim(EEL_F img, EEL_F *w, EEL_F *h);
  EEL_F gfx_setimgdim(int img, EEL_F *w, EEL_F *h);
  void gfx_blurto(EEL_F x, EEL_F y);
  void gfx_blit(EEL_F img, EEL_F scale, EEL_F rotate);
  void gfx_blitext(EEL_F img, EEL_F *coords, EEL_F angle);
  void gfx_blitext2(int np, EEL_F **parms, int mode); // 0=blit, 1=deltablit
  void gfx_transformblit(EEL_F **parms, int div_w, int div_h, EEL_F *tab); // parms[0]=src, 1-4=x,y,w,h
  void gfx_circle(float x, float y, float r, bool fill, bool aaflag);
  void gfx_triangle(EEL_F** parms, int nparms);  
  void gfx_drawstr(void *opaque, EEL_F **parms, int nparms, int formatmode); // formatmode=1 for format, 2 for purely measure no format, 3 for measure char
  EEL_F gfx_loadimg(void *opaque, int img, EEL_F loadFrom);
  EEL_F gfx_setfont(void *opaque, int np, EEL_F **parms);
  EEL_F gfx_getfont(void *opaque, int np, EEL_F **parms);
  EEL_F gfx_getdropfile(void *opaque, int np, EEL_F **parms);

  LICE_pixel getCurColor();
  int getCurMode();
  int getCurModeForBlit(bool isFBsrc);

#ifdef EEL_LICE_WANT_STANDALONE
  HWND create_wnd(HWND par, int isChild);
  HWND hwnd_standalone;
  int hwnd_standalone_kb_state[32]; // pressed keys, if any

  // these have to be **parms because of the hack for getting string from parm index
  EEL_F gfx_showmenu(void* opaque, EEL_F** parms, int nparms);
  EEL_F gfx_setcursor(void* opaque, EEL_F** parms, int nparms);

  int m_kb_queue[64];
  unsigned char m_kb_queue_valid;
  unsigned char m_kb_queue_pos;
  int m_cursor_resid;
#ifdef EEL_LICE_LOADTHEMECURSOR
  char m_cursor_name[128];
#endif

#ifndef EEL_LICE_STANDALONE_NOINITQUIT
  RECT m_last_undocked_r;
#endif

#endif
  int m_has_cap; // high 16 bits are current capture state, low 16 bits are temporary flags from mousedown
  bool m_has_had_getch; // set on first gfx_getchar(), makes mouse_cap updated with modifiers even when no mouse click is down

  WDL_PtrList<char> m_ddrop_files;
};


#ifndef EEL_LICE_API_ONLY

eel_lice_state::eel_lice_state(NSEEL_VMCTX vm, void *ctx, int image_slots, int font_slots)
{
#ifdef EEL_LICE_WANT_STANDALONE
  hwnd_standalone=NULL;
  memset(hwnd_standalone_kb_state,0,sizeof(hwnd_standalone_kb_state));
  m_kb_queue_valid=0;
  m_cursor_resid=0;
#ifndef EEL_LICE_STANDALONE_NOINITQUIT
  memset(&m_last_undocked_r,0,sizeof(m_last_undocked_r));
#endif

#ifdef EEL_LICE_LOADTHEMECURSOR
  m_cursor_name[0]=0;
#endif
#endif
  m_user_ctx=ctx;
  m_vmref= vm;
  m_gfx_font_active=-1;
  m_gfx_fonts.Resize(font_slots);
  memset(m_gfx_fonts.Get(),0,m_gfx_fonts.GetSize()*sizeof(m_gfx_fonts.Get()[0]));

  m_gfx_images.Resize(image_slots);
  memset(m_gfx_images.Get(),0,m_gfx_images.GetSize()*sizeof(m_gfx_images.Get()[0]));
  m_framebuffer=m_framebuffer_extra=0;
  m_framebuffer_dirty=0;

  m_gfx_r = NSEEL_VM_regvar(vm,"gfx_r");
  m_gfx_g = NSEEL_VM_regvar(vm,"gfx_g");
  m_gfx_b = NSEEL_VM_regvar(vm,"gfx_b");
  m_gfx_a = NSEEL_VM_regvar(vm,"gfx_a");
  m_gfx_a2 = NSEEL_VM_regvar(vm,"gfx_a2");

  m_gfx_w = NSEEL_VM_regvar(vm,"gfx_w");
  m_gfx_h = NSEEL_VM_regvar(vm,"gfx_h");
  m_gfx_x = NSEEL_VM_regvar(vm,"gfx_x");
  m_gfx_y = NSEEL_VM_regvar(vm,"gfx_y");
  m_gfx_mode = NSEEL_VM_regvar(vm,"gfx_mode");
  m_gfx_clear = NSEEL_VM_regvar(vm,"gfx_clear");
  m_gfx_texth = NSEEL_VM_regvar(vm,"gfx_texth");
  m_gfx_dest = NSEEL_VM_regvar(vm,"gfx_dest");
  m_gfx_ext_retina = NSEEL_VM_regvar(vm,"gfx_ext_retina");

  m_mouse_x = NSEEL_VM_regvar(vm,"mouse_x");
  m_mouse_y = NSEEL_VM_regvar(vm,"mouse_y");
  m_mouse_cap = NSEEL_VM_regvar(vm,"mouse_cap");
  m_mouse_wheel=NSEEL_VM_regvar(vm,"mouse_wheel");
  m_mouse_hwheel=NSEEL_VM_regvar(vm,"mouse_hwheel");

  if (m_gfx_texth) *m_gfx_texth=8;

  m_has_cap=0;
  m_has_had_getch=false;
}
eel_lice_state::~eel_lice_state()
{
#ifdef EEL_LICE_WANT_STANDALONE
  if (hwnd_standalone) DestroyWindow(hwnd_standalone);
#endif
  if (LICE_FUNCTION_VALID(LICE__Destroy)) 
  {
    LICE__Destroy(m_framebuffer_extra);
    LICE__Destroy(m_framebuffer);
    int x;
    for (x=0;x<m_gfx_images.GetSize();x++)
    {
      LICE__Destroy(m_gfx_images.Get()[x]);
    }
  }
  if (LICE_FUNCTION_VALID(LICE__DestroyFont))
  {
    int x;
    for (x=0;x<m_gfx_fonts.GetSize();x++)
    {
      if (m_gfx_fonts.Get()[x].font) LICE__DestroyFont(m_gfx_fonts.Get()[x].font);
    }
  }
  m_ddrop_files.Empty(true,free);
}

int eel_lice_state::getCurMode()
{
  const int gmode = (int) (*m_gfx_mode);
  const int sm=(gmode>>4)&0xf;
  if (sm > LICE_BLIT_MODE_COPY && sm <= LICE_BLIT_MODE_HSVADJ) return sm;

  return (gmode&1) ? LICE_BLIT_MODE_ADD : LICE_BLIT_MODE_COPY;
}
int eel_lice_state::getCurModeForBlit(bool isFBsrc)
{
  const int gmode = (int) (*m_gfx_mode);
 
  const int sm=(gmode>>4)&0xf;

  int mode;
  if (sm > LICE_BLIT_MODE_COPY && sm <= LICE_BLIT_MODE_HSVADJ) mode=sm;
  else mode=((gmode&1) ? LICE_BLIT_MODE_ADD : LICE_BLIT_MODE_COPY);


  if (!isFBsrc && !(gmode&2)) mode|=LICE_BLIT_USE_ALPHA;
  if (!(gmode&4)) mode|=LICE_BLIT_FILTER_BILINEAR;
 
  return mode;
}
LICE_pixel eel_lice_state::getCurColor()
{
  int red=(int) (*m_gfx_r*255.0);
  int green=(int) (*m_gfx_g*255.0);
  int blue=(int) (*m_gfx_b*255.0);
  int a2=(int) (*m_gfx_a2*255.0);
  if (red<0) red=0;else if (red>255)red=255;
  if (green<0) green=0;else if (green>255)green=255;
  if (blue<0) blue=0; else if (blue>255) blue=255;
  if (a2<0) a2=0; else if (a2>255) a2=255;
  return LICE_RGBA(red,green,blue,a2);
}


static EEL_F * NSEEL_CGEN_CALL _gfx_lineto(void *opaque, EEL_F *xpos, EEL_F *ypos, EEL_F *useaa)
{
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT(opaque);
  if (ctx) ctx->gfx_lineto(*xpos, *ypos, *useaa);
  return xpos;
}
static EEL_F * NSEEL_CGEN_CALL _gfx_lineto2(void *opaque, EEL_F *xpos, EEL_F *ypos)
{
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT(opaque);
  if (ctx) ctx->gfx_lineto(*xpos, *ypos, 1.0f);
  return xpos;
}

static EEL_F * NSEEL_CGEN_CALL _gfx_rectto(void *opaque, EEL_F *xpos, EEL_F *ypos)
{
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT(opaque);
  if (ctx) ctx->gfx_rectto(*xpos, *ypos);
  return xpos;
}

static EEL_F NSEEL_CGEN_CALL _gfx_line(void *opaque, INT_PTR np, EEL_F **parms)
{
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT(opaque);
  if (ctx) ctx->gfx_line((int)np,parms);
  return 0.0;
}

static EEL_F NSEEL_CGEN_CALL _gfx_rect(void *opaque, INT_PTR np, EEL_F **parms)
{
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT(opaque);
  if (ctx) ctx->gfx_rect((int)np,parms);
  return 0.0;
}
static EEL_F NSEEL_CGEN_CALL _gfx_roundrect(void *opaque, INT_PTR np, EEL_F **parms)
{
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT(opaque);
  if (ctx) ctx->gfx_roundrect((int)np,parms);
  return 0.0;
}
static EEL_F NSEEL_CGEN_CALL _gfx_arc(void *opaque, INT_PTR np, EEL_F **parms)
{
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT(opaque);
  if (ctx) ctx->gfx_arc((int)np,parms);
  return 0.0;
}
static EEL_F NSEEL_CGEN_CALL _gfx_set(void *opaque, INT_PTR np, EEL_F **parms)
{
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT(opaque);
  if (ctx) ctx->gfx_set((int)np,parms);
  return 0.0;
}
static EEL_F NSEEL_CGEN_CALL _gfx_gradrect(void *opaque, INT_PTR np, EEL_F **parms)
{
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT(opaque);
  if (ctx) ctx->gfx_grad_or_muladd_rect(0,(int)np,parms);
  return 0.0;
}

static EEL_F NSEEL_CGEN_CALL _gfx_muladdrect(void *opaque, INT_PTR np, EEL_F **parms)
{
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT(opaque);
  if (ctx) ctx->gfx_grad_or_muladd_rect(1,(int)np,parms);
  return 0.0;
}

static EEL_F NSEEL_CGEN_CALL _gfx_deltablit(void *opaque, INT_PTR np, EEL_F **parms)
{
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT(opaque);
  if (ctx) ctx->gfx_blitext2((int)np,parms,1);
  return 0.0;
}

static EEL_F NSEEL_CGEN_CALL _gfx_transformblit(void *opaque, INT_PTR np, EEL_F **parms)
{
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT(opaque);
  if (ctx) 
  {
#ifndef EEL_LICE_NO_RAM
    const int divw = (int) (parms[5][0]+0.5);
    const int divh = (int) (parms[6][0]+0.5);
    if (divw < 1 || divh < 1) return 0.0;
    const int sz = divw*divh*2;

#ifdef EEL_LICE_RAMFUNC
    EEL_F *d = EEL_LICE_RAMFUNC(opaque,7,sz);
    if (!d) return 0.0;
#else
    EEL_F **blocks = ctx->m_vmref  ? ((compileContext*)ctx->m_vmref)->ram_state.blocks : 0;
    if (!blocks || np < 8) return 0.0;

    const int addr1= (int) (parms[7][0]+0.5);
    EEL_F *d=__NSEEL_RAMAlloc(blocks,addr1);
    if (sz>NSEEL_RAM_ITEMSPERBLOCK)
    {
      int x;
      for(x=NSEEL_RAM_ITEMSPERBLOCK;x<sz-1;x+=NSEEL_RAM_ITEMSPERBLOCK)
        if (__NSEEL_RAMAlloc(blocks,addr1+x) != d+x) return 0.0;
    }
    EEL_F *end=__NSEEL_RAMAlloc(blocks,addr1+sz-1);
    if (end != d+sz-1) return 0.0; // buffer not contiguous
#endif

    ctx->gfx_transformblit(parms,divw,divh,d);
#endif
  }
  return 0.0;
}

static EEL_F NSEEL_CGEN_CALL _gfx_circle(void *opaque, INT_PTR np, EEL_F **parms)
{
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT(opaque);
  bool aa = true, fill = false;
  if (np>3) fill = parms[3][0] > 0.5;
  if (np>4) aa = parms[4][0] > 0.5;
  if (ctx) ctx->gfx_circle((float)parms[0][0], (float)parms[1][0], (float)parms[2][0], fill, aa);
  return 0.0;
}

static EEL_F NSEEL_CGEN_CALL _gfx_triangle(void* opaque, INT_PTR np, EEL_F **parms)
{
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT(opaque);
  if (ctx) ctx->gfx_triangle(parms, (int)np);
  return 0.0;
}

static EEL_F * NSEEL_CGEN_CALL _gfx_drawnumber(void *opaque, EEL_F *n, EEL_F *nd)
{
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT(opaque);
  if (ctx) ctx->gfx_drawnumber(*n, *nd);
  return n;
}

static EEL_F * NSEEL_CGEN_CALL _gfx_drawchar(void *opaque, EEL_F *n)
{
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT(opaque);
  if (ctx) ctx->gfx_drawchar(*n);
  return n;
}

static EEL_F * NSEEL_CGEN_CALL _gfx_measurestr(void *opaque, EEL_F *str, EEL_F *xOut, EEL_F *yOut)
{
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT(opaque);
  if (ctx) 
  {
    EEL_F *p[3]={str,xOut,yOut};
    ctx->gfx_drawstr(opaque,p,3,2);
  }
  return str;
}
static EEL_F * NSEEL_CGEN_CALL _gfx_measurechar(void *opaque, EEL_F *str, EEL_F *xOut, EEL_F *yOut)
{
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT(opaque);
  if (ctx) 
  {
    EEL_F *p[3]={str,xOut,yOut};
    ctx->gfx_drawstr(opaque,p,3,3);
  }
  return str;
}

static EEL_F NSEEL_CGEN_CALL _gfx_drawstr(void *opaque, INT_PTR nparms, EEL_F **parms)
{
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT(opaque);
  if (ctx) ctx->gfx_drawstr(opaque,parms,(int)nparms,0);
  return parms[0][0];
}

static EEL_F NSEEL_CGEN_CALL _gfx_printf(void *opaque, INT_PTR nparms, EEL_F **parms)
{
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT(opaque);
  if (ctx && nparms>0) 
  {
    EEL_F v= **parms;
    ctx->gfx_drawstr(opaque,parms,(int)nparms,1);
    return v;
  }
  return 0.0;
}

static EEL_F NSEEL_CGEN_CALL _gfx_showmenu(void* opaque, INT_PTR nparms, EEL_F **parms)
{
  eel_lice_state* ctx=EEL_LICE_GET_CONTEXT(opaque);
  if (ctx) return ctx->gfx_showmenu(opaque, parms, (int)nparms);
  return 0.0;
}

static EEL_F NSEEL_CGEN_CALL _gfx_setcursor(void* opaque,  INT_PTR nparms, EEL_F **parms)
{
  eel_lice_state* ctx=EEL_LICE_GET_CONTEXT(opaque);
  if (ctx) return ctx->gfx_setcursor(opaque, parms, (int)nparms);
  return 0.0;
}

static EEL_F * NSEEL_CGEN_CALL _gfx_setpixel(void *opaque, EEL_F *r, EEL_F *g, EEL_F *b)
{
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT(opaque);
  if (ctx) ctx->gfx_setpixel(*r, *g, *b);
  return r;
}

static EEL_F * NSEEL_CGEN_CALL _gfx_getpixel(void *opaque, EEL_F *r, EEL_F *g, EEL_F *b)
{
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT(opaque);
  if (ctx) ctx->gfx_getpixel(r, g, b);
  return r;
}

static EEL_F * NSEEL_CGEN_CALL _gfx_blit(void *opaque, EEL_F *img, EEL_F *scale, EEL_F *rotate)
{
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT(opaque);
  if (ctx) ctx->gfx_blit(*img,*scale,*rotate);
  return img;
}

static EEL_F NSEEL_CGEN_CALL _gfx_setfont(void *opaque, INT_PTR np, EEL_F **parms)
{
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT(opaque);
  if (ctx) return ctx->gfx_setfont(opaque,(int)np,parms);
  return 0.0;
}

static EEL_F NSEEL_CGEN_CALL _gfx_getfont(void *opaque, INT_PTR np, EEL_F **parms)
{
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT(opaque);
  if (ctx) 
  {
    const int idx=ctx->m_gfx_font_active;
    if (idx>=0 && idx < ctx->m_gfx_fonts.GetSize())
    {
      eel_lice_state::gfxFontStruct* f=ctx->m_gfx_fonts.Get()+idx;

      EEL_STRING_MUTEXLOCK_SCOPE
    
#ifdef NOT_EEL_STRING_UPDATE_STRING
      NOT_EEL_STRING_UPDATE_STRING(parms[0][0],f->actual_fontname);
#else
      WDL_FastString *fs=NULL;
      EEL_STRING_GET_FOR_WRITE(parms[0][0],&fs);
      if (fs) fs->Set(f->actual_fontname);
#endif
    }
    return idx;
  }
  return 0.0;
}

static EEL_F NSEEL_CGEN_CALL _gfx_blit2(void *opaque, INT_PTR np, EEL_F **parms)
{
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT(opaque);
  if (ctx && np>=3) 
  {
    ctx->gfx_blitext2((int)np,parms,0);
    return *(parms[0]);
  }
  return 0.0;
}

static EEL_F * NSEEL_CGEN_CALL _gfx_blitext(void *opaque, EEL_F *img, EEL_F *coordidx, EEL_F *rotate)
{
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT(opaque);
  if (ctx) 
  {
#ifndef EEL_LICE_NO_RAM
#ifdef EEL_LICE_RAMFUNC
    EEL_F *buf = EEL_LICE_RAMFUNC(opaque,1,10);
    if (!buf) return img;
#else
    EEL_F fc = *coordidx;
    if (fc < -0.5 || fc >= NSEEL_RAM_BLOCKS*NSEEL_RAM_ITEMSPERBLOCK) return img;
    int a=(int)fc;
    if (a<0) return img;
        
    EEL_F buf[10];
    int x;
    EEL_F **blocks = ctx->m_vmref  ? ((compileContext*)ctx->m_vmref)->ram_state.blocks : 0;
    if (!blocks) return img;
    for (x = 0;x < 10; x ++)
    {
      EEL_F *d=__NSEEL_RAMAlloc(blocks,a++);
      if (!d || d==&nseel_ramalloc_onfail) return img;
      buf[x]=*d;
    }
#endif
    // read megabuf
    ctx->gfx_blitext(*img,buf,*rotate);
#endif
  }
  return img;
}


static EEL_F * NSEEL_CGEN_CALL _gfx_blurto(void *opaque, EEL_F *x, EEL_F *y)
{
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT(opaque);
  if (ctx) ctx->gfx_blurto(*x,*y);
  return x;
}

static EEL_F * NSEEL_CGEN_CALL _gfx_getimgdim(void *opaque, EEL_F *img, EEL_F *w, EEL_F *h)
{
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT(opaque);
  if (ctx) ctx->gfx_getimgdim(*img,w,h);
  return img;
}

static EEL_F NSEEL_CGEN_CALL _gfx_loadimg(void *opaque, EEL_F *img, EEL_F *fr)
{
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT(opaque);
  if (ctx) return ctx->gfx_loadimg(opaque,(int)*img,*fr);
  return 0.0;
}

static EEL_F NSEEL_CGEN_CALL _gfx_getdropfile(void *opaque, INT_PTR np, EEL_F **parms)
{
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT(opaque);
  if (ctx) return ctx->gfx_getdropfile(opaque,(int) np, parms);
  return 0.0;
}
static EEL_F NSEEL_CGEN_CALL _gfx_setimgdim(void *opaque, EEL_F *img, EEL_F *w, EEL_F *h)
{
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT(opaque);
  if (ctx) return ctx->gfx_setimgdim((int)*img,w,h);
  return 0.0;
}




void eel_lice_state::gfx_lineto(EEL_F xpos, EEL_F ypos, EEL_F aaflag)
{
  LICE_IBitmap *dest = GetImageForIndex(*m_gfx_dest,"gfx_lineto");
  if (!dest) return;

  int x1=(int)floor(xpos),y1=(int)floor(ypos),x2=(int)floor(*m_gfx_x), y2=(int)floor(*m_gfx_y);
  if (LICE_FUNCTION_VALID(LICE__GetWidth) && LICE_FUNCTION_VALID(LICE__GetHeight) && LICE_FUNCTION_VALID(LICE_Line) && 
      LICE_FUNCTION_VALID(LICE_ClipLine) && 
      LICE_ClipLine(&x1,&y1,&x2,&y2,0,0,LICE__GetWidth(dest),LICE__GetHeight(dest))) 
  {
    SetImageDirty(dest);
    LICE_Line(dest,x1,y1,x2,y2,getCurColor(),(float) *m_gfx_a,getCurMode(),aaflag > 0.5);
  }
  *m_gfx_x = xpos;
  *m_gfx_y = ypos;
  
}

void eel_lice_state::gfx_circle(float x, float y, float r, bool fill, bool aaflag)
{
  LICE_IBitmap *dest = GetImageForIndex(*m_gfx_dest,"gfx_circle");
  if (!dest) return;

  if (LICE_FUNCTION_VALID(LICE_Circle) && LICE_FUNCTION_VALID(LICE_FillCircle))
  {
    SetImageDirty(dest);
    if(fill)
      LICE_FillCircle(dest, x, y, r, getCurColor(), (float) *m_gfx_a, getCurMode(), aaflag);
    else
      LICE_Circle(dest, x, y, r, getCurColor(), (float) *m_gfx_a, getCurMode(), aaflag);
  }
}

void eel_lice_state::gfx_triangle(EEL_F** parms, int np)
{
  LICE_IBitmap *dest = GetImageForIndex(*m_gfx_dest, "gfx_triangle");
  if (np >= 6)
  {
    np &= ~1;
    SetImageDirty(dest);
    if (np == 6)
    {        
      if (!LICE_FUNCTION_VALID(LICE_FillTriangle)) return;

      LICE_FillTriangle(dest, (int)parms[0][0], (int)parms[1][0], (int)parms[2][0], (int)parms[3][0], 
          (int)parms[4][0], (int)parms[5][0], getCurColor(), (float)*m_gfx_a, getCurMode());
    }
    else
    {
      if (!LICE_FUNCTION_VALID(LICE_FillConvexPolygon)) return;

      const int maxpt = 512;
      const int n = wdl_min(np/2, maxpt);
      int i, rdi=0;
      int x[maxpt], y[maxpt];
      for (i=0; i < n; i++)
      {
        x[i]=(int)parms[rdi++][0];
        y[i]=(int)parms[rdi++][0];
      }

      LICE_FillConvexPolygon(dest, x, y, n, getCurColor(), (float)*m_gfx_a, getCurMode());
    }
  }
}

void eel_lice_state::gfx_rectto(EEL_F xpos, EEL_F ypos)
{
  LICE_IBitmap *dest = GetImageForIndex(*m_gfx_dest,"gfx_rectto");
  if (!dest) return;

  EEL_F x1=xpos,y1=ypos,x2=*m_gfx_x, y2=*m_gfx_y;
  if (x2<x1) { x1=x2; x2=xpos; }
  if (y2<y1) { y1=y2; y2=ypos; }

  if (LICE_FUNCTION_VALID(LICE_FillRect) && x2-x1 > 0.5 && y2-y1 > 0.5)
  {
    SetImageDirty(dest);
    LICE_FillRect(dest,(int)x1,(int)y1,(int)(x2-x1),(int)(y2-y1),getCurColor(),(float)*m_gfx_a,getCurMode());
  }
  *m_gfx_x = xpos;
  *m_gfx_y = ypos;
}


void eel_lice_state::gfx_line(int np, EEL_F **parms)
{
  LICE_IBitmap *dest = GetImageForIndex(*m_gfx_dest,"gfx_line");
  if (!dest) return;

  int x1=(int)floor(parms[0][0]),y1=(int)floor(parms[1][0]),x2=(int)floor(parms[2][0]), y2=(int)floor(parms[3][0]);
  if (LICE_FUNCTION_VALID(LICE__GetWidth) && 
      LICE_FUNCTION_VALID(LICE__GetHeight) && 
      LICE_FUNCTION_VALID(LICE_Line) && 
      LICE_FUNCTION_VALID(LICE_ClipLine) && LICE_ClipLine(&x1,&y1,&x2,&y2,0,0,LICE__GetWidth(dest),LICE__GetHeight(dest))) 
  {
    SetImageDirty(dest);
    LICE_Line(dest,x1,y1,x2,y2,getCurColor(),(float)*m_gfx_a,getCurMode(),np< 5 || parms[4][0] > 0.5);
  } 
}

void eel_lice_state::gfx_rect(int np, EEL_F **parms)
{
  LICE_IBitmap *dest = GetImageForIndex(*m_gfx_dest,"gfx_rect");
  if (!dest) return;

  int x1=(int)floor(parms[0][0]),y1=(int)floor(parms[1][0]),w=(int)floor(parms[2][0]),h=(int)floor(parms[3][0]);  
  int filled=(np < 5 || parms[4][0] > 0.5);

  if (LICE_FUNCTION_VALID(LICE_FillRect) && LICE_FUNCTION_VALID(LICE_DrawRect) && w>0 && h>0)
  {
    SetImageDirty(dest);
    if (filled) LICE_FillRect(dest,x1,y1,w,h,getCurColor(),(float)*m_gfx_a,getCurMode());
    else LICE_DrawRect(dest, x1, y1, w-1, h-1, getCurColor(), (float)*m_gfx_a, getCurMode());
  }
}

void eel_lice_state::gfx_roundrect(int np, EEL_F **parms)
{
  LICE_IBitmap *dest = GetImageForIndex(*m_gfx_dest,"gfx_roundrect");
  if (!dest) return;

  const bool aa = np <= 5 || parms[5][0]>0.5;

  if (LICE_FUNCTION_VALID(LICE_RoundRect) && parms[2][0]>0 && parms[3][0]>0)
  {
    SetImageDirty(dest);
    LICE_RoundRect(dest, (float)parms[0][0], (float)parms[1][0], (float)parms[2][0], (float)parms[3][0], (int)parms[4][0], getCurColor(), (float)*m_gfx_a, getCurMode(), aa);
  }
}

void eel_lice_state::gfx_arc(int np, EEL_F **parms)
{
  LICE_IBitmap *dest = GetImageForIndex(*m_gfx_dest,"gfx_arc");
  if (!dest) return;

  const bool aa = np <= 5 || parms[5][0]>0.5;

  if (LICE_FUNCTION_VALID(LICE_Arc))
  {
    SetImageDirty(dest);
    LICE_Arc(dest, (float)parms[0][0], (float)parms[1][0], (float)parms[2][0], (float)parms[3][0], (float)parms[4][0], getCurColor(), (float)*m_gfx_a, getCurMode(), aa);
  }
}

void eel_lice_state::gfx_grad_or_muladd_rect(int whichmode, int np, EEL_F **parms)
{
  LICE_IBitmap *dest = GetImageForIndex(*m_gfx_dest,whichmode==0?"gfx_gradrect":"gfx_muladdrect");
  if (!dest) return;

  const int x1=(int)floor(parms[0][0]),y1=(int)floor(parms[1][0]),w=(int)floor(parms[2][0]), h=(int)floor(parms[3][0]);

  if (w>0 && h>0)
  {
    SetImageDirty(dest);
    if (whichmode==0 && LICE_FUNCTION_VALID(LICE_GradRect) && np > 7)
    {
      LICE_GradRect(dest,x1,y1,w,h,(float)parms[4][0],(float)parms[5][0],(float)parms[6][0],(float)parms[7][0],
                                   np > 8 ? (float)parms[8][0]:0.0f, np > 9 ? (float)parms[9][0]:0.0f,  np > 10 ? (float)parms[10][0]:0.0f, np > 11 ? (float)parms[11][0]:0.0f,  
                                   np > 12 ? (float)parms[12][0]:0.0f, np > 13 ? (float)parms[13][0]:0.0f,  np > 14 ? (float)parms[14][0]:0.0f, np > 15 ? (float)parms[15][0]:0.0f,  
                                   getCurMode());
    }
    else if (whichmode==1 && LICE_FUNCTION_VALID(LICE_MultiplyAddRect) && np > 6)
    {
      const double sc = 255.0;
      LICE_MultiplyAddRect(dest,x1,y1,w,h,(float)parms[4][0],(float)parms[5][0],(float)parms[6][0],np>7 ? (float)parms[7][0]:1.0f,
        (float)(np > 8 ? sc*parms[8][0]:0.0), (float)(np > 9 ? sc*parms[9][0]:0.0),  (float)(np > 10 ? sc*parms[10][0]:0.0), (float)(np > 11 ? sc*parms[11][0]:0.0));
    }
  }
}



void eel_lice_state::gfx_setpixel(EEL_F r, EEL_F g, EEL_F b)
{
  LICE_IBitmap *dest = GetImageForIndex(*m_gfx_dest,"gfx_setpixel");
  if (!dest) return;

  int red=(int) (r*255.0);
  int green=(int) (g*255.0);
  int blue=(int) (b*255.0);
  if (red<0) red=0;else if (red>255)red=255;
  if (green<0) green=0;else if (green>255)green=255;
  if (blue<0) blue=0; else if (blue>255) blue=255;

  if (LICE_FUNCTION_VALID(LICE_PutPixel)) 
  {
    SetImageDirty(dest);
    LICE_PutPixel(dest,(int)*m_gfx_x, (int)*m_gfx_y,LICE_RGBA(red,green,blue,255), (float)*m_gfx_a,getCurMode());
  }
}

void eel_lice_state::gfx_getimgdim(EEL_F img, EEL_F *w, EEL_F *h)
{
  *w=*h=0;
#ifdef DYNAMIC_LICE
  if (!LICE__GetWidth || !LICE__GetHeight) return;
#endif

  LICE_IBitmap *bm=GetImageForIndex(img,"gfx_getimgdim"); 
  if (bm)
  {
    *w=LICE__GetWidth(bm);
    *h=LICE__GetHeight(bm);
  }
}

EEL_F eel_lice_state::gfx_getdropfile(void *opaque, int np, EEL_F **parms)
{
  const int idx = (int) parms[0][0];
  if (idx<0) m_ddrop_files.Empty(true,free);
  if (idx < 0 || idx >= m_ddrop_files.GetSize()) return 0.0;

#ifdef NOT_EEL_STRING_UPDATE_STRING
  NOT_EEL_STRING_UPDATE_STRING(parms[1][0],m_ddrop_files.Get(idx));
#else
  if (np > 1) 
  {
    EEL_STRING_MUTEXLOCK_SCOPE
    WDL_FastString *fs=NULL;
    EEL_STRING_GET_FOR_WRITE(parms[1][0], &fs);
    if (fs) fs->Set(m_ddrop_files.Get(idx));
  }
#endif
  return 1.0;
}

EEL_F eel_lice_state::gfx_loadimg(void *opaque, int img, EEL_F loadFrom)
{
#ifdef DYNAMIC_LICE
  if (!__LICE_LoadImage || !LICE__Destroy) return 0.0;
#endif

  if (img >= 0 && img < m_gfx_images.GetSize()) 
  {
    WDL_FastString fs;
    bool ok = EEL_LICE_GET_FILENAME_FOR_STRING(loadFrom,&fs,0);

    if (ok && fs.GetLength())
    {
      LICE_IBitmap *bm = LICE_LoadImage(fs.Get(),NULL,false);
      if (bm)
      {
        LICE__Destroy(m_gfx_images.Get()[img]);
        m_gfx_images.Get()[img]=bm;
        return img;
      }
    }
  }
  return -1.0;

}

EEL_F eel_lice_state::gfx_setimgdim(int img, EEL_F *w, EEL_F *h)
{
  int rv=0;
#ifdef DYNAMIC_LICE
  if (!LICE__resize ||!LICE__GetWidth || !LICE__GetHeight||!__LICE_CreateBitmap) return 0.0;
#endif

  int use_w = (int)*w;
  int use_h = (int)*h;
  if (use_w<1 || use_h < 1) use_w=use_h=0;
  if (use_w > 2048) use_w=2048;
  if (use_h > 2048) use_h=2048;
  
  LICE_IBitmap *bm=NULL;
  if (img >= 0 && img < m_gfx_images.GetSize()) 
  {
    bm=m_gfx_images.Get()[img];  
    if (!bm) 
    {
      m_gfx_images.Get()[img] = bm = __LICE_CreateBitmap(1,use_w,use_h);
      rv=!!bm;
    }
    else 
    {
      rv=LICE__resize(bm,use_w,use_h);
    }
  }

  return rv?1.0:0.0;
}

void eel_lice_state::gfx_blurto(EEL_F x, EEL_F y)
{
  LICE_IBitmap *dest = GetImageForIndex(*m_gfx_dest,"gfx_blurto");
  if (!dest
#ifdef DYNAMIC_LICE
    ||!LICE_Blur
#endif
    ) return;

  SetImageDirty(dest);
  
  int srcx = (int)x;
  int srcy = (int)y;
  int srcw=(int) (*m_gfx_x-x);
  int srch=(int) (*m_gfx_y-y);
  if (srch < 0) { srch=-srch; srcy = (int)*m_gfx_y; }
  if (srcw < 0) { srcw=-srcw; srcx = (int)*m_gfx_x; }
  LICE_Blur(dest,dest,srcx,srcy,srcx,srcy,srcw,srch);
  *m_gfx_x = x;
  *m_gfx_y = y;
}

static bool CoordsSrcDestOverlap(EEL_F *coords)
{
  if (coords[0]+coords[2] < coords[4]) return false;
  if (coords[0] > coords[4] + coords[6]) return false;
  if (coords[1]+coords[3] < coords[5]) return false;
  if (coords[1] > coords[5] + coords[7]) return false;
  return true;
}

void eel_lice_state::gfx_transformblit(EEL_F **parms, int div_w, int div_h, EEL_F *tab)
{
  LICE_IBitmap *dest = GetImageForIndex(*m_gfx_dest,"gfx_transformblit");

  if (!dest
#ifdef DYNAMIC_LICE
    ||!LICE_ScaledBlit || !LICE_TransformBlit2 ||!LICE__GetWidth||!LICE__GetHeight
#endif 
    ) return;

  LICE_IBitmap *bm=GetImageForIndex(parms[0][0],"gfx_transformblit:src"); 
  if (!bm) return;

  const int bmw=LICE__GetWidth(bm);
  const int bmh=LICE__GetHeight(bm);
 
  const bool isFromFB = bm==m_framebuffer;

  SetImageDirty(dest);

  if (bm == dest)
  {
    if (!m_framebuffer_extra && LICE_FUNCTION_VALID(__LICE_CreateBitmap)) m_framebuffer_extra=__LICE_CreateBitmap(0,bmw,bmh);
    if (m_framebuffer_extra)
    {
    
      LICE__resize(bm=m_framebuffer_extra,bmw,bmh);
      LICE_ScaledBlit(bm,dest, // copy the entire image
        0,0,bmw,bmh,
        0.0f,0.0f,(float)bmw,(float)bmh,
        1.0f,LICE_BLIT_MODE_COPY);      
    }
  }
  LICE_TransformBlit2(dest,bm,(int)floor(parms[1][0]),(int)floor(parms[2][0]),(int)floor(parms[3][0]),(int)floor(parms[4][0]),tab,div_w,div_h, (float)*m_gfx_a,getCurModeForBlit(isFromFB));
}

EEL_F eel_lice_state::gfx_setfont(void *opaque, int np, EEL_F **parms)
{
  int a = np>0 ? ((int)floor(parms[0][0]))-1 : -1;

  if (a>=0 && a < m_gfx_fonts.GetSize())
  {
    gfxFontStruct *s = m_gfx_fonts.Get()+a;
    if (np>1 && LICE_FUNCTION_VALID(LICE_CreateFont) && LICE_FUNCTION_VALID(LICE__SetFromHFont))
    {
      const int sz=np>2 ? (int)parms[2][0] : 10;
      
      bool doCreate=false;
      int fontflag=0;
      if (!s->font) s->actual_fontname[0]=0;

      {
        EEL_STRING_MUTEXLOCK_SCOPE
      
        const char *face=EEL_STRING_GET_FOR_INDEX(parms[1][0],NULL);
        #ifdef EEL_STRING_DEBUGOUT
          if (!face) EEL_STRING_DEBUGOUT("gfx_setfont: invalid string identifier %f",parms[1][0]);
        #endif
        if (!face || !*face) face="Arial";

        {
          unsigned int c = np > 3 ? (unsigned int) parms[3][0] : 0;
          while (c)
          {
            if (toupper(c&0xff)=='B') fontflag|=1;
            else if (toupper(c&0xff)=='I') fontflag|=2;
            else if (toupper(c&0xff)=='U') fontflag|=4;
            else if (toupper(c&0xff)=='R') fontflag|=16; //LICE_FONT_FLAG_FX_BLUR
            else if (toupper(c&0xff)=='V') fontflag|=32;//LICE_FONT_FLAG_FX_INVERT
            else if (toupper(c&0xff)=='M') fontflag|=64;//LICE_FONT_FLAG_FX_MONO
            else if (toupper(c&0xff)=='S') fontflag|=128; //LICE_FONT_FLAG_FX_SHADOW
            else if (toupper(c&0xff)=='O') fontflag|=256; //LICE_FONT_FLAG_FX_OUTLINE
            c>>=8;
          }
        }
      

        if (fontflag != s->last_fontflag || sz!=s->last_fontsize || strncmp(s->last_fontname,face,sizeof(s->last_fontname)-1))
        {
          lstrcpyn_safe(s->last_fontname,face,sizeof(s->last_fontname));
          s->last_fontsize=sz;
          s->last_fontflag=fontflag;
          doCreate=1;
        }
      }

      if (doCreate)
      {
        s->actual_fontname[0]=0;
        if (!s->font) s->font=LICE_CreateFont();
        if (s->font)
        {
          HFONT hf=CreateFont(sz,0,0,0,(fontflag&1) ? FW_BOLD : FW_NORMAL,!!(fontflag&2),!!(fontflag&4),FALSE,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,s->last_fontname);
          if (!hf)
          {
            s->use_fonth=0; // disable this font
          }
          else
          {
            TEXTMETRIC tm;
            tm.tmHeight = sz;

            if (!m_framebuffer && LICE_FUNCTION_VALID(__LICE_CreateBitmap)) m_framebuffer=__LICE_CreateBitmap(1,64,64);

            if (m_framebuffer && LICE_FUNCTION_VALID(LICE__GetDC))
            {
              HGDIOBJ oldFont = 0;
              HDC hdc=LICE__GetDC(m_framebuffer);
              if (hdc)
              {
                oldFont = SelectObject(hdc,hf);
                GetTextMetrics(hdc,&tm);
                GetTextFace(hdc, sizeof(s->actual_fontname), s->actual_fontname);
                SelectObject(hdc,oldFont);
              }
            }

            s->use_fonth=wdl_max(tm.tmHeight,1);
            LICE__SetFromHFont(s->font,hf,512 | (fontflag&(511-15)));//LICE_FONT_FLAG_OWNS_HFONT);
          }
        }
      }
    }

    
    if (s->font && s->use_fonth)
    {
      m_gfx_font_active=a;
      if (m_gfx_texth) *m_gfx_texth=s->use_fonth;
      return 1.0;
    }
    // try to init this font
  }
  #ifdef EEL_STRING_DEBUGOUT
  if (a >= m_gfx_fonts.GetSize()) EEL_STRING_DEBUGOUT("gfx_setfont: invalid font %d specified",a);
  #endif

  if (a<0||a>=m_gfx_fonts.GetSize()||!m_gfx_fonts.Get()[a].font)
  {
    m_gfx_font_active=-1;
    if (m_gfx_texth) *m_gfx_texth=8;
    return 1.0;
  }
  return 0.0;
}

void eel_lice_state::gfx_blitext2(int np, EEL_F **parms, int blitmode)
{
  LICE_IBitmap *dest = GetImageForIndex(*m_gfx_dest,"gfx_blitext2");

  if (!dest
#ifdef DYNAMIC_LICE
    ||!LICE_ScaledBlit || !LICE_RotatedBlit||!LICE__GetWidth||!LICE__GetHeight
#endif 
    ) return;

  LICE_IBitmap *bm=GetImageForIndex(parms[0][0],"gfx_blitext2:src"); 
  if (!bm) return;

  const int bmw=LICE__GetWidth(bm);
  const int bmh=LICE__GetHeight(bm);
  
  // 0=img, 1=scale, 2=rotate
  double coords[8];
  const double sc = blitmode==0 ? parms[1][0] : 1.0,
            angle = blitmode==0 ? parms[2][0] : 0.0;
  if (blitmode==0)
  {
    parms+=2;
    np -= 2;
  }

  coords[0]=np > 1 ? parms[1][0] : 0.0f;
  coords[1]=np > 2 ? parms[2][0] : 0.0f;
  coords[2]=np > 3 ? parms[3][0] : bmw;
  coords[3]=np > 4 ? parms[4][0] : bmh;
  coords[4]=np > 5 ? parms[5][0] : *m_gfx_x;
  coords[5]=np > 6 ? parms[6][0] : *m_gfx_y;
  coords[6]=np > 7 ? parms[7][0] : coords[2]*sc;
  coords[7]=np > 8 ? parms[8][0] : coords[3]*sc;
 
  const bool isFromFB = bm == m_framebuffer;
  SetImageDirty(dest);
 
  if (bm == dest && CoordsSrcDestOverlap(coords))
  {
    if (!m_framebuffer_extra && LICE_FUNCTION_VALID(__LICE_CreateBitmap)) m_framebuffer_extra=__LICE_CreateBitmap(0,bmw,bmh);
    if (m_framebuffer_extra)
    {
    
      LICE__resize(bm=m_framebuffer_extra,bmw,bmh);
      LICE_ScaledBlit(bm,dest, // copy the source portion
        (int)coords[0],(int)coords[1],(int)coords[2],(int)coords[3],
        (float)coords[0],(float)coords[1],(float)coords[2],(float)coords[3],
        1.0f,LICE_BLIT_MODE_COPY);      
    }
  }
  
  if (blitmode==1)
  {
    if (LICE_FUNCTION_VALID(LICE_DeltaBlit))
      LICE_DeltaBlit(dest,bm,(int)coords[4],(int)coords[5],(int)coords[6],(int)coords[7],
                (float)coords[0],(float)coords[1],(float)coords[2],(float)coords[3],
                np > 9 ? (float)parms[9][0]:1.0f, // dsdx
                np > 10 ? (float)parms[10][0]:0.0f, // dtdx
                np > 11 ? (float)parms[11][0]:0.0f, // dsdy
                np > 12 ? (float)parms[12][0]:1.0f, // dtdy
                np > 13 ? (float)parms[13][0]:0.0f, // dsdxdy
                np > 14 ? (float)parms[14][0]:0.0f, // dtdxdy
                np <= 15 || parms[15][0] > 0.5, (float)*m_gfx_a,getCurModeForBlit(isFromFB));
  }
  else if (fabs(angle)>0.000000001)
  {
    LICE_RotatedBlit(dest,bm,(int)coords[4],(int)coords[5],(int)coords[6],(int)coords[7],
      (float)coords[0],(float)coords[1],(float)coords[2],(float)coords[3],
      (float)angle,true, (float)*m_gfx_a,getCurModeForBlit(isFromFB),
       np > 9 ? (float)parms[9][0] : 0.0f,
       np > 10 ? (float)parms[10][0] : 0.0f);
  }
  else
  {
    LICE_ScaledBlit(dest,bm,(int)coords[4],(int)coords[5],(int)coords[6],(int)coords[7],
      (float)coords[0],(float)coords[1],(float)coords[2],(float)coords[3], (float)*m_gfx_a,getCurModeForBlit(isFromFB));
  }
}

void eel_lice_state::gfx_blitext(EEL_F img, EEL_F *coords, EEL_F angle)
{
  LICE_IBitmap *dest = GetImageForIndex(*m_gfx_dest,"gfx_blitext");

  if (!dest
#ifdef DYNAMIC_LICE
    ||!LICE_ScaledBlit || !LICE_RotatedBlit||!LICE__GetWidth||!LICE__GetHeight
#endif 
    ) return;

  LICE_IBitmap *bm=GetImageForIndex(img,"gfx_blitext:src");
  if (!bm) return;
  
  SetImageDirty(dest);
  const bool isFromFB = bm == m_framebuffer;
 
  int bmw=LICE__GetWidth(bm);
  int bmh=LICE__GetHeight(bm);
  
  if (bm == dest && CoordsSrcDestOverlap(coords))
  {
    if (!m_framebuffer_extra && LICE_FUNCTION_VALID(__LICE_CreateBitmap)) m_framebuffer_extra=__LICE_CreateBitmap(0,bmw,bmh);
    if ( m_framebuffer_extra)
    {
    
      LICE__resize(bm=m_framebuffer_extra,bmw,bmh);
      LICE_ScaledBlit(bm,dest, // copy the source portion
        (int)coords[0],(int)coords[1],(int)coords[2],(int)coords[3],
        (float)coords[0],(float)coords[1],(float)coords[2],(float)coords[3],
        1.0f,LICE_BLIT_MODE_COPY);      
    }
  }
  
  if (fabs(angle)>0.000000001)
  {
    LICE_RotatedBlit(dest,bm,(int)coords[4],(int)coords[5],(int)coords[6],(int)coords[7],
      (float)coords[0],(float)coords[1],(float)coords[2],(float)coords[3],(float)angle,
      true, (float)*m_gfx_a,getCurModeForBlit(isFromFB),
          (float)coords[8],(float)coords[9]);
  }
  else
  {
    LICE_ScaledBlit(dest,bm,(int)coords[4],(int)coords[5],(int)coords[6],(int)coords[7],
      (float)coords[0],(float)coords[1],(float)coords[2],(float)coords[3], (float)*m_gfx_a,getCurModeForBlit(isFromFB));
  }
}

void eel_lice_state::gfx_blit(EEL_F img, EEL_F scale, EEL_F rotate)
{
  LICE_IBitmap *dest = GetImageForIndex(*m_gfx_dest,"gfx_blit");
  if (!dest
#ifdef DYNAMIC_LICE
    ||!LICE_ScaledBlit || !LICE_RotatedBlit||!LICE__GetWidth||!LICE__GetHeight
#endif
    ) return;

  LICE_IBitmap *bm=GetImageForIndex(img,"gfx_blit:src");
  
  if (!bm) return;
  
  SetImageDirty(dest);
  const bool isFromFB = bm == m_framebuffer;
  
  int bmw=LICE__GetWidth(bm);
  int bmh=LICE__GetHeight(bm);
  if (fabs(rotate)>0.000000001)
  {
    LICE_RotatedBlit(dest,bm,(int)*m_gfx_x,(int)*m_gfx_y,(int) (bmw*scale),(int) (bmh*scale),0.0f,0.0f,(float)bmw,(float)bmh,(float)rotate,true, (float)*m_gfx_a,getCurModeForBlit(isFromFB),
        0.0f,0.0f);
  }
  else
  {
    LICE_ScaledBlit(dest,bm,(int)*m_gfx_x,(int)*m_gfx_y,(int) (bmw*scale),(int) (bmh*scale),0.0f,0.0f,(float)bmw,(float)bmh, (float)*m_gfx_a,getCurModeForBlit(isFromFB));
  }
}

void eel_lice_state::gfx_set(int np, EEL_F **parms)
{
  if (np < 1) return;
  if (m_gfx_r) *m_gfx_r = parms[0][0];
  if (m_gfx_g) *m_gfx_g = np > 1 ? parms[1][0] : parms[0][0];
  if (m_gfx_b) *m_gfx_b = np > 2 ? parms[2][0] : parms[0][0];
  if (m_gfx_a) *m_gfx_a = np > 3 ? parms[3][0] : 1.0;
  if (m_gfx_mode) *m_gfx_mode = np > 4 ? parms[4][0] : 0;
  if (np > 5 && m_gfx_dest) *m_gfx_dest = parms[5][0];
  if (m_gfx_a2) *m_gfx_a2 = np > 6 ? parms[6][0] : 1.0;
}

void eel_lice_state::gfx_getpixel(EEL_F *r, EEL_F *g, EEL_F *b)
{
  LICE_IBitmap *dest = GetImageForIndex(*m_gfx_dest,"gfx_getpixel");
  if (!dest) return;

  int ret=LICE_FUNCTION_VALID(LICE_GetPixel)?LICE_GetPixel(dest,(int)*m_gfx_x, (int)*m_gfx_y):0;

  *r=LICE_GETR(ret)/255.0;
  *g=LICE_GETG(ret)/255.0;
  *b=LICE_GETB(ret)/255.0;

}


static int __drawTextWithFont(LICE_IBitmap *dest, const RECT *rect, LICE_IFont *font, const char *buf, int buflen, 
  int fg, int mode, float alpha, int flags, EEL_F *wantYoutput, EEL_F **measureOnly)
{
  if (font && LICE_FUNCTION_VALID(LICE__DrawText))
  {
    RECT tr=*rect;
    LICE__SetTextColor(font,fg);
    LICE__SetTextCombineMode(font,mode,alpha);

    int maxx=0;
    RECT r={0,0,tr.left,0};
    while (buflen>0)
    {
      int thislen = 0;
      while (thislen < buflen && buf[thislen] != '\n') thislen++;
      memset(&r,0,sizeof(r));
      int lineh = LICE__DrawText(font,dest,buf,thislen?thislen:1,&r,DT_SINGLELINE|DT_NOPREFIX|DT_CALCRECT);
      if (!measureOnly)
      {
        r.right += tr.left;
        lineh = LICE__DrawText(font,dest,buf,thislen?thislen:1,&tr,DT_SINGLELINE|DT_NOPREFIX|flags);
        if (wantYoutput) *wantYoutput = tr.top;
      }
      else
      {
        if (r.right > maxx) maxx=r.right;
      }
      tr.top += lineh;

      buflen -= thislen+1;
      buf += thislen+1;      
    }
    if (measureOnly) 
    {
      measureOnly[0][0] = maxx;
      measureOnly[1][0] = tr.top;
    }
    return r.right;
  }
  else
  { 
    int xpos=rect->left, ypos=rect->top;
    int x;
    int maxx=0,maxy=0;

    LICE_SubBitmap sbm(
#ifdef DYNAMIC_LICE
        (LICE_IBitmap_disabledAPI*)
#endif
        dest,rect->left,rect->top,rect->right-rect->left,rect->bottom-rect->top);

    if (!measureOnly)
    {
      if (!(flags & DT_NOCLIP))
      {
        if (rect->right <= rect->left || rect->bottom <= rect->top) return 0; // invalid clip rect hm

        xpos = ypos = 0;
        dest = &sbm;
      }
      if (flags & (DT_RIGHT|DT_BOTTOM|DT_CENTER|DT_VCENTER))
      {
        EEL_F w=0.0,h=0.0;
        EEL_F *mo[2] = { &w,&h};
        RECT tr={0,};
        __drawTextWithFont(dest,&tr,NULL,buf,buflen,0,0,0.0f,0,NULL,mo);

        if (flags & DT_RIGHT) xpos += (rect->right-rect->left) - (int)floor(w);
        else if (flags & DT_CENTER) xpos += (rect->right-rect->left)/2 - (int)floor(w*.5);

        if (flags & DT_BOTTOM) ypos += (rect->bottom-rect->top) - (int)floor(h);
        else if (flags & DT_VCENTER) ypos += (rect->bottom-rect->top)/2 - (int)floor(h*.5);
      }
    }
    const int sxpos = xpos;

    if (LICE_FUNCTION_VALID(LICE_DrawChar)) for(x=0;x<buflen;x++)
    {
      switch (buf[x])
      {
        case '\n': 
          ypos += 8; 
        case '\r': 
          xpos = sxpos; 
        break;
        case ' ': xpos += 8; break;
        case '\t': xpos += 8*5; break;
        default:
          if (!measureOnly) LICE_DrawChar(dest,xpos,ypos,buf[x], fg,alpha,mode);
          xpos += 8;
          if (xpos > maxx) maxx=xpos;
          maxy = ypos + 8;
        break;
      }
    }
    if (measureOnly)
    {
      measureOnly[0][0]=maxx;
      measureOnly[1][0]=maxy;
    }
    else
    {
      if (wantYoutput) *wantYoutput=ypos;
    }
    return xpos;
  }
}

static HMENU PopulateMenuFromStr(const char** str, int* startid)
{
  HMENU hm=CreatePopupMenu();
  int pos=0;
  int id=*startid;

  char buf[1024];
  const char* p=*str;
  const char* sep=strchr(p, '|');
  while (sep || *p)
  {
    int len = (int)(sep ? sep-p : strlen(p));
    int destlen=wdl_min(len, (int)sizeof(buf)-1);
    lstrcpyn(buf, p, destlen+1);
    p += len;
    if (sep) sep=strchr(++p, '|');

    const char* q=buf;
    HMENU subm=NULL;
    bool done=false;
    int flags=MF_BYPOSITION|MF_STRING;
    while (strspn(q, ">#!<"))
    {
      if (*q == '>' && !subm)
      {
        subm=PopulateMenuFromStr(&p, &id);
        sep=strchr(p, '|');
      }
      if (*q == '#') flags |= MF_GRAYED;
      if (*q == '!') flags |= MF_CHECKED;
      if (*q == '<') done=true;
      ++q;
    }
    if (subm) flags |= MF_POPUP;
    if (*q) InsertMenu(hm, pos++, flags, (subm ? (INT_PTR)subm : (INT_PTR)id++), q);
    else if (!done) InsertMenu(hm, pos++, MF_BYPOSITION|MF_SEPARATOR, 0, NULL);
    if (done) break;
  }

  *str=p;
  *startid=id;

  if (!pos) 
  { 
    DestroyMenu(hm);
    return NULL;
  }
  return hm;
}

EEL_F eel_lice_state::gfx_showmenu(void* opaque, EEL_F** parms, int nparms)
{
  const char* p=EEL_STRING_GET_FOR_INDEX(parms[0][0], NULL);
  if (!p || !p[0]) return 0.0;

  int id=1;
  HMENU hm=PopulateMenuFromStr(&p, &id);

  int ret=0;
  if (hm)
  {
    POINT pt;
    if (hwnd_standalone)
    {
#ifdef __APPLE__
      if (*m_gfx_ext_retina > 1.0) 
      { 
        pt.x = (short)(*m_gfx_x * .5);
        pt.y = (short)(*m_gfx_y * .5);
      }
      else
#endif
      {
        pt.x = (short)*m_gfx_x;
        pt.y = (short)*m_gfx_y;
      }
      ClientToScreen(hwnd_standalone, &pt);
    }
    else
      GetCursorPos(&pt);
    ret=TrackPopupMenu(hm, TPM_NONOTIFY|TPM_RETURNCMD, pt.x, pt.y, 0, hwnd_standalone, NULL);
    DestroyMenu(hm);
  }
  return (EEL_F)ret;
}

EEL_F eel_lice_state::gfx_setcursor(void* opaque, EEL_F** parms, int nparms)
{
  if (!hwnd_standalone) return 0.0;

  m_cursor_resid=(int)parms[0][0];

#ifdef EEL_LICE_LOADTHEMECURSOR
  m_cursor_name[0]=0;
  if (nparms > 1)
  {
    const char* p=EEL_STRING_GET_FOR_INDEX(parms[1][0], NULL);
    if (p && p[0]) lstrcpyn(m_cursor_name, p, sizeof(m_cursor_name));
  }
#endif
  return 1.0;
}


void eel_lice_state::gfx_drawstr(void *opaque, EEL_F **parms, int nparms, int formatmode)// formatmode=1 for format, 2 for purely measure no format
{
  int nfmtparms = nparms-1;
  EEL_F **fmtparms = parms+1;
  const char *funcname =  formatmode==1?"gfx_printf":
                          formatmode==2?"gfx_measurestr":
                          formatmode==3?"gfx_measurechar" : "gfx_drawstr";

  LICE_IBitmap *dest = GetImageForIndex(*m_gfx_dest,funcname);
  if (!dest) return;

#ifdef DYNAMIC_LICE
  if (!LICE__GetWidth || !LICE__GetHeight) return;
#endif

  EEL_STRING_MUTEXLOCK_SCOPE

  WDL_FastString *fs=NULL;
  char buf[4096];
  int s_len=0;

  const char *s;
  if (formatmode==3) 
  {
    s_len = WDL_MakeUTFChar(buf, (int)parms[0][0], sizeof(buf));
    s=buf;
  }
  else 
  {
    s=EEL_STRING_GET_FOR_INDEX(parms[0][0],&fs);
    #ifdef EEL_STRING_DEBUGOUT
      if (!s) EEL_STRING_DEBUGOUT("gfx_%s: invalid string identifier %f",funcname,parms[0][0]);
    #endif
    if (!s) 
    {
      s="<bad string>";
      s_len = 12;
    }
    else if (formatmode==1)
    {
      extern int eel_format_strings(void *, const char *s, const char *ep, char *, int, int, EEL_F **);
      s_len = eel_format_strings(opaque,s,fs?(s+fs->GetLength()):NULL,buf,sizeof(buf),nfmtparms,fmtparms);
      if (s_len<1) return;
      s=buf;
    }
    else 
    {
      s_len = fs?fs->GetLength():(int)strlen(s);
    }
  }

  if (s_len)
  {
    SetImageDirty(dest);
    if (formatmode>=2)
    {
      if (nfmtparms==2)
      {
        RECT r={0,0,0,0};
        __drawTextWithFont(dest,&r,GetActiveFont(),s,s_len,
          getCurColor(),getCurMode(),(float)*m_gfx_a,0,NULL,fmtparms);
      }
    }
    else
    {    
      RECT r={(int)floor(*m_gfx_x),(int)floor(*m_gfx_y),0,0};
      int flags=DT_NOCLIP;
      if (formatmode == 0 && nparms >= 4)
      {
        flags=(int)*parms[1];
        flags &= (DT_CENTER|DT_RIGHT|DT_VCENTER|DT_BOTTOM|DT_NOCLIP);
        r.right=(int)*parms[2];
        r.bottom=(int)*parms[3];
      }
      *m_gfx_x=__drawTextWithFont(dest,&r,GetActiveFont(),s,s_len,
        getCurColor(),getCurMode(),(float)*m_gfx_a,flags,m_gfx_y,NULL);
    }
  }
}

void eel_lice_state::gfx_drawchar(EEL_F ch)
{
  LICE_IBitmap *dest = GetImageForIndex(*m_gfx_dest,"gfx_drawchar");
  if (!dest) return;

  SetImageDirty(dest);

  int a=(int)(ch+0.5);
  if (a == '\r' || a=='\n') a=' ';

  char buf[32];
  const int buflen = WDL_MakeUTFChar(buf, a, sizeof(buf));

  RECT r={(int)floor(*m_gfx_x),(int)floor(*m_gfx_y),0,0};
  *m_gfx_x = __drawTextWithFont(dest,&r,
                         GetActiveFont(),buf,buflen,
                         getCurColor(),getCurMode(),(float)*m_gfx_a,DT_NOCLIP,NULL,NULL);

}


void eel_lice_state::gfx_drawnumber(EEL_F n, EEL_F ndigits)
{
  LICE_IBitmap *dest = GetImageForIndex(*m_gfx_dest,"gfx_drawnumber");
  if (!dest) return;

  SetImageDirty(dest);

  char buf[512];
  int a=(int)(ndigits+0.5);
  if (a <0)a=0;
  else if (a > 16) a=16;
  snprintf(buf,sizeof(buf),"%.*f",a,n);

  RECT r={(int)floor(*m_gfx_x),(int)floor(*m_gfx_y),0,0};
  *m_gfx_x = __drawTextWithFont(dest,&r,
                           GetActiveFont(),buf,(int)strlen(buf),
                           getCurColor(),getCurMode(),(float)*m_gfx_a,DT_NOCLIP,NULL,NULL);
}

int eel_lice_state::setup_frame(HWND hwnd, RECT r, int _mouse_x, int _mouse_y, int has_dpi)
{
  int use_w = r.right - r.left;
  int use_h = r.bottom - r.top;

  POINT pt = { _mouse_x, _mouse_y };
  if (hwnd)
  {
    GetCursorPos(&pt);
    ScreenToClient(hwnd,&pt);
  }
  *m_mouse_x=pt.x-r.left;
  *m_mouse_y=pt.y-r.top;
  if (has_dpi>0 && *m_gfx_ext_retina > 0.0)
  {
    *m_gfx_ext_retina = has_dpi/256.0;
  }
  else if (*m_gfx_ext_retina > 0.0)
  {
#ifdef __APPLE__
    *m_gfx_ext_retina = (hwnd && SWELL_IsRetinaHWND(hwnd)) ? 2.0 : 1.0;
    if (*m_gfx_ext_retina > 1.0)
    {
      *m_mouse_x *= 2.0;
      *m_mouse_y *= 2.0;
      use_w*=2;
      use_h*=2;
    }
#else
    *m_gfx_ext_retina = 1.0;
    #ifdef _WIN32
       static UINT (WINAPI *__GetDpiForWindow)(HWND);
       if (!__GetDpiForWindow)
       {
         HINSTANCE h = LoadLibrary("user32.dll");
         if (h) *(void **)&__GetDpiForWindow = GetProcAddress(h,"GetDpiForWindow");
         if (!__GetDpiForWindow)
           *(void **)&__GetDpiForWindow = (void*)(INT_PTR)1;
       }
       if (hwnd && (UINT_PTR)__GetDpiForWindow > (UINT_PTR)1)
       {
         int dpi = __GetDpiForWindow(hwnd);
         if (dpi != 96)
           *m_gfx_ext_retina = dpi / 96.0;
       }
    #else
       const int rsc = SWELL_GetScaling256();
       if (rsc > 256) *m_gfx_ext_retina = rsc/256.0;
    #endif
#endif
  }
  int dr=0;
  if (!m_framebuffer && LICE_FUNCTION_VALID(__LICE_CreateBitmap)) 
  {
    m_framebuffer=__LICE_CreateBitmap(1,use_w,use_h);
    dr=1;
  }

  if (!m_framebuffer || !LICE_FUNCTION_VALID(LICE__GetHeight) || !LICE_FUNCTION_VALID(LICE__GetWidth)) return -1;

  if (use_w != LICE__GetWidth(m_framebuffer) || use_h != LICE__GetHeight(m_framebuffer))
  {
    LICE__resize(m_framebuffer,use_w,use_h);
    dr=1;
  }
  *m_gfx_w = use_w;
  *m_gfx_h = use_h;
  
  if (*m_gfx_clear > -1.0 && dr)
  {
    const int a=(int)*m_gfx_clear;
    if (LICE_FUNCTION_VALID(LICE_Clear)) LICE_Clear(m_framebuffer,LICE_RGBA((a&0xff),((a>>8)&0xff),((a>>16)&0xff),0));
  }
  m_framebuffer_dirty = dr;

  int vflags=0;

  if (m_has_cap)
  {
    bool swap = false;
#ifdef _WIN32
    swap = !!GetSystemMetrics(SM_SWAPBUTTON);
#endif
    vflags|=m_has_cap&0xffff;
    if (GetAsyncKeyState(VK_LBUTTON)&0x8000) vflags|=swap?2:1;
    if (GetAsyncKeyState(VK_RBUTTON)&0x8000) vflags|=swap?1:2;
    if (GetAsyncKeyState(VK_MBUTTON)&0x8000) vflags|=64;
  }
  if (m_has_cap || (m_has_had_getch && hwnd && GetFocus()==hwnd))
  {
    if (GetAsyncKeyState(VK_CONTROL)&0x8000) vflags|=4;
    if (GetAsyncKeyState(VK_SHIFT)&0x8000) vflags|=8;
    if (GetAsyncKeyState(VK_MENU)&0x8000) vflags|=16;
    if (GetAsyncKeyState(VK_LWIN)&0x8000) vflags|=32;
  }
  m_has_cap &= 0xf0000;

  *m_mouse_cap=(EEL_F)vflags;

  *m_gfx_dest = -1.0; // m_framebuffer
  *m_gfx_a2 = *m_gfx_a = 1.0; // default to full alpha every call
  int fh;
  if (m_gfx_font_active>=0&&m_gfx_font_active<m_gfx_fonts.GetSize() && (fh=m_gfx_fonts.Get()[m_gfx_font_active].use_fonth)>0)
    *m_gfx_texth=fh;
  else 
    *m_gfx_texth = 8;
  
  return dr;
}

void eel_lice_state::finish_draw()
{
  if (hwnd_standalone && m_framebuffer_dirty) 
  {
#ifdef __APPLE__
    void *p = SWELL_InitAutoRelease();
#endif

    InvalidateRect(hwnd_standalone,NULL,FALSE);
    UpdateWindow(hwnd_standalone);

#ifdef __APPLE__
    SWELL_QuitAutoRelease(p);
#endif
    m_framebuffer_dirty = 0;
  }
}

#ifndef EEL_LICE_NO_REGISTER
void eel_lice_register()
{
  NSEEL_addfunc_retptr("gfx_lineto",3,NSEEL_PProc_THIS,&_gfx_lineto);
  NSEEL_addfunc_retptr("gfx_lineto",2,NSEEL_PProc_THIS,&_gfx_lineto2);
  NSEEL_addfunc_retptr("gfx_rectto",2,NSEEL_PProc_THIS,&_gfx_rectto);
  NSEEL_addfunc_varparm("gfx_rect",4,NSEEL_PProc_THIS,&_gfx_rect);
  NSEEL_addfunc_varparm("gfx_line",4,NSEEL_PProc_THIS,&_gfx_line); // 5th param is optionally AA
  NSEEL_addfunc_varparm("gfx_gradrect",8,NSEEL_PProc_THIS,&_gfx_gradrect);
  NSEEL_addfunc_varparm("gfx_muladdrect",7,NSEEL_PProc_THIS,&_gfx_muladdrect);
  NSEEL_addfunc_varparm("gfx_deltablit",9,NSEEL_PProc_THIS,&_gfx_deltablit);
  NSEEL_addfunc_exparms("gfx_transformblit",8,NSEEL_PProc_THIS,&_gfx_transformblit);
  NSEEL_addfunc_varparm("gfx_circle",3,NSEEL_PProc_THIS,&_gfx_circle);
  NSEEL_addfunc_varparm("gfx_triangle", 6, NSEEL_PProc_THIS, &_gfx_triangle);
  NSEEL_addfunc_varparm("gfx_roundrect",5,NSEEL_PProc_THIS,&_gfx_roundrect);
  NSEEL_addfunc_varparm("gfx_arc",5,NSEEL_PProc_THIS,&_gfx_arc);
  NSEEL_addfunc_retptr("gfx_blurto",2,NSEEL_PProc_THIS,&_gfx_blurto);
  NSEEL_addfunc_exparms("gfx_showmenu",1,NSEEL_PProc_THIS,&_gfx_showmenu);
  NSEEL_addfunc_varparm("gfx_setcursor",1, NSEEL_PProc_THIS, &_gfx_setcursor);
  NSEEL_addfunc_retptr("gfx_drawnumber",2,NSEEL_PProc_THIS,&_gfx_drawnumber);
  NSEEL_addfunc_retptr("gfx_drawchar",1,NSEEL_PProc_THIS,&_gfx_drawchar);
  NSEEL_addfunc_varparm("gfx_drawstr",1,NSEEL_PProc_THIS,&_gfx_drawstr);
  NSEEL_addfunc_retptr("gfx_measurestr",3,NSEEL_PProc_THIS,&_gfx_measurestr);
  NSEEL_addfunc_retptr("gfx_measurechar",3,NSEEL_PProc_THIS,&_gfx_measurechar);
  NSEEL_addfunc_varparm("gfx_printf",1,NSEEL_PProc_THIS,&_gfx_printf);
  NSEEL_addfunc_retptr("gfx_setpixel",3,NSEEL_PProc_THIS,&_gfx_setpixel);
  NSEEL_addfunc_retptr("gfx_getpixel",3,NSEEL_PProc_THIS,&_gfx_getpixel);
  NSEEL_addfunc_retptr("gfx_getimgdim",3,NSEEL_PProc_THIS,&_gfx_getimgdim);
  NSEEL_addfunc_retval("gfx_setimgdim",3,NSEEL_PProc_THIS,&_gfx_setimgdim);
  NSEEL_addfunc_retval("gfx_loadimg",2,NSEEL_PProc_THIS,&_gfx_loadimg);
  NSEEL_addfunc_retptr("gfx_blit",3,NSEEL_PProc_THIS,&_gfx_blit);
  NSEEL_addfunc_retptr("gfx_blitext",3,NSEEL_PProc_THIS,&_gfx_blitext);
  NSEEL_addfunc_varparm("gfx_blit",4,NSEEL_PProc_THIS,&_gfx_blit2);
  NSEEL_addfunc_varparm("gfx_setfont",1,NSEEL_PProc_THIS,&_gfx_setfont);
  NSEEL_addfunc_varparm("gfx_getfont",1,NSEEL_PProc_THIS,&_gfx_getfont);
  NSEEL_addfunc_varparm("gfx_set",1,NSEEL_PProc_THIS,&_gfx_set);
  NSEEL_addfunc_varparm("gfx_getdropfile",1,NSEEL_PProc_THIS,&_gfx_getdropfile);
}
#endif

#ifdef EEL_LICE_WANT_STANDALONE

#ifdef _WIN32
static HINSTANCE eel_lice_hinstance;
#endif
static const char *eel_lice_standalone_classname;

#ifdef EEL_LICE_WANT_STANDALONE_UPDATE
static EEL_F * NSEEL_CGEN_CALL _gfx_update(void *opaque, EEL_F *n)
{
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT(opaque);
  if (ctx)
  {
    ctx->m_ddrop_files.Empty(true,free);
    if (ctx->hwnd_standalone) 
    {
#ifndef EEL_LICE_WANT_STANDALONE_UPDATE_NO_SETUPFRAME
      ctx->finish_draw();
#endif

      // run message pump
#ifndef EEL_LICE_WANT_STANDALONE_UPDATE_NO_MSGPUMP

#ifdef _WIN32
      MSG msg;
      while (PeekMessage(&msg,NULL,0,0,PM_REMOVE)) 
      {
	      TranslateMessage(&msg);
	      DispatchMessage(&msg);
      }
#else
      void SWELL_RunEvents();
      SWELL_RunEvents();
#endif
#endif
#ifndef EEL_LICE_WANT_STANDALONE_UPDATE_NO_SETUPFRAME
      RECT r;
      GetClientRect(ctx->hwnd_standalone,&r);
      ctx->setup_frame(ctx->hwnd_standalone,r);
#endif
    }
  }
  return n;
}
#endif



static EEL_F NSEEL_CGEN_CALL _gfx_getchar(void *opaque, EEL_F *p)
{
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT(opaque);
  if (ctx)
  {
    ctx->m_has_had_getch=true;
    if (*p >= 2.0)
    {
      if (*p == 65536.0)
      {
        int rv = 1;
        if (ctx->hwnd_standalone)
        {
          if (ctx->hwnd_standalone==GetFocus()) rv|=2;
          if (IsWindowVisible(ctx->hwnd_standalone)) rv|=4;
        }
        return rv;
      }
      int x;
      const int n = sizeof(ctx->hwnd_standalone_kb_state) / sizeof(ctx->hwnd_standalone_kb_state[0]);
      int *st = ctx->hwnd_standalone_kb_state;
      int a = (int)*p;
      for (x=0;x<n && st[x] != a;x++);
      return x<n ? 1.0 : 0.0;
    }

    if (!ctx->hwnd_standalone) return -1.0;

    if (ctx->m_kb_queue_valid)
    {
      const int qsize = sizeof(ctx->m_kb_queue)/sizeof(ctx->m_kb_queue[0]);
      const int a = ctx->m_kb_queue[ctx->m_kb_queue_pos & (qsize-1)];
      ctx->m_kb_queue_pos++;
      ctx->m_kb_queue_valid--;
      return a;
    }
  }
  return 0.0;
}

static int eel_lice_key_xlate(int msg, int wParam, int lParam, bool *isAltOut)
{
#define EEL_MB_C(a) (sizeof(a)<=2 ? a[0] : \
                     sizeof(a)==3 ?  (((a[0])<<8)+(a[1])) : \
                     sizeof(a)==4 ? (((a[0])<<16)+((a[1])<<8)+(a[2])) : \
                     (((a[0])<<24)+((a[1])<<16)+((a[2])<<8)+(a[3])))

  if (msg != WM_CHAR)
  {
#ifndef _WIN32
    if (lParam & FVIRTKEY)
#endif
    switch (wParam)
	  {
      case VK_HOME: return EEL_MB_C("home");
      case VK_UP: return EEL_MB_C("up");
      case VK_PRIOR: return EEL_MB_C("pgup");
      case VK_LEFT: return EEL_MB_C("left");
      case VK_RIGHT: return EEL_MB_C("rght");
      case VK_END: return EEL_MB_C("end");
      case VK_DOWN: return EEL_MB_C("down");
      case VK_NEXT: return EEL_MB_C("pgdn");
      case VK_INSERT: return EEL_MB_C("ins");
      case VK_DELETE: return EEL_MB_C("del");
      case VK_F1: return EEL_MB_C("f1");
      case VK_F2: return EEL_MB_C("f2");
      case VK_F3: return EEL_MB_C("f3");
      case VK_F4: return EEL_MB_C("f4");
      case VK_F5: return EEL_MB_C("f5");
      case VK_F6: return EEL_MB_C("f6");
      case VK_F7: return EEL_MB_C("f7");
      case VK_F8: return EEL_MB_C("f8");
      case VK_F9: return EEL_MB_C("f9");
      case VK_F10: return EEL_MB_C("f10");
      case VK_F11: return EEL_MB_C("f11");
      case VK_F12: return EEL_MB_C("f12");
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
      case VK_RETURN: 
      case VK_BACK: 
      case VK_TAB: 
      case VK_ESCAPE: 
        return wParam;
      
      case VK_CONTROL: break;
    
      default:
        {
          const bool isctrl = !!(GetAsyncKeyState(VK_CONTROL)&0x8000);
          const bool isalt = !!(GetAsyncKeyState(VK_MENU)&0x8000);
          if(isctrl || isalt)
          {
            if (wParam>='a' && wParam<='z') 
            {
              if (isctrl) wParam += 1-'a';
              if (isalt) wParam += 256;
              *isAltOut=isalt;
              return wParam;
            }
            if (wParam>='A' && wParam<='Z') 
            {
              if (isctrl) wParam += 1-'A';
              if (isalt) wParam += 256;
              *isAltOut=isalt;
              return wParam;
            }
          }

          if (isctrl)
          {
            if ((wParam&~0x80) == '[') return 27;
            if ((wParam&~0x80) == ']') return 29;
          }
        }
      break;
    }
  }
    
  if(wParam>=32) 
  {
    #ifdef _WIN32
      if (msg == WM_CHAR) return wParam;
    #else
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
    #endif
  }      
  return 0;
}
#undef EEL_MB_C

static LRESULT WINAPI eel_lice_wndproc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
#ifdef __APPLE__
extern "C" 
{
  void *objc_getClass(const char *p);
  void *sel_getUid(const char *p);
  void objc_msgSend(void);
};
#endif


HWND eel_lice_state::create_wnd(HWND par, int isChild)
{
  if (hwnd_standalone) return hwnd_standalone;
#ifdef _WIN32
  return CreateWindowEx(WS_EX_ACCEPTFILES,eel_lice_standalone_classname,"",
                        isChild ? (WS_CHILD|WS_TABSTOP) : (WS_POPUP|WS_CAPTION|WS_THICKFRAME|WS_SYSMENU),CW_USEDEFAULT,CW_USEDEFAULT,100,100,par,NULL,eel_lice_hinstance,this);
#else
  HWND h = SWELL_CreateDialog(NULL,isChild ? NULL : ((const char *)(INT_PTR)0x400001),par,(DLGPROC)eel_lice_wndproc,(LPARAM)this);
  if (h)
  {
    SWELL_SetClassName(h,eel_lice_standalone_classname);
    SWELL_EnableMetal(h,1);
  }
  return h;
#endif
}

#ifdef EEL_LICE_WANTDOCK
#ifndef ID_DOCKWINDOW
#define ID_DOCKWINDOW 40269
#endif

static EEL_F NSEEL_CGEN_CALL _gfx_dock(void *opaque, INT_PTR np, EEL_F **parms)
{
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT(opaque);
  if (ctx)
  {
    if (np > 0 && parms[0][0] >= 0.0 && ctx->hwnd_standalone) EEL_LICE_WANTDOCK(ctx,(int)parms[0][0]);

    if (np > 1 && parms[1]) parms[1][0] = ctx->m_last_undocked_r.left;
    if (np > 2 && parms[2]) parms[2][0] = ctx->m_last_undocked_r.top;
    if (np > 3 && parms[3]) parms[3][0] = ctx->m_last_undocked_r.right;
    if (np > 4 && parms[4]) parms[4][0] = ctx->m_last_undocked_r.bottom;

#ifdef EEL_LICE_ISDOCKED
    return EEL_LICE_ISDOCKED(ctx); 
#endif
  }
  return 0.0;
}

#endif //EEL_LICE_WANTDOCK


#ifndef EEL_LICE_STANDALONE_NOINITQUIT

static EEL_F * NSEEL_CGEN_CALL _gfx_quit(void *opaque, EEL_F *n)
{
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT(opaque);
  if (ctx)
  {
    if (ctx->hwnd_standalone) 
    {
      DestroyWindow(ctx->hwnd_standalone);
    }
    ctx->hwnd_standalone=0;
  }
  return n; 
}

static EEL_F NSEEL_CGEN_CALL _gfx_init(void *opaque, INT_PTR np, EEL_F **parms)
{
#ifdef EEL_LICE_GET_CONTEXT_INIT
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT_INIT(opaque); 
#else
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT(opaque); 
#endif
  if (ctx)
  {
    bool wantShow=false, wantResize=true;
    int sug_w = np > 1 ? (int)parms[1][0] : 640;
    int sug_h = np > 2 ? (int)parms[2][0] : 480;
    if (sug_w <1 && sug_h < 1 && ctx->hwnd_standalone) 
    {
      RECT r;
      GetClientRect(ctx->hwnd_standalone,&r);
      sug_w = r.right;
      sug_h = r.bottom;
    }
    #ifdef EEL_LICE_WANTDOCK
    const int pos_offs = 4;
    #else
    const int pos_offs = 3;
    #endif

    if (sug_w < 16) sug_w=16;
    else if (sug_w > 2048) sug_w=2048;
    if (sug_h < 16) sug_h=16;
    else if (sug_h > 1600) sug_h=1600;

    if (!ctx->hwnd_standalone)
    {
      #ifdef __APPLE__
        void *(*send_msg)(void *, void *) = (void *(*)(void *, void *))objc_msgSend;
        void (*send_msg_longparm)(void *, void *, long) = (void (*)(void *, void *, long))objc_msgSend; // long = NSInteger

        void *nsapp=send_msg( objc_getClass("NSApplication"), sel_getUid("sharedApplication"));
        send_msg_longparm(nsapp,sel_getUid("setActivationPolicy:"), 0);
        send_msg_longparm(nsapp,sel_getUid("activateIgnoringOtherApps:"), 1);

      #endif

      #ifdef EEL_LICE_STANDALONE_PARENT
        HWND par = EEL_LICE_STANDALONE_PARENT(opaque);
      #elif defined(_WIN32)
        HWND par=GetDesktopWindow();
      #else
        HWND par=NULL;
      #endif

      ctx->create_wnd(par,0);
      // resize client

      if (ctx->hwnd_standalone)
      {
        int px=0,py=0;
        if (np >= pos_offs+2)
        {
          px = (int) floor(parms[pos_offs][0] + 0.5);
          py = (int) floor(parms[pos_offs+1][0] + 0.5);
#ifdef EEL_LICE_VALIDATE_RECT_ON_SCREEN
          RECT r = {px,py,px+sug_w,py+sug_h};
          EEL_LICE_VALIDATE_RECT_ON_SCREEN(r);
          px=r.left; py=r.top; sug_w = r.right-r.left; sug_h = r.bottom-r.top;
#endif
          ctx->m_last_undocked_r.left = px;
          ctx->m_last_undocked_r.top = py;
          ctx->m_last_undocked_r.right = sug_w;
          ctx->m_last_undocked_r.bottom = sug_h;
        }

        RECT r1,r2;
        GetWindowRect(ctx->hwnd_standalone,&r1);
        GetClientRect(ctx->hwnd_standalone,&r2);
        sug_w += (r1.right-r1.left) - r2.right;
        sug_h += abs(r1.bottom-r1.top) - r2.bottom;

        SetWindowPos(ctx->hwnd_standalone,NULL,px,py,sug_w,sug_h,(np >= pos_offs+2 ? 0:SWP_NOMOVE)|SWP_NOZORDER|SWP_NOACTIVATE);

        wantShow=true;
        #ifdef EEL_LICE_WANTDOCK
          if (np > 3) EEL_LICE_WANTDOCK(ctx,parms[3][0]);
        #endif
        #ifdef EEL_LICE_WANT_STANDALONE_UPDATE
          {
            RECT r;
            GetClientRect(ctx->hwnd_standalone,&r);
            ctx->setup_frame(ctx->hwnd_standalone,r);
          }
        #endif
      }
      wantResize=false;
    }
    if (!ctx->hwnd_standalone) return 0;

    if (np>0)
    {
      EEL_STRING_MUTEXLOCK_SCOPE
      const char *title=EEL_STRING_GET_FOR_INDEX(parms[0][0],NULL);
      #ifdef EEL_STRING_DEBUGOUT
        if (!title) EEL_STRING_DEBUGOUT("gfx_init: invalid string identifier %f",parms[0][0]);
      #endif
      if (title&&*title)
      {
        SetWindowText(ctx->hwnd_standalone,title);
        wantResize=false; // ignore resize if we're setting title
      }
    }
    if (wantShow)
      ShowWindow(ctx->hwnd_standalone,SW_SHOW);
    if (wantResize && np>2 && !(GetWindowLong(ctx->hwnd_standalone,GWL_STYLE)&WS_CHILD))
    {
      RECT r1,r2;
      GetWindowRect(ctx->hwnd_standalone,&r1);
      GetClientRect(ctx->hwnd_standalone,&r2);
      const bool do_size = sug_w != r2.right || sug_h != r2.bottom;

      sug_w += (r1.right-r1.left) - r2.right;
      sug_h += abs(r1.bottom-r1.top) - r2.bottom;

      int px=0,py=0;
      const bool do_move=(np >= pos_offs+2);
      if (do_move)
      {
        px = (int) floor(parms[pos_offs][0] + 0.5);
        py = (int) floor(parms[pos_offs+1][0] + 0.5);
#ifdef EEL_LICE_VALIDATE_RECT_ON_SCREEN
        RECT r = {px,py,px+sug_w,py+sug_h};
        EEL_LICE_VALIDATE_RECT_ON_SCREEN(r);
        px=r.left; py=r.top; sug_w = r.right-r.left; sug_h = r.bottom-r.top;
#endif
      }
      if (do_size || do_move)
        SetWindowPos(ctx->hwnd_standalone,NULL,px,py,sug_w,sug_h,
            (do_size ? 0 : SWP_NOSIZE)|(do_move? 0:SWP_NOMOVE)|SWP_NOZORDER|SWP_NOACTIVATE);
    }
    return 1;
  }
  return 0;  
}

static EEL_F NSEEL_CGEN_CALL _gfx_screentoclient(void *opaque, EEL_F *x, EEL_F *y)
{
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT(opaque);
  if (ctx && ctx->hwnd_standalone)
  {
    POINT pt={(int) *x, (int) *y};
    ScreenToClient(ctx->hwnd_standalone,&pt);
    *x = pt.x; 
    *y = pt.y;
    return 1.0;
  }
  return 0.0;
}

static EEL_F NSEEL_CGEN_CALL _gfx_clienttoscreen(void *opaque, EEL_F *x, EEL_F *y)
{
  eel_lice_state *ctx=EEL_LICE_GET_CONTEXT(opaque);
  if (ctx && ctx->hwnd_standalone)
  {
    POINT pt={(int) *x, (int) *y};
    ClientToScreen(ctx->hwnd_standalone,&pt);
    *x = pt.x; 
    *y = pt.y;
    return 1.0;
  }
  return 0.0;
}

#endif // !EEL_LICE_STANDALONE_NOINITQUIT


#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL 0x20A
#endif
#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL 0x20E
#endif

LRESULT WINAPI eel_lice_wndproc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef WIN32
  static UINT Scroll_Message;
  static bool sm_init;
  if (!sm_init)
  {
    sm_init=true;
    Scroll_Message = RegisterWindowMessage("MSWHEEL_ROLLMSG");
  }
  if (Scroll_Message && uMsg == Scroll_Message)
  {
    uMsg=WM_MOUSEWHEEL;
    wParam<<=16; 
  }
#endif

  switch (uMsg)
  {
    case WM_CREATE: 
      {
#ifdef _WIN32
        LPCREATESTRUCT lpcs= (LPCREATESTRUCT )lParam;
        eel_lice_state *ctx=(eel_lice_state*)lpcs->lpCreateParams;
        SetWindowLongPtr(hwnd,GWLP_USERDATA,(LPARAM)lpcs->lpCreateParams);
#else
        eel_lice_state *ctx=(eel_lice_state*)lParam;
        SetWindowLongPtr(hwnd,GWLP_USERDATA,lParam);
        SetWindowLong(hwnd,GWL_EXSTYLE, GetWindowLong(hwnd,GWL_EXSTYLE) | WS_EX_ACCEPTFILES);
#endif
        ctx->m_kb_queue_valid=0;
        ctx->hwnd_standalone=hwnd;
      }
    return 0;
#ifndef _WIN32
    case WM_CLOSE:
      DestroyWindow(hwnd);
    return 0;
#endif
    case WM_DESTROY:
      {
        eel_lice_state *ctx=(eel_lice_state*)GetWindowLongPtr(hwnd,GWLP_USERDATA);
        if (ctx) 
        {
#ifdef EEL_LICE_WANTDOCK
          EEL_LICE_WANTDOCK(ctx,0);
#endif
          ctx->hwnd_standalone=NULL;
        }
      }
    return 0;
    case WM_ACTIVATE:
      {
        eel_lice_state *ctx=(eel_lice_state*)GetWindowLongPtr(hwnd,GWLP_USERDATA);
        if (ctx) memset(&ctx->hwnd_standalone_kb_state,0,sizeof(ctx->hwnd_standalone_kb_state));
      }
    break;
    case WM_SETCURSOR:
    {
      eel_lice_state *ctx=(eel_lice_state*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
      if (ctx && ctx->m_cursor_resid > 0)
      {
        POINT p;
        GetCursorPos(&p);
        ScreenToClient(hwnd, &p);
        RECT r;
        GetClientRect(hwnd, &r);
        if (p.x >= 0 && p.x < r.right && p.y >= 0 && p.y < r.bottom)
        {
#ifdef EEL_LICE_LOADTHEMECURSOR
          if (ctx->m_cursor_name[0]) 
            SetCursor(EEL_LICE_LOADTHEMECURSOR(ctx->m_cursor_resid, ctx->m_cursor_name));
          else
#endif
            SetCursor(LoadCursor(NULL, MAKEINTRESOURCE(ctx->m_cursor_resid)));
          return TRUE;
        }
      }
    }
    break;
#ifdef EEL_LICE_WANTDOCK
    case WM_CONTEXTMENU:
    {
      eel_lice_state *ctx=(eel_lice_state*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
      if (ctx)
      {
        char title[512], buf[1024];
        GetWindowText(hwnd, title, sizeof(title)-1);
        if (!title[0]) strcpy(title, "ReaScript");

        HMENU hm=CreatePopupMenu();
        int pos=0;

        int flag=((EEL_LICE_ISDOCKED(ctx)&1) ? MF_CHECKED : 0);
        snprintf(buf, sizeof(buf), "Dock %s window in Docker", title);
        InsertMenu(hm, pos++, MF_BYPOSITION|MF_STRING|flag, ID_DOCKWINDOW, buf);
        snprintf(buf, sizeof(buf), "Close %s window", title);
        InsertMenu(hm, pos++, MF_BYPOSITION|MF_STRING, IDCANCEL, buf);
        
        POINT pt;
        GetCursorPos(&pt);
        TrackPopupMenu(hm, 0, pt.x, pt.y, 0, hwnd, NULL);
        DestroyMenu(hm);
      }
    }
    return 0;
#endif
    case WM_COMMAND:
      switch (LOWORD(wParam))
      {
#ifdef EEL_LICE_WANTDOCK
        case ID_DOCKWINDOW:
        {
          eel_lice_state *ctx=(eel_lice_state*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
          if (ctx) EEL_LICE_WANTDOCK(ctx, EEL_LICE_ISDOCKED(ctx)^1);
        }
        return 0;
#endif
        case IDCANCEL:
          DestroyWindow(hwnd);
        return 0;
      }
    break;
    case WM_DROPFILES:
      {
        eel_lice_state *ctx=(eel_lice_state*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (ctx && wParam)
        {
          ctx->m_ddrop_files.Empty(true,free);

          HDROP hDrop = (HDROP) wParam;
          const int n=DragQueryFile(hDrop,-1,NULL,0);
          for (int x=0;x<n;x++)
          {
            char buf[4096];
            buf[0]=0;
            DragQueryFile(hDrop,x,buf,sizeof(buf));
            if (buf[0]) ctx->m_ddrop_files.Add(strdup(buf));
          }
          DragFinish(hDrop);
        }
      }
    return 0;
    case WM_MOUSEHWHEEL:   
    case WM_MOUSEWHEEL:
      {
        eel_lice_state *ctx=(eel_lice_state*)GetWindowLongPtr(hwnd,GWLP_USERDATA);
        if (ctx)
        {
          EEL_F *p= uMsg==WM_MOUSEHWHEEL ? ctx->m_mouse_hwheel : ctx->m_mouse_wheel;
          if (p) *p += (EEL_F) (short)HIWORD(wParam);
        }
      }
      return -1;
#ifdef _WIN32
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
#endif
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_CHAR:
      {
        eel_lice_state *ctx=(eel_lice_state*)GetWindowLongPtr(hwnd,GWLP_USERDATA);

        bool hadAltAdj=false;
        int a=eel_lice_key_xlate(uMsg,(int)wParam,(int)lParam, &hadAltAdj);
#ifdef _WIN32
        if (!a && (uMsg == WM_KEYUP || uMsg == WM_SYSKEYUP) && wParam >= 'A' && wParam <= 'Z') a=(int)wParam + 'a' - 'A';
#endif
        const int mask = hadAltAdj ? ~256 : ~0;

        if (a & mask)
        {
          int a_no_alt = (a&mask);
          const int lowera = a_no_alt >= 1 && a_no_alt < 27 ? (a_no_alt+'a'-1) : a_no_alt >= 'A' && a_no_alt <= 'Z' ? a_no_alt+'a'-'A' : a_no_alt;

          int *st = ctx->hwnd_standalone_kb_state;

          const int n = sizeof(ctx->hwnd_standalone_kb_state) / sizeof(ctx->hwnd_standalone_kb_state[0]);
          int zp=n-1,x;

          for (x=0;x<n && st[x] != lowera;x++) if (x < zp && !st[x]) zp=x;

          if (uMsg==WM_KEYUP
#ifdef _WIN32
             ||uMsg == WM_SYSKEYUP
#endif
            )
          {
            if (x<n) st[x]=0;
          }
          else if (x==n) // key not already down
          {
            st[zp]=lowera;
          }
        }

        if (a && uMsg != WM_KEYUP
#ifdef _WIN32
            && uMsg != WM_SYSKEYUP
#endif
            )
        {
          // add to queue
          const int qsize = sizeof(ctx->m_kb_queue)/sizeof(ctx->m_kb_queue[0]);
          if (ctx->m_kb_queue_valid>=qsize) // queue full, dump an old event!
          {
            ctx->m_kb_queue_valid--;
            ctx->m_kb_queue_pos++;
          }
          ctx->m_kb_queue[(ctx->m_kb_queue_pos + ctx->m_kb_queue_valid++) & (qsize-1)] = a;
        }

      }
    return 0;
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_LBUTTONDOWN:
    {
      POINT p = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
      RECT r;
      GetClientRect(hwnd, &r);
      if (p.x >= r.left && p.x < r.right && p.y >= r.top && p.y < r.bottom)
      {
        if (GetCapture()!=hwnd) SetFocus(hwnd);
        eel_lice_state *ctx=(eel_lice_state*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (ctx) 
        {
          if (GetCapture()!=hwnd) SetCapture(hwnd);
          int f = 0;
          if (uMsg == WM_LBUTTONDBLCLK || uMsg == WM_LBUTTONDOWN) f=0x10001;
          else if (uMsg == WM_RBUTTONDBLCLK || uMsg == WM_RBUTTONDOWN) f=0x20002;
          else if (uMsg == WM_MBUTTONDBLCLK || uMsg == WM_MBUTTONDOWN) f=0x40040;

          if (GetAsyncKeyState(VK_CONTROL)&0x8000) f|=4;
          if (GetAsyncKeyState(VK_SHIFT)&0x8000) f|=8;
          if (GetAsyncKeyState(VK_MENU)&0x8000) f|=16;
          if (GetAsyncKeyState(VK_LWIN)&0x8000) f|=32;

          ctx->m_has_cap|=f;
        }
      }
    }
    return 1;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    case WM_CAPTURECHANGED:
    {
      eel_lice_state *ctx=(eel_lice_state*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
      if (ctx) 
      {
        if (uMsg == WM_CAPTURECHANGED) 
        {
          ctx->m_has_cap &= 0xffff;
        }
        else 
        {
          if (uMsg == WM_LBUTTONUP) ctx->m_has_cap &= ~0x10000;
          else if (uMsg == WM_RBUTTONUP) ctx->m_has_cap &= ~0x20000;
          else if (uMsg == WM_MBUTTONUP) ctx->m_has_cap &= ~0x40000;

          if (!(ctx->m_has_cap & 0xf0000)) 
          {
            ReleaseCapture();
          }
        }
      }
    }
    return 1;
#ifdef _WIN32
    case WM_GETDLGCODE:
      if (GetWindowLong(hwnd,GWL_STYLE)&WS_CHILD) return DLGC_WANTALLKEYS;
    break;
    case 0x02E0: //WM_DPICHANGED
      if (!(GetWindowLong(hwnd,GWL_STYLE)&WS_CHILD))
      {
        RECT *prcNewWindow = (RECT*)lParam;
        SetWindowPos(hwnd,
          NULL,
          prcNewWindow ->left,
          prcNewWindow ->top,
          prcNewWindow->right - prcNewWindow->left,
          prcNewWindow->bottom - prcNewWindow->top,
          SWP_NOZORDER | SWP_NOACTIVATE);
      }
    break;
#endif
    case WM_SIZE:
      // fall through
#ifndef EEL_LICE_STANDALONE_NOINITQUIT
    case WM_MOVE:
      if (uMsg != WM_SIZE || wParam != SIZE_MINIMIZED)
      {
        eel_lice_state *ctx=(eel_lice_state*)GetWindowLongPtr(hwnd,GWLP_USERDATA);
        if (ctx 
#ifdef EEL_LICE_ISDOCKED
          && !(GetWindowLong(hwnd,GWL_STYLE)&WS_CHILD)
#endif
          )  
        {
          RECT r;
          GetWindowRect(hwnd,&ctx->m_last_undocked_r);
          GetClientRect(hwnd,&r);
          if (ctx->m_last_undocked_r.bottom < ctx->m_last_undocked_r.top) ctx->m_last_undocked_r.top = ctx->m_last_undocked_r.bottom;
          ctx->m_last_undocked_r.right = r.right;
          ctx->m_last_undocked_r.bottom = r.bottom;
        }

      }
#endif

    break;

    case WM_PAINT:
      {
        PAINTSTRUCT ps;
        if (BeginPaint(hwnd,&ps))
        {
          eel_lice_state *ctx=(eel_lice_state*)GetWindowLongPtr(hwnd,GWLP_USERDATA);
          if (ctx && ctx->m_framebuffer && 
              LICE_FUNCTION_VALID(LICE__GetDC) && LICE_FUNCTION_VALID(LICE__GetWidth) && LICE_FUNCTION_VALID(LICE__GetHeight))
          {
            int w = LICE__GetWidth(ctx->m_framebuffer);
            int h = LICE__GetHeight(ctx->m_framebuffer);
#ifdef __APPLE__
            if (*ctx->m_gfx_ext_retina > 1.0)
            {
              StretchBlt(ps.hdc,0,0,w/2,h/2,LICE__GetDC(ctx->m_framebuffer),0,0,w,h,SRCCOPY);
            }
            else
#endif
              BitBlt(ps.hdc,0,0,w,h,LICE__GetDC(ctx->m_framebuffer),0,0,SRCCOPY);
          }
          EndPaint(hwnd,&ps);
        }
      }
    return 0;
    case WM_GETMINMAXINFO:
    {
      LPMINMAXINFO p=(LPMINMAXINFO)lParam;
      if (p->ptMinTrackSize.x > 10) p->ptMinTrackSize.x = 10;
      if (p->ptMinTrackSize.y > 10) p->ptMinTrackSize.y = 10;
    }
    return 0;
  }

  return DefWindowProc(hwnd,uMsg,wParam,lParam);
}



void eel_lice_register_standalone(HINSTANCE hInstance, const char *classname, HWND hwndPar, HICON icon)
{
  eel_lice_standalone_classname=classname && *classname ? classname : "EEL_LICE_gfx_standalone";
#ifdef _WIN32
  static bool reg;
  if (!reg)
  {
    eel_lice_hinstance=hInstance;
    WNDCLASS wc={CS_DBLCLKS,eel_lice_wndproc,0,0,hInstance,icon,LoadCursor(NULL,IDC_ARROW), NULL, NULL,eel_lice_standalone_classname};
    RegisterClass(&wc);
    reg = true;
  }
#endif

#ifndef EEL_LICE_NO_REGISTER
  // gfx_init(title[, w,h, flags])
#ifndef EEL_LICE_STANDALONE_NOINITQUIT
  NSEEL_addfunc_varparm("gfx_init",1,NSEEL_PProc_THIS,&_gfx_init); 
  NSEEL_addfunc_retptr("gfx_quit",1,NSEEL_PProc_THIS,&_gfx_quit);

  NSEEL_addfunc_retval("gfx_screentoclient",2,NSEEL_PProc_THIS,&_gfx_screentoclient);
  NSEEL_addfunc_retval("gfx_clienttoscreen",2,NSEEL_PProc_THIS,&_gfx_clienttoscreen);

#endif
#ifdef EEL_LICE_WANTDOCK
  NSEEL_addfunc_varparm("gfx_dock",1,NSEEL_PProc_THIS,&_gfx_dock);
#endif

#ifdef EEL_LICE_WANT_STANDALONE_UPDATE
  NSEEL_addfunc_retptr("gfx_update",1,NSEEL_PProc_THIS,&_gfx_update);
#endif

  NSEEL_addfunc_retval("gfx_getchar",1,NSEEL_PProc_THIS,&_gfx_getchar);
#endif
}


#endif

#endif//!EEL_LICE_API_ONLY




#ifdef DYNAMIC_LICE
static void eel_lice_initfuncs(void *(*getFunc)(const char *name))
{
  if (!getFunc) return;

  *(void **)&__LICE_CreateBitmap = getFunc("LICE_CreateBitmap");
  *(void **)&LICE_Clear = getFunc("LICE_Clear");
  *(void **)&LICE_Line = getFunc("LICE_LineInt");
  *(void **)&LICE_ClipLine = getFunc("LICE_ClipLine");
  *(void **)&LICE_FillRect = getFunc("LICE_FillRect");
  *(void **)&LICE_DrawRect = getFunc("LICE_DrawRect");
  *(void **)&LICE_PutPixel = getFunc("LICE_PutPixel");
  *(void **)&LICE_GetPixel = getFunc("LICE_GetPixel");
  *(void **)&LICE_DrawText = getFunc("LICE_DrawText");
  *(void **)&LICE_DrawChar = getFunc("LICE_DrawChar");
  *(void **)&LICE_MeasureText = getFunc("LICE_MeasureText");
  *(void **)&LICE_LoadImage = getFunc("LICE_LoadImage");
  *(void **)&LICE__GetDC = getFunc("LICE__GetDC");
  *(void **)&LICE__Destroy = getFunc("LICE__Destroy");
  *(void **)&LICE__GetWidth = getFunc("LICE__GetWidth");
  *(void **)&LICE__GetHeight = getFunc("LICE__GetHeight");
  *(void **)&LICE__resize = getFunc("LICE__resize");
  *(void **)&LICE_Blur = getFunc("LICE_Blur");
  *(void **)&LICE_RotatedBlit = getFunc("LICE_RotatedBlit");
  *(void **)&LICE_ScaledBlit = getFunc("LICE_ScaledBlit");
  *(void **)&LICE_Circle = getFunc("LICE_Circle");
  *(void **)&LICE_FillCircle = getFunc("LICE_FillCircle");
  *(void **)&LICE_FillTriangle=getFunc("LICE_FillTriangle");
  *(void **)&LICE_FillConvexPolygon=getFunc("LICE_FillConvexPolygon");  
  *(void **)&LICE_RoundRect = getFunc("LICE_RoundRect");
  *(void **)&LICE_Arc = getFunc("LICE_Arc");

  *(void **)&LICE_MultiplyAddRect  = getFunc("LICE_MultiplyAddRect");
  *(void **)&LICE_GradRect  = getFunc("LICE_GradRect");
  *(void **)&LICE_TransformBlit2  = getFunc("LICE_TransformBlit2");
  *(void **)&LICE_DeltaBlit  = getFunc("LICE_DeltaBlit");

  *(void **)&LICE__DestroyFont = getFunc("LICE__DestroyFont");    
  *(void **)&LICE_CreateFont = getFunc("LICE_CreateFont");    
  *(void **)&LICE__SetFromHFont = getFunc("LICE__SetFromHFont2");

  *(void **)&LICE__SetTextColor = getFunc("LICE__SetTextColor");    
  *(void **)&LICE__SetTextCombineMode = getFunc("LICE__SetTextCombineMode");    
  *(void **)&LICE__DrawText = getFunc("LICE__DrawText");    
}
#endif

#ifdef EEL_WANT_DOCUMENTATION

#ifdef EELSCRIPT_LICE_MAX_IMAGES
#define MKSTR2(x) #x
#define MKSTR(x) MKSTR2(x)
#define EEL_LICE_DOC_MAXHANDLE MKSTR(EELSCRIPT_LICE_MAX_IMAGES-1)
#else
#define EEL_LICE_DOC_MAXHANDLE "127"
#endif


static const char *eel_lice_function_reference =
#ifdef EEL_LICE_WANT_STANDALONE
#ifndef EEL_LICE_STANDALONE_NOINITQUIT
#ifdef EEL_LICE_WANTDOCK
  "gfx_init\t\"name\"[,width,height,dockstate,xpos,ypos]\tInitializes the graphics window with title name. Suggested width and height can be specified.\n\n"
#else
  "gfx_init\t\"name\"[,width,height,xpos,ypos]\tInitializes the graphics window with title name. Suggested width and height can be specified.\n\n"
#endif
  "Once the graphics window is open, gfx_update() should be called periodically. \0"
  "gfx_quit\t\tCloses the graphics window.\0"
#endif
#ifdef EEL_LICE_WANT_STANDALONE_UPDATE
  "gfx_update\t\tUpdates the graphics display, if opened\0"
#endif
#endif
#ifdef EEL_LICE_WANTDOCK
  "gfx_dock\tv[,wx,wy,ww,wh]\tCall with v=-1 to query docked state, otherwise v>=0 to set docked state. State is &1 if docked, second byte is docker index (or last docker index if undocked). If wx-wh are specified, they will be filled with the undocked window position/size\0"
#endif
  "gfx_aaaaa\t\t"
  "The following global variables are special and will be used by the graphics system:\n\n\3"
  "\4gfx_r, gfx_g, gfx_b, gfx_a2 - These represent the current red, green, blue, and alpha components used by drawing operations (0.0..1.0). gfx_a2 is the value written to the alpha channel when writing solid colors (normally ignored but useful when creating transparent images)\n"
  "\4gfx_a, gfx_mode - Alpha and blend mode for drawing. Set mode to 0 for default options. Add 1.0 for additive blend mode (if you wish to do subtractive, set gfx_a to negative and use gfx_mode as additive). Add 2.0 to disable source alpha for gfx_blit(). Add 4.0 to disable filtering for gfx_blit(). \n"
  "\4gfx_w, gfx_h - These are set to the current width and height of the UI framebuffer. \n"
  "\4gfx_x, gfx_y - These set the \"current\" graphics position in x,y. You can set these yourselves, and many of the drawing functions update them as well. \n"
  "\4gfx_clear - If set to a value greater than -1.0, this will result in the framebuffer being cleared to that color. the color for this one is packed RGB (0..255), i.e. red+green*256+blue*65536. The default is 0 (black). \n"
  "\4gfx_dest - Defaults to -1, set to 0.." EEL_LICE_DOC_MAXHANDLE " to have drawing operations go to an offscreen buffer (or loaded image).\n"
  "\4gfx_texth - Set to the height of a line of text in the current font. Do not modify this variable.\n"
  "\4gfx_ext_retina - If set to 1.0 on initialization, will be updated to 2.0 if high resolution display is supported, and if so gfx_w/gfx_h/etc will be doubled.\n"
  "\4mouse_x, mouse_y - mouse_x and mouse_y are set to the coordinates of the mouse relative to the graphics window.\n"
  "\4mouse_wheel, mouse_hwheel - mouse wheel (and horizontal wheel) positions. These will change typically by 120 or a multiple thereof, the caller should clear the state to 0 after reading it."
  "\4mouse_cap is a bitfield of mouse and keyboard modifier state.\3"
    "\4" "1: left mouse button\n"
    "\4" "2: right mouse button\n"
#ifdef __APPLE__
    "\4" "4: Command key\n"
    "\4" "8: Shift key\n"
    "\4" "16: Option key\n"
    "\4" "32: Control key\n"
#else
    "\4" "4: Control key\n"
    "\4" "8: Shift key\n"
    "\4" "16: Alt key\n"
    "\4" "32: Windows key\n"
#endif
    "\4" "64: middle mouse button\n"
  "\2"
  "\2\0"

"gfx_getchar\t[char]\tIf char is 0 or omitted, returns a character from the keyboard queue, or 0 if no character is available, or -1 if the graphics window is not open. "
     "If char is specified and nonzero, that character's status will be checked, and the function will return greater than 0 if it is pressed.\n\n"
     "Common values are standard ASCII, such as 'a', 'A', '=' and '1', but for many keys multi-byte values are used, including 'home', 'up', 'down', 'left', 'rght', 'f1'.. 'f12', 'pgup', 'pgdn', 'ins', and 'del'. \n\n"
     "Modified and special keys can also be returned, including:\3\n"
     "\4Ctrl/Cmd+A..Ctrl+Z as 1..26\n"
     "\4Ctrl/Cmd+Alt+A..Z as 257..282\n"
     "\4Alt+A..Z as 'A'+256..'Z'+256\n"
     "\4" "27 for ESC\n"
     "\4" "13 for Enter\n"
     "\4' ' for space\n"
     "\4" "65536 for query of special flags, returns: &1 (supported), &2=window has focus, &4=window is visible\n"
     "\2\0"
    
  "gfx_showmenu\t\"str\"\tShows a popup menu at gfx_x,gfx_y. str is a list of fields separated by | characters. "
    "Each field represents a menu item.\nFields can start with special characters:\n\n"
    "# : grayed out\n"
    "! : checked\n"
    "> : this menu item shows a submenu\n"
    "< : last item in the current submenu\n\n"
    "An empty field will appear as a separator in the menu. "
    "gfx_showmenu returns 0 if the user selected nothing from the menu, 1 if the first field is selected, etc.\nExample:\n\n"
    "gfx_showmenu(\"first item, followed by separator||!second item, checked|>third item which spawns a submenu|#first item in submenu, grayed out|<second and last item in submenu|fourth item in top menu\")\0"  
  
#ifdef EEL_LICE_LOADTHEMECURSOR
  "gfx_setcursor\tresource_id,custom_cursor_name\tSets the mouse cursor. resource_id is a value like 32512 (for an arrow cursor), custom_cursor_name is a string description (such as \"arrow\") that will be override the resource_id, if available. In either case resource_id should be nonzero.\0"
#else
  "gfx_setcursor\tresource_id\tSets the mouse cursor. resource_id is a value like 32512 (for an arrow cursor).\0"
#endif
  "gfx_lineto\tx,y[,aa]\tDraws a line from gfx_x,gfx_y to x,y. If aa is 0.5 or greater, then antialiasing is used. Updates gfx_x and gfx_y to x,y.\0"
  "gfx_line\tx,y,x2,y2[,aa]\tDraws a line from x,y to x2,y2, and if aa is not specified or 0.5 or greater, it will be antialiased. \0"
  "gfx_rectto\tx,y\tFills a rectangle from gfx_x,gfx_y to x,y. Updates gfx_x,gfx_y to x,y. \0"
  "gfx_rect\tx,y,w,h[,filled]\tFills a rectangle at x,y, w,h pixels in dimension, filled by default. \0"
  "gfx_setpixel\tr,g,b\tWrites a pixel of r,g,b to gfx_x,gfx_y.\0"
  "gfx_getpixel\tr,g,b\tGets the value of the pixel at gfx_x,gfx_y into r,g,b. \0"
  "gfx_drawnumber\tn,ndigits\tDraws the number n with ndigits of precision to gfx_x, gfx_y, and updates gfx_x to the right side of the drawing. The text height is gfx_texth.\0"
  "gfx_drawchar\tchar\tDraws the character (can be a numeric ASCII code as well), to gfx_x, gfx_y, and moves gfx_x over by the size of the character.\0"
  "gfx_drawstr\t\"str\"[,flags,right,bottom]\tDraws a string at gfx_x, gfx_y, and updates gfx_x/gfx_y so that subsequent draws will occur in a similar place.\n\n"
    "If flags, right ,bottom passed in:\n"
    "\4flags&1: center horizontally\n"
    "\4flags&2: right justify\n"
    "\4flags&4: center vertically\n"
    "\4flags&8: bottom justify\n"
    "\4flags&256: ignore right/bottom, otherwise text is clipped to (gfx_x, gfx_y, right, bottom)\0"
  "gfx_measurestr\t\"str\",&w,&h\tMeasures the drawing dimensions of a string with the current font (as set by gfx_setfont). \0"
  "gfx_measurechar\tcharacter,&w,&h\tMeasures the drawing dimensions of a character with the current font (as set by gfx_setfont). \0"
  "gfx_setfont\tidx[,\"fontface\", sz, flags]\tCan select a font and optionally configure it. idx=0 for default bitmapped font, no configuration is possible for this font. idx=1..16 for a configurable font, specify fontface such as \"Arial\", sz of 8-100, and optionally specify flags, which is a multibyte character, which can include 'i' for italics, 'u' for underline, or 'b' for bold. These flags may or may not be supported depending on the font and OS. After calling gfx_setfont(), gfx_texth may be updated to reflect the new average line height.\0"
  "gfx_getfont\t[#str]\tReturns current font index. If a string is passed, it will receive the actual font face used by this font, if available.\0"
  "gfx_printf\t\"format\"[, ...]\tFormats and draws a string at gfx_x, gfx_y, and updates gfx_x/gfx_y accordingly (the latter only if the formatted string contains newline). For more information on format strings, see sprintf()\0"
  "gfx_blurto\tx,y\tBlurs the region of the screen between gfx_x,gfx_y and x,y, and updates gfx_x,gfx_y to x,y.\0"
  "gfx_blit\tsource,scale,rotation\tIf three parameters are specified, copies the entirity of the source bitmap to gfx_x,gfx_y using current opacity and copy mode (set with gfx_a, gfx_mode). You can specify scale (1.0 is unscaled) and rotation (0.0 is not rotated, angles are in radians).\nFor the \"source\" parameter specify -1 to use the main framebuffer as source, or an image index (see gfx_loadimg()).\0"
  "gfx_blit\tsource, scale, rotation[, srcx, srcy, srcw, srch, destx, desty, destw, desth, rotxoffs, rotyoffs]\t"
                   "srcx/srcy/srcw/srch specify the source rectangle (if omitted srcw/srch default to image size), destx/desty/destw/desth specify dest rectangle (if not specified, these will default to reasonable defaults -- destw/desth default to srcw/srch * scale). \0"
  "gfx_blitext\tsource,coordinatelist,rotation\tDeprecated, use gfx_blit instead.\0"
  "gfx_getimgdim\timage,w,h\tRetreives the dimensions of image (representing a filename: index number) into w and h. Sets these values to 0 if an image failed loading (or if the filename index is invalid).\0"
  "gfx_setimgdim\timage,w,h\tResize image referenced by index 0.." EEL_LICE_DOC_MAXHANDLE ", width and height must be 0-2048. The contents of the image will be undefined after the resize.\0"
  "gfx_loadimg\timage,\"filename\"\tLoad image from filename into slot 0.." EEL_LICE_DOC_MAXHANDLE " specified by image. Returns the image index if success, otherwise -1 if failure. The image will be resized to the dimensions of the image file. \0"
  "gfx_gradrect\tx,y,w,h, r,g,b,a[, drdx, dgdx, dbdx, dadx, drdy, dgdy, dbdy, dady]\tFills a gradient rectangle with the color and alpha specified. drdx-dadx reflect the adjustment (per-pixel) applied for each pixel moved to the right, drdy-dady are the adjustment applied for each pixel moved toward the bottom. Normally drdx=adjustamount/w, drdy=adjustamount/h, etc.\0"
  "gfx_muladdrect\tx,y,w,h,mul_r,mul_g,mul_b[,mul_a,add_r,add_g,add_b,add_a]\tMultiplies each pixel by mul_* and adds add_*, and updates in-place. Useful for changing brightness/contrast, or other effects.\0"
  "gfx_deltablit\tsrcimg,srcs,srct,srcw,srch,destx,desty,destw,desth,dsdx,dtdx,dsdy,dtdy,dsdxdy,dtdxdy[,usecliprect=1]\tBlits from srcimg(srcx,srcy,srcw,srch) "
      "to destination (destx,desty,destw,desth). Source texture coordinates are s/t, dsdx represents the change in s coordinate for each x pixel"
      ", dtdy represents the change in t coordinate for each y pixel, etc. dsdxdy represents the change in dsdx for each line. If usecliprect is specified and 0, then srcw/srch are ignored.\0"
  "gfx_transformblit\tsrcimg,destx,desty,destw,desth,div_w,div_h,table\tBlits to destination at (destx,desty), size (destw,desth). "
      "div_w and div_h should be 2..64, and table should point to a table of 2*div_w*div_h values (this table must not cross a "
      "65536 item boundary). Each pair in the table represents a S,T coordinate in the source image, and the table is treated as "
      "a left-right, top-bottom list of texture coordinates, which will then be rendered to the destination.\0"
  "gfx_circle\tx,y,r[,fill,antialias]\tDraws a circle, optionally filling/antialiasing. \0"
  "gfx_triangle\tx1,y1,x2,y2,x3,y3[x4,y4...]\tDraws a filled triangle, or any convex polygon. \0"
  "gfx_roundrect\tx,y,w,h,radius[,antialias]\tDraws a rectangle with rounded corners. \0"
  "gfx_arc\tx,y,r,ang1,ang2[,antialias]\tDraws an arc of the circle centered at x,y, with ang1/ang2 being specified in radians.\0"
  "gfx_set\tr[,g,b,a,mode,dest,a2]\tSets gfx_r/gfx_g/gfx_b/gfx_a/gfx_mode/gfx_a2, sets gfx_dest if final parameter specified\0"
  "gfx_getdropfile\tidx[,#str]\tEnumerates any drag/dropped files. call gfx_dropfile(-1) to clear the list when finished. Returns 1 if idx is valid, 0 if idx is out of range.\0"

#ifdef EEL_LICE_WANT_STANDALONE
#ifndef EEL_LICE_STANDALONE_NOINITQUIT
  "gfx_clienttoscreen\tx,y\tConverts client coordinates x,y to screen coordinates.\0"
  "gfx_screentoclient\tx,y\tConverts screen coordinates x,y to client coordinates.\0"
#endif
#endif
;
#ifdef EELSCRIPT_LICE_MAX_IMAGES
#undef MKSTR2
#undef MKSTR
#endif
#endif

#endif//_EEL_LICE_H_
