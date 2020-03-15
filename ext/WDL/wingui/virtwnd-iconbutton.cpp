/*
    WDL - virtwnd-iconbutton.cpp
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
      

    Implementation for virtual window icon buttons, icons, combo boxes, and static text controls.

*/

#include <ctype.h>
#include "virtwnd-controls.h"
#include "../lice/lice.h"

#ifdef _WIN32
#define WDL_WIN32_UTF8_IMPL static
#define WDL_WIN32_UTF8_NO_UI_IMPL
#include "../win32_utf8.c"
#endif

WDL_VirtualIconButton::WDL_VirtualIconButton()
{
  m_alpha=1.0;
  m_checkstate=-1;
  m_textfont=0;
  m_textfontv=0;
  m_textalign=0;
  m_bgcol1_msg=0;
  m_is_button=true;
  m_pressed=0;
  m_iconCfg=0;
  m_en=true;
  m_grayed = false;
  m_forceborder=false;
  m_forcetext=false;
  m_forcetext_color=0;
  m_ownsicon=false;
  m_immediate=false;
  m_margin_r = m_margin_l = 0;
  m_margin_t = m_margin_b = 0;
}

WDL_VirtualIconButton::~WDL_VirtualIconButton()
{
  if (m_ownsicon && m_iconCfg)
  {
    delete m_iconCfg->image;
    delete m_iconCfg->olimage;
    delete m_iconCfg;
  }
}

void WDL_VirtualIconButton::SetTextLabel(const char *text)
{ 
  m_textlbl.Set(text); 
  if (!m_iconCfg || m_forcetext) RequestRedraw(NULL); 
} 

void WDL_VirtualIconButton::SetTextLabel(const char *text, int align, LICE_IFont *font) 
{ 
  if (font) m_textfont=font;
  m_textalign=align;
  m_textlbl.Set(text); 
  if (!m_iconCfg || m_forcetext) RequestRedraw(NULL); 
} 

void WDL_VirtualIconButton::SetCheckState(char state)
{
  if (state != m_checkstate)
  {
    m_checkstate=state;
    RequestRedraw(NULL);
  }
}

void WDL_VirtualIconButton::SetIcon(WDL_VirtualIconButton_SkinConfig *cfg, float alpha, bool buttonownsicon)
{ 
  if (m_iconCfg != cfg || m_alpha != alpha) 
  {
    bool combineRects=false;
    RECT r;
    if (m_iconCfg && m_iconCfg != cfg && m_iconCfg->olimage)
    {
      combineRects=true;
      GetPositionPaintExtent(&r,WDL_VWND_SCALEBASE);
      if (WantsPaintOver())
      {
        RECT r3;
        GetPositionPaintOverExtent(&r3,WDL_VWND_SCALEBASE);
        if (r3.left<r.left) r.left=r3.left;
        if (r3.top<r.top) r.top=r3.top;
        if (r3.right>r.right) r.right=r3.right;
        if (r3.bottom>r.bottom) r.bottom=r3.bottom;
      }
    }
    if (m_ownsicon && m_iconCfg && m_iconCfg != cfg)
    {
      delete m_iconCfg->image;
      delete m_iconCfg->olimage;
      delete m_iconCfg;
    }
    m_alpha=alpha; 
    m_iconCfg=cfg;

    if (combineRects)
    {
      RECT r3;
      GetPositionPaintExtent(&r3,WDL_VWND_SCALEBASE);
      if (r3.left<r.left) r.left=r3.left;
      if (r3.top<r.top) r.top=r3.top;
      if (r3.right>r.right) r.right=r3.right;
      if (r3.bottom>r.bottom) r.bottom=r3.bottom;

      if (WantsPaintOver())
      {
        GetPositionPaintOverExtent(&r3,WDL_VWND_SCALEBASE);
        if (r3.left<r.left) r.left=r3.left;
        if (r3.top<r.top) r.top=r3.top;
        if (r3.right>r.right) r.right=r3.right;
        if (r3.bottom>r.bottom) r.bottom=r3.bottom;
      }

      r.left -= m_position.left;
      r.right -= m_position.left;
      r.top -= m_position.top;
      r.bottom -= m_position.top;
      RequestRedraw(&r);
    }
    else
    {
      RequestRedraw(NULL); 
    }
  }
  m_ownsicon = buttonownsicon;
}

