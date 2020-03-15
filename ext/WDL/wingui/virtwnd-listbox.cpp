/*
    WDL - virtwnd-listbox.cpp
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
      

    Implementation for virtual window listboxes.

*/

#include "virtwnd-controls.h"
#include "../lice/lice.h"

WDL_VirtualListBox::WDL_VirtualListBox()
{
  memset(m_lastscrollbuttons,0,sizeof(m_lastscrollbuttons));
  m_scrollbuttonsize=14;
  m_cap_startitem=-1;
  m_cap_state=0;
  m_cap_startpos.x = m_cap_startpos.y = 0;
  m_margin_l=m_margin_r=0;
  m_GetItemInfo=0;
  m_CustomDraw=0;
  m_GetItemHeight=NULL;
  m_GetItemInfo_ctx=0;
  m_viewoffs=0;
  m_align=-1;
  m_rh=14;
  m_maxcolwidth=m_mincolwidth=0;
  m_lsadj=-1000;
  m_font=0;
  m_clickmsg=0;
  m_dropmsg=0;
  m_dragmsg=0;
  m_grayed=false;
}

WDL_VirtualListBox::~WDL_VirtualListBox()
{
}

static bool items_fit(int test_h, const int *heights, int heights_sz, int h, int max_cols, int rh_base)
{
  int y=test_h, cols=1;
  for (int item = 0; item < heights_sz; item ++)
  {
    int rh = heights[item];
    if (y > 0 && y + rh > h) 
    {
      if (max_cols == 1 && y + rh_base <= h) return item == heights_sz-1; // allow partial of larger item in single column mode
      if (++cols > max_cols) return false;
      y=0;
    }
    y += rh;
  }
  return true;
}

void WDL_VirtualListBox::CalcLayout(int num_items, layout_info *layout)
{
  const int w = m_position.right - m_position.left, h = m_position.bottom - m_position.top;

  int max_cols = 1, min_cols = 1, max_cols2 = 1;
  if (m_mincolwidth>0) 
  {
    max_cols = w / m_mincolwidth;
    max_cols2 = (w - m_scrollbuttonsize*2) / m_mincolwidth;
    if (max_cols < 1) max_cols = 1;
    if (max_cols2 < 1) max_cols2 = 1;
  }
  if (m_maxcolwidth>0)
  {
    min_cols = w / m_maxcolwidth;
    if (min_cols < 1) min_cols = 1;
  }
  if (max_cols < min_cols) max_cols = min_cols;

  static WDL_TypedBuf<int> s_heights;

  s_heights.Resize(0,false);

  int startitem = wdl_min(m_viewoffs,num_items-1), cols = 1, y = 0;
  if (startitem < 0) startitem = 0;

  const int rh_base = GetRowHeight();
  int item;
  for (item = startitem; item < num_items; item ++)
  {
    int rh = GetItemHeight(item);
    if (y > 0 && y + rh > h) 
    {
      if (max_cols == 1 && y + rh_base <= h) s_heights.Add(rh); // allow partial of larger item in single column mode
      if (cols >= max_cols) break;
      cols++;
      y=0;
    }
    s_heights.Add(rh);
    y += rh;
  }
  if (item >= num_items)
  {
    while (startitem > 0)
    {
      const int use_h = h - ((max_cols == 1 && startitem>1) ? m_scrollbuttonsize : 0);

      int rh = GetItemHeight(startitem-1);
      if (!items_fit(rh,s_heights.Get(),s_heights.GetSize(),use_h,max_cols2,rh_base)) break;
      s_heights.Insert(rh,0);
      startitem--;
    }
  }
  const bool has_scroll = item < num_items || startitem > 0;

  if (has_scroll && cols > 1)
  {
    max_cols = max_cols2;
    if (max_cols < min_cols) max_cols = min_cols;
    if (cols > max_cols) cols = max_cols;
  }
  if (cols < min_cols) cols = min_cols;
  layout->startpos = startitem;
  layout->columns = cols;
  layout->heights = &s_heights;
  int scroll_mode = has_scroll && cols > 0 ? cols == 1 ? 1 : 2 : 0;

  if (scroll_mode == 1 && h - m_scrollbuttonsize < rh_base && w - m_scrollbuttonsize*2 >= m_mincolwidth/2)
    scroll_mode = 2;

  layout->updownbutton_h = scroll_mode == 1 ? m_scrollbuttonsize : 0;
  layout->leftrightbutton_w = scroll_mode == 2 ? m_scrollbuttonsize : 0;
  layout->item_area_w = w - 2*layout->leftrightbutton_w;
  layout->item_area_h = h - layout->updownbutton_h;
}

