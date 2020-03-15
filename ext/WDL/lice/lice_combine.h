#ifndef _LICE_COMBINE_H_
#define _LICE_COMBINE_H_


#if defined(_MSC_VER)
#pragma warning(disable:4244) // float-to-int
#endif

#define __LICE_BOUND(x,lo,hi) ((x)<(lo)?(lo):((x)>(hi)?(hi):(x)))


#define LICE_PIXEL_HALF(x) (((x)>>1)&0x7F7F7F7F)
#define LICE_PIXEL_QUARTER(x) (((x)>>2)&0x3F3F3F3F)
#define LICE_PIXEL_EIGHTH(x) (((x)>>3)&0x1F1F1F1F)


static inline void __LICE_BilinearFilterI(int *r, int *g, int *b, int *a, const LICE_pixel_chan *pin, const LICE_pixel_chan *pinnext, unsigned int xfrac, unsigned int yfrac)
{
  const unsigned int f4=(xfrac*yfrac)>>16;
  const unsigned int f3=yfrac-f4; // (1.0-xfrac)*yfrac;
  const unsigned int f2=xfrac-f4; // xfrac*(1.0-yfrac);
  const unsigned int f1=65536-yfrac-xfrac+f4; // (1.0-xfrac)*(1.0-yfrac);
  #define DOCHAN(output, inchan) \
    (output)=(pin[(inchan)]*f1 + pin[4+(inchan)]*f2 + pinnext[(inchan)]*f3 + pinnext[4+(inchan)]*f4)>>16;
  DOCHAN(*r,LICE_PIXEL_R)
  DOCHAN(*g,LICE_PIXEL_G)
  DOCHAN(*b,LICE_PIXEL_B)
  DOCHAN(*a,LICE_PIXEL_A)
  #undef DOCHAN
}

static inline void __LICE_BilinearFilterIPixOut(LICE_pixel_chan *out, const LICE_pixel_chan *pin, const LICE_pixel_chan *pinnext, unsigned int xfrac, unsigned int yfrac)
{
  const unsigned int f4=(xfrac*yfrac)>>16;
  const unsigned int f3=yfrac-f4; // (1.0-xfrac)*yfrac;
  const unsigned int f2=xfrac-f4; // xfrac*(1.0-yfrac);
  const unsigned int f1=65536-yfrac-xfrac+f4; // (1.0-xfrac)*(1.0-yfrac);
  #define DOCHAN(inchan) \
    (out[inchan])=(pin[(inchan)]*f1 + pin[4+(inchan)]*f2 + pinnext[(inchan)]*f3 + pinnext[4+(inchan)]*f4)>>16;
  DOCHAN(LICE_PIXEL_R)
  DOCHAN(LICE_PIXEL_G)
  DOCHAN(LICE_PIXEL_B)
  DOCHAN(LICE_PIXEL_A)
  #undef DOCHAN
}


static inline void __LICE_BilinearFilterI_2(int *r, int *g, int *b, int *a, const LICE_pixel_chan *pin, const LICE_pixel_chan *pinnext, int npoffs, unsigned int xfrac, unsigned int yfrac)
{
  const unsigned int f4=(xfrac*yfrac)>>16;
  const unsigned int f3=yfrac-f4; // (1.0-xfrac)*yfrac;
  const unsigned int f2=xfrac-f4; // xfrac*(1.0-yfrac);
  const unsigned int f1=65536-yfrac-xfrac+f4; // (1.0-xfrac)*(1.0-yfrac);
  *r=(pin[LICE_PIXEL_R]*f1 + pin[npoffs+LICE_PIXEL_R]*f2 + pinnext[LICE_PIXEL_R]*f3 + pinnext[npoffs+LICE_PIXEL_R]*f4)>>16;
  *g=(pin[LICE_PIXEL_G]*f1 + pin[npoffs+LICE_PIXEL_G]*f2 + pinnext[LICE_PIXEL_G]*f3 + pinnext[npoffs+LICE_PIXEL_G]*f4)>>16;
  *b=(pin[LICE_PIXEL_B]*f1 + pin[npoffs+LICE_PIXEL_B]*f2 + pinnext[LICE_PIXEL_B]*f3 + pinnext[npoffs+LICE_PIXEL_B]*f4)>>16;
  *a=(pin[LICE_PIXEL_A]*f1 + pin[npoffs+LICE_PIXEL_A]*f2 + pinnext[LICE_PIXEL_A]*f3 + pinnext[npoffs+LICE_PIXEL_A]*f4)>>16;
}


