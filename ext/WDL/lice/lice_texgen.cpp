/*
  Cockos WDL - LICE - Lightweight Image Compositing Engine
  Copyright (C) 2007 and later, Cockos Incorporated
  File: lice_texgen.cpp (LICE texture generator routines)
  See lice.h for license and other information
*/


#include "lice.h"
#include <math.h>

void LICE_TexGen_Marble(LICE_IBitmap *dest, const RECT *rect, float rv, float gv, float bv, float intensity)
{
  int span=dest->getRowSpan();
  int w = dest->getWidth();
  int h = dest->getHeight();
  int x = 0;
  int y = 0;
  if(rect)
  {
    x = rect->left;
    y = rect->top;
    w = rect->right - rect->left;
    h = rect->bottom - rect->top;
  }

  if (x<0) { w+=x; x=0; }
  if (y<0) { h+=y; y=0; }

  const int destbm_w = dest->getWidth(), destbm_h = dest->getHeight();
  if (w<1 || h < 1 || x >= destbm_w || y >= destbm_h) return;

  if (w>destbm_w-x) w=destbm_w-x;
  if (h>destbm_h-y) h=destbm_h-y;


  LICE_pixel *startp = dest->getBits();
  if (dest->isFlipped())
  {
    startp += x + (dest->getHeight()-1-y)*span;
    span=-span;
  }
  else startp  += x + y*span;

  //simple 16bit marble noise generator

#define ROL(x,y) ((x<<(y))|(((unsigned short)x)>>(16-(y))))
#define ROR(x,y) ((((unsigned short)x)>>(y))|(x<<(16-(y))))

  intensity/=1024.0f;
  int maxc = 0;
  {
    LICE_pixel *p = startp;
    short n1 = 0, n2 = 0;
    for(int i=0;i<h;i++)
    {
      for(int j=0;j<w;j++)
      {
        n1 += n2;
        n1 = ROL(n1, n2&0xf);
        n2 += 2;
        n2 = ROR(n2, 1);
        
        int val = (int)(n1*intensity)+1;
        
        LICE_pixel c = w;
        LICE_pixel c2 = w/2;
        if(i>0)
        {
          c = p[j-span];
          if(j==0)
            c2 = p[(w-1)-span];
          else
            c2 = p[(j-1)-span];
        }
        
        int pix = (((c + c2)/2) + val);
        if(pix>maxc) maxc = pix;
        p[j] = pix;
      }
      p+=span;
    }
  }

  //normalize values and apply gamma
  {
    LICE_pixel *p = startp;
    float sc=255.0f/maxc;

    for(int i=0;i<h;i++)
    {
      for(int j=0;j<w;j++)
      {
        float col = (float)fabs(p[j]*sc);
        p[j] = LICE_RGBA((int)(col*rv),(int)(col*gv),(int)(col*bv),255);
      }
      p+=span;
    }
  }
}

//standard perlin noise implementation
#if 1
int m_noiseTab[512];
void initNoise()
{
  static int init = 0;
  if(!init)
  {
    const int permutation[] = { 151,160,137,91,90,15,
      131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
      190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
      88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
      77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
      102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
      135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
      5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
      223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
      129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
      251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
      49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
      138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
    };
    for (int i=0; i < 256 ; i++) 
      m_noiseTab[256+i] = m_noiseTab[i] = permutation[i];    
    init = 1;
  }
}

static __inline float fade(float t) 
{ 
  return t * t * t * (t * (t * 6 - 15) + 10); 
}
static __inline float lerp(float t, float a, float b) 
{ 
  return a + t * (b - a); 
}
static __inline float grad(int hash, float x, float y) 
{
  //convert lo 4 bits of hash code into 12 gradient directions
  int h = hash & 15;
  float u = h<8 ? x : y,
         v = h<4 ? y : h==12||h==14 ? x : 0;
  return ((h&1) == 0 ? u : -u) + ((h&2) == 0 ? v : -v);
}
static float noise(float x, float y) 
{
  //find unit cube that contains point
  int X = (int)floor(x) & 255, Y = (int)floor(y) & 255;

  //find relative x,y,z of point in cube
  x -= (float)floor(x);
  y -= (float)floor(y);

  //compute fade curves for each of x,y,z
  float u = fade(x), v = fade(y);

  //hash coordinates of the 8 cube corners
  int A = m_noiseTab[X  ]+Y, AA = m_noiseTab[A], AB = m_noiseTab[A+1], 
      B = m_noiseTab[X+1]+Y, BA = m_noiseTab[B], BB = m_noiseTab[B+1];

  //and add blended results from 8 corners of cube
  return lerp(v, lerp(u, grad(m_noiseTab[AA  ], x  , y     ),
                         grad(m_noiseTab[BA  ], x-1, y     )),
                 lerp(u, grad(m_noiseTab[AB  ], x  , y-1   ),
                         grad(m_noiseTab[BB  ], x-1, y-1   )));
}
#else
//faster implementation but way lower quality
#define noiseWidth 128
#define noiseHeight 128

float m_noiseTab[noiseWidth][noiseHeight];

void initNoise()
{
  static int init = 0;
  if(init) return;

  for (int x = 0; x < noiseWidth; x++)
    for (int y = 0; y < noiseHeight; y++)
    {
      m_noiseTab[x][y] = (float)(rand() % 32768) / 32768.0;
    }

  init = 1;  
}