static void maxSizePreservingAspect(int sw, int sh, int dw, int dh, int *outw, int *outh)
{
  *outw=dw;
  *outh=dh;
  if (sw < 1 || sh < 1) return;
  int xwid = (sw * dh) / sh; // calculate width required if we make it dh pixels tall
  if (xwid > dw)
  {
    // too wide, make maximum width and reduce height
    *outh = (dw * sh) / sw;
  }
  else 
  {
    // too narrow, use full height and reduce width
    *outw = (dh * sw) / sh;
  }
}

static void DrawBkImage(LICE_IBitmap *drawbm, WDL_VirtualWnd_BGCfg *bkbm, int drawx, int drawy, int draww, int drawh,
                RECT *cliprect, int drawsrcx, int drawsrcw, int bkbmstate, float alpha, int whichpass=0)
{
  bool haspink=bkbm->bgimage_lt[0]||bkbm->bgimage_lt[1]||bkbm->bgimage_rb[0] || bkbm->bgimage_rb[1];

  if (whichpass==1)
  {
    int sw = (bkbm->bgimage->getWidth() - (haspink? 2 :0))/2;
    int sh = (bkbm->bgimage->getHeight() - (haspink? 2 :0))/3;

    int usew,useh;
    // scale drawing coords by image dimensions
    maxSizePreservingAspect(sw,sh,draww,drawh,&usew,&useh);

    if (usew == sw-1 || usew == sw+1) usew=sw;
    if (useh == sh-1 || useh == sh+1) useh=sh;
    drawx += (draww-usew)/2;
    drawy += (drawh-useh)/2;
    draww = usew;
    drawh = useh;
  }

  int hh=bkbm->bgimage->getHeight()/3;

  if (haspink)
  {
    WDL_VirtualWnd_BGCfg tmp = *bkbm;
    if ((tmp.bgimage_noalphaflags&0xffff)!=0xffff) tmp.bgimage_noalphaflags=0;  // force alpha channel if any alpha

    if (drawsrcx>0) { drawsrcx--; drawsrcw++; }
    LICE_SubBitmap bm(tmp.bgimage,drawsrcx,bkbmstate*hh,drawsrcw+1,hh+2);
    tmp.bgimage = &bm;

    WDL_VirtualWnd_ScaledBlitBG(drawbm,&tmp,
                                  drawx,drawy,draww,drawh,
                                  cliprect->left,cliprect->top,cliprect->right-cliprect->left,cliprect->bottom-cliprect->top,
                                  alpha,LICE_BLIT_USE_ALPHA|LICE_BLIT_MODE_COPY|LICE_BLIT_FILTER_BILINEAR);
  }
  else
  {
    LICE_ScaledBlit(drawbm,bkbm->bgimage,
      drawx,drawy,draww,drawh,
      (float)drawsrcx,bkbmstate*(float)hh,
      (float)drawsrcw,(float)hh,alpha,LICE_BLIT_USE_ALPHA|LICE_BLIT_MODE_COPY|LICE_BLIT_FILTER_BILINEAR);
  }
}


