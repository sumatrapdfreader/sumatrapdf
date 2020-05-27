/*
  Cockos WDL - LICE - Lightweight Image Compositing Engine
  Copyright (C) 2007 and later, Cockos Incorporated
  File: lice.cpp (LICE core processing)
  See lice.h for license and other information
*/


#ifndef __LICE_CPP_IMPLEMENTED__
#define __LICE_CPP_IMPLEMENTED__

#include "lice.h"
#include <math.h>
#include <stdio.h> // only included in case we need to debug with sprintf etc

#include "lice_combine.h"
#include "lice_extended.h"

#ifndef _WIN32
#include "../swell/swell.h"
#endif

#define IGNORE_SCALING(mode) ((mode)&LICE_BLIT_IGNORE_SCALING)

#define DO_RECT_SC(mode) \
  const int __sc = (int)dest->Extended(LICE_EXT_GET_SCALING,NULL); \
  if (__sc>0) { \
    if (!IGNORE_SCALING(mode)) { \
      __LICE_SC(x); \
      __LICE_SC(y); \
      __LICE_SCU(w); \
      __LICE_SCU(h); \
    } \
    __LICE_SCU(destbm_w); \
    __LICE_SCU(destbm_h); \
  }



_LICE_ImageLoader_rec *LICE_ImageLoader_list;

LICE_pixel LICE_CombinePixels(LICE_pixel dest, LICE_pixel src, float alpha, int mode)
{
  int r = LICE_GETR(src);
  int g = LICE_GETG(src);
  int b = LICE_GETB(src);
  int a = LICE_GETA(src);
  int al = (int)(alpha*256.0f);

#define __LICE__ACTION(COMBFUNC) COMBFUNC::doPix((LICE_pixel_chan*)&dest,r, g, b, a, al)
  __LICE_ACTION_SRCALPHA(mode, al,false);
#undef __LICE__ACTION

  return dest;
}


void LICE_CombinePixels2(LICE_pixel *destptr, int r, int g, int b, int a, int ia, int mode)
{
#define __LICE__ACTION(COMBFUNC) COMBFUNC::doPix((LICE_pixel_chan*)destptr,r, g, b, a, ia)
  __LICE_ACTION_SRCALPHA(mode, ia, false);
#undef __LICE__ACTION
}
void LICE_CombinePixels2Clamp(LICE_pixel *destptr, int r, int g, int b, int a, int ia, int mode)
{
#define __LICE__ACTION(COMBFUNC) COMBFUNC::doPix((LICE_pixel_chan*)destptr,r, g, b, a, ia)
  __LICE_ACTION_SRCALPHA(mode, ia, true);
#undef __LICE__ACTION
}


LICE_MemBitmap::LICE_MemBitmap(int w, int h, unsigned int linealign)
{
  m_allocsize=0;
  m_fb=0;
  m_width=0;
  m_height=0;
  m_linealign = linealign > 1 ? ((linealign & ~(linealign-1))-1) : 0; // force to be contiguous bits
  if (m_linealign>16) m_linealign=16;
  if (w>0&&h>0) __resize(w,h);
}

LICE_MemBitmap::~LICE_MemBitmap() { free(m_fb); }

bool LICE_MemBitmap::__resize(int w, int h)
{
  if (w!=m_width||h!=m_height)
  {
#ifdef DEBUG_TIGHT_ALLOC // dont enable for anything you want to be even remotely fast
    free(m_fb);
    m_fb = (LICE_pixel *)malloc((m_allocsize = ((w+m_linealign)&~m_linealign)*h*sizeof(LICE_pixel)) + LICE_MEMBITMAP_ALIGNAMT);
    m_width=m_fb?w:0;
    m_height=m_fb?h:0;
    return true;
#endif
    int sz=(((m_width=w)+m_linealign)&~m_linealign)*(m_height=h)*sizeof(LICE_pixel);

    if (sz<=0||w<1||h<1) { free(m_fb); m_fb=0; m_allocsize=0; }
    else if (!m_fb) m_fb=(LICE_pixel*)malloc((m_allocsize=sz) + LICE_MEMBITMAP_ALIGNAMT);
    else 
    {
      if (sz>m_allocsize)
      {
        void *op=m_fb;
        if (!(m_fb=(LICE_pixel*)realloc(m_fb,(m_allocsize=sz+sz/4)+LICE_MEMBITMAP_ALIGNAMT)))
        {
          free(op);
          m_fb=(LICE_pixel*)malloc((m_allocsize=sz)+LICE_MEMBITMAP_ALIGNAMT);
        }
      }
    }
    if (!m_fb) {m_width=m_height=0; }

    return true;
  }
  return false;
}



#ifndef _LICE_NO_SYSBITMAPS_

LICE_SysBitmap::LICE_SysBitmap(int w, int h)
{
  m_allocw=m_alloch=0;
#ifdef _WIN32
  m_dc = CreateCompatibleDC(NULL);
  m_bitmap = 0;
  m_oldbitmap = 0;
#else
  m_dc=0;
#endif
  m_bits=0;
  m_width=m_height=0;
  m_adv_scaling=0;
  m_draw_scaling=0;

  __resize(w,h);
}


LICE_SysBitmap::~LICE_SysBitmap()
{
#ifdef _WIN32
  if (m_oldbitmap && m_dc) SelectObject(m_dc,m_oldbitmap);
  if (m_bitmap) DeleteObject(m_bitmap);
  if (m_dc) DeleteDC(m_dc);
#else
  if (m_dc)
    SWELL_DeleteGfxContext(m_dc);
#endif
}

bool LICE_SysBitmap::__resize(int w, int h)
{
#ifdef _WIN32
  if (!m_dc) { m_width=m_height=0; m_bits=0; return false; }
#endif

  if (m_width==w && m_height == h) return false;

  m_width=w;
  m_height=h;

  if (m_draw_scaling > 0)
  {
    w = (w * m_draw_scaling) >> 8;
    h = (h * m_draw_scaling) >> 8;
  }
  w = (w+3)&~3; // always keep backing store a multiple of 4px wide

#ifndef DEBUG_TIGHT_ALLOC
  // dont resize down bitmaps
  if (w && h && w <= m_allocw && h <= m_alloch && m_bits) 
  {
#ifndef _WIN32
    if (isFlipped())
    {
      m_bits=(LICE_pixel*)SWELL_GetCtxFrameBuffer(m_dc);
      m_bits += (m_alloch-h)*m_allocw;
    }
#endif
    return true;
  }
#endif//!DEBUG_TIGHT_ALLOC

  m_allocw=w;
  m_alloch=h;

#ifdef _WIN32
  if (m_oldbitmap) 
  {
    SelectObject(m_dc,m_oldbitmap);
    m_oldbitmap=0;
  }
  if (m_bitmap) DeleteObject(m_bitmap);
  m_bitmap=0;
  m_bits=0;


  if (w<1 || h<1) return false;

  BITMAPINFO pbmInfo = {0,};
  pbmInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  pbmInfo.bmiHeader.biWidth = w;
  pbmInfo.bmiHeader.biHeight = isFlipped()?h:-h;
  pbmInfo.bmiHeader.biPlanes = 1;
  pbmInfo.bmiHeader.biBitCount = sizeof(LICE_pixel)*8;
  pbmInfo.bmiHeader.biCompression = BI_RGB;
  m_bitmap = CreateDIBSection( NULL, &pbmInfo, DIB_RGB_COLORS, (void **)&m_bits, NULL, 0);

  if (m_bitmap) m_oldbitmap=SelectObject(m_dc, m_bitmap);
  else { m_width=m_height=0; m_bits=0; }
#else
  if (m_dc) SWELL_DeleteGfxContext(m_dc);
  m_dc=0;
  m_bits=0;

  if (w<1 || h<1) return false;

  m_dc=SWELL_CreateMemContext(0,w,h);
  if (!m_dc) { m_width=m_height=0; m_bits=0; }
  else m_bits=(LICE_pixel*)SWELL_GetCtxFrameBuffer(m_dc);
#endif

  return true;
}

#endif // _LICE_NO_SYSBITMAPS_



#ifndef LICE_NO_BLIT_SUPPORT
void LICE_Copy(LICE_IBitmap *dest, LICE_IBitmap *src) // resizes dest
{
  if (src&&dest)
  {
    dest->resize(src->getWidth(),src->getHeight());
    LICE_Blit(dest,src,0,0,NULL,1.0,LICE_BLIT_MODE_COPY);
  }
}
#endif

template<class COMBFUNC> class _LICE_Template_Blit0 // these always templated
{
  public:
    static void solidBlitFAST(LICE_pixel *dest, int w, int h, 
                         LICE_pixel color,
                         int dest_span)
    {
      while (h--)
      {
        LICE_pixel *pout=dest;
        int n=w;
        while (n--)
        {
          COMBFUNC::doPixFAST(pout,color);          
          ++pout;
        }
        dest+=dest_span;
      }
    }



    static void scaleBlitFAST(LICE_pixel_chan *dest, const LICE_pixel_chan *src, int w, int h, 
                          int icurx, int icury, int idx, int idy, unsigned int clipright, unsigned int clipbottom,     
                          int src_span, int dest_span)
    {
      LICE_pixel* destpx = (LICE_pixel*) dest;
      int destpxspan = dest_span*(int)sizeof(LICE_pixel_chan)/(int)sizeof(LICE_pixel);

      while (h--)
      {
        const unsigned int cury = icury >> 16;
        if (cury < clipbottom)
        {
          int curx=icurx;
          const LICE_pixel_chan *inptr=src + cury * src_span;
          LICE_pixel* pout = destpx;
          int n=w;
          while (n--)
          {
            const unsigned int offs=curx >> 16;
            if (offs<clipright)
            {
              COMBFUNC::doPixFAST(pout,((LICE_pixel *)inptr)[offs]);
            }

            ++pout;
            curx+=idx;
          }
        }
        destpx += destpxspan;
        icury+=idy;
      }
    }

};

#ifndef LICE_FAVOR_SIZE_EXTREME
template<class COMBFUNC> 
#endif
class _LICE_Template_Blit1 // these controlled by LICE_FAVOR_SIZE_EXTREME
{
#ifdef LICE_FAVOR_SIZE_EXTREME
    #define DOPIX(pout,r,g,b,a,ia) combFunc(pout,r,g,b,a,ia);
#else
    #define DOPIX(pout,r,g,b,a,ia) COMBFUNC::doPix(pout,r,g,b,a,ia);
#endif
  public:
    static void solidBlit(LICE_pixel_chan *dest, int w, int h, 
                         int ir, int ig, int ib, int ia,
                         int dest_span
#ifdef LICE_FAVOR_SIZE_EXTREME
                          , LICE_COMBINEFUNC combFunc
#endif
                         )
    {
      while (h--)
      {
        LICE_pixel_chan *pout=dest;
        int n=w;
        while (n--)
        {
          DOPIX(pout,ir,ig,ib,ia,ia);          
          pout += sizeof(LICE_pixel)/sizeof(LICE_pixel_chan);
        }
        dest+=dest_span;
      }
    }
    static void gradBlit(LICE_pixel_chan *dest, int w, int h, 
                         int ir, int ig, int ib, int ia,
                         int drdx, int dgdx, int dbdx, int dadx,
                         int drdy, int dgdy, int dbdy, int dady,
                         int dest_span
#ifdef LICE_FAVOR_SIZE_EXTREME
                          , LICE_COMBINEFUNC combFunc
#endif
                         )
    {
      while (h--)
      {
        int r=ir,g=ig,b=ib,a=ia;
        ir+=drdy; ig+=dgdy; ib+=dbdy; ia+=dady;
        LICE_pixel_chan *pout=dest;
        int n=w;
        while (n--)
        {
          const int aa=a/65536;
          DOPIX(pout,r/65536,g/65536,b/65536,aa,aa);
          pout += sizeof(LICE_pixel)/sizeof(LICE_pixel_chan);
          r+=drdx; g+=dgdx; b+=dbdx; a+=dadx;
        }
        dest+=dest_span;
      }
    }

#undef DOPIX
};


#ifndef LICE_FAVOR_SIZE
template<class COMBFUNC> 
#endif
class _LICE_Template_Blit2 // these controlled by LICE_FAVOR_SIZE
{
#ifdef LICE_FAVOR_SIZE
  #define DOPIX(pout,r,g,b,a,ia) combFunc(pout,r,g,b,a,ia);
#else
    #define DOPIX(pout,r,g,b,a,ia) COMBFUNC::doPix(pout,r,g,b,a,ia);
#endif

  public:

    static void blit(LICE_pixel_chan *dest, const LICE_pixel_chan *src, int w, int h, int src_span, int dest_span, int ia
#ifdef LICE_FAVOR_SIZE
                          , LICE_COMBINEFUNC combFunc
#endif
      )
    {
      while (h-->0)
      {
        int n=w;
        const LICE_pixel_chan *pin=src;
        LICE_pixel_chan *pout=dest;
        while (n--)
        {

          DOPIX(pout,pin[LICE_PIXEL_R],pin[LICE_PIXEL_G],pin[LICE_PIXEL_B],pin[LICE_PIXEL_A],ia);

          pin += sizeof(LICE_pixel)/sizeof(LICE_pixel_chan);
          pout += sizeof(LICE_pixel)/sizeof(LICE_pixel_chan);
        }
        dest+=dest_span;
        src += src_span;
      }
    }

