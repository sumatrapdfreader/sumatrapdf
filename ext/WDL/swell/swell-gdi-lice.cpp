/* Cockos SWELL (Simple/Small Win32 Emulation Layer for Linux/OSX)
   Copyright (C) 2006 and later, Cockos, Inc.

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
  

    This file provides basic win32 GDI-->lice translation. 

*/

#ifdef SWELL_LICE_GDI
#ifndef SWELL_PROVIDED_BY_APP

#include "swell.h"
#include "swell-internal.h"

#include "../mutex.h"
#include "../ptrlist.h"
#include "../wdlcstring.h"

#include "swell-gdi-internalpool.h"


#ifdef SWELL_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#ifdef SWELL_FONTCONFIG
#include <fontconfig/fontconfig.h>
#else
#include "../dirscan.h"
#endif

static bool s_freetype_failed;
static FT_Library s_freetype; // todo: TLS for multithread support? -- none of the text drawing is thread safe!

static int utf8char(const char *ptr, unsigned short *charOut) // returns char length
{
  const unsigned char *p = (const unsigned char *)ptr;
  unsigned char tc = *p;

  if (tc < 128) 
  {
    if (charOut) *charOut = (unsigned short) tc;
    return 1;
  }
  else if (tc < 0xC2) // invalid chars (subsequent in sequence, or overlong which we disable for)
  {
  }
  else if (tc < 0xE0) // 2 char seq
  {
    if (p[1] >= 0x80 && p[1] <= 0xC0)
    {
      if (charOut) *charOut = ((tc&0x1f)<<6) | (p[1]&0x3f);
      return 2;
    }
  }
  else if (tc < 0xF0) // 3 char seq
  {
    if (p[1] >= 0x80 && p[1] <= 0xC0 && p[2] >= 0x80 && p[2] <= 0xC0)
    {
      if (charOut) *charOut = ((tc&0xf)<<12) | ((p[1]&0x3f)<<6) | ((p[2]&0x3f));
      return 3;
    }
  }
  else if (tc < 0xF5) // 4 char seq
  {
    if (p[1] >= 0x80 && p[1] <= 0xC0 && p[2] >= 0x80 && p[2] <= 0xC0 && p[3] >= 0x80 && p[3] <= 0xC0)
    {
      if (charOut) *charOut = (unsigned short)' '; // dont support 4 byte sequences yet(ever?)
      return 4;
    }
  }  
  if (charOut) *charOut = (unsigned short) tc;
  return 1;  
}

extern const char *swell_last_font_filename;

#ifndef SWELL_FREETYPE_CACHE_SIZE
#define SWELL_FREETYPE_CACHE_SIZE 80
#endif

class fontConfigCacheEnt
{
  public:
    fontConfigCacheEnt(const char *name, int flags, int w, int h, FT_Face face, const char *fndesc)
    {
      m_name = strdup(name);
      m_flags = flags;
      m_face = face;
      m_w=w;
      m_h=h;
      m_fndesc = strdup(fndesc);
      FT_Reference_Face(m_face);
    }
    ~fontConfigCacheEnt() 
    { 
      free(m_name); 
      free(m_fndesc);
      FT_Done_Face(m_face); 
    }

    char *m_name;
    char *m_fndesc;
    int m_flags, m_w, m_h;
    FT_Face m_face;
};


#ifdef SWELL_FONTCONFIG

static FcConfig *s_fontconfig;
const char *swell_enumFontFiles(int x)
{
  if (!s_fontconfig) return NULL;

  static FcPattern *pat;
  static FcObjectSet *prop;
  static FcFontSet *fonts;
  if (x<0)
  {
    if (fonts) FcFontSetDestroy(fonts);
    if (prop) FcObjectSetDestroy(prop);
    if (pat) FcPatternDestroy(pat);
    pat=NULL;
    prop=NULL;
    fonts=NULL;
    return NULL;
  }
  if (!pat)
  {
    pat = FcPatternCreate();
    prop = FcObjectSetBuild(FC_FAMILY, NULL);
    fonts = FcFontList(s_fontconfig,pat,prop);
  }
  if (fonts && x < fonts->nfont)
  {
    FcPattern *f = fonts->fonts[x];
    FcChar8 *fn = NULL;
    if (FcPatternGetString(f,FC_FAMILY,0,&fn) == FcResultMatch && fn && *fn) 
    return (const char *)fn;
  }
  
  return NULL;
}

#else

static WDL_PtrList<char> s_freetype_fontlist;
static WDL_PtrList<char> s_freetype_regfonts;

const char *swell_enumFontFiles(int x)
{
  const int n1 = s_freetype_regfonts.GetSize();
  if (x < n1) return s_freetype_regfonts.Get(x);
  return s_freetype_fontlist.Get(x-n1);
}


static const char *stristr(const char *a, const char *b)
{
  const size_t blen = strlen(b);
  while (*a && strnicmp(a,b,blen)) a++;
  return *a ? a : NULL;
}

static void ScanFontDirectory(const char *path, int maxrec=3)
{
  WDL_DirScan ds;
  WDL_FastString fs;
  if (!ds.First(path)) do
  { 
    if (ds.GetCurrentFN()[0] != '.')
    {
      if (ds.GetCurrentIsDirectory())
      {
        if (maxrec>0 && 
            stricmp(ds.GetCurrentFN(),"type1") &&
            stricmp(ds.GetCurrentFN(),"cmap") &&
            stricmp(ds.GetCurrentFN(),"ghostscript") &&
            strcmp(ds.GetCurrentFN(),"X11")
           )
        {
          ds.GetCurrentFullFN(&fs);
          ScanFontDirectory(fs.Get(),maxrec-1);
        }
      }
      else
      {
        const char *ext = WDL_get_fileext(ds.GetCurrentFN());
        if (!stricmp(ext,".ttf") || !stricmp(ext,".otf"))
        {
          ds.GetCurrentFullFN(&fs);
          s_freetype_fontlist.Add(strdup(fs.Get()));
        }
      }
    }
  } while (!ds.Next());
}

static int sortByFilePart(const char **a, const char **b)
{
  return stricmp(WDL_get_filepart(*a),WDL_get_filepart(*b));
}

struct fontScoreMatched {
  int score1,score2;
  const char *fn;

  static int sortfunc(const void *a, const void *b)
  {
    const int v = ((const fontScoreMatched *)a)->score1 - ((const fontScoreMatched *)b)->score1;
    return v?v:((const fontScoreMatched *)a)->score2 - ((const fontScoreMatched *)b)->score2;
  }

};

static FT_Face MatchFont(const char *lfFaceName, int weight, int italic, int exact, char *matchFnOut, size_t matchFnOutSize)
{
  const int fn_len = strlen(lfFaceName), ntab=2;
  WDL_PtrList<char> *tab[ntab]= { &s_freetype_regfonts, &s_freetype_fontlist };
  int x;
  bool match;
  static WDL_TypedBuf<fontScoreMatched> matchlist;
  matchlist.Resize(0,false);

  for (x=0;x<ntab;x++) 
  {
    WDL_PtrList<char> *t = tab[x];
    int px = t->LowerBound(lfFaceName,&match,sortByFilePart);
    for (;;) 
    {
      const char *fn = t->Get(px++);
      if (!fn) break;
      const char *fnp = WDL_get_filepart(fn);
      if (strnicmp(fnp,lfFaceName,fn_len)) break;

      fontScoreMatched s;
      const char *residual = fnp + fn_len;
      const char *dash = strstr(residual,"-");
      const char *ext = WDL_get_fileext(residual);

      if (!dash)
      {
        if (!strnicmp(residual,"Bold",4) ||
            !strnicmp(residual,"Italic",6) ||
            !strnicmp(residual,"Light",5) ||
            !strnicmp(residual,"Oblique",7))
          dash = residual;
        else if (ext > residual && ext <= residual+2)
        {
          char c1 = residual[0],c2=residual[1];
          if (c1>0) c1=toupper(c1);
          if (c2>0) c2=toupper(c2);
          if ((c1 == 'B' || c1 == 'I' || c1 == 'L') &&
              (c2 == 'B' || c2 == 'I' || c2 == 'L' || c2 == '.'))
            dash=residual;
        }
      }

      s.fn = fn;
      s.score1 = (int)((dash?dash:ext)-residual); // characters between font and either "-" or "."

      if (exact > 0)
      {
        if (s.score1) continue;
      }
      else if (exact < 0 && !s.score1) 
      {
        continue;
      }

      s.score2 = 0;

      if (dash) { if (*dash == '-') dash++; }
      else dash=residual;

      while (*dash && *dash != '.')
      {
        if (*dash > 0 && isalnum(*dash)) s.score2++;
        dash++;
      }

      if (stristr(residual,"Regular")) s.score2 -= 7; // ignore "Regular"
      if (italic && stristr(residual,"Italic")) s.score2 -= 6+7;
      else if (italic && stristr(residual,"Oblique")) s.score2 -= 7+3; // if Italic isnt available, use Oblique
      if (weight >= FW_BOLD && stristr(residual,"Bold")) s.score2 -= 4+7;
      else if (weight <= FW_LIGHT && stristr(residual,"Light")) s.score2 -= 5+7;

      if (ext > residual && ext <= residual+2)
      {
        char c1 = residual[0],c2=residual[1];
        if (c1>0) c1=toupper(c1);
        if (c2>0) c2=toupper(c2);

        if (weight >= FW_BOLD && (c1 == 'B' || c2 == 'B')) s.score2 -= 2;
        else if (weight <= FW_LIGHT && (c1 == 'L' || c2 == 'L')) s.score2 -= 2;
        if (italic && (c1 == 'I' || c2 == 'I')) s.score2 -= 2;
      }

      s.score2 = s.score2*ntab + x; 

      matchlist.Add(s);
    }
  } 
  if (matchlist.GetSize()>1)
    qsort(matchlist.Get(),matchlist.GetSize(),sizeof(fontScoreMatched),fontScoreMatched::sortfunc);

  for (x=0; x < matchlist.GetSize(); x ++)
  {
    const fontScoreMatched *s = matchlist.Get()+x;
 
    FT_Face face=NULL;
    //printf("trying '%s' for '%s' score %d,%d w %d i %d\n",s->fn,lfFaceName,s->score1,s->score2,weight,italic);
    FT_New_Face(s_freetype,s->fn,0,&face);
    if (face) 
    {
      lstrcpyn_safe(matchFnOut,s->fn,matchFnOutSize);
      return face;
    }
  }
  return NULL;
}
#endif // !SWELL_FONTCONFIG

