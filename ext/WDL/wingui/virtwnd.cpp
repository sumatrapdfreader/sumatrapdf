/*
    WDL - virtwnd.cpp
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
      

    Implementation for basic virtual window types.

*/

#include "virtwnd-controls.h"
#include "../lice/lice.h"
#include "../assocarray.h"

WDL_VWnd_Painter::WDL_VWnd_Painter()
{
  m_GSC=0;
  m_bm=0;
  m_bgbm=0;
  m_bgbmtintUnderMode = false;
  m_bgbmtintcolor = -1;

  m_paint_xorig=m_paint_yorig=0;
  m_cur_hwnd=0;
  memset(&m_ps,0,sizeof(m_ps));
  m_wantg=-1;
  m_gradstart=0.5;
  m_gradslope=0.2;
  m_bgcache=0;
  m_render_scale = WDL_VWND_SCALEBASE;
  m_advisory_scale = WDL_VWND_SCALEBASE;
}

WDL_VWnd_Painter::~WDL_VWnd_Painter()
{
  delete m_bm;
}

void WDL_VWnd_Painter::SetGSC(int (*_GSC)(int))
{
  m_GSC=_GSC;
}
int WDL_VWnd_Painter::GSC(int a)
{
  if (m_GSC) return m_GSC(a);
  return GetSysColor(a);
}

void WDL_VWnd_Painter::SetBGGradient(int wantGradient, double start, double slope)
{
  m_wantg=wantGradient;
  m_gradstart=start;
  m_gradslope=slope;
}

void WDL_VWnd_Painter::DoPaintBackground(LICE_IBitmap *bmOut, int bgcolor, const RECT *clipr, int wnd_w, int wnd_h, int xoffs, int yoffs)
{
  if (!bmOut) return;

  if (m_bgbm&&m_bgbm->bgimage)
  {
    int srcw=m_bgbm->bgimage->getWidth();
    int srch=m_bgbm->bgimage->getHeight();
    if (srcw && srch)
    {
      int fflags=0;
      if (srcw  < wnd_w/4 || srch < wnd_h/4)
        fflags|=LICE_BLIT_FILTER_BILINEAR;
   
      if (m_bgbmtintUnderMode)
      {
        tintRect(bmOut, clipr, xoffs, yoffs,true);
      }

      if (m_bgcache && !xoffs && !yoffs)
      {
        const int sinfo2 = bmOut ? (int)bmOut->Extended(LICE_EXT_GET_ADVISORY_SCALING,NULL) : 0;
        if (m_bgbmtintUnderMode)
        {
          LICE_IBitmap *tmp = m_bgcache->GetCachedBG(wnd_w, wnd_h, sinfo2, this, m_bgbm->bgimage);
          if (!tmp)
          {
            tmp = m_bgcache->SetCachedBG(wnd_w, wnd_h, sinfo2, NULL, this, m_bgbm->bgimage);
            
            // if added to cache, scale and copy alpha information
            if (tmp) WDL_VirtualWnd_ScaledBlitBG(tmp, m_bgbm, 0, 0, wnd_w, wnd_h,
              0, 0,
              wnd_w,
              wnd_h,
              1.0, LICE_BLIT_MODE_COPY | fflags);
          }

          if (tmp) // copy from cache
          {
            LICE_Blit(bmOut, tmp, clipr->left, clipr->top, clipr->left, clipr->top, clipr->right - clipr->left, clipr->bottom - clipr->top, 1.0f, LICE_BLIT_USE_ALPHA|LICE_BLIT_MODE_COPY);
          }
          else // scale as if no cache present
          {
            WDL_VirtualWnd_ScaledBlitBG(bmOut, m_bgbm, 0, 0, wnd_w, wnd_h,
              0, 0,
              wnd_w,
              wnd_h,
              1.0, LICE_BLIT_MODE_COPY | fflags | LICE_BLIT_USE_ALPHA);
          }

        }
        else
        {
          // tint-over mode, we can render then cache
          LICE_IBitmap *tmp = m_bgcache->GetCachedBG(wnd_w, wnd_h, sinfo2, this, m_bgbm->bgimage);
          if (tmp)
          {
            LICE_Blit(bmOut, tmp, clipr->left, clipr->top, clipr->left, clipr->top, clipr->right - clipr->left, clipr->bottom - clipr->top, 1.0f, LICE_BLIT_MODE_COPY);
          }
          else
          {
            WDL_VirtualWnd_ScaledBlitBG(bmOut, m_bgbm, 0, 0, wnd_w, wnd_h,
              0, 0,
              wnd_w,
              wnd_h,
              1.0, LICE_BLIT_MODE_COPY | fflags);
            m_bgcache->SetCachedBG(wnd_w, wnd_h, sinfo2, bmOut, this, m_bgbm->bgimage);
          }
        }
      }
      else // no bg cache
      {
        WDL_VirtualWnd_ScaledBlitBG(bmOut,m_bgbm,xoffs,yoffs,wnd_w,wnd_h,
                                    clipr->left+xoffs,clipr->top+yoffs,
                                    clipr->right-clipr->left,
                                    clipr->bottom-clipr->top,
                                    1.0, LICE_BLIT_MODE_COPY | fflags | (m_bgbmtintUnderMode?LICE_BLIT_USE_ALPHA:0));
      }

      if (!m_bgbmtintUnderMode)
        tintRect(bmOut,clipr,xoffs,yoffs,false);

      return;
    }
  }

  if (bgcolor<0) bgcolor=m_GSC?m_GSC(COLOR_3DFACE):GetSysColor(COLOR_3DFACE);

  int needfill=1;

#ifdef WDL_VWND_WANTBGGRADIENT_SUPPORT
  double gradslope=m_gradslope;
  double gradstart=m_gradstart;
  bool wantGrad=m_wantg>0;
  if (m_wantg<0) wantGrad=WDL_STYLE_GetBackgroundGradient(&gradstart,&gradslope);

  if (wantGrad && gradslope >= 0.01)
  {

    {
      needfill=0;

      int spos = (int) (gradstart * wnd_h);
      if (spos > 0)
      {
        if (spos > wnd_h) spos=wnd_h;
        if (clipr->top < spos)
        {
          LICE_FillRect(bmOut,clipr->left+xoffs,clipr->top+yoffs,
                             clipr->right-clipr->left,spos-clipr->top,
                             LICE_RGBA_FROMNATIVE(bgcolor,255),1.0f,LICE_BLIT_MODE_COPY);
        }
      }
      else spos=0;

      if (spos < wnd_h)
      {
        struct
        {
          int x,y,Red,Green,Blue;
        }
        vert[2]={{0,},};

        double sr=GetRValue(bgcolor);
        double sg=GetGValue(bgcolor);
        double sb=GetBValue(bgcolor);

        vert [0] .x = clipr->left;
        vert [1] .x = clipr->right;


        vert[0].y=clipr->top;
        vert[1].y=clipr->bottom;

        if (vert[0].y < spos) vert[0].y=spos;
        if (vert[1].y>wnd_h) vert[1].y=wnd_h;

        wnd_h-=spos;

        int x;
        for (x =0 ; x < 2; x ++)
        {
          double sc1=(wnd_h-(vert[x].y-spos)*gradslope)/(double)wnd_h * 256.0;

          vert[x].Red = (int) (sr * sc1);
          vert[x].Green = (int) (sg * sc1);
          vert[x].Blue = (int) (sb * sc1);
        }

        {

          int bmh=vert[1].y-vert[0].y;
          float s=(float) (1.0/(65535.0*bmh));

          LICE_GradRect(bmOut,vert[0].x+xoffs,vert[0].y+yoffs,clipr->right-clipr->left,bmh,
            vert[0].Red/65535.0f,vert[0].Green/65535.0f,vert[0].Blue/65535.0f,1.0,0,0,0,0,
            (vert[1].Red-vert[0].Red)*s,
            (vert[1].Green-vert[0].Green)*s,
            (vert[1].Blue-vert[0].Blue)*s,
             0.0,LICE_BLIT_MODE_COPY);
        }
      }
    }
  }

#endif//WDL_VWND_WANTBGGRADIENT_SUPPORT

  if (needfill)
  {
    LICE_FillRect(bmOut,clipr->left+xoffs,
                       clipr->top+yoffs,
                       clipr->right-clipr->left,
                       clipr->bottom-clipr->top,LICE_RGBA_FROMNATIVE(bgcolor,255),1.0f,LICE_BLIT_MODE_COPY);
  }        

}

