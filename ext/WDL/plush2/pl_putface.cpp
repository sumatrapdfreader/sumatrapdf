#include "plush.h"
#include "../lice/lice_combine.h"

//#define PLUSH_NO_SOLIDFLAT // make non-texturemapped flat shading optimized engine
//#define PLUSH_NO_SOLIDGOURAUD // disable non-texturemapped gouraud  optimized engine
//#define PLUSH_NO_TEXTURE // disable single-texture optimized engine
//#define PLUSH_NO_MULTITEXTURE // disable multitexture (this can do any of em)
#define XPOS_BITS 19 // allows 2^13 max screen width, or 8192px

#define SWAP(a,b,t) { t ____tmp=(a); (a)=(b); (b)=____tmp; }

#define PUTFACE_SORT() \
  char i0 = 0; char i1 = 1; char i2 = 2; char stat; \
  if (TriFace->Scry[0] > TriFace->Scry[1]) {  i0 = 1; i1 = 0;  } \
  if (TriFace->Scry[i0] > TriFace->Scry[2]) { SWAP(i0,i2,char); } \
  if (TriFace->Scry[i1] > TriFace->Scry[i2]) { SWAP(i1,i2,char); } \
  int Scrx[3] = {(int)(TriFace->Scrx[0]*(1<<XPOS_BITS)), (int)(TriFace->Scrx[1]*(1<<XPOS_BITS)),(int)(TriFace->Scrx[2]*(1<<XPOS_BITS))}; \
  int Scry[3] = {(int) (TriFace->Scry[0]+0.5), (int) (TriFace->Scry[1]+0.5), (int) (TriFace->Scry[2]+0.5)}; \

#define DO_STAT_XDELTAS \
          if (stat & 1) { \
            dX1 = (Scrx[i2]-(X1 = Scrx[i1]))/dY;  \
            if (stat & 8) dX2 = (Scrx[i2]-(X2 = Scrx[i0]))/dY; \
          } \
          else if (stat & 2) { \
            dX2 = (Scrx[i2]-(X2 = Scrx[i1]))/dY; \
            if (stat & 4) dX1 = (Scrx[i2]-(X1 = Scrx[i0]))/dY; \
          }


static inline void OverlayBlend(int &red, int &green, int &blue, int &alpha, int r, int g, int b, int a, int usea)
{
  int da=(256-usea)*128; 
  int srcr = r*usea+da, srcg = g*usea+da, srcb = b*usea+da, srca = usea*a + da;
  red = ( red*( (red*(32768-srcr))/256 + srcr ) )/32768;
  green = ( green*( (green*(32768-srcg))/256 + srcg ) )/32768;
  blue = ( blue*( (blue*(32768-srcb))/256 + srcb ) )/32768;
  alpha = ( alpha*( (alpha*(32768-srca))/256 + srca ) )/32768;
}

static inline void MulBlend(int &red, int &green, int &blue, int &alpha, int r, int g, int b, int a, int ta)
{
  int ta2=(256-ta)*256;
  red = (r*ta*red + red*ta2)/65536;
  green = (g*ta*green + green*ta2)/65536;
  blue = (b*ta*blue + blue*ta2)/65536;
  alpha = (a*ta*alpha + alpha*ta2)/65536;
}


static inline void AdjustHSV(int &red, int &green, int &blue, int r, int g, int b, int texalpha)
{
  int h,s,v;
  __LICE_RGB2HSV(red,green,blue,&h,&s,&v);
    h+=(((r+r/2) - 192) * texalpha)/256;
    if (h<0)h+=384;
    else if (h>=384) h-=384;
    s+=((g-128)*texalpha)/256;
    if (s&~0xff)
    {
      if (s<0)s=0;
      else s=255;
    }
    v+=((b-128)*texalpha)/256;
    if (v&~0xff)
    {
      if (v<0)v=0;
      else v=255;
    }
  __LICE_HSV2RGB(h,s,v,&red,&green,&blue);
}

static inline void DodgeBlend(int &red, int &green, int &blue, int &alpha, int r, int g, int b, int a, int ta)
{
  int src_r = 256-r*ta/256;
  int src_g = 256-g*ta/256;
  int src_b = 256-b*ta/256;
  int src_a = 256-(a*ta)/256;

  red = src_r > 1 ? 256*red / src_r : 256*red;
  green = src_g > 1 ? 256*green / src_g : 256*green;
  blue = src_b > 1 ? 256*blue / src_b : 256*blue;
  alpha =  src_a > 1 ? 256*alpha / src_a : 256*alpha;
}