#endif // SWELL_FREETYPE

HDC SWELL_CreateMemContext(HDC hdc, int w, int h)
{
  LICE_MemBitmap * bm = new LICE_MemBitmap(w,h);
  if (!bm) return 0;
  LICE_Clear(bm,LICE_RGBA(0,0,0,0));

  HDC__ *ctx=SWELL_GDP_CTX_NEW();
  ctx->surface = bm;
  ctx->surface_offs.x=0;
  ctx->surface_offs.y=0;
  ctx->dirty_rect_valid=false;

  SetTextColor(ctx,0);
  return ctx;
}


void SWELL_DeleteGfxContext(HDC ctx)
{
  HDC__ *ct=(HDC__ *)ctx;
  if (HDC_VALID(ct))
  {   
    delete ct->surface;
    ct->surface=0;
    SWELL_GDP_CTX_DELETE(ct);
  }
}

HPEN CreatePen(int attr, int wid, int col)
{
  return CreatePenAlpha(attr,wid,col,1.0f);
}

HBRUSH CreateSolidBrush(int col)
{
  return CreateSolidBrushAlpha(col,1.0f);
}

HPEN CreatePenAlpha(int attr, int wid, int col, float alpha)
{
  HGDIOBJ__ *pen=GDP_OBJECT_NEW();
  pen->type=TYPE_PEN;
  pen->wid=wid<0?0:wid;
  pen->alpha = alpha;
  pen->color=LICE_RGBA_FROMNATIVE(col);
  return pen;
}
HBRUSH  CreateSolidBrushAlpha(int col, float alpha)
{
  HGDIOBJ__ *brush=GDP_OBJECT_NEW();
  brush->type=TYPE_BRUSH;
  brush->color=LICE_RGBA_FROMNATIVE(col);
  brush->alpha = alpha;
  brush->wid=0; 
  return brush;
}

void SetBkColor(HDC ctx, int col)
{
  HDC__ *ct=(HDC__ *)ctx;
  if (!HDC_VALID(ct)) return;
  ct->curbkcol=LICE_RGBA_FROMNATIVE(col,255);
}

void SetBkMode(HDC ctx, int col)
{
  HDC__ *ct=(HDC__ *)ctx;
  if (!HDC_VALID(ct)) return;
  ct->curbkmode=col;
}

HGDIOBJ GetStockObject(int wh)
{
  switch (wh)
  {
    case NULL_BRUSH:
    {
      static HGDIOBJ__ br={0,};
      br.type=TYPE_BRUSH;
      br.wid=-1;
      return &br;
    }
    case NULL_PEN:
    {
      static HGDIOBJ__ pen={0,};
      pen.type=TYPE_PEN;
      pen.wid=-1;
      return &pen;
    }
  }
  return 0;
}

HFONT CreateFont(int lfHeight, int lfWidth, int lfEscapement, int lfOrientation, int lfWeight, char lfItalic, 
  char lfUnderline, char lfStrikeOut, char lfCharSet, char lfOutPrecision, char lfClipPrecision, 
         char lfQuality, char lfPitchAndFamily, const char *lfFaceName)
{
  HGDIOBJ__ *font=NULL;
#ifdef SWELL_FREETYPE
  if (!s_freetype_failed && !s_freetype) 
  {
    s_freetype_failed = !!FT_Init_FreeType(&s_freetype);
    if (s_freetype)
    {
#ifdef SWELL_FONTCONFIG
      if (!s_fontconfig) s_fontconfig = FcInitLoadConfigAndFonts();
#else
      ScanFontDirectory("/usr/share/fonts");

      qsort(s_freetype_fontlist.GetList(),s_freetype_fontlist.GetSize(),sizeof(const char *),(int (*)(const void *,const void*))sortByFilePart);
#endif
    }
  }
  if (lfWidth<0) lfWidth=-lfWidth;
  if (lfHeight<0) lfHeight=-lfHeight;

  static WDL_PtrList<fontConfigCacheEnt> cache;
  const int cache_flag = wdl_max(lfWeight,0) | (lfItalic ? (1<<30) : 0);
  FT_Face face=NULL;
  for (int x=0;x<cache.GetSize();x++)
  {
    fontConfigCacheEnt *ent = cache.Get(x);
    if (ent->m_flags==cache_flag && 
        ent->m_w == lfWidth && 
        ent->m_h == lfHeight && 
        !strcmp(ent->m_name,lfFaceName?lfFaceName:""))
    {
      face = ent->m_face;
      swell_last_font_filename = ent->m_fndesc;
      FT_Reference_Face(face);
      if (x < cache.GetSize()-1) 
      {
        cache.Delete(x);
        cache.Add(ent); // make this cache entry most recent
      }
      break;
    }
  }

  if (!face && s_freetype)
  {
    int face_idx=0;
    char face_fn[1024];
    face_fn[0]=0;
#ifdef SWELL_FONTCONFIG
    if (!face && s_fontconfig)
    {
      FcPattern *pat = FcPatternCreate();
      if (pat)
      {
        if (lfFaceName && *lfFaceName) FcPatternAddString(pat,FC_FAMILY,(FcChar8*)lfFaceName);
        if (lfWeight > 0)
        {
          int wt;
          if (lfWeight >= FW_HEAVY) wt=FC_WEIGHT_HEAVY;
          else if (lfWeight >= FW_EXTRABOLD) wt=FC_WEIGHT_EXTRABOLD;
          else if (lfWeight >= FW_BOLD) wt=FC_WEIGHT_BOLD;
          else if (lfWeight >= FW_SEMIBOLD) wt=FC_WEIGHT_SEMIBOLD;
          else if (lfWeight >= FW_MEDIUM) wt=FC_WEIGHT_MEDIUM;
          else if (lfWeight >= FW_NORMAL) wt=FC_WEIGHT_NORMAL;
          else if (lfWeight >= FW_LIGHT) wt=FC_WEIGHT_LIGHT;
          else if (lfWeight >= FW_EXTRALIGHT) wt=FC_WEIGHT_EXTRALIGHT;
          else wt=FC_WEIGHT_THIN;
          FcPatternAddInteger(pat,FC_WEIGHT,wt);
        }
        if (lfItalic)
          FcPatternAddInteger(pat,FC_SLANT,FC_SLANT_ITALIC);
        FcConfigSubstitute(s_fontconfig,pat,FcMatchPattern);
        FcDefaultSubstitute(pat);
        FcResult result;
        FcPattern *hit = FcFontMatch(s_fontconfig,pat,&result);
        if (hit)
        {
          FcChar8 *fn = NULL;
          if (FcPatternGetString(hit,FC_FILE,0,&fn) == FcResultMatch && fn && *fn) 
          {
            if (FcPatternGetInteger(hit,FC_INDEX,0,&face_idx) != FcResultMatch) face_idx=0;
            FT_New_Face(s_freetype,(const char *)fn,face_idx,&face);
            if (face) lstrcpyn_safe(face_fn,(const char *)fn,sizeof(face_fn));
          }
          FcPatternDestroy(hit);
        }
        FcPatternDestroy(pat);
      }
    }
#else

    if (!face && lfFaceName && *lfFaceName) face = MatchFont(lfFaceName,lfWeight,lfItalic,0, face_fn, sizeof(face_fn));

    if (!face)
    {
      static const char *fallbacklist[2];
      const int wl = (lfFaceName && (
                        !strnicmp(lfFaceName,"Courier",7) ||
                        !strnicmp(lfFaceName,"Fixed",5)
                        )) ? 1 : 0;
      if (!fallbacklist[wl])
      {
        static const char *ent[2] = { "ft_font_fallback", "ft_font_fallback_fixedwidth" };
        static const char *def[2] = { 
          "// LiberationSans Cantarell FreeSans DejaVuSans NotoSans Oxygen Arial Verdana", 
          "// LiberationMono FreeMono DejaVuSansMono NotoMono OxygenMono Courier" 
        };
        char tmp[1024];
        GetPrivateProfileString(".swell",ent[wl],"",tmp,sizeof(tmp),"");
        if (!tmp[0])
          WritePrivateProfileString(".swell",ent[wl],def[wl],"");
        const char *rp = tmp;
        while (*rp == ' ' || *rp == '\t') rp++;
        if (!*rp || *rp == '/') rp = def[wl] + 3;

        char *b = (char*) malloc(strlen(rp) + 2);
        fallbacklist[wl] = b ? b : "FreeSans\0";
        if (b)
        {
          for(;;)
          {
            while (*rp == ' ' || *rp == '\t') rp++;
            while (*rp && *rp != ' ' && *rp != '\t') *b++ = *rp++;
            *b++=0;
            if (!*rp) break;
          }
          *b++=0;
        }
      }
      for (int exact=0;exact<2 && !face;exact++)
      {
        const char *l = fallbacklist[wl];
        while (*l && !face)
        {
          face = MatchFont(l,lfWeight,lfItalic,exact?-1:1, face_fn, sizeof(face_fn));
          l += strlen(l)+1;
        }
      }
    }
#endif

    if (face)
    {
      if (face_idx) snprintf_append(face_fn,sizeof(face_fn)," <%d>",face_idx);
      fontConfigCacheEnt *ce = new fontConfigCacheEnt(lfFaceName?lfFaceName:"",cache_flag,lfWidth,lfHeight,face, face_fn);
      cache.Add(ce);
      if (cache.GetSize()>SWELL_FREETYPE_CACHE_SIZE) cache.Delete(0,true);
      swell_last_font_filename = ce->m_fndesc;

      FT_Set_Char_Size(face,lfWidth*64, lfHeight*64,0,0); // 72dpi
    }
  }
  
  if (face)
  {
    font = GDP_OBJECT_NEW();
    font->type=TYPE_FONT;
    font->typedata = face;
    font->alpha = 1.0f;
  }
#else
  font->type=TYPE_FONT;
#endif
 
  return font;
}