void WDL_VirtualListBox::OnPaint(LICE_IBitmap *drawbm, int origin_x, int origin_y, RECT *cliprect, int rscale)
{
  RECT r;
  WDL_VWnd::GetPositionPaintExtent(&r,rscale);
  r.left+=origin_x;
  r.right+=origin_x;
  r.top+=origin_y;
  r.bottom+=origin_y;

  WDL_VirtualWnd_BGCfg *mainbk=0;
  int num_items = m_GetItemInfo ? m_GetItemInfo(this,-1,NULL,0,NULL,&mainbk) : 0;
  LICE_pixel bgc=GSC(COLOR_BTNFACE);
  bgc=LICE_RGBA_FROMNATIVE(bgc,255);

  layout_info layout;
  CalcLayout(num_items,&layout);
  m_viewoffs = layout.startpos; // latch to new startpos

  const int usedw = layout.item_area_w * rscale / WDL_VWND_SCALEBASE;
  const int leftrightbuttonsize = layout.leftrightbutton_w * rscale / WDL_VWND_SCALEBASE;
  const int updownbuttonsize = layout.updownbutton_h * rscale / WDL_VWND_SCALEBASE;
  const int endpos=layout.item_area_h * rscale / WDL_VWND_SCALEBASE;
  const int startpos = layout.startpos;
  const int rh_base = GetRowHeight();

  const int sbs = m_scrollbuttonsize * rscale / WDL_VWND_SCALEBASE;

  if (r.right > r.left + usedw+2*leftrightbuttonsize) r.right=r.left+usedw+2*leftrightbuttonsize;

  if (mainbk && mainbk->bgimage)
  {
    if (mainbk->bgimage->getWidth()>1 && mainbk->bgimage->getHeight()>1)
    {
      WDL_VirtualWnd_ScaledBlitBG(drawbm,mainbk,
                                    r.left,r.top,r.right-r.left,r.bottom-r.top,
                                    cliprect->left,cliprect->top,cliprect->right-cliprect->left,cliprect->bottom-cliprect->top,
                                    1.0,LICE_BLIT_USE_ALPHA|LICE_BLIT_MODE_COPY|LICE_BLIT_FILTER_BILINEAR);
    }
  }
  else
  {
    LICE_FillRect(drawbm,r.left,r.top,r.right-r.left,r.bottom-r.top,bgc,1.0f,LICE_BLIT_MODE_COPY);
  }

  LICE_pixel pencol = GSC(COLOR_3DSHADOW);
  LICE_pixel pencol2 = GSC(COLOR_3DHILIGHT);
  pencol=LICE_RGBA_FROMNATIVE(pencol,255);
  pencol2=LICE_RGBA_FROMNATIVE(pencol2,255);

  LICE_pixel tcol=GSC(COLOR_BTNTEXT);
  if (m_font) 
  {
    m_font->SetBkMode(TRANSPARENT);
    if (m_lsadj != -1000)
      m_font->SetLineSpacingAdjust(m_lsadj);
  }

  float alpha = (m_grayed ? 0.25f : 1.0f);

  int itempos=startpos;
  
  const int num_cols = layout.columns;
  for (int colpos = 0; colpos < num_cols; colpos ++)
  {
    int col_x = r.left + leftrightbuttonsize + (usedw*colpos) / num_cols;
    int col_w = r.left + leftrightbuttonsize + (usedw*(colpos+1)) / num_cols - col_x;
    int y=r.top;
    for (;;)
    {
      int ly=y;
      const int idx = itempos-startpos;
      const int rh = ((idx >= 0 && idx < layout.heights->GetSize()) ? 
           layout.heights->Get()[idx] : 
         GetItemHeight(itempos)) * rscale / WDL_VWND_SCALEBASE;
      y += rh;
      if (y > r.top+endpos) 
      {
        if (y-rh + rh_base > r.top+endpos) break;
        if (colpos < num_cols-1) break;
        y = r.top+endpos; // size expanded-sized item to fit
      }

      WDL_VirtualWnd_BGCfg *bkbm=0;
      if (m_GetItemInfo && ly >= r.top)
      {
        char buf[64];
        buf[0]=0;
        int color=tcol;

        if (m_GetItemInfo(this,itempos++,buf,sizeof(buf),&color,&bkbm))
        {
          color=LICE_RGBA_FROMNATIVE(color,0);
          RECT thisr;
          thisr.left = col_x;
          thisr.right = col_x + col_w;
          thisr.top = ly+1;
          thisr.bottom = y-1;
          int rev=0;
          int bkbmstate=0;
          if (m_cap_state==1 && m_cap_startitem==itempos-1)
          {
            if (bkbm) bkbmstate=1;
            else color = ((color>>1)&0x7f7f7f7f)+LICE_RGBA(0x7f,0x7f,0x7f,0);
          }
          if (m_cap_state>=0x1000 && m_cap_startitem==itempos-1)
          {
            if (bkbm) bkbmstate=2;
            else
            {
              rev=1;
              LICE_FillRect(drawbm,thisr.left,thisr.top,thisr.right-thisr.left,thisr.bottom-thisr.top, color,alpha,LICE_BLIT_MODE_COPY);
            }
          }
          if (bkbm && bkbm->bgimage) //draw image!
          {            
            DrawBkImage(drawbm,bkbm,
                thisr.left,thisr.top-1,thisr.right-thisr.left,thisr.bottom-thisr.top+2,
                cliprect,
                0,bkbm->bgimage->getWidth(),bkbmstate,alpha);

          }
          if (m_CustomDraw)
          {
            m_CustomDraw(this,itempos-1,&thisr,drawbm,rscale);
          }

          if (buf[0])
          {
            thisr.left+=m_margin_l;
            thisr.right-=m_margin_r;
            if (m_font)
            {
              m_font->SetTextColor(rev?bgc:color);
              m_font->SetCombineMode(LICE_BLIT_MODE_COPY, alpha); // maybe gray text only if !bkbm->bgimage
              if (m_align == 0)
              {
                RECT r2={0,};
                m_font->DrawText(drawbm,buf,-1,&r2,DT_CALCRECT|DT_NOPREFIX);
                m_font->DrawText(drawbm,buf,-1,&thisr,DT_VCENTER|((r2.right <= thisr.right-thisr.left) ? DT_CENTER : DT_LEFT)|DT_NOPREFIX);
              }
              else
                m_font->DrawText(drawbm,buf,-1,&thisr,DT_VCENTER|(m_align<0?DT_LEFT:DT_RIGHT)|DT_NOPREFIX);
            }
          }
        }
      }

      if (!bkbm)
      {
        LICE_Line(drawbm,col_x,y,col_x+col_w,y,pencol2,1.0f,LICE_BLIT_MODE_COPY,false);
      }
    }
  }
  memset(&m_lastscrollbuttons,0,sizeof(m_lastscrollbuttons));
  if (updownbuttonsize||leftrightbuttonsize)
  {
    int y = r.top + layout.item_area_h * rscale / WDL_VWND_SCALEBASE;
    WDL_VirtualWnd_BGCfg *bkbm[2]={0,};
    int a=m_GetItemInfo ? m_GetItemInfo(this,
      leftrightbuttonsize ? WDL_VWND_LISTBOX_ARROWINDEX_LR : WDL_VWND_LISTBOX_ARROWINDEX,NULL,0,NULL,bkbm) : 0;

    if (!a) bkbm[0]=0;

    if (leftrightbuttonsize)
    {
      RECT br={0,0,(r.right-r.left) * WDL_VWND_SCALEBASE /rscale,(r.bottom-r.top)*WDL_VWND_SCALEBASE/rscale};

      if (startpos>0)
      {
        m_lastscrollbuttons[0]=br;
        m_lastscrollbuttons[0].right = br.left + m_scrollbuttonsize;
      }
      if (itempos < num_items)
      {
        m_lastscrollbuttons[1]=br;
        m_lastscrollbuttons[1].left=br.right - m_scrollbuttonsize;
      }
    }
    else 
    {
      RECT br={0,
        (y-r.top) * WDL_VWND_SCALEBASE/rscale,
        (r.right-r.left) * WDL_VWND_SCALEBASE / rscale,
        (y-r.top + sbs) * WDL_VWND_SCALEBASE/rscale
      };
      if (startpos>0)
      {
        m_lastscrollbuttons[0]=br;
        m_lastscrollbuttons[0].right = (br.left+br.right)/2;
      }
      if (itempos < num_items)
      {
        m_lastscrollbuttons[1]=br;
        m_lastscrollbuttons[1].left = (br.left+br.right)/2;
      }
    }


    int wb;
    for (wb=0;wb<2;wb++)
    {
      if (bkbm[wb] && bkbm[wb]->bgimage)
      {
        int tw = bkbm[wb]->bgimage->getWidth();
        int bkbmstate=startpos>0 ? 2 : 1;
        if (leftrightbuttonsize)
        {
          DrawBkImage(drawbm,bkbm[wb],
              r.left,r.top,sbs,(r.bottom-r.top),
              cliprect,
              0,tw/2,bkbmstate,1.0, wb);


          bkbmstate=itempos<num_items ? 2 : 1;
          DrawBkImage(drawbm,bkbm[wb],
              r.right-sbs,r.top,sbs,(r.bottom-r.top),
              cliprect,
              tw/2,tw - tw/2,bkbmstate,1.0,wb);
        }
        else
        {
          DrawBkImage(drawbm,bkbm[wb],
              r.left,y,(r.right-r.left)/2,sbs,
              cliprect,
              0,tw/2,bkbmstate,1.0,wb);
  
          bkbmstate=itempos<num_items ? 2 : 1;
          DrawBkImage(drawbm,bkbm[wb],
              (r.left+r.right)/2,y,(r.right-r.left) - (r.right-r.left)/2,sbs,
              cliprect,
              tw/2,tw - tw/2,bkbmstate,1.0,wb);
        }
      }
    }

    if (!bkbm[0] || !bkbm[0]->bgimage ||
        !bkbm[1] || !bkbm[1]->bgimage)
    {
      bool butaa = true;
      if (updownbuttonsize)
      {
        int cx=(r.left+r.right)/2;
        int bs=5 * rscale / WDL_VWND_SCALEBASE;
        int bsh=8 * rscale / WDL_VWND_SCALEBASE;
        y += sbs;

        if (!bkbm[0] || !bkbm[0]->bgimage)
        {
          LICE_Line(drawbm,cx,y-sbs+2,cx,y-1,pencol2,1.0f,0,false);
          LICE_Line(drawbm,r.left,y,r.right,y,pencol2,1.0f,0,false);
        }
      
        y-=sbs/2+bsh/2;

        if (!bkbm[1] || !bkbm[1]->bgimage)
        {
          if (itempos<num_items)
          {
            cx=(r.left+r.right)*3/4;

            LICE_Line(drawbm,cx-bs+1,y+2,cx,y+bsh-2,pencol2,1.0f,0,butaa);
            LICE_Line(drawbm,cx,y+bsh-2,cx+bs-1,y+2,pencol2,1.0f,0,butaa);
            LICE_Line(drawbm,cx+bs-1,y+2,cx-bs+1,y+2,pencol2,1.0f,0,butaa);

            LICE_Line(drawbm,cx-bs-1,y+1,cx,y+bsh-1,pencol,1.0f,0,butaa);
            LICE_Line(drawbm,cx,y+bsh-1,cx+bs+1,y+1,pencol,1.0f,0,butaa);
            LICE_Line(drawbm,cx+bs+1,y+1,cx-bs-1,y+1,pencol,1.0f,0,butaa);
          }
          if (startpos>0)
          {
            y-=2;
            cx=(r.left+r.right)/4;
            LICE_Line(drawbm,cx-bs+1,y+bsh,cx,y+3+1,pencol2,1.0f,0,butaa);
            LICE_Line(drawbm,cx,y+3+1,cx+bs-1,y+bsh,pencol2,1.0f,0,butaa);
            LICE_Line(drawbm,cx+bs-1,y+bsh,cx-bs+1,y+bsh,pencol2,1.0f,0,butaa);

            LICE_Line(drawbm,cx-bs-1,y+bsh+1,cx,y+3,pencol,1.0f,0,butaa);
            LICE_Line(drawbm,cx,y+3,cx+bs+1,y+bsh+1,pencol,1.0f,0,butaa);
            LICE_Line(drawbm,cx+bs+1,y+bsh+1, cx-bs-1,y+bsh+1, pencol,1.0f,0,butaa);
          }
        }
      }
      else  // sideways buttons
      {
        if (!bkbm[1] || !bkbm[1]->bgimage)
        {
          #define LICE_LINEROT(bm,x1,y1,x2,y2,pc,al,mode,aa) LICE_Line(bm,y1,x1,y2,x2,pc,al,mode,aa)
          int bs=5 * rscale / WDL_VWND_SCALEBASE;
          int bsh=8 * rscale / WDL_VWND_SCALEBASE;
          int cx = (r.bottom + r.top)/2;
          if (itempos < num_items)
          {
            int z = r.right - leftrightbuttonsize/2 - bsh/2;
            LICE_LINEROT(drawbm,cx-bs+1,z+2,cx,z+bsh-2,pencol2,1.0f,0,butaa);
            LICE_LINEROT(drawbm,cx,z+bsh-2,cx+bs-1,z+2,pencol2,1.0f,0,butaa);
            LICE_LINEROT(drawbm,cx+bs-1,z+2,cx-bs+1,z+2,pencol2,1.0f,0,butaa);

            LICE_LINEROT(drawbm,cx-bs-1,z+1,cx,z+bsh-1,pencol,1.0f,0,butaa);
            LICE_LINEROT(drawbm,cx,z+bsh-1,cx+bs+1,z+1,pencol,1.0f,0,butaa);
            LICE_LINEROT(drawbm,cx+bs+1,z+1,cx-bs-1,z+1,pencol,1.0f,0,butaa);
          }
          if (startpos>0)
          {
            int z = r.left + leftrightbuttonsize/2-bsh/2 - 2;
            LICE_LINEROT(drawbm,cx-bs+1,z+bsh,cx,z+3+1,pencol2,1.0f,0,butaa);
            LICE_LINEROT(drawbm,cx,z+3+1,cx+bs-1,z+bsh,pencol2,1.0f,0,butaa);
            LICE_LINEROT(drawbm,cx+bs-1,z+bsh,cx-bs+1,z+bsh,pencol2,1.0f,0,butaa);

            LICE_LINEROT(drawbm,cx-bs-1,z+bsh+1,cx,z+3,pencol,1.0f,0,butaa);
            LICE_LINEROT(drawbm,cx,z+3,cx+bs+1,z+bsh+1,pencol,1.0f,0,butaa);
            LICE_LINEROT(drawbm,cx+bs+1,z+bsh+1, cx-bs-1,z+bsh+1, pencol,1.0f,0,butaa);
          }
          #undef LICE_LINEROT
        }
      }
    }
  }



  if (!mainbk)
  {
    LICE_Line(drawbm,r.left,r.bottom-1,r.left,r.top,pencol,1.0f,0,false);
    LICE_Line(drawbm,r.left,r.top,r.right-1,r.top,pencol,1.0f,0,false);
    LICE_Line(drawbm,r.right-1,r.top,r.right-1,r.bottom-1,pencol2,1.0f,0,false);
    LICE_Line(drawbm,r.right-1,r.bottom-1,r.left,r.bottom-1,pencol2,1.0f,0,false);
  }


}
void WDL_VirtualListBox::DoScroll(int dir, const layout_info *layout)
{
  if (dir < 0 && layout->columns>1)
  {
    int y=0;
    while (m_viewoffs>0)
    {
      y += GetItemHeight(--m_viewoffs);
      if (y >= layout->item_area_h) break;
    }
    if (y) RequestRedraw(NULL);
  }
  else if (dir > 0 && layout->columns>1)
  {
    int y=0,i;
    for (i = 0;  i < layout->heights->GetSize(); i ++)
    {
      y += layout->heights->Get()[i];
      if (y >= layout->item_area_h) break;
      m_viewoffs++;
    }
    if (i) RequestRedraw(NULL);
  }
  else if (dir > 0)
  {
    m_viewoffs++; // let painting sort out total visibility (we could easily calculate but meh)
    RequestRedraw(NULL);
  }
  else if (dir < 0)
  {
    if (m_viewoffs>0)
    {
      m_viewoffs--;
      RequestRedraw(NULL);
    }
  }
}

