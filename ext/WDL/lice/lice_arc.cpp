#include "lice.h"
#include "lice_combine.h"
#include <math.h>

#define _PI 3.141592653589793238f

#define IGNORE_SCALING(mode) ((mode)&LICE_BLIT_IGNORE_SCALING)
template <class T> inline void _SWAP(T& a, T& b) { T tmp = a; a = b; b = tmp; }

#define A(x) ((LICE_pixel_chan)((x)*255.0+0.5))
#define AF(x) (255)

#define DEF_ALPHAS(dim) \
      static const LICE_pixel_chan alphas_unfill[] = { __ALPHAS__(A) }; \
      static const LICE_pixel_chan alphas_fill[] = { __ALPHAS__(AF) }; \
      const LICE_pixel_chan * const alphas = fill ? alphas_fill : alphas_unfill; \
      ((void)sizeof(char[1 - 2*(sizeof(alphas_unfill) != dim*dim)])); \
      ((void)sizeof(char[1 - 2*(sizeof(alphas_fill) != dim*dim)]));

static bool CachedCircle(LICE_IBitmap* dest, float cx, float cy, float r, LICE_pixel color, float alpha, int mode, bool aa, bool fill)
{
  // fast draw for some small circles 
  if (r == 1.5f)
  {
    if (aa) 
    {
#define __ALPHAS__(B) \
        A(0.31), A(1.00), A(1.00), A(0.31), \
        A(1.00), B(0.06), B(0.06), A(1.00), \
        A(1.00), B(0.06), B(0.06), A(1.00), \
        A(0.31), A(1.00), A(1.00), A(0.31),

      DEF_ALPHAS(4)
#undef __ALPHAS__
      LICE_DrawGlyph(dest, cx-r, cy-r, color, alphas, 4, 4, alpha, mode);
    }
    else 
    {
#define __ALPHAS__(B) \
        A(0.00), A(1.00), A(1.00), A(0.00), \
        A(1.00), B(0.00), B(0.00), A(1.00), \
        A(1.00), B(0.00), B(0.00), A(1.00), \
        A(0.00), A(1.00), A(1.00), A(0.00),

      DEF_ALPHAS(4)
#undef __ALPHAS__
      LICE_DrawGlyph(dest, cx-r, cy-r, color, alphas, 4, 4, alpha, mode);    
    }
    return true;
  }
  else if (r == 2.0f)
  {
    if (aa) 
    {
#define __ALPHAS__(B) \
        A(0.06), A(0.75), A(1.00), A(0.75), A(0.06), \
        A(0.75), A(0.82), B(0.31), A(0.82), A(0.75), \
        A(1.00), B(0.31), B(0.00), B(0.31), A(1.00), \
        A(0.75), A(0.82), B(0.31), A(0.82), A(0.75), \
        A(0.06), A(0.75), A(1.00), A(0.75), A(0.06)

      DEF_ALPHAS(5)
#undef __ALPHAS__
      LICE_DrawGlyph(dest, cx-r, cy-r, color, alphas, 5, 5, alpha, mode);
    }
    else 
    {
#define __ALPHAS__(B) \
        A(0.00), A(0.00), A(1.00), A(0.00), A(0.00), \
        A(0.00), A(1.00), B(0.00), A(1.00), A(0.00), \
        A(1.00), B(0.00), B(0.00), B(0.00), A(1.00), \
        A(0.00), A(1.00), B(0.00), A(1.00), A(0.00), \
        A(0.00), A(0.00), A(1.00), A(0.00), A(0.00)

      DEF_ALPHAS(5)
#undef __ALPHAS__
      LICE_DrawGlyph(dest, cx-r, cy-r, color, alphas, 5, 5, alpha, mode);    
    }
    return true;
  }
  else if (r == 2.5f) {
    if (aa) {
#define __ALPHAS__(B) \
        A(0.06), A(0.75), A(1.00), A(1.00), A(0.75), A(0.06), \
        A(0.75), A(0.82), B(0.31), B(0.31), A(0.82), A(0.75), \
        A(1.00), B(0.31), B(0.00), B(0.00), B(0.31), A(1.00), \
        A(1.00), B(0.31), B(0.00), B(0.00), B(0.31), A(1.00), \
        A(0.75), A(0.82), B(0.31), B(0.31), A(0.82), A(0.75), \
        A(0.06), A(0.75), A(1.00), A(1.00), A(0.75), A(0.06)

      DEF_ALPHAS(6)
#undef __ALPHAS__
      LICE_DrawGlyph(dest, cx-r, cy-r, color, alphas, 6, 6, alpha, mode);
    }
    else {
#define __ALPHAS__(B) \
        A(0.00), A(0.00), A(1.00), A(1.00), A(0.00), A(0.00), \
        A(0.00), A(1.00), B(0.00), B(0.00), A(1.00), A(0.00), \
        A(1.00), B(0.00), B(0.00), B(0.00), B(0.00), A(1.00), \
        A(1.00), B(0.00), B(0.00), B(0.00), B(0.00), A(1.00), \
        A(0.00), A(1.00), B(0.00), B(0.00), A(1.00), A(0.00), \
        A(0.00), A(0.00), A(1.00), A(1.00), A(0.00), A(0.00)

      DEF_ALPHAS(6)
#undef __ALPHAS__
      LICE_DrawGlyph(dest, cx-r, cy-r, color, alphas, 6, 6, alpha, mode);    
    }
    return true;
  }
  else if (r == 3.0f) {
    if (aa) {
#define __ALPHAS__(B) \
        A(0.00), A(0.56), A(1.00), A(1.00), A(1.00), A(0.56), A(0.00), \
        A(0.56), A(1.00), B(0.38), B(0.25), B(0.38), A(1.00), A(0.56), \
        A(1.00), B(0.44), B(0.00), B(0.00), B(0.00), B(0.44), A(1.00), \
        A(1.00), B(0.19), B(0.00), B(0.00), B(0.00), B(0.19), A(1.00), \
        A(1.00), B(0.44), B(0.00), B(0.00), B(0.00), B(0.44), A(1.00), \
        A(0.56), A(1.00), B(0.38), B(0.25), B(0.38), A(1.00), A(0.56), \
        A(0.00), A(0.56), A(1.00), A(1.00), A(1.00), A(0.56), A(0.00)

      DEF_ALPHAS(7)
#undef __ALPHAS__
      LICE_DrawGlyph(dest, cx-r, cy-r, color, alphas, 7, 7, alpha, mode);
    }
    else {
#define __ALPHAS__(B) \
        A(0.00), A(0.00), A(1.00), A(1.00), A(1.00), A(0.00), A(0.00), \
        A(0.00), A(1.00), B(0.00), B(0.00), B(0.00), A(1.00), A(0.00), \
        A(1.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), A(1.00), \
        A(1.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), A(1.00), \
        A(1.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), A(1.00), \
        A(0.00), A(1.00), B(0.00), B(0.00), B(0.00), A(1.00), A(0.00), \
        A(0.00), A(0.00), A(1.00), A(1.00), A(1.00), A(0.00), A(0.00)

      DEF_ALPHAS(7)
#undef __ALPHAS__
      LICE_DrawGlyph(dest, cx-r, cy-r, color, alphas, 7, 7, alpha, mode);    
    }
    return true;
  }
  else if (r == 3.5f) {
    if (aa) {
#define __ALPHAS__(B) \
        A(0.00), A(0.31), A(0.87), A(1.00), A(1.00), A(0.87), A(0.31), A(0.00), \
        A(0.31), A(1.00), A(0.69), B(0.25), B(0.25), A(0.69), A(1.00), A(0.31), \
        A(0.87), A(0.69), B(0.00), B(0.00), B(0.00), B(0.00), A(0.69), A(0.87), \
        A(1.00), B(0.25), B(0.00), B(0.00), B(0.00), B(0.00), B(0.25), A(1.00), \
        A(1.00), B(0.25), B(0.00), B(0.00), B(0.00), B(0.00), B(0.25), A(1.00), \
        A(0.87), A(0.69), B(0.00), B(0.00), B(0.00), B(0.00), A(0.69), A(0.87), \
        A(0.31), A(1.00), A(0.69), B(0.25), B(0.25), A(0.69), A(1.00), A(0.31), \
        A(0.00), A(0.31), A(0.87), A(1.00), A(1.00), A(0.87), A(0.31), A(0.00)

      DEF_ALPHAS(8)
#undef __ALPHAS__
      LICE_DrawGlyph(dest, cx-r, cy-r, color, alphas, 8, 8, alpha, mode);
    }
    else {
#define __ALPHAS__(B) \
        A(0.00), A(0.00), A(1.00), A(1.00), A(1.00), A(1.00), A(0.00), A(0.00), \
        A(0.00), A(1.00), A(1.00), B(0.00), B(0.00), A(1.00), A(1.00), A(0.00), \
        A(1.00), A(1.00), B(0.00), B(0.00), B(0.00), B(0.00), A(1.00), A(1.00), \
        A(1.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), A(1.00), \
        A(1.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), A(1.00), \
        A(1.00), A(1.00), B(0.00), B(0.00), B(0.00), B(0.00), A(1.00), A(1.00), \
        A(0.00), A(1.00), A(1.00), B(0.00), B(0.00), A(1.00), A(1.00), A(0.00), \
        A(0.00), A(0.00), A(1.00), A(1.00), A(1.00), A(1.00), A(0.00), A(0.00)

      DEF_ALPHAS(8)
#undef __ALPHAS__
      LICE_DrawGlyph(dest, cx-r, cy-r, color, alphas, 8, 8, alpha, mode);
    }
    return true;
  }
  else if (r == 4.0f) {
    if (aa) {
#define __ALPHAS__(B) \
        A(0.00), A(0.12), A(0.69), A(1.00), A(1.00), A(1.00), A(0.69), A(0.12), A(0.00), \
        A(0.12), A(0.94), A(0.82), B(0.31), B(0.25), B(0.31), A(0.82), A(0.94), A(0.12), \
        A(0.69), A(0.82), B(0.06), B(0.00), B(0.00), B(0.00), B(0.06), A(0.82), A(0.69), \
        A(1.00), B(0.31), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.31), A(1.00), \
        A(1.00), B(0.19), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.19), A(1.00), \
        A(1.00), B(0.31), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.31), A(1.00), \
        A(0.69), A(0.82), B(0.06), B(0.00), B(0.00), B(0.00), B(0.06), A(0.82), A(0.69), \
        A(0.12), A(0.94), A(0.82), B(0.31), B(0.25), B(0.31), A(0.82), A(0.94), A(0.12), \
        A(0.00), A(0.12), A(0.69), A(1.00), A(1.00), A(1.00), A(0.69), A(0.12), A(0.00)

      DEF_ALPHAS(9)