HFONT CreateFontIndirect(LOGFONT *lf)
{
  return CreateFont(lf->lfHeight, lf->lfWidth,lf->lfEscapement, lf->lfOrientation, lf->lfWeight, lf->lfItalic, 
                    lf->lfUnderline, lf->lfStrikeOut, lf->lfCharSet, lf->lfOutPrecision,lf->lfClipPrecision, 
                    lf->lfQuality, lf->lfPitchAndFamily, lf->lfFaceName);
}

int GetTextFace(HDC ctx, int nCount, LPTSTR lpFaceName)
{
  if (lpFaceName && nCount>0) lpFaceName[0]=0;
#ifdef SWELL_FREETYPE
  HDC__ *ct=(HDC__*)ctx;
  if (!HDC_VALID(ct) || nCount<1 || !lpFaceName || !ct->curfont) return 0;

  const FT_FaceRec *p = (const FT_FaceRec *)ct->curfont->typedata;
  if (p)
  {
    lstrcpyn_safe(lpFaceName, p->family_name, nCount);
    return 1;
  }
#endif
  return 0;
}

void DeleteObject(HGDIOBJ pen)
{
  if (HGDIOBJ_VALID(pen))
  {
    HGDIOBJ__ *p=(HGDIOBJ__ *)pen;
    if (--p->additional_refcnt < 0)
    {
      if (p->type == TYPE_PEN || p->type == TYPE_BRUSH || p->type == TYPE_FONT || p->type == TYPE_BITMAP)
      {
        if (p->type == TYPE_FONT)
        {
#ifdef SWELL_FREETYPE
          if (p->typedata)
          {
            FT_Done_Face((FT_Face)p->typedata);
            p->typedata = 0;
          }
#endif
        }
        else if (p->type == TYPE_PEN || p->type == TYPE_BRUSH)
        {
          if (p->wid<0) return;
        }
        else if (p->type == TYPE_BITMAP)
        { 
          if (p->wid>0) delete (LICE_IBitmap *)p->typedata;
          p->typedata = NULL;
        }
  
        GDP_OBJECT_DELETE(p);
      }
      // JF> don't free unknown objects, this should never happen anyway: else free(p);
    }
  }
}


HGDIOBJ SelectObject(HDC ctx, HGDIOBJ pen)
{
  HDC__ *c=(HDC__ *)ctx;
  HGDIOBJ__ *p= pen;
  HGDIOBJ__ **mod=0;
  if (!HDC_VALID(c)||!p) return 0;
  
  if (p == (HGDIOBJ__ *)TYPE_PEN) mod=&c->curpen;
  else if (p == (HGDIOBJ__ *)TYPE_BRUSH) mod=&c->curbrush;
  else if (p == (HGDIOBJ__ *)TYPE_FONT) mod=&c->curfont;

  if (mod)
  {
    HGDIOBJ__ *np=*mod;
    *mod=0;
    return np?np:p;
  }
  
  if (!HGDIOBJ_VALID(p)) return 0;

  if (p->type == TYPE_PEN) mod=&c->curpen;
  else if (p->type == TYPE_BRUSH) mod=&c->curbrush;
  else if (p->type == TYPE_FONT) mod=&c->curfont;
  else return 0;
  
  HGDIOBJ__ *op=*mod;
  if (!op) op=(HGDIOBJ__ *)(INT_PTR)p->type;
  if (op != p)
  {
    *mod=p;
  }
  return op;
}


static void swell_DirtyContext(HDC__ *out, int x1, int y1, int x2, int y2)
{
  if (x2<x1) { int t=x1; x1=x2; x2=t; } 
  if (y2<y1) { int t=y1; y1=y2; y2=t; } 
  x1+=out->surface_offs.x;
  x2+=out->surface_offs.x;
  y1+=out->surface_offs.y;
  y2+=out->surface_offs.y;

  if (!out->dirty_rect_valid)
  { 
    out->dirty_rect_valid=true;
    out->dirty_rect.left = x1;
    out->dirty_rect.top = y1;
    out->dirty_rect.right = x2;
    out->dirty_rect.bottom = y2;
  } 
  else
  {
    if (out->dirty_rect.left > x1) out->dirty_rect.left=x1;
    if (out->dirty_rect.top > y1) out->dirty_rect.top = y1;
    if (out->dirty_rect.right < x2) out->dirty_rect.right = x2;
    if (out->dirty_rect.bottom < y2) out->dirty_rect.bottom = y2;
  }
}


void SWELL_FillRect(HDC ctx, const RECT *r, HBRUSH br)
{
  HDC__ *c=(HDC__ *)ctx;
  HGDIOBJ__ *b=(HGDIOBJ__ *) br;
  if (!HDC_VALID(c) || !HGDIOBJ_VALID(b,TYPE_BRUSH)) return;
  if (!c->surface) return;

  if (b->wid<0) return;
  LICE_FillRect(c->surface,
      r->left+c->surface_offs.x,
      r->top+c->surface_offs.y,
      r->right-r->left,r->bottom-r->top,b->color,b->alpha,LICE_BLIT_MODE_COPY);
  swell_DirtyContext(ctx,r->left,r->top,r->right,r->bottom);
}

void RoundRect(HDC ctx, int x, int y, int x2, int y2, int xrnd, int yrnd)
{
	xrnd/=3;
	yrnd/=3;
	POINT pts[10]={ // todo: curves between edges
		{x,y+yrnd},
		{x+xrnd,y},
		{x2-xrnd,y},
		{x2,y+yrnd},
		{x2,y2-yrnd},
		{x2-xrnd,y2},
		{x+xrnd,y2},
		{x,y2-yrnd},		
    {x,y+yrnd},
		{x+xrnd,y},
};
	
	WDL_GDP_Polygon(ctx,pts,sizeof(pts)/sizeof(pts[0]));
}

void Ellipse(HDC ctx, int l, int t, int r, int b)
{
  HDC__ *c=(HDC__ *)ctx;
  if (!HDC_VALID(c) || !c->surface) return;
  
  swell_DirtyContext(ctx,l,t,r,b);
  
  l += c->surface_offs.x;
  t += c->surface_offs.y;
  r += c->surface_offs.x;
  b += c->surface_offs.y;

  int rad = wdl_min(r-l, b-t)/2; // todo: actual ellipse, for now just circles

  bool wantPen = HGDIOBJ_VALID(c->curpen,TYPE_PEN) && c->curpen->wid >= 0;
  if (HGDIOBJ_VALID(c->curbrush,TYPE_BRUSH) && c->curbrush->wid >= 0)
  {
    int use_rad = rad;
    if (use_rad > 0) LICE_FillCircle(c->surface,l+use_rad,t+use_rad,use_rad,c->curbrush->color,c->curbrush->alpha,LICE_BLIT_MODE_COPY,!wantPen);
  }
  if (wantPen)
  {
    LICE_Circle(c->surface,l+rad,t+rad,rad,c->curpen->color,c->curpen->alpha,LICE_BLIT_MODE_COPY,true);
  }
}