static inline void __LICE_LinearFilterI(int *r, int *g, int *b, int *a, const LICE_pixel_chan *pin, const LICE_pixel_chan *pinnext, unsigned int frac)
{
  const unsigned int f=65536-frac;
  *r=(pin[LICE_PIXEL_R]*f + pinnext[LICE_PIXEL_R]*frac)>>16;
  *g=(pin[LICE_PIXEL_G]*f + pinnext[LICE_PIXEL_G]*frac)>>16;
  *b=(pin[LICE_PIXEL_B]*f + pinnext[LICE_PIXEL_B]*frac)>>16;
  *a=(pin[LICE_PIXEL_A]*f + pinnext[LICE_PIXEL_A]*frac)>>16;
}
static inline void __LICE_LinearFilterIPixOut(LICE_pixel_chan *out, const LICE_pixel_chan *pin, const LICE_pixel_chan *pinnext, unsigned int frac)
{
  const unsigned int f=65536-frac;
  out[LICE_PIXEL_R]=(pin[LICE_PIXEL_R]*f + pinnext[LICE_PIXEL_R]*frac)>>16;
  out[LICE_PIXEL_G]=(pin[LICE_PIXEL_G]*f + pinnext[LICE_PIXEL_G]*frac)>>16;
  out[LICE_PIXEL_B]=(pin[LICE_PIXEL_B]*f + pinnext[LICE_PIXEL_B]*frac)>>16;
  out[LICE_PIXEL_A]=(pin[LICE_PIXEL_A]*f + pinnext[LICE_PIXEL_A]*frac)>>16;
}

static void inline _LICE_MakePixelClamp(LICE_pixel_chan *out, int r, int g, int b, int a)
{
#define LICE_PIX_MAKECHAN(a,b) out[a] = (b&~0xff) ? (b<0?0:255) : b; 
  LICE_PIX_MAKECHAN(LICE_PIXEL_B,b)
  LICE_PIX_MAKECHAN(LICE_PIXEL_G,g)
  LICE_PIX_MAKECHAN(LICE_PIXEL_R,r)
  LICE_PIX_MAKECHAN(LICE_PIXEL_A,a)
#undef LICE_PIX_MAKECHAN
}

static void inline _LICE_MakePixelNoClamp(LICE_pixel_chan *out, LICE_pixel_chan r, LICE_pixel_chan g, LICE_pixel_chan b, LICE_pixel_chan a)
{
#define LICE_PIX_MAKECHAN(a,b) out[a] = b;
  LICE_PIX_MAKECHAN(LICE_PIXEL_B,b)
  LICE_PIX_MAKECHAN(LICE_PIXEL_G,g)
  LICE_PIX_MAKECHAN(LICE_PIXEL_R,r)
  LICE_PIX_MAKECHAN(LICE_PIXEL_A,a)
#undef LICE_PIX_MAKECHAN
}



#define HSV_P v*(256-s)/256
#define HSV_Q(hval) v*(16384-(hval)*s)/16384
#define HSV_T(hval) v*(16384-(64-(hval))*s)/16384
#define HSV_X v
extern unsigned short _LICE_RGB2HSV_invtab[256]; // 65536/idx - 1

#ifdef LICE_COMBINE_IMPLEMENT_HSV
  LICE_pixel LICE_HSV2Pix(int h, int s, int v, int alpha)
  #define __LICE_HSV2Pix LICE_HSV2Pix
#else
  static inline LICE_pixel __LICE_HSV2Pix(int h, int s, int v, int alpha)
#endif
{
  if (h<192)
  {
    if (h<64) return LICE_RGBA(HSV_X,HSV_T(h),HSV_P,alpha); 
    if (h<128) return LICE_RGBA(HSV_Q(h-64),HSV_X,HSV_P,alpha); 
    return LICE_RGBA(HSV_P,HSV_X,HSV_T(h-128),alpha); 
  }
  if (h < 256) return LICE_RGBA(HSV_P,HSV_Q(h-192),HSV_X,alpha); 
  if (h < 320) return LICE_RGBA(HSV_T(h-256),HSV_P,HSV_X,alpha); 
  return LICE_RGBA(HSV_X,HSV_P,HSV_Q(h-320),alpha); 
}

#ifdef LICE_COMBINE_IMPLEMENT_HSV
void LICE_HSV2RGB(int h, int s, int v, int* r, int* g, int* b)
#define __LICE_HSV2RGB LICE_HSV2RGB
#else
static inline void __LICE_HSV2RGB(int h, int s, int v, int* r, int* g, int* b)
#endif
{
  if (h<192)
  {
    if (h<64)
    {
      *r = HSV_X; *g = HSV_T(h); *b = HSV_P; 
    }
    else if (h<128)
    {
      *r = HSV_Q(h-64); *g = HSV_X; *b = HSV_P; 
    }
    else
    {
      *r = HSV_P; *g = HSV_X; *b = HSV_T(h-128); 
    }
  }
  else
  {
    if (h < 256)
    {
      *r = HSV_P; *g = HSV_Q(h-192); *b = HSV_X; 
    }
    else if (h < 320)
    {
      *r = HSV_T(h-256); *g = HSV_P; *b = HSV_X; 
    }
    else
    {
      *r = HSV_X; *g = HSV_P; *b = HSV_Q(h-320); 
    }
  }
}


