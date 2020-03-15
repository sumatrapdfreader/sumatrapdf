#ifndef _WDL_VIRTWND_SKIN_H_
#define _WDL_VIRTWND_SKIN_H_

class LICE_IBitmap;

#include "../ptrlist.h"

typedef struct // if set these override the default virtualwnd styles for this object
{
  LICE_IBitmap *bgimage;
  int bgimage_lt[2],bgimage_rb[2]; // size of 
  int bgimage_lt_out[2],bgimage_rb_out[2]; // size of outside area (like shadows)
  int bgimage_noalphaflags; // 4x4 flags of "no alpha", so 65535 is image has no alpha whatsoever
} WDL_VirtualWnd_BGCfg;



class WDL_VirtualWnd_BGCfgCache_ar;

class WDL_VirtualWnd_BGCfgCache
{
public:
  WDL_VirtualWnd_BGCfgCache(int want_size=15, int max_size=30);
  ~WDL_VirtualWnd_BGCfgCache();

  void Invalidate();

  LICE_IBitmap *GetCachedBG(int w, int h, int sinfo2, void *owner_hint, const LICE_IBitmap *bgbmp);
  LICE_IBitmap *SetCachedBG(int w, int h, int sinfo2, LICE_IBitmap *bm, void *owner_hint, const LICE_IBitmap *bgbmp);

private:
  WDL_VirtualWnd_BGCfgCache_ar *m_ar;

  
  int m_want_size, m_max_size;
};

void WDL_VirtualWnd_PreprocessBGConfig(WDL_VirtualWnd_BGCfg *a);

// used by elements to draw a WDL_VirtualWnd_BGCfg
#define WDL_VWND_SCALEDBLITBG_IGNORE_LR 0x40000000
#define WDL_VWND_SCALEDBLITBG_IGNORE_INSIDE 0x20000000
#define WDL_VWND_SCALEDBLITBG_IGNORE_OUTSIDE 0x10000000
void WDL_VirtualWnd_ScaledBlitBG(LICE_IBitmap *dest, 
                                 WDL_VirtualWnd_BGCfg *src,
                                 int destx, int desty, int destw, int desth,
                                 int clipx, int clipy, int clipw, int cliph,
                                 float alpha, int mode);
int WDL_VirtualWnd_ScaledBG_GetPix(WDL_VirtualWnd_BGCfg *src,
                                   int ww, int wh,
                                   int x, int y);

void WDL_VirtualWnd_ScaledBlitSubBG(LICE_IBitmap *dest,
                                    WDL_VirtualWnd_BGCfg *src,
                                    int destx, int desty, int destw, int desth,
                                    int clipx, int clipy, int clipw, int cliph,
                                    int srcx, int srcy, int srcw, int srch, // these coordinates are not including pink lines (i.e. if pink lines are present, use src->bgimage->getWidth()-2, etc)
                                    float alpha, int mode);


typedef struct // if set these override the default virtualwnd styles for this object
{
  WDL_VirtualWnd_BGCfg bgimagecfg[2];
  LICE_IBitmap *thumbimage[2]; // h,v 
  int thumbimage_lt[2],thumbimage_rb[2];
  unsigned int zeroline_color; // needs alpha channel set!
} WDL_VirtualSlider_SkinConfig;

void WDL_VirtualSlider_PreprocessSkinConfig(WDL_VirtualSlider_SkinConfig *a);

typedef struct
{
  LICE_IBitmap *image; // 3x width, second third is "mouseover" image. then mousedown, or straight image if image_issingle set
  LICE_IBitmap *olimage; // drawn in second pass

  union
  {
    char flags; // &1 = overlay, &2=main
    bool asBool; // on PPC this is 4 bytes, need to preserve it
  }
  image_ltrb_used;
  bool image_issingle;
  short image_ltrb_ol[4]; // extents outside the rect
  short image_ltrb_main[4]; // unscaled areas of main image (not used if single)
} WDL_VirtualIconButton_SkinConfig;

void WDL_VirtualIconButton_PreprocessSkinConfig(WDL_VirtualIconButton_SkinConfig *a);



#endif
