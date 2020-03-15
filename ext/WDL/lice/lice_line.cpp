#include "lice.h"
#include "lice_combine.h"
#include "lice_extended.h"
#include <math.h>
#include <stdio.h>
//#include <assert.h>

#define IGNORE_SCALING(mode) ((mode)&LICE_BLIT_IGNORE_SCALING)

template <class T> inline void SWAP(T& a, T& b) { T tmp = a; a = b; b = tmp; }

enum { eOK = 0, eXLo = 1, eXHi = 2, eYLo = 4, eYHi = 8 };

static int OffscreenTest(int x, int y, int nX, int nY)
{
  int e = eOK;
  if (x < 0) e |= eXLo; 
  else if (x >= nX) e |= eXHi; 
  if (y < 0) e |= eYLo; 
  else if (y >= nY) e |= eYHi; 
  return e;
}

// Cohen-Sutherland.  Returns false if the line is entirely offscreen.
static bool ClipLine(int* pX1, int* pY1, int* pX2, int* pY2, int nX, int nY)
{
  int x1 = *pX1, y1 = *pY1, x2 = *pX2, y2 = *pY2;
  int e1 = OffscreenTest(x1, y1, nX, nY); 
  int e2 = OffscreenTest(x2, y2, nX, nY);
  int timeout = 32;
  bool accept = false, done = false;
  do
  {
    if (!(e1 | e2)) {
      accept = done = true;
    }
    else
    if (e1 & e2) {
      done = true;	// Line is entirely offscreen.
    }
    else { 
      int x, y;
      int eOut = e1 ? e1 : e2;
      if (eOut & eYHi) {
        x = x1 + (int) ((double) (x2 - x1) * (double) (nY - y1) / (double) (y2 - y1));
        y = nY - 1;
      }
      else
      if (eOut & eYLo) {
        x = x1 + (int) ((double) (x2 - x1) * (double) -y1 / (double) (y2 - y1));
        y = 0;
      }
      else 
      if (eOut & eXHi) {
        y = y1 + (int) ((double) (y2 - y1) * (double) (nX - x1) / (double) (x2 - x1));
        x = nX - 1;
      }
      else {
        y = y1 + (int) ((double) (y2 - y1) * (double) -x1 / (double) (x2 - x1));
        x = 0;
      }

      if (eOut == e1) { 
        x1 = x; 
        y1 = y;
        e1 = OffscreenTest(x1, y1, nX, nY);
      }
      else {
        x2 = x;
        y2 = y;
        e2 = OffscreenTest(x2, y2, nX, nY);
      }
    }
  }
  while (!done && timeout--);

  *pX1 = x1;
  *pY1 = y1;
  *pX2 = x2;
  *pY2 = y2;
  return accept;
}

template<class T> static int OffscreenFTest(T x, T y, T w, T h)
{
  int e = eOK;
  if (x < 0.0f) e |= eXLo; 
  else if (x >= w) e |= eXHi; 
  if (y < 0.0f) e |= eYLo; 
  else if (y >= h) e |= eYHi; 
  return e;
}

template<class T> static bool ClipFLine(T * x1, T * y1, T * x2, T *y2, int w, int h)
{
  T tx1 = *x1, ty1 = *y1, tx2 = *x2, ty2 = *y2;
  T tw = (T)(w-1), th = (T)(h-1);
  if (!lice_isfinite(tx1) || !lice_isfinite(tx2) || 
      !lice_isfinite(ty1) || !lice_isfinite(ty2)) return false;
  
  int e1 = OffscreenFTest(tx1, ty1, tw, th); 
  int e2 = OffscreenFTest(tx2, ty2, tw, th);
  
  int timeout = 32;
  bool accept = false, done = false;
  do
  {
    if (!(e1|e2)) 
    {
      accept = done = true;
    }
    else
    if (e1&e2) 
    {
      done = true;	// Line is entirely offscreen.
    }
    else 
    { 
      T x, y;
      int eOut = (e1 ? e1 : e2);
      if (eOut&eYHi) 
      {
        x = tx1+(tx2-tx1)*(th-ty1)/(ty2-ty1);
        y = th-1.0f;
      }
      else if (eOut&eYLo) 
      {
        x = tx1+(tx2-tx1)*ty1/(ty1-ty2);
        y = 0.0f;
      }
      else if (eOut&eXHi) 
      {
        y = ty1+(ty2-ty1)*(tw-tx1)/(tx2-tx1);
        x = tw-1.0f;
      }
      else
      {
        y = ty1+(ty2-ty1)*tx1/(tx1-tx2);
        x = 0.0f;
      }

      if (eOut == e1) 
      { 
        tx1 = x; 
        ty1 = y;
        e1 = OffscreenFTest(tx1, ty1, tw, th);
      }
      else 
      {
        tx2 = x;
        ty2 = y;
        e2 = OffscreenFTest(tx2, ty2, tw, th);
      }
    }
  }
  while (!done && timeout--);

  *x1 = tx1;
  *y1 = ty1;
  *x2 = tx2;
  *y2 = ty2;
  return accept;
}


inline static void LICE_DiagLineFAST(LICE_pixel *px, int span, int n, int xstep, int ystep, LICE_pixel color, bool aa)
{
  int step = xstep+ystep;
  if (aa)
  {
    LICE_pixel color75 = ((color>>1)&0x7f7f7f7f)+((color>>2)&0x3f3f3f3f);
    LICE_pixel color25 = (color>>2)&0x3f3f3f3f;
    while (n--)
    {
      _LICE_CombinePixelsThreeQuarterMix2FAST::doPixFAST(px, color75);    
      _LICE_CombinePixelsQuarterMix2FAST::doPixFAST(px+xstep, color25);
      _LICE_CombinePixelsQuarterMix2FAST::doPixFAST(px+ystep, color25);
      px += step;
    }
    _LICE_CombinePixelsThreeQuarterMix2FAST::doPixFAST(px, color75);  
  }
  else
  {
    ++n;
    while (n--)
    {
      *px = color;
      px += step;
    }
  }
}

inline static void LICE_DottedVertLineFAST(LICE_IBitmap* dest, int x, int y1, int y2, LICE_pixel color)
{
  int span = dest->getRowSpan();
  LICE_pixel* px = dest->getBits()+y1*span+x;

  int n = (y2-y1+1)/2;
  while (n--)
  {
    *px = color;
    px += 2*span;
  }
}

// this is the white-color table, doing this properly requires correcting the destination color specifically
#define DO_AA_GAMMA_CORRECT 0
#if DO_AA_GAMMA_CORRECT
static unsigned char AA_GAMMA_CORRECT[256] =
{  
  // 1.8 gamma
  0,11,17,21,25,28,31,34,37,39,42,44,46,48,50,52,54,56,58,60,61,63,65,67,68,70,71,73,74,76,77,79,80,81,83,84,85,87,88,89,91,92,93,94,96,97,98,99,100,101,103,104,105,106,107,108,109,110,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,129,130,131,132,132,133,134,135,136,137,138,139,140,141,142,142,143,144,145,146,147,148,149,149,150,151,152,153,154,154,155,156,157,158,159,159,160,161,162,163,163,164,165,166,166,167,168,169,170,170,171,172,173,173,174,175,176,176,177,178,179,179,180,181,182,182,183,184,184,185,186,187,187,188,189,189,190,191,191,192,193,194,194,195,196,196,197,198,198,199,200,200,201,202,202,203,204,204,205,206,206,207,208,208,209,210,210,211,212,212,213,214,214,215,215,216,217,217,218,219,219,220,220,221,222,222,223,224,224,225,225,226,227,227,228,228,229,230,230,231,231,232,233,233,234,234,235,236,236,237,237,238,239,239,240,240,241,241,242,243,243,244,244,245,245,246,247,247,248,248,249,249,250,251,251,252,252,253,253,254,255

  // 2.0 gamma
  //0,15,22,27,31,35,39,42,45,47,50,52,55,57,59,61,63,65,67,69,71,73,74,76,78,79,81,82,84,85,87,88,90,91,93,94,95,97,98,99,100,102,103,104,105,107,108,109,110,111,112,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,141,142,143,144,145,146,147,148,148,149,150,151,152,153,153,154,155,156,157,158,158,159,160,161,162,162,163,164,165,165,166,167,168,168,169,170,171,171,172,173,174,174,175,176,177,177,178,179,179,180,181,182,182,183,184,184,185,186,186,187,188,188,189,190,190,191,192,192,193,194,194,195,196,196,197,198,198,199,200,200,201,201,202,203,203,204,205,205,206,206,207,208,208,209,210,210,211,211,212,213,213,214,214,215,216,216,217,217,218,218,219,220,220,221,221,222,222,223,224,224,225,225,226,226,227,228,228,229,229,230,230,231,231,232,233,233,234,234,235,235,236,236,237,237,238,238,239,240,240,241,241,242,242,243,243,244,244,245,245,246,246,247,247,248,248,249,249,250,250,251,251,252,252,253,253,254,255

  // 2.2 gamma
  //0,20,28,33,38,42,46,49,52,55,58,61,63,65,68,70,72,74,76,78,80,81,83,85,87,88,90,91,93,94,96,97,99,100,102,103,104,106,107,108,109,111,112,113,114,115,117,118,119,120,121,122,123,124,125,126,128,129,130,131,132,133,134,135,136,136,137,138,139,140,141,142,143,144,145,146,147,147,148,149,150,151,152,153,153,154,155,156,157,158,158,159,160,161,162,162,163,164,165,165,166,167,168,168,169,170,171,171,172,173,174,174,175,176,176,177,178,178,179,180,181,181,182,183,183,184,185,185,186,187,187,188,189,189,190,190,191,192,192,193,194,194,195,196,196,197,197,198,199,199,200,200,201,202,202,203,203,204,205,205,206,206,207,208,208,209,209,210,210,211,212,212,213,213,214,214,215,216,216,217,217,218,218,219,219,220,220,221,222,222,223,223,224,224,225,225,226,226,227,227,228,228,229,229,230,230,231,231,232,232,233,233,234,234,235,235,236,236,237,237,238,238,239,239,240,240,241,241,242,242,243,243,244,244,245,245,246,246,247,247,248,248,249,249,249,250,250,251,251,252,252,253,253,254,254,255

  // 2.6 gamma
  //0,30,39,46,51,56,60,63,67,70,73,76,78,81,83,85,87,89,91,93,95,97,99,101,102,104,105,107,109,110,111,113,114,116,117,118,120,121,122,123,125,126,127,128,129,130,131,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,148,149,150,151,152,153,154,155,155,156,157,158,159,160,160,161,162,163,164,164,165,166,167,167,168,169,170,170,171,172,173,173,174,175,175,176,177,177,178,179,179,180,181,181,182,183,183,184,185,185,186,187,187,188,188,189,190,190,191,192,192,193,193,194,195,195,196,196,197,197,198,199,199,200,200,201,201,202,203,203,204,204,205,205,206,206,207,207,208,208,209,210,210,211,211,212,212,213,213,214,214,215,215,216,216,217,217,218,218,219,219,220,220,221,221,222,222,223,223,223,224,224,225,225,226,226,227,227,228,228,229,229,230,230,230,231,231,232,232,233,233,234,234,234,235,235,236,236,237,237,237,238,238,239,239,240,240,240,241,241,242,242,243,243,243,244,244,245,245,245,246,246,247,247,247,248,248,249,249,249,250,250,251,251,251,252,252,253,253,253,254,254,255
};
#endif