#undef __ALPHAS__
      LICE_DrawGlyph(dest, cx-r, cy-r, color, alphas, 9, 9, alpha, mode);
    }
    else {
#define __ALPHAS__(B) \
        A(0.00), A(0.00), A(1.00), A(1.00), A(1.00), A(1.00), A(1.00), A(0.00), A(0.00), \
        A(0.00), A(1.00), A(1.00), B(0.00), B(0.00), B(0.00), A(1.00), A(1.00), A(0.00), \
        A(1.00), A(1.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), A(1.00), A(1.00), \
        A(1.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), A(1.00), \
        A(1.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), A(1.00), \
        A(1.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), A(1.00), \
        A(1.00), A(1.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), A(1.00), A(1.00), \
        A(0.00), A(1.00), A(1.00), B(0.00), B(0.00), B(0.00), A(1.00), A(1.00), A(0.00), \
        A(0.00), A(0.00), A(1.00), A(1.00), A(1.00), A(1.00), A(1.00), A(0.00), A(0.00)

      DEF_ALPHAS(9)
#undef __ALPHAS__
      LICE_DrawGlyph(dest, cx-r, cy-r, color, alphas, 9, 9, alpha, mode);
    }
    return true;
  }
  else if (r == 5.0f)
  {
    if (aa) {
#define __ALPHAS__(B) \
        A(0.00), A(0.00), A(0.00), A(0.58), A(0.90), A(1.00), A(0.90), A(0.58), A(0.00), A(0.00), A(0.00), \
        A(0.00), A(0.00), A(1.00), B(0.42), B(0.10), B(0.00), B(0.10), B(0.42), A(1.00), A(0.00), A(0.00), \
        A(0.00), A(1.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), A(1.00), A(0.00), \
        A(0.58), B(0.42), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.42), A(0.58), \
        A(0.90), B(0.10), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.10), A(0.90), \
        A(1.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), A(1.00), \
        A(0.90), B(0.10), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.10), A(0.90), \
        A(0.58), B(0.42), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.42), A(0.58), \
        A(0.00), A(1.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), A(1.00), A(0.00), \
        A(0.00), A(0.00), A(1.00), B(0.42), B(0.10), B(0.00), B(0.10), B(0.42), A(1.00), A(0.00), A(0.00), \
        A(0.00), A(0.00), A(0.00), A(0.58), A(0.90), A(1.00), A(0.90), A(0.58), A(0.00), A(0.00), A(0.00)

      DEF_ALPHAS(11)