bool WDL_VirtualListBox::HandleScrollClicks(int xpos, int ypos, const layout_info *layout)
{
  if (layout->leftrightbutton_w && (xpos<m_scrollbuttonsize || xpos >= layout->item_area_w+m_scrollbuttonsize))
  {
    if (xpos<m_scrollbuttonsize)
    {
      DoScroll(-1,layout);
    }
    else
    {
      DoScroll(1,layout);
    }
    m_cap_state=0;
    m_cap_startitem=-1;
    return true;
  }
  if (layout->updownbutton_h && ypos >= layout->item_area_h)
  {
    if (ypos < layout->item_area_h + m_scrollbuttonsize)
    {
      if (xpos < layout->item_area_w/2)
      {
        DoScroll(-1,layout);
      }
      else
      {
        DoScroll(1,layout);
      }
    }
    m_cap_state=0;
    m_cap_startitem=-1;
    return true;
  }
  return false;
}

int WDL_VirtualListBox::OnMouseDown(int xpos, int ypos)
{
  if (m_grayed) return 0;
  m_cap_startpos.x = xpos;
  m_cap_startpos.y = ypos;

  if (m__iaccess) m__iaccess->OnFocused();
  const int num_items = m_GetItemInfo ? m_GetItemInfo(this,-1,NULL,0,NULL,NULL) : 0;
  layout_info layout;
  CalcLayout(num_items,&layout);

  int idx = IndexFromPtInt(xpos,ypos,layout);
  if (idx < 0)
  {
    if (idx == -2) return 0;
    if (HandleScrollClicks(xpos,ypos,&layout)) return 1;
    return 0;
  }

  m_cap_state=0x1000;
  m_cap_startitem=idx;
  RequestRedraw(NULL);
  return 1;
}


