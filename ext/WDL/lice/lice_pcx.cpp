/*
  Cockos WDL - LICE - Lightweight Image Compositing Engine
  Copyright (C) 2007 and later, Cockos Incorporated
  File: lice_pcx.cpp (PCX loading for LICE)
  See lice.h for license and other information
*/

#include "lice.h"
#include "../wdltypes.h"

#include <stdio.h>

// note: you'd never really want to use PCX files, but in case you do...

LICE_IBitmap *LICE_LoadPCX(const char *filename, LICE_IBitmap *_bmp)
{
  FILE *fp = fopen(filename,"rb");
  if(!fp) return 0;

  fgetc(fp);
  if (fgetc(fp) != 5) { fclose(fp); return NULL; }
  if (fgetc(fp) != 1) { fclose(fp); return NULL; }
  if (fgetc(fp) != 8) { fclose(fp); return NULL; }

  int sx = fgetc(fp); sx += fgetc(fp)<<8;
  int sy = fgetc(fp); sy += fgetc(fp)<<8;
  int ex = fgetc(fp); ex += fgetc(fp)<<8;
  int ey = fgetc(fp); ey += fgetc(fp)<<8;


  unsigned char pal[768];
  fseek(fp,-769,SEEK_END);
  if (fgetc(fp) != 12) { fclose(fp); return NULL; }
  fread(pal,1,768,fp);
  if (feof(fp)) { fclose(fp); return NULL; }


  LICE_IBitmap *usebmp = NULL;
  if (_bmp) (usebmp=_bmp)->resize(ex-sx+1,ey-sy+1);
  else usebmp = new WDL_NEW LICE_MemBitmap(ex-sx+1,ey-sy+1);
  if (!usebmp || usebmp->getWidth() != (ex-sx+1) || usebmp->getHeight() != (ey-sy+1)) 
  {
    if (usebmp != _bmp) delete usebmp;
    fclose(fp);
    return NULL;
  }

  fseek(fp,128,SEEK_SET);

  LICE_Clear(usebmp,0);
  int y = usebmp->getHeight();
  int w = usebmp->getWidth();
  int rowspan = usebmp->getRowSpan();
  LICE_pixel *pout = usebmp->getBits();
  if (usebmp->isFlipped())
  {
    pout += rowspan*(y-1);
    rowspan=-rowspan;
  }
  while (y--)
  {
    int xpos = 0;
    while (xpos < w)
    {
      int c = fgetc(fp);
      if (c&~255) break;
      if ((c & 192) == 192) 
      {
        int oc = (fgetc(fp))&255;
        LICE_pixel t=LICE_RGBA(pal[oc*3],pal[oc*3+1],pal[oc*3+2],255);

        c&=63;
        while (c-- && xpos<w) pout[xpos++] =  t;
      } 
      else pout[xpos++] = LICE_RGBA(pal[c*3],pal[c*3+1],pal[c*3+2],255);
    } 
    pout+=rowspan;
  }
  fclose(fp);

  return usebmp;
}


class LICE_PCXLoader
{
public:
  _LICE_ImageLoader_rec rec;
  LICE_PCXLoader() 
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
      if (stricmp(p,".pcx")) return 0;
    }
    return LICE_LoadPCX(filename,bmpbase);
  }
  static const char *get_extlist()
  {
    return "PCX files (*.PCX)\0*.PCX\0";
  }

};

LICE_PCXLoader LICE_pcxldr;