#undef __ALPHAS__
      LICE_DrawGlyph(dest, cx-r, cy-r, color, alphas, 11, 11, alpha, mode);
      return true;
    }
  }
  else if (r == 6.0f)
  {
    if (aa) {
#define __ALPHAS__(B) \
        A(0.00), A(0.00), A(0.00), A(0.20), A(0.66), A(0.92), A(1.00), A(0.92), A(0.66), A(0.20), A(0.00), A(0.00), A(0.00), \
        A(0.00), A(0.00), A(0.47), A(0.81), B(0.35), B(0.09), B(0.00), B(0.09), B(0.35), A(0.81), A(0.47), A(0.00), A(0.00), \
        A(0.00), A(0.47), B(0.53), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.53), A(0.47), A(0.00), \
        A(0.20), A(0.81), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), A(0.81), A(0.20), \
        A(0.66), B(0.35), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.35), A(0.66), \
        A(0.92), B(0.09), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.09), A(0.92), \
        A(1.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), A(1.00), \
        A(0.92), B(0.09), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.09), A(0.92), \
        A(0.66), B(0.35), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.35), A(0.66), \
        A(0.20), A(0.81), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), A(0.81), A(0.20), \
        A(0.00), A(0.47), B(0.53), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.53), A(0.47), A(0.00), \
        A(0.00), A(0.00), A(0.47), A(0.81), B(0.35), B(0.09), B(0.00), B(0.09), B(0.35), A(0.81), A(0.47), A(0.00), A(0.00), \
        A(0.00), A(0.00), A(0.00), A(0.20), A(0.66), A(0.92), A(1.00), A(0.92), A(0.66), A(0.20), A(0.00), A(0.00), A(0.00),

      DEF_ALPHAS(13)