#define LICE_RGB2HSV_USE_TABLE
// h = [0,384), s and v = [0,256)

#ifdef LICE_COMBINE_IMPLEMENT_HSV
  void LICE_RGB2HSV(int r, int g, int b, int* h, int* s, int* v)
  #define __LICE_RGB2HSV LICE_RGB2HSV
#else
  static inline void __LICE_RGB2HSV(int r, int g, int b, int* h, int* s, int* v)
#endif
{

  // this makes it just 3 conditional branches per call
  int df,d,maxrgb;
  int degoffs;
  if (g > r)
  {
    if (g>b) // green max
    {
      maxrgb=g;
      degoffs=128;
      df = maxrgb - lice_min(b,r);
      d=b-r;
    }
    else // blue max
    {
      maxrgb=b;
      degoffs=256;
      df = maxrgb - lice_min(g,r);
      d=r-g;
    }
  }
  else // r >= g
  {
    if (r > b) // red max
    {
      maxrgb=r;

      if (g<b)
      {
        degoffs=383; // not technically correct, but close enough (and simplifies the rounding case -- if you want more accuracy, set to 384,
                     // then add a if (*h == 384) *h=0; after the *h assignment below
        df = maxrgb - g;
      }
      else
      {
        degoffs=0;
        df = maxrgb - b;
      }
      d=g-b;
    }
    else  // blue max
    {
      maxrgb=b;
      degoffs=256;
      df = maxrgb - lice_min(g,r);
      d=r-g;
    }
  }

  
  *v = maxrgb;
#ifndef LICE_RGB2HSV_USE_TABLE // table mode doesnt need this check
  if (!df) {
    *h = *s = 0;
  }
  else 
#endif
  {

#ifdef LICE_RGB2HSV_USE_TABLE


    *h = (d*((int)(_LICE_RGB2HSV_invtab[df]+1)))/1024  + degoffs;
    *s = (df*((int)_LICE_RGB2HSV_invtab[maxrgb]))/256;
#else
    *h = ((d*64)/df) + degoffs;
    *s = (df*256)/(maxrgb+1);
#endif
  }
}

//void doPix(LICE_pixel_chan *dest, int r, int g, int b, int a, int alpha)    // alpha is ignored.
// generally speaking, the "a"  is 0-255, and alpha is 0-256/1-256.

// Optimization when a=255 and alpha=1.0f, useful for doing a big vector drawn fill or something.
// This could be called _LICE_PutPixel but that would probably be confusing.
class _LICE_CombinePixelsClobberNoClamp
{
public:
  static inline void doPix(LICE_pixel_chan *dest, int r, int g, int b, int a, int alpha)    // alpha is ignored.
  {
    _LICE_MakePixelNoClamp(dest, r, g, b, a);
  }
};

class _LICE_CombinePixelsClobberClamp
{
public:
  static inline void doPix(LICE_pixel_chan *dest, int r, int g, int b, int a, int alpha)    // alpha is ignored.
  {
    _LICE_MakePixelClamp(dest, r, g, b, a);
  }
};

class _LICE_CombinePixelsClobberFAST
{
public:
  static inline void doPixFAST(LICE_pixel *dest, LICE_pixel src)    // alpha is ignored.
  {
    *dest = src;
  }
};

class _LICE_CombinePixelsHalfMixNoClamp
{
public:
  static inline void doPix(LICE_pixel_chan *dest, int r, int g, int b, int a, int alpha)
  {
    _LICE_MakePixelNoClamp(dest,
      (dest[LICE_PIXEL_R]+r)>>1,
      (dest[LICE_PIXEL_G]+g)>>1,
      (dest[LICE_PIXEL_B]+b)>>1,
      (dest[LICE_PIXEL_A]+a)>>1);
  }
};

class _LICE_CombinePixelsHalfMixFAST
{
public:
  static inline void doPixFAST(LICE_pixel *dest, LICE_pixel src)    // src is full range
  {
    *dest = ((*dest>>1) &0x7f7f7f7f) + ((src>>1)&0x7f7f7f7f);
  }
};

class _LICE_CombinePixelsHalfMixClamp
{
public:
  static inline void doPix(LICE_pixel_chan *dest, int r, int g, int b, int a, int alpha)
  {
    _LICE_MakePixelClamp(dest,
      (dest[LICE_PIXEL_R]+r)>>1,
      (dest[LICE_PIXEL_G]+g)>>1,
      (dest[LICE_PIXEL_B]+b)>>1,
      (dest[LICE_PIXEL_A]+a)>>1);
  }

};


class _LICE_CombinePixelsHalfMix2FAST
{
public:
  static inline void doPixFAST(LICE_pixel *dest, LICE_pixel src)    // src is pre-halfed and masked
  {
    *dest = ((*dest>>1) &0x7f7f7f7f) + src;
  }
};