bool WDL_VirtualListBox::OnMouseDblClick(int xpos, int ypos)
{
  if (m_grayed) return false;

  const int num_items = m_GetItemInfo ? m_GetItemInfo(this,-1,NULL,0,NULL,NULL) : 0;
  layout_info layout;
  CalcLayout(num_items,&layout);

  int idx = IndexFromPtInt(xpos,ypos,layout);
  if (idx<0)
  {
    if (idx == -2) return false;
    if (HandleScrollClicks(xpos,ypos,&layout)) return true;
    idx = num_items;
  }
  
  RequestRedraw(NULL);
  if (m_clickmsg) SendCommand(m_clickmsg,(INT_PTR)this,idx,this);
  return false;
}

bool WDL_VirtualListBox::OnMouseWheel(int xpos, int ypos, int amt)
{
  if (m_grayed) return false;

  const int num_items = m_GetItemInfo ? m_GetItemInfo(this,-1,NULL,0,NULL,NULL) : 0;
  layout_info layout;
  CalcLayout(num_items,&layout);

  if (xpos >= layout.item_area_w + layout.leftrightbutton_w*2) return false;

  DoScroll(-amt,&layout);
  return true;
}

void WDL_VirtualListBox::OnMouseMove(int xpos, int ypos)
{
  if (m_grayed) return;

  if (m_cap_state>=0x1000)
  {
    m_cap_state++;
    if (m_cap_state < 0x1008)
    {
      int dx = (xpos - m_cap_startpos.x), dy=(ypos-m_cap_startpos.y);
      if (dx*dx + dy*dy > 36) 
        m_cap_state=0x1008;
    }
    if (m_cap_state>=0x1008)
    {
      if (m_dragmsg)
      {
        SendCommand(m_dragmsg,(INT_PTR)this,m_cap_startitem,this);
      }
    }
  }
  else if (m_cap_state==0)
  {
    int a=IndexFromPt(xpos,ypos);
    if (a>=0)
    {
      m_cap_startitem=a;
      m_cap_state=1;
      RequestRedraw(NULL);
    }
  }
  else if (m_cap_state==1)
  {
    int a=IndexFromPt(xpos,ypos);
    if (a>=0 && a != m_cap_startitem)
    {
      m_cap_startitem=a;
      m_cap_state=1;
      RequestRedraw(NULL);
    }
    else if (a<0)
    {
      m_cap_state=0;
      RequestRedraw(NULL);
    }
  }
}

