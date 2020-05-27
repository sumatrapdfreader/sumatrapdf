#ifndef _LICE_H
#define _LICE_H

/*
  Cockos WDL - LICE - Lightweight Image Compositing Engine

  Copyright (C) 2007 and later, Cockos Incorporated
  Portions Copyright (C) 2007 "schwa"

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

#ifdef _WIN32
#include <windows.h>
#else
#include "../swell/swell-types.h" // use SWELL on other systems
#endif


// one of these can be defined in your project if you choose:
//#define LICE_FAVOR_SPEED // optimizes some stuff that doesnt seem to benefit much (like LICE_DeltaBlit/LICE_RotatedBlit/LICE_TransformBlit)
// (nothing) default probably good overall
//#define LICE_FAVOR_SIZE // reduces code size of normal/scaled blit functions
//#define LICE_FAVOR_SIZE_EXTREME // same as LICE_FAVOR_SIZE w/ smaller gains with bigger perf penalties (solid fills etc)

#ifdef LICE_FAVOR_SPEED
  #ifdef LICE_FAVOR_SIZE_EXTREME
    #undef LICE_FAVOR_SIZE_EXTREME
  #endif
  #ifdef LICE_FAVOR_SIZE
    #undef LICE_FAVOR_SIZE
  #endif
#endif
#if defined(LICE_FAVOR_SIZE_EXTREME) && !defined(LICE_FAVOR_SIZE)
#define LICE_FAVOR_SIZE
#endif


typedef unsigned int LICE_pixel;
typedef unsigned char LICE_pixel_chan;

#define LICE_RGBA(r,g,b,a) (((b)&0xff)|(((g)&0xff)<<8)|(((r)&0xff)<<16)|(((a)&0xff)<<24))
#define LICE_GETB(v) ((v)&0xff)
#define LICE_GETG(v) (((v)>>8)&0xff)
#define LICE_GETR(v) (((v)>>16)&0xff)
#define LICE_GETA(v) (((v)>>24)&0xff)

#if defined(__APPLE__) && defined(__ppc__)
#define LICE_PIXEL_A 0
#define LICE_PIXEL_R 1
#define LICE_PIXEL_G 2
#define LICE_PIXEL_B 3
#else
#define LICE_PIXEL_B 0
#define LICE_PIXEL_G 1
#define LICE_PIXEL_R 2
#define LICE_PIXEL_A 3
#endif


static inline LICE_pixel LICE_RGBA_FROMNATIVE(LICE_pixel col, int alpha=0)
{
  return LICE_RGBA(GetRValue(col),GetGValue(col),GetBValue(col),alpha);
}

// bitmap interface, and some built-in types (memory bitmap and system bitmap)

class LICE_IBitmap
{
public:
  virtual ~LICE_IBitmap() { }

  virtual LICE_pixel *getBits()=0;
  virtual int getWidth()=0;
  virtual int getHeight()=0;
  virtual int getRowSpan()=0; // includes any off-bitmap data
  virtual bool isFlipped() { return false;  }
  virtual bool resize(int w, int h)=0;

  virtual HDC getDC() { return 0; } // only sysbitmaps have to implement this


  virtual INT_PTR Extended(int id, void* data) { return 0; }  
};

#define LICE_EXT_SET_SCALING 0x2000 // data = int *, scaling is .8 fixed point. returns true if supported. affects LICE_*() draw operations
#define LICE_EXT_GET_SCALING 0x2001 // data ignored, returns .8 fixed point, returns 0 if unscaled
#define LICE_EXT_SET_ADVISORY_SCALING 0x2002 // data = int *, scaling is .8 fixed point. returns true if supported. does not affect draw operations
#define LICE_EXT_GET_ADVISORY_SCALING 0x2003 // data ignored, returns .8 fixed point. returns 0 if unscaled
#define LICE_EXT_GET_ANY_SCALING 0x2004 // data ignored, returns .8 fixed point, 0 if unscaled

#define LICE_MEMBITMAP_ALIGNAMT 63

class LICE_MemBitmap : public LICE_IBitmap
{
public:
  LICE_MemBitmap(int w=0, int h=0, unsigned int linealign=4);
  virtual ~LICE_MemBitmap();


  // LICE_IBitmap interface
  virtual LICE_pixel *getBits() 
  { 
    const UINT_PTR extra=LICE_MEMBITMAP_ALIGNAMT;
    return (LICE_pixel *) (((UINT_PTR)m_fb + extra)&~extra);
  }
  virtual int getWidth() { return m_width; }
  virtual int getHeight() { return m_height; }
  virtual int getRowSpan() { return (m_width+m_linealign)&~m_linealign; }
  virtual bool resize(int w, int h) { return __resize(w,h); } // returns TRUE if a resize occurred

  // todo: LICE_EXT_SET_SCALING ?

private:
  bool __resize(int w, int h);
  LICE_pixel *m_fb;
  int m_width, m_height;
  int m_allocsize;
  unsigned int m_linealign;
};

class LICE_SysBitmap : public LICE_IBitmap
{
public:
  LICE_SysBitmap(int w=0, int h=0);
  virtual ~LICE_SysBitmap();
  
  // LICE_IBitmap interface
  virtual LICE_pixel *getBits() { return m_bits; }
  virtual int getWidth() { return m_width; }
  virtual int getHeight() { return m_height; }
  virtual int getRowSpan() { return m_allocw; }; 
  virtual bool resize(int w, int h) { return __resize(w,h); } // returns TRUE if a resize occurred

  virtual INT_PTR Extended(int id, void* data)
  {
    switch (id)
    {
      case LICE_EXT_SET_ADVISORY_SCALING: 
        {
          int sc = data && *(int*)data != 256 ? *(int *)data : 0; 
          if (sc < 0) sc = 0;
          m_adv_scaling = sc;
        }
      return 1;
      case LICE_EXT_SET_SCALING: 
        {
          int sc = data && *(int*)data != 256 ? *(int *)data : 0; 
          if (sc < 0) sc = 0;
          if (m_draw_scaling != sc)
          {
            const int tmp=m_width;
            m_draw_scaling = sc;
            m_width=0;
            resize(tmp,m_height);
          }
        }
      return 1;
      case LICE_EXT_GET_SCALING: 
      return m_draw_scaling;
      case LICE_EXT_GET_ADVISORY_SCALING: 
      return m_adv_scaling;
      case LICE_EXT_GET_ANY_SCALING:
        if (m_draw_scaling > 0) 
        {
          if (m_adv_scaling > 0)
            return (m_adv_scaling * m_draw_scaling) >> 8;
          return m_draw_scaling;
        }
        return m_adv_scaling;
    }
    return 0;
  }

  // sysbitmap specific calls
  virtual HDC getDC() { return m_dc; }


private:
  bool __resize(int w, int h);
  int m_width, m_height;

  HDC m_dc;
  LICE_pixel *m_bits;
  int m_allocw, m_alloch;
#ifdef _WIN32
  HBITMAP m_bitmap;
  HGDIOBJ m_oldbitmap;
#endif
  int m_draw_scaling, m_adv_scaling;
};

class LICE_WrapperBitmap : public LICE_IBitmap 
{
  public:
    LICE_WrapperBitmap(LICE_pixel *buf, int w, int h, int span, bool flipped)
    {
      m_buf=buf;
      m_w=w;
      m_h=h;
      m_span=span;
      m_flipped=flipped;
    }
    virtual ~LICE_WrapperBitmap() {}

    virtual bool resize(int w, int h) { return false; }
    virtual LICE_pixel *getBits() { return m_buf; }
    virtual int getWidth() { return m_w; }
    virtual int getHeight() { return m_h; }
    virtual int getRowSpan() { return m_span; }

    virtual HDC getDC() { return NULL; }
    virtual bool isFlipped() { return m_flipped; }


    LICE_pixel *m_buf;
    int m_w,m_h,m_span;
    bool m_flipped;
};


class LICE_SubBitmap : public LICE_IBitmap // note: you should only keep these around as long as they are needed, and don't resize the parent while this is allocated
{
  public:
    LICE_SubBitmap(LICE_IBitmap *parent, int x, int y, int w, int h)
    {
      m_parent=parent;
      if(x<0)x=0; 
      if(y<0)y=0;
      m_x=x;m_y=y;
      __resize(w,h);
    }
    virtual ~LICE_SubBitmap() { }

    virtual bool resize(int w, int h) { return __resize(w,h); }

    bool __resize(int w, int h)
    {
      m_w=0;m_h=0;
      if (m_parent && m_x >= 0 && m_y >= 0 && m_x < m_parent->getWidth() && m_y < m_parent->getHeight())
      {
        if (w > m_parent->getWidth()-m_x) w=m_parent->getWidth()-m_x;
        if (h > m_parent->getHeight()-m_y) h=m_parent->getHeight()-m_y;

        m_w=w; 
        m_h=h;
      }

      return true;
    }

    virtual bool isFlipped() { return m_parent && m_parent->isFlipped();  }

    virtual LICE_pixel *getBits() 
    {
      if (!m_parent) return 0;

      int xc = m_x, yc = m_y, h = m_h;
      const int scale = (int)m_parent->Extended(LICE_EXT_GET_SCALING,NULL);
      if (scale > 0)
      {
        xc = (xc*scale)>>8;
        yc = (yc*scale)>>8;
        h = (h*scale)>>8;
      }

      LICE_pixel* parentptr=m_parent->getBits();
      if (m_parent->isFlipped()) parentptr += (m_parent->getHeight() - (yc+h))*m_parent->getRowSpan()+xc;
      else parentptr += yc*m_parent->getRowSpan()+xc;

      return parentptr; 
    }

    enum { 
        LICE_GET_SUBBITMAP_VERSION = 0x51b7000, 
        LICE_SUBBITMAP_VERSION = 0x1000  // if we change any of this struct, then we *must* increment this version.
    }; 

    virtual INT_PTR Extended(int id, void* data)
    {
      if (id == LICE_GET_SUBBITMAP_VERSION) return LICE_SUBBITMAP_VERSION;

      if (!m_parent) return 0;
      return m_parent->Extended(id, data);
    }
      
    virtual int getWidth() { return m_w; }
    virtual int getHeight() { return m_h; }
    virtual int getRowSpan() { return m_parent ? m_parent->getRowSpan() : 0; }

    virtual HDC getDC() { return NULL; }

    int m_w,m_h,m_x,m_y;
    LICE_IBitmap *m_parent;
};


// flags that most blit functions can take

#define LICE_BLIT_MODE_MASK 0xff
#define LICE_BLIT_MODE_COPY 0
#define LICE_BLIT_MODE_ADD 1
#define LICE_BLIT_MODE_DODGE 2
#define LICE_BLIT_MODE_MUL 3
#define LICE_BLIT_MODE_OVERLAY 4
#define LICE_BLIT_MODE_HSVADJ 5

#define LICE_BLIT_MODE_CHANCOPY 0xf0 // in this mode, only available for LICE_Blit(), the low nibble is 2 bits of source channel (low 2), 2 bits of dest channel (high 2)

#define LICE_BLIT_FILTER_MASK 0xff00
#define LICE_BLIT_FILTER_NONE 0
#define LICE_BLIT_FILTER_BILINEAR 0x100 // currently pretty slow! ack
#define LICE_BLIT_IGNORE_SCALING 0x20000


#define LICE_BLIT_USE_ALPHA 0x10000 // use source's alpha channel

#ifndef lice_max
#define lice_max(x,y) ((x)<(y)?(y):(x))
#define lice_min(x,y) ((x)<(y)?(x):(y))
#endif

#ifdef _MSC_VER
  #include <float.h>
  #define lice_isfinite(x) _finite(x)
#else
  #define lice_isfinite(x) isfinite(x)
#endif

// Reaper exports most LICE functions, so the function declarations below
// will collide with reaper_plugin.h
#ifndef LICE_PROVIDED_BY_APP


// bitmap loaders

// dispatch to a linked loader implementation based on file extension 
LICE_IBitmap* LICE_LoadImage(const char* filename, LICE_IBitmap* bmp=NULL, bool tryIgnoreExtension=false);
char *LICE_GetImageExtensionList(bool wantAllSup=true, bool wantAllFiles=true); // returns doublenull terminated GetOpenFileName() style list -- free() when done.
bool LICE_ImageIsSupported(const char *filename);  // must be a filename that ends in .jpg, etc. if you want to check the extension, pass .ext


// pass a bmp if you wish to load it into that bitmap. note that if it fails bmp will not be deleted.
LICE_IBitmap *LICE_LoadPNG(const char *filename, LICE_IBitmap *bmp=NULL); // returns a bitmap (bmp if nonzero) on success
LICE_IBitmap *LICE_LoadPNGFromMemory(const void *data_in, int buflen, LICE_IBitmap *bmp=NULL);
LICE_IBitmap *LICE_LoadPNGFromResource(HINSTANCE hInst, const char *resid, LICE_IBitmap *bmp=NULL); // returns a bitmap (bmp if nonzero) on success
#ifndef _WIN32
LICE_IBitmap *LICE_LoadPNGFromNamedResource(const char *name, LICE_IBitmap *bmp=NULL); // returns a bitmap (bmp if nonzero) on success
#endif

LICE_IBitmap *LICE_LoadBMP(const char *filename, LICE_IBitmap *bmp=NULL); // returns a bitmap (bmp if nonzero) on success
LICE_IBitmap *LICE_LoadBMPFromResource(HINSTANCE hInst, const char *resid, LICE_IBitmap *bmp=NULL); // returns a bitmap (bmp if nonzero) on success

LICE_IBitmap *LICE_LoadIcon(const char *filename, int reqiconsz=16, LICE_IBitmap *bmp=NULL); // returns a bitmap (bmp if nonzero) on success
LICE_IBitmap *LICE_LoadIconFromResource(HINSTANCE hInst, const char *resid, int reqiconsz=16, LICE_IBitmap *bmp=NULL); // returns a bitmap (bmp if nonzero) on success

LICE_IBitmap *LICE_LoadJPG(const char *filename, LICE_IBitmap *bmp=NULL);
LICE_IBitmap *LICE_LoadJPGFromMemory(const void *data_in, int buflen, LICE_IBitmap *bmp = NULL);
LICE_IBitmap* LICE_LoadJPGFromResource(HINSTANCE hInst, const char *resid, LICE_IBitmap* bmp = 0);

LICE_IBitmap *LICE_LoadGIF(const char *filename, LICE_IBitmap *bmp=NULL, int *nframes=NULL); // if nframes set, will be set to number of images (stacked vertically), otherwise first frame used

LICE_IBitmap *LICE_LoadPCX(const char *filename, LICE_IBitmap *bmp=NULL); // returns a bitmap (bmp if nonzero) on success

LICE_IBitmap *LICE_LoadSVG(const char *filename, LICE_IBitmap *bmp=NULL);

// bitmap saving
bool LICE_WritePNG(const char *filename, LICE_IBitmap *bmp, bool wantalpha=true);
bool LICE_WriteJPG(const char *filename, LICE_IBitmap *bmp, int quality=95, bool force_baseline=true);
bool LICE_WriteGIF(const char *filename, LICE_IBitmap *bmp, int transparent_alpha=0, bool dither=true); // if alpha<transparent_alpha then transparent. if transparent_alpha<0, then intra-frame checking is used

// animated GIF API. use transparent_alpha=-1 to encode unchanged pixels as transparent
void *LICE_WriteGIFBegin(const char *filename, LICE_IBitmap *firstframe, int transparent_alpha=0, int frame_delay=0, bool dither=true, int nreps=0); // nreps=0 for infinite
void *LICE_WriteGIFBeginNoFrame(const char *filename, int w, int h, int transparent_alpha=0, bool dither=true, bool is_append=false);
bool LICE_WriteGIFFrame(void *handle, LICE_IBitmap *frame, int xpos, int ypos, bool perImageColorMap=false, int frame_delay=0, int nreps=0); // nreps only used on the first frame, 0=infinite
unsigned int LICE_WriteGIFGetSize(void *handle); // gets current output size
bool LICE_WriteGIFEnd(void *handle);
int LICE_SetGIFColorMapFromOctree(void *wr, void *octree, int numcolors); // can use after LICE_WriteGIFBeginNoFrame and before LICE_WriteGIFFrame

// animated GIF reading
void *LICE_GIF_LoadEx(const char *filename);
void LICE_GIF_Close(void *handle);
void LICE_GIF_Rewind(void *handle);
unsigned int LICE_GIF_GetFilePos(void *handle); // gets current read position
int LICE_GIF_UpdateFrame(void *handle, LICE_IBitmap *bm); // returns duration in msec (0 or more), or <0 if no more frames. bm will be modified/resized with new frame data



// basic primitives
void LICE_PutPixel(LICE_IBitmap *bm, int x, int y, LICE_pixel color, float alpha, int mode);
LICE_pixel LICE_GetPixel(LICE_IBitmap *bm, int x, int y);

// blit functions

void LICE_Copy(LICE_IBitmap *dest, LICE_IBitmap *src); // resizes dest to fit


//alpha parameter = const alpha (combined with source alpha if spcified)
void LICE_Blit(LICE_IBitmap *dest, LICE_IBitmap *src, int dstx, int dsty, const RECT *srcrect, float alpha, int mode);
void LICE_Blit(LICE_IBitmap *dest, LICE_IBitmap *src, int dstx, int dsty, int srcx, int srcy, int srcw, int srch, float alpha, int mode);

void LICE_Blur(LICE_IBitmap *dest, LICE_IBitmap *src, int dstx, int dsty, int srcx, int srcy, int srcw, int srch);

// dstw/dsty can be negative, srcw/srch can be as well (for flipping)
void LICE_ScaledBlit(LICE_IBitmap *dest, LICE_IBitmap *src, int dstx, int dsty, int dstw, int dsth, 
                     float srcx, float srcy, float srcw, float srch, float alpha, int mode);


void LICE_HalveBlitAA(LICE_IBitmap *dest, LICE_IBitmap *src); // AA's src down to dest. uses the minimum size of both (use with LICE_SubBitmap to do sections)

// if cliptosourcerect is false, then areas outside the source rect can get in (otherwise they are not drawn)
void LICE_RotatedBlit(LICE_IBitmap *dest, LICE_IBitmap *src, 
                      int dstx, int dsty, int dstw, int dsth, 
                      float srcx, float srcy, float srcw, float srch, 
                      float angle, 
                      bool cliptosourcerect, float alpha, int mode,
                      float rotxcent=0.0, float rotycent=0.0); // these coordinates are offset from the center of the image, in source pixel coordinates


void LICE_TransformBlit(LICE_IBitmap *dest, LICE_IBitmap *src,  
                    int dstx, int dsty, int dstw, int dsth,
                    const float *srcpoints, int div_w, int div_h, // srcpoints coords should be div_w*div_h*2 long, and be in source image coordinates
                    float alpha, int mode);
void LICE_TransformBlit2(LICE_IBitmap *dest, LICE_IBitmap *src,  
                    int dstx, int dsty, int dstw, int dsth,
                    const double *srcpoints, int div_w, int div_h, // srcpoints coords should be div_w*div_h*2 long, and be in source image coordinates
                    float alpha, int mode);

void LICE_TransformBlit2Alpha(LICE_IBitmap *dest, LICE_IBitmap *src,  
                    int dstx, int dsty, int dstw, int dsth,
                    const double *srcpoints, int div_w, int div_h, // srcpoints coords should be div_w*div_h*3 long, and be in source image coordinates + alpha
                    int mode);

// if cliptosourcerect is false, then areas outside the source rect can get in (otherwise they are not drawn)
void LICE_DeltaBlit(LICE_IBitmap *dest, LICE_IBitmap *src, 
                    int dstx, int dsty, int dstw, int dsth,                     
                    float srcx, float srcy, float srcw, float srch, 
                    double dsdx, double dtdx, double dsdy, double dtdy,         
                    double dsdxdy, double dtdxdy,
                    bool cliptosourcerect, float alpha, int mode);

void LICE_DeltaBlitAlpha(LICE_IBitmap *dest, LICE_IBitmap *src, 
                    int dstx, int dsty, int dstw, int dsth,                     
                    float srcx, float srcy, float srcw, float srch, 
                    double dsdx, double dtdx, double dsdy, double dtdy,         
                    double dsdxdy, double dtdxdy,
                    bool cliptosourcerect, float alpha, int mode, double dadx, double dady, double dadxdy);


// only LICE_BLIT_MODE_ADD or LICE_BLIT_MODE_COPY are used by this, for flags
// ir-ia should be 0.0..1.0 (or outside that and they'll be clamped)
// drdx should be X/dstw, drdy X/dsth etc
void LICE_GradRect(LICE_IBitmap *dest, int dstx, int dsty, int dstw, int dsth, 
                      float ir, float ig, float ib, float ia,
                      float drdx, float dgdx, float dbdx, float dadx,
                      float drdy, float dgdy, float dbdy, float dady,
                      int mode);

void LICE_FillRect(LICE_IBitmap *dest, int x, int y, int w, int h, LICE_pixel color, float alpha = 1.0f, int mode = 0);
void LICE_ProcessRect(LICE_IBitmap *dest, int x, int y, int w, int h, void (*procFunc)(LICE_pixel *p, void *parm), void *parm);

void LICE_Clear(LICE_IBitmap *dest, LICE_pixel color);
void LICE_ClearRect(LICE_IBitmap *dest, int x, int y, int w, int h, LICE_pixel mask=0, LICE_pixel orbits=0);
void LICE_MultiplyAddRect(LICE_IBitmap *dest, int x, int y, int w, int h, 
                          float rsc, float gsc, float bsc, float asc, // 0-1, or -100 .. +100 if you really are insane
                          float radd, float gadd, float badd, float aadd); // 0-255 is the normal range on these.. of course its clamped

void LICE_SetAlphaFromColorMask(LICE_IBitmap *dest, LICE_pixel color);


// non-flood fill. simply scans up/down and left/right
void LICE_SimpleFill(LICE_IBitmap *dest, int x, int y, LICE_pixel newcolor,  
                     LICE_pixel comparemask=LICE_RGBA(255,255,255,0), 
                     LICE_pixel keepmask=LICE_RGBA(0,0,0,0));


// texture generators
void LICE_TexGen_Marble(LICE_IBitmap *dest, const RECT *rect, float rv, float gv, float bv, float intensity); //fills whole bitmap if rect == NULL

//this function generates a Perlin noise
//fills whole bitmap if rect == NULL
//smooth needs to be a multiple of 2
enum
{
  NOISE_MODE_NORMAL = 0,
  NOISE_MODE_WOOD,
};
void LICE_TexGen_Noise(LICE_IBitmap *dest, const RECT *rect, float rv, float gv, float bv, float intensity, int mode=NOISE_MODE_NORMAL, int smooth=1); 

//this function generates a Perlin noise in a circular fashion
//fills whole bitmap if rect == NULL
//size needs to be a multiple of 2
void LICE_TexGen_CircNoise(LICE_IBitmap *dest, const RECT *rect, float rv, float gv, float bv, float nrings, float power, int size);


// bitmapped text drawing:
void LICE_DrawChar(LICE_IBitmap *bm, int x, int y, char c, 
                   LICE_pixel color, float alpha, int mode);
void LICE_DrawText(LICE_IBitmap *bm, int x, int y, const char *string, 
                   LICE_pixel color, float alpha, int mode);
void LICE_MeasureText(const char *string, int *w, int *h);

// line drawing functions

void LICE_Line(LICE_IBitmap *dest, int x1, int y1, int x2, int y2, LICE_pixel color, float alpha=1.0f, int mode=0, bool aa=true);
void LICE_FLine(LICE_IBitmap* dest, float x1, float y1, float x2, float y2, LICE_pixel color, float alpha=1.0f, int mode=0, bool aa=true);
void LICE_ThickFLine(LICE_IBitmap* dest, double x1, double y1, double x2, double y2, LICE_pixel color, float alpha, int mode, int wid); // always AA. wid is not affected by scaling (1 is always normal line, 2 is always 2 physical pixels, etc)

void LICE_DashedLine(LICE_IBitmap* dest, int x1, int y1, int x2, int y2, int pxon, int pxoff, LICE_pixel color, float alpha=1.0f, int mode=0, bool aa=false); // straight lines only for now

void LICE_FillTrapezoidF(LICE_IBitmap* dest, double fx1a, double fx1b, int y1, double fx2a, double fx2b, int y2, LICE_pixel color, float alpha, int mode);
void LICE_FillTrapezoid(LICE_IBitmap* dest, int x1a, int x1b, int y1, int x2a, int x2b, int y2, LICE_pixel color, float alpha, int mode);
void LICE_FillConvexPolygon(LICE_IBitmap* dest, const int* x, const int* y, int npoints, LICE_pixel color, float alpha, int mode);

void LICE_FillTriangle(LICE_IBitmap *dest, int x1, int y1, int x2, int y2, int x3, int y3, LICE_pixel color, float alpha=1.0f, int mode=0);


// Returns false if the line is entirely offscreen.
bool LICE_ClipLine(int* pX1, int* pY1, int* pX2, int* pY2, int xLo, int yLo, int xHi, int yHi);
bool LICE_ClipFLine(float* px1, float* py1, float* px2, float* py2, float xlo, float ylo, float xhi, float yhi);

void LICE_Arc(LICE_IBitmap* dest, float cx, float cy, float r, float minAngle, float maxAngle, 
              LICE_pixel color, float alpha=1.0f, int mode=0, bool aa=true);
void LICE_Circle(LICE_IBitmap* dest, float cx, float cy, float r, LICE_pixel color, float alpha=1.0f, int mode=0, bool aa=true);
void LICE_FillCircle(LICE_IBitmap* dest, float cx, float cy, float r, LICE_pixel color, float alpha=1.0f, int mode=0, bool aa=true);
void LICE_RoundRect(LICE_IBitmap *drawbm, float xpos, float ypos, float w, float h, int cornerradius,
                    LICE_pixel col, float alpha, int mode, bool aa);

// useful for drawing shapes from a cache
void LICE_DrawGlyph(LICE_IBitmap* dest, int x, int y, LICE_pixel color, const LICE_pixel_chan* alphas, int glyph_w, int glyph_h, float alpha=1.0f, int mode = 0);
void LICE_DrawGlyphEx(LICE_IBitmap* dest, int x, int y, LICE_pixel color, const LICE_pixel_chan* alphas, int glyph_w, int glyph_span, int glyph_h, float alpha=1.0f, int mode = 0);

void LICE_DrawMonoGlyph(LICE_IBitmap* dest, int x, int y, LICE_pixel color, const unsigned char* alphas, int glyph_w, int glyph_span, int glyph_h, float alpha=1.0f, int mode = 0);

// quadratic bezier
// tol means try to draw segments no longer than tol px
void LICE_DrawQBezier(LICE_IBitmap* dest, double xstart, double ystart, double xctl, double yctl, double xend, double yend, 
  LICE_pixel color, float alpha=1.0f, int mode=0, bool aa=true, double tol=0.0); 

// cubic bezier
// tol means try to draw segments no longer than tol px
void LICE_DrawCBezier(LICE_IBitmap* dest, double xstart, double ystart, double xctl1, double yctl1,
  double xctl2, double yctl2, double xend, double yend, LICE_pixel color, float alpha=1.0f, int mode=0, bool aa=true, double tol=0.0); 

void LICE_DrawThickCBezier(LICE_IBitmap* dest, double xstart, double ystart, double xctl1, double yctl1,
  double xctl2, double yctl2, double xend, double yend, LICE_pixel color, float alpha=1.0f, int mode=0, int wid=2, double tol=0.0);

// vertical fill from y=yfill
void LICE_FillCBezier(LICE_IBitmap* dest, double xstart, double ystart, double xctl1, double yctl1,
  double xctl2, double yctl2, double xend, double yend, int yfill, LICE_pixel color, float alpha=1.0f, int mode=0, double tol=0.0);
// horizontal fill from x=xfill
void LICE_FillCBezierX(LICE_IBitmap* dest, double xstart, double ystart, double xctl1, double yctl1,
  double xctl2, double yctl2, double xend, double yend, int xfill, LICE_pixel color, float alpha=1.0f, int mode=0, double tol=0.0); 

// convenience functions
void LICE_DrawRect(LICE_IBitmap *dest, int x, int y, int w, int h, LICE_pixel color, float alpha=1.0f, int mode=0);
void LICE_BorderedRect(LICE_IBitmap *dest, int x, int y, int w, int h, LICE_pixel bgcolor, LICE_pixel fgcolor, float alpha=1.0f, int mode=0);

// bitmap compare-by-value function
int LICE_BitmapCmp(LICE_IBitmap* a, LICE_IBitmap* b, int *coordsOut=NULL);
int LICE_BitmapCmpEx(LICE_IBitmap* a, LICE_IBitmap* b, LICE_pixel mask, int *coordsOut=NULL);

// colorspace functions
void LICE_RGB2HSV(int r, int g, int b, int* h, int* s, int* v); // rgb, sv: [0,256), h: [0,384)
void LICE_HSV2RGB(int h, int s, int v, int* r, int* g, int* b); // rgb, sv: [0,256), h: [0,384)
LICE_pixel LICE_HSV2Pix(int h, int s, int v, int alpha); // sv: [0,256), h: [0,384)

LICE_pixel LICE_AlterColorHSV(LICE_pixel color, float d_hue, float d_saturation, float d_value);  // hue is rolled over, saturation and value are clamped, all 0..1
void LICE_AlterBitmapHSV(LICE_IBitmap* src, float d_hue, float d_saturation, float d_value);  // hue is rolled over, saturation and value are clamped, all 0..1
void LICE_AlterRectHSV(LICE_IBitmap* src, int x, int y, int w, int h, float d_hue, float d_saturation, float d_value, int mode=0);  // hue is rolled over, saturation and value are clamped, all 0..1. mode only used for scaling disable

LICE_pixel LICE_CombinePixels(LICE_pixel dest, LICE_pixel src, float alpha, int mode);

void LICE_CombinePixels2(LICE_pixel *destptr, int r, int g, int b, int a, int ia, int mode); // does not clamp
void LICE_CombinePixels2Clamp(LICE_pixel *destptr, int r, int g, int b, int a, int ia, int mode);

//// LVG

class ProjectStateContext;
void *LICE_LoadLVG(const char *filename);
void *LICE_LoadLVGFromContext(ProjectStateContext *ctx, const char *nameInfo=NULL, int defw=0, int defh=0);
void *LICE_GetSubLVG(void *lvg, const char *subname);
LICE_IBitmap *LICE_RenderLVG(void *lvg, int reqw=0, int reqh=0, LICE_IBitmap *useBM=NULL);
void LICE_DestroyLVG(void *lvg);

/// palette

void* LICE_CreateOctree(int maxcolors);
void LICE_DestroyOctree(void* octree);
void LICE_ResetOctree(void *octree, int maxcolors); // resets back to stock, but with spares (to avoid mallocs)
int LICE_BuildOctree(void* octree, LICE_IBitmap* bmp);
int LICE_BuildOctreeForAlpha(void* octree, LICE_IBitmap* bmp, unsigned int minalpha);
int LICE_BuildOctreeForDiff(void* octree, LICE_IBitmap* bmp, LICE_IBitmap* refbmp, LICE_pixel mask=LICE_RGBA(255,255,255,0));
int LICE_FindInOctree(void* octree, LICE_pixel color);
int LICE_ExtractOctreePalette(void* octree, LICE_pixel* palette);

// wrapper
int LICE_BuildPalette(LICE_IBitmap* bmp, LICE_pixel* palette, int maxcolors);
void LICE_TestPalette(LICE_IBitmap* bmp, LICE_pixel* palette, int numcolors);


struct _LICE_ImageLoader_rec
{
  LICE_IBitmap *(*loadfunc)(const char *filename, bool checkFileName, LICE_IBitmap *bmpbase); 
  const char *(*get_extlist)(); // returns GetOpenFileName sort of list "JPEG files (*.jpg)\0*.jpg\0"

  struct _LICE_ImageLoader_rec *_next;
};
extern _LICE_ImageLoader_rec *LICE_ImageLoader_list;


#endif // LICE_PROVIDED_BY_APP

#ifdef __APPLE__
#define LICE_Scale_BitBlt(hdc, x,y,w,h, src, sx,sy, mode) do { \
   const int _x=(x), _y=(y), _w=(w), _h=(h), _sx = (sx), _sy = (sy), _mode=(mode); \
   const int rsc = (int) (src)->Extended(LICE_EXT_GET_SCALING,NULL); \
   if (rsc>0) \
     StretchBlt(hdc,_x,_y,_w,_h,(src)->getDC(),(_sx*rsc)/256,(_sy*rsc)/256,(_w*rsc)>>8,(_h*rsc)>>8,_mode); \
   else BitBlt(hdc,_x,_y,_w,_h,(src)->getDC(),_sx,_sy,_mode); \
} while (0)
#else
#define LICE_Scale_BitBlt(hdc, x,y,w,h, src, sx,sy, mode) BitBlt(hdc,x,y,w,h,(src)->getDC(),sx,sy,mode)
#endif

#endif