class _LICE_CombinePixelsQuarterMix2FAST
{
public:
  static inline void doPixFAST(LICE_pixel *dest, LICE_pixel src)    // src is pre-quartered and masked
  {
    LICE_pixel tmp = *dest;
    *dest = ((tmp>>1) &0x7f7f7f7f) + ((tmp>>2) &0x3f3f3f3f) + src;
  }
};

class _LICE_CombinePixelsThreeEighthMix2FAST
{
public:
  static inline void doPixFAST(LICE_pixel *dest, LICE_pixel src)    // src is pre-three-eighthed and masked
  {
    LICE_pixel tmp = *dest;
    *dest = ((tmp>>1) &0x7f7f7f7f) + ((tmp>>3) &0x1f1f1f1f) + src;
  }
};

class _LICE_CombinePixelsThreeQuarterMix2FAST
{
public:
  static inline void doPixFAST(LICE_pixel *dest, LICE_pixel src)    // src is pre-three-quartered and masked
  {
    *dest = ((*dest>>2) &0x3f3f3f3f) + src;
  }
};

class _LICE_CombinePixelsCopyNoClamp
{
public:
  static inline void doPix(LICE_pixel_chan *dest, int r, int g, int b, int a, int alpha)
  {
    const int sc=(256-alpha);

    // don't check alpha=0 here, since the caller should (since alpha is usually used for static alphas)
    _LICE_MakePixelNoClamp(dest,
        r + ((dest[LICE_PIXEL_R]-r)*sc)/256,
        g + ((dest[LICE_PIXEL_G]-g)*sc)/256,
        b + ((dest[LICE_PIXEL_B]-b)*sc)/256,
        a + ((dest[LICE_PIXEL_A]-a)*sc)/256);
  }
};

class _LICE_CombinePixelsCopyClamp 
{
public:
  static inline void doPix(LICE_pixel_chan *dest, int r, int g, int b, int a, int alpha)
  {
    const int sc=(256-alpha);

    // don't check alpha=0 here, since the caller should (since alpha is usually used for static alphas)
    _LICE_MakePixelClamp(dest,
        r + ((dest[LICE_PIXEL_R]-r)*sc)/256,
        g + ((dest[LICE_PIXEL_G]-g)*sc)/256,
        b + ((dest[LICE_PIXEL_B]-b)*sc)/256,
        a + ((dest[LICE_PIXEL_A]-a)*sc)/256);
  }
};

class _LICE_CombinePixelsCopySourceAlphaNoClamp
{
public:
  static inline void doPix(LICE_pixel_chan *dest, int r, int g, int b, int a, int alpha)
  {
    if (a)
    {
      const int sc2=(alpha*(a+1))/256;
      const int sc = 256 - sc2;

      _LICE_MakePixelNoClamp(dest,
        r + ((dest[LICE_PIXEL_R]-r)*sc)/256,
        g + ((dest[LICE_PIXEL_G]-g)*sc)/256,
        b + ((dest[LICE_PIXEL_B]-b)*sc)/256,
        lice_min(255,sc2 + dest[LICE_PIXEL_A]));
    }
  }
};

class _LICE_CombinePixelsCopySourceAlphaClamp
{
public:
  static inline void doPix(LICE_pixel_chan *dest, int r, int g, int b, int a, int alpha)
  {
    if (a)
    {
      const int sc2=(alpha*(a+1))/256;
      const int sc = 256 - sc2;

      _LICE_MakePixelClamp(dest,
        r + ((dest[LICE_PIXEL_R]-r)*sc)/256,
        g + ((dest[LICE_PIXEL_G]-g)*sc)/256,
        b + ((dest[LICE_PIXEL_B]-b)*sc)/256,
        sc2 + dest[LICE_PIXEL_A]);  
    }
  }
};
class _LICE_CombinePixelsCopySourceAlphaIgnoreAlphaParmNoClamp
{
public:
  static inline void doPix(LICE_pixel_chan *dest, int r, int g, int b, int a, int alpha)
  {
    if (a)
    {
      if (a==255)
      {
        _LICE_MakePixelNoClamp(dest,r,g,b,a);
      }
      else
      {
        const int sc=(255-a);

        _LICE_MakePixelNoClamp(dest,
            r + ((dest[LICE_PIXEL_R]-r)*sc)/256,
            g + ((dest[LICE_PIXEL_G]-g)*sc)/256,
            b + ((dest[LICE_PIXEL_B]-b)*sc)/256,
            lice_min(255,a + dest[LICE_PIXEL_A]));  
      }
    }
  }
};
class _LICE_CombinePixelsCopySourceAlphaIgnoreAlphaParmClamp
{
public:
  static inline void doPix(LICE_pixel_chan *dest, int r, int g, int b, int a, int alpha)
  {
    if (a)
    {
      if (a==255)
      {
        _LICE_MakePixelClamp(dest,r,g,b,a);
      }
      else
      {
        const int sc=(255-a);

        _LICE_MakePixelClamp(dest,
           r + ((dest[LICE_PIXEL_R]-r)*sc)/256,
           g + ((dest[LICE_PIXEL_G]-g)*sc)/256,
           b + ((dest[LICE_PIXEL_B]-b)*sc)/256,
           a + dest[LICE_PIXEL_A]);  
      }
    }
  }
};