void Rectangle(HDC ctx, int l, int t, int r, int b)
{
  HDC__ *c=(HDC__ *)ctx;
  if (!HDC_VALID(c) || !c->surface) return;
  
  //CGRect rect=CGRectMake(l,t,r-l,b-t);

  swell_DirtyContext(ctx,l,t,r,b);
  
  l += c->surface_offs.x;
  t += c->surface_offs.y;
  r += c->surface_offs.x;
  b += c->surface_offs.y;

  if (HGDIOBJ_VALID(c->curbrush,TYPE_BRUSH) && c->curbrush->wid >= 0)
  {
    LICE_FillRect(c->surface,l,t,r-l,b-t,c->curbrush->color,c->curbrush->alpha,LICE_BLIT_MODE_COPY);
  }
  if (HGDIOBJ_VALID(c->curpen,TYPE_PEN) && c->curpen->wid >= 0)
  {
    if (r>l+1 && b>t+1)
      LICE_DrawRect(c->surface,l,t,r-l-1,b-t-1,c->curpen->color,c->curpen->alpha,LICE_BLIT_MODE_COPY);
  }
}

void Polygon(HDC ctx, POINT *pts, int npts)
{
  HDC__ *c=(HDC__ *)ctx;
  if (!HDC_VALID(c) || !c->surface) return;
  const bool fill=HGDIOBJ_VALID(c->curbrush,TYPE_BRUSH)&&c->curbrush->wid>=0;
  const bool outline = HGDIOBJ_VALID(c->curpen,TYPE_PEN)&&c->curpen->wid>=0;
  if ((!fill && !outline) || npts<2 || !pts) return;

  const int dx=c->surface_offs.x;
  const int dy=c->surface_offs.y;

  int minx=c->surface->getWidth()+1, maxx=0;
  int miny=c->surface->getHeight()+1, maxy=0;

  if (fill)
  {
    int _tmp[256], *tmp=_tmp;
    if (npts>128)
    {
      tmp = (int *)malloc(npts*2*sizeof(int));
    }
    if (tmp)
    {
      for (int x = 0; x < npts; x ++)
      {
        const int xp = pts[x].x, yp = pts[x].y;
        if (xp < minx) minx=xp;
        if (xp > maxx) maxx=xp;
        if (yp < miny) miny=yp;
        if (yp > maxy) maxy=yp;
        tmp[x] = xp + dx;
        tmp[x+npts] = yp + dy;
      }
      LICE_FillConvexPolygon(c->surface,tmp, tmp+npts,npts,
        c->curbrush->color,c->curbrush->alpha,LICE_BLIT_MODE_COPY);
    }
    if (tmp != _tmp) free(tmp);
  }
  if (outline)
  {
    int lx=0,ly=0,sx=0,sy=0;
    for (int x = 0; x < npts; x ++)
    {
      const int xp = pts[x].x, yp = pts[x].y;
      if (xp < minx) minx=xp;
      if (xp > maxx) maxx=xp;
      if (yp < miny) miny=yp;
      if (yp > maxy) maxy=yp;
      if (x)
        LICE_Line(c->surface,xp+dx,yp+dy,lx+dx,ly+dy,c->curpen->color,c->curpen->alpha,LICE_BLIT_MODE_COPY,true);
      else { sx=xp; sy=yp; }
      lx=xp;
      ly=yp;
    }
    LICE_Line(c->surface,sx+dx,sy+dy,lx+dx,ly+dy,c->curpen->color,c->curpen->alpha,LICE_BLIT_MODE_COPY,true);
  }

  if (maxx>minx && maxy>miny)
    swell_DirtyContext(ctx, minx,miny,maxx,maxy);
}

void MoveToEx(HDC ctx, int x, int y, POINT *op)
{
  HDC__ *c=(HDC__ *)ctx;
  if (!HDC_VALID(c)) return;
  if (op) 
  { 
    op->x = (int) (c->lastpos_x);
    op->y = (int) (c->lastpos_y);
  }
  c->lastpos_x=(float)x;
  c->lastpos_y=(float)y;
}

void PolyBezierTo(HDC ctx, POINT *pts, int np)
{
  HDC__ *c=(HDC__ *)ctx;
  if (!HDC_VALID(c)||!HGDIOBJ_VALID(c->curpen,TYPE_PEN)||c->curpen->wid<0||np<3) return;
  
  int x; 
  float xp=c->lastpos_x,yp=c->lastpos_y;
  for (x = 0; x < np-2; x += 3)
  {
      // todo
      xp=(float)pts[x+2].x;
      yp=(float)pts[x+2].y;    
  }
  c->lastpos_x=(float)xp;
  c->lastpos_y=(float)yp;
}


void SWELL_LineTo(HDC ctx, int x, int y)
{
  HDC__ *c=(HDC__ *)ctx;
  if (!HDC_VALID(c)||!HGDIOBJ_VALID(c->curpen,TYPE_PEN)||c->curpen->wid<0) return;

  float fx=(float)x,fy=(float)y;

  int dx=c->surface_offs.x;
  int dy=c->surface_offs.y;
  int lx = (int)c->lastpos_x, ly = (int) c->lastpos_y;
  if (c->surface) 
    LICE_Line(c->surface,x+dx,y+dy,lx+dx,ly+dy,c->curpen->color,c->curpen->alpha,LICE_BLIT_MODE_COPY,false);
  
  c->lastpos_x=fx;
  c->lastpos_y=fy;

  if (lx<x) { int a=x; x=lx; lx=a; }
  if (ly<y) { int a=y; y=ly; ly=a; }

  swell_DirtyContext(ctx, x-1,y-1,lx+1,ly+1);
 
}

void PolyPolyline(HDC ctx, POINT *pts, DWORD *cnts, int nseg)
{
  HDC__ *c=(HDC__ *)ctx;
  if (!HDC_VALID(c)||!HGDIOBJ_VALID(c->curpen,TYPE_PEN)||c->curpen->wid<0||nseg<1) return;

  while (nseg-->0)
  {
    DWORD cnt=*cnts++;
    if (!cnt) continue;
    if (!--cnt) { pts++; continue; }
    
 //   CGContextMoveToPoint(c->ctx,(float)pts->x,(float)pts->y);
    pts++;
    
    while (cnt--)
    {
//      CGContextAddLineToPoint(c->ctx,(float)pts->x,(float)pts->y);
      pts++;
    }
  }
//  CGContextStrokePath(c->ctx);
}
void *SWELL_GetCtxGC(HDC ctx)
{
  HDC__ *ct=(HDC__ *)ctx;
  if (!HDC_VALID(ct)) return 0;
  return NULL; 
}


void SWELL_SetPixel(HDC ctx, int x, int y, int c)
{
  HDC__ *ct=(HDC__ *)ctx;
  if (!HDC_VALID(ct) || !ct->surface) return;
  LICE_PutPixel(ct->surface,x+ct->surface_offs.x, y+ct->surface_offs.y, LICE_RGBA_FROMNATIVE(c,255),1.0f,LICE_BLIT_MODE_COPY);
  swell_DirtyContext(ct,x,y,x+1,y+1);
}

#ifdef SWELL_FREETYPE
HFONT SWELL_GetDefaultFont()
{
  static HFONT def;
  if (!def)
  {
    def = CreateFont(g_swell_ctheme.default_font_size,0,0,0,FW_NORMAL,0,0,0,0,0,0,0,0,g_swell_deffont_face);
  }
  return def;
}
#endif

BOOL GetTextMetrics(HDC ctx, TEXTMETRIC *tm)
{
  HDC__ *ct=(HDC__ *)ctx;
  if (tm) // give some sane defaults
  {
    tm->tmInternalLeading=0;
    tm->tmAscent=8;
    tm->tmDescent=0;
    tm->tmHeight=8;
    tm->tmAveCharWidth = 8;
  }
  if (!HDC_VALID(ct)||!tm) return 0;

#ifdef SWELL_FREETYPE
  HGDIOBJ__  *font  = HGDIOBJ_VALID(ct->curfont,TYPE_FONT) ? ct->curfont : SWELL_GetDefaultFont();
  if (font && font->typedata)
  {
    FT_Face face=(FT_Face) font->typedata;
    tm->tmAscent = face->size->metrics.ascender/64;
    tm->tmDescent = -face->size->metrics.descender/64;
    tm->tmHeight = (face->size->metrics.ascender - face->size->metrics.descender)/64;
    tm->tmAveCharWidth = face->size->metrics.height / 112;

    tm->tmInternalLeading = (face->size->metrics.ascender + face->size->metrics.descender - face->size->metrics.height)/64;
    if (tm->tmInternalLeading<0) tm->tmInternalLeading=0;
  }
#endif
  
  return 1;
}