void WDL_VWnd_Painter::PaintBegin(HWND hwnd, int bgcolor, const RECT *limitBGrect, const RECT *windowRect, HDC hdcOut, const RECT *clip_r)
{
  if (!hwnd && (!windowRect||!hdcOut||!clip_r)) return;
  if (!m_cur_hwnd)
  {
    if (hwnd)
    {
      if (BeginPaint(hwnd,&m_ps)) 
      {
        m_cur_hwnd=hwnd;
      }
    }
    else
    {
      m_ps.hdc = hdcOut;
      m_ps.rcPaint = *clip_r;
    }
    if (m_cur_hwnd || !hwnd)
    {
      RECT rrr;
      if (windowRect) rrr=*windowRect;
      else GetClientRect(m_cur_hwnd,&rrr);
      RenderScaleRect(&rrr);
      int fwnd_w=rrr.right-rrr.left,fwnd_h=rrr.bottom-rrr.top;
      if (fwnd_h<0)fwnd_h=-fwnd_h;

      int wnd_w,wnd_h;
      
      if (fwnd_w < 2048 && fwnd_h < 2048)
      {
        m_paint_xorig=m_paint_yorig=0;
        wnd_w=fwnd_w;
        wnd_h=fwnd_h;
      }
      else // alternate large canvas mode 
      {
        m_bgcache=0; // force no caching in large canvas mode

        // note: there can be some slight background artifacts in this mode that need to be resolved (REAPER TCP bg bottom line on partial redraw etc)
        RECT pr = m_ps.rcPaint;
        RenderScaleRect(&pr);
        m_paint_xorig=pr.left;
        m_paint_yorig=pr.top;
        wnd_w = pr.right-pr.left;
        wnd_h = pr.bottom - pr.top;
      }
      
      if (wnd_h<0)wnd_h=-wnd_h;

      if (!m_bm) m_bm=new LICE_SysBitmap;

      m_bm->Extended(LICE_EXT_SET_ADVISORY_SCALING,&m_advisory_scale);
      
      if (m_bm->getWidth()<wnd_w || m_bm->getHeight() < wnd_h)
      {
        m_bm->resize(wdl_max(m_bm->getWidth(),wnd_w),wdl_max(m_bm->getHeight(),wnd_h));
      }
      RECT tr = m_ps.rcPaint;
      RenderScaleRect(&tr);
      RECT lbg;
      if (limitBGrect) 
      {
        lbg = *limitBGrect;
        RenderScaleRect(&lbg);
      }

      if (!limitBGrect || (lbg.left <1 && lbg.top < 1 && lbg.right >= fwnd_w && lbg.bottom >= fwnd_h))
      {
        DoPaintBackground(m_bm,bgcolor,&tr, fwnd_w, fwnd_h, -m_paint_xorig, -m_paint_yorig);
      }
      else
      {
        if (tr.left < lbg.left) tr.left = lbg.left;
        if (tr.top < lbg.top) tr.top = lbg.top;
        if (tr.right > lbg.right) tr.right = lbg.right;
        if (tr.bottom > lbg.bottom) tr.bottom = lbg.bottom;

        if (tr.left < tr.right && tr.top < tr.bottom)
        {
          const int bgw = lbg.right-lbg.left, bgh = lbg.bottom-lbg.top;

          const int xl = lbg.left - m_paint_xorig, yl = lbg.top - m_paint_yorig;
          const int xo = wdl_min(xl,0), yo = wdl_min(yl,0);
          const int use_w = bgw + xo, use_h = bgh + yo;
          if (use_w > 0 && use_h > 0)
          {
            LICE_SubBitmap bm(m_bm, wdl_max(xl,0), wdl_max(yl,0), use_w, use_h);

            OffsetRect(&tr, -lbg.left,-lbg.top);
            DoPaintBackground(&bm,bgcolor,&tr, bgw,bgh, xo, yo);
          }
        }
      }
    }
  }
}


#ifdef _WIN32
typedef struct
{
  HRGN rgn;
  HWND par;
  RECT *sr;
} enumInfo;

static BOOL CALLBACK enumProc(HWND hwnd,LPARAM lParam)
{
  enumInfo *p=(enumInfo*)lParam;
  if (IsWindowVisible(hwnd)) 
  {
    RECT r;
    GetWindowRect(hwnd,&r);
    ScreenToClient(p->par,(LPPOINT)&r);
    ScreenToClient(p->par,((LPPOINT)&r)+1);
    if (!p->rgn) p->rgn=CreateRectRgnIndirect(p->sr);

    HRGN rgn2=CreateRectRgnIndirect(&r);
    CombineRgn(p->rgn,p->rgn,rgn2,RGN_DIFF);
    DeleteObject(rgn2);
  }
  return TRUE;
}
#endif

void WDL_VWnd_Painter::PaintEnd(int xoffs, int yoffs)
{
  m_bgbm=0;
  m_bgbmtintUnderMode = false;
  m_bgbmtintcolor = -1;
  if (!m_cur_hwnd && !m_ps.hdc) return;
  if (m_bm)
  {
#ifdef _WIN32
    HRGN rgnsave=0;
    if (m_cur_hwnd)
    {
      enumInfo a={0,m_cur_hwnd,&m_ps.rcPaint};
      EnumChildWindows(m_cur_hwnd,enumProc,(LPARAM)&a);
      if (a.rgn)
      {
        rgnsave=CreateRectRgn(0,0,0,0);
        GetClipRgn(m_ps.hdc,rgnsave);

        ExtSelectClipRgn(m_ps.hdc,a.rgn,RGN_AND);
        DeleteObject(a.rgn);
      }
    }
    BitBlt(m_ps.hdc,xoffs+m_ps.rcPaint.left,yoffs+m_ps.rcPaint.top,
                    m_ps.rcPaint.right-m_ps.rcPaint.left,
                    m_ps.rcPaint.bottom-m_ps.rcPaint.top,
                    m_bm->getDC(),m_ps.rcPaint.left-m_paint_xorig,m_ps.rcPaint.top-m_paint_yorig,SRCCOPY);

    if (rgnsave)
    {
      SelectClipRgn(m_ps.hdc,rgnsave);
      DeleteObject(rgnsave);
    }
#else
    SWELL_SyncCtxFrameBuffer(m_bm->getDC());
    const int rscale = GetRenderScale();

    if (rscale != 256)
    {
      RECT p2 = m_ps.rcPaint;
      RenderScaleRect(&p2);
      StretchBlt(m_ps.hdc,xoffs+m_ps.rcPaint.left,yoffs+m_ps.rcPaint.top,
             m_ps.rcPaint.right-m_ps.rcPaint.left,
             m_ps.rcPaint.bottom-m_ps.rcPaint.top,
             m_bm->getDC(),
             p2.left-m_paint_xorig,
             p2.top-m_paint_yorig,
             p2.right-p2.left,
             p2.bottom-p2.top,
             SRCCOPY);
    }
    else
    {
      BitBlt(m_ps.hdc,xoffs+m_ps.rcPaint.left,yoffs+m_ps.rcPaint.top,
             m_ps.rcPaint.right-m_ps.rcPaint.left,
             m_ps.rcPaint.bottom-m_ps.rcPaint.top,
             m_bm->getDC(),m_ps.rcPaint.left-m_paint_xorig,m_ps.rcPaint.top-m_paint_yorig,SRCCOPY);
    }
#endif
  }
  if (m_cur_hwnd) 
  {
    EndPaint(m_cur_hwnd,&m_ps);
    m_cur_hwnd=0;
  }
  m_ps.hdc=NULL;
}

void WDL_VWnd_Painter::GetPaintInfo(RECT *rclip, int *xoffsdraw, int *yoffsdraw)
{
  if (rclip) 
  {
    *rclip = m_ps.rcPaint;
    RenderScaleRect(rclip);
  }
  if (xoffsdraw) *xoffsdraw = -m_paint_xorig;
  if (yoffsdraw) *yoffsdraw = -m_paint_yorig;
}

