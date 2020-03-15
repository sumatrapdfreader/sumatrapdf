#ifndef _LICE_TEXT_H_
#define _LICE_TEXT_H_

#include "lice.h"

#ifndef _WIN32
#include "../swell/swell.h"
#endif
#include "../heapbuf.h"

#define LICE_FONT_FLAG_VERTICAL 1 // rotate text to vertical (do not set the windows font to vertical though)
#define LICE_FONT_FLAG_VERTICAL_BOTTOMUP 2

#define LICE_FONT_FLAG_PRECALCALL 4
//#define LICE_FONT_FLAG_ALLOW_NATIVE 8
#define LICE_FONT_FLAG_FORCE_NATIVE 1024

#define LICE_FONT_FLAG_FX_BLUR 16
#define LICE_FONT_FLAG_FX_INVERT 32
#define LICE_FONT_FLAG_FX_MONO 64 // faster but no AA/etc

#define LICE_FONT_FLAG_FX_SHADOW 128 // these imply MONO
#define LICE_FONT_FLAG_FX_OUTLINE 256

#define LICE_FONT_FLAG_OWNS_HFONT 512

// could do a mask for these flags
#define LICE_FONT_FLAGS_HAS_FX(flag) \
  (flag&(LICE_FONT_FLAG_VERTICAL|LICE_FONT_FLAG_VERTICAL_BOTTOMUP| \
         LICE_FONT_FLAG_FX_BLUR|LICE_FONT_FLAG_FX_INVERT|LICE_FONT_FLAG_FX_MONO| \
         LICE_FONT_FLAG_FX_SHADOW|LICE_FONT_FLAG_FX_OUTLINE))

#define LICE_DT_NEEDALPHA  0x80000000 // include in DrawText() if the output alpha channel is important
#define LICE_DT_USEFGALPHA 0x40000000 // uses alpha channel in fg color 

class LICE_IFont
{
  public:
    virtual ~LICE_IFont() {}

    virtual void SetFromHFont(HFONT font, int flags=0)=0; // hfont must REMAIN valid, unless LICE_FONT_FLAG_PRECALCALL or LICE_FONT_FLAG_OWNS_HFONT set (OWNS means LICE_IFont will clean up hfont on font change or exit)

    virtual LICE_pixel SetTextColor(LICE_pixel color)=0;
    virtual LICE_pixel SetBkColor(LICE_pixel color)=0;
    virtual LICE_pixel SetEffectColor(LICE_pixel color)=0;
    virtual int SetBkMode(int bkmode)=0;
    virtual void SetCombineMode(int combine, float alpha=1.0f)=0;

    virtual int DrawText(LICE_IBitmap *bm, const char *str, int strcnt, RECT *rect, UINT dtFlags)=0;

    virtual LICE_pixel GetTextColor()=0;
    virtual HFONT GetHFont()=0;
    virtual int GetLineHeight()=0;
    virtual void SetLineSpacingAdjust(int amt)=0;
};


#ifndef LICE_TEXT_NO_DECLARE_CACHEDFONT

class LICE_CachedFont : public LICE_IFont
{
  public:
    LICE_CachedFont();
    virtual ~LICE_CachedFont();

    virtual void SetFromHFont(HFONT font, int flags=0);

    virtual LICE_pixel SetTextColor(LICE_pixel color) { LICE_pixel ret=m_fg; m_fg=color; return ret; }
    virtual LICE_pixel SetBkColor(LICE_pixel color) { LICE_pixel ret=m_bg; m_bg=color; return ret; }
    virtual LICE_pixel SetEffectColor(LICE_pixel color) { LICE_pixel ret=m_effectcol; m_effectcol=color; return ret; }
    virtual int SetBkMode(int bkmode) { int bk = m_bgmode; m_bgmode=bkmode; return bk; }
    virtual void SetCombineMode(int combine, float alpha=1.0f) { m_comb=combine; m_alpha=alpha; }

    virtual int DrawText(LICE_IBitmap *bm, const char *str, int strcnt, RECT *rect, UINT dtFlags)
    {
      return DrawTextImpl(bm,str,strcnt,rect,dtFlags);
    }

    virtual LICE_pixel GetTextColor() { return m_fg; }
    virtual HFONT GetHFont() { return m_font; }
    virtual int GetLineHeight() { return m_line_height; }

    virtual void SetLineSpacingAdjust(int amt) { m_lsadj=amt; }

  protected:

    virtual bool DrawGlyph(LICE_IBitmap *bm, unsigned short c, int xpos, int ypos, RECT *clipR);
    int DrawTextImpl(LICE_IBitmap *bm, const char *str, int strcnt, RECT *rect, UINT dtFlags); // cause swell defines DrawText to SWELL_DrawText etc

    bool RenderGlyph(unsigned short idx);

    LICE_pixel m_fg,m_bg,m_effectcol;
    int m_bgmode;
    int m_comb;
    float m_alpha;
    int m_flags;

    int m_line_height,m_lsadj;
    struct charEnt
    {
      int base_offset; // offset in m_cachestore+1, so 1=offset0, 0=unset, -1=failed to render
      int width, height;
      int advance;
      int charid; // used by m_extracharlist
      int left_extra;
    };
    charEnt *findChar(unsigned short c);

    charEnt m_lowchars[128]; // first 128 chars cached here
    WDL_TypedBuf<charEnt> m_extracharlist;
    WDL_TypedBuf<unsigned char> m_cachestore;
    
    static int _charSortFunc(const void *a, const void *b);

    HFONT m_font;

};

#endif // !LICE_TEXT_NO_DECLARE_CACHEDFONT