#undef __ALPHAS__
      LICE_DrawGlyph(dest, cx-r, cy-r, color, alphas, 13, 13, alpha, mode);
      return true;
    }
  }
  else if (r == 7.0f)
  {
    if (aa) {
#define __ALPHAS__(B) \
        A(0.00), A(0.00), A(0.00), A(0.00), A(0.33), A(0.71), A(0.93), A(1.00), A(0.93), A(0.71), A(0.33), A(0.00), A(0.00), A(0.00), A(0.00), \
        A(0.00), A(0.00), A(0.00), A(0.75), A(0.68), B(0.29), B(0.07), B(0.00), B(0.07), B(0.29), A(0.68), A(0.75), A(0.00), A(0.00), A(0.00), \
        A(0.00), A(0.00), A(0.90), B(0.26), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.34), A(0.90), A(0.00), A(0.00), \
        A(0.00), A(0.75), B(0.34), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.26), A(0.75), A(0.00), \
        A(0.33), A(0.68), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), A(0.68), A(0.33), \
        A(0.71), B(0.29), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.29), A(0.71), \
        A(0.93), B(0.07), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.07), A(0.93), \
        A(1.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), A(1.00), \
        A(0.93), B(0.07), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.07), A(0.93), \
        A(0.71), B(0.29), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.29), A(0.71), \
        A(0.33), A(0.68), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), A(0.68), A(0.33), \
        A(0.00), A(0.75), B(0.34), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.26), A(0.75), A(0.00), \
        A(0.00), A(0.00), A(0.90), B(0.26), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.00), B(0.34), A(0.90), A(0.00), A(0.00), \
        A(0.00), A(0.00), A(0.00), A(0.75), A(0.68), B(0.29), B(0.07), B(0.00), B(0.07), B(0.29), A(0.68), A(0.75), A(0.00), A(0.00), A(0.00), \
        A(0.00), A(0.00), A(0.00), A(0.00), A(0.33), A(0.71), A(0.93), A(1.00), A(0.93), A(0.71), A(0.33), A(0.00), A(0.00), A(0.00), A(0.00),

      DEF_ALPHAS(15)
#undef __ALPHAS__
      LICE_DrawGlyph(dest, cx-r, cy-r, color, alphas, 15, 15, alpha, mode);
      return true;
    }
  }

  return false;
}