void WDL_VWnd_Painter::tintRect(LICE_IBitmap *bmOut, const RECT *clipr, int xoffs, int yoffs, bool isCopy)
{
  if (m_bgbmtintcolor>=0)
  {
    if (isCopy)
    {
      LICE_FillRect(bmOut, clipr->left + xoffs, clipr->top + yoffs, clipr->right - clipr->left, clipr->bottom - clipr->top, LICE_RGBA_FROMNATIVE(m_bgbmtintcolor), 1.0f, LICE_BLIT_MODE_COPY);
      return;
    }
    float rv=GetRValue(m_bgbmtintcolor)/255.0f;
    float gv=GetGValue(m_bgbmtintcolor)/255.0f;
    float bv=GetBValue(m_bgbmtintcolor)/255.0f;

    float avg=(rv+gv+bv)*0.33333f;
    if (avg<0.05f)avg=0.05f;

    float sc=0.5f;
    float sc2 = (1.0f-sc)/avg;

    float sc3=32.0f;
    float sc4=64.0f*(avg-0.5f);
    // tint
    LICE_MultiplyAddRect(bmOut,clipr->left+xoffs,clipr->top+yoffs,clipr->right-clipr->left,clipr->bottom-clipr->top,
        sc+rv*sc2,sc+gv*sc2,sc+bv*sc2,1,
        (rv-avg)*sc3+sc4,
        (gv-avg)*sc3+sc4,
        (bv-avg)*sc3+sc4,
        0);
  }
}


void WDL_VWnd_Painter::PaintBGCfg(WDL_VirtualWnd_BGCfg *bitmap, const RECT *coords, bool allowTint, float alpha, int mode)
{
  if (!bitmap || !coords || !bitmap->bgimage || !m_bm) return;

  RECT rr, paintScaled = m_ps.rcPaint, coordsScaled = *coords;
  RenderScaleRect(&coordsScaled);
  RenderScaleRect(&paintScaled);
  rr.left = wdl_max(coordsScaled.left, paintScaled.left);
  rr.top = wdl_max(coordsScaled.top, paintScaled.top);
  rr.right = wdl_min(coordsScaled.right, paintScaled.right);
  rr.bottom = wdl_min(coordsScaled.bottom, paintScaled.bottom);

  if (allowTint && m_bgbmtintUnderMode)
  {
    if (rr.right>rr.left && rr.bottom>rr.top)
      tintRect(m_bm, &rr, -m_paint_xorig, -m_paint_yorig,true);
  }

  WDL_VirtualWnd_ScaledBlitBG(m_bm,bitmap,coordsScaled.left - m_paint_xorig,
                                          coordsScaled.top - m_paint_yorig,
                                          coordsScaled.right-coordsScaled.left,
                                          coordsScaled.bottom-coordsScaled.top,
                                          paintScaled.left - m_paint_xorig,
                                          paintScaled.top - m_paint_yorig,
                                          paintScaled.right - paintScaled.left,
                                          paintScaled.bottom - paintScaled.top,alpha,mode);

  if (allowTint && !m_bgbmtintUnderMode)
  {
    if (rr.right>rr.left && rr.bottom>rr.top)
      tintRect(m_bm,&rr,-m_paint_xorig,-m_paint_yorig,false);
  }
}

void WDL_VWnd_Painter::PaintVirtWnd(WDL_VWnd *vwnd, int borderflags, int x_xlate, int y_xlate)
{
  RECT tr=m_ps.rcPaint;
  RenderScaleRect(&tr);
  if (!m_bm||(!m_cur_hwnd&&!m_ps.hdc)|| !vwnd->IsVisible()) return;

  RECT r;
  vwnd->GetPosition(&r); // maybe should use GetPositionPaintExtent or GetPositionPaintOverExtent ?
  OffsetRect(&r,x_xlate,y_xlate);
  RenderScaleRect(&r);
  const int rscale = GetRenderScale();

  if (tr.left<r.left) tr.left=r.left;
  if (tr.right>r.right) tr.right=r.right;
  if (tr.top<r.top) tr.top=r.top;
  if (tr.bottom>r.bottom)tr.bottom=r.bottom;

  if (tr.bottom > tr.top && tr.right > tr.left)
  {
    const int xorig = m_paint_xorig - x_xlate * rscale / WDL_VWND_SCALEBASE;
    const int yorig = m_paint_yorig - y_xlate * rscale / WDL_VWND_SCALEBASE;
    tr.left -= m_paint_xorig;
    tr.right -= m_paint_xorig;
    tr.top -= m_paint_yorig;
    tr.bottom -= m_paint_yorig;
    vwnd->SetCurPainter(this);
    vwnd->OnPaint(m_bm,-xorig,-yorig,&tr,rscale);
    if (borderflags)
    {
      PaintBorderForRect(&r,borderflags);
    }
    if (vwnd->WantsPaintOver()) vwnd->OnPaintOver(m_bm,-xorig,-yorig,&tr,rscale);
    vwnd->SetCurPainter(NULL);

  }
}

void WDL_VWnd_Painter::PaintBorderForHWND(HWND hwnd, int borderflags)
{
#ifdef _WIN32
  if (m_cur_hwnd)
  {
    RECT r;
    GetWindowRect(hwnd,&r);
    ScreenToClient(m_cur_hwnd,(LPPOINT)&r);
    ScreenToClient(m_cur_hwnd,((LPPOINT)&r)+1);
    RenderScaleRect(&r);
    PaintBorderForRect(&r,borderflags);
  }
#endif
}

void WDL_VWnd_Painter::PaintBorderForRect(const RECT *r, int borderflags)
{
  if (!m_bm|| (!m_cur_hwnd && !m_ps.hdc) ||!borderflags) return;
  RECT rrr = *r;
  RenderScaleRect(&rrr);
  rrr.left-=m_paint_xorig;
  rrr.right-=m_paint_xorig;
  rrr.top-=m_paint_yorig;
  rrr.bottom-=m_paint_yorig;

  LICE_pixel pencol = m_GSC?m_GSC(COLOR_3DHILIGHT):GetSysColor(COLOR_3DHILIGHT);
  LICE_pixel pencol2 = m_GSC?m_GSC(COLOR_3DSHADOW):GetSysColor(COLOR_3DSHADOW);
  pencol = LICE_RGBA_FROMNATIVE(pencol,255);
  pencol2 = LICE_RGBA_FROMNATIVE(pencol2,255);

  if (borderflags== WDL_VWP_SUNKENBORDER || borderflags == WDL_VWP_SUNKENBORDER_NOTOP)
  {
    LICE_Line(m_bm,rrr.left-1,rrr.bottom,rrr.right,rrr.bottom,pencol,1.0f,LICE_BLIT_MODE_COPY,false);
    LICE_Line(m_bm,rrr.right,rrr.bottom,rrr.right,rrr.top-1,pencol,1.0f,LICE_BLIT_MODE_COPY,false);

    if (borderflags != WDL_VWP_SUNKENBORDER_NOTOP)
      LICE_Line(m_bm,rrr.right,rrr.top-1,rrr.left-1,rrr.top-1,pencol2,1.0f,LICE_BLIT_MODE_COPY,false);

    LICE_Line(m_bm,rrr.left-1,rrr.top-1,rrr.left-1,rrr.bottom,pencol2,1.0f,LICE_BLIT_MODE_COPY,false);
  }
  else if (borderflags == WDL_VWP_DIVIDER_VERT) // vertical
  {
    int left=rrr.left;

    LICE_Line(m_bm,left,rrr.top,left,rrr.bottom+1,pencol2,1.0f,LICE_BLIT_MODE_COPY,false);
    LICE_Line(m_bm,left+1,rrr.top,left+1,rrr.bottom+1,pencol,1.0f,LICE_BLIT_MODE_COPY,false);
  }
  else if (borderflags == WDL_VWP_DIVIDER_HORZ) 
  {
    int top=rrr.top+1;
    LICE_Line(m_bm,rrr.left,top,rrr.right+1,top,pencol2,1.0f,LICE_BLIT_MODE_COPY,false);
    LICE_Line(m_bm,rrr.left,top+1,rrr.right+1,top+1,pencol,1.0f,LICE_BLIT_MODE_COPY,false);
  }
}

WDL_VWnd::WDL_VWnd() 
{ 
  m__iaccess=0;
  m__iaccess_desc=0;
  m_visible=true; m_id=0; 
  m_position.left=0; m_position.top=0; m_position.right=0; m_position.bottom=0; 
  m_parent=0;
  m_children=0;
  m_realparent=0;
  m_captureidx=-1;
  m_lastmouseidx=-1;
  m_userdata=0;
  m_curPainter=0;
}

WDL_VWnd::~WDL_VWnd() 
{ 
  if (m_children) 
  {
    WDL_VWnd *cap = m_children->Get(m_captureidx);
    if (cap) cap->OnCaptureLost();
    m_children->Empty(true); 
    delete m_children;
  }
  if (m__iaccess) m__iaccess->Release();
}