    // this is only designed for filtering down an image approx 2:1 to 4:1 or so.. it'll work (poortly) for higher, and for less it's crap too.
    // probably need to redo it using linear interpolation of the filter coefficients, but bleh I'm gonna go play some call of duty
    static void scaleBlitFilterDown(LICE_pixel_chan *dest, const LICE_pixel_chan *src, int w, int h, 
                          int icurx, int icury, int idx, int idy, int clipright, int clipbottom,     
                          int src_span, int dest_span, int ia, const int *filter, int filt_start, int filtsz
#ifdef LICE_FAVOR_SIZE
                          , LICE_COMBINEFUNC combFunc
#endif
                          
                          )
    {

      while (h--)
      {
        int cury = icury >> 16;
        int curx=icurx;          
        int n=w;
        if (cury >= 0 && cury < clipbottom)
        {
          const LICE_pixel_chan *inptr=src + (cury+filt_start) * src_span;
          LICE_pixel_chan *pout=dest;
          while (n--)
          {
            int offs=curx >> 16;
            if (offs>=0 && offs<clipright)
            {
              int r=0,g=0,b=0,a=0;
              int sc=0;
              int fy=filtsz;
              int ypos=cury+filt_start;
              const LICE_pixel_chan *rdptr  = inptr + (offs + filt_start)*(int) (sizeof(LICE_pixel)/sizeof(LICE_pixel_chan));
              const int *scaletab = filter;
              while (fy--)
              {
                if (ypos >= clipbottom) break;

                if (ypos >= 0)
                {
                  int xpos=offs + filt_start;
                  const LICE_pixel_chan *pin = rdptr;
                  int fx=filtsz;
                  while (fx--)
                  {
                    int tsc = *scaletab++;
                    if (xpos >= 0 && xpos < clipright)
                    {
                      r+=pin[LICE_PIXEL_R]*tsc;
                      g+=pin[LICE_PIXEL_G]*tsc;
                      b+=pin[LICE_PIXEL_B]*tsc;
                      a+=pin[LICE_PIXEL_A]*tsc;
                      sc+=tsc;
                    }
                    xpos++;
                    pin+=sizeof(LICE_pixel)/sizeof(LICE_pixel_chan);
                  }
                }
                else scaletab += filtsz;

                ypos++;
                rdptr+=src_span;
              }
              if (sc>0)
              {
                DOPIX(pout,r/sc,g/sc,b/sc,a/sc,ia);
              }
            }

            pout += sizeof(LICE_pixel)/sizeof(LICE_pixel_chan);
            curx+=idx;
          }
        }
        dest+=dest_span;
        icury+=idy;
      }
    }
    static void scaleBlit(LICE_pixel_chan *dest, const LICE_pixel_chan *src, int w, int h, 
                          int icurx, int icury, int idx, int idy, unsigned int clipright, unsigned int clipbottom,     
                          int src_span, int dest_span, int ia, int filtermode
#ifdef LICE_FAVOR_SIZE
                          , LICE_COMBINEFUNC combFunc
#endif
                          
                          )
    {

      if (filtermode == LICE_BLIT_FILTER_BILINEAR)
      {
        while (h--)
        {
          const unsigned int cury = icury >> 16;
          const int yfrac=icury&65535;
          int curx=icurx;          
          const LICE_pixel_chan *inptr=src + cury * src_span;
          LICE_pixel_chan *pout=dest;
          int n=w;
          if (cury < clipbottom-1)
          {
            while (n--)
            {
              const unsigned int offs=curx >> 16;
              const LICE_pixel_chan *pin = inptr + offs*sizeof(LICE_pixel);
              if (offs<clipright-1)
              {
                int r,g,b,a;
                __LICE_BilinearFilterI(&r,&g,&b,&a,pin,pin+src_span,curx&0xffff,yfrac);
                DOPIX(pout,r,g,b,a,ia)
              }
              else if (offs==clipright-1)
              {
                int r,g,b,a;
                __LICE_LinearFilterI(&r,&g,&b,&a,pin,pin+src_span,yfrac);
                DOPIX(pout,r,g,b,a,ia)
              }

              pout += sizeof(LICE_pixel)/sizeof(LICE_pixel_chan);
              curx+=idx;
            }
          }
          else if (cury == clipbottom-1)
          {
            while (n--)
            {
              const unsigned int offs=curx >> 16;
              const LICE_pixel_chan *pin = inptr + offs*sizeof(LICE_pixel);
              if (offs<clipright-1)
              {
                int r,g,b,a;
                __LICE_LinearFilterI(&r,&g,&b,&a,pin,pin+sizeof(LICE_pixel)/sizeof(LICE_pixel_chan),curx&0xffff);
                DOPIX(pout,r,g,b,a,ia)
              }
              else if (offs==clipright-1)
              {
                DOPIX(pout,pin[LICE_PIXEL_R],pin[LICE_PIXEL_G],pin[LICE_PIXEL_B],pin[LICE_PIXEL_A],ia)
              }

              pout += sizeof(LICE_pixel)/sizeof(LICE_pixel_chan);
              curx+=idx;
            }
          }
          dest+=dest_span;
          icury+=idy;
        }
      }
      else
      {
        while (h--)
        {
          const unsigned int cury = icury >> 16;
          if (cury < clipbottom)
          {
            int curx=icurx;
            const LICE_pixel_chan *inptr=src + cury * src_span;
            LICE_pixel_chan *pout=dest;
            int n=w;
            while (n--)
            {
              const unsigned int offs=curx >> 16;
              if (offs<clipright)
              {
                const LICE_pixel_chan *pin = inptr + offs*sizeof(LICE_pixel);

                DOPIX(pout,pin[LICE_PIXEL_R],pin[LICE_PIXEL_G],pin[LICE_PIXEL_B],pin[LICE_PIXEL_A],ia)
              }

              pout += sizeof(LICE_pixel)/sizeof(LICE_pixel_chan);
              curx+=idx;
            }
          }
          dest+=dest_span;
          icury+=idy;
        }
      }
    }


#undef DOPIX
};

#ifdef LICE_FAVOR_SPEED
template<class COMBFUNC> 
#endif
class _LICE_Template_Blit3 // stuff controlled by LICE_FAVOR_SPEED
{
#ifndef LICE_FAVOR_SPEED
    #define DOPIX(pout,r,g,b,a,ia) combFunc(pout,r,g,b,a,ia);
#else
    #define DOPIX(pout,r,g,b,a,ia) COMBFUNC::doPix(pout,r,g,b,a,ia);
#endif

  public:

    static void deltaBlit(LICE_pixel_chan *dest, const LICE_pixel_chan *src, int w, int h, 
                          int isrcx, int isrcy, int idsdx, int idtdx, int idsdy, int idtdy,
                          int idsdxdy, int idtdxdy,
                          unsigned int src_right, unsigned int src_bottom,
                          int src_span, int dest_span, int ia, int filtermode
#ifndef LICE_FAVOR_SPEED
                          , LICE_COMBINEFUNC combFunc
#endif
                          )
    {
      if (filtermode == LICE_BLIT_FILTER_BILINEAR)
      {
        while (h--)
        {
          int thisx=isrcx;
          int thisy=isrcy;
          LICE_pixel_chan *pout=dest;
          int n=w;
          while (n--)
          {
            const unsigned int cury = thisy >> 16;
            const unsigned int curx = thisx >> 16;
            if (cury < src_bottom-1)
            {
              if (curx < src_right-1)
              {
                const LICE_pixel_chan *pin = src + cury * src_span + curx*sizeof(LICE_pixel);
                int r,g,b,a;

                __LICE_BilinearFilterI(&r,&g,&b,&a,pin,pin+src_span,thisx&65535,thisy&65535);

                DOPIX(pout,r,g,b,a,ia);
              }
              else if (curx==src_right-1)
              {

                const LICE_pixel_chan *pin = src + cury * src_span + curx*sizeof(LICE_pixel);
                int r,g,b,a;

                __LICE_LinearFilterI(&r,&g,&b,&a,pin,pin+src_span,thisy&65535);
                DOPIX(pout,r,g,b,a,ia);
              }
            }
            else if (cury==src_bottom-1)
            {
              if (curx<src_right-1)
              {
                const LICE_pixel_chan *pin = src + cury * src_span + curx*sizeof(LICE_pixel);

                int r,g,b,a;

                __LICE_LinearFilterI(&r,&g,&b,&a,pin,pin+sizeof(LICE_pixel)/sizeof(LICE_pixel_chan),thisx&65535);

                DOPIX(pout,r,g,b,a,ia);
              }
              else if (curx==src_right-1)
              {
                const LICE_pixel_chan *pin = src + cury * src_span + curx*sizeof(LICE_pixel);
                DOPIX(pout,pin[LICE_PIXEL_R],pin[LICE_PIXEL_G],pin[LICE_PIXEL_B],pin[LICE_PIXEL_A],ia);
              }
            }

            pout += sizeof(LICE_pixel)/sizeof(LICE_pixel_chan);
            thisx+=idsdx;
            thisy+=idtdx;
          }
          idsdx+=idsdxdy;
          idtdx+=idtdxdy;
          isrcx+=idsdy;
          isrcy+=idtdy;
          dest+=dest_span;
        }
      }
      else
      {
        while (h--)
        {
          int thisx=isrcx;
          int thisy=isrcy;
          LICE_pixel_chan *pout=dest;
          int n=w;
          while (n--)
          {
            const unsigned int cury = thisy >> 16;
            const unsigned int curx = thisx >> 16;
            if (cury < src_bottom && curx < src_right)
            {
              const LICE_pixel_chan *pin = src + cury * src_span + curx*sizeof(LICE_pixel);

              DOPIX(pout,pin[LICE_PIXEL_R],pin[LICE_PIXEL_G],pin[LICE_PIXEL_B],pin[LICE_PIXEL_A],ia);
            }

            pout += sizeof(LICE_pixel)/sizeof(LICE_pixel_chan);
            thisx+=idsdx;
            thisy+=idtdx;
          }
          idsdx+=idsdxdy;
          idtdx+=idtdxdy;
          isrcx+=idsdy;
          isrcy+=idtdy;
          dest+=dest_span;
        }
      }
    }

#undef DOPIX
};

#ifdef LICE_FAVOR_SPEED
template<class COMBFUNC> 
#endif
class _LICE_Template_Blit4 // stuff controlled by LICE_FAVOR_SPEED
{
#ifndef LICE_FAVOR_SPEED
    #define DOPIX(pout,r,g,b,a,ia) combFunc(pout,r,g,b,a,ia);
#else
    #define DOPIX(pout,r,g,b,a,ia) COMBFUNC::doPix(pout,r,g,b,a,ia);
#endif

  public:

    static void deltaBlitAlpha(LICE_pixel_chan *dest, const LICE_pixel_chan *src, int w, int h, 
                          int isrcx, int isrcy, int idsdx, int idtdx, int idsdy, int idtdy,
                          int idsdxdy, int idtdxdy,
                          unsigned int src_right, unsigned int src_bottom,
                          int src_span, int dest_span, int ia, int idadx, int idady, int idadxdy, int filtermode
#ifndef LICE_FAVOR_SPEED
                          , LICE_COMBINEFUNC combFunc
#endif
                          )
    {
      if (filtermode == LICE_BLIT_FILTER_BILINEAR)
      {
        while (h--)
        {
          int thisx=isrcx;
          int thisy=isrcy;
          int thisa=ia;
          LICE_pixel_chan *pout=dest;
          int n=w;
          while (n--)
          {
            const unsigned int cury = thisy >> 16;
            const unsigned int curx = thisx >> 16;
            if (cury < src_bottom-1)
            {
              if (curx < src_right-1)
              {
                const LICE_pixel_chan *pin = src + cury * src_span + curx*sizeof(LICE_pixel);
                int r,g,b,a;

                __LICE_BilinearFilterI(&r,&g,&b,&a,pin,pin+src_span,thisx&65535,thisy&65535);

                DOPIX(pout,r,g,b,a,thisa>>8);
              }
              else if (curx==src_right-1)
              {

                const LICE_pixel_chan *pin = src + cury * src_span + curx*sizeof(LICE_pixel);
                int r,g,b,a;

                __LICE_LinearFilterI(&r,&g,&b,&a,pin,pin+src_span,thisy&65535);
                DOPIX(pout,r,g,b,a,thisa>>8);
              }
            }
            else if (cury==src_bottom-1)
            {
              if (curx<src_right-1)
              {
                const LICE_pixel_chan *pin = src + cury * src_span + curx*sizeof(LICE_pixel);

                int r,g,b,a;

                __LICE_LinearFilterI(&r,&g,&b,&a,pin,pin+sizeof(LICE_pixel)/sizeof(LICE_pixel_chan),thisx&65535);

                DOPIX(pout,r,g,b,a,thisa>>8);
              }
              else if (curx==src_right-1)
              {
                const LICE_pixel_chan *pin = src + cury * src_span + curx*sizeof(LICE_pixel);
                DOPIX(pout,pin[LICE_PIXEL_R],pin[LICE_PIXEL_G],pin[LICE_PIXEL_B],pin[LICE_PIXEL_A],thisa>>8);
              }
            }

            pout += sizeof(LICE_pixel)/sizeof(LICE_pixel_chan);
            thisx+=idsdx;
            thisy+=idtdx;
            thisa+=idadx;
          }
          idsdx+=idsdxdy;
          idtdx+=idtdxdy;
          idadx+=idadxdy;
          isrcx+=idsdy;
          isrcy+=idtdy;
          ia += idady;
          dest+=dest_span;
        }
      }
      else
      {
        while (h--)
        {
          int thisx=isrcx;
          int thisy=isrcy;
          int thisa=ia;
          LICE_pixel_chan *pout=dest;
          int n=w;
          while (n--)
          {
            const unsigned int cury = thisy >> 16;
            const unsigned int curx = thisx >> 16;
            if (cury < src_bottom && curx < src_right)
            {
              const LICE_pixel_chan *pin = src + cury * src_span + curx*sizeof(LICE_pixel);

              DOPIX(pout,pin[LICE_PIXEL_R],pin[LICE_PIXEL_G],pin[LICE_PIXEL_B],pin[LICE_PIXEL_A],thisa>>8);
            }

            pout += sizeof(LICE_pixel)/sizeof(LICE_pixel_chan);
            thisx+=idsdx;
            thisy+=idtdx;
            thisa+=idadx;
          }
          idsdx+=idsdxdy;
          idtdx+=idtdxdy;
          idadx+=idadxdy;
          isrcx+=idsdy;
          isrcy+=idtdy;
          ia+=idady;
          dest+=dest_span;
        }
      }
    }

#undef DOPIX
};



#ifndef LICE_NO_GRADIENT_SUPPORT