static void GetAAPxWeight(int err, int alpha, int* wt, int* iwt)
{
  int i = err>>8;
  int w = 255-i;

#if DO_AA_GAMMA_CORRECT
  w = AA_GAMMA_CORRECT[w];
  i = AA_GAMMA_CORRECT[i];
#endif

  w = (alpha*w) >> 8;
  i = (alpha*i) >> 8;
  *wt = w;
  *iwt = i;
}

static void GetAAPxWeightFAST(int err, int* wt, int* iwt)
{
  int i = err>>8;
  int w = 255-i;

#if DO_AA_GAMMA_CORRECT
  w = AA_GAMMA_CORRECT[w];
  i = AA_GAMMA_CORRECT[i];
#endif

  *wt = w;
  *iwt = i;
}
template <class COMBFUNC> class __LICE_LineClassSimple
{
public:
  static void LICE_VertLineFAST(LICE_pixel *px, int span, int len, LICE_pixel color)
  {
    while (len--)
    {
      COMBFUNC::doPixFAST(px, color);
      px+=span;
    }
  }

  static void LICE_HorizLineFAST(LICE_pixel *px, int n, LICE_pixel color)
  {
    while (n--)
    {
      COMBFUNC::doPixFAST(px, color);
      px++;
    }
  }
  static void LICE_VertLine(LICE_pixel *px, int span, int len, int color, int aw)
  {
    int r = LICE_GETR(color), g = LICE_GETG(color), b = LICE_GETB(color), a = LICE_GETA(color);
    while (len--)
    {
      COMBFUNC::doPix((LICE_pixel_chan*) px, r, g, b, a, aw);
      px+=span;
    }
  }

  static void LICE_HorizLine(LICE_pixel *px, int n, LICE_pixel color, int aw)
  {
    int r = LICE_GETR(color), g = LICE_GETG(color), b = LICE_GETB(color), a = LICE_GETA(color);

    while (n--)
    {
      COMBFUNC::doPix((LICE_pixel_chan*) px, r, g, b, a, aw);
      px++;
    }
  }
  static void LICE_DiagLine(LICE_pixel *px, int span, int n, int xstep, int ystep, LICE_pixel color, int aw)
  {
    int r = LICE_GETR(color), g = LICE_GETG(color), b = LICE_GETB(color), a = LICE_GETA(color);
    int step = xstep+ystep;

    for (int i = 0; i <= n; ++i, px += step) 
    {
      COMBFUNC::doPix((LICE_pixel_chan*) px, r, g, b, a, aw);
    }
  }
  static void LICE_DiagLineAA(LICE_pixel *px, int span, int n, int xstep, int ystep, LICE_pixel color, int aw)
  {
    int r = LICE_GETR(color), g = LICE_GETG(color), b = LICE_GETB(color), a = LICE_GETA(color);
    int step = xstep+ystep;

#if DO_AA_GAMMA_CORRECT
    int iw = aw*AA_GAMMA_CORRECT[256*3/4]/256;
    int dw = aw*AA_GAMMA_CORRECT[256/4]/256;
#else
    int iw = aw*3/4;
    int dw = aw/4;
#endif
    for (int i = 0; i < n; ++i, px += step)
    {
      COMBFUNC::doPix((LICE_pixel_chan*) px, r, g, b, a, iw);       
      COMBFUNC::doPix((LICE_pixel_chan*) (px+xstep), r, g, b, a, dw); 
      COMBFUNC::doPix((LICE_pixel_chan*) (px+ystep), r, g, b, a, dw);
    }
    COMBFUNC::doPix((LICE_pixel_chan*) px, r, g, b, a, iw);
  }
};


#ifndef LICE_FAVOR_SIZE
template<class COMBFUNC> 
#endif
class __LICE_LineClass
{
public:

#ifdef LICE_FAVOR_SIZE
  #define DOPIX(pout,r,g,b,a,ia) combFunc(pout,r,g,b,a,ia);
#else
    #define DOPIX(pout,r,g,b,a,ia) COMBFUNC::doPix(pout,r,g,b,a,ia);
#endif

  static void DashedLine(LICE_IBitmap* dest, int x1, int y1, int x2, int y2, int pxon, int pxoff, LICE_pixel color, int aw
#ifdef LICE_FAVOR_SIZE
                          , LICE_COMBINEFUNC combFunc
#endif
    )
  {
    int span = dest->getRowSpan();
    LICE_pixel* px = dest->getBits()+y1*span+x1;
    int r = LICE_GETR(color), g = LICE_GETG(color), b = LICE_GETB(color), a = LICE_GETA(color);

    if (x1 == x2)
    {
      int i, y;
      for (y = y1; y < y2-pxon; y += pxon+pxoff)
      {
        for (i = 0; i < pxon; ++i, px += span) DOPIX((LICE_pixel_chan*) px, r, g, b, a, aw)
        px += pxoff*span;
      }
      for (i = 0; i < lice_min(pxon, y2-y); ++i, px += span) DOPIX((LICE_pixel_chan*) px, r, g, b, a, aw)
    }
    else if (y1 == y2)
    {
      int i, x;
      for (x = x1; x < x2-pxon; x += pxon+pxoff)
      {
        for (i = 0; i < pxon; ++i, ++px) DOPIX((LICE_pixel_chan*) px, r, g, b, a, aw)
        px += pxoff;
      }
      for (i = 0; i < lice_min(pxon, x2-x); ++i, ++px) DOPIX((LICE_pixel_chan*) px, r, g, b, a, aw)
    }
  }




  static void LICE_LineImpl(LICE_pixel *px, LICE_pixel *px2, int derr, int astep, int da, int bstep, LICE_pixel color, int aw, bool aa
#ifdef LICE_FAVOR_SIZE
                          , LICE_COMBINEFUNC combFunc
#endif
    )
  {
    int r = LICE_GETR(color), g = LICE_GETG(color), b = LICE_GETB(color), a = LICE_GETA(color);

    int err = 0;
    int i;
    int n = (da+1)/2;

    if (aa) 
    {
      DOPIX((LICE_pixel_chan*) px, r, g, b, a, aw)
      DOPIX((LICE_pixel_chan*) px2, r, g, b, a, aw)
      px += astep;
      px2 -= astep;
      err = derr;        
          
      if (aw == 256)
      {
        for (i = 1; i < n; ++i)
        {
          int wt, iwt;
          GetAAPxWeightFAST(err, &wt, &iwt);
          DOPIX((LICE_pixel_chan*)px, r, g, b, a, wt)
          DOPIX((LICE_pixel_chan*)(px+bstep), r, g, b, a, iwt)
          DOPIX((LICE_pixel_chan*)px2, r, g, b, a, wt)
          DOPIX((LICE_pixel_chan*)(px2-bstep), r, g, b, a, iwt)

          err += derr;
          if (err >= 65536)
          {
            px += bstep;
            px2 -= bstep;
            err -= 65536;
          }
          px += astep;
          px2 -= astep;
        }
      }
      else  // alpha<256
      {
        for (i = 1; i < n; ++i)
        {
          int wt, iwt;
          GetAAPxWeight(err, aw, &wt, &iwt);
          DOPIX((LICE_pixel_chan*)px, r, g, b, a, wt)
          DOPIX((LICE_pixel_chan*)(px+bstep), r, g, b, a, iwt)
          DOPIX((LICE_pixel_chan*)px2, r, g, b, a, wt)
          DOPIX((LICE_pixel_chan*)(px2-bstep), r, g, b, a, iwt)

          err += derr;
          if (err >= 65536)
          {
            px += bstep;
            px2 -= bstep;
            err -= 65536;
          }
          px += astep;
          px2 -= astep;
        }
      }
      if (!(da%2))
      {
        int wt, iwt;
        if (aw == 256) GetAAPxWeightFAST(err, &wt, &iwt);
        else GetAAPxWeight(err, aw, &wt, &iwt);
        DOPIX((LICE_pixel_chan*)px, r, g, b, a, wt)
        DOPIX((LICE_pixel_chan*)(px+bstep), r, g, b, a, iwt)
      }
    } 
    else  // not aa
    {
      for (i = 0; i < n; ++i) 
      {
        DOPIX((LICE_pixel_chan*)px, r, g, b, a, aw)
        DOPIX((LICE_pixel_chan*)px2, r, g, b, a, aw)
        err += derr;
        if (err >= 65536/2) 
        {
          px += bstep;
          px2 -= bstep;
          err -= 65536;
        }

        px += astep;
        px2 -= astep;
      }
      if (!(da%2))
      {
        DOPIX((LICE_pixel_chan*)px, r, g, b, a, aw)
      }
    }
  }