int WDL_VWnd::GSC(int a)
{
  return m_curPainter ? m_curPainter->GSC(a) : GetSysColor(a);
}

INT_PTR WDL_VWnd::SendCommand(int command, INT_PTR parm1, INT_PTR parm2, WDL_VWnd *src)
{
  if (m_realparent)
  {
    return SendMessage(m_realparent,command,parm1,parm2);
  }
  else if (m_parent) return m_parent->SendCommand(command,parm1,parm2,src);
  return 0;
}

void WDL_VWnd::RequestRedraw(RECT *r)
{ 
  if (!IsVisible() || 
      m_position.right <= m_position.left || 
      m_position.bottom <= m_position.top) return;

  RECT r2;
  
  if (r)
  {
    r2=*r; 
    r2.left+=m_position.left; r2.right += m_position.left; 
    r2.top += m_position.top; r2.bottom += m_position.top;
  }
  else 
  {
    GetPositionPaintExtent(&r2, WDL_VWND_SCALEBASE); 
    RECT r3;
    if (WantsPaintOver())
    {
      GetPositionPaintOverExtent(&r3, WDL_VWND_SCALEBASE); 
      if (r3.left<r2.left)r2.left=r3.left;
      if (r3.top<r2.top)r2.top=r3.top;
      if (r3.right>r2.right)r2.right=r3.right;
      if (r3.bottom>r2.bottom)r2.bottom=r3.bottom;
    }
  }

  if (m_realparent)
  {
#ifdef _WIN32
    HWND hCh;
    if ((hCh=GetWindow(m_realparent,GW_CHILD)))
    {
      HRGN rgn=CreateRectRgnIndirect(&r2);
      int n=30; // limit to 30 children
      while (n-- && hCh)
      {
        if (IsWindowVisible(hCh))
        {
          RECT r;
          GetWindowRect(hCh,&r);
          ScreenToClient(m_realparent,(LPPOINT)&r);
          ScreenToClient(m_realparent,((LPPOINT)&r)+1);
          HRGN tmprgn=CreateRectRgn(r.left,r.top,r.right,r.bottom);
          CombineRgn(rgn,rgn,tmprgn,RGN_DIFF);
          DeleteObject(tmprgn);
        }
        hCh=GetWindow(hCh,GW_HWNDNEXT);
      }
      InvalidateRgn(m_realparent,rgn,FALSE);
      DeleteObject(rgn);

    }
    else
#else
  // OS X, expand region up slightly
  r2.top--;
#endif
      InvalidateRect(m_realparent,&r2,FALSE);
  }
  else if (m_parent) m_parent->RequestRedraw(&r2); 
}

bool WDL_VWnd::IsDescendent(WDL_VWnd *w)
{
  if (!w || !m_children) return false;
  int x,n=m_children->GetSize();
  for(x=0;x<n;x++) if (m_children->Get(x) == w) return true;
  for(x=0;x<n;x++) 
  {
    WDL_VWnd *tmp = m_children->Get(x);
    if (tmp && tmp->IsDescendent(w)) return true;
  }
  return false;
}

void WDL_VWnd::SetChildPosition(WDL_VWnd *ch, int pos)
{
  if (!ch || !m_children) return;
  int x;
  for(x=0;x<m_children->GetSize();x++)
  {
    if (m_children->Get(x) == ch) 
    {
      if (pos>x) pos--;
      if (pos != x)
      {
        WDL_VWnd * const cap = m_children->Get(m_captureidx);
        m_children->Delete(x);
        m_children->Insert(pos,ch);
        if (cap) m_captureidx = m_children->Find(cap);
      }
      return;
    }
  }
}


void WDL_VWnd::AddChild(WDL_VWnd *wnd, int pos)
{
  if (!wnd) return;

  wnd->SetParent(this);
  if (!m_children) m_children=new WDL_PtrList<WDL_VWnd>;
  if (pos<0||pos>=m_children->GetSize())
  {
    m_children->Add(wnd);
  }
  else
  {
    m_children->Insert(pos,wnd);
    if (pos <= m_captureidx) m_captureidx++;
  }
  if (m__iaccess) m__iaccess->ClearCaches();
}

WDL_VWnd *WDL_VWnd::GetChildByID(int id)
{
  if (m_children) 
  {
    int x;
    for (x = 0; x < m_children->GetSize(); x ++)
      if (m_children->Get(x)->GetID()==id) return m_children->Get(x);
    WDL_VWnd *r;
    for (x = 0; x < m_children->GetSize(); x ++) if ((r=m_children->Get(x)->GetChildByID(id))) return r;
  }

  return 0;
}

void WDL_VWnd::RemoveChild(WDL_VWnd *wnd, bool dodel)
{
  int idx=m_children ? m_children->Find(wnd) : -1;
  if (idx>=0) 
  {
    if (idx == m_captureidx)
    {
      wnd->OnCaptureLost();
      m_captureidx = -1;
    }
    else if (idx < m_captureidx)
    {
      m_captureidx--;
    }

    if (!dodel) wnd->SetParent(NULL);
    m_children->Delete(idx,dodel);
  }
  if (m__iaccess) m__iaccess->ClearCaches();
}

void WDL_VWnd::OnPaint(LICE_IBitmap *drawbm, int origin_x, int origin_y, RECT *cliprect, int rscale)
{
  int x;
  if (m_children) for (x = m_children->GetSize()-1; x >=0; x --)
  {
    WDL_VWnd *ch=m_children->Get(x);
    if (ch->IsVisible())
    {
      RECT re;
      ch->GetPosition(&re);
      if (re.right>re.left&&re.bottom>re.top)
      {
        ch->GetPositionPaintExtent(&re, rscale);
        RECT p = m_position;
        ScaleRect(&p,rscale);
        re.left += origin_x + p.left;
        re.right += origin_x + p.left;
        re.top += origin_y + p.top;
        re.bottom += origin_y + p.top;

        RECT cr=*cliprect;
        if (cr.left < re.left) cr.left=re.left;
        if (cr.right > re.right) cr.right=re.right;
        if (cr.top < re.top) cr.top=re.top;
        if (cr.bottom > re.bottom) cr.bottom=re.bottom;

        if (cr.left < cr.right && cr.top < cr.bottom)
        {
          ch->SetCurPainter(m_curPainter);
          ch->OnPaint(drawbm,p.left+origin_x,p.top+origin_y,&cr, rscale);
          ch->SetCurPainter(NULL);
        }
      }
    }
  }
}

void WDL_VWnd::OnPaintOver(LICE_IBitmap *drawbm, int origin_x, int origin_y, RECT *cliprect, int rscale)
{
  int x;
  if (m_children) for (x = m_children->GetSize()-1; x >=0; x --)
  {
    WDL_VWnd *ch=m_children->Get(x);
    if (ch->IsVisible() && ch->WantsPaintOver())
    {
      RECT re;
      ch->GetPosition(&re);
      if (re.right>re.left && re.bottom > re.top)
      {
        ch->GetPositionPaintOverExtent(&re,rscale);
        RECT p = m_position;
        ScaleRect(&p,rscale);
        re.left += origin_x + p.left;
        re.right += origin_x + p.left;
        re.top += origin_y + p.top;
        re.bottom += origin_y + p.top;

        RECT cr=*cliprect;

        if (cr.left < re.left) cr.left=re.left;
        if (cr.right > re.right) cr.right=re.right;
        if (cr.top < re.top) cr.top=re.top;
        if (cr.bottom > re.bottom) cr.bottom=re.bottom;

        if (cr.left < cr.right && cr.top < cr.bottom)
        {
          ch->SetCurPainter(m_curPainter);
          ch->OnPaintOver(drawbm,p.left+origin_x,p.top+origin_y,&cr,rscale);
          ch->SetCurPainter(NULL);
        }
      }
    }
  }
}

int WDL_VWnd::GetNumChildren()
{
  return m_children ? m_children->GetSize() : 0;
}
WDL_VWnd *WDL_VWnd::EnumChildren(int x)
{
  return m_children ? m_children->Get(x) : 0;
}