void LICE_GradRect(LICE_IBitmap *dest, int dstx, int dsty, int dstw, int dsth, 
                      float ir, float ig, float ib, float ia,
                      float drdx, float dgdx, float dbdx, float dadx,
                      float drdy, float dgdy, float dbdy, float dady,
                      int mode)
{
  if (!dest) return;

  ir*=255.0; ig*=255.0; ib*=255.0; ia*=256.0;
  drdx*=255.0; dgdx*=255.0; dbdx*=255.0; dadx*=256.0;
  drdy*=255.0; dgdy*=255.0; dbdy*=255.0; dady*=256.0;
  // dont scale alpha

  // clip to output
  if (dstx < 0) 
  { 
    ir-=dstx*drdx; ig-=dstx*dgdx; ib-=dstx*dbdx; ia-=dstx*dadx; 
    dstw+=dstx; 
    dstx=0; 
  }
  if (dsty < 0) 
  {
    ir -= dsty*drdy; ig-=dsty*dgdy; ib -= dsty*dbdy; ia -= dsty*dady;
    dsth += dsty; 
    dsty=0; 
  }  

  int dest_span=dest->getRowSpan()*sizeof(LICE_pixel);
  LICE_pixel_chan *pdest = (LICE_pixel_chan *)dest->getBits();

  const int destbm_w = dest->getWidth(), destbm_h = dest->getHeight();

  if (!pdest || !dest_span || dstw < 1 || dsth < 1 || dstx >= destbm_w || dsty >= destbm_h) return;

  if (dstw > destbm_w-dstx) dstw = destbm_w-dstx;
  if (dsth > destbm_h-dsty) dsth = destbm_h-dsty;
 
  if (dest->isFlipped())
  {
    pdest += (destbm_h-dsty - 1)*dest_span;
    dest_span=-dest_span;
  }
  else
  {
    pdest += dsty*dest_span;
  }
  pdest+=dstx*sizeof(LICE_pixel);
#define TOFIX(a) ((int)((a)*65536.0))

  int iir=TOFIX(ir), iig=TOFIX(ig), iib=TOFIX(ib), iia=TOFIX(ia), idrdx=TOFIX(drdx),idgdx=TOFIX(dgdx),idbdx=TOFIX(dbdx),idadx=TOFIX(dadx),
      idrdy=TOFIX(drdy), idgdy=TOFIX(dgdy), idbdy=TOFIX(dbdy), idady=TOFIX(dady);


#ifdef LICE_FAVOR_SIZE_EXTREME
  LICE_COMBINEFUNC blitfunc=NULL;      
  #define __LICE__ACTION(comb) blitfunc=comb::doPix;
#else

  #define __LICE__ACTION(comb) _LICE_Template_Blit1<comb>::gradBlit(pdest,dstw,dsth,iir,iig,iib,iia,idrdx,idgdx,idbdx,idadx,idrdy,idgdy,idbdy,idady,dest_span)
#endif

    // todo: could predict whether or not the colors will ever go out of 0.255 range and optimize

    if ((mode & LICE_BLIT_MODE_MASK)==LICE_BLIT_MODE_COPY && iia==65536 && idady==0 && idadx == 0)
    {
      __LICE__ACTION(_LICE_CombinePixelsClobberClamp);
    }
    else 
    {
      __LICE_ACTION_NOSRCALPHA(mode,256,true);
    }
  #undef __LICE__ACTION

#ifdef LICE_FAVOR_SIZE_EXTREME
   if (blitfunc) _LICE_Template_Blit1::gradBlit(pdest,dstw,dsth,iir,iig,iib,iia,idrdx,idgdx,idbdx,idadx,idrdy,idgdy,idbdy,idady,dest_span,blitfunc);
#endif

#undef TOFIX
}

#endif


#ifndef LICE_NO_BLIT_SUPPORT 

static void LICE_BlitInt(LICE_IBitmap *dest, LICE_IBitmap *src, int dstx, int dsty, const RECT *srcrect, float alpha, int mode, bool allowSc)
{
  if (!dest || !src || !alpha) return;

  int srcbm_w = src->getWidth(), srcbm_h = src->getHeight();
  int destbm_w = dest->getWidth(), destbm_h = dest->getHeight();
  int __sc2 = (int)src->Extended(LICE_EXT_GET_SCALING,NULL);
  if (__sc2 > 0)
  {
    const int __sc = __sc2;
    __LICE_SCU(srcbm_w);
    __LICE_SCU(srcbm_h);
  }

  RECT sr={0,0,srcbm_w, srcbm_h};
  if (srcrect) 
  {
    sr=*srcrect;    
    if (sr.left < 0) { dstx-=sr.left; sr.left=0; }
    if (sr.top < 0) { dsty-=sr.top; sr.top=0; }
    if (sr.right > srcbm_w) sr.right=srcbm_w;
    if (sr.bottom > srcbm_h) sr.bottom=srcbm_h;
  }

  int __sc = (int)dest->Extended(LICE_EXT_GET_SCALING,NULL);
  if (allowSc && __sc != __sc2 && !IGNORE_SCALING(mode))
  {
    const int w = sr.right-sr.left, h=sr.bottom-sr.top;
    LICE_ScaledBlit(dest,src,dstx,dsty,w,h, sr.left,sr.top,w,h,alpha,mode);
    return;
  }
  if (__sc>0)
  {
    __LICE_SCU(destbm_w);
    __LICE_SCU(destbm_h);
    if (!IGNORE_SCALING(mode))
    {
      __LICE_SC(dstx);
      __LICE_SC(dsty);
    }
  }
  if (__sc2>0 && !IGNORE_SCALING(mode))
  {
    __sc=__sc2;
    __LICE_SC(sr.left);
    __LICE_SC(sr.right);
    __LICE_SC(sr.top);
    __LICE_SC(sr.bottom);
  }


  // clip to output
  if (dstx < 0) { sr.left -= dstx; dstx=0; }
  if (dsty < 0) { sr.top -= dsty; dsty=0; }  

  if (sr.left < 0 || sr.top < 0) return; // overflow check
  if (sr.right <= sr.left || sr.bottom <= sr.top || dstx >= destbm_w || dsty >= destbm_h) return;

  if (sr.right > sr.left + (destbm_w-dstx)) sr.right = sr.left + (destbm_w-dstx);
  if (sr.bottom > sr.top + (destbm_h-dsty)) sr.bottom = sr.top + (destbm_h-dsty);

  // ignore blits that are 0
  if (sr.right <= sr.left || sr.bottom <= sr.top) return;

#ifndef DISABLE_LICE_EXTENSIONS
  if (dest->Extended(LICE_EXT_SUPPORTS_ID, (void*) LICE_EXT_BLIT_ACCEL))
  {
    LICE_Ext_Blit_acceldata data(src, dstx, dsty, 
      sr.left, sr.top, 
      sr.right-sr.left, sr.bottom-sr.top, 
        alpha, mode);
    if (dest->Extended(LICE_EXT_BLIT_ACCEL, &data)) return;
  }
#endif


  int dest_span=dest->getRowSpan()*sizeof(LICE_pixel);
  int src_span=src->getRowSpan()*sizeof(LICE_pixel);
  const LICE_pixel_chan *psrc = (LICE_pixel_chan *)src->getBits();
  LICE_pixel_chan *pdest = (LICE_pixel_chan *)dest->getBits();
  if (!psrc || !pdest) return;

  if (src->isFlipped())
  {
    psrc += (src->getHeight()-sr.top - 1)*src_span;
    src_span=-src_span;
  }
  else psrc += sr.top*src_span;
  psrc += sr.left*sizeof(LICE_pixel);

  if (dest->isFlipped())
  {
    pdest += (destbm_h-dsty - 1)*dest_span;
    dest_span=-dest_span;
  }
  else pdest += dsty*dest_span;
  pdest+=dstx*sizeof(LICE_pixel);

  int i=sr.bottom-sr.top;
  int cpsize=sr.right-sr.left;

  if ((mode&LICE_BLIT_MODE_MASK) >= LICE_BLIT_MODE_CHANCOPY && (mode&LICE_BLIT_MODE_MASK) < LICE_BLIT_MODE_CHANCOPY+0x10)
  {
    while (i-->0)
    {
      LICE_pixel_chan *o=pdest+((mode>>2)&3);
      const LICE_pixel_chan *in=psrc+(mode&3);
      int a=cpsize;
      while (a--)
      {
        *o=*in;
        o+=sizeof(LICE_pixel);
        in+=sizeof(LICE_pixel);
      }
      pdest+=dest_span;
      psrc += src_span;
    }
  }
  // special fast case for copy with no source alpha and alpha=1.0 or 0.5
  else if ((mode&(LICE_BLIT_MODE_MASK|LICE_BLIT_USE_ALPHA))==LICE_BLIT_MODE_COPY && (alpha==1.0||alpha==0.5))
  {
    if (alpha==0.5)
    {
      while (i-->0)
      {
        int a=cpsize;
        const LICE_pixel *rd = (LICE_pixel *)psrc;
        LICE_pixel *wr = (LICE_pixel *)pdest;
        while (a-->0)
        {
          *wr = ((*wr>>1)&0x7f7f7f7f)+((*rd++>>1)&0x7f7f7f7f);
          wr++;
        }

        pdest+=dest_span;
        psrc += src_span;
      }
    }
    else
    {
      while (i-->0)
      {
        memmove(pdest,psrc,cpsize*sizeof(LICE_pixel));
        pdest+=dest_span;
        psrc += src_span;
      }
    }
  }
  else 
  {
    int ia=(int)(alpha*256.0);
    #ifdef LICE_FAVOR_SIZE
        LICE_COMBINEFUNC blitfunc=NULL;      
        #define __LICE__ACTION(comb) blitfunc=comb::doPix;
    #else
        #define __LICE__ACTION(comb) _LICE_Template_Blit2<comb>::blit(pdest,psrc,cpsize,i,src_span,dest_span,ia)
    #endif
      
        __LICE_ACTION_SRCALPHA(mode,ia,false);
    
    #undef __LICE__ACTION

    #ifdef LICE_FAVOR_SIZE
        if (blitfunc) _LICE_Template_Blit2::blit(pdest,psrc,cpsize,i,src_span,dest_span,ia,blitfunc);
    #endif
  }
}

void LICE_Blit(LICE_IBitmap *dest, LICE_IBitmap *src, int dstx, int dsty, const RECT *srcrect, float alpha, int mode)
{
  LICE_BlitInt(dest,src,dstx,dsty,srcrect,alpha,mode,true);
}

void LICE_Blit(LICE_IBitmap *dest, LICE_IBitmap *src, int dstx, int dsty, int srcx, int srcy, int srcw, int srch, float alpha, int mode)
{
  RECT r={srcx,srcy,srcx+srcw,srcy+srch};
  LICE_BlitInt(dest,src,dstx,dsty,&r,alpha,mode,true);
}

#endif

#ifndef LICE_NO_BLUR_SUPPORT

void LICE_Blur(LICE_IBitmap *dest, LICE_IBitmap *src, int dstx, int dsty, int srcx, int srcy, int srcw, int srch) // src and dest can overlap, however it may look fudgy if they do
{
  if (!dest || !src) return;

  RECT sr={srcx,srcy,srcx+srcw,srcy+srch};
  if (sr.left < 0) sr.left=0;
  if (sr.top < 0) sr.top=0;
  if (sr.right > src->getWidth()) sr.right=src->getWidth();
  if (sr.bottom > src->getHeight()) sr.bottom = src->getHeight();

  // clip to output
  if (dstx < 0) { sr.left -= dstx; dstx=0; }
  if (dsty < 0) { sr.top -= dsty; dsty=0; }

  if (sr.left < 0 || sr.top < 0) return; // overflow check

  const int destbm_w = dest->getWidth(), destbm_h = dest->getHeight();
  if (sr.right <= sr.left || sr.bottom <= sr.top || dstx >= destbm_w || dsty >= destbm_h) return;

  if (sr.right > sr.left + (destbm_w-dstx)) sr.right = sr.left + (destbm_w-dstx);
  if (sr.bottom > sr.top + (destbm_h-dsty)) sr.bottom = sr.top + (destbm_h-dsty);

  // ignore blits that are smaller than 2x2
  if (sr.right <= sr.left+1 || sr.bottom <= sr.top+1) return;

  int dest_span=dest->getRowSpan();
  int src_span=src->getRowSpan();
  const LICE_pixel *psrc = (LICE_pixel *)src->getBits();
  LICE_pixel *pdest = (LICE_pixel *)dest->getBits();
  if (!psrc || !pdest) return;

  if (src->isFlipped())
  {
    psrc += (src->getHeight()-sr.top - 1)*src_span;
    src_span=-src_span;
  }
  else psrc += sr.top*src_span;
  psrc += sr.left;

  if (dest->isFlipped())
  {
    pdest += (destbm_h-dsty - 1)*dest_span;
    dest_span=-dest_span;
  }
  else pdest += dsty*dest_span;
  pdest+=dstx;

  LICE_pixel *tmpbuf=NULL;
  int w=sr.right-sr.left;  

  // buffer to save the last unprocessed lines for the cases where blurring from a bitmap to itself
  LICE_pixel turdbuf[2048];
  if (src==dest)
  {
    if (w <= (int) (sizeof(turdbuf)/sizeof(turdbuf[0])/2)) tmpbuf=turdbuf;
    else tmpbuf=(LICE_pixel*)malloc(w*2*sizeof(LICE_pixel));
  }

  int i;
  for (i = sr.top; i < sr.bottom; i ++)
  {
    if (tmpbuf)
      memcpy(tmpbuf+((i&1)?w:0),psrc,w*sizeof(LICE_pixel));

    if (i==sr.top || i==sr.bottom-1)
    {
      const LICE_pixel *psrc2=psrc+(i==sr.top ? src_span : -src_span);

      LICE_pixel lp;

      pdest[0] = LICE_PIXEL_HALF(lp=psrc[0]) + 
                 LICE_PIXEL_QUARTER(psrc[1]) + 
                 LICE_PIXEL_QUARTER(psrc2[0]);
      int x;
      for (x = 1; x < w-1; x ++)
      {
        LICE_pixel tp;
        pdest[x] = LICE_PIXEL_HALF(tp=psrc[x]) + 
                   LICE_PIXEL_QUARTER(psrc2[x]) + 
                   LICE_PIXEL_EIGHTH(psrc[x+1]) +
                   LICE_PIXEL_EIGHTH(lp);
        lp=tp;
      }
      pdest[x] = LICE_PIXEL_HALF(psrc[x]) + 
                   LICE_PIXEL_QUARTER(lp) + 
                   LICE_PIXEL_QUARTER(psrc2[x]);
    }
    else
    {
      const LICE_pixel *psrc2=psrc-src_span;
      const LICE_pixel *psrc3=psrc+src_span;
      if (tmpbuf)
        psrc2=tmpbuf + ((i&1) ? 0 : w);

      LICE_pixel lp;
      pdest[0] = LICE_PIXEL_HALF(lp=psrc[0]) + 
                 LICE_PIXEL_QUARTER(psrc[1]) +
                 LICE_PIXEL_EIGHTH(psrc2[0]) +
                 LICE_PIXEL_EIGHTH(psrc3[0]);
      int x;
      for (x = 1; x < w-1; x ++)
      {
        LICE_pixel tp;
        pdest[x] = LICE_PIXEL_HALF(tp=psrc[x]) +
                   LICE_PIXEL_EIGHTH(psrc[x+1]) +
                   LICE_PIXEL_EIGHTH(lp) +
                   LICE_PIXEL_EIGHTH(psrc2[x]) + 
                   LICE_PIXEL_EIGHTH(psrc3[x]);
        lp=tp;
      }
      pdest[x] = LICE_PIXEL_HALF(psrc[x]) + 
                 LICE_PIXEL_QUARTER(lp) + 
                 LICE_PIXEL_EIGHTH(psrc2[x]) +
                 LICE_PIXEL_EIGHTH(psrc3[x]);
    }
    pdest+=dest_span;
    psrc += src_span;
  }
  if (tmpbuf && tmpbuf != turdbuf)
    free(tmpbuf);
}