  static void LICE_FLineImpl(LICE_pixel *px, int n , int err, int derr, int astep, int bstep, LICE_pixel color, int aw
#ifdef LICE_FAVOR_SIZE
                          , LICE_COMBINEFUNC combFunc
#endif
    
    ) // only does AA
  {    
    int r = LICE_GETR(color), g = LICE_GETG(color), b = LICE_GETB(color), a = LICE_GETA(color);


    int wt, iwt;
    int i;

    if (aw == 256)
    {
      for (i = 0; i <= n; ++i)
      {
        GetAAPxWeightFAST(err, &wt, &iwt);
        DOPIX((LICE_pixel_chan*)px, r, g, b, a, wt)
        DOPIX((LICE_pixel_chan*)(px+bstep), r, g, b, a, iwt)

        err += derr;
        if (err >= 65536)
        {
          px += bstep;
          err -= 65536;
        }
        px += astep;
      }  
    }
    else // alpha != 256
    {
      for (i = 0; i <= n; ++i)
      {
        GetAAPxWeight(err, aw, &wt, &iwt);
        DOPIX((LICE_pixel_chan*)px, r, g, b, a, wt)
        DOPIX((LICE_pixel_chan*)(px+bstep), r, g, b, a, iwt)

        err += derr;
        if (err >= 65536)
        {
          px += bstep;
          err -= 65536;
        }
        px += astep;
      }     
    }
  }

  static void LICE_FLineImplFill(LICE_pixel *px, int n , int err, int derr, int astep, int bstep, LICE_pixel color, int aw,
      int fill_sz, int b_pos, unsigned int b_max
#ifdef LICE_FAVOR_SIZE
                          , LICE_COMBINEFUNC combFunc
#endif
    )
  {
    // fill_sz always >= 2
    int r = LICE_GETR(color), g = LICE_GETG(color), b = LICE_GETB(color), a = LICE_GETA(color);

    int wt, iwt;
    int i;

    const int dbpos = bstep < 0 ? -1 : 1;

    const int b_adj = -(fill_sz/2);
    b_pos += b_adj*dbpos;
    px += b_adj*bstep;

    fill_sz--; // fill size of 2 has one extra pixel in the middle, 2 AA pixels

    if (aw == 256)
    {
      for (i = 0; i <= n; ++i)
      {
        GetAAPxWeightFAST(err, &wt, &iwt);
        LICE_pixel *wr = px;
        unsigned int bp = b_pos;
        if (bp<b_max) { DOPIX((LICE_pixel_chan*)wr, r, g, b, a, wt)  }
        for (int j=0;j<fill_sz;j++)
        {
          wr += bstep;
          if ((bp+=dbpos)<b_max) { DOPIX((LICE_pixel_chan*)wr, r, g, b, a, 256) }
        }
        if ((bp+dbpos)<b_max) { DOPIX((LICE_pixel_chan*)(wr+bstep), r, g, b, a, iwt) }

        err += derr;
        if (err >= 65536)
        {
          px += bstep;
          b_pos += dbpos;
          err -= 65536;
        }
        px += astep;
      }
    }
    else // alpha != 256
    {
      for (i = 0; i <= n; ++i)
      {
        GetAAPxWeight(err, aw, &wt, &iwt);
        LICE_pixel *wr = px;
        unsigned int bp = b_pos;
        if (bp<b_max) { DOPIX((LICE_pixel_chan*)wr, r, g, b, a, wt) }
        for (int j=0;j<fill_sz;j++)
        {
          wr += bstep;
          if ((bp+=dbpos)<b_max) { DOPIX((LICE_pixel_chan*)wr, r, g, b, a, aw) }
        }
        if ((bp+dbpos)<b_max) { DOPIX((LICE_pixel_chan*)(wr+bstep), r, g, b, a, iwt) }

        err += derr;
        if (err >= 65536)
        {
          px += bstep;
          b_pos += dbpos;
          err -= 65536;
        }
        px += astep;
      }
    }
  }

#undef DOPIX
};


void LICE_Line(LICE_IBitmap *dest, int x1, int y1, int x2, int y2, LICE_pixel color, float alpha, int mode, bool aa)
{
  if (!dest) return;

  int w = dest->getWidth();
  int h = dest->getHeight();
  const int __sc = (int)dest->Extended(LICE_EXT_GET_SCALING,NULL);
  if (__sc>0)
  {
    __LICE_SCU(w);
    __LICE_SCU(h);
    if (!IGNORE_SCALING(mode))
    {
      __LICE_SC(x1);
      __LICE_SC(y1);
      __LICE_SC(x2);
      __LICE_SC(y2);
    }
  }

#ifndef DISABLE_LICE_EXTENSIONS
  if (dest->Extended(LICE_EXT_SUPPORTS_ID, (void*) LICE_EXT_LINE_ACCEL))
  {
    LICE_Ext_Line_acceldata data(x1, y1, x2, y2, color, alpha, mode, aa);
    if (dest->Extended(LICE_EXT_LINE_ACCEL, &data)) return;
  }
#endif

  if (dest->isFlipped()) 
  {
    y1 = h-1-y1;
    y2 = h-1-y2;
  }

	if (ClipLine(&x1, &y1, &x2, &y2, w, h)) 
  {
    int xdiff = x2-x1;
    if (y1 == y2) // horizontal line optimizations 
    {
      if (x1 > x2) SWAP(x1, x2);
      int span = dest->getRowSpan();
      LICE_pixel* px = dest->getBits()+y1*span+x1;
      int n=x2-x1+1;

      if ((mode&LICE_BLIT_MODE_MASK) == LICE_BLIT_MODE_COPY && alpha == 1.0f)
      {
        __LICE_LineClassSimple<_LICE_CombinePixelsClobberFAST>::LICE_HorizLineFAST(px, n, color);
      }
      else if ((mode&LICE_BLIT_MODE_MASK) == LICE_BLIT_MODE_COPY && alpha == 0.5f)
      {
        color = (color>>1)&0x7f7f7f7f;
        __LICE_LineClassSimple<_LICE_CombinePixelsHalfMix2FAST>::LICE_HorizLineFAST(px, n, color);
      }
      else if ((mode&LICE_BLIT_MODE_MASK) == LICE_BLIT_MODE_COPY && alpha == 0.25f)
      {
        color = (color>>2)&0x3f3f3f3f;
        __LICE_LineClassSimple<_LICE_CombinePixelsQuarterMix2FAST>::LICE_HorizLineFAST(px, n, color);
      }
      else if ((mode&LICE_BLIT_MODE_MASK) == LICE_BLIT_MODE_COPY && alpha == 0.75f)
      {
        color = ((color>>1)&0x7f7f7f7f)+((color>>2)&0x3f3f3f3f);
        __LICE_LineClassSimple<_LICE_CombinePixelsThreeQuarterMix2FAST>::LICE_HorizLineFAST(px, n, color);
      }
      else
      {
        int aw = (int)(256.0f*alpha);
#define __LICE__ACTION(COMBFUNC) __LICE_LineClassSimple<COMBFUNC>::LICE_HorizLine(px, n, color, aw)
        __LICE_ACTION_CONSTANTALPHA(mode, aw, false);
#undef __LICE__ACTION    
      }
    }
    else if (!xdiff)  // vertical line optimizations
    {
      if (y1 > y2) SWAP(y1, y2);
      int len=y2+1-y1;
      int span = dest->getRowSpan();
      LICE_pixel* px = dest->getBits()+y1*span+x1;
      int aw = (int)(256.0f*alpha);
      if ((mode&LICE_BLIT_MODE_MASK) == LICE_BLIT_MODE_COPY && alpha == 1.0f)
      {
        __LICE_LineClassSimple<_LICE_CombinePixelsClobberFAST>::LICE_VertLineFAST(px, span, len, color);
      }
      else if ((mode&LICE_BLIT_MODE_MASK) == LICE_BLIT_MODE_COPY && alpha == 0.5f)
      {
        color = (color>>1)&0x7f7f7f7f;
        __LICE_LineClassSimple<_LICE_CombinePixelsHalfMix2FAST>::LICE_VertLineFAST(px, span, len, color);
      }
      else if ((mode&LICE_BLIT_MODE_MASK) == LICE_BLIT_MODE_COPY && alpha == 0.25f)
      {
        color = (color>>2)&0x3f3f3f3f;
        __LICE_LineClassSimple<_LICE_CombinePixelsQuarterMix2FAST>::LICE_VertLineFAST(px, span, len, color);
      }
      else if ((mode&LICE_BLIT_MODE_MASK) == LICE_BLIT_MODE_COPY && alpha == 0.75f)
      {
        color = ((color>>1)&0x7f7f7f7f)+((color>>2)&0x3f3f3f3f);
        __LICE_LineClassSimple<_LICE_CombinePixelsThreeQuarterMix2FAST>::LICE_VertLineFAST(px, span, len, color);
      }
      else
      {
#define __LICE__ACTION(COMBFUNC) __LICE_LineClassSimple<COMBFUNC>::LICE_VertLine(px, span, len, color,aw)
        __LICE_ACTION_CONSTANTALPHA(mode, aw, false);
#undef __LICE__ACTION    
      }
    }
    else if ((xdiff=abs(xdiff)) == abs(y2-y1)) // diagonal line optimizations
    {
      int span = dest->getRowSpan();
      LICE_pixel* px = dest->getBits()+y1*span+x1;
      int aw = (int)(256.0f*alpha);
      int xstep = (x2 > x1 ? 1 : -1);
      int ystep = (y2 > y1 ? span : -span);
      if ((mode&LICE_BLIT_MODE_MASK) == LICE_BLIT_MODE_COPY && alpha == 1.0f)
      {
        LICE_DiagLineFAST(px,span, xdiff, xstep, ystep, color, aa);        
      }
      else
      {
        if (aa) 
        {
#define __LICE__ACTION(COMBFUNC) __LICE_LineClassSimple<COMBFUNC>::LICE_DiagLineAA(px,span, xdiff, xstep, ystep, color, aw)
          __LICE_ACTION_NOSRCALPHA(mode, aw, false);
#undef __LICE__ACTION
        }
        else 
        {
#define __LICE__ACTION(COMBFUNC) __LICE_LineClassSimple<COMBFUNC>::LICE_DiagLine(px,span, xdiff, xstep, ystep, color, aw)
          __LICE_ACTION_CONSTANTALPHA(mode, aw, false);
#undef __LICE__ACTION
        }
      }
    }
    else 
    {

      // common set-up for normal line draws

      int span = dest->getRowSpan();
      int aw = (int)(256.0f*alpha);
      LICE_pixel* px = dest->getBits()+y1*span+x1;
      LICE_pixel* px2 = dest->getBits()+y2*span+x2;

      int da, db;
      int astep, bstep;
      int dx = x2-x1;
      int dy = y2-y1;

      if (abs(dx) > abs(dy))
      {
        da = dx;
        db = dy;
        astep = 1;
        bstep = span;
      }
      else
      {
        da = dy;
        db = dx;
        astep = span;
        bstep = 1;
      }

      if (da < 0) 
      {
        da = -da;
        db = -db;
        SWAP(px, px2);
      }
      if (db < 0) 
      {
        db = -db;
        bstep = -bstep;
      }

      double dbda = (double)db/(double)da;

      int derr = (int)(dbda*65536.0);

#ifdef LICE_FAVOR_SIZE

      LICE_COMBINEFUNC blitfunc=NULL;      
      #define __LICE__ACTION(comb) blitfunc=comb::doPix;

#else
      #define __LICE__ACTION(COMBFUNC) __LICE_LineClass<COMBFUNC>::LICE_LineImpl(px,px2, derr, astep, da, bstep, color, aw, aa)	
#endif
            if (aa) 
            {
              __LICE_ACTION_NOSRCALPHA(mode, aw, false);
            }
            else 
            {
              __LICE_ACTION_CONSTANTALPHA(mode, aw, false);
            }

      #undef __LICE__ACTION

#ifdef LICE_FAVOR_SIZE
        if (blitfunc) __LICE_LineClass::LICE_LineImpl(px,px2, derr, astep, da, bstep, color, aw, aa, blitfunc);
#endif
		}
	}
}