#ifndef LICE_DISABLE_BLEND_ADD

class _LICE_CombinePixelsAdd
{
public:
  static inline void doPix(LICE_pixel_chan *dest, int r, int g, int b, int a, int alpha)
  { 
    // don't check alpha=0 here, since the caller should (since alpha is usually used for static alphas)

    _LICE_MakePixelClamp(dest,
      dest[LICE_PIXEL_R]+(r*alpha)/256,
      dest[LICE_PIXEL_G]+(g*alpha)/256,
      dest[LICE_PIXEL_B]+(b*alpha)/256,
      dest[LICE_PIXEL_A]+(a*alpha)/256);

  }
};
class _LICE_CombinePixelsAddSourceAlpha
{
public:
  static inline void doPix(LICE_pixel_chan *dest, int r, int g, int b, int a, int alpha)
  { 
    if (a)
    {
      alpha=(alpha*(a+1))/256;
      _LICE_MakePixelClamp(dest,
        dest[LICE_PIXEL_R]+(r*alpha)/256,
        dest[LICE_PIXEL_G]+(g*alpha)/256,
        dest[LICE_PIXEL_B]+(b*alpha)/256,
        dest[LICE_PIXEL_A]+(a*alpha)/256);
    }
  }
};

#else // !LICE_DISABLE_BLEND_ADD
#define _LICE_CombinePixelsAddSourceAlpha _LICE_CombinePixelsCopySourceAlphaClamp
#define _LICE_CombinePixelsAdd _LICE_CombinePixelsCopyClamp
#endif

#ifndef LICE_DISABLE_BLEND_DODGE

class _LICE_CombinePixelsColorDodge
{
public:
  static inline void doPix(LICE_pixel_chan *dest, int r, int g, int b, int a, int alpha)
  { 
      const int src_r = 256-r*alpha/256;
      const int src_g = 256-g*alpha/256;
      const int src_b = 256-b*alpha/256;
      const int src_a = 256-a*alpha/256;

      _LICE_MakePixelClamp(dest,
        src_r > 1 ? 256*dest[LICE_PIXEL_R] / src_r : 256*dest[LICE_PIXEL_R],
        src_g > 1 ? 256*dest[LICE_PIXEL_G] / src_g : 256*dest[LICE_PIXEL_G],
        src_b > 1 ? 256*dest[LICE_PIXEL_B] / src_b : 256*dest[LICE_PIXEL_B],
        src_a > 1 ? 256*dest[LICE_PIXEL_A] / src_a : 256*dest[LICE_PIXEL_A]);
  }
};

class _LICE_CombinePixelsColorDodgeSourceAlpha
{
public:
  static inline void doPix(LICE_pixel_chan *dest, int r, int g, int b, int a, int alpha)
  { 
      const int ualpha=(alpha*(a+1))/256;
    
      const int src_r = 256-r*ualpha/256;
      const int src_g = 256-g*ualpha/256;
      const int src_b = 256-b*ualpha/256;
      const int src_a = 256-a*ualpha/256;

      _LICE_MakePixelClamp(dest,
        src_r > 1 ? 256*dest[LICE_PIXEL_R] / src_r : 256*dest[LICE_PIXEL_R],
        src_g > 1 ? 256*dest[LICE_PIXEL_G] / src_g : 256*dest[LICE_PIXEL_G],
        src_b > 1 ? 256*dest[LICE_PIXEL_B] / src_b : 256*dest[LICE_PIXEL_B],
        src_a > 1 ? 256*dest[LICE_PIXEL_A] / src_a : 256*dest[LICE_PIXEL_A]);
  }
};

#else // !LICE_DISABLE_BLEND_DODGE
#define _LICE_CombinePixelsColorDodgeSourceAlpha _LICE_CombinePixelsCopySourceAlphaClamp
#define _LICE_CombinePixelsColorDodge _LICE_CombinePixelsCopyClamp
#endif


#ifndef LICE_DISABLE_BLEND_MUL