#endif

#ifndef LICE_NO_BLIT_SUPPORT
void LICE_ScaledBlit(LICE_IBitmap *dest, LICE_IBitmap *src, 
                     int dstx, int dsty, int dstw, int dsth, 
                     float srcx, float srcy, float srcw, float srch, 
                     float alpha, int mode)
{
  if (!dest || !src || !dstw || !dsth || !alpha) return;

  int srcbm_w = src->getWidth(), srcbm_h = src->getHeight();
  int destbm_w = dest->getWidth(), destbm_h = dest->getHeight();
  int __sc = (int)dest->Extended(LICE_EXT_GET_SCALING,NULL);
  const float srcx_orig = srcx, srcy_orig = srcy, srcw_orig = srcw, srch_orig = srch;
  const int dstx_orig = dstx, dsty_orig = dsty;
  if (__sc>0)
  {
    if (!IGNORE_SCALING(mode))
    {
      __LICE_SC(dstx);
      __LICE_SC(dsty);
      __LICE_SC(dstw);
      __LICE_SC(dsth);
    }
    __LICE_SCU(destbm_w);
    __LICE_SCU(destbm_h);
  }
  __sc = (int)src->Extended(LICE_EXT_GET_SCALING,NULL);
  if (__sc>0)
  {
    if (!IGNORE_SCALING(mode))
    {
      __LICE_SC(srcx);
      __LICE_SC(srcy);
      __LICE_SC(srcw);
      __LICE_SC(srch);
    }
    __LICE_SCU(srcbm_w);
    __LICE_SCU(srcbm_h);
  }

  // non-scaling optimized omde
  if (fabs(srcw-dstw)<0.001 && fabs(srch-dsth)<0.001)
  {
    // and if not bilinear filtering, or 
    // the source coordinates are near their integer counterparts
    if ((mode&LICE_BLIT_FILTER_MASK)!=LICE_BLIT_FILTER_BILINEAR ||
        (fabs(srcx-floor(srcx+0.5f))<0.03 && fabs(srcy-floor(srcy+0.5f))<0.03))
    {
      RECT sr={(int)(srcx_orig+0.5f),(int)(srcy_orig+0.5f),};
      sr.right=sr.left+(int) (srcw_orig+0.5);
      sr.bottom=sr.top+(int) (srch_orig+0.5);
      LICE_BlitInt(dest,src,dstx_orig,dsty_orig,&sr,alpha,mode,false);
      return;
    }
  }

#ifndef DISABLE_LICE_EXTENSIONS
  if (dest->Extended(LICE_EXT_SUPPORTS_ID, (void*) LICE_EXT_SCALEDBLIT_ACCEL))
  {
    LICE_Ext_ScaledBlit_acceldata data(src, dstx, dsty, dstw, dsth, srcx, srcy, srcw, srch, alpha, mode);
    if (dest->Extended(LICE_EXT_SCALEDBLIT_ACCEL, &data)) return;
  }
#endif

  if (dstw<0)
  {
    dstw=-dstw;
    dstx-=dstw;
    srcx+=srcw;  
    srcw=-srcw;
  }
  if (dsth<0)
  {
    dsth=-dsth;
    dsty-=dsth;
    srcy+=srch;
    srch=-srch;
  }

  double xadvance = srcw / dstw;
  double yadvance = srch / dsth;

  int clip_r=(int)(srcx+lice_max(srcw,0)+0.999999); // this rounding logic is shit, but we're stuck with it
  int clip_b=(int)(srcy+lice_max(srch,0)+0.999999);
  if (clip_r>srcbm_w) clip_r=srcbm_w;
  if (clip_b>srcbm_h) clip_b=srcbm_h;


  if (dstx < 0) { srcx -= (float) (dstx*xadvance); dstw+=dstx; dstx=0; }
  if (dsty < 0) { srcy -= (float) (dsty*yadvance); dsth+=dsty; dsty=0; }  
  
  if (dstw < 1 || dsth < 1 || dstx >= destbm_w || dsty >= destbm_h) return;

  if (dstw > destbm_w-dstx) dstw=destbm_w-dstx;
  if (dsth > destbm_h-dsty) dsth=destbm_h-dsty;

  const double fidx=floor(xadvance*65536.0), fidy=floor(yadvance*65536.0);

  double ficurx=floor(srcx*65536.0), ficury=floor(srcy*65536.0);

  // the clip area calculations need to be done fixed point so the results match runtime

  if (fidx>0)
  {
    if (ficurx < 0) // increase dstx, decrease dstw
    {
      int n = (int)((fidx-1-ficurx)/fidx);
      dstw-=n;
      dstx+=n;
      ficurx+=fidx*n;
    }
    if ((ficurx + fidx*(dstw-1)) >= srcbm_w*65536.0)
    {
      int neww = (int)(((srcbm_w-1)*65536.0 - ficurx)/fidx);
      if (neww < dstw) dstw=neww;
    }
  }
  else if (fidx<0)
  {
    // todo: optimize source-clipping with reversed X axis
  }

  if (fidy > 0)
  {
    if (ficury < 0) // increase dsty, decrease dsth
    {
      int n = (int) ((fidy-1-ficury)/fidy);
      dsth-=n;
      dsty+=n;
      ficury+=fidy*n;
    }
    if ((ficury + fidy*(dsth-1)) >= srcbm_h*65536.0)
    {
      int newh = (int)(((srcbm_h-1)*65536.0 - ficury)/fidy);
      if (newh < dsth) dsth=newh;
    }
  }
  else if (fidy<0)
  {
    // todo: optimize source-clipping with reversed Y axis (check icury against src->getHeight(), etc)
  }
  if (dstw<1 || dsth<1) return;

  int dest_span=dest->getRowSpan()*sizeof(LICE_pixel);
  int src_span=src->getRowSpan()*sizeof(LICE_pixel);

  const LICE_pixel_chan *psrc = (const LICE_pixel_chan *)src->getBits();
  LICE_pixel_chan *pdest = (LICE_pixel_chan *)dest->getBits();
  if (!psrc || !pdest) return;

  const int srcoffs_x = (int)((ficurx + (fidx<0 ? fidx*dstw : 0))*(1.0/65536.0));
  const int srcoffs_y = (int)((ficury + (fidy<0 ? fidy*dsth : 0))*(1.0/65536.0));

  if (src->isFlipped())
  {
    psrc += (srcbm_h-1-srcoffs_y)*src_span;
    src_span=-src_span;
  }
  else psrc += srcoffs_y * src_span;
  psrc += srcoffs_x * sizeof(LICE_pixel);

  if (dest->isFlipped())
  {
    pdest += (destbm_h-dsty - 1)*dest_span;
    dest_span=-dest_span;
  }
  else pdest += dsty*dest_span;
  pdest+=dstx*sizeof(LICE_pixel);

  clip_r -= srcoffs_x;
  clip_b -= srcoffs_y;

  const int icurx = (int) (ficurx - srcoffs_x*65536.0);
  const int icury = (int) (ficury - srcoffs_y*65536.0);
  const int idx = (int) fidx, idy = (int) fidy;

  if (clip_r<1||clip_b<1) return;

  int ia=(int)(alpha*256.0);

  if ((mode&(LICE_BLIT_FILTER_MASK|LICE_BLIT_MODE_MASK|LICE_BLIT_USE_ALPHA))==LICE_BLIT_MODE_COPY && (ia==128 || ia==256))
  {
    if (ia==128)
    {
      _LICE_Template_Blit0<_LICE_CombinePixelsHalfMixFAST>::scaleBlitFAST(pdest,psrc,dstw,dsth,icurx,icury,idx,idy,clip_r,clip_b,src_span,dest_span);
    }
    else
    {
      _LICE_Template_Blit0<_LICE_CombinePixelsClobberFAST>::scaleBlitFAST(pdest,psrc,dstw,dsth,icurx,icury,idx,idy,clip_r,clip_b,src_span,dest_span);
    }
  }
  else
  {
    if (xadvance>=1.7 && yadvance >=1.7 && (mode&LICE_BLIT_FILTER_MASK)==LICE_BLIT_FILTER_BILINEAR)
    {
      int msc = lice_max(idx,idy);
      const int filtsz=msc>(3<<16) ? 5 : 3;
      const int filt_start = - (filtsz/2);

      int filter[25]; // 5x5 max
      {
        int y;
      //  char buf[4096];
    //    sprintf(buf,"filter, msc=%f: ",msc);
        int *p=filter;
        for(y=0;y<filtsz;y++)
        {
          int x;
          for(x=0;x<filtsz;x++)
          {
            if (x==y && x==filtsz/2) *p++ = 65536; // src pix is always valued at 1.
            else
            {
              double dx=x+filt_start;
              double dy=y+filt_start;
              double v = (msc-1.0) / sqrt(dx*dx+dy*dy); // this needs serious tweaking...

  //            sprintf(buf+strlen(buf),"%f,",v);

              if(v<0.0) *p++=0;
              else if (v>1.0) *p++=65536;
              else *p++=(int)(v*65536.0);
            }
          }
        }
//        OutputDebugString(buf);
      }

      #ifdef LICE_FAVOR_SIZE
        LICE_COMBINEFUNC blitfunc=NULL;      
        #define __LICE__ACTION(comb) blitfunc=comb::doPix;
      #else
        #define __LICE__ACTION(comb) _LICE_Template_Blit2<comb>::scaleBlitFilterDown(pdest,psrc,dstw,dsth,icurx,icury,idx,idy,clip_r,clip_b,src_span,dest_span,ia,filter,filt_start,filtsz)
      #endif
          __LICE_ACTION_SRCALPHA(mode,ia,false);
      #undef __LICE__ACTION

      #ifdef LICE_FAVOR_SIZE
        if (blitfunc) _LICE_Template_Blit2::scaleBlitFilterDown(pdest,psrc,dstw,dsth,icurx,icury,idx,idy,clip_r,clip_b,src_span,dest_span,ia,filter,filt_start,filtsz,blitfunc);
      #endif

    }
    else
    {
      #ifdef LICE_FAVOR_SIZE
        LICE_COMBINEFUNC blitfunc=NULL;      
        #define __LICE__ACTION(comb) blitfunc=comb::doPix;
      #else
        #define __LICE__ACTION(comb) _LICE_Template_Blit2<comb>::scaleBlit(pdest,psrc,dstw,dsth,icurx,icury,idx,idy,clip_r,clip_b,src_span,dest_span,ia,mode&LICE_BLIT_FILTER_MASK)
      #endif
          __LICE_ACTION_SRCALPHA(mode,ia,false);
      #undef __LICE__ACTION
      #ifdef LICE_FAVOR_SIZE
        if (blitfunc) _LICE_Template_Blit2::scaleBlit(pdest,psrc,dstw,dsth,icurx,icury,idx,idy,clip_r,clip_b,src_span,dest_span,ia,mode&LICE_BLIT_FILTER_MASK,blitfunc);
      #endif
    }
  }
}