void LICE_FLine(LICE_IBitmap* dest, float x1, float y1, float x2, float y2, LICE_pixel color, float alpha, int mode, bool aa)
{
  if (!dest) return;
  if (!aa)
  {
    LICE_Line(dest,(int)x1,(int)y1,(int)x2,(int)y2,color,alpha,mode,false);
    return;
  }

  int w = dest->getWidth();
  int h = dest->getHeight();
  if (dest->isFlipped()) 
  {
    y1 = (float)(h-1)-y1;
    y2 = (float)(h-1)-y2;
  }

  const int __sc = (int)dest->Extended(LICE_EXT_GET_SCALING,NULL);
  if (__sc>0)
  {
    __LICE_SCU(w);
    __LICE_SCU(h);
    if (!IGNORE_SCALING(mode))
    {
      __LICE_SC(x1);
      __LICE_SC(x2);
      __LICE_SC(y1);
      __LICE_SC(y2);
    }
  }

  if (ClipFLine(&x1, &y1, &x2, &y2, w, h))
  {
    if (x1 != x2 || y1 != y2) 
    {
      int span = dest->getRowSpan();
      int aw = (int)(256.0f*alpha);

      float a1, a2, b1, b2, da, db;
      int astep, bstep;
      float dx = x2-x1;
      float dy = y2-y1;

      if (fabs(dx) > fabs(dy))
      {
        a1 = x1;
        a2 = x2;
        b1 = y1;
        b2 = y2;
        da = dx;
        db = dy;
        astep = 1;
        bstep = span;
      }
      else
      {
        a1 = y1;
        a2 = y2;
        b1 = x1;
        b2 = x2;
        da = dy;
        db = dx;
        astep = span;
        bstep = 1;
      }

      if (da < 0.0f) 
      {      
        da = -da;
        db = -db;
        SWAP(a1, a2);
        SWAP(b1, b2);
      }
      if (db < 0.0f)
      {
        bstep = -bstep;
      }

      int n = (int)(floor(a2)-ceil(a1)); 
      float dbda = db/da;

      float ta = ceil(a1);
      float tb = b1+(ta-a1)*dbda;
      float bf = tb-floor(tb);
      int err = (int)(bf*65536.0f);
      if (bstep < 0) err = 65535-err;
      int derr = (int)(fabs(dbda)*65536.0f);    
      
      LICE_pixel* px = dest->getBits()+(int)ta*astep+(int)tb*abs(bstep);

      if (bstep < 0) px -= bstep;

#ifdef LICE_FAVOR_SIZE

      LICE_COMBINEFUNC blitfunc=NULL;      
      #define __LICE__ACTION(comb) blitfunc=comb::doPix;

#else

      #define __LICE__ACTION(COMBFUNC) __LICE_LineClass<COMBFUNC>::LICE_FLineImpl(px,n,err,derr,astep,bstep, color, aw)
#endif

      __LICE_ACTION_NOSRCALPHA(mode, aw, false);    

#ifdef LICE_FAVOR_SIZE
      if (blitfunc) __LICE_LineClass::LICE_FLineImpl(px,n,err,derr,astep,bstep, color, aw, blitfunc);
#endif

  #undef __LICE__ACTION
    }
  }
}

void LICE_DashedLine(LICE_IBitmap* dest, int x1, int y1, int x2, int y2, int pxon, int pxoff, LICE_pixel color, float alpha, int mode, bool aa) 
{  
  if (!dest) return;

  int w = dest->getWidth();
  int h = dest->getHeight();
  const int __sc = (int)dest->Extended(LICE_EXT_GET_SCALING,NULL);
  if (__sc>0)
  {
    __LICE_SCU(w);
    __LICE_SCU(h);
    if (!IGNORE_SCALING(mode))
    {
      __LICE_SC(x1);
      __LICE_SC(y1);
      __LICE_SC(x2);
      __LICE_SC(y2);
      __LICE_SCU(pxon);
      __LICE_SCU(pxoff);
    }
  }

#ifndef DISABLE_LICE_EXTENSIONS
  if (dest->Extended(LICE_EXT_SUPPORTS_ID, (void*) LICE_EXT_DASHEDLINE_ACCEL))
  {
    LICE_Ext_DashedLine_acceldata data(x1, y1, x2, y2, pxon, pxoff, color, alpha, mode, aa);
    if (dest->Extended(LICE_EXT_DASHEDLINE_ACCEL, &data)) return;
  }
#endif

  if (ClipLine(&x1, &y1, &x2, &y2, w, h)) 
  {
    if (y1 > y2) SWAP(y1, y2);
    if (pxon == 1 && pxoff == 1 && x1 == x2 && (mode&LICE_BLIT_MODE_MASK) == LICE_BLIT_MODE_COPY && alpha == 1.0f)
    {
      LICE_DottedVertLineFAST(dest, x1, y1, y2, color);        
    }
    else
    {
      int aw = (int)(256.0f*alpha);
      if (x1 > x2) SWAP(x1, x2);

#ifdef LICE_FAVOR_SIZE

      LICE_COMBINEFUNC blitfunc=NULL;      
      #define __LICE__ACTION(comb) blitfunc=comb::doPix;

#else

  #define __LICE__ACTION(COMBFUNC) __LICE_LineClass<COMBFUNC>::DashedLine(dest, x1, y1, x2, y2, pxon, pxoff, color, aw);
#endif

      __LICE_ACTION_CONSTANTALPHA(mode, aw, false);   

#ifdef LICE_FAVOR_SIZE
      if (blitfunc) __LICE_LineClass::DashedLine(dest, x1, y1, x2, y2, pxon, pxoff, color, aw, blitfunc);
#endif

#undef __LICE__ACTION
    }
  }
}

