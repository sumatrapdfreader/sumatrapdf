#include "lice.h"
#include <math.h>

#define LICE_COMBINE_IMPLEMENT_HSV
#include "lice_combine.h"


LICE_pixel LICE_AlterColorHSV_int(LICE_pixel color, int dH, int dS, int dV)  // H is rolled over [0,384), S and V are clamped [0,255)
{
  int h, s, v;
  LICE_RGB2HSV(LICE_GETR(color), LICE_GETG(color), LICE_GETB(color), &h, &s, &v);
  
  h += dH;
  s += dS;
  v += dV;

  if (h < 0) h += 384;
  else if (h >= 384) h -= 384;
  
  if (s & ~255) 
  {
    if (s<0) s = 0;
    else s = 255;
  }
  
  if (v&~255)
  {
    if (v < 0) v = 0.;
    else v = 255;
  }

  return LICE_HSV2Pix(h, s, v, LICE_GETA(color));
}

LICE_pixel LICE_AlterColorHSV(LICE_pixel color, float dH, float dS, float dV)  // H is rolled over, S and V are clamped, all [0,1)
{
  int dHi = (int)(dH*384.0f);
  int dSi = (int)(dS*255.0f);
  int dVi = (int)(dV*255.0f);
  return LICE_AlterColorHSV_int(color, dHi, dSi, dVi);
}

void LICE_AlterBitmapHSV(LICE_IBitmap* src, float dH, float dS, float dV) // H is rolled over, S and V are clamped
{
  if (src) LICE_AlterRectHSV(src,0,0,src->getWidth(),src->getHeight(),dH,dS,dV);
}

void LICE_AlterRectHSV(LICE_IBitmap* src, int xpos, int ypos, int w, int h, float dH, float dS, float dV, int mode) // H is rolled over, S and V are clamped
{
  if (!src) return;

  int destbm_w = src->getWidth(), destbm_h = src->getHeight();
  const int __sc = (int)src->Extended(LICE_EXT_GET_SCALING,NULL);
  if (__sc>0)
  {
    __LICE_SCU(destbm_w);
    __LICE_SCU(destbm_h);
    if (!(mode & LICE_BLIT_IGNORE_SCALING))
    {
      __LICE_SC(w);
      __LICE_SC(h);
      __LICE_SC(xpos);
      __LICE_SC(ypos);
    }
  }

  if (xpos < 0) {
    w += xpos;
    xpos = 0;
  }
  if (ypos < 0) {
    h += ypos;
    ypos = 0;
  }

  const int span = src->getRowSpan();
  if (span < 1 || w < 1 || h < 1 || xpos >= destbm_w || ypos >= destbm_h) return;

  if (w > destbm_w - xpos) w = destbm_w - xpos;
  if (h > destbm_h - ypos) h = destbm_h - ypos;
  
  LICE_pixel* px = src->getBits()+ypos*span+xpos;

  int dHi = (int)(dH*384.0f);
  int dSi = (int)(dS*255.0f);
  int dVi = (int)(dV*255.0f);
  if (dHi > 383) dHi=383;
  else if (dHi < -383) dHi=-383;


  if (!dHi && !dSi && !dVi) return; // no mod

  if (w*h > 8192)
  {
    // generate a table of HSV translations with clip/clamp
    unsigned char stab[256], vtab[256];
    short htab[384];
    int x;
    for(x=0;x<256;x++)
    {
      int a=x+dSi;
      if(a<0)a=0; else if (a>255)a=255;
      stab[x]=a;

      a=x+dVi;
      if(a<0)a=0; else if (a>255)a=255;
      vtab[x]=a;

      a=x+dHi;
      if(a<0)a+=384; else if (a>=384)a-=384;
      htab[x]=a;
    }
    for(;x<384;x++)
    {
      int a=x+dHi;
      if(a<0)a+=384; else if (a>=384)a-=384;
      htab[x]=a;
    }

    while (h-->0)
    {
      LICE_pixel* tpx = px;
      px+=span;
      int xi=w;
      while (xi-->0)
      {
        LICE_pixel color = *tpx;
        int hh,s,v;
        LICE_RGB2HSV(LICE_GETR(color), LICE_GETG(color), LICE_GETB(color), &hh, &s, &v);
        *tpx++ = LICE_HSV2Pix(htab[hh],stab[s],vtab[v],LICE_GETA(color));
      }
    }
  }
  else
  {
    while (h-->0)
    {
      LICE_pixel* tpx = px;
      px+=span;
      int xi=w;
      while (xi-->0)
      {
        *tpx = LICE_AlterColorHSV_int(*tpx, dHi, dSi, dVi);
        tpx++;
      }
    }
  }
}