void WDL_VirtualIconButton::OnPaintOver(LICE_IBitmap *drawbm, int origin_x, int origin_y, RECT *cliprect, int rscale)
{
  if (m_iconCfg && m_iconCfg->olimage)
  {
    int sx=0;
    int sy=0;
    int w=m_iconCfg->olimage->getWidth();
    int h=m_iconCfg->olimage->getHeight();
    if (m_iconCfg->image_ltrb_used.flags&1)  { w-=2; h-= 2; sx++,sy++; }

    w/=3;

    if (w>0 && h>0)
    {
      if (m_is_button)
      {
        if ((m_pressed&2))  sx+=(m_pressed&1) ? w*2 : w;
      }

      if (m_iconCfg->image_ltrb_used.flags&2) // use main image's stretch areas (outer areas become unstretched)
      {
        WDL_VirtualWnd_BGCfg cfg={0,};
        LICE_SubBitmap sb(m_iconCfg->olimage,sx,sy,w,h);
        cfg.bgimage = &sb;
        cfg.bgimage_lt[0] = m_iconCfg->image_ltrb_main[0]+1; // image_ltrb_main expects 1-based number
        cfg.bgimage_lt[1] = m_iconCfg->image_ltrb_main[1]+1;
        cfg.bgimage_rb[0] = m_iconCfg->image_ltrb_main[2]+1;
        cfg.bgimage_rb[1] = m_iconCfg->image_ltrb_main[3]+1;
        cfg.bgimage_lt_out[0] = m_iconCfg->image_ltrb_ol[0]+1;
        cfg.bgimage_lt_out[1] = m_iconCfg->image_ltrb_ol[1]+1;
        cfg.bgimage_rb_out[0] = m_iconCfg->image_ltrb_ol[2]+1;
        cfg.bgimage_rb_out[1] = m_iconCfg->image_ltrb_ol[3]+1;
        cfg.bgimage_noalphaflags=0;

        RECT r=m_position,r2;
        ScaleRect(&r,rscale);
        GetPositionPaintOverExtent(&r2,rscale);
        WDL_VirtualWnd_ScaledBlitBG(drawbm,&cfg,
          r.left+origin_x,r.top+origin_y,r.right-r.left,r.bottom-r.top,
          r2.left+origin_x,r2.top+origin_y,r2.right-r2.left,r2.bottom-r2.top,
          m_alpha,LICE_BLIT_MODE_COPY|LICE_BLIT_FILTER_BILINEAR|LICE_BLIT_USE_ALPHA);
      }
      else
      {
        RECT r;
        GetPositionPaintOverExtent(&r,rscale);
        LICE_ScaledBlit(drawbm,m_iconCfg->olimage,r.left+origin_x,r.top+origin_y,
          r.right-r.left,
          r.bottom-r.top,
          (float)sx,(float)sy,(float)w,(float)h, m_alpha,  // m_grayed?
          LICE_BLIT_MODE_COPY|LICE_BLIT_FILTER_BILINEAR|LICE_BLIT_USE_ALPHA);      
      }
    }
  }
}


