/*
    WDL - virtwnd-slider.cpp
    Copyright (C) 2006 and later Cockos Incorporated

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
      

    Implementation for virtual window sliders.

*/


#if defined(_WIN32) && !defined(WINVER) && _MSC_VER <= 1200
  #define WINVER 0x500
#endif

#include <math.h>
#include "virtwnd-controls.h"
#include "../wdlcstring.h"
#include "../lice/lice.h"

void vwnd_slider_drawknobstack(LICE_IBitmap *drawbm, double val, WDL_VirtualWnd_BGCfg *knobimage, int ksw, int ksh, int ks_offs, int dx, int dy, int dw, int dh, float alpha)
{
  const bool v = knobimage->bgimage->getWidth() < knobimage->bgimage->getHeight();

  const int ni=((v ? knobimage->bgimage->getHeight() : knobimage->bgimage->getWidth())-ks_offs*2) / (v ? ksh : ksw);

  if (val<0.0)val=0.0;
  else if (val>1.0)val=1.0;
  int p=(int) (val * (ni-1));
  if (p<0) p=0;
  else if (p> ni-1) p=ni-1;

  p *= (v ? ksh : ksw);

  if (ks_offs && 
      knobimage->bgimage_lt_out[0] > 0 &&
      knobimage->bgimage_lt_out[1] > 0 && 
      knobimage->bgimage_rb_out[0] > 0 &&
      knobimage->bgimage_rb_out[1] > 0)
  {
    int l = knobimage->bgimage_lt_out[0]-1;
    int t = knobimage->bgimage_lt_out[1]-1;
    int r = knobimage->bgimage_rb_out[0]-1;
    int b = knobimage->bgimage_rb_out[1]-1;

    int ww = ksw - l - r;
    if (ww > 0)
    {
      dx -= (dw * l) / ww;
      dw = (dw * ksw) / ww;
    }
    int wh=ksh - t -b;
    if (wh)
    {
      dy -= (dh * t) / wh;
      dh = (dh * ksh) / wh;
    }
  }

  LICE_ScaledBlit(drawbm,knobimage->bgimage,dx,dy,dw,dh,
    (float) (ks_offs + (v?0:p)),(float)(ks_offs + (v?p:0)),(float)ksw,(float)ksh,alpha,LICE_BLIT_USE_ALPHA|LICE_BLIT_FILTER_BILINEAR);
}


WDL_VirtualWnd_BGCfg *vwnd_slider_getknobimageforsize(WDL_VirtualWnd_BGCfg *knoblist, int nknoblist,int *vieww, int *viewh, int *ksw, int *ksh, int *ks_offs)
{
  if (!knoblist) return NULL;
  WDL_VirtualWnd_BGCfg *knobimage=NULL;
  int x;
  int best_neww=*vieww, best_newh = *viewh;

  double bestdiff=0;
  double target_area_inv=1.0 / ((double)*vieww * *viewh);

  double target_aspect_inv = *viewh / (double) *vieww;

  for(x=0;x<nknoblist; x++)
  {
    if (knoblist[x].bgimage)
    {
      const int w=knoblist[x].bgimage->getWidth(), h=knoblist[x].bgimage->getHeight();
      const bool isVS = w < h;

      const int hasPink = knoblist[x].bgimage_lt[0] > 0 &&
                  knoblist[x].bgimage_lt[1] > 0 &&
                  knoblist[x].bgimage_rb[0] > 0 &&
                  knoblist[x].bgimage_rb[1] > 0;

      const int slice_w = isVS ? (w - hasPink*2) : 
            hasPink && knoblist[x].bgimage_lt[0]>1 ? (knoblist[x].bgimage_lt[0]-1 + (knoblist[x].bgimage_lt_out[0]>1 ? knoblist[x].bgimage_lt_out[0]-1:0)) : 
            (h-hasPink*2);
      const int slice_h = !isVS ? (h - hasPink*2) : 
            hasPink && knoblist[x].bgimage_lt[1]>1 ? (knoblist[x].bgimage_lt[1]-1  + (knoblist[x].bgimage_lt_out[1]>1 ? knoblist[x].bgimage_lt_out[1]-1:0)) : 
            (w-hasPink*2);


      int fmtw = slice_w, fmth=slice_h;
      if (hasPink && 
          knoblist[x].bgimage_lt_out[0] > 0 &&
          knoblist[x].bgimage_lt_out[1] > 0 && 
          knoblist[x].bgimage_rb_out[0] > 0 &&
          knoblist[x].bgimage_rb_out[1] > 0)
      {
        int l = knoblist[x].bgimage_lt_out[0]-1;
        int t = knoblist[x].bgimage_lt_out[1]-1;
        int r = knoblist[x].bgimage_rb_out[0]-1;
        int b = knoblist[x].bgimage_rb_out[1]-1;

        if (fmtw > l+r) fmtw -= l+r;
        if (fmth > b+t) fmth -= b+t;
      }

      // prioritize getting the aspect ratio right, then add target area differences -- this needs testing!
      double diff = ((double)fmtw / fmth) * target_aspect_inv;
      if (diff < 1.0) diff=1.0/diff;

      double diff2 = ((fmtw * (double)fmth) * target_area_inv);
      if (diff2 < 1.0) diff2=1.0/diff2;
      
      diff += diff2 * 0.01;     

      if (slice_w > 0 && slice_h > 0 && (!knobimage || bestdiff > diff))
      {
        knobimage=&knoblist[x];
        bestdiff=diff;
        *ksw = slice_w;
        *ksh = slice_h;
        *ks_offs = hasPink;

        int tmp=(fmtw * *viewh) / fmth;
        if (tmp <= *vieww)
        {
          best_neww = tmp;
          best_newh = *viewh;
        }
        else
        {
          best_neww = *vieww;
          best_newh = (fmth * *vieww) / fmtw;
        }
      }
    }
  }
  if (knobimage)
  {
    *vieww=best_neww;
    *viewh=best_newh;
  }
  return knobimage;
}


WDL_VirtualSlider::WDL_VirtualSlider()
{
  m_accessDescCopy=0;
  m_knob_lineextrasize=0;
  m_knobbias=0;
  m_zl_color = m_knob_color=0;
  m_is_knob=false;
  m_tl_extra=m_br_extra=0;
  m_skininfo=0;
  m_bgcol1_msg=0;
  m_minr=0;
  m_maxr=1000;
  m_needflush=0;
  m_pos=m_center=500;
  m_captured=false;
  m_grayed = false;
  m_knobbg[0]=m_knobbg[1]=0;
  m_knobstacks=0;
  m_nknobstacks=0;
  m_sendmsgonclick=false;
  m_dblclickmsg=0;
  m_last_rscale = WDL_VWND_SCALEBASE;
  m_last_advscale = 0;
}