template <class COMBFUNC> class _LICE_CircleDrawer
{
public:

  static void DrawClippedPt(LICE_IBitmap* dest, int x, int y, const int *clip, 
    int r, int g, int b, int a, int alpha, bool doclip)
  {
    if (doclip && (x < clip[0] || x >= clip[2] || y < clip[1] || y >= clip[3])) return;
    LICE_pixel* px = dest->getBits()+y*dest->getRowSpan()+x;
    COMBFUNC::doPix((LICE_pixel_chan*)px, r, g, b, a, alpha);
  }

  static void DrawClippedHorzLine(LICE_IBitmap* dest, int y, int xlo, int xhi, const int *clip,
    int r, int g, int b, int a, int alpha, bool doclip)
  {
    if (doclip) 
    {
      if (y < clip[1] || y >= clip[3]) return;
      xlo = lice_max(xlo, clip[0]);
      xhi = lice_min(xhi, clip[2]-1);
    }
    LICE_pixel* px = dest->getBits()+y*dest->getRowSpan()+xlo;
    while (xlo <= xhi) 
    {
      COMBFUNC::doPix((LICE_pixel_chan*)px, r, g, b, a, alpha);
      ++px;
      ++xlo;
    }    
  }

 static void DrawClippedVertLine(LICE_IBitmap* dest, int x, int ylo, int yhi, const int *clip,
    int r, int g, int b, int a, int alpha, bool doclip)
  {
    if (doclip) 
    {
      if (x < clip[0] || x >= clip[2]) return;
      ylo = lice_max(ylo, clip[1]);
      yhi = lice_min(yhi, clip[3]-1);
    }
    int span=dest->getRowSpan();
    LICE_pixel* px = dest->getBits()+ylo*span+x;
    while (ylo <= yhi) 
    {
      COMBFUNC::doPix((LICE_pixel_chan*)px, r, g, b, a, alpha);
      px += span;
      ++ylo;
    }    
  }

  static void DrawClippedCircleAA(LICE_IBitmap* dest, float cx, float cy, float rad,
    const int *clip, LICE_pixel color, int ai, bool filled, bool doclip)
  {
    int r = LICE_GETR(color), g = LICE_GETG(color), b = LICE_GETB(color), a = LICE_GETA(color);
    
    const int cx0=(int)(cx+0.5f);
    const int cy0=(int)(cy+0.5f);

    int y=(int)rad;
    double w=rad-floor(rad);
    int wa=(int)((double)ai*w);
 
    DrawClippedPt(dest, cx0, cy0-y-1, clip, r, g, b, a, wa, doclip);
    DrawClippedPt(dest, cx0, cy0+y+1, clip, r, g, b, a, wa, doclip);  
    DrawClippedPt(dest, cx0-y-1, cy0, clip, r, g, b, a, wa, doclip);
    DrawClippedPt(dest, cx0+y+1, cy0, clip, r, g, b, a, wa, doclip);
    
    if (filled)
    {
      DrawClippedVertLine(dest, cx0, cy0-y, cy0-1, clip, r, g, b, a, ai, doclip);
      DrawClippedVertLine(dest, cx0, cy0+1, cy0+y, clip, r, g, b, a, ai, doclip);
      DrawClippedHorzLine(dest, cy0, cx0-y, cx0+y, clip, r, g, b, a, ai, doclip);
    }
    else
    {
      int iwa=ai-wa;
      DrawClippedPt(dest, cx0, cy0-y, clip, r, g, b, a, iwa, doclip); 
      DrawClippedPt(dest, cx0+y, cy0, clip, r, g, b, a, iwa, doclip);  
      DrawClippedPt(dest, cx0, cy0+y, clip, r, g, b, a, iwa, doclip);
      DrawClippedPt(dest, cx0-y, cy0, clip, r, g, b, a, iwa, doclip);
    }

    double r2=rad*rad;
    double yf=sqrt(r2-1.0);
    int yl=(int)(yf+0.5);

    int x=1;
    while (x <= yl)
    {
      y=(int)yf;
      w=yf-floor(yf);
      wa=(int)((double)ai*w);

      DrawClippedPt(dest, cx0-x, cy0-y-1, clip, r, g, b, a, wa, doclip);
      DrawClippedPt(dest, cx0-x, cy0+y+1, clip, r, g, b, a, wa, doclip);
      DrawClippedPt(dest, cx0+x, cy0-y-1, clip, r, g, b, a, wa, doclip);
      DrawClippedPt(dest, cx0+x, cy0+y+1, clip, r, g, b, a, wa, doclip);
      if (x != yl)
      {
        DrawClippedPt(dest, cx0-y-1, cy0-x, clip, r, g, b, a, wa, doclip);
        DrawClippedPt(dest, cx0+y+1, cy0-x, clip, r, g, b, a, wa, doclip);
        DrawClippedPt(dest, cx0-y-1, cy0+x, clip, r, g, b, a, wa, doclip);
        DrawClippedPt(dest, cx0+y+1, cy0+x, clip, r, g, b, a, wa, doclip);
      }

      if (filled)
      {
        DrawClippedVertLine(dest, cx0-x, cy0-y, cy0-x-1, clip, r, g, b, a, ai, doclip);
        DrawClippedVertLine(dest, cx0-x, cy0+x+1, cy0+y, clip, r, g, b, a, ai, doclip);
        DrawClippedHorzLine(dest, cy0-x, cx0-y, cx0-x, clip, r, g, b, a, ai, doclip);
        DrawClippedHorzLine(dest, cy0-x, cx0+x, cx0+y, clip, r, g, b, a, ai, doclip);
        DrawClippedHorzLine(dest, cy0+x, cx0-y, cx0-x, clip, r, g, b, a, ai, doclip);
        DrawClippedHorzLine(dest, cy0+x, cx0+x, cx0+y, clip, r, g, b, a, ai, doclip);
        DrawClippedVertLine(dest, cx0+x, cy0-y, cy0-x-1, clip, r, g, b, a, ai, doclip);
        DrawClippedVertLine(dest, cx0+x, cy0+x+1, cy0+y, clip, r, g, b, a, ai, doclip);
      }
      else
      {
        int iwa=ai-wa;
        DrawClippedPt(dest, cx0-y, cy0-x, clip, r, g, b, a, iwa, doclip);
        DrawClippedPt(dest, cx0+y, cy0-x, clip, r, g, b, a, iwa, doclip);     
        DrawClippedPt(dest, cx0-x, cy0+y, clip, r, g, b, a, iwa, doclip);
        DrawClippedPt(dest, cx0+x, cy0+y, clip, r, g, b, a, iwa, doclip);
        if (x != yl) 
        {
          DrawClippedPt(dest, cx0-x, cy0-y, clip, r, g, b, a, iwa, doclip);  
          DrawClippedPt(dest, cx0+x, cy0-y, clip, r, g, b, a, iwa, doclip);
          DrawClippedPt(dest, cx0-y, cy0+x, clip, r, g, b, a, iwa, doclip);
          DrawClippedPt(dest, cx0+y, cy0+x, clip, r, g, b, a, iwa, doclip); 
        }
      }
      
      ++x;
      yf=sqrt(r2-(double)(x*x));
      yl=(int)(yf+0.5);
    }
  }

  static void DrawClippedCircle(LICE_IBitmap* dest, float cx, float cy, float rad,
    const int *clip, LICE_pixel color, int ai, bool filled, bool doclip)
  {
    const int r = LICE_GETR(color), g = LICE_GETG(color), b = LICE_GETB(color), a = LICE_GETA(color);

    const int cx0=(int)(cx+0.5f);
    const int cy0=(int)(cy+0.5f);
    const int r0=(int)(rad+0.5f);
   
    if (filled)
    {
      DrawClippedVertLine(dest, cx0, cy0-r0, cy0-1, clip, r, g, b, a, ai, doclip);
      DrawClippedVertLine(dest, cx0, cy0+1, cy0+r0, clip, r, g, b, a, ai, doclip);
      DrawClippedHorzLine(dest, cy0, cx0-r0, cx0+r0, clip, r, g, b, a, ai, doclip);
    }
    else
    {
      DrawClippedPt(dest, cx0, cy0-r0, clip, r, g, b, a, ai, doclip);
      DrawClippedPt(dest, cx0+r0, cy0, clip, r, g, b, a, ai, doclip);
      DrawClippedPt(dest, cx0, cy0+r0, clip, r, g, b, a, ai, doclip);
      DrawClippedPt(dest, cx0-r0, cy0, clip, r, g, b, a, ai, doclip);
    }  

    int x=0;
    int y=r0;
    int e=-r0;
    while (++x < y)
    {
      if (e < 0) 
      {
        e += 2*x+1;
      }
      else
      {
        --y;
        e += 2*(x-y)+1;
      }

      if (filled)
      {
        DrawClippedVertLine(dest, cx0-x, cy0-y, cy0-x-1, clip, r, g, b, a, ai, doclip);
        DrawClippedVertLine(dest, cx0-x, cy0+x+1, cy0+y, clip, r, g, b, a, ai, doclip);
        DrawClippedHorzLine(dest, cy0-x, cx0-y, cx0-x, clip, r, g, b, a, ai, doclip);
        DrawClippedHorzLine(dest, cy0-x, cx0+x, cx0+y, clip, r, g, b, a, ai, doclip);
        DrawClippedHorzLine(dest, cy0+x, cx0-y, cx0-x, clip, r, g, b, a, ai, doclip);
        DrawClippedHorzLine(dest, cy0+x, cx0+x, cx0+y, clip, r, g, b, a, ai, doclip);
        DrawClippedVertLine(dest, cx0+x, cy0-y, cy0-x-1, clip, r, g, b, a, ai, doclip);
        DrawClippedVertLine(dest, cx0+ x, cy0+x+1, cy0+y, clip, r, g, b, a, ai, doclip);
      }
      else
      {
        DrawClippedPt(dest, cx0-x, cy0-y, clip, r, g, b, a, ai, doclip);  
        DrawClippedPt(dest, cx0-x, cy0+y, clip, r, g, b, a, ai, doclip);
        DrawClippedPt(dest, cx0+x, cy0-y, clip, r, g, b, a, ai, doclip);
        DrawClippedPt(dest, cx0+x, cy0+y, clip, r, g, b, a, ai, doclip);
        if (x != y)
        {
          DrawClippedPt(dest, cx0-y, cy0-x, clip, r, g, b, a, ai, doclip);
          DrawClippedPt(dest, cx0-y, cy0+x, clip, r, g, b, a, ai, doclip);
          DrawClippedPt(dest, cx0+y, cy0-x, clip, r, g, b, a, ai, doclip);
          DrawClippedPt(dest, cx0+y, cy0+x, clip, r, g, b, a, ai, doclip); 
        }
      }
    }
  }

};