void WDL_VirtualIconButton::OnPaint(LICE_IBitmap *drawbm, int origin_x, int origin_y, RECT *cliprect, int rscale) 
{ 
  int col;

  float alpha = (m_grayed ? 0.25f : 1.0f) * m_alpha;

  bool isdown = !!(m_pressed&1);
  bool ishover = !!(m_pressed&2);

  if (m_iconCfg && m_iconCfg->image && !m_iconCfg->image_issingle)
  {
    bool swapupdown = (m_checkstate > 0);
    bool isdownimg = (swapupdown != isdown);
    
    RECT r=m_position;
    ScaleRect(&r,rscale);

    int sx=0;
    int sy=0;
    int w=m_iconCfg->image->getWidth();
    int h=m_iconCfg->image->getHeight();

    if (w>0 && (m_iconCfg->image_ltrb_used.flags&2))
      w-=2;

    w/=3;
    if (w>0 && h > 0)
    {
      if (m_is_button)
      {
        if (isdownimg) sx += w*2;
        else if (ishover) sx += w;
      }


      if (m_iconCfg->image_ltrb_used.flags&2)
      {
        WDL_VirtualWnd_BGCfg cfg={0,};
        LICE_SubBitmap sb(m_iconCfg->image,sx+1,sy+1,w,h-2);
        cfg.bgimage = &sb;
        cfg.bgimage_lt[0] = m_iconCfg->image_ltrb_main[0]+1; // image_ltrb_main expects 1-based number
        cfg.bgimage_lt[1] = m_iconCfg->image_ltrb_main[1]+1;
        cfg.bgimage_rb[0] = m_iconCfg->image_ltrb_main[2]+1;
        cfg.bgimage_rb[1] = m_iconCfg->image_ltrb_main[3]+1;
        cfg.bgimage_noalphaflags=0;

        WDL_VirtualWnd_ScaledBlitBG(drawbm,&cfg,
          r.left+origin_x,r.top+origin_y,r.right-r.left,r.bottom-r.top,
          r.left+origin_x,r.top+origin_y,r.right-r.left,r.bottom-r.top,
          alpha,LICE_BLIT_MODE_COPY|LICE_BLIT_FILTER_BILINEAR|LICE_BLIT_USE_ALPHA);

      }
      else
        LICE_ScaledBlit(drawbm,m_iconCfg->image,r.left+origin_x,r.top+origin_y,
          r.right-r.left,
          r.bottom-r.top,
          (float)sx,(float)sy,(float)w,(float)h, alpha,
          LICE_BLIT_MODE_COPY|LICE_BLIT_FILTER_BILINEAR|LICE_BLIT_USE_ALPHA);      
    }
  }
  else
  {
    RECT r=m_position;
    ScaleRect(&r,rscale);
    r.left+=origin_x;
    r.right+=origin_x;
    r.top+=origin_y;
    r.bottom+=origin_y;
    if (m_is_button)
    {
      if (WDL_STYLE_WantGlobalButtonBackground(&col))
      {
        LICE_FillRect(drawbm,r.left,r.top,r.right-r.left,r.bottom-r.top,LICE_RGBA_FROMNATIVE(col,255),alpha,LICE_BLIT_MODE_COPY);
      }

      if (ishover || m_forceborder || WDL_STYLE_WantGlobalButtonBorders())
      {
        int cidx=isdown?COLOR_3DSHADOW:COLOR_3DHILIGHT;

        int pencol = GSC(cidx);
        pencol = LICE_RGBA_FROMNATIVE(pencol,255);

        LICE_Line(drawbm,r.left,r.bottom-1,r.left,r.top,pencol,alpha,LICE_BLIT_MODE_COPY,false);
        LICE_Line(drawbm,r.left,r.top,r.right-1,r.top,pencol,alpha,LICE_BLIT_MODE_COPY,false);
        cidx = isdown?COLOR_3DHILIGHT:COLOR_3DSHADOW;
        pencol = GSC(cidx);
        pencol = LICE_RGBA_FROMNATIVE(pencol,255);
        LICE_Line(drawbm,r.right-1,r.top,r.right-1,r.bottom-1,pencol,alpha,LICE_BLIT_MODE_COPY,false);
        LICE_Line(drawbm,r.right-1,r.bottom-1,r.left,r.bottom-1,pencol,alpha,LICE_BLIT_MODE_COPY,false);
      }
    }
    if (m_iconCfg && m_iconCfg->image)
    {
      const int rscale2 = drawbm ? drawbm->Extended(LICE_EXT_GET_ADVISORY_SCALING,NULL) : 0;
      const int sz=rscale2 ? 16*rscale2/256 : 16;
    
      int x=r.left+((r.right-r.left)-sz)/2;
      int y=r.top+((r.bottom-r.top)-sz)/2;
      if (m_is_button)
      {
        if (isdown && ishover) { x++; y++; }
      }

      LICE_ScaledBlit(drawbm,m_iconCfg->image,x,y,sz,sz,0.0f,0.0f,
        (float)m_iconCfg->image->getWidth(),
        (float)m_iconCfg->image->getHeight(),alpha,LICE_BLIT_MODE_COPY|LICE_BLIT_FILTER_BILINEAR|LICE_BLIT_USE_ALPHA);

    }
  }

  if (!m_iconCfg || m_forcetext)
  {
    RECT r2=m_position;
    ScaleRect(&r2,rscale);
    r2.left+=origin_x;
    r2.right+=origin_x;
    r2.top+=origin_y;
    r2.bottom+=origin_y;

    if (m_checkstate>=0 && !m_iconCfg)
    {
      RECT tr=r2;
      int sz=tr.bottom-tr.top;
      int adj = 2*rscale/WDL_VWND_SCALEBASE;
      r2.left+=sz+adj;

      tr.top+=adj;
      tr.bottom-=adj;
      sz-=adj*2;
      sz&=~1;
      LICE_FillRect(drawbm ,tr.left,tr.top,sz,sz,LICE_RGBA(255,255,255,255),alpha,LICE_BLIT_MODE_COPY);
      LICE_Line(drawbm,tr.left,tr.top,tr.left+sz,tr.top,LICE_RGBA(128,128,128,255),alpha,LICE_BLIT_MODE_COPY,false);
      LICE_Line(drawbm,tr.left+sz,tr.top,tr.left+sz,tr.bottom,LICE_RGBA(128,128,128,255),alpha,LICE_BLIT_MODE_COPY,false);
      LICE_Line(drawbm,tr.left+sz,tr.bottom,tr.left,tr.bottom,LICE_RGBA(128,128,128,255),alpha,LICE_BLIT_MODE_COPY,false);
      LICE_Line(drawbm,tr.left,tr.bottom,tr.left,tr.top,LICE_RGBA(128,128,128,255),alpha,LICE_BLIT_MODE_COPY,false);
      int nl = (m_checkstate>0) ? 3:0;        
      if (isdown) nl ^= 2;

      if (nl&1)
        LICE_Line(drawbm,tr.left+2,tr.bottom-2,tr.left+sz-2,tr.top+2,LICE_RGBA(0,0,0,255),alpha,LICE_BLIT_MODE_COPY,false);
      if (nl&2)
        LICE_Line(drawbm,tr.left+2,tr.top+2,tr.left+sz-2,tr.bottom-2,LICE_RGBA(0,0,0,255),alpha,LICE_BLIT_MODE_COPY,false);


    }

    LICE_IFont *font = m_textfont;
    bool isVert=false;
    if (font && m_textfontv && m_position.right-m_position.left < m_position.bottom - m_position.top)
    {
      isVert=true;
      font = m_textfontv;
    }
    // draw text
    if (font&&m_textlbl.Get()[0])
    {
      int fgc=m_forcetext_color ? m_forcetext_color : LICE_RGBA_FROMNATIVE(GSC(COLOR_BTNTEXT),255);
      //font->SetCombineMode(LICE_BLIT_MODE_COPY, alpha); // this affects the glyphs that get cached
      font->SetBkMode(TRANSPARENT);
      font->SetTextColor(fgc);

      r2.left += m_margin_l * rscale / WDL_VWND_SCALEBASE;
      r2.right -= m_margin_r * rscale / WDL_VWND_SCALEBASE;
      r2.top += m_margin_t * rscale / WDL_VWND_SCALEBASE;
      r2.bottom -= m_margin_b * rscale / WDL_VWND_SCALEBASE;

      if (isdown)
      {
        if (m_textalign<0) r2.left+= rscale / WDL_VWND_SCALEBASE;
        else if (m_textalign>0) r2.right+= rscale / WDL_VWND_SCALEBASE;
        else r2.left+=2 * rscale / WDL_VWND_SCALEBASE;
        r2.top+=2 * rscale/WDL_VWND_SCALEBASE;
      }
      int f = DT_SINGLELINE|DT_NOPREFIX;
      if (isVert)
      {
        if (m_textalign == 0)
        {
          RECT mr={0,};
          font->DrawText(drawbm,m_textlbl.Get(),-1,&mr,f|DT_CALCRECT);
          f |= (mr.bottom < r2.bottom-r2.top) ? DT_VCENTER : DT_TOP;
        }
        else
          f |= m_textalign<0?DT_TOP:DT_BOTTOM;

        f |= DT_CENTER;
      }
      else
      {
        if (m_textalign == 0)
        {
          RECT mr={0,};
          font->DrawText(drawbm,m_textlbl.Get(),-1,&mr,f|DT_CALCRECT);
          f |= (mr.right < r2.right-r2.left) ? DT_CENTER : DT_LEFT;
        }
        else
          f |= m_textalign<0?DT_LEFT:DT_RIGHT;

        f |= DT_VCENTER;
      }
      font->DrawText(drawbm,m_textlbl.Get(),-1,&r2,f);
    }
    
  }

  if (m_bgcol1_msg)
  {
    int brcol=-100;
    SendCommand(m_bgcol1_msg,(INT_PTR)&brcol,GetID(),this);
    if (brcol != -100)
    {
      RECT r=m_position;

      int bh=(r.bottom-r.top)/5;
      if (bh<1) bh=1;
      int bw=(r.right-r.left)/5;
      if (bw<1) bw=1;

      LICE_FillRect(drawbm,
        r.left+origin_x,r.top+origin_y,
        r.right-r.left,
        bh,LICE_RGBA_FROMNATIVE(brcol,255),0.75,LICE_BLIT_MODE_COPY);

      LICE_FillRect(drawbm,
        r.left+origin_x,r.top+origin_y+bh,
        bw,
        r.bottom-r.top-bh*2,LICE_RGBA_FROMNATIVE(brcol,255),0.75,LICE_BLIT_MODE_COPY);

      LICE_FillRect(drawbm,
        r.right+origin_x-bw,r.top+origin_y+bh,
        bw,
        r.bottom-r.top-bh*2,LICE_RGBA_FROMNATIVE(brcol,255),0.75,LICE_BLIT_MODE_COPY);

      LICE_FillRect(drawbm,
        r.left+origin_x,r.bottom+origin_y-bh,
        r.right-r.left,
        bh,LICE_RGBA_FROMNATIVE(brcol,255),0.75,LICE_BLIT_MODE_COPY);
    }
  }

} 