#ifndef LICE_TEXT_NO_MULTIDPI
class __LICE_dpiAwareFont : public LICE_IFont
{
  struct rec {
    LICE_IFont *cache;
    int sz;
  };
  WDL_TypedBuf<rec> m_list; // used entries are at end of list, most recently used last. sz=0 for unused

  int (*m_getflags)(int);
  int m_flags;
  LICE_pixel m_fg, m_bg, m_effectcol;
  int m_bgmode, m_comb;
  float m_alpha;
  int m_lsadj;

public:
  LOGFONT m_lf;


  // LICE_IFont interface
  virtual void SetFromHFont(HFONT font, int flags=0) { }

  virtual LICE_pixel SetTextColor(LICE_pixel color) { LICE_pixel ret=m_fg; m_fg=color; return ret; }
  virtual LICE_pixel SetBkColor(LICE_pixel color) { LICE_pixel ret=m_bg; m_bg=color; return ret; }
  virtual LICE_pixel SetEffectColor(LICE_pixel color) { LICE_pixel ret=m_effectcol; m_effectcol=color; return ret; }
  virtual int SetBkMode(int bkmode) { int bk = m_bgmode; m_bgmode=bkmode; return bk; }
  virtual void SetCombineMode(int combine, float alpha=1.0f) { m_comb=combine; m_alpha=alpha; }

  virtual int DrawText(LICE_IBitmap *bm, const char *str, int strcnt, RECT *rect, UINT dtFlags)
  {
    LICE_IFont *f = get(bm);
    if (!f) return 0;
    if (!(dtFlags & DT_CALCRECT))
    {
      f->SetTextColor(m_fg);
      f->SetBkColor(m_bg);
      f->SetEffectColor(m_effectcol);
      f->SetBkMode(m_bgmode);
      f->SetCombineMode(m_comb,m_alpha);
      f->SetLineSpacingAdjust(m_lsadj);
    }
    return f->DrawText(bm,str,strcnt,rect,dtFlags);
  }

  virtual LICE_pixel GetTextColor() { return m_fg; }
  virtual HFONT GetHFont() { return NULL; }
  virtual int GetLineHeight() { return GetLineHeightDPI(NULL); }

  virtual void SetLineSpacingAdjust(int amt) { m_lsadj=amt; }

  __LICE_dpiAwareFont(int maxsz)
  {
    memset(&m_lf,0,sizeof(m_lf));
    m_getflags = NULL;
    m_flags=0;
    m_fg=m_bg=m_effectcol=0;
    m_bgmode=TRANSPARENT;
    m_comb=0;
    m_alpha=1.0;
    m_lsadj=0;

    rec *l = m_list.ResizeOK(maxsz);
    if (l) memset(l,0,sizeof(*l)*maxsz);
  }
  ~__LICE_dpiAwareFont()
  {
    for (int x = 0; x < m_list.GetSize(); x ++) delete m_list.Get()[x].cache;
  }
  void SetFromLogFont(LOGFONT *lf, int (*get_flags)(int))
  {
    m_lf = *lf;
    m_getflags = get_flags;
    m_flags = get_flags ? get_flags(0) : 0;
    clear();
  }

  void clear()
  {
    int x = m_list.GetSize()-1;
    rec *t = m_list.Get();
    while (x>=0 && t[x].sz) t[x--].sz=0;
  }

  LICE_IFont *get(LICE_IBitmap *bm)
  {
    int use_flag = m_getflags ? m_getflags(0) & ~LICE_FONT_FLAG_PRECALCALL : 0;
    if (m_flags != use_flag)
    {
      m_flags = use_flag;
      clear();
    }

    int ht = m_lf.lfHeight, ht2 = m_lf.lfWidth;
    if (bm)
    {
      int sz = (int)bm->Extended(LICE_EXT_GET_ANY_SCALING,NULL);
      if (sz) 
      {
        ht = (ht * sz) / 256;
        ht2 = (ht2 * sz) / 256;
        if (sz != 256)
          use_flag |= LICE_FONT_FLAG_FORCE_NATIVE;
      }
    }

    int x = m_list.GetSize()-1;
    rec *t = m_list.Get();
    while (x>=0 && t[x].sz != ht && t[x].sz) x--;
    if (x<0) t[x=0].sz = 0; // if list full, use oldest item

    // move to end of list
    if (x != m_list.GetSize()-1)
    {
      rec tmp = t[x];
      m_list.Delete(x);
      m_list.Add(tmp);
    }

    t = m_list.Get() + m_list.GetSize() - 1;
    if (!t->cache) t->cache = __CreateFont();
    if (!t->sz && t->cache)
    {
      t->sz = ht;
      LOGFONT lf = m_lf;
      lf.lfHeight = ht;
      lf.lfWidth = ht2;
      #ifdef _WIN32
      if (!(m_flags & LICE_FONT_FLAG_FORCE_NATIVE) && abs(lf.lfHeight) <= 14) lf.lfQuality = NONANTIALIASED_QUALITY;
      #endif
      t->cache->SetFromHFont(CreateFontIndirect(&lf), LICE_FONT_FLAG_OWNS_HFONT | use_flag);
    }

    return t->cache;
  }

  int GetLineHeightDPI(LICE_IBitmap *bm)
  {
    LICE_IFont *f = get(bm);
    return f ? f->GetLineHeight() : 10;
  }
  virtual LICE_IFont *__CreateFont()=0;
};

template<class BASEFONT> class LICE_dpiAwareFont : public __LICE_dpiAwareFont {
  public:
    LICE_dpiAwareFont(int max) : __LICE_dpiAwareFont(max) { }
    virtual LICE_IFont *__CreateFont() { return new BASEFONT; }
};
#endif//LICE_TEXT_NO_MULTIDPI

#endif//_LICE_TEXT_H_