bool LICE_ClipLine(int* pX1, int* pY1, int* pX2, int* pY2, int xLo, int yLo, int xHi, int yHi)
{
    int x1 = *pX1-xLo;
    int y1 = *pY1-yLo;
    int x2 = *pX2-xLo;
    int y2 = *pY2-yLo;
    bool onscreen = ClipLine(&x1, &y1, &x2, &y2, xHi-xLo, yHi-yLo);
    *pX1 = x1+xLo;
    *pY1 = y1+yLo;
    *pX2 = x2+xLo;
    *pY2 = y2+yLo;
    return onscreen;
}

bool LICE_ClipFLine(float* px1, float* py1, float* px2, float* py2, float xlo, float ylo, float xhi, float yhi)
{
  float x1 = *px1-xlo;
  float y1 = *py1-ylo;
  float x2 = *px2-xlo;
  float y2 = *py2-ylo;
  bool onscreen = ClipFLine(&x1, &y1, &x2, &y2, xhi-xlo, yhi-ylo);
  *px1 = x1+xlo;
  *py1 = y1+ylo;
  *px2 = x2+xlo;
  *py2 = y2+ylo;
  return onscreen;
}


#include "lice_bezier.h"

static void DoBezierFillSegment(LICE_IBitmap* dest, int x1, int y1, int x2, int y2, int yfill, LICE_pixel color, float alpha, int mode)
{
  if (x2 < x1) return;
  if (x2 == x1)
  {
    if (y1 > y2) SWAP(y1,y2);
    int ylo = lice_min(y1,yfill);
    int yhi = lice_max(y2,yfill+1);
    LICE_FillRect(dest, x1, ylo, 1, yhi-ylo+1, color, alpha, mode);
    return;
  }

  if ((y1 < yfill) == (y2 < yfill))
  {       
    if (y1 < yfill) ++yfill;
    int x[4] = { x1, x1, x2, x2 };
    int y[4] = { y1, yfill, y2, yfill };
    LICE_FillConvexPolygon(dest, x, y, 4, color, alpha, mode);
  }
  else
  {    
    int x = x1+(int)((double)(yfill-y1)*(double)(x2-x1)/(double)(y2-y1));
    int yf = yfill;
    if (y1 < yfill) ++yf;   
    LICE_FillTriangle(dest, x1, y1, x1, yf, x, yf, color, alpha, mode);
    yf = yfill;
    if (y2 < yfill) ++yf;
    LICE_FillTriangle(dest, x, yf, x2, yf, x2, y2, color, alpha, mode);
  }
}

static void DoBezierFillSegmentX(LICE_IBitmap* dest, int x1, int y1, int x2, int y2, int xfill, LICE_pixel color, float alpha, int mode)
{
  if (y2 < y1) return;
  if (y2 == y1)
  {
    if (x1 > x2) SWAP(x1,x2);
    int xlo = lice_min(x1,xfill);
    int xhi = lice_max(x2,xfill+1);
    LICE_FillRect(dest, xlo, y1, xhi-xlo+1, 1, color, alpha, mode);
    return;
  }

  if ((x1 < xfill) == (x2 < xfill))
  {       
    if (x1 < xfill) ++xfill;
    int x[4] = { x1, xfill, x2, xfill };
    int y[4] = { y1, y1, y2+1, y2+1 };
    LICE_FillConvexPolygon(dest, x, y, 4, color, alpha, mode);
  }
  else
  {    
    int y = y1+(int)((double)(xfill-x1)*(double)(y2-y1)/(double)(x2-x1));
    int xf = xfill;
    if (x1 < xfill) ++xf;
    LICE_FillTriangle(dest, x1, y1, xf, y1, xf, y, color, alpha, mode);
    xf = xfill;
    if (x2 < xfill) ++xf;
    LICE_FillTriangle(dest, xf, y, xf, y2, x2, y2, color, alpha, mode);
  }
}


// quadratic bezier ... NOT TESTED YET
// attempt to draw segments no longer than tol px
void LICE_DrawQBezier(LICE_IBitmap* dest, double xstart, double ystart, double xctl, double yctl, double xend, double yend, 
  LICE_pixel color, float alpha, int mode, bool aa, double tol)
{
  if (!dest) return;

  int w = dest->getWidth();

  const int __sc = (int)dest->Extended(LICE_EXT_GET_SCALING,NULL);
  if (__sc)
  {
    __LICE_SCU(w);
    if (!IGNORE_SCALING(mode)) 
    {
      __LICE_SC(xstart);
      __LICE_SC(ystart);
      __LICE_SC(xctl);
      __LICE_SC(yctl);
      __LICE_SC(xend);
      __LICE_SC(yend);
    }
    mode|=LICE_BLIT_IGNORE_SCALING;
  }



    
  if (xstart > xend) 
  {
    SWAP(xstart, xend);
    SWAP(ystart, yend);
  }

  double len = sqrt((xctl-xstart)*(xctl-xstart)+(yctl-ystart)*(yctl-ystart));
  len += sqrt((xend-xctl)*(xend-xctl)+(yend-yctl)*(yend-yctl));
      
  double xlo = xstart;
  double xhi = xend;
  double ylo = ystart;
  double yhi = yend;
  double tlo = 0.0;
  double thi = 1.0;

  if (xlo < 0.0f) 
  {
    xlo = 0.0f;
    ylo = LICE_Bezier_GetY(xstart, xctl, xend, ystart, yctl, yend, xlo, &tlo);
  }
  if (xhi >= (float)w)
  {
    xhi = (float)(w-1);
    yhi = LICE_Bezier_GetY(xstart, xctl, xend, ystart, yctl, yend, xhi, &thi);
  }
  if (xlo > xhi) return;

  len *= (thi-tlo);
  if (tol <= 0.0f) tol = 1.0f;
  int nsteps = (int)(len/tol);
  if (nsteps <= 0) nsteps = 1;

  double dt = (thi-tlo)/(double)nsteps;
  double t = tlo+dt;

  double lastx = xlo;
  double lasty = ylo;
  double x, y;
  int i;
  for (i = 1; i < nsteps; ++i)
  {
    LICE_Bezier(xstart, xctl, xend, ystart, yctl, yend, t, &x, &y);
    LICE_FLine(dest, lastx, lasty, x, y, color, alpha, mode, aa);
    lastx = x;
    lasty = y;
    t += dt;
  } 
  LICE_FLine(dest, lastx, lasty, xhi, yhi, color, alpha, mode, aa);

}

int LICE_CBezPrep(int dest_w, double xstart, double ystart, double xctl1, double yctl1,
  double xctl2, double yctl2, double xend, double yend, double tol, bool xbasis,
  double* ax, double* bx, double* cx, double* dx, double* ay, double* by, double* cy, double* dy,
  double* xlo, double* xhi, double* ylo, double* yhi, double* tlo, double* thi)
{
  const int w = dest_w;
    
  if ((xbasis && xstart > xend) || (!xbasis && ystart > yend))
  {
    SWAP(xstart, xend);
    SWAP(xctl1, xctl2);
    SWAP(ystart, yend);
    SWAP(yctl1, yctl2);
  }

  double len = sqrt((xctl1-xstart)*(xctl1-xstart)+(yctl1-ystart)*(yctl1-ystart));
  len += sqrt((xctl2-xctl1)*(xctl2-xctl1)+(yctl2-yctl1)*(yctl2-yctl1));
  len += sqrt((xend-xctl2)*(xend-xctl2)+(yend-yctl2)*(yend-yctl2));

  LICE_CBezier_GetCoeffs(xstart, xctl1, xctl2, xend, ystart, yctl1, yctl2, yend, ax, bx, cx, ay, by, cy);
  *dx = xstart;
  *dy = ystart;

  *xlo = xstart;
  *xhi = xend;
  *ylo = ystart;
  *yhi = yend;
  *tlo = 0.0;
  *thi = 1.0;

  if (*xlo < 0.0f) 
  {
    *xlo = 0.0f;
    *ylo = LICE_CBezier_GetY(xstart, xctl1, xctl2, xend, ystart, yctl1, yctl2, yend, *xlo, (double*)NULL, (double*)NULL, (double*)NULL, tlo);
  }
  if (*xhi > w)
  {
    *xhi = w;
    *yhi = LICE_CBezier_GetY(xstart, xctl1, xctl2, xend, ystart, yctl1, yctl2, yend, *xhi, (double*)NULL, (double*)(double*)NULL, thi, (double*)NULL);
  }
  if ((xbasis && *xlo > *xhi) || (!xbasis && *ylo > *yhi))
  {
    return 0;
  }

  len *= (*thi-*tlo);
  if (tol <= 0.0f) tol = 1.0f;
  int nsteps = (int)(len/tol);
  if (nsteps <= 0) nsteps = 1;
  return nsteps;
}

#define __LICE_SC_BEZ \
    __LICE_SC(destbm_w); \
    if (!IGNORE_SCALING(mode)) { \
      __LICE_SC(xstart); \
      __LICE_SC(ystart); \
      __LICE_SC(xctl1); \
      __LICE_SC(yctl1); \
      __LICE_SC(xctl2); \
      __LICE_SC(yctl2); \
      __LICE_SC(xend); \
      __LICE_SC(yend); \
    }