void LICE_DeltaBlit(LICE_IBitmap *dest, LICE_IBitmap *src, 
                    int dstx, int dsty, int dstw, int dsth, 
                    float srcx, float srcy, float srcw, float srch, 
                    double dsdx, double dtdx, double dsdy, double dtdy,
                    double dsdxdy, double dtdxdy,
                    bool cliptosourcerect, float alpha, int mode)
{
  if (!dest || !src || !dstw || !dsth) return;

  int srcbm_w = src->getWidth(), srcbm_h = src->getHeight();
  int destbm_w = dest->getWidth(), destbm_h = dest->getHeight();

  int __sc = (int)dest->Extended(LICE_EXT_GET_SCALING,NULL);
  if (__sc>0)
  {
    if (!IGNORE_SCALING(mode))
    {
      __LICE_SC(dstx);
      __LICE_SC(dsty);
      __LICE_SC(dstw);
      __LICE_SC(dsth);
    }
    __LICE_SCU(destbm_w);
    __LICE_SCU(destbm_h);

  }
  const int __scdest = __sc;
  __sc = (int)src->Extended(LICE_EXT_GET_SCALING,NULL);

  if (__sc>0)
  {
    if (!IGNORE_SCALING(mode))
    {
      __LICE_SC(srcx);
      __LICE_SC(srcy);
      __LICE_SC(srcw);
      __LICE_SC(srch);
    }
    __LICE_SCU(srcbm_w);
    __LICE_SCU(srcbm_h);
  }

  if (__scdest != __sc && !IGNORE_SCALING(mode))
  {
    const double adj = (__sc>0 ? __sc : 256.0) / (double) (__scdest>0 ? __scdest : 256.0);
    dsdx *= adj;
    dtdx *= adj;
    dsdy *= adj;
    dtdy *= adj;
    dsdxdy *= adj;
    dtdxdy *= adj;
  }

  double src_top=0.0,src_left=0.0,src_right=srcbm_w,src_bottom=srcbm_h;

  if (cliptosourcerect)
  {
    if (srcx > src_left) src_left=srcx;
    if (srcy > src_top) src_top=srcy;
    if (srcx+srcw < src_right) src_right=srcx+srcw;
    if (srcy+srch < src_bottom) src_bottom=srcy+srch;
  }

  if (dstw<0)
  {
    dstw=-dstw;
    dstx-=dstw;
    srcx+=srcw;  
  }
  if (dsth<0)
  {
    dsth=-dsth;
    dsty-=dsth;
    srcy+=srch;
  }


  if (dstx < 0) 
  { 
    srcx -= (float) (dstx*dsdx); 
    srcy -= (float) (dstx*dtdx);
    dstw+=dstx; 
    dstx=0; 
  }
  if (dsty < 0) 
  { 
    srcy -= (float) (dsty*dtdy);
    srcx -= (float) (dsty*dsdy);
    dsth+=dsty; 
    dsty=0; 
  }  

  if (dstw < 1 || dsth < 1 || dstx >= destbm_w || dsty >= destbm_h) return;

  if (dstw > destbm_w-dstx) dstw=destbm_w-dstx;
  if (dsth > destbm_h-dsty) dsth=destbm_h-dsty;


  int dest_span=dest->getRowSpan()*sizeof(LICE_pixel);
  int src_span=src->getRowSpan()*sizeof(LICE_pixel);

  const LICE_pixel_chan *psrc = (LICE_pixel_chan *)src->getBits();
  LICE_pixel_chan *pdest = (LICE_pixel_chan *)dest->getBits();
  if (!psrc || !pdest) return;

  if (src->isFlipped())
  {
    psrc += (srcbm_h-1)*src_span;
    src_span=-src_span;
  }

  if (dest->isFlipped())
  {
    pdest += (destbm_h-dsty - 1)*dest_span;
    dest_span=-dest_span;
  }
  else pdest += dsty*dest_span;
  pdest+=dstx*sizeof(LICE_pixel);

  int sl=(int)(src_left);
  int sr=(int)(src_right);
  int st=(int)(src_top);
  int sb=(int)(src_bottom);

  sr -= sl;
  sb -= st;
  if (sr < 1 || sb < 1) return;

  psrc += src_span * st + sl * sizeof(LICE_pixel);

  int ia=(int)(alpha*256.0);
  int isrcx=(int)(srcx*65536.0);
  int isrcy=(int)(srcy*65536.0);
  int idsdx=(int)(dsdx*65536.0);
  int idtdx=(int)(dtdx*65536.0);
  int idsdy=(int)(dsdy*65536.0);
  int idtdy=(int)(dtdy*65536.0);
  int idsdxdy=(int)(dsdxdy*65536.0);
  int idtdxdy=(int)(dtdxdy*65536.0);

#ifndef LICE_FAVOR_SPEED
  LICE_COMBINEFUNC blitfunc=NULL;
  #define __LICE__ACTION(comb) blitfunc = comb::doPix;
#else
  #define __LICE__ACTION(comb) _LICE_Template_Blit3<comb>::deltaBlit(pdest,psrc,dstw,dsth,isrcx,isrcy,idsdx,idtdx,idsdy,idtdy,idsdxdy,idtdxdy,sr,sb,src_span,dest_span,ia,mode&LICE_BLIT_FILTER_MASK)
#endif
      __LICE_ACTION_SRCALPHA(mode,ia,false);
  #undef __LICE__ACTION

#ifndef LICE_FAVOR_SPEED
  if (blitfunc) _LICE_Template_Blit3::deltaBlit(pdest,psrc,dstw,dsth,isrcx,isrcy,idsdx,idtdx,idsdy,idtdy,idsdxdy,idtdxdy,sr,sb,src_span,dest_span,ia,mode&LICE_BLIT_FILTER_MASK,blitfunc);
#endif

}

void LICE_DeltaBlitAlpha(LICE_IBitmap *dest, LICE_IBitmap *src, 
                    int dstx, int dsty, int dstw, int dsth, 
                    float srcx, float srcy, float srcw, float srch, 
                    double dsdx, double dtdx, double dsdy, double dtdy,
                    double dsdxdy, double dtdxdy,
                    bool cliptosourcerect, float alpha, int mode, double dadx, double dady, double dadxdy)
{
  if (!dest || !src || !dstw || !dsth) return;

  int destbm_w = dest->getWidth(), destbm_h = dest->getHeight();
  int srcbm_w = src->getWidth(), srcbm_h = src->getHeight();
  int __sc = (int)dest->Extended(LICE_EXT_GET_SCALING,NULL);
  if (__sc>0)
  {
    if (!IGNORE_SCALING(mode))
    {
      __LICE_SC(dstx);
      __LICE_SC(dsty);
      __LICE_SC(dstw);
      __LICE_SC(dsth);
    }
    __LICE_SCU(destbm_w);
    __LICE_SCU(destbm_h);

  }
  const int __scdest = __sc;
  __sc = (int)src->Extended(LICE_EXT_GET_SCALING,NULL);

  if (__sc>0)
  {
    if (!IGNORE_SCALING(mode))
    {
      __LICE_SC(srcx);
      __LICE_SC(srcy);
      __LICE_SC(srcw);
      __LICE_SC(srch);
    }
    __LICE_SCU(srcbm_w);
    __LICE_SCU(srcbm_h);
  }

  if (lice_max(__scdest,0) != lice_max(__sc,0))
  {
    const double adj = (__sc>0 ? __sc : 256.0) / (double) (__scdest>0 ? __scdest : 256.0);
    dsdx *= adj;
    dtdx *= adj;
    dadx *= adj;
    dsdy *= adj;
    dtdy *= adj;
    dady *= adj;
    dsdxdy *= adj;
    dtdxdy *= adj;
    dadxdy *= adj;
  }

  const double eps = 0.0001;
  if (fabs(dadx*dstw) < eps && fabs(dady*dsth) < eps && fabs(dadxdy*dsth) < eps)
  {
    LICE_DeltaBlit(dest, src, dstx, dsty, dstw, dsth, srcx, srcy, srcw, srch, dsdx, dtdx, dsdy, dtdy, dsdxdy, dtdxdy, cliptosourcerect, alpha, mode);
    return;
  }

  double src_top=0.0,src_left=0.0,src_right=srcbm_w,src_bottom=srcbm_h;

  if (cliptosourcerect)
  {
    if (srcx > src_left) src_left=srcx;
    if (srcy > src_top) src_top=srcy;
    if (srcx+srcw < src_right) src_right=srcx+srcw;
    if (srcy+srch < src_bottom) src_bottom=srcy+srch;
  }

  if (dstw<0)
  {
    dstw=-dstw;
    dstx-=dstw;
    srcx+=srcw;  
  }
  if (dsth<0)
  {
    dsth=-dsth;
    dsty-=dsth;
    srcy+=srch;
  }


  if (dstx < 0) 
  { 
    alpha -= (float) (dstx*dadx);
    srcx -= (float) (dstx*dsdx); 
    srcy -= (float) (dstx*dtdx);
    dstw+=dstx; 
    dstx=0; 
  }
  if (dsty < 0) 
  { 
    alpha -= (float) (dsty*dady);
    srcy -= (float) (dsty*dtdy);
    srcx -= (float) (dsty*dsdy);
    dsth+=dsty; 
    dsty=0; 
  }  

  if (dstw < 1 || dsth < 1 || dstx >= destbm_w || dsty >= destbm_h) return;

  if (dstw > destbm_w-dstx) dstw=destbm_w-dstx;
  if (dsth > destbm_h-dsty) dsth=destbm_h-dsty;


  int dest_span=dest->getRowSpan()*sizeof(LICE_pixel);
  int src_span=src->getRowSpan()*sizeof(LICE_pixel);

  const LICE_pixel_chan *psrc = (LICE_pixel_chan *)src->getBits();
  LICE_pixel_chan *pdest = (LICE_pixel_chan *)dest->getBits();
  if (!psrc || !pdest) return;

  if (src->isFlipped())
  {
    psrc += (srcbm_h-1)*src_span;
    src_span=-src_span;
  }

  if (dest->isFlipped())
  {
    pdest += (destbm_h-dsty - 1)*dest_span;
    dest_span=-dest_span;
  }
  else pdest += dsty*dest_span;
  pdest+=dstx*sizeof(LICE_pixel);

  int sl=(int)(src_left);
  int sr=(int)(src_right);
  int st=(int)(src_top);
  int sb=(int)(src_bottom);

  sr -= sl;
  sb -= st;
  if (sr < 1 || sb < 1) return;

  psrc += src_span * st + sl * sizeof(LICE_pixel);

  int ia=(int)(alpha*65536.0);
  int isrcx=(int)(srcx*65536.0);
  int isrcy=(int)(srcy*65536.0);
  int idsdx=(int)(dsdx*65536.0);
  int idtdx=(int)(dtdx*65536.0);
  int idsdy=(int)(dsdy*65536.0);
  int idtdy=(int)(dtdy*65536.0);
  int idsdxdy=(int)(dsdxdy*65536.0);
  int idtdxdy=(int)(dtdxdy*65536.0);

  int idadx=(int)(dadx*65536.0);
  int idady=(int)(dady*65536.0);
  int idadxdy=(int)(dadxdy*65536.0);

#ifndef LICE_FAVOR_SPEED
  LICE_COMBINEFUNC blitfunc=NULL;
  #define __LICE__ACTION(comb) blitfunc = comb::doPix;
#else
  #define __LICE__ACTION(comb) _LICE_Template_Blit4<comb>::deltaBlitAlpha(pdest,psrc,dstw,dsth,isrcx,isrcy,idsdx,idtdx,idsdy,idtdy,idsdxdy,idtdxdy,sr,sb,src_span,dest_span,ia,idadx,idady,idadxdy,mode&LICE_BLIT_FILTER_MASK)
#endif
      __LICE_ACTION_NOSRCALPHA(mode,256,true);
  #undef __LICE__ACTION

#ifndef LICE_FAVOR_SPEED
  if (blitfunc) _LICE_Template_Blit4::deltaBlitAlpha(pdest,psrc,dstw,dsth,isrcx,isrcy,idsdx,idtdx,idsdy,idtdy,idsdxdy,idtdxdy,sr,sb,src_span,dest_span,ia,idadx,idady,idadxdy,mode&LICE_BLIT_FILTER_MASK,blitfunc);
#endif

}
                      
                      


void LICE_RotatedBlit(LICE_IBitmap *dest, LICE_IBitmap *src, 
                      int dstx, int dsty, int dstw, int dsth, 
                      float srcx, float srcy, float srcw, float srch, 
                      float angle, 
                      bool cliptosourcerect, float alpha, int mode, float rotxcent, float rotycent)
{
  if (!dest || !src || !dstw || !dsth) return;

  int destbm_w = dest->getWidth(), destbm_h = dest->getHeight();
  int srcbm_w = src->getWidth(), srcbm_h = src->getHeight();
  int __sc = (int)dest->Extended(LICE_EXT_GET_SCALING,NULL);
  if (__sc>0)
  {
    if (!IGNORE_SCALING(mode))
    {
      __LICE_SC(dstx);
      __LICE_SC(dsty);
      __LICE_SC(dstw);
      __LICE_SC(dsth);
    }
    __LICE_SCU(destbm_w);
    __LICE_SCU(destbm_h);

  }
  __sc = (int)src->Extended(LICE_EXT_GET_SCALING,NULL);

  if (__sc>0)
  {
    if (!IGNORE_SCALING(mode))
    {
      __LICE_SC(srcx);
      __LICE_SC(srcy);
      __LICE_SC(srcw);
      __LICE_SC(srch);
    }
    __LICE_SCU(srcbm_w);
    __LICE_SCU(srcbm_h);
  }
  double src_top=0.0,src_left=0.0,src_right=srcbm_w,src_bottom=srcbm_h;

  if (cliptosourcerect)
  {
    if (srcx > src_left) src_left=srcx;
    if (srcy > src_top) src_top=srcy;
    if (srcx+srcw < src_right) src_right=srcx+srcw;
    if (srcy+srch < src_bottom) src_bottom=srcy+srch;
  }

  if (dstw<0)
  {
    dstw=-dstw;
    dstx-=dstw;
    srcx+=srcw;  
    srcw=-srcw;
  }
  if (dsth<0)
  {
    dsth=-dsth;
    dsty-=dsth;
    srcy+=srch;
    srch=-srch;
  }

  double cosa=cos(angle);
  double sina=sin(angle);

  double xsc=srcw / dstw;
  double ysc=srch / dsth;

  double dsdx = xsc * cosa;
  double dtdy = ysc * cosa;
  double dsdy = xsc * sina;
  double dtdx = ysc * -sina;

  srcx -= (float) (0.5 * (dstw*dsdx + dsth*dsdy - srcw) - rotxcent);
  srcy -= (float) (0.5 * (dsth*dtdy + dstw*dtdx - srch) - rotycent);

  if (dstx < 0) 
  { 
    srcx -= (float) (dstx*dsdx); 
    srcy -= (float) (dstx*dtdx);
    dstw+=dstx; 
    dstx=0; 
  }
  if (dsty < 0) 
  { 
    srcy -= (float) (dsty*dtdy);
    srcx -= (float) (dsty*dsdy);
    dsth+=dsty; 
    dsty=0; 
  }  

  if (dstw < 1 || dsth < 1 || dstx >= destbm_w || dsty >= destbm_h) return;

  if (dstw > destbm_w-dstx) dstw=destbm_w-dstx;
  if (dsth > destbm_h-dsty) dsth=destbm_h-dsty;


  int dest_span=dest->getRowSpan()*sizeof(LICE_pixel);
  int src_span=src->getRowSpan()*sizeof(LICE_pixel);

  const LICE_pixel_chan *psrc = (LICE_pixel_chan *)src->getBits();
  LICE_pixel_chan *pdest = (LICE_pixel_chan *)dest->getBits();
  if (!psrc || !pdest) return;

  if (src->isFlipped())
  {
    psrc += (srcbm_h-1)*src_span;
    src_span=-src_span;
  }

  if (dest->isFlipped())
  {
    pdest += (destbm_h-dsty - 1)*dest_span;
    dest_span=-dest_span;
  }
  else pdest += dsty*dest_span;
  pdest+=dstx*sizeof(LICE_pixel);

  int sl=(int)(src_left);
  int sr=(int)(src_right);
  int st=(int)(src_top);
  int sb=(int)(src_bottom);

  sr -= sl;
  sb -= st;
  srcx -= sl;
  srcy -= st;
  if (sr < 1 || sb < 1) return;

  psrc += src_span * st + sl * sizeof(LICE_pixel);

  int ia=(int)(alpha*256.0);
  int isrcx=(int)(srcx*65536.0);
  int isrcy=(int)(srcy*65536.0);
  int idsdx=(int)(dsdx*65536.0);
  int idtdx=(int)(dtdx*65536.0);
  int idsdy=(int)(dsdy*65536.0);
  int idtdy=(int)(dtdy*65536.0);

#ifndef LICE_FAVOR_SPEED
  LICE_COMBINEFUNC blitfunc=NULL;
  #define __LICE__ACTION(comb) blitfunc = comb::doPix;
#else
  #define __LICE__ACTION(comb) _LICE_Template_Blit3<comb>::deltaBlit(pdest,psrc,dstw,dsth,isrcx,isrcy,idsdx,idtdx,idsdy,idtdy,0,0,sr,sb,src_span,dest_span,ia,mode&LICE_BLIT_FILTER_MASK)
#endif
      __LICE_ACTION_SRCALPHA(mode,ia,false);
  #undef __LICE__ACTION

#ifndef LICE_FAVOR_SPEED
  if (blitfunc) _LICE_Template_Blit3::deltaBlit(pdest,psrc,dstw,dsth,isrcx,isrcy,idsdx,idtdx,idsdy,idtdy,0,0,sr,sb,src_span,dest_span,ia,mode&LICE_BLIT_FILTER_MASK,blitfunc);
#endif
}