WDL_VirtualSlider::~WDL_VirtualSlider()
{
  free(m_accessDescCopy);
}

bool WDL_VirtualSlider::GetIsVert()
{
  return m_position.right-m_position.left < m_position.bottom-m_position.top;
}

static int InvScaleVal(int sc, int v) {
  return sc>0 && sc != 256 ? (v*256)/sc : v;
}
static void ScaleVals(int sc, int *a, int *b, int *c)
{
  if (sc && sc != 256)
  {
    if (a) *a = (*a * sc)/256;
    if (b) *b = (*b * sc)/256;
    if (c) *c = (*c * sc)/256;
  }
}
static void AdjustThumbImageSize(int sc, int wndw, int wndh, WDL_VirtualSlider_SkinConfig *a, bool vert, int *bmw, int *bmh, int *startoffs=NULL, bool *want_knob=NULL, int knob_bias=0)
{
  if (knob_bias<=0)
  {
    wndw = InvScaleVal(sc,wndw);
    wndh = InvScaleVal(sc,wndh);
  }
  if (want_knob) *want_knob=knob_bias>0;
  if (a) 
  {
    int ret=a->thumbimage_rb[vert] - a->thumbimage_lt[vert];
    if (ret>0)
    {
      if (startoffs) *startoffs = a->thumbimage_lt[vert];
      if (vert)
      {
        if (*bmh > ret) (*bmw)--;
        *bmh=ret;
      }
      else 
      {
        if (*bmw > ret) (*bmh)--;
        *bmw=ret;
      }
    }
  }
  if (vert)
  {
    if (want_knob && *bmh >= wndh*3/4 && !knob_bias) *want_knob=true;
    if (*bmh > wndh/2)
    {
      if (startoffs) *startoffs += (*bmh - wndh/2)/2;
      *bmh=wndh/2;
    }
  }
  else
  {
    if (want_knob && *bmw >= wndw*3/4 && !knob_bias) *want_knob=true;
    if (*bmw > wndw/2)
    {
      if (startoffs) *startoffs += (*bmw - wndw/2)/2;
      *bmw=wndw/2;
    }
  }
}

void WDL_VirtualSlider_PreprocessSkinConfig(WDL_VirtualSlider_SkinConfig *a)
{
  if (!a) return;

  if (a->thumbimage[0])
  {
    int w=a->thumbimage[0]->getWidth();
    int h=a->thumbimage[0]->getHeight();
    int x;
    for (x = 0; x < w && LICE_GetPixel(a->thumbimage[0],x,h-1)==LICE_RGBA(255,0,255,255); x ++);
    a->thumbimage_lt[0] = x;
    for (x = w-1; x > a->thumbimage_lt[0]+1 && LICE_GetPixel(a->thumbimage[0],x,h-1)==LICE_RGBA(255,0,255,255); x --);
    a->thumbimage_rb[0] = x;
  }
  if (a->thumbimage[1])
  {
    int w=a->thumbimage[1]->getWidth();
    int h=a->thumbimage[1]->getHeight();
    int y;
    for (y = 0; y < h-4 && LICE_GetPixel(a->thumbimage[1],w-1,y)==LICE_RGBA(255,0,255,255); y++);
    a->thumbimage_lt[1] = y;
    for (y = h-1; y > a->thumbimage_lt[1]+1 && LICE_GetPixel(a->thumbimage[1],w-1,y)==LICE_RGBA(255,0,255,255); y --);
    a->thumbimage_rb[1] = y;
  }
  WDL_VirtualWnd_PreprocessBGConfig(&a->bgimagecfg[0]);
  WDL_VirtualWnd_PreprocessBGConfig(&a->bgimagecfg[1]);
}

void WDL_VirtualSlider::GetButtonSize(int *w, int *h, int rscale)
{
  if (m_is_knob)
  {
    *w=*h=0;
    return;
  }
  RECT mp;
  WDL_VWnd::GetPositionPaintExtent(&mp,rscale);
  bool isVert = GetIsVert();
  LICE_IBitmap *bm_image=m_skininfo ? m_skininfo->thumbimage[isVert] : 0;
  if (bm_image)
  {
    *w = bm_image->getWidth();
    *h = bm_image->getHeight();
    AdjustThumbImageSize(m_last_advscale,mp.right-mp.left,mp.bottom-mp.top,m_skininfo,isVert,w,h);
  }
  else
  {
    bm_image=WDL_STYLE_GetSliderBitmap2(isVert);
    if (bm_image)
    {
      *w=bm_image->getWidth();
      *h=bm_image->getHeight();
    }
    else *w=*h=16;
    AdjustThumbImageSize(m_last_advscale,mp.right-mp.left,mp.bottom-mp.top,NULL,isVert,w,h);
  }
  ScaleVals(m_last_advscale,w,h,NULL);
}

WDL_VirtualWnd_BGCfg *WDL_VirtualSlider::getKnobBackgroundForSize(int sz) const
{ 
  int h1 = 100000, h2 = 100000;
  if (m_knobbg[0] && m_knobbg[0]->bgimage)
  { 
    h1 = m_knobbg[0]->bgimage->getHeight();
    if (m_knobbg[0]->bgimage_lt[1]>0 && m_knobbg[0]->bgimage_rb[1]>0)
    {
      if (m_knobbg[0]->bgimage_lt_out[0]>0 && m_knobbg[0]->bgimage_lt_out[1]>0 &&
          m_knobbg[0]->bgimage_rb_out[0]>0 && m_knobbg[0]->bgimage_rb_out[1]>0)
        h1 -= m_knobbg[0]->bgimage_lt_out[1] + m_knobbg[0]->bgimage_rb_out[1];
      else 
        h1 -= 2;
    }
  }
  if (m_knobbg[1] && m_knobbg[1]->bgimage)
  {
    h2 = m_knobbg[1]->bgimage->getHeight();
    if (m_knobbg[1]->bgimage_lt[1]>0 && m_knobbg[1]->bgimage_rb[1]>0)
    {
      if (m_knobbg[1]->bgimage_lt_out[0]>0 && m_knobbg[1]->bgimage_lt_out[1]>0 &&
          m_knobbg[1]->bgimage_rb_out[0]>0 && m_knobbg[1]->bgimage_rb_out[1]>0)
        h2 -= m_knobbg[1]->bgimage_lt_out[1] + m_knobbg[1]->bgimage_rb_out[1];
      else 
        h2 -= 2;
    }
  }

  // don't scale down
  h1 -= sz; if (h1<0) h1=-h1;
  h2 -= sz; if (h2<0) h2=-h2;
  WDL_VirtualWnd_BGCfg *bg = m_knobbg[h1 > h2];
  return bg && bg->bgimage ? bg : NULL;
}

