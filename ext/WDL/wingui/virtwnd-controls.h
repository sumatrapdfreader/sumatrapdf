/*
    WDL - virtwnd-controls.h
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
      
*/



#ifndef _WDL_VIRTWND_CONTROLS_H_
#define _WDL_VIRTWND_CONTROLS_H_

#include "virtwnd.h"
#include "virtwnd-skin.h"

#include "../lice/lice_text.h"

// an app should implement these
extern int WDL_STYLE_WantGlobalButtonBorders();
extern bool WDL_STYLE_WantGlobalButtonBackground(int *col);
extern int WDL_STYLE_GetSysColor(int);
extern bool WDL_Style_WantTextShadows(int *col);

// this is the default, you can override per painter if you want
extern bool WDL_STYLE_GetBackgroundGradient(double *gradstart, double *gradslope); // return values 0.0-1.0 for each, return false if no gradient desired

// for slider
extern LICE_IBitmap *WDL_STYLE_GetSliderBitmap2(bool vert);
extern bool WDL_STYLE_AllowSliderMouseWheel();
extern int WDL_STYLE_GetSliderDynamicCenterPos();


// functions for handling knob drawing in non-vwnds
extern WDL_VirtualWnd_BGCfg *vwnd_slider_getknobimageforsize(WDL_VirtualWnd_BGCfg *knoblist, int nknoblist,int *vieww, int *viewh, int *ksw, int *ksh, int *ks_offs);
extern void vwnd_slider_drawknobstack(LICE_IBitmap *drawbm, double val, WDL_VirtualWnd_BGCfg *knobimage, int ksw, int ksh, int ks_offs, int dx, int dy, int dw, int dh, float alpha=1.0f);


/* recommended defaults for the above:

int WDL_STYLE_WantGlobalButtonBorders() { return 0; }
bool WDL_STYLE_WantGlobalButtonBackground(int *col) { return false; }
int WDL_STYLE_GetSysColor(int p) { return GetSysColor(p); }
bool WDL_Style_WantTextShadows(int *col) { return false; }
bool WDL_STYLE_GetBackgroundGradient(double *gradstart, double *gradslope) { return false; }
LICE_IBitmap *WDL_STYLE_GetSliderBitmap2(bool vert) { return NULL; }
bool WDL_STYLE_AllowSliderMouseWheel() { return true; }
int WDL_STYLE_GetSliderDynamicCenterPos() { return 500; }

*/



// virtwnd-iconbutton.cpp
class WDL_VirtualIconButton : public WDL_VWnd
{
  public:
    WDL_VirtualIconButton();
    virtual ~WDL_VirtualIconButton();
    virtual const char *GetType() { return "vwnd_iconbutton"; }

    virtual int OnMouseDown(int xpos, int ypos); // return -1 to eat, >0 to capture
    virtual void OnMouseMove(int xpos, int ypos);
    virtual void OnMouseUp(int xpos, int ypos);
    virtual bool OnMouseDblClick(int xpos, int ypos);

    virtual void OnPaint(LICE_IBitmap *drawbm, int origin_x, int origin_y, RECT *cliprect, int rscale);
    virtual void OnPaintOver(LICE_IBitmap *drawbm, int origin_x, int origin_y, RECT *cliprect, int rscale);

    virtual bool WantsPaintOver();
    virtual void GetPositionPaintOverExtent(RECT *r, int rscale);


    void SetEnabled(bool en) {m_en=en; }
    bool GetEnabled() { return m_en; }

    void SetGrayed(bool grayed) { m_grayed = grayed; SetEnabled(!grayed); }

    virtual void SetIcon(WDL_VirtualIconButton_SkinConfig *cfg, float alpha=1.0f, bool buttonownsicon=false);
    void SetIsButton(bool isbutton) { m_is_button=isbutton; }
    bool GetIsButton() { return m_is_button; }

    void SetImmediate(bool immediate) { m_immediate=immediate; } // send message on mousedown, not mouseup

    void SetBGCol1Callback(int msg) { m_bgcol1_msg=msg; }

    void SetForceBorder(bool fb) { m_forceborder=fb; }

    // only used if no icon config set, or if force is set
    void SetTextLabel(const char *text); // no change of alignment etc
    void SetTextLabel(const char *text, int align, LICE_IFont *font=NULL);
    const char* GetTextLabel() { return m_textlbl.Get(); }
    void SetMargins(int l, int r) { m_margin_l=l; m_margin_r=r; }
    void SetVMargins(int t, int b) { m_margin_t=t; m_margin_b=b; };