#endif


void LICE_Clear(LICE_IBitmap *dest, LICE_pixel color)
{
  if (!dest) return;

#ifndef DISABLE_LICE_EXTENSIONS
  if (dest->Extended(LICE_EXT_SUPPORTS_ID, (void*) LICE_EXT_CLEAR_ACCEL))
  {
    if (dest->Extended(LICE_EXT_CLEAR_ACCEL, &color)) return;
  }
#endif

  LICE_pixel *p=dest->getBits();
  int h=dest->getHeight();
  int w=dest->getWidth();
  const int sp=dest->getRowSpan();

  const int __sc = (int)dest->Extended(LICE_EXT_GET_SCALING,NULL);
  if (__sc>0)
  {
    __LICE_SCU(w);
    __LICE_SCU(h);
  }

  if (!p || w<1 || h<1 || !sp) return;

  while (h-->0)
  {
    int n=w;
    while (n--) *p++ = color;
    p+=sp-w;
  }
}




void LICE_MultiplyAddRect(LICE_IBitmap *dest, int x, int y, int w, int h, 
                          float rsc, float gsc, float bsc, float asc,
                          float radd, float gadd, float badd, float aadd)
{
  if (!dest) return;

  int destbm_w = dest->getWidth(), destbm_h = dest->getHeight();
  DO_RECT_SC(0)

  if (x<0) { w+=x; x=0; }
  if (y<0) { h+=y; y=0; }

  LICE_pixel *p=dest->getBits();
  const int sp=dest->getRowSpan();
  if (!p || !sp || w<1 || h < 1 || x >= destbm_w || y >= destbm_h) return;

  if (w>destbm_w-x) w=destbm_w-x;
  if (h>destbm_h-y) h=destbm_h-y;

  if (dest->isFlipped())
  {
    p+=(destbm_h - y - h)*sp;
  }
  else 
  {
    p+=sp*y;
  }

  p += x;

  int ir=(int)(rsc*256.0);
  int ig=(int)(gsc*256.0);
  int ib=(int)(bsc*256.0);
  int ia=(int)(asc*256.0);
  int ir2=(int)(radd*256.0);
  int ig2=(int)(gadd*256.0);
  int ib2=(int)(badd*256.0);
  int ia2=(int)(aadd*256.0);

  while (h-->0)
  {
    int n=w;
    while (n--) 
    {
      LICE_pixel_chan *ptr=(LICE_pixel_chan *)p++;
      _LICE_MakePixelClamp(ptr,(ptr[LICE_PIXEL_R]*ir+ir2)>>8,
                          (ptr[LICE_PIXEL_G]*ig+ig2)>>8,
                          (ptr[LICE_PIXEL_B]*ib+ib2)>>8,
                          (ptr[LICE_PIXEL_A]*ia+ia2)>>8);
    }
    p+=sp-w;
  }
}

void LICE_ProcessRect(LICE_IBitmap *dest, int x, int y, int w, int h, void (*procFunc)(LICE_pixel *p, void *parm), void *parm)
{
  if (!dest||!procFunc) return;

  int destbm_w = dest->getWidth(), destbm_h = dest->getHeight();
  DO_RECT_SC(0)

  if (x<0) { w+=x; x=0; }
  if (y<0) { h+=y; y=0; }
  
  LICE_pixel *p=dest->getBits();
  const int sp=dest->getRowSpan();

  if (!p || !sp || w<1 || h < 1 || x >= destbm_w || y >= destbm_h) return;

  if (w>destbm_w-x) w=destbm_w-x;
  if (h>destbm_h-y) h=destbm_h-y;

  if (dest->isFlipped())
  {
    p+=(destbm_h - y - h)*sp;
  }
  else 
  {
    p+=sp*y;
  }

  p += x;

  while (h--)
  {
    LICE_pixel *pout=p;
    int n=w;
    while (n-->0) procFunc(pout++,parm);
    p+=sp;
  }


}

void LICE_FillRect(LICE_IBitmap *dest, int x, int y, int w, int h, LICE_pixel color, float alpha, int mode)
{
  if (!dest) return;

  int destbm_w = dest->getWidth(), destbm_h = dest->getHeight();
  DO_RECT_SC(mode)

#ifndef DISABLE_LICE_EXTENSIONS
  if (dest->Extended(LICE_EXT_SUPPORTS_ID, (void*) LICE_EXT_FILLRECT_ACCEL))
  {
    LICE_Ext_FillRect_acceldata data(x, y, w, h, color, alpha, mode);
    if (dest->Extended(LICE_EXT_FILLRECT_ACCEL, &data)) return;
  }
#endif

  if (mode & LICE_BLIT_USE_ALPHA) alpha *= LICE_GETA(color)/255.0f;

  LICE_pixel *p=dest->getBits();
  const int sp=dest->getRowSpan();

  if (x<0) { w+=x; x=0; }
  if (y<0) { h+=y; y=0; }
  if (!alpha || !p || !sp || w<1 || h < 1 || x >= destbm_w || y >= destbm_h) return;

  if (w>destbm_w-x) w=destbm_w-x;
  if (h>destbm_h-y) h=destbm_h-y;
  
  if (dest->isFlipped())
  {
    p+=(destbm_h - y - h)*sp;
  }
  else 
  {
    p+=sp*y;
  }

  p += x;

  const int ia=(int)(alpha*256.0);
  // copy, alpha=1, alpha=0.5, 0.25, 0.75 optimizations
  if ((mode&LICE_BLIT_MODE_MASK)==LICE_BLIT_MODE_COPY)
  {
    if (ia==256)
    {
      _LICE_Template_Blit0<_LICE_CombinePixelsClobberFAST>::solidBlitFAST(p,w,h,color,sp);
      return;
    }
    if (ia==128)
    {
      // we use _LICE_CombinePixelsHalfMix2 because we pre-halve color here
      _LICE_Template_Blit0<_LICE_CombinePixelsHalfMix2FAST>::solidBlitFAST(p,w,h,(color>>1)&0x7f7f7f7f,sp);
      return;
    }
    if (ia==64)
    {
      _LICE_Template_Blit0<_LICE_CombinePixelsQuarterMix2FAST>::solidBlitFAST(p,w,h,(color>>2)&0x3f3f3f3f,sp);
      return;
    }
    if (ia==192)
    {
      _LICE_Template_Blit0<_LICE_CombinePixelsThreeQuarterMix2FAST>::solidBlitFAST(p,w,h,
        ((color>>1)&0x7f7f7f7f)+((color>>2)&0x3f3f3f3f),sp);
      return;
    }
  }

#ifdef LICE_FAVOR_SIZE_EXTREME
  LICE_COMBINEFUNC blitfunc=NULL;      
  #define __LICE__ACTION(comb) blitfunc=comb::doPix;
#else
  #define __LICE__ACTION(comb) _LICE_Template_Blit1<comb>::solidBlit((LICE_pixel_chan*)p,w,h,LICE_GETR(color),LICE_GETG(color),LICE_GETB(color),ia,sp*sizeof(LICE_pixel))
#endif

    // we use __LICE_ACTION_NOSRCALPHA even though __LICE_ACTION_CONSTANTALPHA
    // is valid, sinc we optimized the 1.0f/0.5f cases above
      __LICE_ACTION_NOSRCALPHA(mode,ia,false);
  #undef __LICE__ACTION

#ifdef LICE_FAVOR_SIZE_EXTREME
  if (blitfunc) _LICE_Template_Blit1::solidBlit((LICE_pixel_chan*)p,w,h,LICE_GETR(color),LICE_GETG(color),LICE_GETB(color),ia,sp*sizeof(LICE_pixel),blitfunc);
#endif
}

void LICE_ClearRect(LICE_IBitmap *dest, int x, int y, int w, int h, LICE_pixel mask, LICE_pixel orbits)
{
  if (!dest) return;
  LICE_pixel *p=dest->getBits();

  int destbm_w = dest->getWidth(), destbm_h = dest->getHeight();
  DO_RECT_SC(0)

  if (x<0) { w+=x; x=0; }
  if (y<0) { h+=y; y=0; }

  const int sp=dest->getRowSpan();
  if (!p || !sp || w<1 || h < 1 || x >= destbm_w || y >= destbm_h) return;

  if (w>destbm_w-x) w=destbm_w-x;
  if (h>destbm_h-y) h=destbm_h-y;
  
  if (dest->isFlipped())
  {
    p+=(destbm_h - y - h)*sp;
  }
  else p+=sp*y;

  p += x;
  while (h-->0)
  {
    int n=w;
    while (n--) 
    {
      *p = (*p&mask)|orbits;
      p++;
    }
    p+=sp-w;
  }
}


LICE_pixel LICE_GetPixel(LICE_IBitmap *bm, int x, int y)
{
  if (!bm) return 0;

  int w = bm->getWidth(), h = bm->getHeight();
  const int __sc = (int)bm->Extended(LICE_EXT_GET_SCALING,NULL);
  if (__sc>0)
  {
    __LICE_SCU(w);
    __LICE_SCU(h);
    __LICE_SC(x);
    __LICE_SC(y);
  }
#ifndef DISABLE_LICE_EXTENSIONS
  if (bm->Extended(LICE_EXT_SUPPORTS_ID, (void*) LICE_EXT_GETPIXEL_ACCEL))
  {
    LICE_Ext_GetPixel_acceldata data(x, y);
    if (bm->Extended(LICE_EXT_GETPIXEL_ACCEL, &data)) return data.px;
  }
#endif

  const LICE_pixel *px;

  if (!(px=bm->getBits()) || x < 0 || y < 0 || x >= w || y>= h) return 0;
  if (bm->isFlipped()) return px[(h-1-y) * bm->getRowSpan() + x];
  return px[y * bm->getRowSpan() + x];
}

void LICE_PutPixel(LICE_IBitmap *bm, int x, int y, LICE_pixel color, float alpha, int mode)
{
  if (!bm) return;

  const int __sc = (int)bm->Extended(LICE_EXT_GET_SCALING,NULL);
  if (__sc>0)
  {
    if (!IGNORE_SCALING(mode))
    {
      LICE_FillRect(bm,x,y,1,1,color,alpha,mode);
      return;
    }

  }

  int w = bm->getWidth(), h = bm->getHeight();
  if (__sc>0)
  {
    __LICE_SCU(w);
    __LICE_SCU(h);
  }
#ifndef DISABLE_LICE_EXTENSIONS
  if (bm->Extended(LICE_EXT_SUPPORTS_ID, (void*) LICE_EXT_PUTPIXEL_ACCEL))
  {
    LICE_Ext_PutPixel_acceldata data(x, y, color, alpha, mode);
    if (bm->Extended(LICE_EXT_PUTPIXEL_ACCEL, &data)) return;
  }
#endif

  LICE_pixel *px;
  if (!(px=bm->getBits()) || x < 0 || y < 0 || x >= w || y >= h) return;

  if (bm->isFlipped()) px+=x+(h-1-y)*bm->getRowSpan();
  else px+=x+y*bm->getRowSpan();

  int ia = (int)(alpha * 256.0f);
  if ((mode&LICE_BLIT_MODE_MASK)==LICE_BLIT_MODE_COPY)
  {
    if (ia==256)
    {
      *px = color;
      return;
    }
    if (ia==128)
    {
      *px = ((*px>>1)&0x7f7f7f7f) + ((color>>1)&0x7f7f7f7f);
      return;
    }
    if (ia==64)
    {
      *px = ((*px>>1)&0x7f7f7f7f) + ((*px>>2)&0x3f3f3f3f) + ((color>>2)&0x3f3f3f3f);
      return;
    }
    if (ia==192)
    {
      *px = ((*px>>2)&0x3f3f3f3f) + ((color>>1)&0x7f7f7f7f) + ((color>>2)&0x3f3f3f3f);
      return;
    }
  }
#define __LICE__ACTION(comb) comb::doPix((LICE_pixel_chan *)px, LICE_GETR(color),LICE_GETG(color),LICE_GETB(color),LICE_GETA(color), ia)
  __LICE_ACTION_NOSRCALPHA(mode,ia,false);
#undef __LICE__ACTION
}


#ifndef LICE_NO_BLIT_SUPPORT