int DrawText(HDC ctx, const char *buf, int buflen, RECT *r, int align)
{
  HDC__ *ct=(HDC__ *)ctx;
  if (!r) return 0;

  int lineh = 8;
  int charw = 8;

  HGDIOBJ__  *font  = NULL;
#ifdef SWELL_FREETYPE
  int ascent=0, descent=0;
  font  = HDC_VALID(ct) && HGDIOBJ_VALID(ct->curfont,TYPE_FONT) ? ct->curfont : SWELL_GetDefaultFont();
  FT_Face face = NULL;
  if (font && font->typedata) 
  {
    face=(FT_Face)font->typedata;
    lineh = face->size->metrics.height/64;
    ascent = face->size->metrics.ascender/64;
    descent = face->size->metrics.descender/64;
    charw = face->size->metrics.height / 112;
  }
#endif

  if (align&DT_CALCRECT)
  {
    if (!font && (align&DT_SINGLELINE))
    {
      r->right = r->left + ( buflen < 0 ? strlen(buf) : buflen ) * charw;
      int h = r->right ? (ascent-descent) :0;
      r->bottom = r->top+h;
      return h;
    }

    int xpos=0;
    int ypos=0;

    r->bottom=r->top;
    bool in_prefix=false;
    while (buflen && *buf) // if buflen<0, go forever
    {
      unsigned short c=0;
      int charlen = utf8char(buf,&c);
      buf+=charlen;
      if (buflen > 0) 
      {
        buflen -= charlen;
        if (buflen < 0) buflen=0;
      }
      if (!c) break;

      if (c=='&' && !in_prefix && !(align&DT_NOPREFIX)) 
      {
        in_prefix = true;
        continue;
      }
      in_prefix=false;
 
      if (c == '\n') { ypos += lineh; xpos=0; }
      else if (c != '\r')
      {
        if (font)
        {
#ifdef SWELL_FREETYPE
          if (c != '\t' && !FT_Load_Char(face, c, FT_LOAD_DEFAULT) && face->glyph)
          {
            // measure character
            FT_GlyphSlot g = face->glyph;
            int rext = xpos;
            if ((align&(DT_BOTTOM|DT_VCENTER|DT_CENTER|DT_RIGHT))!=DT_RIGHT)
              rext += (g->metrics.width + g->metrics.horiBearingX)/64;

            xpos += g->metrics.horiAdvance/64;
            if (rext<xpos) rext=xpos;
            if (r->left+rext > r->right) r->right = r->left+rext;

            int bext = r->top + ypos + ascent - descent;
            if (bext > r->bottom) r->bottom = bext;
            continue;
          }
#endif
        }
        xpos += c=='\t' ? charw*5 : charw;
        int bext = r->top + ypos + ascent - descent;
        if (bext > r->bottom) r->bottom = bext;
        if (r->left+xpos>r->right) r->right=r->left+xpos;
      }
    }
    return r->bottom-r->top;
  }
  if (!HDC_VALID(ct)) return 0;

  RECT use_r = *r;
  if ((align & DT_VCENTER) && use_r.top > use_r.bottom)
  {
    use_r.top = r->bottom;
    use_r.bottom = r->top;
  }

  use_r.left += ct->surface_offs.x;
  use_r.right += ct->surface_offs.x;
  use_r.top += ct->surface_offs.y;
  use_r.bottom += ct->surface_offs.y;

  int xpos = use_r.left;
  int ypos = use_r.top;
  if (align&(DT_CENTER|DT_VCENTER|DT_RIGHT|DT_BOTTOM))
  {
    RECT tr={0,};
    DrawText(ctx,buf,buflen,&tr,align|DT_CALCRECT);

    if (align&DT_CENTER)
      xpos -= ((tr.right-tr.left) - (use_r.right-use_r.left))/2;
    else if (align&DT_RIGHT)
      xpos = use_r.right - (tr.right-tr.left);

    if (align&DT_VCENTER)
      ypos -= ((tr.bottom-tr.top) - (use_r.bottom-use_r.top))/2;
    else if (align&DT_BOTTOM)
      ypos = use_r.bottom - (tr.bottom-tr.top);
  }
  LICE_IBitmap *surface = ct->surface;
  int fgcol = ct->cur_text_color_int;
  int bgcol = ct->curbkcol;
  int bgmode = ct->curbkmode;


  int clip_x1=wdl_max(use_r.left,0), clip_y1 = wdl_max(use_r.top,0);
  int clip_w=0, clip_h=0;
  if (surface)
  {
    clip_w = wdl_min(use_r.right,surface->getWidth())-clip_x1;
    clip_h = wdl_min(use_r.bottom,surface->getHeight())-clip_y1;
    if (clip_w<0)clip_w=0;
    if (clip_h<0)clip_h=0;
  }

  LICE_SubBitmap clipbm(surface,clip_x1,clip_y1,clip_w,clip_h);
  if (surface && !(align&DT_NOCLIP)) { surface = &clipbm; xpos-=clip_x1; ypos-=clip_y1; }

  int left_xpos = xpos,start_ypos = ypos, max_ypos=ypos,max_xpos=0;
  bool in_prefix=false;

  while (buflen && *buf)
  {
    unsigned short c=0;
    int  charlen = utf8char(buf,&c);
    if (buflen>0)
    {
      buflen -= charlen;
      if (buflen<0) buflen=0;
    }
    buf+=charlen;

    bool doUl=in_prefix;

    if (c=='&' && !in_prefix && !(align&DT_NOPREFIX)) 
    {
      in_prefix = true;
      continue;
    }
    in_prefix=false;

    if (c=='\n' && !(align&DT_SINGLELINE)) { xpos=left_xpos; ypos+=lineh; }
    else if (c=='\r')  {} 
    else 
    {
      bool needr=true;
      if (font)
      {
#ifdef SWELL_FREETYPE
        if (c != '\t' && !FT_Load_Char(face, c, FT_LOAD_RENDER) && face->glyph)
        {
          FT_GlyphSlot g = face->glyph;
          const int ha = g->metrics.horiAdvance/64;
          if (bgmode==OPAQUE) LICE_FillRect(surface,xpos,ypos,ha,(align & DT_SINGLELINE) ? (ascent-descent) : lineh,bgcol,1.0f,LICE_BLIT_MODE_COPY);
  
          if (g->bitmap.pixel_mode == FT_PIXEL_MODE_MONO)
          {
            LICE_DrawMonoGlyph(surface,xpos+g->bitmap_left,ypos+ascent-g->bitmap_top,fgcol,(const unsigned char*)g->bitmap.buffer,g->bitmap.width,g->bitmap.pitch,g->bitmap.rows,1.0f,LICE_BLIT_MODE_COPY);
          }
          else  // FT_PIXEL_MODE_GRAY (hopefully!)
          {
            LICE_DrawGlyphEx(surface,xpos+g->bitmap_left,ypos+ascent-g->bitmap_top,fgcol,(LICE_pixel_chan *)g->bitmap.buffer,g->bitmap.width,g->bitmap.pitch,g->bitmap.rows,1.0f,LICE_BLIT_MODE_COPY);
          }
          if (doUl) 
          {
            int xw = g->metrics.width/64;
            if (xw > 1) xw--;
            LICE_Line(surface,xpos + g->metrics.horiBearingX/64,ypos+ascent+1,
                              xpos + xw,ypos+ascent+1,fgcol,1.0f,LICE_BLIT_MODE_COPY,false);
          }
  
          int rext = xpos + (g->metrics.width + g->metrics.horiBearingX)/64;
          if (rext<=xpos) rext=xpos + ha;
          if (rext > max_xpos) max_xpos=rext;
          xpos += ha;
          const int bext = ypos + ascent-descent;
          if (max_ypos < bext) max_ypos=bext;
          needr=false;
        }
#endif
      }

      if (needr)
      {
        if (c=='\t') 
        {
          if (bgmode==OPAQUE) LICE_FillRect(surface,xpos,ypos,charw*5,(align & DT_SINGLELINE) ? (ascent-descent) : lineh,bgcol,1.0f,LICE_BLIT_MODE_COPY);
          xpos+=charw*5;
         
          const int bext = ypos+ascent-descent;
          if (max_ypos < bext) max_ypos=bext;
        }
        else 
        {
          if (bgmode==OPAQUE) LICE_FillRect(surface,xpos,ypos,charw,(align & DT_SINGLELINE) ? (ascent-descent) : lineh,bgcol,1.0f,LICE_BLIT_MODE_COPY);
          LICE_DrawChar(surface,xpos,ypos,c,fgcol,1.0f,LICE_BLIT_MODE_COPY);
          if (doUl) LICE_Line(surface,xpos,ypos+(ascent-descent)+1,xpos+charw,ypos+(ascent-descent)+1,fgcol,1.0f,LICE_BLIT_MODE_COPY,false);
  
          const int bext=ypos+ascent-descent+(doUl ? 2:1);
          if (max_ypos < bext) max_ypos=bext;
          xpos+=charw;
        }
      }
    }
    if(xpos>max_xpos)max_xpos=xpos;
  }
  if (surface==&clipbm)
    swell_DirtyContext(ct,clip_x1+left_xpos,clip_y1+start_ypos,clip_x1+max_xpos,clip_y1+max_ypos);
  else
    swell_DirtyContext(ct,left_xpos,start_ypos,max_xpos,max_ypos);
  return max_ypos - start_ypos;
}