void WDL_VWnd::RemoveAllChildren(bool dodel)
{
  if (m_children) 
  {
    WDL_VWnd *cap = m_children->Get(m_captureidx);
    if (cap) cap->OnCaptureLost();
    m_captureidx = -1;
    if (!dodel) // update parent pointers
    {
      int x;
      for (x = 0; x < m_children->GetSize(); x++)
      {
        WDL_VWnd *ch = m_children->Get(x);
        if (ch) ch->SetParent(NULL);
      }
    }
    m_children->Empty(dodel);
  }
}

WDL_VWnd *WDL_VWnd::VirtWndFromPoint(int xpos, int ypos, int maxdepth)
{
  int x;
  if (m_children) for (x = 0; x < m_children->GetSize(); x++)
  {
    WDL_VWnd *wnd=m_children->Get(x);
    if (wnd->IsVisible())
    {
      RECT r;
      wnd->GetPosition(&r);
      if (xpos >= r.left && xpos < r.right && ypos >= r.top && ypos < r.bottom) 
      {
        if (maxdepth!=0)
        {
          WDL_VWnd *cwnd=wnd->VirtWndFromPoint(xpos-r.left,ypos-r.top,maxdepth > 0 ? (maxdepth-1) : -1);
          if (cwnd) return cwnd;
        }
        return wnd;
      }
    }
  }
  return 0;

}


int WDL_VWnd::OnMouseDown(int xpos, int ypos) // returns TRUE if handled
{
  if (!m_children) return 0;

  WDL_VWnd *wnd=VirtWndFromPoint(xpos,ypos,0);
  if (!wnd) 
  {
    m_captureidx=-1;
    return 0;
  }  
  RECT r;
  wnd->GetPosition(&r);
  WDL_VWND_DCHK(chk);
  int a;
  if ((a=wnd->OnMouseDown(xpos-r.left,ypos-r.top)))
  {
    if (a<0) return -1;
    if (chk.isOK()) m_captureidx=m_children->Find(wnd);
    return 1;
  }
  return 0;
}

bool WDL_VWnd::OnMouseDblClick(int xpos, int ypos) // returns TRUE if handled
{
  WDL_VWnd *wnd=VirtWndFromPoint(xpos,ypos,0);
  if (!wnd) return false;
  RECT r;
  wnd->GetPosition(&r);
  return wnd->OnMouseDblClick(xpos-r.left,ypos-r.top);
}


bool WDL_VWnd::OnMouseWheel(int xpos, int ypos, int amt)
{
  WDL_VWnd *wnd=VirtWndFromPoint(xpos,ypos,0);
  if (!wnd) return false;
  RECT r;
  wnd->GetPosition(&r);
  return wnd->OnMouseWheel(xpos-r.left,ypos-r.top,amt);
}


bool WDL_VWnd::GetToolTipString(int xpos, int ypos, char *bufOut, int bufOutSz)
{
  WDL_VWnd *wnd=VirtWndFromPoint(xpos,ypos,0);
  if (!wnd) return false;

  RECT r;
  wnd->GetPosition(&r);
  return wnd->GetToolTipString(xpos-r.left,ypos-r.top,bufOut,bufOutSz);
}

int WDL_VWnd::UpdateCursor(int xpos, int ypos)
{
  WDL_VWnd *wnd=VirtWndFromPoint(xpos,ypos,0);
  if (!wnd) return 0;

  RECT r;
  wnd->GetPosition(&r);
  return wnd->UpdateCursor(xpos-r.left,ypos-r.top);
}

void WDL_VWnd::OnMouseMove(int xpos, int ypos)
{
  if (!m_children) return;

  WDL_VWnd *wnd=m_children->Get(m_captureidx);
  
  WDL_VWND_DCHK(chk);
  if (!wnd) 
  {
    wnd=VirtWndFromPoint(xpos,ypos,0);
    if (wnd) // todo: stuff so if the mouse goes out of the window completely, the virtualwnd gets notified
    {
      int idx=m_children->Find(wnd);
      if (idx != m_lastmouseidx)
      {
        WDL_VWnd *t=m_children->Get(m_lastmouseidx);
        if (t)
        {
          RECT r;
          t->GetPosition(&r);
          t->OnMouseMove(xpos-r.left,ypos-r.top);
        }
        if (chk.isOK()) m_lastmouseidx=idx;
      }
    }
    else
    {
      WDL_VWnd *t=m_children->Get(m_lastmouseidx);
      if (t)
      {
        RECT r;
        t->GetPosition(&r);
        t->OnMouseMove(xpos-r.left,ypos-r.top);
      }
      if (chk.isOK()) m_lastmouseidx=-1;
    }
  }

  if (wnd && chk.isOK()) 
  {
    RECT r;
    wnd->GetPosition(&r);
    wnd->OnMouseMove(xpos-r.left,ypos-r.top);
  }
}

void WDL_VWnd::OnCaptureLost()
{
  int oldcap=m_captureidx;
  m_captureidx=-1;
  if (m_children)
  {
    WDL_VWnd *wnd=m_children->Get(oldcap);
    if (wnd) 
    {
      wnd->OnCaptureLost();
    }
  }
}

void WDL_VWnd::OnMouseUp(int xpos, int ypos)
{
  int oldcap=m_captureidx;
  m_captureidx=-1; // set this before passing to children, in case a child ends up destroying us
  if (m_children)
  {
    WDL_VWnd *wnd=m_children->Get(oldcap);
  
    if (!wnd) 
    {
      wnd=VirtWndFromPoint(xpos,ypos,0);
    }

    if (wnd) 
    {
      RECT r;
      wnd->GetPosition(&r);
      wnd->OnMouseUp(xpos-r.left,ypos-r.top);
    }
  }
}
void WDL_VWnd::GetPositionInTopVWnd(RECT *r)
{
  GetPosition(r);
  WDL_VWnd *par=GetParent();
  while (par)
  {
    WDL_VWnd *tmp=par;
    par=par->GetParent();
    if (par)
    {
      RECT t;
      tmp->GetPosition(&t);
      r->left+=t.left;
      r->right+=t.left;
      r->top+=t.top;
      r->bottom+=t.top;
    }
  };
  
}

class WDL_VirtualWnd_BGCfgCache_img
{
public:
  WDL_VirtualWnd_BGCfgCache_img(unsigned int szinfo, int szinfo2, LICE_IBitmap *image, unsigned int now)
  {
    lastowner=0;
    bgimage=image;
    sizeinfo=szinfo;
    scalinginfo=szinfo2;
    lastused=now;
  }
  ~WDL_VirtualWnd_BGCfgCache_img()
  {
    delete bgimage;
  }

  LICE_IBitmap *bgimage;
  unsigned int sizeinfo; // (h<<16)+w
  int scalinginfo; // advisory scaling
  unsigned int lastused; // last used time
  void *lastowner;

  static int compar(const WDL_VirtualWnd_BGCfgCache_img **a, const WDL_VirtualWnd_BGCfgCache_img ** b)
  {
    const int v = (*a)->scalinginfo - (*b)->scalinginfo;
    if (v) return v;
    return (*a)->sizeinfo - (*b)->sizeinfo;
    
  }
};


class WDL_VirtualWnd_BGCfgCache_ar
{
public:
  WDL_VirtualWnd_BGCfgCache_ar() : m_cachelist(compar, NULL, NULL, destrval) { }
  ~WDL_VirtualWnd_BGCfgCache_ar() {  }

  WDL_AssocArray<const LICE_IBitmap *,  WDL_PtrList<WDL_VirtualWnd_BGCfgCache_img> * > m_cachelist;

  static void destrval(WDL_PtrList<WDL_VirtualWnd_BGCfgCache_img> *list)
  {
    if (list) list->Empty(true);
    delete list;
  }
  static int compar(const LICE_IBitmap **a, const LICE_IBitmap ** b)
  {
    if ((*a) < (*b)) return -1;
    if ((*a) > (*b)) return 1;
    return 0;
  }

};


WDL_VirtualWnd_BGCfgCache::WDL_VirtualWnd_BGCfgCache(int want_size, int max_size)
{
  m_ar = new WDL_VirtualWnd_BGCfgCache_ar;
  m_want_size=want_size;
  m_max_size = max_size;
}
WDL_VirtualWnd_BGCfgCache::~WDL_VirtualWnd_BGCfgCache()
{
  delete m_ar;
}

void WDL_VirtualWnd_BGCfgCache::Invalidate()
{
  m_ar->m_cachelist.DeleteAll();
}