void WDL_VirtualListBox::OnMouseUp(int xpos, int ypos)
{
  if (m_grayed) return;

  int cmd=0;
  INT_PTR p1, p2;
  int hit=IndexFromPt(xpos,ypos);
  if (m_cap_state>=0x1000 && m_cap_state<0x1008 && hit==m_cap_startitem) 
  {
    if (m_clickmsg)
    {
      cmd=m_clickmsg;
      p1=(INT_PTR)this;
      p2=hit;
    }
  }
  else if (m_cap_state>=0x1008)
  {
    // send a message saying drag & drop occurred
    if (m_dropmsg)
    {
      cmd=m_dropmsg;
      p1=(INT_PTR)this;
      p2=m_cap_startitem;
    }
  }

  m_cap_state=0;
  RequestRedraw(NULL);
  if (cmd) SendCommand(cmd,p1,p2,this);
}

int WDL_VirtualListBox::GetVisibleItemRects(WDL_TypedBuf<RECT> *list)
{
  const int num_items = m_GetItemInfo ? m_GetItemInfo(this,-1,NULL,0,NULL,NULL) : 0;
  layout_info layout;
  CalcLayout(num_items,&layout);

  const int rh_base = GetRowHeight();
  const int n = layout.heights->GetSize();
  RECT *r = list->ResizeOK(n,false);
  if (!r) { list->Resize(0,false); return 0; }
  
  int col = 0, y=0;
  int xpos = layout.leftrightbutton_w;
  int nx = layout.leftrightbutton_w + (col+1)*layout.item_area_w / layout.columns;
  for (int x=0;x<n;x++)
  {
    const int rh = layout.heights->Get()[x];
    if (y > 0 && y + rh > layout.item_area_h && (col < layout.columns-1 || y+rh_base > layout.item_area_h)) 
    { 
      if (++col >= layout.columns) break;
      y = 0;
      xpos=nx;
      nx = layout.leftrightbutton_w + (col+1)*layout.item_area_w / layout.columns;
    }
    r->left = xpos;
    r->right = nx;
    r->top = y;
    r->bottom = wdl_min(y + rh,layout.item_area_h);
    r++;
    y += rh;
  }
  return layout.startpos;
}