class _LICE_CombinePixelsMulNoClamp
{
public:
  static inline void doPix(LICE_pixel_chan *dest, int r, int g, int b, int a, int alpha)
  { 
    // we could check alpha=0 here, but the caller should (since alpha is usually used for static alphas)

    const int da=(256-alpha)*256;
    _LICE_MakePixelNoClamp(dest,
      (dest[LICE_PIXEL_R]*(da + (r*alpha)))>>16,
      (dest[LICE_PIXEL_G]*(da + (g*alpha)))>>16,
      (dest[LICE_PIXEL_B]*(da + (b*alpha)))>>16,
      (dest[LICE_PIXEL_A]*(da + (a*alpha)))>>16);

  }
};
class _LICE_CombinePixelsMulClamp
{
public:
  static inline void doPix(LICE_pixel_chan *dest, int r, int g, int b, int a, int alpha)
  { 
    // we could check alpha=0 here, but the caller should (since alpha is usually used for static alphas)

    const int da=(256-alpha)*256;
    _LICE_MakePixelClamp(dest,
      (dest[LICE_PIXEL_R]*(da + (r*alpha)))>>16,
      (dest[LICE_PIXEL_G]*(da + (g*alpha)))>>16,
      (dest[LICE_PIXEL_B]*(da + (b*alpha)))>>16,
      (dest[LICE_PIXEL_A]*(da + (a*alpha)))>>16);

  }
};
class _LICE_CombinePixelsMulSourceAlphaNoClamp
{
public:
  static inline void doPix(LICE_pixel_chan *dest, int r, int g, int b, int a, int alpha)
  { 
    if (a)
    {
      const int ualpha=(alpha*(a+1))/256;
      const int da=(256-ualpha)*256;
      _LICE_MakePixelNoClamp(dest,
        (dest[LICE_PIXEL_R]*(da + (r*ualpha)))>>16,
        (dest[LICE_PIXEL_G]*(da + (g*ualpha)))>>16,
        (dest[LICE_PIXEL_B]*(da + (b*ualpha)))>>16,
        (dest[LICE_PIXEL_A]*(da + (a*ualpha)))>>16);

    }
  }
};
class _LICE_CombinePixelsMulSourceAlphaClamp
{
public:
  static inline void doPix(LICE_pixel_chan *dest, int r, int g, int b, int a, int alpha)
  { 
    if (a)
    {
      const int ualpha=(alpha*(a+1))/256;
      const int da=(256-ualpha)*256;
      _LICE_MakePixelClamp(dest,
        (dest[LICE_PIXEL_R]*(da + (r*ualpha)))>>16,
        (dest[LICE_PIXEL_G]*(da + (g*ualpha)))>>16,
        (dest[LICE_PIXEL_B]*(da + (b*ualpha)))>>16,
        (dest[LICE_PIXEL_A]*(da + (a*ualpha)))>>16);

    }
  }
};

#else // !LICE_DISABLE_BLEND_MUL
#define _LICE_CombinePixelsMulSourceAlphaNoClamp _LICE_CombinePixelsCopySourceAlphaNoClamp
#define _LICE_CombinePixelsMulSourceAlphaClamp _LICE_CombinePixelsCopySourceAlphaClamp
#define _LICE_CombinePixelsMulNoClamp _LICE_CombinePixelsCopyNoClamp
#define _LICE_CombinePixelsMulClamp _LICE_CombinePixelsCopyClamp
#endif

//#define LICE_DISABLE_BLEND_OVERLAY
#ifndef LICE_DISABLE_BLEND_OVERLAY

class _LICE_CombinePixelsOverlay
{
public:
  static inline void doPix(LICE_pixel_chan *dest, int r, int g, int b, int a, int alpha)
  { 
    // we could check alpha=0 here, but the caller should (since alpha is usually used for static alphas)

    int destr = dest[LICE_PIXEL_R], destg = dest[LICE_PIXEL_G], destb = dest[LICE_PIXEL_B], desta = dest[LICE_PIXEL_A];

#if 0
    int srcr = r*alpha, srcg = g*alpha, srcb = b*alpha, srca = a*alpha;
    int da=(256-alpha)*256;
    int mr = (destr*(da+srcr))/65536;
    int mg = (destg*(da+srcg))/65536;
    int mb = (destb*(da+srcb))/65536;
    int ma = (desta*(da+srca))/65536;
    int sr = 256-(65536-srcr)*(256-destr)/65536;
    int sg = 256-(65536-srcg)*(256-destg)/65536;
    int sb = 256-(65536-srcb)*(256-destb)/65536;
    int sa = 256-(65536-srca)*(256-desta)/65536;

    destr = (destr*sr+(256-destr)*mr)/256;
    destg = (destg*sg+(256-destg)*mg)/256;
    destb = (destb*sb+(256-destb)*mb)/256;
    desta = (desta*sa+(256-desta)*ma)/256;
#else 
    // can produce slightly diff (+-1) results from above due to rounding
    const int da=(256-alpha)*128;
    const int srcr = r*alpha+da, srcg = g*alpha+da, srcb = b*alpha+da, srca = a*alpha + da;
    destr = ( destr*( (destr*(32768-srcr))/256 + srcr ) ) >> 15;
    destg = ( destg*( (destg*(32768-srcg))/256 + srcg ) ) >> 15;
    destb = ( destb*( (destb*(32768-srcb))/256 + srcb ) ) >> 15;
    desta = ( desta*( (desta*(32768-srca))/256 + srca ) ) >> 15;

#endif

    _LICE_MakePixelClamp(dest, destr, destg, destb, desta);
  }
};