static void inline DoTextureCombine(int texcomb, int r, int g, int b, int a, int &red, int &green, int &blue, int &alpha, int texalpha, int texalpha2)
{
  switch (texcomb)
  {
    case LICE_BLIT_MODE_COPY:
      red = (r*texalpha + red*texalpha2) >> 8;
      green = (g*texalpha + green*texalpha2) >> 8;
      blue = (b*texalpha + blue*texalpha2) >> 8;
      alpha = (a*texalpha + alpha*texalpha2) >> 8;
    break;
    case LICE_BLIT_MODE_ADD:
      red += (r*texalpha) >> 8;
      green += (g*texalpha) >> 8;
      blue += (b*texalpha) >> 8;
      alpha += (a*texalpha) >> 8;
    break;
    case LICE_BLIT_MODE_MUL:
      MulBlend(red,green,blue,alpha,r,g,b,a,texalpha);
    break;
    case LICE_BLIT_MODE_DODGE:
      DodgeBlend(red,green,blue,alpha,r,g,b,a,texalpha);

    break;
    case LICE_BLIT_MODE_OVERLAY:
      OverlayBlend(red,green,blue,alpha,r,g,b,a, texalpha);
    break;
    case LICE_BLIT_MODE_HSVADJ:
      AdjustHSV(red,green,blue,r,g,b,texalpha);
    break;
    case LICE_BLIT_MODE_COPY|LICE_BLIT_USE_ALPHA:
      {
        int ta=(texalpha*(a+1));
        int ta2=(65536-ta);
        red = (r*ta + red*ta2) >> 16;
        green = (g*ta + green*ta2) >> 16;
        blue = (b*ta + blue*ta2) >> 16;
        alpha = (a*ta + alpha*ta2) >> 16;
      }
    break;
    case LICE_BLIT_MODE_ADD|LICE_BLIT_USE_ALPHA:
      {
        int ta=(texalpha*(a+1));
        red += (r*ta) >> 16;
        green += (g*ta) >> 16;
        blue += (b*ta) >> 16;
        alpha += (a*ta) >> 16;
      }
    break;
    case LICE_BLIT_MODE_DODGE|LICE_BLIT_USE_ALPHA:
      {
        int ta=(texalpha*(a+1))/256;
        DodgeBlend(red,green,blue,alpha,r,g,b,a,ta);
      }
    break;
    case LICE_BLIT_MODE_MUL|LICE_BLIT_USE_ALPHA:
      MulBlend(red,green,blue,alpha,r,g,b,a,(texalpha*(a+1))/256);
    break;
    case LICE_BLIT_MODE_OVERLAY|LICE_BLIT_USE_ALPHA:
      OverlayBlend(red,green,blue,alpha,r,g,b,a, (texalpha*(a+1))/256);
    break;
    case LICE_BLIT_MODE_HSVADJ|LICE_BLIT_USE_ALPHA:
      AdjustHSV(red,green,blue,r,g,b,(texalpha*(a+1))/256);
    break;
    case -2:
    break;
    default:
      red=r; green=g; blue=b; alpha=a;
    break;
  }
}