void WDL_VirtualSlider::OnPaint(LICE_IBitmap *drawbm, int origin_x, int origin_y, RECT *cliprect, int rscale)
{
  RECT mp;
  m_last_advscale = drawbm ? (int)drawbm->Extended(LICE_EXT_GET_ADVISORY_SCALING,NULL) : 0;
  m_last_rscale=rscale;
  WDL_VWnd::GetPositionPaintExtent(&mp,rscale);
  origin_x += mp.left; // convert drawing origin to local coords
  origin_y += mp.top;

  bool isVert = GetIsVert();

  int rsize=m_maxr-m_minr;
  if (rsize<1)rsize=1;

  int viewh=mp.bottom-mp.top;
  int vieww=mp.right-mp.left;

  WDL_VirtualWnd_BGCfg *back_image=m_skininfo && m_skininfo->bgimagecfg[isVert].bgimage ? &m_skininfo->bgimagecfg[isVert] : 0;
  LICE_IBitmap *bm_image=m_skininfo ? m_skininfo->thumbimage[isVert] : 0;
  int bm_w=16,bm_h=16,bm_w2=16,bm_h2=16;
  int imgoffset=0;
  bool wantKnob=false;
  if (bm_image)
  {
    bm_w2=bm_w=bm_image->getWidth();
    bm_h2=bm_h=bm_image->getHeight();
    AdjustThumbImageSize(m_last_advscale,vieww,viewh,m_skininfo,isVert,&bm_w2,&bm_h2,&imgoffset,&wantKnob,m_knobbias);
  }
  else
  {
    bm_image=WDL_STYLE_GetSliderBitmap2(isVert);
    if (bm_image)
    {
      bm_w2=bm_w=bm_image->getWidth();
      bm_h2=bm_h=bm_image->getHeight();
    }
    AdjustThumbImageSize(m_last_advscale,vieww,viewh,NULL,isVert,&bm_w2,&bm_h2,&imgoffset,&wantKnob,m_knobbias);
  }
  const int bm_w_src = isVert?bm_w2:bm_w, bm_h_src = isVert ? bm_h:bm_h2;
  if (!wantKnob)
  {
    ScaleVals(m_last_advscale,&bm_w2,&bm_h2,&imgoffset);
    ScaleVals(m_last_advscale,&bm_w,&bm_h,NULL);
  }

  float alpha = (m_grayed ? 0.25f : 1.0f);

  m_is_knob = wantKnob;

  if (isVert||wantKnob)
  {
    int pos = ((m_maxr-m_pos)*(viewh-bm_h2))/rsize; //viewh - bm_h2 - ((m_pos-m_minr) * (viewh - bm_h2))/rsize;

    const int old_vieww=vieww, old_viewh=viewh, old_origin_x=origin_x, old_origin_y=origin_y;

    if (wantKnob)
    {
      int sz= wdl_min(vieww,viewh);
      origin_x += (vieww-sz)/2;
      origin_y += (viewh-sz)/2;
      vieww = viewh = sz;       

      back_image = getKnobBackgroundForSize(InvScaleVal(m_last_advscale,sz));
    }


    if (back_image)
    {
      WDL_VirtualWnd_ScaledBlitBG(drawbm,back_image,
        origin_x,origin_y,vieww,viewh,
        origin_x,origin_y,vieww,viewh,
        1.0f,LICE_BLIT_MODE_COPY|LICE_BLIT_FILTER_BILINEAR|LICE_BLIT_USE_ALPHA);
                                            
      if (m_bgcol1_msg)
      {
        int brcol=-100;
        SendCommand(m_bgcol1_msg,(INT_PTR)&brcol,GetID(),this);
        if (brcol != -100)
        {
          static LICE_SysBitmap tmpbm;//not threadsafe
          tmpbm.resize(vieww,viewh);
          tmpbm.Extended(LICE_EXT_SET_ADVISORY_SCALING,&m_last_advscale);

          WDL_VirtualWnd_ScaledBlitBG(&tmpbm,back_image,0,0,vieww,viewh,
              0,0,vieww,viewh,1.0f,LICE_BLIT_MODE_COPY|LICE_BLIT_FILTER_BILINEAR);

          LICE_ClearRect(&tmpbm,0,0,vieww,viewh,LICE_RGBA(0,0,0,255),LICE_RGBA(GetRValue(brcol),GetGValue(brcol),GetBValue(brcol),0));

          RECT r={0,0,vieww,viewh};
          LICE_Blit(drawbm,&tmpbm,origin_x,origin_y,&r,0.5,LICE_BLIT_MODE_COPY|LICE_BLIT_USE_ALPHA);
        }
      }
    }

    if (!wantKnob)
    {
      int zlc = m_zl_color;
      if (!zlc && m_skininfo) zlc = m_skininfo->zeroline_color;
      if (!back_image || zlc)
      {
        int center=m_center;
        if (center < 0) center=WDL_STYLE_GetSliderDynamicCenterPos();

        int y=((m_maxr-center)*(viewh-bm_h2))/rsize + ((bm_h-1)/2-imgoffset);

        if (!zlc) zlc = LICE_RGBA_FROMNATIVE(GSC(COLOR_BTNTEXT),255);
        LICE_Line(drawbm,origin_x+2,origin_y+y,origin_x+vieww-2,origin_y+y, zlc, LICE_GETA(zlc)/255.0f, LICE_BLIT_MODE_COPY,false);
      }


      if (!back_image)
      {

        LICE_pixel fgcol  = GSC(COLOR_3DHILIGHT);
        fgcol = LICE_RGBA_FROMNATIVE(fgcol,255);
        LICE_pixel bgcol=GSC(COLOR_3DSHADOW);
        if (m_bgcol1_msg)
          SendCommand(m_bgcol1_msg,(INT_PTR)&bgcol,GetID(),this);
        bgcol = LICE_RGBA_FROMNATIVE(bgcol,255);


        int offs= (vieww - 4)/2;
        // white with black border, mmm

        RECT r={origin_x + offs,origin_y + bm_h2/3, origin_x + offs + 5,origin_y + viewh - bm_h2/3};

        LICE_FillRect(drawbm,r.left+1,r.top+1,
                             r.right-r.left-2,r.bottom-r.top-2,bgcol,1.0f,LICE_BLIT_MODE_COPY);

        LICE_Line(drawbm,r.left+1,r.top,r.right-2,r.top,fgcol,1.0f,LICE_BLIT_MODE_COPY,false);
        LICE_Line(drawbm,r.left+1,r.bottom-1,r.right-2,r.bottom-1,fgcol,1.0f,LICE_BLIT_MODE_COPY,false);

        LICE_Line(drawbm,r.left,r.top+1,r.left,r.bottom-2,fgcol,1.0f,LICE_BLIT_MODE_COPY,false);
        LICE_Line(drawbm,r.right-1,r.top+1,r.right-1,r.bottom-2,fgcol,1.0f,LICE_BLIT_MODE_COPY,false);    

      }

      if (bm_image)
      {
        int ypos=origin_y+pos-imgoffset;
        int xpos=origin_x;

        xpos+=(vieww-bm_w2)/2;

        m_tl_extra=origin_y-ypos;
        if (m_tl_extra<0)m_tl_extra=0;

        m_br_extra=ypos+bm_h - (origin_y+mp.bottom-mp.top);
        if (m_br_extra<0)m_br_extra=0;


        LICE_ScaledBlit(drawbm,bm_image,xpos,ypos,bm_w2,bm_h, 0,0, bm_w_src,bm_h_src,alpha,LICE_BLIT_MODE_COPY|LICE_BLIT_USE_ALPHA|LICE_BLIT_FILTER_BILINEAR);
      }
    }
    else 
    {
      int ksw=0,ksh=0;
      WDL_VirtualWnd_BGCfg *knobimage = NULL;
      int ks_offs=0;
      int vw = old_vieww, vh=old_viewh;
      knobimage = vwnd_slider_getknobimageforsize(m_knobstacks,m_nknobstacks,&vw,&vh,&ksw, &ksh,&ks_offs);

      float val;
      int center=m_center;
      if (center < 0) center=WDL_STYLE_GetSliderDynamicCenterPos();
      if (center > m_minr && (m_pos < center || center >= m_maxr)) val = (m_pos-center) / (float)(center-m_minr);
      else val = (m_pos-center) / (float)(m_maxr-center);

      if (knobimage && ksw>0 && ksh>0)
      {
        vwnd_slider_drawknobstack(drawbm,(val+1.0)*0.5,knobimage,ksw,ksh,ks_offs,
          old_origin_x + (old_vieww - vw)/2,old_origin_y+(old_viewh - vh)/2,vw,vh,alpha
          );
      }
      else
      {
        LICE_pixel col  = m_knob_color ? m_knob_color : LICE_RGBA_FROMNATIVE(GSC(COLOR_3DHILIGHT),255);

        float lalpha = LICE_GETA(col)/255.0f;
        int cx=origin_x+vieww/2;
        int cy=origin_y+viewh/2;
        float rd = (float) (vieww/2-4 + m_knob_lineextrasize);
        float r2=rd*0.125f;
        if (!back_image) LICE_Circle(drawbm, (float)cx, (float)cy, rd, col, lalpha, LICE_BLIT_MODE_COPY, true);
      
        #define KNOBANGLE_MAX (3.14159*7.0/8.0);
        float a = val*(float)KNOBANGLE_MAX;
        float sina=sin(a);
        float cosa=cos(a);
        float x1=cx+r2*sina;
        float y1=cy-r2*cosa;
        float x2=cx+rd*sina;
        float y2=cy-rd*cosa;
        LICE_FLine(drawbm, x1, y1, x2, y2, col, lalpha, LICE_BLIT_MODE_COPY, true);
      }
    }
  }
  else
  {
    int pos = ((m_pos-m_minr) * (vieww - bm_w2))/rsize;

    if (back_image)
    {
      WDL_VirtualWnd_ScaledBlitBG(drawbm,back_image,
        origin_x,origin_y,vieww,viewh,
        origin_x,origin_y,vieww,viewh,
        1.0,LICE_BLIT_MODE_COPY|LICE_BLIT_FILTER_BILINEAR|LICE_BLIT_USE_ALPHA);
      // blit, tint color too?

      if (m_bgcol1_msg)
      {
        int brcol=-100;
        SendCommand(m_bgcol1_msg,(INT_PTR)&brcol,GetID(),this);
        if (brcol != -100)
        {
          static LICE_SysBitmap tmpbm; //not threadsafe
          tmpbm.resize(vieww,viewh);
          tmpbm.Extended(LICE_EXT_SET_ADVISORY_SCALING,&m_last_advscale);

          WDL_VirtualWnd_ScaledBlitBG(&tmpbm,back_image,0,0,vieww,viewh,
              0,0,vieww,viewh,1.0,LICE_BLIT_MODE_COPY|LICE_BLIT_FILTER_BILINEAR);

          LICE_ClearRect(&tmpbm,0,0,vieww,viewh,LICE_RGBA(0,0,0,255),LICE_RGBA(GetRValue(brcol),GetGValue(brcol),GetBValue(brcol),0));

          RECT r={0,0,vieww,viewh};
          LICE_Blit(drawbm,&tmpbm,origin_x,origin_y,&r,0.5,LICE_BLIT_MODE_COPY|LICE_BLIT_USE_ALPHA);
        }
      }

    }

    int zlc = m_zl_color;
    if (!zlc && m_skininfo) zlc = m_skininfo->zeroline_color;
    if (!back_image || zlc)
    {
      int center=m_center;
      if (center < 0) center=WDL_STYLE_GetSliderDynamicCenterPos();

      int x=((center-m_minr)*(vieww-bm_w2))/rsize + bm_w/2 - imgoffset;

      if (!zlc) zlc = LICE_RGBA_FROMNATIVE(GSC(COLOR_BTNTEXT),255);

      LICE_Line(drawbm,origin_x+x,origin_y+2,origin_x+x,origin_y+viewh-2,
        zlc, LICE_GETA(zlc)/255.0f, LICE_BLIT_MODE_COPY,false);
    }

    if (!back_image)
    {
      LICE_pixel fgcol  = GSC(COLOR_3DHILIGHT);
      fgcol = LICE_RGBA_FROMNATIVE(fgcol,255);
      LICE_pixel bgcol=GSC(COLOR_3DSHADOW);
      if (m_bgcol1_msg)
        SendCommand(m_bgcol1_msg,(INT_PTR)&bgcol,GetID(),this);
      bgcol = LICE_RGBA_FROMNATIVE(bgcol,255);

      int offs= (viewh - 4)/2;
      // white with black border, mmm
      RECT r={origin_x + bm_w2/3,origin_y + offs, origin_x + vieww - bm_w2/3,origin_y + offs + 5};

      LICE_FillRect(drawbm,r.left+1,r.top+1,
                           r.right-r.left-2,r.bottom-r.top-2,bgcol,1.0f,LICE_BLIT_MODE_COPY);

      LICE_Line(drawbm,r.left+1,r.top,r.right-2,r.top,fgcol,1.0f,LICE_BLIT_MODE_COPY,false);
      LICE_Line(drawbm,r.left+1,r.bottom-1,r.right-2,r.bottom-1,fgcol,1.0f,LICE_BLIT_MODE_COPY,false);

      LICE_Line(drawbm,r.left,r.top+1,r.left,r.bottom-2,fgcol,1.0f,LICE_BLIT_MODE_COPY,false);
      LICE_Line(drawbm,r.right-1,r.top+1,r.right-1,r.bottom-2,fgcol,1.0f,LICE_BLIT_MODE_COPY,false);    

    }

    if (bm_image)
    {
      int xpos=origin_x+pos-imgoffset;
      int ypos=origin_y;

      ypos+=(viewh-bm_h2)/2;

      m_tl_extra=origin_x-xpos;
      if (m_tl_extra<0)m_tl_extra=0;

      m_br_extra=xpos+bm_w - (origin_x+mp.right-mp.left);
      if (m_br_extra<0)m_br_extra=0;

      LICE_ScaledBlit(drawbm,bm_image,xpos,ypos,bm_w,bm_h2,0,0,bm_w_src,bm_h_src,alpha,LICE_BLIT_MODE_COPY|LICE_BLIT_USE_ALPHA|LICE_BLIT_FILTER_BILINEAR);
    }
  }
}