class _LICE_CombinePixelsOverlaySourceAlpha
{
public:
  static inline void doPix(LICE_pixel_chan *dest, int r, int g, int b, int a, int alpha)
  { 
    _LICE_CombinePixelsOverlay::doPix(dest, r, g, b, a, (alpha*(a+1))/256);
  }
};

#else // !LICE_DISABLE_BLEND_OVERLAY
#define _LICE_CombinePixelsOverlaySourceAlpha _LICE_CombinePixelsCopySourceAlphaClamp
#define _LICE_CombinePixelsOverlay _LICE_CombinePixelsCopyClamp
#endif


//#define LICE_DISABLE_BLEND_HSVADJ
#ifndef LICE_DISABLE_BLEND_HSVADJ

class _LICE_CombinePixelsHSVAdjust
{
public:
  static inline void doPix(LICE_pixel_chan *dest, int r, int g, int b, int a, int alpha)
  { 
    int h,s,v;
    __LICE_RGB2HSV(dest[LICE_PIXEL_R],dest[LICE_PIXEL_G],dest[LICE_PIXEL_B],&h,&s,&v);
    h+=(((r+r/2) - 192) * alpha)/256;
    if (h<0)h+=384;
    else if (h>=384) h-=384;
    s+=((g-128)*alpha)/128;
    if (s&~0xff)
    {
      if (s<0)s=0;
      else s=255;
    }
    v+=((b-128)*alpha)/128;
    if (v&~0xff)
    {
      if (v<0)v=0;
      else v=255;
    }

    *(LICE_pixel *)dest = __LICE_HSV2Pix(h,s,v,a);
  }
};

class _LICE_CombinePixelsHSVAdjustSourceAlpha
{
public:
  static inline void doPix(LICE_pixel_chan *dest, int r, int g, int b, int a, int alpha)
  { 
    _LICE_CombinePixelsHSVAdjust::doPix(dest, r, g, b, a, (alpha*(a+1))/256);
  }
};

#else // !LICE_DISABLE_BLEND_HSVADJ
#define _LICE_CombinePixelsHSVAdjustSourceAlpha _LICE_CombinePixelsCopySourceAlphaClamp
#define _LICE_CombinePixelsHSVAdjust _LICE_CombinePixelsCopyClamp
#endif

// note: the "clamp" parameter would generally be false, unless you're working with
// input colors that need to be clamped (i.e. if you have a r value of >255 or <0, etc.
// if your input is LICE_pixel only then use false, and it will clamp as needed depending 
// on the blend mode.. 

//#define __LICE__ACTION(comb) templateclass<comb>::function(parameters)
//__LICE_ACTION_SRCALPHA(mode,alpha,clamp);
//#undef __LICE__ACTION


// use this for paths that support LICE_BLIT_USE_ALPHA (source-alpha combining), but
// otherwise have constant alpha
#define __LICE_ACTION_SRCALPHA(mode,ia,clamp) \
     if ((ia)!=0) switch ((mode)&(LICE_BLIT_MODE_MASK|LICE_BLIT_USE_ALPHA)) { \
      case LICE_BLIT_MODE_COPY: if ((ia)>0) { \
        if (clamp) { \
          if ((ia)==256) { __LICE__ACTION(_LICE_CombinePixelsClobberClamp); } \
          else { __LICE__ACTION(_LICE_CombinePixelsCopyClamp); }  \
        } else { \
          if ((ia)==256) { __LICE__ACTION(_LICE_CombinePixelsClobberNoClamp); } \
          else { __LICE__ACTION(_LICE_CombinePixelsCopyNoClamp); } \
        } \
      }  \
      break;  \
      case LICE_BLIT_MODE_ADD: __LICE__ACTION(_LICE_CombinePixelsAdd); break;  \
      case LICE_BLIT_MODE_DODGE: __LICE__ACTION(_LICE_CombinePixelsColorDodge);  break;  \
      case LICE_BLIT_MODE_MUL: \
        if (clamp) { __LICE__ACTION(_LICE_CombinePixelsMulClamp); } \
        else { __LICE__ACTION(_LICE_CombinePixelsMulNoClamp); } \
      break;  \
      case LICE_BLIT_MODE_OVERLAY: __LICE__ACTION(_LICE_CombinePixelsOverlay); break;  \
      case LICE_BLIT_MODE_HSVADJ: __LICE__ACTION(_LICE_CombinePixelsHSVAdjust); break;  \
      case LICE_BLIT_MODE_COPY|LICE_BLIT_USE_ALPHA: \
        if (clamp) { \
          if ((ia)==256) { __LICE__ACTION(_LICE_CombinePixelsCopySourceAlphaIgnoreAlphaParmClamp);} \
          else { __LICE__ACTION(_LICE_CombinePixelsCopySourceAlphaClamp); } \
        } else { \
          if ((ia)==256) { __LICE__ACTION(_LICE_CombinePixelsCopySourceAlphaIgnoreAlphaParmNoClamp); } \
          else { __LICE__ACTION(_LICE_CombinePixelsCopySourceAlphaNoClamp); } \
        } \
      break;  \
      case LICE_BLIT_MODE_ADD|LICE_BLIT_USE_ALPHA:  \
          __LICE__ACTION(_LICE_CombinePixelsAddSourceAlpha);  \
      break;  \
      case LICE_BLIT_MODE_DODGE|LICE_BLIT_USE_ALPHA: \
          __LICE__ACTION(_LICE_CombinePixelsColorDodgeSourceAlpha); \
      break;  \
      case LICE_BLIT_MODE_MUL|LICE_BLIT_USE_ALPHA: \
        if (clamp) { __LICE__ACTION(_LICE_CombinePixelsMulSourceAlphaClamp); } \
        else { __LICE__ACTION(_LICE_CombinePixelsMulSourceAlphaNoClamp); } \
      break;  \
      case LICE_BLIT_MODE_OVERLAY|LICE_BLIT_USE_ALPHA: \
          __LICE__ACTION(_LICE_CombinePixelsOverlaySourceAlpha); \
      break;  \
      case LICE_BLIT_MODE_HSVADJ|LICE_BLIT_USE_ALPHA: \
          __LICE__ACTION(_LICE_CombinePixelsHSVAdjustSourceAlpha); \
      break;  \
     }