void WDL_VirtualIconButton::OnMouseMove(int xpos, int ypos)
{
  if (m_en&&m_is_button)
  {
    int wp=m_pressed;

    WDL_VWnd *parhit = GetParent();
    if (parhit)
    {
      parhit = parhit->VirtWndFromPoint(m_position.left+xpos,m_position.top+ypos,0);
    }
    else if (!parhit)
    {
      // special case if no parent
      if (xpos >= 0 && xpos < m_position.right-m_position.left && ypos >= 0 && ypos < m_position.bottom-m_position.top) parhit=this;      
    }
    
    if (parhit == this)
    {
      m_pressed|=2;
    }
    else
    {
      m_pressed&=~2;
    }

    if ((m_pressed&3)!=(wp&3))
    {
      RequestRedraw(NULL);
    }
  }
}

int WDL_VirtualIconButton::OnMouseDown(int xpos, int ypos)
{
  if (m_en&&m_is_button)
  {
    m_pressed=3;
    RequestRedraw(NULL);
    if (m__iaccess) m__iaccess->OnFocused();

    if (m_immediate)
    {
      DoSendCommand(xpos, ypos);
    }

    return 1;
  }
  return 0;
}

bool WDL_VirtualIconButton::OnMouseDblClick(int xpos, int ypos)
{
  if (m_is_button) 
  { 
    DoSendCommand(xpos, ypos);
    return true;
  }
  return false;
}

void WDL_VirtualIconButton::OnMouseUp(int xpos, int ypos)
{
  if (!m_is_button) return;

  int waspress=!!m_pressed;
  m_pressed&=~1;
  RequestRedraw(NULL);

  if (waspress && !m_immediate)
  {
    DoSendCommand(xpos, ypos);
  }
}

void WDL_VirtualIconButton::DoSendCommand(int xpos, int ypos)
{
  if (m_en &&
    xpos >= 0 &&
    xpos < m_position.right-m_position.left && 
    ypos >= 0 && 
    ypos < m_position.bottom-m_position.top)
  {
    int code=GetID();
    if (!m_iconCfg && m_textlbl.Get()[0] && m_checkstate >= 0)
    {
      if (xpos < m_position.bottom-m_position.top)
      {
        code|=600<<16;
      }
    }
    WDL_VWND_DCHK(a);
    SendCommand(WM_COMMAND,code,0,this);
    if (a.isOK() && m__iaccess && m_checkstate>=0) m__iaccess->OnStateChange();
  }
}


WDL_VirtualComboBox::WDL_VirtualComboBox()
{
  m_font=0;
  m_align=-1;
  m_curitem=-1;
}