static double m_move_offset;
static int m_click_pos,m_last_y,m_last_x, m_last_precmode;
#if !defined(_WIN32) && !defined(__APPLE__)
static POINT s_lastmousepos;
#endif


int WDL_VirtualSlider::OnMouseDown(int xpos, int ypos)
{
  if (m_grayed) return 0;
  m_needflush=0;

  if (m__iaccess) m__iaccess->OnFocused();

  bool isVert = GetIsVert();
  int rsize=m_maxr-m_minr;
  if (rsize<1)rsize=1;

  RECT mp = m_position;
  const int rscale = m_last_rscale;
  ScaleRect(&mp,rscale); // silliness is that thumb images don't resize

  int viewh=mp.bottom-mp.top;
  int vieww=mp.right-mp.left;
  if (vieww<1) vieww=1;
  if (viewh<1) viewh=1;

  LICE_IBitmap *bm_image=m_skininfo ? m_skininfo->thumbimage[isVert] : 0;
  int bm_w=16,bm_h=16;
  bool wantKnob=false;
  if (bm_image)
  {
    bm_w=bm_image->getWidth();
    bm_h=bm_image->getHeight();
    AdjustThumbImageSize(m_last_advscale,vieww,viewh,m_skininfo,isVert,&bm_w,&bm_h,NULL,&wantKnob,m_knobbias);
  }
  else
  {
    bm_image=WDL_STYLE_GetSliderBitmap2(isVert);
    if (bm_image)
    {
      bm_w=bm_image->getWidth();
      bm_h=bm_image->getHeight();
    }
    AdjustThumbImageSize(m_last_advscale,vieww,viewh,NULL,isVert,&bm_w,&bm_h,NULL,&wantKnob,m_knobbias);
  }
  if (!wantKnob) ScaleVals(m_last_advscale,&bm_w,&bm_h,NULL);

  m_is_knob = wantKnob;

#if !defined(_WIN32) && !defined(__APPLE__)
  GetCursorPos(&s_lastmousepos);
#endif
  m_last_y=ypos;    
  m_last_x=xpos;
  m_last_precmode=0;

  xpos = xpos * rscale / WDL_VWND_SCALEBASE;
  ypos = ypos * rscale / WDL_VWND_SCALEBASE;

  bool needsendcmd = m_sendmsgonclick;
  if (m_is_knob)
  {
    m_move_offset=0;
    m_click_pos=m_pos;
  }
  else if (isVert)
  {
    m_move_offset=ypos-( viewh - bm_h - ((double)((m_pos-m_minr) * (viewh-bm_h))/(double)rsize));
    m_click_pos=m_pos;
    if (!m_is_knob && (m_move_offset < 0 || m_move_offset >= bm_h))
    {
      int xcent=xpos - vieww/2;
      bool hit;

      if (m_skininfo && m_skininfo->bgimagecfg[1].bgimage)
      {
        LICE_pixel pix=WDL_VirtualWnd_ScaledBG_GetPix(&m_skininfo->bgimagecfg[1],
          vieww,viewh,xpos,ypos);

        hit = LICE_GETA(pix)>=64;
      }
      else hit= (xcent >= -2 && xcent < 3 && ypos >= bm_h/3 && ypos <= viewh-bm_h/3);

      if (hit)
      {
        m_move_offset=bm_h/2;
        int pos=(int)(m_minr+((viewh-bm_h - (ypos-m_move_offset))*rsize)/(double)(viewh-bm_h));
        if (pos < m_minr)pos=m_minr;
        else if (pos > m_maxr)pos=m_maxr;
        m_pos=pos;

        needsendcmd=false;
        WDL_VWND_DCHK(chk);
        SendCommand(m_scrollmsg?m_scrollmsg:WM_VSCROLL,SB_THUMBTRACK,GetID(),this);
        if (chk.isOK()) 
        {
          RequestRedraw(NULL);
          if (m__iaccess) m__iaccess->OnStateChange();
        }
      }
      else return 0;
    }
  }
  else
  {
    m_move_offset=xpos-( ((double)((m_pos-m_minr) * (vieww-bm_w))/(double)rsize));
    m_click_pos=m_pos;
    if (m_move_offset < 0 || m_move_offset >= bm_w)
    {
      int ycent=ypos - viewh/2;

      bool hit;
      if (m_skininfo && m_skininfo->bgimagecfg[0].bgimage)
      {
        LICE_pixel pix=WDL_VirtualWnd_ScaledBG_GetPix(&m_skininfo->bgimagecfg[0],
          vieww,viewh,xpos,ypos);

        hit = LICE_GETA(pix)>=64;
      }
      else hit = (ycent >= -2 && ycent < 3 && xpos >= bm_w/3 && xpos <= vieww-bm_w/3);

      if (hit)
      {
        m_move_offset=bm_w/2;
        int pos=(int) (m_minr+((xpos-m_move_offset)*rsize)/(double)(vieww-bm_w));
        if (pos < m_minr)pos=m_minr;
        else if (pos > m_maxr)pos=m_maxr;
        m_pos=pos;

        needsendcmd=false;
        WDL_VWND_DCHK(chk);
        SendCommand(m_scrollmsg?m_scrollmsg:WM_HSCROLL,SB_THUMBTRACK,GetID(),this);
        if (chk.isOK())
        {
          RequestRedraw(NULL);
          if (m__iaccess) m__iaccess->OnStateChange();
        }
      }
      else return 0;
    }
  }

  m_captured=true;
  if (needsendcmd)
  {
    WDL_VWND_DCHK(chk);
    SendCommand(m_scrollmsg?m_scrollmsg:WM_VSCROLL,SB_THUMBTRACK,GetID(),this);
    if (chk.isOK() && m__iaccess) m__iaccess->OnStateChange();
  }
  return 1;
}