// use this for paths that can have per pixel alpha, but calculate it themselves
#define __LICE_ACTION_NOSRCALPHA(mode, ia,clamp) \
     if ((ia)!=0) switch ((mode)&LICE_BLIT_MODE_MASK) { \
      case LICE_BLIT_MODE_COPY: if ((ia)>0) { if (clamp) { __LICE__ACTION(_LICE_CombinePixelsCopyClamp); } else { __LICE__ACTION(_LICE_CombinePixelsCopyNoClamp); } } break;  \
      case LICE_BLIT_MODE_ADD: __LICE__ACTION(_LICE_CombinePixelsAdd); break;  \
      case LICE_BLIT_MODE_DODGE: __LICE__ACTION(_LICE_CombinePixelsColorDodge);  break;  \
      case LICE_BLIT_MODE_MUL: if (clamp) { __LICE__ACTION(_LICE_CombinePixelsMulClamp); } else { __LICE__ACTION(_LICE_CombinePixelsMulNoClamp); } break; \
      case LICE_BLIT_MODE_OVERLAY: __LICE__ACTION(_LICE_CombinePixelsOverlay); break;  \
      case LICE_BLIT_MODE_HSVADJ: __LICE__ACTION(_LICE_CombinePixelsHSVAdjust); break;  \
    }

// For drawing where there is constant alpha and no per-pixel alpha.
#define __LICE_ACTION_CONSTANTALPHA(mode,ia,clamp) \
    if ((ia)!=0) switch ((mode)&LICE_BLIT_MODE_MASK) { \
      case LICE_BLIT_MODE_COPY: \
        if ((ia)==256) { if (clamp) { __LICE__ACTION(_LICE_CombinePixelsClobberClamp); } else { __LICE__ACTION(_LICE_CombinePixelsClobberNoClamp); } } \
        else if ((ia)==128) { if (clamp) { __LICE__ACTION(_LICE_CombinePixelsHalfMixClamp); } else { __LICE__ACTION(_LICE_CombinePixelsHalfMixNoClamp); }  } \
        else if ((ia)>0) { if (clamp) { __LICE__ACTION(_LICE_CombinePixelsCopyClamp); } else { __LICE__ACTION(_LICE_CombinePixelsCopyNoClamp); } } \
      break;  \
      case LICE_BLIT_MODE_ADD: __LICE__ACTION(_LICE_CombinePixelsAdd); break;  \
      case LICE_BLIT_MODE_DODGE: __LICE__ACTION(_LICE_CombinePixelsColorDodge);  break;  \
      case LICE_BLIT_MODE_MUL: if (clamp) { __LICE__ACTION(_LICE_CombinePixelsMulClamp); } else { __LICE__ACTION(_LICE_CombinePixelsMulNoClamp); } break;  \
      case LICE_BLIT_MODE_OVERLAY: __LICE__ACTION(_LICE_CombinePixelsOverlay); break;  \
      case LICE_BLIT_MODE_HSVADJ: __LICE__ACTION(_LICE_CombinePixelsHSVAdjust); break;  \
    }
     
typedef void (*LICE_COMBINEFUNC)(LICE_pixel_chan *dest, int r, int g, int b, int a, int alpha);
   
#define __LICE_SC(x) do { (x) = ((x)*(__sc))/256; } while (0)
#define __LICE_SCU(x) do { (x) = ((x)*(__sc))>>8; } while (0)

#endif // _LICE_COMBINE_H_