bool WDL_VirtualListBox::GetItemRect(int item, RECT *r)
{
  const int num_items = m_GetItemInfo ? m_GetItemInfo(this,-1,NULL,0,NULL,NULL) : 0;
  layout_info layout;
  CalcLayout(num_items,&layout);

  item -= layout.startpos;
  if (item < 0) { if (r) memset(r,0,sizeof(RECT)); return false; }
  
  if (r)
  {
    const int rh_base = GetRowHeight();
    int col = 0,y=0;
    for (int x=0;x<item;x++)
    {
      const int rh = x < layout.heights->GetSize() ? layout.heights->Get()[x] : rh_base;
      if (y > 0 && y + rh > layout.item_area_h && (col < layout.columns-1 ||  y+rh_base > layout.item_area_h)) { col++; y = 0; }
      y += rh;
    }
    const int rh = item < layout.heights->GetSize() ? layout.heights->Get()[item] : rh_base;
    if (y > 0 && y + rh > layout.item_area_h && (col < layout.columns-1 || y+rh_base > layout.item_area_h)) { col++; y = 0; }
    if (col >= layout.columns)  { if (r) memset(r,0,sizeof(RECT)); return false; }

    r->top = y;
    r->bottom = y+rh;
    if (r->bottom > layout.item_area_h) r->bottom = layout.item_area_h;
    r->left = layout.leftrightbutton_w + (col * layout.item_area_w) / layout.columns;
    r->right = layout.leftrightbutton_w + ((col+1) * layout.item_area_w) / layout.columns;
  }
  return true;
}