void WDL_VirtualSlider::OnMoveOrUp(int xpos, int ypos, int isup)
{
  int pos;
  bool isVert = GetIsVert();
  int rsize=m_maxr-m_minr;
  if (rsize<1)rsize=1;

  const int rscale = m_last_rscale;
  RECT mp = m_position;
  ScaleRect(&mp,rscale); // silliness is that thumb images don't resize
  int viewh=mp.bottom-mp.top;
  int vieww=mp.right-mp.left;

  LICE_IBitmap *bm_image=m_skininfo ? m_skininfo->thumbimage[isVert] : 0;
  int bm_w=16,bm_h=16;
  if (bm_image)
  {
    bm_w=bm_image->getWidth();
    bm_h=bm_image->getHeight();
    AdjustThumbImageSize(m_last_advscale,vieww,viewh,m_skininfo,isVert,&bm_w,&bm_h);
  }
  else
  {
    bm_image=WDL_STYLE_GetSliderBitmap2(isVert);
    if (bm_image)
    {
      bm_w=bm_image->getWidth();
      bm_h=bm_image->getHeight();
    }
    AdjustThumbImageSize(m_last_advscale,vieww,viewh,NULL,isVert,&bm_w,&bm_h);
  }

  int precmode=0;
  if (m_is_knob) isVert=true;
  else ScaleVals(m_last_advscale, &bm_w, &bm_h, NULL);

  if (isVert)
  {
#ifndef _WIN32
    if (isup) pos=m_pos;
    else 
#endif
      if (viewh <= bm_h || m_is_knob || (GetAsyncKeyState(VK_CONTROL)&0x8000))
    {
        int sc=m_is_knob && !(GetAsyncKeyState(VK_CONTROL)&0x8000)?4:1;
        pos = m_pos- ((ypos-m_last_y) - (m_is_knob ?xpos-m_last_x:0))*sc;
      precmode=1;
    }
    else 
    {
      pos=m_minr + (int) (((double)(viewh-bm_h - ypos * rscale / WDL_VWND_SCALEBASE + m_move_offset)*(double)rsize)/(double)(viewh-bm_h));
    }
    if (pos < m_minr)pos=m_minr;
    else if (pos > m_maxr)pos=m_maxr;
    

    if (pos != m_pos || isup)
    {
      if (ypos == m_last_y&&(m_is_knob && xpos==m_last_x))
        pos=m_pos;

      if ((GetAsyncKeyState(VK_MENU)&0x8000) && isup)
        pos=m_click_pos;

      m_pos=pos;

      if (isup || ypos != m_last_y||(m_is_knob&&xpos!=m_last_x))
      {
        WDL_VWND_DCHK(chk);
        SendCommand(m_scrollmsg?m_scrollmsg:WM_VSCROLL,isup?SB_ENDSCROLL:SB_THUMBTRACK,GetID(),this);
        if (chk.isOK())
        {
          RequestRedraw(NULL);
          if (m__iaccess) m__iaccess->OnStateChange();
        }
      }
    }
  }
  else
  {
#ifndef _WIN32
    if (isup) pos=m_pos;
    else 
#endif
      if ((GetAsyncKeyState(VK_CONTROL)&0x8000) || vieww <= bm_w || m_is_knob)
    {
      pos = m_pos+ (xpos-m_last_x);
      precmode=1;
    }
    else 
    {
      pos=m_minr + (int) (((double)(xpos * rscale / WDL_VWND_SCALEBASE - m_move_offset)*(double)rsize)/(double)(vieww-bm_w));
    }
    if (pos < m_minr)pos=m_minr;
    else if (pos > m_maxr)pos=m_maxr;

    if (pos != m_pos || isup)
    {
      if (xpos == m_last_x)
        pos=m_pos;

      if ((GetAsyncKeyState(VK_MENU)&0x8000) && isup)
        pos=m_click_pos;

      m_pos=pos;

      if (isup || xpos != m_last_x)
      {
        WDL_VWND_DCHK(chk);
        SendCommand(m_scrollmsg?m_scrollmsg:WM_HSCROLL,isup?SB_ENDSCROLL:SB_THUMBTRACK,GetID(),this);
        if (chk.isOK())
        {
          RequestRedraw(NULL);
          if (m__iaccess) m__iaccess->OnStateChange();
        }
      }
    }
  }
  if (precmode&&GetRealParent())
  {
    if (xpos != m_last_x || ypos != m_last_y)
    {
      POINT p;
#if !defined(_WIN32) && !defined(__APPLE__)
      p = s_lastmousepos;
#else
      GetCursorPos(&p);
      p.x-=(xpos-m_last_x);
#ifdef _WIN32
      p.y-=(ypos-m_last_y);
#else
      p.y+=(ypos-m_last_y);
#endif

#endif
      
      
    #ifdef _WIN32
      if (!m_is_knob)
      {
        POINT pt={0,0};
        ClientToScreen(GetRealParent(),&pt);
        WDL_VWnd *wnd=this;
        while (wnd)
        {
          RECT r;
          wnd->GetPosition(&r);
          pt.x+=r.left;
          pt.y+=r.top;
          wnd=wnd->GetParent();
        }

        if (isVert) 
        {
          m_last_y=(int) ((viewh - bm_h - (((m_pos-m_minr) * (viewh-bm_h))/rsize))+m_move_offset);
          p.y=m_last_y+pt.y;
        }
        else 
        {
          m_last_x= (int) (((((m_pos-m_minr) * (vieww-bm_w))/rsize))+m_move_offset);
          p.x=m_last_x+pt.x;
        }
      }
    #endif
      
#ifdef _WIN32
      CURSORINFO ci={sizeof(ci)};
      GetCursorInfo(&ci);
#endif


      if (
        wdl_virtwnd_nosetcursorpos ||
#ifdef _WIN32
        (ci.flags&2) ||
#endif
        !SetCursorPos(p.x,p.y)) 
      {
        m_last_y = ypos;
        m_last_x = xpos;
#if !defined(_WIN32) && !defined(__APPLE__)
        GetCursorPos(&s_lastmousepos);
#endif
      }
      else
      {
#ifdef _WIN32
        POINT p2;
        GetCursorPos(&p2);
        m_last_x += p2.x - p.x; // win10 hidpi workaround
        m_last_y += p2.y - p.y;
#endif
      }
    }
    do m_last_precmode++; while (ShowCursor(FALSE)>=0);
  }
  else
  {
    m_last_y=ypos;
    m_last_x=xpos;
    while (m_last_precmode>0) {m_last_precmode--; ShowCursor(TRUE); }
#if !defined(_WIN32) && !defined(__APPLE__)
    GetCursorPos(&s_lastmousepos);
#endif
  }
  m_needflush=0;
}