float noise(float x, float y)
{
  //x*=noiseWidth;
  //y*=noiseHeight;
  
  //get fractional part of x and y
  float fractX = x - (int)x;
  float fractY = y - (int)y;
  
  //wrap around
  int x1 = ((int)x + noiseWidth) % noiseWidth;
  int y1 = ((int)y + noiseHeight) % noiseHeight;
  
  //neighbor values
  int x2 = (x1 + noiseWidth - 1) % noiseWidth;
  int y2 = (y1 + noiseHeight - 1) % noiseHeight;
  
  //smooth the noise with bilinear interpolation
  float value = 0.0;
  value += fractX       * fractY       * m_noiseTab[x1][y1];
  value += fractX       * (1 - fractY) * m_noiseTab[x1][y2];
  value += (1 - fractX) * fractY       * m_noiseTab[x2][y1];
  value += (1 - fractX) * (1 - fractY) * m_noiseTab[x2][y2];
  
  return value;
}
#endif

void LICE_TexGen_Noise(LICE_IBitmap *dest, const RECT *rect, float rv, float gv, float bv, float intensity, int mode, int smooth)
{
  initNoise();

  int span=dest->getRowSpan();
  int w = dest->getWidth();
  int h = dest->getHeight();
  int dx = 0;
  int dy = 0;
  if(rect)
  {
    dx = rect->left;
    dy = rect->top;
    w = rect->right - rect->left;
    h = rect->bottom - rect->top;
  }

  if (dx<0) { w+=dx; dx=0; }
  if (dy<0) { h+=dy; dy=0; }
  const int destbm_w = dest->getWidth(), destbm_h = dest->getHeight();
  if (w<1 || h < 1 || dx >= destbm_w || dy >= destbm_h) return;

  if (w>destbm_w-dx) w=destbm_w-dx;
  if (h>destbm_h-dy) h=destbm_h-dy;

  LICE_pixel *startp = dest->getBits();
  if (dest->isFlipped())
  {
    startp += dx + (dest->getHeight()-1-dy)*span;
    span=-span;
  }
  else startp  += dx + dy*span;

  {
    LICE_pixel *p = startp;
    for(int i=0;i<h;i++)
    {
      for(int j=0;j<w;j++)
      {
        float x = (float)j/w*16*intensity;
        float y = (float)i/h*16*intensity;

        float val = 0;
        int size = smooth;
        while(size>=1)
        {
          switch(mode)
          {
          case NOISE_MODE_NORMAL: val += noise(x/size, y/size)*size; break;
          case NOISE_MODE_WOOD: val += (float)cos( x/size + noise(x/size,y/size) )*size/2; break;
          }
          size /= 2;
        }
        float col = (float)fabs(val/smooth)*255;
        if(col>255) col=255;

        p[j] = LICE_RGBA((int)(col*rv),(int)(col*gv),(int)(col*bv),255);
      }
      p+=span;
    }
  }
}

static float turbulence(int x, int y, float size, float isize)
{
  float value = 0.0;
  const float initialSize = isize;
  while(size >= 1)
  {
    value += noise(x * isize, y * isize) * size;
    size *= 0.5f;
    isize *= 2.0f;
  }
  return(128.0f * value * initialSize);
}

void LICE_TexGen_CircNoise(LICE_IBitmap *dest, const RECT *rect, float rv, float gv, float bv, float nrings, float power, int size)
{
  initNoise();

  int span=dest->getRowSpan();
  int w = dest->getWidth();
  int h = dest->getHeight();
  int x = 0;
  int y = 0;
  if(rect)
  {
    x = rect->left;
    y = rect->top;
    w = rect->right - rect->left;
    h = rect->bottom - rect->top;
  }

  if (x<0) { w+=x; x=0; }
  if (y<0) { h+=y; y=0; }
  const int destbm_w = dest->getWidth(), destbm_h = dest->getHeight();
  if (w<1 || h < 1 || x >= destbm_w || y >= destbm_h) return;

  if (w>destbm_w-x) w=destbm_w-x;
  if (h>destbm_h-y) h=destbm_h-y;

  LICE_pixel *startp = dest->getBits();
  if (dest->isFlipped())
  {
    startp += x + (dest->getHeight()-1-y)*span;
    span=-span;
  }
  else startp  += x + y*span;

  float xyPeriod = nrings;
  float turbPower = power;
  const float iturbSize = 1.0f/(float)size;
  const float turbSize = (float)size;
   
  {
    LICE_pixel *p = startp;
    for(int i=0;i<h;i++)
    {
      for(int j=0;j<w;j++)
      {
        float xValue = ((float)j - w / 2) / w;
        float yValue = ((float)i - h / 2) / h;

        float distValue = (float) (sqrt(xValue * xValue + yValue * yValue) + turbPower * turbulence(j, i, turbSize, iturbSize) / 256.0);
        float col = (float)fabs(256.0 * sin(2 * xyPeriod * distValue * 3.14159));

        p[j] = LICE_RGBA((int)(col*rv),(int)(col*bv),(int)(col*gv),255);
      }
      p+=span;
   }
  }
}