void LICE_DrawCBezier(LICE_IBitmap* dest, double xstart, double ystart, double xctl1, double yctl1,
  double xctl2, double yctl2, double xend, double yend, LICE_pixel color, float alpha, int mode, bool aa, double tol)
{ 
  if (!dest) return;
  int destbm_w = dest->getWidth();
  const int __sc = (int)dest->Extended(LICE_EXT_GET_SCALING,NULL);
  if (__sc)
  {
    __LICE_SC_BEZ
    mode|=LICE_BLIT_IGNORE_SCALING;
  }

#ifndef DISABLE_LICE_EXTENSIONS
  if (dest->Extended(LICE_EXT_SUPPORTS_ID, (void*) LICE_EXT_DRAWCBEZIER_ACCEL))
  {
    LICE_Ext_DrawCBezier_acceldata data(xstart, ystart, xctl1, yctl1, xctl2, yctl2, xend, yend, color, alpha, mode, aa);
    if (dest->Extended(LICE_EXT_DRAWCBEZIER_ACCEL, &data)) return;
  }
#endif

  double ax, bx, cx, dx, ay, by, cy, dy;
  double xlo, xhi, ylo, yhi;
  double tlo, thi;
  int nsteps = LICE_CBezPrep(destbm_w, xstart, ystart, xctl1, yctl1, xctl2, yctl2, xend, yend, tol, true,
    &ax, &bx, &cx, &dx, &ay, &by, &cy, &dy, &xlo, &xhi, &ylo, &yhi, &tlo, &thi);
  if (!nsteps) return;
   
  double dt = (thi-tlo)/(double)nsteps;
  double t = tlo+dt;

  double lastx = xlo;
  double lasty = ylo;
  double x, y;
  int i;
  for (i = 1; i < nsteps-1; ++i)
  {
    EVAL_CBEZXY(x, y, ax, bx, cx, dx, ay, by, cy, dy, t);
    LICE_FLine(dest, lastx, lasty, x, y, color, alpha, mode, aa);
    lastx = x;
    lasty = y;
    t += dt;
  } 
  LICE_FLine(dest, lastx, lasty, xhi, yhi, color, alpha, mode, aa);
}

void LICE_DrawThickCBezier(LICE_IBitmap* dest, double xstart, double ystart, double xctl1, double yctl1,
  double xctl2, double yctl2, double xend, double yend, LICE_pixel color, float alpha, int mode, int wid, double tol)
{
  if (!dest) return;
  int destbm_w = dest->getWidth();
  const int __sc = (int)dest->Extended(LICE_EXT_GET_SCALING,NULL);
  if (__sc)
  {
    __LICE_SC_BEZ
    mode|=LICE_BLIT_IGNORE_SCALING;
  }
  
  double ax, bx, cx, dx, ay, by, cy, dy;
  double xlo, xhi, ylo, yhi;
  double tlo, thi;
  int nsteps = LICE_CBezPrep(destbm_w, xstart, ystart, xctl1, yctl1, xctl2, yctl2, xend, yend, tol, true,
                        &ax, &bx, &cx, &dx, &ay, &by, &cy, &dy, &xlo, &xhi, &ylo, &yhi, &tlo, &thi);
  if (!nsteps) return;
  
  double dt = (thi-tlo)/(double)nsteps;
  double t = tlo+dt;
  
  double lastx = xlo;
  double lasty = ylo;
  double x, y;
  bool last_xmaj=false;
  int i;
  for (i = 1; i < nsteps; ++i)
  {
    if (i == nsteps-1)
    {
      x = xhi;
      y = yhi;
    }
    else
    {
      EVAL_CBEZXY(x, y, ax, bx, cx, dx, ay, by, cy, dy, t);
    }
    LICE_ThickFLine(dest, lastx, lasty, x, y, color, alpha, mode, wid);

    bool xmaj = fabs(x-lastx) > fabs(y-lasty);
    if (i>1 && xmaj != last_xmaj)
    {
      //int color = LICE_RGBA(255,0,0,0);
      if (wid>2)
      {
        // tested this with w=3, w=4, w=8 and all looked pretty decent
        double r = wid*.5 - 1;
        if (r<0) r=0;
        LICE_FillCircle(dest,floor(lastx+0.5),floor(lasty+0.5),.5+r*.707,color,alpha,mode,true);
      }
      else
      {
        const int ix = (int)floor(lastx+0.5), iy = (int)floor(lasty);
        const double da = lasty - iy;
        LICE_PutPixel(dest,ix,iy,color,alpha * (1.0-da),mode);
        LICE_PutPixel(dest,ix,iy+1,color,alpha*da,mode);
      }
    }

    last_xmaj = xmaj;
    lastx = x;
    lasty = y;
    t += dt;
  }
}

void LICE_FillCBezier(LICE_IBitmap* dest, double xstart, double ystart, double xctl1, double yctl1,
  double xctl2, double yctl2, double xend, double yend, int yfill, LICE_pixel color, float alpha, int mode, double tol)
{
  if (!dest) return;
  int destbm_w = dest->getWidth();
  const int __sc = (int)dest->Extended(LICE_EXT_GET_SCALING,NULL);
  if (__sc)
  {
    __LICE_SC_BEZ
    if (!IGNORE_SCALING(mode)) 
    {
      __LICE_SC(yfill);
      mode|=LICE_BLIT_IGNORE_SCALING;
    }
  }


  double ax, bx, cx, dx, ay, by, cy, dy;
  double xlo, xhi, ylo, yhi;
  double tlo, thi;
  int nsteps = LICE_CBezPrep(destbm_w, xstart, ystart, xctl1, yctl1, xctl2, yctl2, xend, yend, tol, true,
    &ax, &bx, &cx, &dx, &ay, &by, &cy, &dy, &xlo, &xhi, &ylo, &yhi, &tlo, &thi);
  if (!nsteps) return;
   
  double dt = (thi-tlo)/(double)nsteps;
  double t = tlo+dt;

  int lastfillx = (int)xlo;  
  int lastfilly = (int)(ylo+0.5f);
  double x, y;
  int i;
  for (i = 1; i < nsteps-1; ++i)
  {
    EVAL_CBEZXY(x, y, ax, bx, cx, dx, ay, by, cy, dy, t);
    if ((int)x >= lastfillx)
    {
      int xi = (int)x;
      int yi = (int)(y+0.5f);
      DoBezierFillSegment(dest, lastfillx, lastfilly, xi, yi, yfill, color, alpha, mode);
      lastfillx = xi+1;
      lastfilly = yi;
    }
    t += dt;
  } 
  if ((int)(xhi-1.0f) >= lastfillx)
  {
    DoBezierFillSegment(dest, lastfillx, lastfilly, (int)(xhi-1.0f),(int)(yhi+0.5f), yfill, color, alpha, mode);
  }
}

void LICE_FillCBezierX(LICE_IBitmap* dest, double xstart, double ystart, double xctl1, double yctl1,
  double xctl2, double yctl2, double xend, double yend, int xfill, LICE_pixel color, float alpha, int mode, double tol)
{
  if (!dest) return;

  int destbm_w = dest->getWidth();
  const int __sc = (int)dest->Extended(LICE_EXT_GET_SCALING,NULL);
  if (__sc)
  {
    __LICE_SC_BEZ
    if (!IGNORE_SCALING(mode)) 
    {
      __LICE_SC(xfill);
      mode|=LICE_BLIT_IGNORE_SCALING;
    }
  }

  double ax, bx, cx, dx, ay, by, cy, dy;
  double xlo, xhi, ylo, yhi;
  double tlo, thi;
  int nsteps = LICE_CBezPrep(destbm_w, xstart, ystart, xctl1, yctl1, xctl2, yctl2, xend, yend, tol, false,
    &ax, &bx, &cx, &dx, &ay, &by, &cy, &dy, &xlo, &xhi, &ylo, &yhi, &tlo, &thi);
  if (!nsteps) return;
   
  double dt = (thi-tlo)/(double)nsteps;
  double t = tlo+dt;

  int lastfillx = (int)(xlo+0.5f);
  int lastfilly = (int)ylo;
  double x, y;
  int i;
  for (i = 1; i < nsteps-1; ++i)
  {
    EVAL_CBEZXY(x, y, ax, bx, cx, dx, ay, by, cy, dy, t);
    if ((int)y >= lastfilly)
    {
      int xi = (int)(x+0.5f);
      int yi = (int)y;
      DoBezierFillSegmentX(dest, lastfillx, lastfilly, xi, yi, xfill, color, alpha, mode);
      lastfillx = xi;
      lastfilly = yi+1;
    }
    t += dt;
  } 
  if ((int)(yhi-1.0f) >= lastfilly)
  {
    DoBezierFillSegmentX(dest, lastfillx, lastfilly, (int)(xhi+0.5),(int)(yhi-1.0f), xfill, color, alpha, mode);
  }
}


void LICE_DrawRect(LICE_IBitmap *dest, int x, int y, int w, int h, LICE_pixel color, float alpha, int mode)
{
  const int __sc = IGNORE_SCALING(mode) ? 0 : (int)dest->Extended(LICE_EXT_GET_SCALING,NULL);
  if (__sc>0)
  {
    double x1 = x, y1 = y, x2 = x+w, y2 = y+h;
    const double amt = 1.0 - 256.0/__sc;
    x1 += amt;
    y1 += amt;
    x2 -= amt;
    y2 -= amt;
    LICE_FLine(dest, x1, y1, x2, y1, color, alpha, mode, true);
    LICE_FLine(dest, x2, y1, x2, y2, color, alpha, mode, true);
    LICE_FLine(dest, x2, y2, x1, y2, color, alpha, mode, true);
    LICE_FLine(dest, x1, y2, x1, y1, color, alpha, mode, true);
  }
  else
  {
    LICE_Line(dest, x, y, x+w, y, color, alpha, mode, false);
    LICE_Line(dest, x+w, y, x+w, y+h, color, alpha, mode, false);
    LICE_Line(dest, x+w, y+h, x, y+h, color, alpha, mode, false);
    LICE_Line(dest, x, y+h, x, y, color, alpha, mode, false);
  }
}