WDL_VirtualComboBox::~WDL_VirtualComboBox()
{
  m_items.Empty(true,free);
}


static void GenSubMenu(HMENU menu, int *x, WDL_PtrList<char> *items, int curitem)
{
  int pos=0;
  while (*x < items->GetSize())
  {
    MENUITEMINFO mi={sizeof(mi),MIIM_ID|MIIM_STATE|MIIM_TYPE,MFT_STRING, 0,1000u + *x,NULL,NULL,NULL,0};
    mi.dwTypeData = (char *)items->Get(*x);
    mi.fState = curitem == *x ?MFS_CHECKED:0;

    (*x) ++; // advance to next item

    if (!strcmp(mi.dwTypeData,"<SEP>")) mi.fType=MFT_SEPARATOR;
    else if (!strcmp(mi.dwTypeData,"</SUB>")) break; // done!
    else if (!strncmp(mi.dwTypeData,"<SUB>",5))
    {
      mi.hSubMenu= CreatePopupMenu();
      GenSubMenu(mi.hSubMenu,x,items,curitem);
      mi.fMask |= MIIM_SUBMENU;
      mi.dwTypeData += 5; // skip <SUB>
    }
    InsertMenuItem(menu,pos++,TRUE,&mi);
  }
}

int WDL_VirtualComboBox::OnMouseDown(int xpos, int ypos)
{
  if (m__iaccess) m__iaccess->OnFocused();
  if (m_items.GetSize())
  {    
    //SendCommand(WM_COMMAND, GetID()|(CBN_DROPDOWN<<16), 0, this);

    HMENU menu=CreatePopupMenu();
    int x=0;
    GenSubMenu(menu,&x,&m_items,m_curitem);

    HWND h=GetRealParent();
    POINT p={0,};
    WDL_VirtualWnd *w=this;
    while (w)
    {
      RECT r;
      w->GetPosition(&r);
      p.x+=r.left;
      p.y+=w==this?r.bottom:r.top;
      w=w->GetParent();
    }
    if (h) 
    {
      ClientToScreen(h,&p);
      //SetFocus(h);
    }
    
    WDL_VWND_DCHK(a);

    int ret=TrackPopupMenu(menu,TPM_LEFTALIGN|TPM_TOPALIGN|TPM_RETURNCMD|TPM_NONOTIFY,p.x,p.y,0,h,NULL);

    DestroyMenu(menu);

    if (ret>=1000 && a.isOK())
    {
      m_curitem=ret-1000;
      RequestRedraw(NULL);
    // track menu
      SendCommand(WM_COMMAND,GetID() | (CBN_SELCHANGE<<16),0,this);
      if (a.isOK() && m__iaccess) m__iaccess->OnStateChange();
    }
  }
  return -1;
}

void WDL_VirtualComboBox::OnPaint(LICE_IBitmap *drawbm, int origin_x, int origin_y, RECT *cliprect, int rscale)
{
  {
    if (m_font) m_font->SetBkMode(TRANSPARENT);

    RECT r;
    WDL_VWnd::GetPositionPaintExtent(&r,rscale);
    r.left+=origin_x;
    r.right+=origin_x;
    r.top+=origin_y;
    r.bottom+=origin_y;

    int col=GSC(COLOR_WINDOW);
    col = LICE_RGBA_FROMNATIVE(col,255);
    LICE_FillRect(drawbm,r.left,r.top,r.right-r.left,r.bottom-r.top,col,1.0f,LICE_BLIT_MODE_COPY);

    {
      RECT tr=r;
      tr.left=tr.right-(tr.bottom-tr.top);
      //int col2=GSC(COLOR_BTNFACE);
    //  col2 = LICE_RGBA_FROMNATIVE(col2,255);

      LICE_FillRect(drawbm,tr.left,tr.top,tr.right-tr.left,tr.bottom-tr.top,col,1.0f,LICE_BLIT_MODE_COPY);
    }
    

    int tcol=GSC(COLOR_BTNTEXT);
    tcol=LICE_RGBA_FROMNATIVE(tcol,255);
    if (m_font && m_items.Get(m_curitem)&&m_items.Get(m_curitem)[0])
    {
      RECT tr=r;
      tr.left+=2;
      tr.right-=16;
      m_font->SetTextColor(tcol);
      if (m_align == 0)
      {
        RECT r2={0,};
        m_font->DrawText(drawbm,m_items.Get(m_curitem),-1,&tr,DT_SINGLELINE|DT_CALCRECT|DT_NOPREFIX);
        m_font->DrawText(drawbm,m_items.Get(m_curitem),-1,&tr,DT_SINGLELINE|DT_VCENTER|(r2.right < tr.right-tr.left ? DT_CENTER : DT_LEFT)|DT_NOPREFIX);
      }
      else
        m_font->DrawText(drawbm,m_items.Get(m_curitem),-1,&tr,DT_SINGLELINE|DT_VCENTER|(m_align<0?DT_LEFT:DT_RIGHT)|DT_NOPREFIX);
    }


    // pen3=tcol
    int pencol = GSC(COLOR_3DSHADOW);
    pencol = LICE_RGBA_FROMNATIVE(pencol,255);
    int pencol2 = GSC(COLOR_3DHILIGHT);
    pencol2 = LICE_RGBA_FROMNATIVE(pencol2,255);

    // draw the down arrow button
    {
      int bs=(r.bottom-r.top);
      int l=r.right-bs;

      int a=(bs/4)&~1;

      LICE_Line(drawbm,l,r.top,l,r.bottom-1,pencol,1.0f,LICE_BLIT_MODE_COPY,false);
      LICE_Line(drawbm,l-1,r.top,l-1,r.bottom-1,pencol2,1.0f,LICE_BLIT_MODE_COPY,false);

      LICE_Line(drawbm,l+bs/2-a,r.top+bs/2-a/2,
                       l+bs/2,r.top+bs/2+a/2,tcol,1.0f,LICE_BLIT_MODE_COPY,true);
      LICE_Line(drawbm,l+bs/2,r.top+bs/2+a/2,
                       l+bs/2+a,r.top+bs/2-a/2, tcol,1.0f,LICE_BLIT_MODE_COPY,true);
    }

   

    // draw the border
    LICE_Line(drawbm,r.left,r.bottom-1,r.left,r.top,pencol,1.0f,0,false);
    LICE_Line(drawbm,r.left,r.top,r.right-1,r.top,pencol,1.0f,0,false);
    LICE_Line(drawbm,r.right-1,r.top,r.right-1,r.bottom-1,pencol2,1.0f,0,false);
    LICE_Line(drawbm,r.left,r.bottom-1,r.right-1,r.bottom-1,pencol2,1.0f,0,false);

  }
}