LICE_IBitmap *WDL_VirtualWnd_BGCfgCache::GetCachedBG(int w, int h, int sinfo2, void *owner_hint, const LICE_IBitmap *bgbmp)
{
  if (w<1 || h<1 || w>65535 || h>65535) return NULL;

  WDL_PtrList<WDL_VirtualWnd_BGCfgCache_img> *cache = m_ar->m_cachelist.Get(bgbmp);
  if (!cache) return NULL;

  WDL_VirtualWnd_BGCfgCache_img tmp((h<<16)+w,sinfo2,NULL,0);
  WDL_VirtualWnd_BGCfgCache_img *r  = cache->Get(cache->FindSorted(&tmp,WDL_VirtualWnd_BGCfgCache_img::compar));
  if (r)
  {
    r->lastused = GetTickCount();
    if (owner_hint && r->lastowner != owner_hint) r->lastowner=0;
    return r->bgimage;
  }
  return NULL;
}

class MemBitmapScaleRef : public LICE_MemBitmap {
  public:
    MemBitmapScaleRef(int w, int h, int sc) : LICE_MemBitmap(w,h), m_scaling(sc) { }
    int m_scaling;
    virtual INT_PTR Extended(int id, void* data)
    {
      switch (id)
      {
        case LICE_EXT_GET_ADVISORY_SCALING: return m_scaling;
        case LICE_EXT_SET_ADVISORY_SCALING: m_scaling = *(const int *)data; return 1;
      }
      return 0;
    }
};
LICE_IBitmap *WDL_VirtualWnd_BGCfgCache::SetCachedBG(int w, int h, int sinfo2, LICE_IBitmap *bmCopy, void *owner_hint, const LICE_IBitmap *bgbmp)
{
  if (w<1 || h<1 || w>65535 || h>65535) return NULL;

  const unsigned int sinfo=(h<<16)+w;;
  WDL_PtrList<WDL_VirtualWnd_BGCfgCache_img> *cache = m_ar->m_cachelist.Get(bgbmp);
  if (!cache) 
  {
    cache = new WDL_PtrList<WDL_VirtualWnd_BGCfgCache_img>;
    m_ar->m_cachelist.Insert(bgbmp,cache);
  }

  // caller should ALWAYS call GetCachedBG() and use that if present

  WDL_VirtualWnd_BGCfgCache_img *img = NULL;
  const DWORD now = GetTickCount();
  bool cacheAtWantSize = cache->GetSize()>=m_want_size;
  if (cacheAtWantSize || owner_hint)
  {
    int x;
    int bestpos=-1;
    DWORD best_age=0;
    for(x=0;x<cache->GetSize();x++)
    {
      WDL_VirtualWnd_BGCfgCache_img *a = cache->Get(x);
      if (owner_hint && a->lastowner == owner_hint)
      {
        cacheAtWantSize=true;
        best_age = 5000;
        bestpos=x;
        break; // FOUND exact match!
      }
      const DWORD age = now-a->lastused;
      if (age > best_age) // find oldest entry
      {
        best_age = age;
        bestpos = x;
      }
    }

    if (cacheAtWantSize && (best_age > 500 || cache->GetSize() >= m_max_size)) // use this slot if over 1000ms old, or if we're up against the max size
    {
      img = cache->Get(bestpos);
      cache->Delete(bestpos,false);
      if (img)
      {
        img->sizeinfo = sinfo;
        img->scalinginfo = sinfo2;
        img->lastused = now;
      }
    }

  }


  if (!img)
  {
    LICE_IBitmap *bmcp = new MemBitmapScaleRef(w,h,sinfo2);
    if (bmcp->getWidth()==w && bmcp->getHeight()==h) img = new WDL_VirtualWnd_BGCfgCache_img(sinfo,sinfo2,bmcp,now);
    else delete bmcp;
  }
  else
  {
    if (img->bgimage) 
    {
      img->bgimage->Extended(LICE_EXT_SET_ADVISORY_SCALING,&sinfo2);
      img->bgimage->resize(w,h);
    }
  }

  if (img)
  {
    img->lastowner = owner_hint;
    if (bmCopy) LICE_Copy(img->bgimage, bmCopy);

    cache->InsertSorted(img, WDL_VirtualWnd_BGCfgCache_img::compar);
    return img->bgimage;
  }
  return NULL;
}

void WDL_VirtualWnd_PreprocessBGConfig(WDL_VirtualWnd_BGCfg *a)
{
  if (!a) return;

  if (!a->bgimage) return;
  a->bgimage_lt[0]=a->bgimage_lt[1]=a->bgimage_rb[0]=a->bgimage_rb[1]=0;
  a->bgimage_lt_out[0]=a->bgimage_lt_out[1]=a->bgimage_rb_out[0]=a->bgimage_rb_out[1]=1;

  int w=a->bgimage->getWidth();
  int h=a->bgimage->getHeight();
  if (w>1&&h>1 && LICE_GetPixel(a->bgimage,0,0)==LICE_RGBA(255,0,255,255) &&
      LICE_GetPixel(a->bgimage,w-1,h-1)==LICE_RGBA(255,0,255,255))
  {
    int x;
    for (x = 1; x < w && LICE_GetPixel(a->bgimage,x,0)==LICE_RGBA(255,0,255,255); x ++);
    a->bgimage_lt[0] = x;
    for (x = w-2; x >= a->bgimage_lt[0]+1 && LICE_GetPixel(a->bgimage,x,h-1)==LICE_RGBA(255,0,255,255); x --);
    a->bgimage_rb[0] = w-1-x;

    for (x = 1; x < h && LICE_GetPixel(a->bgimage,0,x)==LICE_RGBA(255,0,255,255); x ++);
    a->bgimage_lt[1] = x;
    for (x = h-2; x >= a->bgimage_lt[1]+1 && LICE_GetPixel(a->bgimage,w-1,x)==LICE_RGBA(255,0,255,255); x --);
    a->bgimage_rb[1] = h-1-x;
  }
  else if (w>1&&h>1 && LICE_GetPixel(a->bgimage,0,0)==LICE_RGBA(255,255,0,255) &&
          LICE_GetPixel(a->bgimage,w-1,h-1)==LICE_RGBA(255,255,0,255))
  {

    bool hadPink=false;

    //graphic image contains an outside area -- must contain at least one pink pixel in its definition or we assume it's just a yellow image...
    int x, x2, x3;
    for (x = 1, x2 = 0, x3 = 0; x < w; x++)
    {
      LICE_pixel p = LICE_GetPixel(a->bgimage,x,0);
      if(p==LICE_RGBA(255,0,255,255)) { hadPink=true; x2++; }
      else if(p==LICE_RGBA(255,255,0,255)) { x3+=x2+1; x2=0; }
      else break;
    }
    a->bgimage_lt[0] = x2+1;
    a->bgimage_lt_out[0] = x3+1;
    for (x = w-2, x2 = 0, x3 = 0; x >= a->bgimage_lt[0]+1; x--)
    {
      LICE_pixel p = LICE_GetPixel(a->bgimage,x,h-1);
      if(p==LICE_RGBA(255,0,255,255)) { hadPink=true; x2++; }
      else if(p==LICE_RGBA(255,255,0,255)) { x3+=x2+1; x2=0; }
      else break;
    }
    a->bgimage_rb[0] = x2+1;
    a->bgimage_rb_out[0] = x3+1;
    
    for (x = 1, x2 = 0, x3 = 0; x < h;x++)
    {
      LICE_pixel p = LICE_GetPixel(a->bgimage,0,x);
      if(p==LICE_RGBA(255,0,255,255)) { hadPink=true; x2++; }
      else if(p==LICE_RGBA(255,255,0,255)) { x3+=x2+1; x2=0; }
      else break;
    }
    a->bgimage_lt[1] = x2+1;
    a->bgimage_lt_out[1] = x3+1;
    for (x = h-2, x2 = 0, x3 = 0; x >= a->bgimage_lt[1]+1; x --)
    {
      LICE_pixel p = LICE_GetPixel(a->bgimage,w-1,x);
      if(p==LICE_RGBA(255,0,255,255)) { hadPink=true; x2++; }
      else if(p==LICE_RGBA(255,255,0,255)) { x3+=x2+1; x2=0; }
      else break;
    }
    a->bgimage_rb[1] = x2+1;
    a->bgimage_rb_out[1] = x3+1;
    if (!hadPink) // yellow by itself isnt enough, need at least a bit of pink.
    {
      a->bgimage_lt[0]=a->bgimage_lt[1]=a->bgimage_rb[0]=a->bgimage_rb[1]=0;
      a->bgimage_lt_out[0]=a->bgimage_lt_out[1]=a->bgimage_rb_out[0]=a->bgimage_rb_out[1]=1;            
    }
  }


  int flags=0xffff;
  LICE_pixel_chan *ch = (LICE_pixel_chan *) a->bgimage->getBits();
  int span = a->bgimage->getRowSpan()*4;
  if (a->bgimage->isFlipped())
  {
    ch += span*(h-1);
    span=-span;
  }

  // not sure if this works yet -- it needs more testing for sure
  bool isFull=true;
  if (a->bgimage_lt[0] ||a->bgimage_lt[1] || a->bgimage_rb[0] || a->bgimage_rb[1])
  {
    isFull=false;
    ch += span; // skip a line
    ch += 4; // skip a column
    h-=2;
    w-=2;
  }

  // points at which we change to the next block
  int xdivs[3] = { a->bgimage_lt[0]+a->bgimage_lt_out[0]-2, 
                   w-a->bgimage_rb[0]-a->bgimage_rb_out[0]+2, 
                   w-a->bgimage_rb_out[0]+1};
  int ydivs[3] = { a->bgimage_lt[1]+a->bgimage_lt_out[1]-2, 
                   h-a->bgimage_rb[1]-a->bgimage_rb_out[1]+2, 
                   h-a->bgimage_rb_out[1]+1};

  int y,ystate=0;
  for(y=0;y<h;y++)
  {
    while (ystate<3 && y>=ydivs[ystate]) ystate++;
    int xstate=0;

    int x;
    LICE_pixel_chan *chptr = ch + LICE_PIXEL_A;
    for (x=0;x<w;x++)
    {
      while (xstate<3 && x>=xdivs[xstate]) xstate++;

      if (*chptr != 255)
      {
        if (isFull) 
        {
          flags=0;
          break;
        }
        else 
        {
          flags &= ~(1<<(ystate*4 + xstate));
          if (!flags) break;
        }
      }
      chptr+=4;
    }
    if (!flags) break;

    ch += span;
  }

  a->bgimage_noalphaflags=flags;

}

