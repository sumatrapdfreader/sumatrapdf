#ifndef _LICE_CURVE_RASTER_BUFFER_H_
#define _LICE_CURVE_RASTER_BUFFER_H_

class CurveRasterBuffer 
{
  static void CopyPixelNoClip(LICE_IBitmap *bmp, int x, int y, LICE_pixel col, double alpha)
  {
    LICE_pixel *p=bmp->getBits()+y*bmp->getRowSpan()+x;
    if (*p == col || alpha <= 0.0) return;

    if (alpha >= 1.0)
    {
      *p=col;
      return;
    }

    const int ia=256-(int)(alpha*256.0);
    int r=LICE_GETR(col), g=LICE_GETG(col), b=LICE_GETB(col);
    LICE_pixel_chan *c=(LICE_pixel_chan*)p;
    c[LICE_PIXEL_R]=r+((c[LICE_PIXEL_R]-r)*ia)/256;
    c[LICE_PIXEL_G]=g+((c[LICE_PIXEL_G]-g)*ia)/256;
    c[LICE_PIXEL_B]=b+((c[LICE_PIXEL_B]-b)*ia)/256;
  }

  static void DrawSlice(LICE_IBitmap* bmp, int x, double y1, double y2, int pxh, LICE_pixel color, float alpha)
  {
    const int iy1=(int)y1, iy2=(int)y2;

    if (iy1 == iy2)
    {
      if (iy1 >= 0 && iy1 < pxh) CopyPixelNoClip(bmp, x, iy1, color, (y2-y1)*alpha);
    }
    else
    {
      if (iy1 >= 0 && iy1 < pxh) CopyPixelNoClip(bmp, x, iy1, color, (1+iy1-y1)*alpha);

      if (iy2 > iy1+1)
      {
        int iy;
        const int n=min(iy2, pxh);
        for (iy=max(iy1+1, 0); iy < n; ++iy)
        { 
          CopyPixelNoClip(bmp, x, iy, color, alpha);
        }
      }

      if (iy2 >= 0 && iy2 < pxh) CopyPixelNoClip(bmp, x, iy2, color, (y2-iy2)*alpha);
    }
  }

  public:
    int xext[2],bmw;
    float *sb;

    CurveRasterBuffer(int w, WDL_TypedBuf<float> *sbuf) 
    { 
      xext[0]=bmw=w; 
      xext[1]=0; 
      sb = sbuf->ResizeOK(w*2,false); 
    }

    void addpt(int xpos, double ypos)
    {
      if (xpos >= 0 && xpos < bmw)
      {
        float *p = sb + xpos*2;
        const int ext0=xext[0], ext1=xext[1];
        if (ext0 <= ext1) // existing range
        {
          if (xpos < ext0) 
          {
            memset(p+2, 0, (ext0-xpos-1)*2*sizeof(*p));
            p[0]=p[1]=ypos;
            xext[0]=xpos;
          }
          else if (xpos > ext1) 
          {
            memset(sb + (ext1+1)*2, 0, (xpos-(ext1+1))*2*sizeof(*p));
            p[0]=p[1]=ypos;
            xext[1]=xpos;
          }
          else if (p[0] == 0.0 && p[1] == 0.0) p[0] = p[1] = ypos;
          else
          {
            if (ypos < p[0]) p[0] = ypos;
            else if (ypos > p[1]) p[1] = ypos;
          }
        }
        else // first point
        {
          xext[0]=xext[1] = xpos;
          p[0]=p[1]=ypos;
        }
      }
    }

    void Draw(LICE_IBitmap *bm, LICE_pixel color, float alpha)
    {
      int bmh = bm->getHeight();
      const int sc = (int) (INT_PTR)bm->Extended(LICE_EXT_GET_SCALING,NULL);
      if (sc > 256) bmh = bmh * sc / 256;
      const int xmax = xext[1];
      int x = xext[0];
      const float *sbuf = sb+x*2;
      for (; x <= xmax; x ++)
      {
        const double v1 = sbuf[0], v2 = sbuf[1];
        if (v2>v1) DrawSlice(bm, x,v1,v2, bmh, color,alpha);
        sbuf+= 2;
      }
    }
};


#endif