WDL_VirtualStaticText::WDL_VirtualStaticText()
{
  m_dotint=false;
  m_bkbm=0;
  m_margin_r=m_margin_l=0;
  m_margin_t=m_margin_b=0;
  m_fg=m_bg=0;
  m_wantborder=false;
  m_vfont=m_font=0;
  m_align=-1;
  m_wantsingle=false;
  m_didvert=0;
  m_didalign=-1;
  m_wantabbr=false;
}

WDL_VirtualStaticText::~WDL_VirtualStaticText()
{
}

void WDL_VirtualStaticText::SetText(const char *text) 
{ 
  if (strcmp(m_text.Get(),text?text:""))
  {
    m_text.Set(text?text:"");
    if (m_font) RequestRedraw(NULL); 
  }
}

void WDL_VirtualStaticText::SetWantPreserveTrailingNumber(bool abbreviate)
{
  m_wantabbr=abbreviate;
  if (m_font) RequestRedraw(NULL); 
}

void WDL_VirtualStaticText::GetPositionPaintExtent(RECT *r, int rscale)
{
  // overridden in case m_bkbm has outer areas
  WDL_VWnd::GetPositionPaintExtent(r,rscale);
  if (m_bkbm && m_bkbm->bgimage)
  {
    if (m_bkbm->bgimage_lt[0]>0 &&
        m_bkbm->bgimage_lt[1]>0 &&
        m_bkbm->bgimage_rb[0]>0 &&
        m_bkbm->bgimage_rb[1]>0 &&
        m_bkbm->bgimage_lt_out[0]>0 &&
        m_bkbm->bgimage_lt_out[1]>0 &&
        m_bkbm->bgimage_rb_out[0]>0 &&
        m_bkbm->bgimage_rb_out[1]>0)
    {
      r->left -= m_bkbm->bgimage_lt_out[0]-1;
      r->top -= m_bkbm->bgimage_lt_out[1]-1;
      r->right += m_bkbm->bgimage_rb_out[0]-1;
      r->bottom += m_bkbm->bgimage_rb_out[1]-1;
    }
  }
}

int WDL_VirtualStaticText::OnMouseDown(int xpos, int ypos)
{
  int a = WDL_VWnd::OnMouseDown(xpos,ypos);
  if (a) return a;

  if (m__iaccess) m__iaccess->OnFocused();

  if (m_wantsingle)
  {
    SendCommand(WM_COMMAND,GetID() | (STN_CLICKED<<16),0,this);
    return -1;
  }
  return 0;
}