    // if icon config is set, check state == 1 will swap the up and down image
    void SetCheckState(char state); // -1 = no checkbox, 0=unchecked, 1=checked
    char GetCheckState() { return m_checkstate; }
    
    WDL_VirtualIconButton_SkinConfig* GetIcon() { return m_iconCfg; } // note button does not own m_iconCfg
    bool ButtonOwnsIcon() { return m_ownsicon; }

    void SetForceText(bool ft, int color=0) { m_forcetext=ft; m_forcetext_color=color; }
    bool GetForceText() { return m_forcetext; }
    void SetTextLabelAlign(char align) { m_textalign=align; }

    void SetFont(LICE_IFont *font, LICE_IFont *vfont=NULL) { m_textfont=font; m_textfontv=vfont; }
    LICE_IFont *GetFont(bool vfont=false) { return vfont?m_textfontv:m_textfont; }

  protected:

    void DoSendCommand(int xpos, int ypos);

    WDL_VirtualIconButton_SkinConfig *m_iconCfg;  
    int m_bgcol1_msg;
    int m_margin_r, m_margin_l;
    int m_margin_t, m_margin_b;
    float m_alpha;
    bool m_is_button,m_forceborder;
    char m_pressed;
    bool m_en, m_grayed, m_ownsicon;
    bool m_immediate;
    char m_textalign;
    char m_checkstate;
    bool m_forcetext;
    int m_forcetext_color;

    WDL_FastString m_textlbl;
    LICE_IFont *m_textfont,*m_textfontv;
};



class WDL_VirtualStaticText : public WDL_VWnd
{
  public:
    WDL_VirtualStaticText();
    virtual ~WDL_VirtualStaticText();
    
    virtual const char *GetType() { return "vwnd_statictext"; }

    virtual void OnPaint(LICE_IBitmap *drawbm, int origin_x, int origin_y, RECT *cliprect, int rscale);
    virtual bool OnMouseDblClick(int xpos, int ypos);
    virtual int OnMouseDown(int xpos, int ypos);

    virtual void GetPositionPaintExtent(RECT *r, int rscale);

    void SetWantSingleClick(bool ws) {m_wantsingle=ws; }
    void SetFont(LICE_IFont *font, LICE_IFont *vfont=NULL) { m_font=font; m_vfont=vfont; }
    LICE_IFont *GetFont(bool vfont=false) { return vfont?m_vfont:m_font; }
    void SetAlign(int align) { m_align=align; } // -1=left,0=center,1=right
    void SetText(const char *text);
    void SetBorder(bool bor) { m_wantborder=bor; }
    const char *GetText() { return m_text.Get(); }
    void SetColors(int fg=0, int bg=0, bool tint=false) { m_fg=fg; m_bg=bg; m_dotint=tint; }
    void SetMargins(int l, int r) { m_margin_l=l; m_margin_r=r; }
    void SetVMargins(int t, int b) { m_margin_t=t; m_margin_b=b; };
    void SetBkImage(WDL_VirtualWnd_BGCfg *bm) { m_bkbm=bm; }
    WDL_VirtualWnd_BGCfg* GetBkImage() { return m_bkbm; }
    void SetWantPreserveTrailingNumber(bool preserve); // if the text ends in a number, make sure the number is always displayed

  protected:
    WDL_VirtualWnd_BGCfg *m_bkbm;
    int m_align;
    bool m_dotint;
    int m_fg,m_bg;
    int m_margin_r, m_margin_l;
    int m_margin_t, m_margin_b;
    bool m_wantborder;
    bool m_wantsingle;
    bool m_wantabbr;
    LICE_IFont *m_font,*m_vfont;
    WDL_FastString m_text;
    bool m_didvert; // true if text was drawn vertically on the last paint
    int m_didalign; // the actual alignment used on the last paint
};

class WDL_VirtualComboBox : public WDL_VWnd
{
  public:
    WDL_VirtualComboBox();
    virtual ~WDL_VirtualComboBox();
    virtual const char *GetType() { return "vwnd_combobox"; }
    virtual void OnPaint(LICE_IBitmap *drawbm, int origin_x, int origin_y, RECT *cliprect, int rscale);
    virtual int OnMouseDown(int xpos, int ypos);

    void SetFont(LICE_IFont *font) { m_font=font; }
    LICE_IFont *GetFont() { return m_font; }
    void SetAlign(int align) { m_align=align; } // -1=left,0=center,1=right