static void __DrawCircleClipped(LICE_IBitmap* dest, float cx, float cy, float rad,
  LICE_pixel color, int ia, bool aa, bool filled, int mode, const int *clip, bool doclip)
{
  // todo: more clipped/filled versions (to optimize constants out?)
  if (aa) 
  {
    #define __LICE__ACTION(COMBFUNC) _LICE_CircleDrawer<COMBFUNC>::DrawClippedCircleAA(dest, cx, cy, rad, clip, color, ia, filled, doclip)
      __LICE_ACTION_NOSRCALPHA(mode, ia,false)
    #undef __LICE__ACTION
  }
  else 
  {
    #define __LICE__ACTION(COMBFUNC) _LICE_CircleDrawer<COMBFUNC>::DrawClippedCircle(dest, cx, cy, rad, clip, color, ia, filled, doclip)
      __LICE_ACTION_CONSTANTALPHA(mode,ia,false)
    #undef __LICE__ACTION
  }
}


static void __DrawArc(int w, int h, LICE_IBitmap* dest, float cx, float cy, float rad, double anglo, double anghi,
  LICE_pixel color, int ialpha, bool aa, int mode)
{
  const int __sc = (int)dest->Extended(LICE_EXT_GET_SCALING,NULL);
  if (__sc>0)
  {
    __LICE_SCU(w);
    __LICE_SCU(h);
    if (!IGNORE_SCALING(mode))
    {
      __LICE_SC(cx);
      __LICE_SC(cy);
      __LICE_SC(rad);
    }
  }
  // -2PI <= anglo <= anghi <= 2PI
  anglo += 2.0*_PI;
  anghi += 2.0*_PI;

  // 0 <= anglo <= anghi <= 4PI

  double next_ang = anglo - fmod(anglo,0.5*_PI);

  int ly = (int)(cy - rad*cos(anglo) + 0.5);
  int lx = (int)(cx + rad*sin(anglo) + 0.5);

  while (anglo < anghi)
  {
    next_ang += 0.5*_PI;
    if (next_ang > anghi) next_ang = anghi;

    int yhi = (int) (cy-rad*cos(next_ang)+0.5);
    int xhi = (int) (cx+rad*sin(next_ang)+0.5);
    int ylo = ly;
    int xlo = lx;

    ly = yhi;
    lx = xhi;
    
    if (yhi < ylo) { int tmp = ylo; ylo = yhi; yhi=tmp; }  
    if (xhi < xlo) { int tmp = xlo; xlo = xhi; xhi=tmp; }

    anglo = next_ang;

    if (xhi != cx) xhi++;
    if (yhi != cy) yhi++;

    const int clip[4]={lice_max(xlo,0),lice_max(0, ylo),lice_min(w,xhi+1),lice_min(h, yhi+1)};

    __DrawCircleClipped(dest,cx,cy,rad,color,ialpha,aa,false,mode,clip,true);
  }
}