void WDL_VirtualStaticText::OnPaint(LICE_IBitmap *drawbm, int origin_x, int origin_y, RECT *cliprect, int rscale)
{
  RECT r=m_position;
  ScaleRect(&r,rscale);
  r.left+=origin_x;
  r.right+=origin_x;
  r.top += origin_y;
  r.bottom += origin_y;

  if (m_bkbm && m_bkbm->bgimage)
  {
    WDL_VirtualWnd_ScaledBlitBG(drawbm,m_bkbm,
      r.left,r.top,r.right-r.left,r.bottom-r.top,
      r.left,r.top,r.right-r.left,r.bottom-r.top,
      1.0,LICE_BLIT_MODE_COPY|LICE_BLIT_FILTER_BILINEAR|LICE_BLIT_USE_ALPHA);

    if (m_dotint && LICE_GETA(m_bg)) 
    {
        float amt = LICE_GETA(m_bg)/255.0f;

        // todo: apply amt

        float rv=LICE_GETR(m_bg)/255.0f;
        float gv=LICE_GETG(m_bg)/255.0f;
        float bv=LICE_GETB(m_bg)/255.0f;

        float avg=(rv+gv+bv)*0.33333f;
        if (avg<0.05f)avg=0.05f;

        float sc=0.5f*amt;
        float sc2 = (amt-sc)/avg;

        float sc3=32.0f * amt;
        float sc4=64.0f*(avg-0.5f) * amt;

        // tint
        LICE_MultiplyAddRect(drawbm,
          r.left,r.top,
            r.right-r.left,
            r.bottom-r.top,
            sc+rv*sc2 + (1.0f-amt),
            sc+gv*sc2 + (1.0f-amt),
            sc+bv*sc2 + (1.0f-amt),
            1.0f,
            (rv-avg)*sc3+sc4,
            (gv-avg)*sc3+sc4,
            (bv-avg)*sc3+sc4,
            0.0f);
    }
  }
  else 
  {
    if (LICE_GETA(m_bg))
    {
      LICE_FillRect(drawbm,r.left,r.top,r.right-r.left,r.bottom-r.top,m_bg,LICE_GETA(m_bg)/255.0f,LICE_BLIT_MODE_COPY);
    }

    if (m_wantborder)
    {    
      int cidx=COLOR_3DSHADOW;

      int pencol = GSC(cidx);
      pencol = LICE_RGBA_FROMNATIVE(pencol,255);

      LICE_Line(drawbm,r.left,r.bottom-1,r.left,r.top,pencol,1.0f,LICE_BLIT_MODE_COPY,false);
      LICE_Line(drawbm,r.left,r.top,r.right-1,r.top,pencol,1.0f,LICE_BLIT_MODE_COPY,false);
      cidx=COLOR_3DHILIGHT;
      pencol = GSC(cidx);
      pencol = LICE_RGBA_FROMNATIVE(pencol,255);
      LICE_Line(drawbm,r.right-1,r.top,r.right-1,r.bottom-1,pencol,1.0f,LICE_BLIT_MODE_COPY,false);
      LICE_Line(drawbm,r.right-1,r.bottom-1,r.left,r.bottom-1,pencol,1.0f,LICE_BLIT_MODE_COPY,false);

      r.left++;
      r.bottom--;
      r.top++;
      r.right--;

    }
  }

  if (m_text.Get()[0])
  {
    r.left += m_margin_l * rscale / WDL_VWND_SCALEBASE;
    r.right -= m_margin_r * rscale / WDL_VWND_SCALEBASE;
    r.top += m_margin_t * rscale / WDL_VWND_SCALEBASE;
    r.bottom -= m_margin_b * rscale / WDL_VWND_SCALEBASE;

    m_didvert=m_vfont && (r.right-r.left)<(r.bottom-r.top)/2;
    LICE_IFont *font = m_didvert ? m_vfont : m_font;

    if (font)
    {
      font->SetBkMode(TRANSPARENT);    

      m_didalign=m_align;
      if (m_didalign==0)
      {
        RECT r2={0,0,0,0};
        font->DrawText(drawbm,m_text.Get(),-1,&r2,DT_SINGLELINE|DT_NOPREFIX|DT_CALCRECT);
        if (m_didvert)
        {
         if (r2.bottom > r.bottom-r.top) m_didalign=-1;
        }
        else
        {
          if (r2.right > r.right-r.left) m_didalign=-1;
        }
      }

      int dtflags=DT_SINGLELINE|DT_NOPREFIX;

      if (m_didvert)
      {
        dtflags |= DT_CENTER;
        if (m_didalign < 0) dtflags |= DT_TOP;
        else if (m_didalign > 0) dtflags |= DT_BOTTOM;
        else dtflags |= DT_VCENTER;
      }
      else
      {
        dtflags|=DT_VCENTER;

        if (m_didalign < 0) dtflags |= DT_LEFT;
        else if (m_didalign > 0) dtflags |= DT_RIGHT;
        else dtflags |= DT_CENTER;
      }
      const char* txt=m_text.Get();
      const int len = m_text.GetLength();

      int abbrx=0;
      char abbrbuf[64];
      abbrbuf[0]=0;

      if (m_wantabbr)
      {
        if (len && txt[len-1] > 0 && isdigit(txt[len-1]))
        {
          RECT tr = { 0, 0, 0, 0 };
          font->DrawText(drawbm, txt, -1, &tr, DT_SINGLELINE|DT_NOPREFIX|DT_CALCRECT);
          if (m_didvert ? (tr.bottom > r.bottom-r.top) : (tr.right > r.right-r.left))
          {
            strcpy(abbrbuf, "..");
            int i;
            for (i=len-1; i >= 0; --i)
            {
              if (txt[i] < 0 || !isdigit(txt[i]) || len-i > 4) break;
            }
            strcat(abbrbuf, txt+i+1);

            int f=dtflags&~(DT_TOP|DT_VCENTER|DT_BOTTOM|DT_LEFT|DT_CENTER|DT_RIGHT);
            RECT tr2 = { 0, 0, 0, 0 };
            if (m_didvert)
            {
              font->DrawText(drawbm, abbrbuf, -1, &tr2, f|DT_CALCRECT);
              abbrx=tr2.bottom;
            }
            else
            {
              font->DrawText(drawbm, abbrbuf, -1, &tr2, f|DT_CALCRECT);
              abbrx=tr2.right;
            }
          }
        }
      }

      int tcol=m_fg ? m_fg : LICE_RGBA_FROMNATIVE(GSC(COLOR_BTNTEXT));
      font->SetTextColor(tcol);
      if (m_fg && LICE_GETA(m_fg) != 0xff) font->SetCombineMode(LICE_BLIT_MODE_COPY,LICE_GETA(m_fg)/255.0f);

      if (abbrx && abbrbuf[0])
      {
        if (m_didvert)
        {
          int f=dtflags&~(DT_TOP|DT_VCENTER|DT_BOTTOM);
          RECT r1 = { r.left, r.top, r.right, r.bottom-abbrx };
          font->DrawText(drawbm, txt, -1, &r1, f|DT_TOP);
          RECT r2 = { r.left, r.bottom-abbrx, r.right, r.bottom };
          font->DrawText(drawbm, abbrbuf, -1, &r2, f|DT_BOTTOM);
        }
        else
        {
          int f=dtflags&~(DT_LEFT|DT_CENTER|DT_RIGHT);
          RECT r1 = { r.left, r.top, r.right-abbrx, r.bottom };
          font->DrawText(drawbm, txt, -1, &r1, f|DT_LEFT);
          RECT r2 = { r.right-abbrx, r.top, r.right, r.bottom };
          font->DrawText(drawbm, abbrbuf, -1, &r2, f|DT_RIGHT);
        }
      }
      else
      {
        font->DrawText(drawbm,txt,-1,&r,dtflags);
      }

      if (m_fg && LICE_GETA(m_fg) != 0xff) font->SetCombineMode(LICE_BLIT_MODE_COPY,1.0f);
    }


  }
  WDL_VWnd::OnPaint(drawbm,origin_x,origin_y,cliprect,rscale);
}