int GetTextColor(HDC ctx)
{
  HDC__ *ct=(HDC__ *)ctx;
  if (!HDC_VALID(ct)) return -1;
  return ct->cur_text_color_int;
}
void SetTextColor(HDC ctx, int col)
{
  HDC__ *ct=(HDC__ *)ctx;
  if (!HDC_VALID(ct)) return;
  ct->cur_text_color_int = LICE_RGBA_FROMNATIVE(col,255);
  
}


////////// todo: some sort of HICON emul

HICON LoadNamedImage(const char *name, bool alphaFromMask)
{
#ifdef SWELL_TARGET_GDK
  char buf[1024];
  GdkPixbuf *pb = NULL;
  if (strstr(name,"/")) 
  {
    lstrcpyn_safe(buf,name,sizeof(buf));
    pb = gdk_pixbuf_new_from_file(buf,NULL);
  }
  else
  {
    GetModuleFileName(NULL,buf,sizeof(buf));
    WDL_remove_filepart(buf);
    snprintf_append(buf,sizeof(buf),"/Resources/%s.ico",name);
    pb = gdk_pixbuf_new_from_file(buf,NULL);
    if (!pb)
    {
      WDL_remove_fileext(buf);
      lstrcatn(buf,".bmp",sizeof(buf));
      pb = gdk_pixbuf_new_from_file(buf,NULL);
    }
  }
  if (pb)
  {
    HGDIOBJ__ *ret=NULL;
    const int w = gdk_pixbuf_get_width(pb), h = gdk_pixbuf_get_height(pb);
    const int bpc = gdk_pixbuf_get_bits_per_sample(pb), chan = gdk_pixbuf_get_n_channels(pb);
    const int alpha = gdk_pixbuf_get_has_alpha(pb);
    const guchar *rd = gdk_pixbuf_get_pixels(pb);
    if (bpc == 8 && (chan == 4 || chan == 3) && w > 0 && h>0 && rd)
    {
      LICE_MemBitmap *bm = new LICE_MemBitmap(w,h);
      LICE_pixel_chan *wr = (LICE_pixel_chan*)bm->getBits();
      if (wr)
      {
        const int rdadv = gdk_pixbuf_get_rowstride(pb);
        const int wradv = bm->getRowSpan()*4;
        const unsigned char alphamod = !alpha ? 255 : 0;
        int y;
        for (y=0;y <h; y++)
        {
          int x;
          for (x=0;x<w;x++)
          {
            wr[LICE_PIXEL_R] = rd[0];
            wr[LICE_PIXEL_G] = rd[1];
            wr[LICE_PIXEL_B] = rd[2];
            wr[LICE_PIXEL_A] = chan==4 ? (rd[LICE_PIXEL_A] | alphamod) : 255;
            wr+=4; rd+=chan;
          }
          wr+=wradv-w*4;
          rd+=rdadv-w*chan;
        }
        ret=GDP_OBJECT_NEW();
        ret->type=TYPE_BITMAP;
        ret->alpha = 1.0f;
        ret->wid=1;
        ret->typedata = bm;
      }
      else delete wr;
    }
    g_object_unref(pb);
    return ret;
  }
  

#endif
  return 0; // todo
}

void DrawImageInRect(HDC hdcOut, HICON in, const RECT *r)
{
  HDC__ *out = (HDC__ *)hdcOut;
  if (!HDC_VALID(out) || !HGDIOBJ_VALID(in,TYPE_BITMAP) || !out->surface || !in->typedata) return;

  const int x = r->left, y=r->top, w=r->right-x, h=r->bottom-y;
  LICE_IBitmap *src=(LICE_IBitmap *)in->typedata;
  LICE_ScaledBlit(out->surface,src,
            x+out->surface_offs.x,y+out->surface_offs.y,w,h,
            0,0, src->getWidth(),src->getHeight(),
            1.0f,LICE_BLIT_MODE_COPY|LICE_BLIT_USE_ALPHA|LICE_BLIT_FILTER_BILINEAR);
  swell_DirtyContext(out,x,y,x+w,y+h);
}


BOOL GetObject(HICON icon, int bmsz, void *_bm)
{
  memset(_bm,0,bmsz);
  if (bmsz < 2*(int)sizeof(LONG)) return false;
  BITMAP *bm=(BITMAP *)_bm;
  HGDIOBJ__ *i = (HGDIOBJ__ *)icon;
  if (!HGDIOBJ_VALID(i,TYPE_BITMAP) || !i->typedata) return false;

  bm->bmWidth = ((LICE_IBitmap *)i->typedata)->getWidth();
  bm->bmHeight = ((LICE_IBitmap *)i->typedata)->getHeight();

  if (bmsz >= (int)sizeof(BITMAP))
  {
    bm->bmWidthBytes = ((LICE_IBitmap *)i->typedata)->getRowSpan()*4;
    bm->bmPlanes = 1;
    bm->bmBitsPixel = 32;
    bm->bmBits = ((LICE_IBitmap *)i->typedata)->getBits();
  }

  return true;
}


////////////////////////////////////


void BitBltAlphaFromMem(HDC hdcOut, int x, int y, int w, int h, void *inbufptr, int inbuf_span, int inbuf_h, int xin, int yin, int mode, bool useAlphaChannel, float opacity)
{
 // todo: use LICE_WrapperBitmap?
}

void BitBltAlpha(HDC hdcOut, int x, int y, int w, int h, HDC hdcIn, int xin, int yin, int mode, bool useAlphaChannel, float opacity)
{
  HDC__ *in = (HDC__ *)hdcIn;
  HDC__ *out = (HDC__ *)hdcOut;
  if (!HDC_VALID(out) || !HDC_VALID(in)) return;
  if (!in->surface || !out->surface) return;
  LICE_Blit(out->surface,in->surface,
            x+out->surface_offs.x,y+out->surface_offs.y,
            xin+in->surface_offs.x,yin+in->surface_offs.y,w,h,
            opacity,LICE_BLIT_MODE_COPY|(useAlphaChannel?LICE_BLIT_USE_ALPHA:0));
  swell_DirtyContext(out,x,y,x+w,y+h);
}

void BitBlt(HDC hdcOut, int x, int y, int w, int h, HDC hdcIn, int xin, int yin, int mode)
{
  HDC__ *in = (HDC__ *)hdcIn;
  HDC__ *out = (HDC__ *)hdcOut;
  if (!HDC_VALID(out) || !HDC_VALID(in)) return;
  if (!in->surface || !out->surface) return;
  LICE_Blit(out->surface,in->surface,
            x+out->surface_offs.x,y+out->surface_offs.y,
            xin+in->surface_offs.x,yin+in->surface_offs.y,w,h,
            1.0f,LICE_BLIT_MODE_COPY | (mode == (int)SRCCOPY_USEALPHACHAN ? LICE_BLIT_USE_ALPHA : 0));
  swell_DirtyContext(out,x,y,x+w,y+h);
}

void StretchBltFromMem(HDC hdcOut, int x, int y, int w, int h, const void *bits, int srcw, int srch, int srcspan)
{
  HDC__ *out = (HDC__ *)hdcOut;
  if (!HDC_VALID(out) || !bits) return;
  if (!out->surface) return;

  LICE_WrapperBitmap srcbm((LICE_pixel*)bits,srcw,srch,srcspan,false);
  LICE_ScaledBlit(out->surface,&srcbm,
            x+out->surface_offs.x,y+out->surface_offs.y,w,h,
            0,0,srcw,srch,
            1.0f,LICE_BLIT_MODE_COPY);
  swell_DirtyContext(out,x,y,x+w,y+h);
}

void StretchBlt(HDC hdcOut, int x, int y, int w, int h, HDC hdcIn, int xin, int yin, int srcw, int srch, int mode)
{
  HDC__ *in = (HDC__ *)hdcIn;
  HDC__ *out = (HDC__ *)hdcOut;
  if (!HDC_VALID(out) || !HDC_VALID(in)) return;
  if (!in->surface || !out->surface) return;
  LICE_ScaledBlit(out->surface,in->surface,
            x+out->surface_offs.x,y+out->surface_offs.y,w,h,
            xin+in->surface_offs.x,yin+in->surface_offs.y,srcw,srch,
            1.0f,LICE_BLIT_MODE_COPY | (mode == (int)SRCCOPY_USEALPHACHAN ? LICE_BLIT_USE_ALPHA : 0));
  swell_DirtyContext(out,x,y,x+w,y+h);
}