void LICE_BorderedRect(LICE_IBitmap *dest, int x, int y, int w, int h, LICE_pixel bgcolor, LICE_pixel fgcolor, float alpha, int mode)
{
  LICE_FillRect(dest, x+1, y+1, w-1, h-1, bgcolor, alpha, mode);
  LICE_DrawRect(dest, x, y, w, h, fgcolor, alpha, mode);
}


#ifndef LICE_FAVOR_SIZE_EXTREME
template<class COMBFUNC> 
#endif
class _LICE_Fill
{

#ifdef LICE_FAVOR_SIZE_EXTREME
  #define DOPIX(pout,r,g,b,a,ia) combFunc(pout,r,g,b,a,ia);
#else
    #define DOPIX(pout,r,g,b,a,ia) COMBFUNC::doPix(pout,r,g,b,a,ia);
#endif

public:

  // da, db are [0..65536]
  static void FillClippedTrapezoid(int wid, int span, LICE_pixel *px, int y, int xa, int xb, int da, int db, int a, int b, int astep, int bstep, int cr, int cg, int cb, int ca, int aw
#ifdef LICE_FAVOR_SIZE_EXTREME
                          , LICE_COMBINEFUNC combFunc
#endif
    )
  {
    if (!da && !db)
    {
      while (y-->0)
      {
        LICE_pixel* xpx = px;
        int x=xb;
        while (x--)
        {
          DOPIX((LICE_pixel_chan*)xpx, cr, cg, cb, ca, aw)
          ++xpx;
        }
        px += span;
      }
      return;
    }


    while (y-->0)
    {
      int x1=lice_max(xa,0);
      int x2=lice_min(xb,wid);
      LICE_pixel* xpx = px + x1;
      int cnt=x2-x1;
      while (cnt-->0)
      {
        DOPIX((LICE_pixel_chan*)xpx, cr, cg, cb, ca, aw)
        ++xpx;
      }

      a += da;
      b += db;
      if (a >= 65536)
      {
        int na = a>>16;
        a &= 65535;
        if (astep<0)na=-na;
        xa += na;
      }
      if (b >= 65536)
      {
        int nb = b>>16;
        b &= 65535;
        if (bstep<0)nb=-nb;
        xb += nb;
      }
      px += span;
    }
  }
};


template <class COMBFUNC> class _LICE_FillFast
{
public:

  // da, db are [0..65536]
  static void FillClippedTrapezoidFAST(int wid, int span, LICE_pixel *px, int y, int xa, int xb, int da, int db, int a, int b, int astep, int bstep, LICE_pixel color)
  {
    if (!da && !db)
    {
      while (y-->0)
      {
        LICE_pixel* xpx = px;
        int x=xb;
        while (x--)
        {
          COMBFUNC::doPixFAST(xpx, color);
          ++xpx;
        }
        px += span;
      }
      return;
    }

 
    while (y-->0)
    {
      int x1=lice_max(xa,0);
      int x2=lice_min(xb,wid);
      LICE_pixel* xpx = px + x1;
      int cnt=x2-x1;
      while (cnt-->0)
      {
        COMBFUNC::doPixFAST(xpx, color);
        ++xpx;
      }

      a += da;
      b += db;
      if (a >= 65536)
      {
        int na = a>>16;
        a &= 65535;
        if (astep<0)na=-na;
        xa += na;
      }
      if (b >= 65536)
      {
        int nb = b>>16;
        b &= 65535;
        if (bstep<0)nb=-nb;
        xb += nb;
      }
      px += span;
    }
  }
};

static double FindXOnSegment(int x1, int y1, int x2, int y2, int ty)
{
  if (y1 > y2)
  {
    SWAP(x1, x2);
    SWAP(y1, y2);
  }
  if (ty <= y1) return x1;
  if (ty >= y2) return x2;
  const double dxdy = (x2-x1)/(double)(y2-y1);
  return x1+(ty-y1)*dxdy;
}

void LICE_FillTrapezoidF(LICE_IBitmap* dest, double fx1a, double fx1b, int y1, double fx2a, double fx2b, int y2, LICE_pixel color, float alpha, int mode)
{
  if (!dest) return; 
  if (y1 > y2)
  {
    SWAP(y1, y2);
    SWAP(fx1a, fx2a);
    SWAP(fx1b, fx2b);
  }
  if (fx1a > fx1b) SWAP(fx1a, fx1b);
  if (fx2a > fx2b) SWAP(fx2a, fx2b); 
  
  int w = dest->getWidth();
  int h = dest->getHeight();

  const int __sc = (int)dest->Extended(LICE_EXT_GET_SCALING,NULL);
  if (__sc>0)
  {
    __LICE_SCU(w);
    __LICE_SCU(h);
    if (!IGNORE_SCALING(mode))
    {
      __LICE_SC(fx1a);
      __LICE_SC(fx1b);
      __LICE_SC(fx2a);
      __LICE_SC(fx2b);
      __LICE_SC(y1);
      __LICE_SC(y2);
    }
  }

  if (fx1b < 0 && fx2b < 0) return;
  if (fx1a >= w && fx2a >= w) return;

  if (fx1a <= 0 && fx2a <= 0) fx1a = fx2a = 0;
  if (fx1b >= w-1 && fx2b >= w-1) fx1b = fx2b = w-1;

  if (y2 < 0 || y1 >= h) return;

  int aw = (int)(alpha*256.0f);

  double idy = y2==y1 ? 0.0 : (65536.0/(y2-y1));
  
  const double maxv=(double)(1<<29);
  double tmp = (fx2a-fx1a)*idy;
  if (tmp > maxv) tmp=maxv;
  else if (tmp < -maxv) tmp=-maxv;
  int dxady = (int)floor(tmp+0.5);

  tmp = ((fx2b-fx1b)*idy);
  if (tmp > maxv) tmp=maxv;
  else if (tmp < -maxv) tmp=-maxv;
  int dxbdy = (int)floor(tmp+0.5);

  int astep = 1;
  int bstep = 1;
  if (dxady < 0)
  {
    dxady = -dxady;
    astep = -1;
  }
  if (dxbdy < 0)
  {
    dxbdy = -dxbdy;
    bstep = -1;
  }
  
  int x1a = (int)floor(fx1a);
  int x1b = (int)floor(fx1b);
  int a = (int) floor((fx1a-x1a)*65536.0*astep+0.5);
  int b = (int) floor((fx1b-x1b)*65536.0*bstep+0.5);

  if (y1<0)
  {
    a -= dxady*y1;
    b -= dxbdy*y1;
    y1=0;
  }
  if (a< 0 || a >= 65536)
  {
    int na = a>>16;
    a &= 65535;
    if (astep<0)na=-na;
    x1a += na;
  }
  if (b < 0 || b >= 65536)
  {
    int nb = b>>16;
    b &= 65535;
    if (bstep<0)nb=-nb;
    x1b += nb;
  }
  const int extra = __sc> 0 && !IGNORE_SCALING(mode) ? (__sc/256 - 1) : 0;
  if (y2 > h-1-extra) y2 = h-1-extra;

  int wid = w;
  int span = dest->getRowSpan();
  LICE_pixel* px = dest->getBits()+y1*span;
  int y = y2-y1 + 1 + extra;

  x1b++; // from now on draw [x1a,x1b)

  if (!dxady && !dxbdy)
  {
    if (x1a<0)x1a=0;
    x1b = lice_min(x1b,wid)-x1a;    
    px+=x1a;
    if (x1b<1) return;
  }

  if ((mode&LICE_BLIT_MODE_MASK) == LICE_BLIT_MODE_COPY && aw==256)
  {
    _LICE_FillFast<_LICE_CombinePixelsClobberFAST>::FillClippedTrapezoidFAST(wid,span,px,y, x1a, x1b, dxady, dxbdy, a,b, astep,bstep, color);
  }
  else if ((mode&LICE_BLIT_MODE_MASK) == LICE_BLIT_MODE_COPY && aw==128)
  {
    color = (color>>1)&0x7f7f7f7f;
    _LICE_FillFast<_LICE_CombinePixelsHalfMix2FAST>::FillClippedTrapezoidFAST(wid,span,px,y,  x1a, x1b, dxady, dxbdy, a,b, astep,bstep, color);
  }
  else if ((mode&LICE_BLIT_MODE_MASK) == LICE_BLIT_MODE_COPY && aw==64)
  {
    color = (color>>2)&0x3f3f3f3f;
    _LICE_FillFast<_LICE_CombinePixelsQuarterMix2FAST>::FillClippedTrapezoidFAST(wid,span,px,y,  x1a, x1b, dxady, dxbdy, a,b, astep,bstep, color);
  }
  else if ((mode&LICE_BLIT_MODE_MASK) == LICE_BLIT_MODE_COPY && aw==192)
  {
    color = ((color>>1)&0x7f7f7f7f)+((color>>2)&0x3f3f3f3f);
    _LICE_FillFast<_LICE_CombinePixelsThreeQuarterMix2FAST>::FillClippedTrapezoidFAST(wid,span,px,y,  x1a, x1b, dxady, dxbdy,a,b, astep,bstep, color);
  }
  else
  {
    int cr = LICE_GETR(color), cg = LICE_GETG(color), cb = LICE_GETB(color), ca = LICE_GETA(color);
#ifdef LICE_FAVOR_SIZE_EXTREME

    LICE_COMBINEFUNC blitfunc=NULL;      
#define __LICE__ACTION(comb) blitfunc=comb::doPix;
#else
#define __LICE__ACTION(COMBFUNC) _LICE_Fill<COMBFUNC>::FillClippedTrapezoid(wid,span,px,y,  x1a, x1b, dxady, dxbdy, a,b, astep,bstep, cr,cg,cb,ca, aw);
#endif

    __LICE_ACTION_CONSTANTALPHA(mode, aw, false);

#ifdef LICE_FAVOR_SIZE_EXTREME
      if (blitfunc) _LICE_Fill::FillClippedTrapezoid(wid,span,px,y,  x1a, x1b, dxady, dxbdy, a,b, astep,bstep, cr,cg,cb,ca, aw, blitfunc);
#endif

#undef __LICE__ACTION
  }
}