int WDL_VirtualListBox::IndexFromPt(int x, int y)
{
  const int num_items = m_GetItemInfo ? m_GetItemInfo(this,-1,NULL,0,NULL,NULL) : 0;
  layout_info layout;
  CalcLayout(num_items,&layout);
  return IndexFromPtInt(x,y,layout);
}

int WDL_VirtualListBox::IndexFromPtInt(int x, int y, const layout_info &layout)
{
  if (x >= layout.item_area_w + layout.leftrightbutton_w*2) return -2;

  if (y < 0 || y >= layout.item_area_h || x< layout.leftrightbutton_w || x >= layout.leftrightbutton_w + layout.item_area_w) 
  {
    return -1;
  }

  // step through visible items
  const int usewid=layout.item_area_w;
  int xpos = layout.leftrightbutton_w;
  int col = 1;

  const int rh_base = GetRowHeight();
  int idx = 0;
  for (;;)
  {
    if (x < xpos) return -1;
    int ypos = 0;
    const int nx = layout.leftrightbutton_w + (col++ * usewid) / layout.columns;
    for (;;)
    {
      const int rh = idx < layout.heights->GetSize() ? layout.heights->Get()[idx] : rh_base;
      if (ypos > 0 && ypos + rh > layout.item_area_h && (col < layout.columns-1 || ypos+rh_base > layout.item_area_h)) break;
      if (x < nx && y >= ypos && y < ypos+rh) return layout.startpos + idx;
      ypos += rh;
      idx++;
    }
    if (x < nx && y >= ypos) return -1; // empty space at bottom of column
    xpos = nx;
  }
}

void WDL_VirtualListBox::SetViewOffset(int offs)
{
  int num_items = m_GetItemInfo ? m_GetItemInfo(this,-1,NULL,0,NULL,NULL) : 0;
  if (num_items) 
  {
    if (offs < 0) offs=0;
    else if (offs >= num_items) offs = num_items-1;
    if (offs != m_viewoffs)
    {
      m_viewoffs = offs;
      RequestRedraw(0);
    }
  }
}

int WDL_VirtualListBox::GetViewOffset()
{
  return m_viewoffs;
}

int WDL_VirtualListBox::GetItemHeight(int idx)
{
  const int a = m_GetItemHeight ? m_GetItemHeight(this,idx) : -1;
  return a>=0 ? a : GetRowHeight();
}