void SWELL_FillDialogBackground(HDC hdc, const RECT *r, int level)
{
  HBRUSH br = CreateSolidBrush(g_swell_ctheme._3dface);
  FillRect(hdc,r,br);
  DeleteObject(br);
}

HGDIOBJ SWELL_CloneGDIObject(HGDIOBJ a)
{ 
  if (HGDIOBJ_VALID(a))
  {
    a->additional_refcnt++;
    return a;
  }
  return NULL;
}

void SWELL_PushClipRegion(HDC ctx)
{
//  HDC__ *ct=(HDC__ *)ctx;
//  if (ct && ct->ctx) CGContextSaveGState(ct->ctx);
}

void SWELL_SetClipRegion(HDC ctx, const RECT *r)
{
//  HDC__ *ct=(HDC__ *)ctx;
//  if (ct && ct->ctx) CGContextClipToRect(ct->ctx,CGRectMake(r->left,r->top,r->right-r->left,r->bottom-r->top));

}

void SWELL_PopClipRegion(HDC ctx)
{
//  HDC__ *ct=(HDC__ *)ctx;
//  if (ct && ct->ctx) CGContextRestoreGState(ct->ctx);
}

void *SWELL_GetCtxFrameBuffer(HDC ctx)
{
  HDC__ *ct=(HDC__ *)ctx;
  if (HDC_VALID(ct)&&ct->surface) return ct->surface->getBits();
  return 0;
}



struct swell_gdpLocalContext
{
  HDC__ ctx;
  RECT clipr;
};



HDC SWELL_internalGetWindowDC(HWND h, bool calcsize_on_first)
{
  if (!h) return NULL;

  int xoffs=0,yoffs=0;
  int wndw = h->m_position.right-h->m_position.left;
  int wndh = h->m_position.bottom-h->m_position.top;

  bool vis=true;
  HWND starth = h;
  int ltrim=0, ttrim=0, rtrim=0, btrim=0;
  for (;;)
  {
    if ((calcsize_on_first || h!=starth)  && h->m_wndproc)
    {
      RECT r;
      GetWindowContentViewRect(h,&r);
      NCCALCSIZE_PARAMS p={{r,},};
      h->m_wndproc(h,WM_NCCALCSIZE,FALSE,(LPARAM)&p);
      RECT r2=r;
      r=p.rgrc[0];

      // todo: handle left edges and adjust postions/etc accordingly
      if (h==starth)  // if initial window, adjust rects to client area (this implies calcsize_on_first being false)
      {
        wndw=r.right-r.left;
        wndh=r.bottom-r.top;
      }
      yoffs += r.top-r2.top;
      xoffs += r.left-r2.left;
    } 
    if (!h->m_visible) vis = false;

    if (h->m_backingstore || !h->m_parent) break; // found our target window

    xoffs += h->m_position.left;
    yoffs += h->m_position.top;

    ltrim = wdl_max(ltrim, -xoffs);
    ttrim = wdl_max(ttrim, -yoffs);
    rtrim = wdl_max(rtrim, xoffs+wndw - h->m_position.right);
    btrim = wdl_max(btrim, yoffs+wndh - h->m_position.bottom);

    h = h->m_parent;
  }

  swell_gdpLocalContext *p = (swell_gdpLocalContext*)SWELL_GDP_CTX_NEW();

  p->clipr.left=p->clipr.right=xoffs + ltrim;
  p->clipr.top=p->clipr.bottom=yoffs + ttrim;

  if (h->m_backingstore && vis)
  {
    p->ctx.surface=new LICE_SubBitmap(h->m_backingstore,
        xoffs+ltrim,yoffs+ttrim,
        wndw-ltrim-rtrim,wndh-ttrim-btrim);
    p->clipr.right += p->ctx.surface->getWidth();
    p->clipr.bottom += p->ctx.surface->getHeight();
  }
  if (xoffs<0) p->ctx.surface_offs.x = xoffs;
  if (yoffs<0) p->ctx.surface_offs.y = yoffs;
  p->ctx.surface_offs.x -= ltrim;
  p->ctx.surface_offs.y -= ttrim;

  p->ctx.curfont = starth->m_font;
  // todo: other GDI defaults?

  return (HDC)p;
}
HDC GetWindowDC(HWND h)
{
  return SWELL_internalGetWindowDC(h,0);
}

HDC GetDC(HWND h)
{
  return SWELL_internalGetWindowDC(h,1);
}

void ReleaseDC(HWND h, HDC hdc)
{
  if (!h || !HDC_VALID(hdc)) return;
  swell_gdpLocalContext *p = (swell_gdpLocalContext*)hdc;


  if (!h->m_paintctx)
  {
    // handle blitting?
    HWND par = h;
    while (par && !par->m_backingstore) par=par->m_parent;
    if (par) 
    {
      if (p->ctx.dirty_rect_valid)
      {
        RECT r = p->clipr, dr=p->ctx.dirty_rect; 
        dr.left += r.left;
        dr.top += r.top;
        dr.right += r.left;
        dr.bottom += r.top;
#if 1
        if (dr.left > r.left) r.left=dr.left;
        if (dr.top > r.top) r.top=dr.top;
        if (dr.right < r.right) r.right=dr.right;
        if (dr.bottom < r.bottom) r.bottom=dr.bottom;
#endif

        if (r.top<r.bottom && r.left<r.right) swell_oswindow_updatetoscreen(par,&r);
      }
    }
  }
  delete p->ctx.surface;
  SWELL_GDP_CTX_DELETE(hdc);
}



HDC BeginPaint(HWND hwnd, PAINTSTRUCT *ps)
{
  if (!ps) return 0;
  memset(ps,0,sizeof(PAINTSTRUCT));
  if (!hwnd) return 0;
  if (!hwnd->m_paintctx) return NULL;

  swell_gdpLocalContext *ctx = (swell_gdpLocalContext *)hwnd->m_paintctx;
  ps->rcPaint = ctx->clipr;
  ps->hdc = &ctx->ctx;
  return &ctx->ctx;
}