static void __VirtClipBlit(int clipx, int clipy, int clipright, int clipbottom,
                           LICE_IBitmap *dest, LICE_IBitmap *src, int dstx, int dsty, int dstw, int dsth, 
                           int _srcx, int _srcy, int _srcw, int _srch, float alpha, int mode)
{
  if (dstw<1||dsth<1 || dstx+dstw < clipx || dstx > clipright ||
      dsty+dsth < clipy || dsty > clipbottom) 
  {
    return; // dont draw if fully outside
  }

  double srcx = (double) _srcx;
  double srcy = (double) _srcy;
  double srcw = (double) _srcw;
  double srch = (double) _srch;

  if (dstx < clipx || dsty < clipy || dstx+dstw > clipright || dsty+dsth > clipbottom) 
  {
    double xsc=srcw/dstw;
    double ysc=srch/dsth;

    if (dstx<clipx)
    {
      int diff=clipx-dstx;
      srcx += xsc*diff;
      srcw -= xsc*diff;
      dstw -= diff;
      dstx += diff;
    }
    if (dsty<clipy)
    {
      int diff=clipy-dsty;
      srcy += ysc*diff;
      srch -= ysc*diff;
      dsth -= diff;
      dsty += diff;
    }
    if (dstx+dstw > clipright)
    {
      int diff = dstx+dstw-clipright; //clipright-dstx-dstw;
      dstw -= diff;
      srcw -= diff*xsc;
    }
    if (dsty+dsth > clipbottom)
    {
      int diff = dsty+dsth-clipbottom; //clipbottom-dsty-dsth;
      dsth -= diff;
      srch -= diff*ysc;
    }

  }

  if (dstw>0&&dsth>0)
  {
    const double eps=0.0005;
    if (srcw > 0.0 && srcw < eps) srcw=eps;
    if (srch > 0.0 && srch < eps) srch=eps;
    LICE_ScaledBlit(dest,src,dstx,dsty,dstw,dsth,(float)srcx,(float)srcy,(float)srcw,(float)srch,alpha,mode);
  }
}

void WDL_VirtualWnd_ScaledBlitSubBG(LICE_IBitmap *dest,
                                    WDL_VirtualWnd_BGCfg *src,
                                    int destx, int desty, int destw, int desth,
                                    int clipx, int clipy, int clipw, int cliph,
                                    int srcx, int srcy, int srcw, int srch, // these coordinates are not including pink lines (i.e. if pink lines are present, use src->bgimage->getWidth()-2, etc)
                                    float alpha, int mode)
{
  if (!src || !src->bgimage) return;

  int adj=2;
  if (src->bgimage_lt[0] < 1 || src->bgimage_lt[1] < 1 || src->bgimage_rb[0] < 1 || src->bgimage_rb[1] < 1) 
  {
    adj=0;
  }
  if (srcx == 0 && srcy == 0 && srcw+adj >= src->bgimage->getWidth()  && srch+adj >= src->bgimage->getHeight()) 
  {
    WDL_VirtualWnd_ScaledBlitBG(dest,src,destx,desty,destw,desth,clipx,clipy,clipw,cliph,alpha,mode);
    return;
  }

  LICE_SubBitmap bm(src->bgimage,srcx,srcy,srcw+adj,srch+adj);
  WDL_VirtualWnd_BGCfg ts = *src;
  ts.bgimage = &bm;

  if ((ts.bgimage_noalphaflags&0xffff)!=0xffff) ts.bgimage_noalphaflags=0;  // force alpha channel if any alpha

  WDL_VirtualWnd_ScaledBlitBG(dest,&ts,destx,desty,destw,desth,clipx,clipy,clipw,cliph,alpha,mode);
}