static void inline TextureMakePixelSolidCombine(int &red, int &green, int &blue, int &alpha,
                                                 pl_sInt32 *CL, int solidcomb, int solidalpha,
                                                 int solidalpha2,
                                                 LICE_pixel_chan *gmemptr)
{

  switch (solidcomb)
  {
    case LICE_BLIT_MODE_COPY:
      red = ((CL[0]>>8)*solidalpha + gmemptr[LICE_PIXEL_R]*solidalpha2)>>16;
      green = ((CL[1]>>8)*solidalpha + gmemptr[LICE_PIXEL_G]*solidalpha2)>>16;
      blue = ((CL[2]>>8)*solidalpha + gmemptr[LICE_PIXEL_B]*solidalpha2)>>16;
      alpha = solidalpha;
    break;
    case LICE_BLIT_MODE_ADD: 
      red = gmemptr[LICE_PIXEL_R] +   (((CL[0]>>8)*solidalpha)>>16);
      green = gmemptr[LICE_PIXEL_G] + (((CL[1]>>8)*solidalpha)>>16);
      blue = gmemptr[LICE_PIXEL_B] +  (((CL[2]>>8)*solidalpha)>>16);
      alpha = gmemptr[LICE_PIXEL_A] + solidalpha;
    break;
    case LICE_BLIT_MODE_DODGE:
      red=gmemptr[LICE_PIXEL_R];
      green=gmemptr[LICE_PIXEL_G];
      blue=gmemptr[LICE_PIXEL_B];
      alpha=gmemptr[LICE_PIXEL_A];
      DodgeBlend(red,green,blue,alpha,CL[0]>>16,CL[1]>>16,CL[2]>>16,solidalpha,solidalpha);
    break;
    case LICE_BLIT_MODE_MUL:
      red=gmemptr[LICE_PIXEL_R];
      green=gmemptr[LICE_PIXEL_G];
      blue=gmemptr[LICE_PIXEL_B];
      alpha=gmemptr[LICE_PIXEL_A];
      MulBlend(red,green,blue,alpha,CL[0]>>16,CL[1]>>16,CL[2]>>16,solidalpha,solidalpha);
    break;
    case LICE_BLIT_MODE_OVERLAY:
      red=gmemptr[LICE_PIXEL_R];
      green=gmemptr[LICE_PIXEL_G];
      blue=gmemptr[LICE_PIXEL_B];
      alpha=gmemptr[LICE_PIXEL_A];
      OverlayBlend(red,green,blue,alpha,CL[0]>>16,CL[1]>>16,CL[2]>>16,solidalpha, solidalpha);
    break;
    case LICE_BLIT_MODE_HSVADJ:
      red=gmemptr[LICE_PIXEL_R];
      green=gmemptr[LICE_PIXEL_G];
      blue=gmemptr[LICE_PIXEL_B];
      alpha=gmemptr[LICE_PIXEL_A];
      AdjustHSV(red,green,blue,CL[0]>>16,CL[1]>>16,CL[2]>>16,solidalpha);
    break;
    case -2:
      red=gmemptr[LICE_PIXEL_R];
      green=gmemptr[LICE_PIXEL_G];
      blue=gmemptr[LICE_PIXEL_B];
      alpha=gmemptr[LICE_PIXEL_A];
    break;
    default:
      red=CL[0]>>16;
      green=CL[1]>>16;
      blue=CL[2]>>16;
      alpha=solidalpha;
    break;
  }
}


static void inline TextureMakePixel2(LICE_pixel_chan *gmemptr,
                   int solidcomb, int solidalpha, int solidalpha2,
                   pl_sInt32 *CL,
                   bool bilinear,
                   pl_sInt32 iUL, pl_sInt32 iVL, 
                   pl_sInt32 texwidth, pl_sInt32 texheight, 
                   LICE_pixel *texture, int tex_rowspan,
                   int texcomb, 
                   int texalpha,
                   int texalpha2,
                   bool bilinear2,
                   pl_sInt32 iUL_2, pl_sInt32 iVL_2, 
                   pl_sInt32 texwidth_2, pl_sInt32 texheight_2, 
                   LICE_pixel *texture2, int tex_rowspan_2,
                   int tex2comb, 
                   int tex2alpha,
                   int tex2alpha2)
{

  int red,green,blue,alpha;
  
  TextureMakePixelSolidCombine(red,green,blue,alpha,CL,solidcomb,solidalpha,solidalpha2,gmemptr);

  int r,g,b,a;

#if defined(PLUSH_NO_TEXTURE)
  if (texture)
#endif
  {
    const int xpos=(iUL>>14)&~3;
    const int ypos=iVL>>16;
    const LICE_pixel_chan *rd = ((LICE_pixel_chan*)texture) + xpos+ypos*tex_rowspan;

    if (bilinear)
    {
      __LICE_BilinearFilterI_2(&r,&g,&b,&a,rd,          
        ypos < texheight - 1 ? rd+tex_rowspan : ((LICE_pixel_chan *)texture)+xpos,
        xpos < texwidth - 4 ? 4 : 4-texwidth,
          iUL&65535,iVL&65535);
    }
    else
    {
      r=rd[LICE_PIXEL_R]; g=rd[LICE_PIXEL_G]; b=rd[LICE_PIXEL_B]; a=rd[LICE_PIXEL_A];
    }

    DoTextureCombine(texcomb, r,g,b,a, red,green,blue,alpha,texalpha,texalpha2);
  }

#ifdef PLUSH_NO_TEXTURE
  if (texture2)
#endif
  {
    const int xpos=(iUL_2>>14)&~3;
    const int ypos=iVL_2>>16;
    const LICE_pixel_chan *rd = ((LICE_pixel_chan*)texture2) + xpos+ypos*tex_rowspan_2;

    if (bilinear2)
    {
      __LICE_BilinearFilterI_2(&r,&g,&b,&a,rd,          
        ypos < texheight_2 - 1 ? rd+tex_rowspan_2 : ((LICE_pixel_chan *)texture2)+xpos,
        xpos < texwidth_2 - 4 ? 4 : 4-texwidth_2,
        iUL_2&65535,iVL_2&65535);
    }
    else
    {
      r=rd[LICE_PIXEL_R]; g=rd[LICE_PIXEL_G]; b=rd[LICE_PIXEL_B]; a=rd[LICE_PIXEL_A];
    }

    DoTextureCombine(tex2comb, r,g,b,a, red,green,blue,alpha,tex2alpha,tex2alpha2);
  }

  _LICE_MakePixelClamp(gmemptr, red,green,blue,alpha);
}