void WDL_VirtualSlider::OnMouseMove(int xpos, int ypos)
{
  if (m_grayed) return;

  if (m_captured) OnMoveOrUp(xpos,ypos,0);
  else if (m_needflush && (xpos <0  || xpos > m_position.right- m_position.left || ypos < 0|| ypos > m_position.bottom-m_position.top ))
  {
    bool isVert = GetIsVert();
    m_needflush=0;
    WDL_VWND_DCHK(chk);
    SendCommand(m_scrollmsg?m_scrollmsg:(isVert?WM_VSCROLL:WM_HSCROLL),SB_ENDSCROLL,GetID(),this);
    if (chk.isOK() && m__iaccess) m__iaccess->OnStateChange();
  }
}

void WDL_VirtualSlider::OnCaptureLost()
{
  m_captured=false;
  while (m_last_precmode>0) {m_last_precmode--; ShowCursor(TRUE); }
}

void WDL_VirtualSlider::OnMouseUp(int xpos, int ypos)
{
  if (m_grayed) return;

  if (m_captured) 
  {
    OnMoveOrUp(xpos,ypos,1);
    while (m_last_precmode>0) {m_last_precmode--; ShowCursor(TRUE); }
  }
  m_captured=false;
}

bool WDL_VirtualSlider::OnMouseDblClick(int xpos, int ypos)
{
  if (m_grayed) return false;

  if (m_dblclickmsg)
  {
    SendCommand(m_dblclickmsg, 0, 0, this);
    return true;
  }

  bool isVert = GetIsVert();
  int pos=m_center;
  if (pos < 0) pos=WDL_STYLE_GetSliderDynamicCenterPos();
  m_pos=pos;
  WDL_VWND_DCHK(chk);
  SendCommand(m_scrollmsg?m_scrollmsg:(isVert?WM_VSCROLL:WM_HSCROLL),SB_ENDSCROLL,GetID(),this);

  if (chk.isOK())
  {
    RequestRedraw(NULL);
    if (m__iaccess) m__iaccess->OnStateChange();
  }

  m_captured=false;
  return true;
}