void LICE_Arc(LICE_IBitmap* dest, float cx, float cy, float r, float minAngle, float maxAngle, 
	LICE_pixel color, float alpha, int mode, bool aa)
{
  if (!dest) return;

  if (dest->isFlipped()) { cy=dest->getHeight()-1-cy; minAngle=_PI-minAngle; maxAngle=_PI-maxAngle; }

  if (maxAngle < minAngle)
  {
    float tmp=maxAngle; 
    maxAngle=minAngle;
    minAngle=tmp;
  }

  if (maxAngle - minAngle >= 2.0f*_PI) 
  {
    LICE_Circle(dest,cx,cy,r,color,alpha,mode,aa);
    return;
  }

  if (maxAngle >= 2.0f*_PI)
  {
    float tmp = fmod(maxAngle,2.0f*_PI);
    minAngle -= maxAngle - tmp; // reduce by factors of 2PI 
    maxAngle = tmp;
  }
  else if (minAngle <= -2.0f*_PI)
  {
    float tmp = fmod(minAngle,2.0f*_PI);
    maxAngle -= minAngle - tmp; // toward zero by factors of 2pi
    minAngle = tmp;
  }

  // -2PI <= minAngle <= maxAngle <= 2PI

  int ia = (int) (alpha*256.0f);
  if (!ia) return;

  __DrawArc(dest->getWidth(),dest->getHeight(),dest,cx,cy,r,minAngle,maxAngle,color,ia,aa,mode);
}




void LICE_Circle(LICE_IBitmap* dest, float cx, float cy, float r, LICE_pixel color, float alpha, int mode, bool aa)
{
  if (!dest) return;

  int w = dest->getWidth(), h = dest->getHeight();
  const int __sc = (int)dest->Extended(LICE_EXT_GET_SCALING,NULL);
  if (__sc>0)
  {
    __LICE_SCU(w);
    __LICE_SCU(h);
    if (!IGNORE_SCALING(mode))
    {
      __LICE_SC(cx);
      __LICE_SC(cy);
      __LICE_SC(r);
    }
  }

  const int clip[4] = { 0, 0, w, h };
  if (w < 1 || h <1 || r<0 || 
      (int)cx+(int)r < -2 || (int)cy + (int)r < - 2 ||
      (int)cx-(int)r > w + 2 || (int)cy - (int)r > h + 2
    ) return;

  int ia = (int) (alpha*256.0f);
  if (!ia) return;

  if (CachedCircle(dest, cx, cy, r, color, alpha, mode|LICE_BLIT_IGNORE_SCALING, aa, false)) return;

  if (dest->isFlipped()) cy=h-1-cy;

  const bool doclip = !(cx-r-2 >= 0 && cy-r-2 >= 0 && cx+r+2 < w && cy+r+2 < h);

  __DrawCircleClipped(dest,cx,cy,r,color,ia,aa,false,mode,clip,doclip);
}