static void inline TextureMakePixel(LICE_pixel_chan *gmemptr,
                   int solidcomb, int solidalpha, int solidalpha2,
                   pl_sInt32 *CL,
                   bool bilinear,
                   pl_sInt32 iUL, pl_sInt32 iVL, 
                   pl_sInt32 texwidth, pl_sInt32 texheight, 
                   LICE_pixel *texture, int tex_rowspan,
                   int texcomb, 
                   int texalpha,
                   int texalpha2)
{

  int red,green,blue,alpha;

  if (
#ifdef PLUSH_NO_SOLIDGOURAUD
    !texture||
#endif
    texcomb!=-1)
    TextureMakePixelSolidCombine(red,green,blue,alpha,CL,solidcomb,solidalpha,solidalpha2,gmemptr);



#ifdef PLUSH_NO_SOLIDGOURAUD
  if (texture)
#endif
  {
    int r,g,b,a;
    const int xpos=(iUL>>14)&~3;            
    const int ypos=iVL>>16;
    const LICE_pixel_chan *rd = ((LICE_pixel_chan*)texture) + xpos+ypos*tex_rowspan;

    if (bilinear)
    {

      __LICE_BilinearFilterI_2(&r,&g,&b,&a,rd,          
        ypos < texheight - 1 ? rd+tex_rowspan : ((LICE_pixel_chan *)texture)+xpos,
        xpos < texwidth - 4 ? 4 : 4-texwidth,
        iUL&65535,iVL&65535);
    }
    else
    {
      r=rd[LICE_PIXEL_R]; g=rd[LICE_PIXEL_G]; b=rd[LICE_PIXEL_B]; a=rd[LICE_PIXEL_A];
    }

    DoTextureCombine(texcomb, r,g,b,a, red,green,blue,alpha,texalpha,texalpha2);
  }

  _LICE_MakePixelClamp(gmemptr, red,green,blue,alpha);
}



#ifndef PLUSH_NO_TEXTURE
#include "pl_pf_tex.h"
#endif

#ifndef PLUSH_NO_MULTITEXTURE
#define PL_PF_MULTITEX
#include "pl_pf_tex.h"
#endif