bool WDL_VirtualSlider::OnMouseWheel(int xpos, int ypos, int amt)
{
  if (m_grayed) return false;

  if (xpos != -100 || ypos != -100) // xpos=ypos=-100 used by virtwnd-nsaccessibility
  {
    if (!WDL_STYLE_AllowSliderMouseWheel()) return false;
  }

  bool isVert = GetIsVert();
	int l=amt;
  if (!(GetAsyncKeyState(VK_CONTROL)&0x8000)) l *= 16;
  l *= (m_maxr-m_minr);
  l/=120000;
  if (!l) { if (amt<0)l=-1; else if (amt>0) l=1; }

  int pos=m_pos+l;
  if (pos < m_minr)pos=m_minr;
  else if (pos > m_maxr)pos=m_maxr;

  m_pos=pos;

  m_needflush=1;
  WDL_VWND_DCHK(chk);
  SendCommand(m_scrollmsg?m_scrollmsg:(isVert?WM_VSCROLL:WM_HSCROLL),SB_THUMBTRACK,GetID(),this);

  if (chk.isOK())
  {
    RequestRedraw(NULL);
    if (m__iaccess) m__iaccess->OnStateChange();
  }
  return true;
}




int WDL_VirtualSlider::GetSliderPosition() 
{
  if (m_pos < m_minr) return m_minr; 
  if (m_pos > m_maxr) return m_maxr; 
  return m_pos; 
}

bool WDL_VirtualSlider::GetAccessValueDesc(char *buf, int bufsz)
{
  if (m_valueText.GetLength())
  {
    if (!strcmp(m_valueText.Get(),"#"))
    {
      snprintf(buf,bufsz,"%d",m_pos);
      return true;
    }

    lstrcpyn_safe(buf,m_valueText.Get(),bufsz);
    return true;
  }
  if (m_maxr > m_minr)
  {
    snprintf(buf,bufsz,"%.1f%%",((m_pos-m_minr)*100.0) / (m_maxr-m_minr));
    return true;
  }
  return false;
}

void WDL_VirtualSlider::SetAccessDescCopy(const char *str)
{
  char *op = m_accessDescCopy;  
  if (str && *str)
  {
    if (op && !strcmp(op,str)) return;

    m_accessDescCopy = strdup(str);
    SetAccessDesc(m_accessDescCopy);
  }
  else
  {
    SetAccessDesc("");
    m_accessDescCopy=NULL;
  }
  free(op);
}
void WDL_VirtualSlider::SetAccessValueDesc(const char *str)
{
  if (!str) str="";
  if (strcmp(m_valueText.Get(),str))
  {
    m_valueText.Set(str);
    if (m__iaccess) m__iaccess->OnStateChange();
  }
}

void WDL_VirtualSlider::SetSliderPosition(int pos) 
{ 
  if (!m_captured)
  {
    if (pos < m_minr) pos=m_minr; 
    else if (pos>m_maxr) pos=m_maxr; 

    if (pos != m_pos)
    {
      m_pos=pos;
      RequestRedraw(NULL); 
    }
  }
}