template<class T> class LICE_TransformBlit_class
{
  public:
  static void blit(LICE_IBitmap *dest, LICE_IBitmap *src,  
                    int dstx, int dsty, int dstw, int dsth,
                    const T *srcpoints, int div_w, int div_h, // srcpoints coords should be div_w*div_h*2 long, and be in source image coordinates
                    float alpha, int mode)
{
  if (!dest || !src || dstw<1 || dsth<1 || div_w<2 || div_h<2) return;

  int cypos=dsty;
  double ypos=dsty;
  double dxpos=dstw/(float)(div_w-1);
  double dypos=dsth/(float)(div_h-1);
  int y;
  const T *curpoints=srcpoints;
  for (y = 0; y < div_h-1; y ++)
  {
    int nypos=(int)((ypos+=dypos) + 0.5);
    int x;
    double xpos=dstx;
    int cxpos=dstx;

    if (nypos != cypos)
    {
      double iy=1.0/(double)(nypos-cypos);
      for (x = 0; x < div_w-1; x ++)
      {
        int nxpos=(int) ((xpos+=dxpos) + 0.5);
        if (nxpos != cxpos)
        {
          int offs=x*2;
          double sx=curpoints[offs];
          double sy=curpoints[offs+1];
          double sw=curpoints[offs+2]-sx;
          double sh=curpoints[offs+3]-sy;

          offs+=div_w*2;
          double sxdiff=curpoints[offs]-sx;
          double sydiff=curpoints[offs+1]-sy;
          double sw3=curpoints[offs+2]-curpoints[offs];
          double sh3=curpoints[offs+3]-curpoints[offs+1];

          double ix=1.0/(double)(nxpos-cxpos);
          double dsdx=sw*ix;
          double dtdx=sh*ix;
          double dsdx2=sw3*ix;
          double dtdx2=sh3*ix;
          double dsdy=sxdiff*iy;
          double dtdy=sydiff*iy;
          double dsdxdy = (dsdx2-dsdx)*iy;
          double dtdxdy = (dtdx2-dtdx)*iy;

          LICE_DeltaBlit(dest,src,cxpos,cypos,nxpos-cxpos,nypos-cypos,
              (float)sx,(float)sy,(float)sw,(float)sh,
              dsdx,dtdx,dsdy,dtdy,dsdxdy,dtdxdy,false,alpha,mode);
        }

        cxpos=nxpos;
      }
    }
    curpoints+=div_w*2;
    cypos=nypos;
  }
}
};

template<class T> class LICE_TransformBlitAlpha_class
{
  public:
  static void blit(LICE_IBitmap *dest, LICE_IBitmap *src,  
                    int dstx, int dsty, int dstw, int dsth,
                    const T *srcpoints, int div_w, int div_h, // srcpoints coords should be div_w*div_h*2 long, and be in source image coordinates
                    int mode)
{
  if (!dest || !src || dstw<1 || dsth<1 || div_w<2 || div_h<2) return;

  int cypos=dsty;
  double ypos=dsty;
  double dxpos=dstw/(float)(div_w-1);
  double dypos=dsth/(float)(div_h-1);
  int y;
  const T *curpoints=srcpoints;
  for (y = 0; y < div_h-1; y ++)
  {
    int nypos=(int)((ypos+=dypos)+0.5);
    int x;
    double xpos=dstx;
    int cxpos=dstx;

    if (nypos != cypos)
    {
      double iy=1.0/(double)(nypos-cypos);
      for (x = 0; x < div_w-1; x ++)
      {
        int nxpos=(int) ((xpos+=dxpos)+0.5);
        if (nxpos != cxpos)
        {
          int offs=x*3;
          double sx=curpoints[offs];
          double sy=curpoints[offs+1];
          double sa=curpoints[offs+2];
          double sw=curpoints[offs+3]-sx;
          double sh=curpoints[offs+4]-sy;
          double sa2 = curpoints[offs+5]-sa;

          offs+=div_w*3;
          double sxdiff=curpoints[offs]-sx;
          double sydiff=curpoints[offs+1]-sy;
          double sadiff=curpoints[offs+2]-sa;
          double sw3=curpoints[offs+3]-curpoints[offs];
          double sh3=curpoints[offs+4]-curpoints[offs+1];
          double sa3=curpoints[offs+5]-curpoints[offs+2];

          double ix=1.0/(double)(nxpos-cxpos);
          double dsdx=sw*ix;
          double dtdx=sh*ix;
          double dadx=sa2*ix;
          double dsdx2=sw3*ix;
          double dtdx2=sh3*ix;
          double dadx2=sa3*ix;
          double dsdy=sxdiff*iy;
          double dtdy=sydiff*iy;
          double dady=sadiff*iy;
          double dsdxdy = (dsdx2-dsdx)*iy;
          double dtdxdy = (dtdx2-dtdx)*iy;
          double dadxdy = (dadx2-dadx)*iy;

          LICE_DeltaBlitAlpha(dest,src,cxpos,cypos,nxpos-cxpos,nypos-cypos,
              (float)sx,(float)sy,(float)sw,(float)sh,
              dsdx,dtdx,dsdy,dtdy,dsdxdy,dtdxdy,false,sa,mode,dadx,dady,dadxdy);
        }

        cxpos=nxpos;
      }
    }
    curpoints+=div_w*3;
    cypos=nypos;
  }
}
};

void LICE_TransformBlit(LICE_IBitmap *dest, LICE_IBitmap *src,  
                    int dstx, int dsty, int dstw, int dsth,
                    const float *srcpoints, int div_w, int div_h, // srcpoints coords should be div_w*div_h*2 long, and be in source image coordinates
                    float alpha, int mode)
{
  LICE_TransformBlit_class<float>::blit(dest,src,dstx,dsty,dstw,dsth,srcpoints,div_w,div_h,alpha,mode);
}
void LICE_TransformBlit2(LICE_IBitmap *dest, LICE_IBitmap *src,  
                    int dstx, int dsty, int dstw, int dsth,
                    const double *srcpoints, int div_w, int div_h, // srcpoints coords should be div_w*div_h*2 long, and be in source image coordinates
                    float alpha, int mode)
{
  LICE_TransformBlit_class<double>::blit(dest,src,dstx,dsty,dstw,dsth,srcpoints,div_w,div_h,alpha,mode);
}


void LICE_TransformBlit2Alpha(LICE_IBitmap *dest, LICE_IBitmap *src,  
                    int dstx, int dsty, int dstw, int dsth,
                    const double *srcpoints, int div_w, int div_h, // srcpoints coords should be div_w*div_h*3 long, and be in source image coordinates+alpha
                    int mode)
{
  LICE_TransformBlitAlpha_class<double>::blit(dest,src,dstx,dsty,dstw,dsth,srcpoints,div_w,div_h,mode);
}
#endif

#ifndef LICE_NO_MISC_SUPPORT

void LICE_SetAlphaFromColorMask(LICE_IBitmap *dest, LICE_pixel color)
{
  if (!dest) return;
  LICE_pixel *p=dest->getBits();
  int h=dest->getHeight();
  int w=dest->getWidth();
  int sp=dest->getRowSpan();
  if (!p || w<1 || h<1 || sp<1) return;

  while (h-->0)
  {
    int n=w;
    while (n--) 
    {
      if ((*p&LICE_RGBA(255,255,255,0)) == color) *p&=LICE_RGBA(255,255,255,0);
      else *p|=LICE_RGBA(0,0,0,255);
      p++;
    }
    p+=sp-w;
  }
}


void LICE_SimpleFill(LICE_IBitmap *dest, int x, int y, LICE_pixel newcolor,  
                     LICE_pixel comparemask, 
                     LICE_pixel keepmask)
{
  if (!dest) return;
  int dw=dest->getWidth();
  int dh=dest->getHeight();
  int rowsize=dest->getRowSpan();
  if (x<0||x>=dw||y<0||y>=dh) return;
  LICE_pixel *ptr=dest->getBits();
  if (!ptr) return;
  ptr += rowsize*y;

  LICE_pixel compval=ptr[x]&comparemask;
  int ay;
  for (ay=y;ay<dh; ay++)
  {
    if ((ptr[x]&comparemask)!=compval) break;
    ptr[x]=(ptr[x]&keepmask)|newcolor;

    int ax;
    for(ax=x+1;ax<dw;ax++)
    {
      if ((ptr[ax]&comparemask)!=compval) break;
      ptr[ax]=(ptr[ax]&keepmask)|newcolor;
    }
    ax=x;
    while (--ax>=0)
    {
      if ((ptr[ax]&comparemask)!=compval) break;
      ptr[ax]=(ptr[ax]&keepmask)|newcolor;
    }
    
    ptr+=rowsize;
  }
  ptr =dest->getBits()+rowsize*y;

  ay=y;
  while (--ay>=0)
  {
    ptr-=rowsize;
    if ((ptr[x]&comparemask)!=compval) break;
    ptr[x]=(ptr[x]&keepmask)|newcolor;

    int ax;
    for(ax=x+1;ax<dw;ax++)
    {
      if ((ptr[ax]&comparemask)!=compval) break;
      ptr[ax]=(ptr[ax]&keepmask)|newcolor;
    }
    ax=x;
    while (--ax>=0)
    {
      if ((ptr[ax]&comparemask)!=compval) break;
      ptr[ax]=(ptr[ax]&keepmask)|newcolor;
    }   
  }
}

// VS6 instantiates this wrong as a template function, needs to be a template class
template <class COMBFUNC> class GlyphDrawImpl
{
public:
  static void DrawGlyph(const LICE_pixel_chan* srcalpha, LICE_pixel* destpx, int src_w, int src_h, LICE_pixel color,  int span, int src_span, int aa)
  {
    const int r = LICE_GETR(color), g = LICE_GETG(color), b = LICE_GETB(color), a = LICE_GETA(color);

    int xi, yi;
    for (yi = 0; yi < src_h; ++yi, srcalpha += src_span, destpx += span) {
      const LICE_pixel_chan* tsrc = srcalpha;
      LICE_pixel* tdest = destpx;
      for (xi = 0; xi < src_w; ++xi, ++tsrc, ++tdest) {
        const LICE_pixel_chan v = *tsrc;
        if (v) {  // glyphs should be expected to have a lot of "holes"
          COMBFUNC::doPix((LICE_pixel_chan*) tdest, r, g, b, a, v*aa/256);
        }
      }
    }
  }
  static void DrawGlyphScale(const LICE_pixel_chan* srcalpha, LICE_pixel* destpx, int src_w, int src_h, LICE_pixel color,  int span, int src_span, int aa, int scale)
  {
    const int r = LICE_GETR(color), g = LICE_GETG(color), b = LICE_GETB(color), a = LICE_GETA(color);

    int xi, yi;
    int ysum = 0;
    for (yi = 0; yi < src_h; ++yi, srcalpha += src_span) {
      ysum += scale;
      while (ysum > 255)
      {
        const LICE_pixel_chan* tsrc = srcalpha;
        LICE_pixel* tdest = destpx;
        ysum -= 256;
        destpx += span;
        int sum = 0;
        for (xi = 0; xi < src_w; ++xi, ++tsrc) {
          const LICE_pixel_chan v = *tsrc;
          sum += scale;
          if (v) {  // glyphs should be expected to have a lot of "holes"
            while (sum>255)
            {
              COMBFUNC::doPix((LICE_pixel_chan*) tdest, r, g, b, a, v*aa/256);
              tdest++;
              sum -= 256;
            }
          }
          else
          {
            tdest += sum/256;
            sum &= 255;
          }
        }
      }
    }
  }
  static void DrawGlyphMono(const unsigned char * srcalpha, LICE_pixel* destpx, int src_w, int src_h, LICE_pixel color,  int span, int src_span, int aa)
  {
    const int r = LICE_GETR(color), g = LICE_GETG(color), b = LICE_GETB(color), a = LICE_GETA(color);

    int xi, yi;
    for (yi = 0; yi < src_h; ++yi, srcalpha += src_span, destpx += span) {
      const unsigned char *tsrc = srcalpha;
      LICE_pixel* tdest = destpx;
      unsigned char cv=0;
      for (xi = 0; xi < src_w; ++xi, ++tdest) {
        if (!(xi&7)) cv = *tsrc++;
        const LICE_pixel_chan v = (cv&128)?255:0;
        cv<<=1;
        if (v) {  // glyphs should be expected to have a lot of "holes"
          COMBFUNC::doPix((LICE_pixel_chan*) tdest, r, g, b, a, v*aa/256);
        }
      }
    }
  }
  static void DrawGlyphMonoScale(const unsigned char * srcalpha, LICE_pixel* destpx, int src_w, int src_h, LICE_pixel color,  int span, int src_span, int aa, int scale)
  {
    const int r = LICE_GETR(color), g = LICE_GETG(color), b = LICE_GETB(color), a = LICE_GETA(color);

    int xi, yi;
    int ysum=0;
    for (yi = 0; yi < src_h; ++yi, srcalpha += src_span) {
      ysum += scale;
      while (ysum > 255)
      {
        const unsigned char *tsrc = srcalpha;
        LICE_pixel* tdest = destpx;
        unsigned char cv=0;
        int sum=0;
        ysum -= 256;
        destpx++;
        for (xi = 0; xi < src_w; ++xi) {
          if (!(xi&7)) cv = *tsrc++;
          const LICE_pixel_chan v = (cv&128)?255:0;
          cv<<=1;
          sum += scale;
          if (v) {  // glyphs should be expected to have a lot of "holes"
            while (sum > 255)
            {
              COMBFUNC::doPix((LICE_pixel_chan*) tdest, r, g, b, a, v*aa/256);
              tdest++;
              sum -= 256;
            }
          }
          else
          {
            tdest += sum/256;
            sum &= 255;
          }
        }
      }
    }
  }
};

void LICE_DrawGlyphEx(LICE_IBitmap* dest, int x, int y, LICE_pixel color, const LICE_pixel_chan* alphas, int glyph_w, int glyph_span, int glyph_h, float alpha, int mode)
{
  if (!dest) return;

#ifndef DISABLE_LICE_EXTENSIONS
  if (dest->Extended(LICE_EXT_SUPPORTS_ID, (void*) LICE_EXT_DRAWGLYPH_ACCEL) && glyph_w == glyph_span)
  {
    LICE_Ext_DrawGlyph_acceldata data(x, y, color, alphas, glyph_w, glyph_h, alpha, mode);
    if (dest->Extended(LICE_EXT_DRAWGLYPH_ACCEL, &data)) return;
  }
#endif

  int destbm_w = dest->getWidth(), destbm_h = dest->getHeight();
  const int __sc = (int)dest->Extended(LICE_EXT_GET_SCALING,NULL);
  if (__sc>0 && IGNORE_SCALING(mode))
  {
    __LICE_SCU(destbm_w);
    __LICE_SCU(destbm_h);
  }

  if (glyph_span < 0) alphas += -glyph_span * (glyph_h-1);

  const int ia= (int)(alpha*256.0f);

  int src_x = 0, src_y = 0, src_w = glyph_w, src_h = glyph_h;
  if (x <= -src_w || y <= -src_h) return;
  
  if (x < 0) {
    src_x -= x;
    if (src_x < 0) return; // overflow
    src_w += x;
    x = 0;
  }
  if (y < 0) {
    src_y -= y;
    if (src_y < 0) return; // overflow
    src_h += y;
    y = 0;
  }

  if (src_w < 0 || src_h < 0 || x >= destbm_w || y >= destbm_h) return;

  if (src_h > destbm_h-y) src_h = destbm_h-y;
  if (src_w > destbm_w-x) src_w = destbm_w-x;
  
  if (src_w < 1 || src_h < 1) return;

  if (__sc>0 && !IGNORE_SCALING(mode))
  {
    __LICE_SCU(destbm_w);
    __LICE_SCU(destbm_h);
    __LICE_SC(x);
    __LICE_SC(y);
  }

  LICE_pixel* destpx = dest->getBits();
  int span = dest->getRowSpan();
  if (dest->isFlipped()) {
    destpx += (destbm_h-y-1)*span+x;
    span = -span;
  }
  else {
    destpx += y*dest->getRowSpan()+x;
  }

  const LICE_pixel_chan* srcalpha = alphas+src_y*glyph_span+src_x;

  if (__sc>0 && !IGNORE_SCALING(mode))
  {
#define __LICE__ACTION(COMBFUNC)  GlyphDrawImpl<COMBFUNC>::DrawGlyphScale(srcalpha,destpx, src_w, src_h, color,span,glyph_span,ia,__sc)
	__LICE_ACTION_NOSRCALPHA(mode, ia, false);
#undef __LICE__ACTION
  }
  else
  {
#define __LICE__ACTION(COMBFUNC)  GlyphDrawImpl<COMBFUNC>::DrawGlyph(srcalpha,destpx, src_w, src_h, color,span,glyph_span,ia)
	__LICE_ACTION_NOSRCALPHA(mode, ia, false);
#undef __LICE__ACTION
  }
}