template<class Comb> class PLSolidPutFace
{
  public:
#ifndef PLUSH_NO_SOLIDGOURAUD
  static void SolidGouraud(LICE_pixel *gmem, int swidth, pl_Face *TriFace, int alpha, pl_ZBuffer *zbuf, int zfb_width) 
  {
    pl_Float dZL=0, dZ1=0, dZ2=0;
    pl_sInt32 dX1=0, dX2=0, C1[3], C2[3], dC1[3]={0}, dC2[3]={0}, dCL[3]={0}, C3[3];

    PUTFACE_SORT();

  
    int a;
    for(a=0;a<3;a++)
    {
      C1[a] = (pl_sInt32) (TriFace->Shades[i0][a]*(1<<24));
      C2[a] = (pl_sInt32) (TriFace->Shades[i1][a]*(1<<24));
      C3[a] = (pl_sInt32) (TriFace->Shades[i2][a]*(1<<24));
    }
    pl_sInt32 X2,X1;
    X2 = X1 = Scrx[i0];
    pl_Float Z1 = TriFace->Scrz[i0];
    pl_Float Z2 = TriFace->Scrz[i1];
    pl_Float Z3 = TriFace->Scrz[i2];

    pl_sInt32 Y0 = Scry[i0];
    pl_sInt32 Y1 = Scry[i1];
    pl_sInt32 Y2 = Scry[i2];

    {
      pl_sInt32 dY = Y2 - Y0;
      if (dY) {
        dX2 = (Scrx[i2] - X1) / dY;
        for(a=0;a<3;a++) dC2[a] = (C3[a] - C1[a]) / dY;
        dZ2 = (Z3 - Z1) / dY;
      }
      dY = Y1 - Y0;
      if (dY) {
        dX1 = (Scrx[i1] - X1) / dY;
        for(a=0;a<3;a++) 
          dC1[a] = (C2[a] - C1[a]) / dY;
        dZ1 = (Z2 - Z1) / dY;
        if (dX2 < dX1) {
          SWAP(dX2,dX1,pl_sInt32);
          for(a=0;a<3;a++)  SWAP(dC1[a],dC2[a],pl_sInt32);
          SWAP(dZ1,dZ2,pl_Float);
          stat = 2;
        } else stat = 1;
        Z2 = Z1;
        C2[0] = C1[0];
        C2[1] = C1[1];
        C2[2] = C1[2];
      } else {
        if (Scrx[i1] > X1) {
          X2 = Scrx[i1];
          stat = 2|4;
        } else {
          for(a=0;a<3;a++) SWAP(C1[a],C2[a],pl_sInt32);
          SWAP(Z1,Z2,pl_Float);
          X1 = Scrx[i1];
          stat = 1|8;
        }
      } 

      pl_sInt32 tmp = (dX1-dX2)*dY;
      if (tmp) {
        double v=(1<<XPOS_BITS)/(double)tmp;
        for(a=0;a<3;a++)
          dCL[a] = (pl_sInt32) (((dC1[a]-dC2[a])*dY)*v);
        dZL = ((dZ1-dZ2)*dY)*v;
      } else {
        tmp = X2-X1;
        if (tmp) {
          double v=(1<<XPOS_BITS)/(double)tmp;
          for(a=0;a<3;a++)
            dCL[a] = (pl_sInt32) ((C2[a]-C1[a])*v);
          dZL = (Z2-Z1)*v;
        }
      }
    }

    gmem += (Y0 * swidth);
    zbuf += (Y0 * zfb_width);

    while (Y0 < Y2) {
      if (Y0 == Y1) {
        pl_sInt32 dY = Y2 - Scry[i1];
        if (dY) {
          double v=1.0/dY;
          dZ1 = (Z3-Z1)*v;
          dC1[0] = (pl_sInt32) ((C3[0]-C1[0])*v);
          dC1[1] = (pl_sInt32) ((C3[1]-C1[1])*v);
          dC1[2] = (pl_sInt32) ((C3[2]-C1[2])*v);

          DO_STAT_XDELTAS

        }
      }
      pl_sInt32 XL1 = (X1+(1<<(XPOS_BITS-1)))>>XPOS_BITS;
      pl_sInt32 XL2 = ((X2+(1<<(XPOS_BITS-1)))>>XPOS_BITS) - XL1;
      if (XL2 > 0) {
        gmem += XL1;         
        XL1 += XL2;
        pl_sInt32 CL[3] = {C1[0],C1[1],C1[2]};
        if (zbuf)
        {
	      pl_Float ZL = Z1;
          zbuf += XL1-XL2;
          do {
            if (*zbuf < ZL) {
              *zbuf = (pl_ZBuffer) ZL;

              Comb::doPix((LICE_pixel_chan *)gmem,CL[0]>>16,CL[1]>>16,CL[2]>>16,255,alpha);
            }
            gmem++;
            zbuf++;
            ZL += dZL;
            CL[0] += dCL[0];
            CL[1] += dCL[1];
            CL[2] += dCL[2];
          } while (--XL2);
          zbuf -= XL1;
        }
        else do {
            Comb::doPix((LICE_pixel_chan *)gmem,CL[0]>>16,CL[1]>>16,CL[2]>>16,255,alpha);
            gmem++;
            CL[0] += dCL[0];
            CL[1] += dCL[1];
            CL[2] += dCL[2];          
        } while (--XL2);
        gmem -= XL1;        
      }
      gmem += swidth;
      zbuf += zfb_width;
      X1 += dX1;
      X2 += dX2; 
      C1[0] += dC1[0];
      C1[1] += dC1[1];
      C1[2] += dC1[2];
      Z1 += dZ1;
      Y0++;
    }
  }
#endif

#ifndef PLUSH_NO_SOLIDFLAT
  static void Solid(LICE_pixel *gmem, int swidth, pl_Face *TriFace, int alpha, pl_ZBuffer *zbuf, int zfb_width) 
  {
    pl_sInt32 dX1=0, dX2=0;
    pl_Float dZL=0, dZ1=0, dZ2=0;

    PUTFACE_SORT();

    int col0 = (int) (TriFace->Shades[0][0]*255.0);
    int col1 = (int) (TriFace->Shades[0][1]*255.0);
    int col2 = (int) (TriFace->Shades[0][2]*255.0);

    pl_sInt32 X1,X2;
    X2 = X1 = Scrx[i0];
    pl_sInt32 Y0 = Scry[i0];
    pl_sInt32 Y1 = Scry[i1];
    pl_sInt32 Y2 = Scry[i2];

    pl_Float Z1 = TriFace->Scrz[i0];
    pl_Float Z2 = TriFace->Scrz[i1];
    pl_Float Z3 = TriFace->Scrz[i2];

    {
      pl_sInt32 dY = Y2-Y0;
      if (dY) {
        dX2 = (Scrx[i2] - X1) / dY;
        dZ2 = (Z3 - Z1) / dY;
      }
      dY = Y1-Y0;
      if (dY) {
        dX1 = (Scrx[i1] - X1) / dY;
        dZ1 = (Z2 - Z1) / dY;
        if (dX2 < dX1) {
          SWAP(dX1,dX2,pl_sInt32);
          SWAP(dZ1,dZ2,pl_Float);
          stat = 2;
        } else stat = 1;
        Z2 = Z1;
      } else {
        if (Scrx[i1] > X1) {
          X2 = Scrx[i1];
          stat = 2|4;
        } else {
          X1 = Scrx[i1];
          SWAP(Z1,Z2,pl_Float);
          stat = 1|8;
        }
      } 

      if (zbuf) 
      {
        pl_sInt32 tmp=(dX1-dX2)*dY;
        if (tmp) dZL = ((dZ1-dZ2)*dY)*(double)(1<<XPOS_BITS)/(double)tmp;
        else { 
          tmp = X2-X1;
          if (tmp) dZL = (Z2-Z1)*(double)(1<<XPOS_BITS)/tmp;
        }
      }
    }

    gmem += (Y0 * swidth);
    zbuf += (Y0 * zfb_width);

    while (Y0 < Y2) {
      if (Y0 == Y1) {
        pl_sInt32 dY = Y2 - Scry[i1];
        if (dY) {
          DO_STAT_XDELTAS
          dZ1 = (Z3-Z1)/dY;
        }
      }
      pl_sInt32 XL1 = (X1+(1<<(XPOS_BITS-1)))>>XPOS_BITS;
      pl_sInt32 XL2 = ((X2+(1<<(XPOS_BITS-1)))>>XPOS_BITS) - XL1;
      if (XL2 > 0) {
        gmem += XL1;
        XL1 += XL2;
        if (zbuf) 
        {
	      pl_Float ZL = Z1;
          zbuf += XL1-XL2;
          do {
            if (*zbuf < ZL) {
              *zbuf = (pl_ZBuffer) ZL;
              Comb::doPix((LICE_pixel_chan *)gmem,col0,col1,col2,255,alpha);
            }
            gmem++; 
            zbuf++;
            ZL += dZL;
          } while (--XL2);
          zbuf -= XL1;
        }
        else 
        {
          do {
            Comb::doPix((LICE_pixel_chan *)gmem,col0,col1,col2,255,alpha);
            gmem++;
          } while (--XL2);
        }
        gmem -= XL1;
        
      }
      gmem += swidth;
      zbuf += zfb_width;
      Z1 += dZ1;
      X1 += dX1;
      X2 += dX2;
      Y0++;
    }
  }
#endif
};