void LICE_FillTrapezoid(LICE_IBitmap* dest, int x1a, int x1b, int y1, int x2a, int x2b, int y2, LICE_pixel color, float alpha, int mode)
{
  LICE_FillTrapezoidF(dest,x1a,x1b,y1,x2a,x2b,y2,color,alpha,mode);
}

static int _ysort(const void* a, const void* b)
{
  int* xya = (int*)a;
  int* xyb = (int*)b;
  if (xya[1] < xyb[1]) return -1;
  if (xya[1] > xyb[1]) return 1;
  if (xya[0] < xyb[0]) return -1;
  if (xya[0] > xyb[0]) return 1;
  return 0;
}

#define _X(i) xy[2*(i)]
#define _Y(i) xy[2*(i)+1]

static int FindNextEdgeVertex(int* xy, int a, int n, int dir)
{
  bool init = false;
  double dxdy_best = 0.0f;
  int i, ilo = a;

  for (i = a+1; i < n; ++i)
  {
    if (_Y(i) == _Y(a)) continue;
    const double dxdy = (_X(i)-_X(a))/(double)(_Y(i)-_Y(a));
    if (!init || dxdy == dxdy_best || (dir < 0 && dxdy < dxdy_best) || (dir > 0 && dxdy > dxdy_best))
    {
      init = true;
      ilo = i;
      dxdy_best = dxdy;
    }
  }
  return ilo;
}

void LICE_FillConvexPolygon(LICE_IBitmap* dest, const int* x, const int* y, int npoints, LICE_pixel color, float alpha, int mode)
{
  if (!dest) return;
  if (npoints < 3) return;

  int destbm_w = dest->getWidth(), destbm_h = dest->getHeight();

  if (IGNORE_SCALING(mode))
  {
    const int __sc = (int)dest->Extended(LICE_EXT_GET_SCALING,NULL);
    if (__sc)
    {
      __LICE_SCU(destbm_w);
      __LICE_SCU(destbm_h);
    }
  }

  int* xy = 0;
  int xyt[1024]; // use stack space if small
  bool usestack = npoints <= (int) (sizeof(xyt)/sizeof(int)/2);
  if (usestack) xy = xyt;
  else xy = (int*)malloc(npoints*sizeof(int)*2);

  int i;
  {
    int min_x=destbm_w,max_x=0;
    for (i = 0; i < npoints; ++i)
    {
      int tx = x[i], ty=y[i];
      if (tx < min_x) min_x=tx;
      if (tx > max_x) max_x=tx;
      _X(i) = tx;
      if (dest->isFlipped()) ty = destbm_h-ty-1;
      _Y(i) = ty;
    }
    qsort(xy, npoints, 2*sizeof(int), _ysort);  // sorts by y, at same y sorts by x


    int ty=_Y(0);
    if (ty == _Y(npoints-1))
    {
      // special case 1px high polygon
      if (ty >= 0 && ty < dest->getHeight() && min_x <= max_x)
      {
        LICE_FillTrapezoid(dest,min_x,max_x,ty,min_x,max_x,ty,color,alpha,mode);
      }
      if (!usestack) free(xy);

      return;
    }
  }

  int a1, b1;   // index of previous vertex L and R
  int a2, b2;   // index of next vertex L and R
  int y1;   // top and bottom of current trapezoid

  a1 = b1 = 0;
  y1 = _Y(0);

  for (i = 1; i < npoints && _Y(i) == y1; ++i)
  {
    if (_X(i) == _X(0)) a1 = i;
    b1 = i;
  }

  a2 = FindNextEdgeVertex(xy, a1, npoints, -1);
  b2 = FindNextEdgeVertex(xy, b1, npoints, 1);

  while (a1 != a2 || b1 != b2)
  {
    int y_a2 = _Y(a2);
    int y_b2 = _Y(b2);

    int y2 = lice_min(y_a2, y_b2);   
    double x1a = FindXOnSegment(_X(a1), _Y(a1), _X(a2), y_a2, y1);
    double x1b = FindXOnSegment(_X(b1), _Y(b1), _X(b2), y_b2, y1);
    double x2a = FindXOnSegment(_X(a1), _Y(a1), _X(a2), y_a2, y2);
    double x2b = FindXOnSegment(_X(b1), _Y(b1), _X(b2), y_b2, y2);
  
    LICE_FillTrapezoidF(dest, x1a, x1b, y1, x2a, x2b, y2, color, alpha, mode);

    bool dir = y1<=y2; // should always be true

    y1 = y2;
    if (y_a2 == y1) 
    {
      a1 = a2;
      a2 = FindNextEdgeVertex(xy, a2, npoints, -1);
    }
    if (y_b2 == y1) 
    {
      b1 = b2;
      b2 = FindNextEdgeVertex(xy, b2, npoints, 1);
    }

    if (dir) y1++; 
    else y1--;
  }

  if (!usestack) free(xy);
}

#undef _X
#undef _Y


void LICE_FillTriangle(LICE_IBitmap *dest, int x1, int y1, int x2, int y2, int x3, int y3, LICE_pixel color, float alpha, int mode)
{
  if (!dest) return;

  int x[3] = { x1, x2, x3 };
  int y[3] = { y1, y2, y3 };
  LICE_FillConvexPolygon(dest, x, y, 3, color, alpha, mode);
}

void LICE_ThickFLine(LICE_IBitmap* dest, double x1, double y1, double x2, double y2, LICE_pixel color, float alpha, int mode, int wid) // always AA. wid is not affected by scaling (1 is always normal line, 2 is always 2 physical pixels, etc)
{
  if (!dest || wid<1) return;
  if (wid==1)
  {
    LICE_Line(dest,(float)x1,(float)y1,(float)x2,float(y2),color,alpha,mode,true);
    return;
  }

  int w = dest->getWidth();
  int h = dest->getHeight();
  if (dest->isFlipped())
  {
    y1 = (h-1)-y1;
    y2 = (h-1)-y2;
  }

  const int __sc = (int)dest->Extended(LICE_EXT_GET_SCALING,NULL);
  if (__sc>0)
  {
    __LICE_SCU(w);
    __LICE_SCU(h);
    if (!IGNORE_SCALING(mode))
    {
      __LICE_SC(x1);
      __LICE_SC(x2);
      __LICE_SC(y1);
      __LICE_SC(y2);
    }
  }

  if (ClipFLine(&x1, &y1, &x2, &y2, w, h))
  {
    if (x1 != x2 || y1 != y2)
    {
      int span = dest->getRowSpan();
      int aw = (int)(256.0f*alpha);

      double a1, a2, b1, b2, da, db;
      int astep, bstep;
      double dx = x2-x1;
      double dy = y2-y1;

      int b_max;
      if (fabs(dx) > fabs(dy))
      {
        a1 = x1;
        a2 = x2;
        b1 = y1;
        b2 = y2;
        da = dx;
        db = dy;
        astep = 1;
        bstep = span;
        b_max = h;
      }
      else
      {
        a1 = y1;
        a2 = y2;
        b1 = x1;
        b2 = x2;
        da = dy;
        db = dx;
        astep = span;
        bstep = 1;
        b_max = w;
      }

      if (da < 0.0)
      {
        da = -da;
        db = -db;
        SWAP(a1, a2);
        SWAP(b1, b2);
      }
      if (db < 0.0)
      {
        bstep = -bstep;
      }

      int n = (int)(floor(a2)-ceil(a1));
      double dbda = db/da;

      double ta = ceil(a1);
      double tb = b1+(ta-a1)*dbda;
      double bf = tb-floor(tb);
      int err = (int)(bf*65536.0);
      if (bstep < 0) err = 65535-err;
      int derr = (int)(fabs(dbda)*65536.0);

      int b_pos = (int) tb;
      LICE_pixel *px = dest->getBits() + (int)ta*astep+b_pos*abs(bstep);

      if (bstep < 0) { px -= bstep; b_pos++; }

#ifdef LICE_FAVOR_SIZE

      LICE_COMBINEFUNC blitfunc=NULL;
      #define __LICE__ACTION(comb) blitfunc=comb::doPix;

#else
      #define __LICE__ACTION(COMBFUNC) \
        __LICE_LineClass<COMBFUNC>::LICE_FLineImplFill(px,n,err,derr,astep,bstep, color, aw, wid, b_pos, b_max)
#endif

      __LICE_ACTION_NOSRCALPHA(mode, aw, false);

#ifdef LICE_FAVOR_SIZE
      if (blitfunc)
      {
        __LICE_LineClass::LICE_FLineImplFill(px,n,err,derr,astep,bstep, color, aw, wid, b_pos, b_max, blitfunc);
      }
#endif

  #undef __LICE__ACTION
    }
  }
}