void LICE_DrawGlyph(LICE_IBitmap* dest, int x, int y, LICE_pixel color, const LICE_pixel_chan* alphas, int glyph_w, int glyph_h, float alpha, int mode)
{
  LICE_DrawGlyphEx(dest,x,y,color,alphas,glyph_w,glyph_w,glyph_h,alpha,mode);
}


void LICE_DrawMonoGlyph(LICE_IBitmap* dest, int x, int y, LICE_pixel color, const unsigned char *alphabits, int glyph_w, int glyph_span, int glyph_h, float alpha, int mode)
{
  if (!dest) return;

  int destbm_w = dest->getWidth(), destbm_h = dest->getHeight();
  const int __sc = (int)dest->Extended(LICE_EXT_GET_SCALING,NULL);
  if (__sc>0 && IGNORE_SCALING(mode))
  {
    __LICE_SCU(destbm_w);
    __LICE_SCU(destbm_h);
  }

  if (glyph_span < 0) alphabits += -glyph_span * (glyph_h-1);

  const int ia= (int)(alpha*256.0f);

  int src_x = 0, src_y = 0, src_w = glyph_w, src_h = glyph_h;
  if (x <= -src_w || y <= -src_h) return;
  
  if (x < 0) {
    src_x -= x;
    src_w += x;
    x = 0;
  }
  if (y < 0) {
    src_y -= y;
    src_h += y;
    y = 0;
  }

  if (src_w < 0 || src_h < 0 || x >= destbm_w || y >= destbm_h) return;

  if (src_h > destbm_h-y) src_h = destbm_h-y;
  if (src_w > destbm_w-x) src_w = destbm_w-x;
  
  if (src_w < 1 || src_h < 1) return;

  if (__sc>0 && !IGNORE_SCALING(mode))
  {
    __LICE_SCU(destbm_w);
    __LICE_SCU(destbm_h);
    __LICE_SC(x);
    __LICE_SC(y);
  }

  LICE_pixel* destpx = dest->getBits();
  int span = dest->getRowSpan();
  if (dest->isFlipped()) {
    destpx += (destbm_h-y-1)*span+x;
    span = -span;
  }
  else {
    destpx += y*dest->getRowSpan()+x;
  }

  const unsigned char * srcalpha = alphabits+src_y*glyph_span+src_x;
  if (__sc>0 && !IGNORE_SCALING(mode))
  {
#define __LICE__ACTION(COMBFUNC)  GlyphDrawImpl<COMBFUNC>::DrawGlyphMonoScale(srcalpha,destpx, src_w, src_h, color,span,glyph_span,ia,__sc)
	__LICE_ACTION_NOSRCALPHA(mode, ia, false);
#undef __LICE__ACTION
  }
  else
  {
#define __LICE__ACTION(COMBFUNC)  GlyphDrawImpl<COMBFUNC>::DrawGlyphMono(srcalpha,destpx, src_w, src_h, color,span,glyph_span,ia)
	__LICE_ACTION_NOSRCALPHA(mode, ia, false);
#undef __LICE__ACTION
  }
}

void LICE_HalveBlitAA(LICE_IBitmap *dest, LICE_IBitmap *src)
{
  if (!dest||!src) return; 
  int w = dest->getWidth();
  if (w > src->getWidth()/2) w=src->getWidth()/2;
  int h = dest->getHeight();
  if (h > src->getHeight()/2) h=src->getHeight()/2;
  int src_span = src->getRowSpan();
  int dest_span = dest->getRowSpan();
  const LICE_pixel *srcptr = src->getBits();
  LICE_pixel *destptr = dest->getBits();

  while (h--)
  {
    const LICE_pixel *sp = srcptr;
    const LICE_pixel *sp2 = srcptr+src_span;
    LICE_pixel *dp = destptr;
    int x=w/2;

    // this is begging for SSE intrinsics, but we never use this function so leave it as-is :)

      // perhaps we should use more precision rather than chopping each src pixel to 6 bits, but oh well
    while (x--) // unroll 2px at a time, about 5% faster on core2 and ICC
    {
      int r1=((sp[0]>>2)&0x3f3f3f3f);
      int r2=((sp[2]>>2)&0x3f3f3f3f);
      r1 += ((sp[1]>>2)&0x3f3f3f3f);
      r2 += ((sp[3]>>2)&0x3f3f3f3f);
      dp[0] = r1  +
        ((sp2[0]>>2)&0x3f3f3f3f) +
        ((sp2[1]>>2)&0x3f3f3f3f);
      dp[1] = r2  +
        ((sp2[2]>>2)&0x3f3f3f3f) +
        ((sp2[3]>>2)&0x3f3f3f3f);
      sp+=4;
      sp2+=4;
      dp+=2;
    }
    if (w&1)
    {
      *dp =
        ((sp[0]>>2)&0x3f3f3f3f) +
        ((sp[1]>>2)&0x3f3f3f3f) +
        ((sp2[0]>>2)&0x3f3f3f3f) +
        ((sp2[1]>>2)&0x3f3f3f3f);
    }
    srcptr+=src_span*2;
    destptr+=dest_span;
  }
}

#endif // LICE_NO_MISC_SUPPORT

int LICE_BitmapCmp(LICE_IBitmap* a, LICE_IBitmap* b, int *coordsOut)
{
  return LICE_BitmapCmpEx(a,b,LICE_RGBA(255,255,255,255),coordsOut);
}

int LICE_BitmapCmpEx(LICE_IBitmap* a, LICE_IBitmap* b, LICE_pixel mask, int *coordsOut)
{
  if (!a || !b) {
    if (!a && b) return -1;
    if (a && !b) return 1;
    return 0;
  }

  int aw = a->getWidth(), bw = b->getWidth();
  if (aw != bw) return bw-aw;
  int ah = a->getHeight(), bh = b->getHeight();
  if (ah != bh) return bh-ah;
  
  //coordsOut
  const LICE_pixel *px1 = a->getBits();
  const LICE_pixel *px2 = b->getBits();
  int span1 = a->getRowSpan();
  int span2 = b->getRowSpan();
  if (a->isFlipped())
  {
    px1+=span1*(ah-1);
    span1=-span1;
  }
  if (b->isFlipped())
  {
    px2+=span2*(ah-1);
    span2=-span2;
  }

  int y;
  if (!coordsOut)
  {
    if (mask == (LICE_pixel)LICE_RGBA(255,255,255,255))
      for (y=0; y < ah; y ++)
      {
        const int dv = memcmp(px1,px2,aw*sizeof(LICE_pixel));
        if (dv) return dv;
        px1+=span1;
        px2+=span2;
      }
    else
      for (y=0; y < ah; y ++)
      {
        const LICE_pixel *ptr1 = px1, *ptr2 = px2;
        int x=aw;
        while (x--)
          if ((*ptr1++ ^ *ptr2++) & mask) return true;
        px1+=span1;
        px2+=span2;
      }
  }
  else
  {
    int x;

    // find first row that differs
    for (y=0; y < ah; y ++)
    {
      // check left side
      for (x=0;x<aw && !((px1[x]^px2[x])&mask);x++);
      if (x < aw) break;

      px1+=span1;
      px2+=span2;
    }
    if (y>=ah) 
    {
      memset(coordsOut,0,4*sizeof(int));
      return 0; // no differences
    }

    int miny=y;
    int minx=x;
    // scan right edge of top differing row
    for (x=aw-1;x>minx && !((px1[x]^px2[x])&mask);x--);
    int maxx=x;

    // find last row that differs
    px1+=span1 * (ah-1-y);
    px2+=span2 * (ah-1-y);
    for (y = ah-1; y > miny; y --)
    {
      // check left side
      for (x=0;x<aw && !((px1[x]^px2[x])&mask);x++);
      if (x < aw) 
      {
        if (x < minx) minx=x;
        break;
      }
      px1-=span1;
      px2-=span2;
    }
    int maxy=y;

    if (y > miny)
    {
      // scan right edge of bottom row that differs
      for (x=aw-1;x>maxx && !((px1[x]^px2[x])&mask);x--);
      maxx=x;
    }


    // find min/max x that differ
    px1+=span1 * (miny+1-y);
    px2+=span2 * (miny+1-y);
    for (y=miny+1;y<maxy && (minx>0 || maxx<aw-1);y++) 
    {
      for (x=0;x<minx && !((px1[x]^px2[x])&mask);x++);
      minx=x;
      for (x=aw-1;x>maxx && !((px1[x]^px2[x])&mask);x--);
      maxx=x;

      px1+=span1;
      px2+=span2;
    }

    coordsOut[0]=minx;
    coordsOut[1]=miny;
    coordsOut[2]=maxx-minx+1;
    coordsOut[3]=maxy-miny+1;

    return 1;
  }

  return 0;
}

unsigned short _LICE_RGB2HSV_invtab[256]={ // 65536/idx - 1
  0,      0xffff, 0x7fff, 0x5554, 0x3fff, 0x3332, 0x2aa9, 0x2491,
  0x1fff, 0x1c70, 0x1998, 0x1744, 0x1554, 0x13b0, 0x1248, 0x1110,
  0x0fff, 0x0f0e, 0x0e37, 0x0d78, 0x0ccb, 0x0c2f, 0x0ba1, 0x0b20,
  0x0aa9, 0x0a3c, 0x09d7, 0x097a, 0x0923, 0x08d2, 0x0887, 0x0841,
  0x07ff, 0x07c0, 0x0786, 0x074f, 0x071b, 0x06ea, 0x06bb, 0x068f,
  0x0665, 0x063d, 0x0617, 0x05f3, 0x05d0, 0x05af, 0x058f, 0x0571,
  0x0554, 0x0538, 0x051d, 0x0504, 0x04eb, 0x04d3, 0x04bc, 0x04a6,
  0x0491, 0x047c, 0x0468, 0x0455, 0x0443, 0x0431, 0x0420, 0x040f,
  0x03ff, 0x03ef, 0x03df, 0x03d1, 0x03c2, 0x03b4, 0x03a7, 0x039a,
  0x038d, 0x0380, 0x0374, 0x0368, 0x035d, 0x0352, 0x0347, 0x033c,
  0x0332, 0x0328, 0x031e, 0x0314, 0x030b, 0x0302, 0x02f9, 0x02f0,
  0x02e7, 0x02df, 0x02d7, 0x02cf, 0x02c7, 0x02bf, 0x02b8, 0x02b0,
  0x02a9, 0x02a2, 0x029b, 0x0294, 0x028e, 0x0287, 0x0281, 0x027b,
  0x0275, 0x026f, 0x0269, 0x0263, 0x025d, 0x0258, 0x0252, 0x024d,
  0x0248, 0x0242, 0x023d, 0x0238, 0x0233, 0x022f, 0x022a, 0x0225,
  0x0221, 0x021c, 0x0218, 0x0213, 0x020f, 0x020b, 0x0207, 0x0203,
  0x01ff, 0x01fb, 0x01f7, 0x01f3, 0x01ef, 0x01eb, 0x01e8, 0x01e4,
  0x01e0, 0x01dd, 0x01d9, 0x01d6, 0x01d3, 0x01cf, 0x01cc, 0x01c9,
  0x01c6, 0x01c2, 0x01bf, 0x01bc, 0x01b9, 0x01b6, 0x01b3, 0x01b1,
  0x01ae, 0x01ab, 0x01a8, 0x01a5, 0x01a3, 0x01a0, 0x019d, 0x019b,
  0x0198, 0x0196, 0x0193, 0x0191, 0x018e, 0x018c, 0x0189, 0x0187,
  0x0185, 0x0182, 0x0180, 0x017e, 0x017c, 0x0179, 0x0177, 0x0175,
  0x0173, 0x0171, 0x016f, 0x016d, 0x016b, 0x0169, 0x0167, 0x0165,
  0x0163, 0x0161, 0x015f, 0x015d, 0x015b, 0x0159, 0x0157, 0x0156,
  0x0154, 0x0152, 0x0150, 0x014f, 0x014d, 0x014b, 0x0149, 0x0148,
  0x0146, 0x0145, 0x0143, 0x0141, 0x0140, 0x013e, 0x013d, 0x013b,
  0x013a, 0x0138, 0x0137, 0x0135, 0x0134, 0x0132, 0x0131, 0x012f,
  0x012e, 0x012d, 0x012b, 0x012a, 0x0128, 0x0127, 0x0126, 0x0124,
  0x0123, 0x0122, 0x0120, 0x011f, 0x011e, 0x011d, 0x011b, 0x011a,
  0x0119, 0x0118, 0x0117, 0x0115, 0x0114, 0x0113, 0x0112, 0x0111,
  0x0110, 0x010e, 0x010d, 0x010c, 0x010b, 0x010a, 0x0109, 0x0108,
  0x0107, 0x0106, 0x0105, 0x0104, 0x0103, 0x0102, 0x0101, 0x0100,
};


#endif//__LICE_CPP_IMPLEMENTED__