// paint hwnd into bmout, where bmout points at bmout_xpos,bmout_ypos in window coordinates
void SWELL_internalLICEpaint(HWND hwnd, LICE_IBitmap *bmout, int bmout_xpos, int bmout_ypos, bool forceref)
{
  // todo: check to see if any children completely intersect clip region, if so then we don't need to render ourselves
  // todo: opaque flag for windows? (to disable above behavior)
  // todo: implement/use per-window backing store.
  //   we would want to only really use them for top level windows, and on those, only update windows (and children of windows) who had backingstore invalidated.
  if (hwnd->m_invalidated) forceref=true;

  if (forceref || hwnd->m_child_invalidated)
  {
    swell_gdpLocalContext ctx;
    memset(&ctx,0,sizeof(ctx));
    ctx.ctx.surface = bmout;
    ctx.ctx.surface_offs.x = -bmout_xpos;
    ctx.ctx.surface_offs.y = -bmout_ypos;
    ctx.clipr.left = bmout_xpos;
    ctx.clipr.top = bmout_ypos;
    ctx.clipr.right = ctx.clipr.left + bmout->getWidth();
    ctx.clipr.bottom = ctx.clipr.top + bmout->getHeight();

    void *oldpaintctx = hwnd->m_paintctx;
    if (forceref) hwnd->m_paintctx = &ctx;

    LICE_SubBitmap tmpsub(NULL,0,0,0,0);
    if (hwnd->m_wndproc) // this happens after m_paintctx is set -- that way GetWindowDC()/GetDC()/ReleaseDC() know to not actually update the screen
    {
      RECT r2;
      GetWindowContentViewRect(hwnd,&r2);
      WinOffsetRect(&r2,-r2.left,-r2.top);

      NCCALCSIZE_PARAMS p;
      memset(&p,0,sizeof(p));
      p.rgrc[0]=r2;
      hwnd->m_wndproc(hwnd,WM_NCCALCSIZE,FALSE,(LPARAM)&p);
      RECT r = p.rgrc[0];
      if (forceref) hwnd->m_wndproc(hwnd,WM_NCPAINT,(WPARAM)1,0);

      // dx,dy is offset from the window's nonclient root
      const int dx = r.left-r2.left,dy=r.top-r2.top;

      WinOffsetRect(&ctx.clipr,-dx,-dy);
      ctx.ctx.surface_offs.x += dx;
      ctx.ctx.surface_offs.y += dy;
      bmout_xpos -= dx;
      bmout_ypos -= dy;

      // make sure we can't overwrite the nonclient area
      const int xo = wdl_max(-bmout_xpos,0), yo = wdl_max(-bmout_ypos,0);
      if (xo || yo)
      {
        tmpsub.m_parent = ctx.ctx.surface;
        tmpsub.m_x = xo;
        tmpsub.m_y = yo;
        tmpsub.m_w = ctx.ctx.surface->getWidth()-xo;
        tmpsub.m_h = ctx.ctx.surface->getHeight()-yo;
        if (tmpsub.m_w<0) tmpsub.m_w=0;
        if (tmpsub.m_h<0) tmpsub.m_h=0;
        ctx.ctx.surface = &tmpsub;
        ctx.ctx.surface_offs.x -= xo;
        ctx.ctx.surface_offs.y -= yo;
      }

      // constrain clip rect to right/bottom extents
      if (ctx.clipr.left < 0) ctx.clipr.left=0;
      if (ctx.clipr.top < 0) ctx.clipr.top=0;
      int newr = r.right-r.left;
      if (ctx.clipr.right > newr) ctx.clipr.right=newr;
      int newb = r.bottom-r.top;
      if (ctx.clipr.bottom > newb) ctx.clipr.bottom=newb;
    }

    // paint
    if (forceref)
    {
      if (hwnd->m_wndproc && ctx.clipr.right > ctx.clipr.left && ctx.clipr.bottom > ctx.clipr.top) 
      {
        ctx.ctx.curfont = hwnd->m_font;
        hwnd->m_wndproc(hwnd,WM_PAINT,(WPARAM)&ctx,0);
      }

       hwnd->m_paintctx = oldpaintctx;
   
      // it might be good to blit here on some OSes, rather than from the top level caller...
      hwnd->m_invalidated=false;
    }
  }

  bool okToClearChild=true;
  if (forceref || hwnd->m_child_invalidated)
  {
    HWND h = hwnd->m_children;
    while (h) 
    {
      if (h->m_visible && (forceref || h->m_invalidated||h->m_child_invalidated))
      {
        int width = h->m_position.right - h->m_position.left, 
            height = h->m_position.bottom - h->m_position.top; // max width possible for this window
        int xp = h->m_position.left - bmout_xpos, yp = h->m_position.top - bmout_ypos;

        if (okToClearChild && !forceref)
        {
          if (xp < 0 || xp+width > bmout->getWidth() ||
              yp < 0 || yp+height > bmout->getHeight()) okToClearChild = false; // extends outside of our region
        }
        // if xp/yp < 0, that means that the clip region starts inside the window, so we need to pass a positive render offset, and decrease the potential draw width
        // if xp/yp > 0, then the clip region starts before the window, so we use the subbitmap and pass 0 for the offset parm
        int subx = 0, suby=0;
        if (xp<0) width+=xp; 
        else { subx=xp; xp=0; }

        if (yp<0) height+=yp;
        else { suby=yp; yp=0; }

        LICE_SubBitmap subbm(bmout,subx,suby,width,height); // the right/bottom will automatically be clipped to the clip rect etc
        if (subbm.getWidth()>0 && subbm.getHeight()>0)
          SWELL_internalLICEpaint(h,&subbm,-xp,-yp,forceref);
      }
      h = h->m_next;
    }
  }
  if (okToClearChild) hwnd->m_child_invalidated=false;
}

HBITMAP CreateBitmap(int width, int height, int numplanes, int bitsperpixel, unsigned char* bits)
{
  if (width < 1 || height < 1 || numplanes != 1 || bitsperpixel != 32 || !bits) return NULL;
  LICE_MemBitmap *bm = new LICE_MemBitmap(width,height);
  if (!bm->getBits()) { delete bm; return NULL; }
  int y;
  LICE_pixel *wr = bm->getBits();
  for (y=0;y<height; y++)
  {
    memcpy(wr,bits,width*4);
    bits += width*4;
    wr += bm->getRowSpan();
  }
  
  

  HGDIOBJ__* icon=GDP_OBJECT_NEW();
  icon->type=TYPE_BITMAP;
  icon->wid=1;
  icon->typedata = (void*)bm;
  return icon;
}

HICON CreateIconIndirect(ICONINFO* iconinfo)
{
  if (!iconinfo || !iconinfo->fIcon) return 0;  
  HGDIOBJ__* i=iconinfo->hbmColor;
  if (!HGDIOBJ_VALID(i,TYPE_BITMAP) ) return 0;

  if (!i->typedata) return 0;

  LICE_MemBitmap *bm = new LICE_MemBitmap;
  LICE_Copy(bm,(LICE_IBitmap*)i->typedata);

  HGDIOBJ__* icon=GDP_OBJECT_NEW();
  icon->type=TYPE_BITMAP;
  icon->wid=1;
  icon->typedata = (void*)bm;

  return icon;
}

HIMAGELIST ImageList_CreateEx()
{
  return (HIMAGELIST)new WDL_PtrList<HGDIOBJ__>;
}

BOOL ImageList_Remove(HIMAGELIST list, int idx)
{
  WDL_PtrList<HGDIOBJ__>* imglist=(WDL_PtrList<HGDIOBJ__>*)list;
  if (imglist && idx < imglist->GetSize())
  {
    if (idx < 0) 
    {
      int x,n=imglist->GetSize();
      for (x=0;x<n;x++)
      {
        HGDIOBJ__ *a = imglist->Get(x);
        if (a) DeleteObject(a);
      }
      imglist->Empty();
    }
    else 
    {
      HGDIOBJ__ *a = imglist->Get(idx);
      imglist->Set(idx, NULL); 
      if (a) DeleteObject(a);
    }
    return TRUE;
  }
  
  return FALSE;
}

void ImageList_Destroy(HIMAGELIST list)
{
  if (!list) return;
  WDL_PtrList<HGDIOBJ__> *p=(WDL_PtrList<HGDIOBJ__>*)list;
  ImageList_Remove(list,-1);
  delete p;
}

int ImageList_ReplaceIcon(HIMAGELIST list, int offset, HICON image)
{
  if (!image || !list) return -1;
  WDL_PtrList<HGDIOBJ__> *l=(WDL_PtrList<HGDIOBJ__> *)list;

  HGDIOBJ__ *imgsrc = (HGDIOBJ__*)image;
  if (!HGDIOBJ_VALID(imgsrc,TYPE_BITMAP)) return -1;

  HGDIOBJ__* icon=GDP_OBJECT_NEW();
  LICE_MemBitmap *bm = new LICE_MemBitmap;
  LICE_Copy(bm,(LICE_IBitmap*)imgsrc->typedata);

  icon->type=TYPE_BITMAP;
  icon->alpha = 1.0f;
  icon->wid=1;
  icon->typedata = (void*)bm;

  image = (HICON) icon;

  if (offset<0||offset>=l->GetSize()) 
  {
    l->Add(image);
    offset=l->GetSize()-1;
  }
  else
  {
    HICON old=l->Get(offset);
    l->Set(offset,image);
    if (old) DeleteObject(old);
  }
  return offset;
}

int ImageList_Add(HIMAGELIST list, HBITMAP image, HBITMAP mask)
{
  if (!image || !list) return -1;
  WDL_PtrList<HGDIOBJ__> *l=(WDL_PtrList<HGDIOBJ__> *)list;
  
  HGDIOBJ__ *imgsrc = (HGDIOBJ__*)image;
  if (!HGDIOBJ_VALID(imgsrc,TYPE_BITMAP)) return -1;
  
  HGDIOBJ__* icon=GDP_OBJECT_NEW();
  LICE_MemBitmap *bm = new LICE_MemBitmap;
  LICE_Copy(bm,(LICE_IBitmap*)imgsrc->typedata);

  icon->type=TYPE_BITMAP;
  icon->wid=1;
  icon->typedata = (void*)bm;

  image = (HICON) icon;
  
  l->Add(image);
  return l->GetSize();
}

int AddFontResourceEx(LPCTSTR str, DWORD fl, void *pdv)
{
  if (str && *str)
  {
#ifdef SWELL_FONTCONFIG
    if (!s_fontconfig) s_fontconfig = FcInitLoadConfigAndFonts();
    return s_fontconfig && FcConfigAppFontAddFile(s_fontconfig,(const FcChar8 *)str) != FcFalse;
#else
    if (s_freetype_regfonts.FindSorted(str,sortByFilePart)>=0) return 0;
    s_freetype_regfonts.InsertSorted(strdup(str), sortByFilePart);
#endif
    return 1;
  } 
  return 0;
}

int GetGlyphIndicesW(HDC ctx, wchar_t *buf, int len, unsigned short *indices, int flags)
{
#ifdef SWELL_FREETYPE
  if (ctx)
  {
    HGDIOBJ font  = HDC_VALID(ctx) && HGDIOBJ_VALID(ctx->curfont,TYPE_FONT) ? ctx->curfont : SWELL_GetDefaultFont();
    if (font)
    {
      FT_Face face=(FT_Face)font->typedata;
      if (face)
      {
        for (int i=0; i < len; ++i)
        {
          int c = FT_Get_Char_Index(face,buf[i]);
          indices[i] = c ? c : 0xFFFF;
        }
        return len;
      }
    }
  }
#endif

  for (int i=0; i < len; ++i) indices[i]=0xFFFF;
  return len;
}

#endif

#endif // SWELL_LICE_GDI