void pl_Cam::PutFace(pl_Face *TriFace)
{
  LICE_pixel *gmem = frameBuffer->getBits();
  
  int zfb_width = 0;
  pl_ZBuffer *zb = NULL;
  if (zBuffer.GetSize()&&TriFace->Material->zBufferable)
  {
    zfb_width = frameBuffer->getWidth();
    zb = zBuffer.Get();
  }
  
  int swidth = frameBuffer->getRowSpan();
  if (frameBuffer->isFlipped())
  {
    gmem += swidth*(frameBuffer->getHeight()-1);
    swidth=-swidth;
  }

  pl_Mat *mat=TriFace->Material;

#ifndef PLUSH_NO_MULTITEXTURE

  #ifndef PLUSH_NO_TEXTURE
    if (mat->Texture&&mat->Texture2)
  #else
    #ifndef PLUSH_NO_SOLIDGOURAUD
      if (mat->Texture||mat->Texture2)
    #endif
  #endif
  {
    pl_Float texsc[4];
    memcpy(texsc,mat->TexScaling,sizeof(mat->TexScaling));
    memcpy(texsc+2,mat->Tex2Scaling,sizeof(mat->Tex2Scaling));
    int tidx = mat->TexMapIdx;
    if (tidx<0 || tidx>=PLUSH_MAX_MAPCOORDS)tidx=PLUSH_MAX_MAPCOORDS-1;
    int tidx2 = mat->Tex2MapIdx;
    if (tidx2<0 || tidx2>=PLUSH_MAX_MAPCOORDS)tidx2=PLUSH_MAX_MAPCOORDS-1;

    PLMTexTri(gmem,swidth,TriFace,zb,zfb_width,(int) (mat->SolidOpacity*256.0),mat->SolidCombineMode,
      mat->Texture,texsc,(int) (mat->TexOpacity*256.0),mat->TexCombineMode,tidx,
      mat->Texture2,(int) (mat->Tex2Opacity*256.0),mat->Tex2CombineMode,tidx2
      );
    return;
  }
#endif


#ifndef PLUSH_NO_TEXTURE
  #ifndef PLUSH_NO_SOLIDGOURAUD
    if (mat->Texture||mat->Texture2)
  #endif
  {
    LICE_IBitmap *tex=mat->Texture ? mat->Texture : mat->Texture2;
    int talpha = (int) (mat->Texture ? mat->TexOpacity*256.0 : mat->Tex2Opacity*256.0);
    int tcomb = (int) (mat->Texture ? mat->TexCombineMode : mat->Tex2CombineMode);
    int tidx = (mat->Texture ? mat->TexMapIdx: mat->Tex2MapIdx);
    if (tidx<0 || tidx>=PLUSH_MAX_MAPCOORDS)tidx=PLUSH_MAX_MAPCOORDS-1;
    pl_Float texsc[2];
    memcpy(texsc,mat->Texture ? mat->TexScaling : mat->Tex2Scaling,sizeof(texsc));
    PLTexTri(gmem,swidth,TriFace,zb,zfb_width,(int) (mat->SolidOpacity*256.0),mat->SolidCombineMode,tex,texsc,talpha,tcomb,tidx);
    return;
  }
#endif

  int alpha=(int) (mat->SolidOpacity*256.0);
  if (!alpha) return;
#ifndef PLUSH_NO_SOLIDGOURAUD
#ifndef PLUSH_NO_SOLIDFLAT
  if (mat->Smoothing)
#endif
  {
    #define __LICE__ACTION(comb) PLSolidPutFace<comb>::SolidGouraud(gmem,swidth,TriFace,alpha,zb,zfb_width);
    __LICE_ACTION_CONSTANTALPHA(mat->SolidCombineMode,alpha,true);
    #undef __LICE__ACTION
    return;
  }
#endif

#ifndef PLUSH_NO_SOLIDFLAT

  #define __LICE__ACTION(comb) PLSolidPutFace<comb>::Solid(gmem,swidth,TriFace,alpha,zb,zfb_width);
  __LICE_ACTION_CONSTANTALPHA(mat->SolidCombineMode,alpha,true);
  #undef __LICE__ACTION

#endif // PLUSH_NO_SOLIDFLAT
}