bool WDL_VirtualStaticText::OnMouseDblClick(int xpos, int ypos)
{
  if (!WDL_VWnd::OnMouseDblClick(xpos,ypos))
  {
    SendCommand(WM_COMMAND,GetID() | (STN_DBLCLK<<16),0,this);
  }

  return true;
}


bool WDL_VirtualIconButton::WantsPaintOver()
{
  return /*m_is_button && */m_iconCfg && m_iconCfg->image && m_iconCfg->olimage;
}

void WDL_VirtualIconButton::GetPositionPaintOverExtent(RECT *r, int rscale)
{
  WDL_VWnd::GetPositionPaintOverExtent(r,rscale);
  if (m_iconCfg && m_iconCfg->image && m_iconCfg->olimage && (m_iconCfg->image_ltrb_used.flags&1))
  {
    if (m_iconCfg->image_ltrb_used.flags&2) // main image has pink lines, use 1:1 pixel for outer area size
    {
      r->left -= m_iconCfg->image_ltrb_ol[0];
      r->top -= m_iconCfg->image_ltrb_ol[1];
      r->right += m_iconCfg->image_ltrb_ol[2];
      r->bottom += m_iconCfg->image_ltrb_ol[3];
    }
    else
    {
      int w=(m_iconCfg->olimage->getWidth()-2)/3-m_iconCfg->image_ltrb_ol[0]-m_iconCfg->image_ltrb_ol[2];
      if (w<1)w=1;
      double wsc=(r->right-r->left)/(double)w;

      int h=m_iconCfg->olimage->getHeight()-2-m_iconCfg->image_ltrb_ol[1]-m_iconCfg->image_ltrb_ol[3];
      if (h<1)h=1;
      double hsc=(r->bottom-r->top)/(double)h;

      r->left-=(int) (m_iconCfg->image_ltrb_ol[0]*wsc);
      r->top-=(int) (m_iconCfg->image_ltrb_ol[1]*hsc);
      r->right+=(int) (m_iconCfg->image_ltrb_ol[2]*wsc);
      r->bottom+=(int) (m_iconCfg->image_ltrb_ol[3]*hsc);
    }
  }
}
void WDL_VirtualIconButton_PreprocessSkinConfig(WDL_VirtualIconButton_SkinConfig *a)
{
  if (a && a->image)
  {
    a->image_ltrb_used.flags=0;
    int wi;
    for(wi=0;wi<2;wi++)
    {
      LICE_IBitmap *srcimg = wi ? a->image : a->olimage;
      if (!srcimg) continue;
      int w=srcimg->getWidth();
      int h=srcimg->getHeight();

      if (LICE_GetPixel(srcimg,0,0)==LICE_RGBA(255,0,255,255)&&
          LICE_GetPixel(srcimg,w-1,h-1)==LICE_RGBA(255,0,255,255))
      {
        int lext=0,rext=0,bext=0,text=0;
        int x;
        for (x = 1; x < w/3 && LICE_GetPixel(srcimg,x,0)==LICE_RGBA(255,0,255,255); x ++);
        lext=x-1;
        for (x = 1; x < h && LICE_GetPixel(srcimg,0,x)==LICE_RGBA(255,0,255,255); x ++);
        text=x-1;

        for (x = w-2; x >= (w*2/3) && LICE_GetPixel(srcimg,x,h-1)==LICE_RGBA(255,0,255,255); x --);
        rext=w-2-x;
        for (x = h-2; x >= text && LICE_GetPixel(srcimg,w-1,x)==LICE_RGBA(255,0,255,255); x --);
        bext=h-2-x;
        if (lext||text||rext||bext)
        {
          a->image_ltrb_used.flags |= 1 << wi;
          short *buf = wi ? a->image_ltrb_main : a->image_ltrb_ol;
          buf[0]=lext;
          buf[1]=text;
          buf[2]=rext;
          buf[3]=bext;
        }
      }
    }
  }
}