void WDL_VirtualWnd_ScaledBlitBG(LICE_IBitmap *dest, 
                                 WDL_VirtualWnd_BGCfg *src,
                                 int destx, int desty, int destw, int desth,
                                 int clipx, int clipy, int clipw, int cliph,
                                 float alpha, int mode)
{
  // todo: blit clipping optimizations
  if (!src || !src->bgimage) return;

  int left_margin=src->bgimage_lt[0];
  int top_margin=src->bgimage_lt[1];
  int right_margin=src->bgimage_rb[0];
  int bottom_margin=src->bgimage_rb[1];

  int left_margin_out=src->bgimage_lt_out[0];
  int top_margin_out=src->bgimage_lt_out[1];
  int right_margin_out=src->bgimage_rb_out[0];
  int bottom_margin_out=src->bgimage_rb_out[1];

  int sw=src->bgimage->getWidth();
  int sh=src->bgimage->getHeight();

  int clipright=clipx+clipw;
  int clipbottom=clipy+cliph;

  if (clipx<destx) clipx=destx;
  if (clipy<desty) clipy=desty;
  if (clipright>destx+destw) clipright=destx+destw;
  if (clipbottom>desty+desth) clipbottom=desty+desth;
  
  if (left_margin<1||top_margin<1||right_margin<1||bottom_margin<1) 
  {
    float xsc=(float)sw/destw;
    float ysc=(float)sh/desth;

    if (mode&LICE_BLIT_USE_ALPHA)
    {
      if ((src->bgimage_noalphaflags & 0xffff)==0xffff)
      {
        mode &= ~LICE_BLIT_USE_ALPHA;
      }
    }


    LICE_ScaledBlit(dest,src->bgimage,
      clipx,clipy,clipright-clipx,clipbottom-clipy,
      (clipx-destx)*xsc,
      (clipy-desty)*ysc,
      (clipright-clipx)*xsc,
      (clipbottom-clipy)*ysc,
      alpha,mode);

    return;
  }

  // remove 1px additional margins from calculations
  left_margin--; top_margin--; right_margin--; bottom_margin--;
  left_margin_out--; top_margin_out--; right_margin_out--; bottom_margin_out--;

  if (left_margin+right_margin>destw) 
  { 
    int w=left_margin+right_margin;
    left_margin = destw*left_margin/wdl_max(w,1);
    right_margin=destw-left_margin; 
  }
  if (top_margin+bottom_margin>desth) 
  { 
    int h=(top_margin+bottom_margin);
    top_margin=desth*top_margin/wdl_max(h,1);
    bottom_margin=desth-top_margin; 
  }

  int no_alpha_flags=src->bgimage_noalphaflags;
  int pass;
  int nbpass = 3;
  if (bottom_margin_out>0) 
    nbpass = 4;

  bool no_inside = !!(mode & WDL_VWND_SCALEDBLITBG_IGNORE_INSIDE);
  bool no_outside = !!(mode & WDL_VWND_SCALEDBLITBG_IGNORE_OUTSIDE);

  bool no_lr=!!(mode & WDL_VWND_SCALEDBLITBG_IGNORE_LR);

  int __sc = (int) dest->Extended(LICE_EXT_GET_ADVISORY_SCALING,NULL);
  if (__sc < 1) __sc=256;
  const int lm = (left_margin*__sc)>>8, 
            rm = (right_margin*__sc)>>8, 
            lmod = (left_margin_out*__sc)>>8, 
            rmod = (right_margin_out*__sc)>>8;

  for (pass=(top_margin_out> 0 ? -1 : 0); pass<nbpass; pass++)
  {
    int outy, outh;
    int iny, inh;    
    int this_clipy = clipy;
    switch (pass)
    {
      case -1: // top margin
        outh = (top_margin_out*__sc) >> 8;
        outy = desty - outh;
        this_clipy-=outh;
        iny=1;
        inh=top_margin_out;
      break;
      case 0:
        outy=desty;
        outh=(top_margin*__sc) >> 8;
        iny=1+top_margin_out;
        inh=src->bgimage_lt[1]-1;
      break;
      case 1:
        outy=desty+((top_margin*__sc)>>8);
        outh=desty+desth-outy-((bottom_margin*__sc)>>8);
        iny=src->bgimage_lt[1]+top_margin_out;
        inh=sh-src->bgimage_rb[1]-bottom_margin_out - iny;
      break;
      case 2:
        outh=(bottom_margin*__sc)>>8;
        outy=desty+desth-outh;
        iny=sh - src->bgimage_rb[1] - bottom_margin_out;
        inh=src->bgimage_rb[1]-1;
      break;
      case 3:
        outh=(bottom_margin_out*__sc)>>8;
        clipbottom += outh;
        outy=desty+desth;
        iny=sh-1-bottom_margin_out;
        inh=bottom_margin_out;
      break;
    }
    bool is_outer = pass<0 || pass>=3;

    if (no_outside && is_outer)
    {
    }
    else if (outh > 0 && inh > 0)
    {

      if (no_lr)
      {
        __VirtClipBlit(clipx,this_clipy,clipright,clipbottom,dest,src->bgimage,destx,outy,
                                destw,outh,
                             src->bgimage_lt[0]+left_margin_out,iny,
                             sw-src->bgimage_lt[0]-src->bgimage_rb[0]-left_margin_out - right_margin_out,
                             inh,alpha,(no_alpha_flags&2) ? (mode&~LICE_BLIT_USE_ALPHA) :  mode);
      }
      else
      {
        // left 
        if (left_margin_out>0 && !no_outside)
        {
          __VirtClipBlit(clipx-lmod,this_clipy,clipright,clipbottom,dest,src->bgimage,destx-lmod,outy,lmod,outh,
                               1,iny,left_margin_out,inh,alpha,
                               (no_alpha_flags&1) ? (mode&~LICE_BLIT_USE_ALPHA) :  mode);
        }

        if (!no_inside||is_outer)
        {
          if (left_margin > 0)
            __VirtClipBlit(clipx,this_clipy,clipright,clipbottom,dest,src->bgimage,destx,outy,lm,outh,
                                 1+left_margin_out,iny,src->bgimage_lt[0]-1,inh,alpha,
                                 (no_alpha_flags&1) ? (mode&~LICE_BLIT_USE_ALPHA) :  mode);


          // center
          __VirtClipBlit(clipx,this_clipy,clipright,clipbottom,dest,src->bgimage,destx+lm,outy,
                                  destw-rm-lm,outh,
                               src->bgimage_lt[0]+left_margin_out,iny,
                               sw-src->bgimage_lt[0]-src->bgimage_rb[0]-right_margin_out-left_margin_out,
                               inh,alpha,(no_alpha_flags&2) ? (mode&~LICE_BLIT_USE_ALPHA) :  mode);
          // right
          if (right_margin > 0)
            __VirtClipBlit(clipx,this_clipy,clipright,clipbottom,dest,src->bgimage,destx+destw-rm,outy, rm,outh,
                                 sw-src->bgimage_rb[0]-right_margin_out,iny,
                                 src->bgimage_rb[0]-1,inh,alpha,(no_alpha_flags&4) ? (mode&~LICE_BLIT_USE_ALPHA) :  mode); 
        }

        // right outside area
        if (right_margin_out>0 && !no_outside)
        {
          __VirtClipBlit(clipx,this_clipy,clipright+rmod,clipbottom,dest,src->bgimage,destx+destw,outy,rmod,outh,
            sw-right_margin_out-1,iny,
            right_margin_out,inh,alpha,(no_alpha_flags&8) ? (mode&~LICE_BLIT_USE_ALPHA) :  mode); 
        }

      }
    }
    if (pass>=0)
      no_alpha_flags>>=4;
  }
}

int WDL_VirtualWnd_ScaledBG_GetPix(WDL_VirtualWnd_BGCfg *src,
                                   int ww, int wh,
                                   int x, int y)
{
  if (!src->bgimage) return 0;
  int imgw=src->bgimage->getWidth();
  int imgh=src->bgimage->getHeight();

  int left_margin=src->bgimage_lt[0];
  int top_margin=src->bgimage_lt[1];
  int right_margin=src->bgimage_rb[0];
  int bottom_margin=src->bgimage_rb[1];

  if (left_margin<1||top_margin<1||right_margin<1||bottom_margin<1) 
  {
    if (ww<1)ww=1;
    x=(x * imgw)/ww;
    if (wh<1)wh=1;
    y=(y * imgh)/wh;
  }
  else
  {
    // remove 1px additional margins from calculations
    left_margin--; top_margin--; right_margin--; bottom_margin--;
    int destw=ww,desth=wh;
    if (left_margin+right_margin>destw) 
    { 
      int w=left_margin+right_margin;
      left_margin = destw*left_margin/wdl_max(w,1);
      right_margin=destw-left_margin; 
    }
    if (top_margin+bottom_margin>desth) 
    { 
      int h=(top_margin+bottom_margin);
      top_margin=desth*top_margin/wdl_max(h,1);
      bottom_margin=desth-top_margin; 
    }

    if (x >= ww-right_margin) x=imgw-1- (ww-x);
    else if (x >= left_margin)
    {
      int xd=ww-left_margin-right_margin;
      if (xd<1)xd=1;
      x=src->bgimage_lt[0] + 
        (x-left_margin) * (imgw-src->bgimage_lt[0]-src->bgimage_rb[0])/xd;
    }
    else x++; 

    if (y >= wh-bottom_margin) y=imgh -1- (wh-y);
    else if (y >= top_margin)
    {
      int yd=wh-top_margin-bottom_margin;
      if (yd<1)yd=1;
      y=src->bgimage_lt[1] + 
        (y-top_margin) * (imgh-src->bgimage_lt[1]-src->bgimage_rb[1])/yd;
    }
    else y++; 
  }

  return LICE_GetPixel(src->bgimage,x,y);
}


#ifdef _WIN32

static WNDPROC vwndDlgHost_oldProc;
static LRESULT CALLBACK vwndDlgHost_newProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  if (msg==WM_ERASEBKGND) return 1;
  if (msg==WM_PAINT)
  {
    WNDPROC pc=(WNDPROC)GetWindowLongPtr(hwnd,DWLP_DLGPROC);
    if (pc)
    {
      CallWindowProc(pc,hwnd,msg,wParam,lParam);
      return 0;
    }
  }   

  return CallWindowProc(vwndDlgHost_oldProc,hwnd,msg,wParam,lParam);
}

#endif

void WDL_VWnd_regHelperClass(const char *classname, void *icon1, void *icon2)
{
#ifdef _WIN32
  static bool reg;
  if (reg) return;

  reg=true;

  WNDCLASSEX wc={sizeof(wc),};
  GetClassInfoEx(NULL,"#32770",&wc);
  wc.lpszClassName = (char*)classname;
  if (icon1) wc.hIcon = (HICON)icon1;
  if (icon2) wc.hIconSm = (HICON)icon2;
  vwndDlgHost_oldProc=wc.lpfnWndProc;
  wc.lpfnWndProc=vwndDlgHost_newProc;
  RegisterClassEx(&wc);
#endif
}

bool wdl_virtwnd_nosetcursorpos;