void LICE_FillCircle(LICE_IBitmap* dest, float cx, float cy, float r, LICE_pixel color, float alpha, int mode, bool aa)
{
  if (!dest) return;

  int w = dest->getWidth(), h = dest->getHeight();
  const int __sc = (int)dest->Extended(LICE_EXT_GET_SCALING,NULL);
  if (__sc>0)
  {
    __LICE_SCU(w);
    __LICE_SCU(h);
    if (!IGNORE_SCALING(mode))
    {
      __LICE_SC(cx);
      __LICE_SC(cy);
      __LICE_SC(r);
    }
  }

  if (w < 1 || h < 1 || r < 0.0 || 
      (int)cx+(int)r < -2 || (int)cy + (int)r < - 2 ||
      (int)cx-(int)r > w + 2 || (int)cy - (int)r > h + 2
      ) return;

  const int ia = (int) (alpha*256.0f);
  if (!ia) return;

  if (CachedCircle(dest, cx, cy, r, color, alpha, mode|LICE_BLIT_IGNORE_SCALING, aa, true)) return;

  if (dest->isFlipped()) cy=h-1-cy;

  const int clip[4] = { 0, 0, w, h };

  const bool doclip = !(cx-r-2 >= 0 && cy-r-2 >= 0 && cx+r+2 < w && cy+r+2 < h);
  __DrawCircleClipped(dest,cx,cy,r,color,ia,aa,true,mode,clip,doclip);
}


void LICE_RoundRect(LICE_IBitmap *drawbm, float xpos, float ypos, float w, float h, int cornerradius,
                    LICE_pixel col, float alpha, int mode, bool aa)
{
  if (cornerradius>0)
  {
    float cr=cornerradius;
    if (cr > w*0.5) cr=w*0.5;
    if (cr > h*0.5) cr=h*0.5;
    cr=floor(cr);

    if (cr>=2)
    {
      double adj = 0.0;
      const int __sc = IGNORE_SCALING(mode) ? 0 : drawbm ? (int)drawbm->Extended(LICE_EXT_GET_SCALING,NULL) : 0;
      if (__sc>0)
      {
        adj = 1.0 - 256.0/__sc;

        LICE_FLine(drawbm,xpos+cr+adj,ypos+adj,xpos+w-cr,ypos+adj,col,alpha,mode,true);
        LICE_FLine(drawbm,xpos+cr-1+adj,ypos+h-adj,xpos+w-cr-adj,ypos+h-adj,col,alpha,mode,true);
        LICE_FLine(drawbm,xpos+w-adj,ypos+cr+adj,xpos+w-adj,ypos+h-cr-adj,col,alpha,mode,true);
        LICE_FLine(drawbm,xpos+adj,ypos+cr-1+adj,xpos+adj,ypos+h-cr-adj,col,alpha,mode,true);
//        aa=true;
      }
      else
      {
        LICE_Line(drawbm,xpos+cr,ypos,xpos+w-cr,ypos,col,alpha,mode,aa);
        LICE_Line(drawbm,xpos+cr-1,ypos+h,xpos+w-cr,ypos+h,col,alpha,mode,aa);
        LICE_Line(drawbm,xpos+w,ypos+cr,xpos+w,ypos+h-cr,col,alpha,mode,aa);
        LICE_Line(drawbm,xpos,ypos+cr-1,xpos,ypos+h-cr,col,alpha,mode,aa);
      }

      LICE_Arc(drawbm,xpos+cr+adj,ypos+cr+adj,cr,-_PI*0.5f,0,col,alpha,mode,aa);
      LICE_Arc(drawbm,xpos+w-cr-adj,ypos+cr+adj,cr,0,_PI*0.5f,col,alpha,mode,aa);
      LICE_Arc(drawbm,xpos+w-cr-adj,ypos+h-cr-adj,cr,_PI*0.5f,_PI,col,alpha,mode,aa);
      LICE_Arc(drawbm,xpos+cr+adj,ypos+h-cr-adj,cr,_PI,_PI*1.5f,col,alpha,mode,aa);

      return;
    }
  }

  LICE_DrawRect(drawbm, xpos, ypos, w, h, col, alpha, mode);
}