    int GetCurSel() { if (m_items.Get(m_curitem)) return m_curitem; return -1; }
    void SetCurSel(int sel) { if (!m_items.Get(sel)) sel=-1; if (m_curitem != sel) { m_curitem=sel; RequestRedraw(NULL); } }

    int GetCount() { return m_items.GetSize(); }
    void Empty() { m_items.Empty(true,free); m_itemdatas.Empty(); }

    int AddItem(const char *str, void *data=NULL) { m_items.Add(strdup(str)); m_itemdatas.Add(data); return m_items.GetSize()-1; }
    const char *GetItem(int item) { return m_items.Get(item); }
    void *GetItemData(int item) { return m_itemdatas.Get(item); }


  protected:
    int m_align;
    int m_curitem;
    LICE_IFont *m_font;

    WDL_PtrList<char> m_items;
    WDL_PtrList<void> m_itemdatas;
};



class WDL_VirtualSlider : public WDL_VWnd
{
  public:
    WDL_VirtualSlider();
    virtual ~WDL_VirtualSlider();
    virtual const char *GetType() { return "vwnd_slider"; }

    virtual void OnPaint(LICE_IBitmap *drawbm, int origin_x, int origin_y, RECT *cliprect, int rscale);
    virtual int OnMouseDown(int xpos, int ypos);
    virtual void OnMouseMove(int xpos, int ypos);
    virtual void OnMouseUp(int xpos, int ypos);
    virtual bool OnMouseDblClick(int xpos, int ypos);
    virtual bool OnMouseWheel(int xpos, int ypos, int amt);
    virtual void GetPositionPaintExtent(RECT *r, int rscale);

    virtual void OnCaptureLost();

    void SetBGCol1Callback(int msg) { m_bgcol1_msg=msg; }
    void SetScrollMessage(int msg) { m_scrollmsg=msg; }
    void SetRange(int minr, int maxr, int center) { m_minr=minr; m_maxr=maxr; m_center=center; }
    void GetRange(int *minr, int *maxr, int *center) { if (minr) *minr=m_minr; if (maxr) *maxr=m_maxr; if (center) *center=m_center; }
    int GetSliderPosition();
    void SetSliderPosition(int pos);
    bool GetIsVert();
    void SetNotifyOnClick(bool en) { m_sendmsgonclick=en; }  // default false

    void SetDblClickCallback(int msg) { m_dblclickmsg=msg; }
    int GetDblClickCallback() { return m_dblclickmsg; }

    void SetGrayed(bool grayed) { m_grayed = grayed; }

    void GetButtonSize(int *w, int *h, int rscale);

    void SetSkinImageInfo(WDL_VirtualSlider_SkinConfig *cfg, WDL_VirtualWnd_BGCfg *knobbg=NULL, WDL_VirtualWnd_BGCfg *knobbgsm=NULL, WDL_VirtualWnd_BGCfg *knobstacks=NULL, int nknobstacks=0)
    { 
      m_skininfo=cfg; 
      m_knobbg[0]=knobbgsm;
      m_knobbg[1]=knobbg;
      m_knobstacks=knobstacks;
      m_nknobstacks=nknobstacks;
    }

    void SetFGColors(int knobcol, int zlcol) { m_knob_color=knobcol; m_zl_color = zlcol; }
    void SetKnobBias(int knobbias, int knobextrasize=0) { m_knobbias=knobbias; m_knob_lineextrasize=knobextrasize; } // 1=force knob, -1=prevent knob

    void SetAccessDescCopy(const char *str);
    void SetAccessValueDesc(const char *str);
    virtual bool GetAccessValueDesc(char *buf, int bufsz);

  protected:
    WDL_FastString m_valueText;
    WDL_VirtualSlider_SkinConfig *m_skininfo;
    WDL_VirtualWnd_BGCfg *m_knobbg[2];
    WDL_VirtualWnd_BGCfg *getKnobBackgroundForSize(int sz) const;
    WDL_VirtualWnd_BGCfg *m_knobstacks;
    char *m_accessDescCopy;
    int m_nknobstacks;

    int m_bgcol1_msg;
    int m_scrollmsg;
    int m_dblclickmsg;

    void OnMoveOrUp(int xpos, int ypos, int isup);
    int m_minr, m_maxr, m_center, m_pos;

    int m_tl_extra, m_br_extra;

    int m_knob_color,m_zl_color;
    int m_last_rscale, m_last_advscale;