void WDL_VirtualSlider::GetPositionPaintExtent(RECT *r, int rscale)
{
  RECT mp;
  WDL_VWnd::GetPositionPaintExtent(&mp,rscale);
  *r = mp;
  bool isVert=GetIsVert();
  bool wantKnob=m_knobbias > 0;
  LICE_IBitmap *bm_image=m_skininfo ? m_skininfo->thumbimage[isVert] : 0;
  if (!bm_image && !wantKnob)
  {
    bm_image=WDL_STYLE_GetSliderBitmap2(isVert);
  }
  if (bm_image && !wantKnob)
  {
    int bm_w=bm_image->getWidth();
    int bm_h=bm_image->getHeight();
    int s=0;
    int bm_w2=bm_w;
    int bm_h2=bm_h;
    AdjustThumbImageSize(m_last_advscale,mp.right-mp.left,mp.bottom-mp.top,m_skininfo,isVert,&bm_w,&bm_h,&s,&wantKnob,m_knobbias);

    if (!wantKnob)
    {
      ScaleVals(m_last_advscale,&bm_w,&bm_h,NULL);
      int rsize=m_maxr-m_minr;
      int viewh=mp.bottom-mp.top;
      int vieww=mp.right-mp.left;

      if (isVert)
      {
        if (bm_w > vieww)
        {
          r->left-=(bm_w-vieww)/2+1;
          r->right+=(bm_w-vieww)/2+1;
        }

        int tadj=m_tl_extra;
        int badj=m_br_extra;

        int pos = viewh - bm_h - ((m_pos-m_minr) * (viewh - bm_h))/rsize-s;

        if (-pos > tadj) tadj=-pos;
        if (pos+bm_h2 > viewh+badj) badj=pos+bm_h2-viewh;

        //m_tl_extra=m_br_extra=
        r->top-=tadj; //s;
        r->bottom += badj; //(bm_h2-bm_h)-s;
      }
      else
      {
        if (bm_h > viewh)
        {
          r->top-=(bm_h-viewh)/2+1;
          r->bottom+=(bm_h-viewh)/2+1;
        }

        int ladj=m_tl_extra;
        int radj=m_br_extra;

        int pos = ((m_pos-m_minr) * (vieww - bm_w))/rsize - s;

        if (-pos > ladj) ladj=-pos;
        if (pos+bm_w2 > vieww+radj) radj=pos+bm_w2-vieww;

        r->left-=ladj; //s;
        r->right += radj; // (bm_w2-bm_w)-s;
      }
    }
  }

  if (wantKnob) 
  {
    const int viewh=mp.bottom-mp.top;
    const int vieww=mp.right-mp.left;
    {
      int sz= wdl_min(vieww,viewh);
      int ox = mp.left + (vieww-sz)/2;
      int oy = mp.top + (viewh-sz)/2;

      WDL_VirtualWnd_BGCfg *back_image = getKnobBackgroundForSize(InvScaleVal(m_last_advscale,sz));
      if (back_image && back_image->bgimage && 
          back_image->bgimage_lt_out[0]>0 &&
          back_image->bgimage_lt_out[1]>0 &&
          back_image->bgimage_rb_out[0]>0 &&
          back_image->bgimage_rb_out[1]>0)
      {
        int tmp = ox - InvScaleVal(m_last_advscale,back_image->bgimage_lt_out[0]-1);
        if (tmp < r->left) r->left=tmp;
        tmp = oy - InvScaleVal(m_last_advscale,back_image->bgimage_lt_out[1]-1);
        if (tmp < r->top) r->top=tmp;
        tmp = ox+sz+InvScaleVal(m_last_advscale,back_image->bgimage_rb_out[0]-1);
        if (tmp > r->right) r->right = tmp;
        tmp = oy+sz+InvScaleVal(m_last_advscale,back_image->bgimage_rb_out[1]-1);
        if (tmp > r->bottom) r->bottom = tmp;
      }
    }

    {
      int kvw = vieww, kvh=viewh;
      int ksw=0,ksh=0,kso=0;
      WDL_VirtualWnd_BGCfg *knobimage=vwnd_slider_getknobimageforsize(m_knobstacks,m_nknobstacks,&kvw,&kvh,&ksw,&ksh,&kso);

      if (knobimage && kso && ksw>0 && ksh>0 && 
            knobimage->bgimage_lt_out[0] > 0 &&
            knobimage->bgimage_lt_out[1] > 0 && 
            knobimage->bgimage_rb_out[0] > 0 &&
            knobimage->bgimage_rb_out[1] > 0)
      {
        const int ox = mp.left + (vieww - kvw)/2;
        const int oy = mp.top + (viewh - kvh)/2;

        int ww = ksw - (knobimage->bgimage_lt_out[0]-1) - (knobimage->bgimage_rb_out[0]-1);
        int wh = ksh - (knobimage->bgimage_lt_out[1]-1) - (knobimage->bgimage_rb_out[1]-1);
        
        if (ww > 0)
        {
          int tmp = ox - InvScaleVal(m_last_advscale,kvw * (knobimage->bgimage_lt_out[0]-1))/ww;
          if (tmp < r->left) r->left=tmp;
          tmp = ox + kvw + InvScaleVal(m_last_advscale,kvw * (knobimage->bgimage_rb_out[0]-1))/ww;
          if (tmp > r->right) r->right=tmp;
        }
        if (wh > 0)
        {
          int tmp = oy - InvScaleVal(m_last_advscale,kvh * (knobimage->bgimage_lt_out[1]-1))/wh;
          if (tmp < r->top) r->top=tmp;
          tmp = oy + kvh + InvScaleVal(m_last_advscale,kvh * (knobimage->bgimage_rb_out[1]-1))/wh;
          if (tmp > r->bottom) r->bottom=tmp;
        }
      }
    }
  }
  else if (m_skininfo && m_skininfo->bgimagecfg[isVert].bgimage)
  {
    // expand paintextent by background image outer extent if necessary
    WDL_VirtualWnd_BGCfg *b = &m_skininfo->bgimagecfg[isVert];
    if (b->bgimage_lt[0]>0 &&
        b->bgimage_lt[1]>0 &&
        b->bgimage_rb[0]>0 &&
        b->bgimage_rb[1]>0 &&
        b->bgimage_lt_out[0]>0 &&
        b->bgimage_lt_out[1]>0 &&
        b->bgimage_rb_out[0]>0 &&
        b->bgimage_rb_out[1]>0)
    {
      int l = mp.left - (b->bgimage_lt_out[0]-1);
      int t = mp.top - (b->bgimage_lt_out[1]-1);
      int right = mp.right + b->bgimage_rb_out[0]-1;
      int bot = mp.bottom + b->bgimage_rb_out[1]-1;

      if (l < r->left) r->left=l;
      if (t < r->top) r->top=t;
      if (right > r->right) r->right = right;
      if (bot > r->bottom) r->bottom = bot;
    }
  }
}
