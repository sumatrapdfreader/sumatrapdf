/*
  Cockos WDL - LICE - Lightweight Image Compositing Engine
  Copyright (C) 2007 and later, Cockos Incorporated
  File: lice_ico.cpp (ICO icon file loading for LICE)
  See lice.h for license and other information

  This file contains some code from Microsoft's MSDN ICO loading sample
  see: http://msdn2.microsoft.com/en-us/library/ms997538.aspx
*/

#include "lice.h"
#include "../wdltypes.h"
#ifndef _WIN32
#include "../swell/swell.h"
#endif

static LICE_IBitmap *icoToBitmap(HICON icon, LICE_IBitmap *bmpOut)
{
  int icon_w = 16, icon_h=16;

#ifdef _WIN32
  ICONINFO ii={0,};
  if (GetIconInfo(icon,&ii))
  {
    bool blah=false;
    if (ii.hbmColor)
    {
      BITMAP bm={0,};
      if (GetObject(ii.hbmColor,sizeof(bm),&bm) && bm.bmWidth && bm.bmHeight)
      {
        icon_w=bm.bmWidth;
        icon_h=bm.bmHeight;
        blah=true;
      }
      DeleteObject(ii.hbmColor);
    }
    if (ii.hbmMask)
    {
      BITMAP bm={0,};
      if (!blah && GetObject(ii.hbmMask,sizeof(bm),&bm) && bm.bmWidth && bm.bmHeight)
      {
        icon_w=bm.bmWidth;
        icon_h=bm.bmHeight;
      }
      DeleteObject(ii.hbmMask);
    }
  }
#else
  BITMAP bm={0,};
  if (GetObject(icon,sizeof(bm),&bm) && bm.bmWidth && bm.bmHeight) // SWELL's GetObject() works on icons
  {
    icon_w=bm.bmWidth;
    icon_h=bm.bmHeight;
  }

#endif

  LICE_SysBitmap tempbm(icon_w*2,icon_h);
  LICE_FillRect(&tempbm,0,0,icon_w,icon_h,LICE_RGBA(0,0,0,255),1.0f,LICE_BLIT_MODE_COPY);
#ifdef _WIN32
  DrawIconEx(tempbm.getDC(),0,0,icon,icon_w,icon_h,0,NULL,DI_NORMAL);
#else
  {
    RECT r={0,0,icon_w,icon_h};
    DrawImageInRect(tempbm.getDC(),icon,&r);
  }
#endif

  LICE_FillRect(&tempbm,icon_w,0,icon_w,icon_h,LICE_RGBA(255,255,255,255),1.0f,LICE_BLIT_MODE_COPY);
#ifdef _WIN32
  DrawIconEx(tempbm.getDC(),icon_w,0,icon,icon_w,icon_h,0,NULL,DI_NORMAL);
#else
  {
    RECT r={icon_w,0,icon_w+icon_w,icon_h};
    DrawImageInRect(tempbm.getDC(),icon,&r);
  }
#endif

  if (!bmpOut) bmpOut = new WDL_NEW LICE_MemBitmap(icon_w,icon_h);
  else bmpOut->resize(icon_w,icon_h);

  int y; // since we have the image drawn on white and on black, we can calculate the alpha channel...
  if (bmpOut) for(y=0;y<icon_h;y++)
  {
    int x;
    for(x=0;x<icon_w;x++)
    {
      LICE_pixel p = LICE_GetPixel(&tempbm,x,y);
      LICE_pixel p2 = LICE_GetPixel(&tempbm,x+icon_w,y);

      int r1=LICE_GETR(p);
      int g1=LICE_GETG(p);
      int b1=LICE_GETB(p);

      int alpha=255 - (LICE_GETR(p2)-r1);
      if (alpha>=255) alpha=255;
      else if (alpha>0)
      {
        r1 = (r1*255)/alpha; // LICE stores its alpha channel non-premultiplied, so we need to scale these up.
        g1 = (g1*255)/alpha;
        b1 = (b1*255)/alpha;
        if (r1>255)r1=255;
        if (g1>255)g1=255;
        if (b1>255)b1=255;
      }
      else alpha=0;
      LICE_PutPixel(bmpOut,x,y,LICE_RGBA(r1,g1,b1,alpha),1.0f,LICE_BLIT_MODE_COPY);
    }
  }

  return bmpOut;
}

LICE_IBitmap *LICE_LoadIcon(const char *filename, int reqiconsz, LICE_IBitmap *bmp) // returns a bitmap (bmp if nonzero) on success
{
  if (reqiconsz<1) reqiconsz=16;
  HICON icon = NULL;
#ifdef _WIN32
  
#ifndef WDL_NO_SUPPORT_UTF8
  #ifdef WDL_SUPPORT_WIN9X
  if (GetVersion()<0x80000000)
  #endif
  {
    WCHAR wf[2048];
    if (MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,filename,-1,wf,2048))
      icon = (HICON)LoadImageW(NULL,wf,IMAGE_ICON,reqiconsz,reqiconsz,LR_LOADFROMFILE);
  }
#endif

  if (!icon) icon = (HICON)LoadImage(NULL,filename,IMAGE_ICON,reqiconsz,reqiconsz,LR_LOADFROMFILE);

#else
  icon = (HICON)LoadNamedImage(filename,false);
#endif
  if (!icon) return 0;

  LICE_IBitmap *ret=icoToBitmap(icon,bmp);
  DestroyIcon(icon);
  return ret;
}

LICE_IBitmap *LICE_LoadIconFromResource(HINSTANCE hInst, const char *resid, int reqiconsz, LICE_IBitmap *bmp) // returns a bitmap (bmp if nonzero) on success
{
#ifdef _WIN32
  if (reqiconsz<1) reqiconsz=16;
  HICON icon = (HICON)LoadImage(hInst,resid,IMAGE_ICON,reqiconsz,reqiconsz,0);
  if (!icon) return 0;

  LICE_IBitmap *ret=icoToBitmap(icon,bmp);
  DestroyIcon(icon);
  return ret;
#else
  return 0;
#endif
}




class LICE_ICOLoader
{
public:
  _LICE_ImageLoader_rec rec;
  LICE_ICOLoader() 
  {
    rec.loadfunc = loadfunc;
    rec.get_extlist = get_extlist;
    rec._next = LICE_ImageLoader_list;
    LICE_ImageLoader_list = &rec;
  }

  static LICE_IBitmap *loadfunc(const char *filename, bool checkFileName, LICE_IBitmap *bmpbase)
  {
    if (checkFileName)
    {
      const char *p=filename;
      while (*p)p++;
      while (p>filename && *p != '\\' && *p != '/' && *p != '.') p--;
      if (stricmp(p,".ico")) return 0;
    }
    return LICE_LoadIcon(filename,16,bmpbase);
  }
  static const char *get_extlist()
  {
    return "ICO files (*.ICO)\0*.ICO\0";
  }

};

LICE_ICOLoader LICE_icoldr;