    signed char m_knobbias;
    signed char m_knob_lineextrasize;
    bool m_captured;
    bool m_needflush;
    bool m_sendmsgonclick;
    bool m_grayed;
    bool m_is_knob;
};


#define WDL_VWND_LISTBOX_ARROWINDEX 0x10000000
#define WDL_VWND_LISTBOX_ARROWINDEX_LR 0x10000001

class WDL_VirtualListBox : public WDL_VWnd
{
  public:
    WDL_VirtualListBox();
    virtual ~WDL_VirtualListBox();
    virtual const char *GetType() { return "vwnd_listbox"; }

    virtual void OnPaint(LICE_IBitmap *drawbm, int origin_x, int origin_y, RECT *cliprect, int rscale);
    virtual int OnMouseDown(int xpos, int ypos);
    virtual bool OnMouseDblClick(int xpos, int ypos);
    virtual bool OnMouseWheel(int xpos, int ypos, int amt);
    virtual void OnMouseMove(int xpos, int ypos);
    virtual void OnMouseUp(int xpos, int ypos);

    void SetFont(LICE_IFont *font, int lsadj=-1000) { m_font=font; m_lsadj=lsadj; }
    LICE_IFont *GetFont() { return m_font; }
    void SetAlign(int align) { m_align=align; } // -1=left,0=center,1=right
    void SetRowHeight(int rh) { m_rh=rh; }
    void SetMaxColWidth(int cw) { m_maxcolwidth=cw; } // 0 = default = allow any sized columns
    void SetMinColWidth(int cw) { m_mincolwidth = cw; } // 0 = default = full width columns
    void SetMargins(int l, int r) { m_margin_l=l; m_margin_r=r; }
    void SetScrollButtonSize(int sz) { m_scrollbuttonsize=sz; } // def 14
    int GetRowHeight() { return m_rh; }
    int GetItemHeight(int idx); // usually row height but not always
    int GetMaxColWidth() { return m_maxcolwidth; }
    int GetMinColWidth() { return m_mincolwidth; }

    void SetDroppedMessage(int msg) { m_dropmsg=msg; }
    void SetClickedMessage(int msg) { m_clickmsg=msg; }
    void SetDragMessage(int msg) { m_dragmsg=msg; }
    int IndexFromPt(int x, int y);
    bool GetItemRect(int item, RECT *r); // returns FALSE if not onscreen
    int GetVisibleItemRects(WDL_TypedBuf<RECT> *list);

    void SetGrayed(bool grayed) { m_grayed=grayed; }    

    void SetViewOffset(int offs);
    int GetViewOffset();

    RECT *GetScrollButtonRect(bool isDown) { return m_lastscrollbuttons[isDown?1:0].left<m_lastscrollbuttons[isDown?1:0].right ? &m_lastscrollbuttons[isDown?1:0]:NULL; }

    // idx<0 means return count of items
    int (*m_GetItemInfo)(WDL_VirtualListBox *sender, int idx, char *nameout, int namelen, int *color, WDL_VirtualWnd_BGCfg **bkbg);
    void (*m_CustomDraw)(WDL_VirtualListBox *sender, int idx, RECT *r, LICE_IBitmap *drawbm, int rscale);
    int (*m_GetItemHeight)(WDL_VirtualListBox *sender, int idx); // returns -1 for default height
    void *m_GetItemInfo_ctx;

  protected:

    struct layout_info {
      int startpos; // first visible item index
      int columns; // 1 or more
      int item_area_w, item_area_h; // area for items (starting at leftrightbutton_width,0)
      int leftrightbutton_w;
      int updownbutton_h;
      WDL_TypedBuf<int> *heights; // visible heights of items starting at startpos
    };
  
    int IndexFromPtInt(int x, int y, const layout_info &layout);
    void CalcLayout(int num_items, layout_info *layout);
    bool HandleScrollClicks(int xpos, int ypos, const layout_info *layout);
    void DoScroll(int dir, const layout_info *layout);
  
    int m_cap_state;
    POINT m_cap_startpos;
    int m_cap_startitem;
    int m_clickmsg,m_dropmsg,m_dragmsg;
    int m_viewoffs;
    int m_align;
    int m_margin_r, m_margin_l;
    int m_rh,m_maxcolwidth,m_mincolwidth ;
    int m_scrollbuttonsize;
    int m_lsadj;
    LICE_IFont *m_font;
    bool m_grayed;

    RECT m_lastscrollbuttons[2];
};


#endif
