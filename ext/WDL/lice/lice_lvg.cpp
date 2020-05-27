#include "lice.h"
#include <stdio.h>
#include <math.h>
#include "../projectcontext.h"
#include "../lineparse.h"
#include "../ptrlist.h"
#include "../assocarray.h"

#define PI 3.1415926535897932384626433832795

static inline int chartohex(char c)
{
  if (c >= '0' && c<='9') return c-'0';
  else if (c>='A' && c<='F') return 10 + c - 'A';
  else if (c>='a' && c<='f') return 10 + c - 'a';
  return -1;
}
static int __boolval(const char *p, int defval)
{
  if (!stricmp(p,"yes") ||
      !stricmp(p,"true") ||
      !stricmp(p,"on") ||
      atoi(p)>0) return 1;
  if (!stricmp(p,"no") ||
      !stricmp(p,"false") ||
      !stricmp(p,"off") ||
      !stricmp(p,"0")) return 0;
  return defval;
}

static LICE_pixel __colorval(const char *p, LICE_pixel def)
{
  const size_t lp = strlen(p);
  if (lp == 3)
  {
    int r = chartohex(p[0]);
    int g = chartohex(p[1]);
    int b = chartohex(p[2]);
    if (r>=0&&g>=0&&b>=0)
      def = LICE_RGBA(r+(r<<4),g+(g<<4),b+(b<<4),255);
  }
  else if (lp == 6||lp==8)
  {
    int r = chartohex(p[0]), r2 = chartohex(p[1]);
    int g = chartohex(p[2]), g2 = chartohex(p[3]);
    int b = chartohex(p[4]), b2 = chartohex(p[5]);
    int a = 0xf, a2=0xf;
    if (lp==8) { a=chartohex(p[6]); a2=chartohex(p[7]); }
    if (r>=0&&g>=0&&b>=0&&r2>=0&&g2>=0&&b2>=0&&a>=0&&a2>=0)
      def = LICE_RGBA((r<<4)+r2,(g<<4)+g2,(b<<4)+b2,(a<<4)+a2);
  }
  return def;
}

class lvgRenderState
{
public:
  lvgRenderState() 
  {
    m_color=LICE_RGBA(255,255,255,255);
    m_alpha=1.0f;
    m_blend = LICE_BLIT_MODE_COPY;
    m_aa = false;
  }
  ~lvgRenderState() { }

  LICE_pixel m_color;
  float m_alpha;
  int m_blend;
  bool m_aa;

  WDL_TypedBuf<bool> m_aa_stack;
  WDL_TypedBuf<float> m_alpha_stack;
  WDL_TypedBuf<LICE_pixel> m_color_stack;
  WDL_TypedBuf<int> m_blend_stack;

/////////

  void parsealpha(const char *p)
  {
    int idx=0;
    if (*p == '-') idx++;
    if (p[idx] == '.' || (p[idx] >= '0' && p[idx] <= '9'))
      m_alpha = (float)atof(p);
  }

  void parseaa(const char *p)
  {
    int a = __boolval(p,-1);
    if (a>=0) m_aa = !!a;
  }

  void parseblend(const char *p)
  {
    if (!stricmp(p,"copy")) m_blend = LICE_BLIT_MODE_COPY;
    else if (!stricmp(p,"add")) m_blend = LICE_BLIT_MODE_ADD;
    else if (!stricmp(p,"dodge")) m_blend = LICE_BLIT_MODE_DODGE;
    else if (!stricmp(p,"mul")||!stricmp(p,"multiply")) m_blend = LICE_BLIT_MODE_MUL;
    else if (!stricmp(p,"overlay")) m_blend = LICE_BLIT_MODE_MUL;
    else if (!stricmp(p,"hsvadj")) m_blend = LICE_BLIT_MODE_HSVADJ;
  }

  void parsecolor(const char *p)
  {
    m_color = __colorval(p,m_color);
  }

#define DEF_PUSHPOP(name,defpopval) \
  void push##name() { int sz=m_##name##_stack.GetSize(); m_##name##_stack.Resize(sz+1,false)[sz] = m_##name; } \
  void pop##name() { int sz = m_##name##_stack.GetSize()-1; m_##name = sz>=0 ? m_##name##_stack.Get()[sz] : defpopval; m_##name##_stack.Resize(sz,false); }
  DEF_PUSHPOP(color,LICE_RGBA(0,0,0,255))
  DEF_PUSHPOP(alpha,1.0f)
  DEF_PUSHPOP(aa,false)
  DEF_PUSHPOP(blend,LICE_BLIT_MODE_COPY)
#undef DEF_PUSHPOP

  bool processAttributeLine(LineParser *lp)
  {
    int i,numtok=lp->getnumtokens();
    switch (lp->gettoken_enum(0,"color\0"
                                "alpha\0"
                                "aa\0"
                                "blend\0"
                                "\0"))
    {
#define PROCTYPE(v,name) \
      case v: for (i=1;i<numtok;i++) { \
          const char *p = lp->gettoken_str(i); \
          if (!stricmp(p,"push")) push##name(); \
          else if (!stricmp(p,"pop")) pop##name(); \
          else parse##name(p); \
        } \
      return true;

      PROCTYPE(0,color)
      PROCTYPE(1,alpha)
      PROCTYPE(2,aa)
      PROCTYPE(3,blend)
#undef PROCTYPE

    }
    return false;
  }


};

class lvgImageCtx 
{
public:
  lvgImageCtx(lvgImageCtx *par) : m_images(true,deleteThis)
  {
    m_in_render=false;
    m_par=par;
    m_cachedImage=0;
    m_base_w=0;
    m_base_h=0;
  }
  ~lvgImageCtx()
  {
    delete m_cachedImage;
    m_lines.Empty(true,free);
  }

  WDL_PtrList<char> m_lines; 
  LICE_IBitmap *m_cachedImage;

  lvgImageCtx *m_par;
  WDL_StringKeyedArray<lvgImageCtx *> m_images;

  int m_base_w,m_base_h;
  bool m_in_render;

  void render(lvgRenderState *rstate, int wantw, int wanth);

private:
  static void deleteThis(lvgImageCtx *t) { delete t; }

  double parsecoord(const char *p, double scale, bool round)
  {
    if (!*p) return 0;
    if (p[0] == 'a' && p[1]) return atoi(p+1);
    if (p[0] == 'w')
    {
      scale = m_cachedImage ? m_cachedImage->getWidth() : 0.0;
      p++;
    }
    else if (p[0] == 'h')
    {
      scale = m_cachedImage ? m_cachedImage->getHeight() : 0.0;
      p++;
    }
    return atof(p) * scale + (round ? 0.5 : 0.0);
  }
  void processLvgLine(LineParser *lp, lvgRenderState *state, LICE_IBitmap *bm, double xscale, double yscale);

};

#define DECL_OPT(type, cfunc) \
  static type getoption_##type(LineParser *lp, int startidx, const char *name, type def) { \
    const size_t namelen = strlen(name); \
    for(;startidx<lp->getnumtokens();startidx++) { \
      const char *p=lp->gettoken_str(startidx); \
      if (!strnicmp(name,p,namelen) && p[namelen]=='=') return cfunc(p+namelen+1,def); \
    } \
    return def; \
  }

static int __intval(const char *p, int def) { return atoi(p); }
static double __dblval(const char *p, double def) { return atof(p); }

DECL_OPT(bool,!!__boolval)
DECL_OPT(int,__intval)
DECL_OPT(double,__dblval)
DECL_OPT(LICE_pixel,__colorval)

#undef DECL_OPT

void lvgImageCtx::processLvgLine(LineParser *lp, lvgRenderState *state, LICE_IBitmap *bm, double xscale, double yscale)
{
  if (state->processAttributeLine(lp)) return;

  int numtok = lp->getnumtokens();
  const char *tok = lp->gettoken_str(0);
  if (!stricmp(tok,"line"))
  {
    int i;
    float lx,ly;
    for (i = 1; i < numtok-1; i+= 2)
    {
      const char *p=lp->gettoken_str(i);
      if (strstr(p,"=")) break;
      float x=(float)parsecoord(p,xscale,false);
      p=lp->gettoken_str(i+1);
      if (strstr(p,"=")) break;
      float y=(float)parsecoord(p,yscale,false);

      if (i!=1) LICE_FLine(bm,lx,ly,x,y,state->m_color,state->m_alpha,state->m_blend,state->m_aa);

      lx=x;
      ly=y;
    }
  }
  else if (!stricmp(tok,"circle"))
  {
    if (numtok>=4)
    {
      float x=(float)parsecoord(lp->gettoken_str(1),xscale,false);
      float y=(float)parsecoord(lp->gettoken_str(2),yscale,false);
      float r=(float)(atof(lp->gettoken_str(3))*lice_min(xscale,yscale));
      if (getoption_bool(lp,1,"fill",false))
      {
        LICE_FillCircle(bm,x,y,r,state->m_color,state->m_alpha,state->m_blend,state->m_aa);
      }
      else
        LICE_Circle(bm,x,y,r,state->m_color,state->m_alpha,state->m_blend,state->m_aa);
    }
  }
  else if (!stricmp(tok,"arc"))
  {
    if (numtok>=6)
    {
      float x=(float)parsecoord(lp->gettoken_str(1),xscale,false);
      float y=(float)parsecoord(lp->gettoken_str(2),yscale,false);
      float r=(float)(atof(lp->gettoken_str(3))*lice_min(xscale,yscale));
      float a1=(float)(atof(lp->gettoken_str(4))*PI/180.0);
      float a2=(float)(atof(lp->gettoken_str(5))*PI/180.0);
      LICE_Arc(bm,x,y,r,a1,a2,state->m_color,state->m_alpha,state->m_blend,state->m_aa);
    }
  }
  else if (!stricmp(tok,"fill"))
  {
    if (numtok>=3)  // fill x y [cmask=xxxxxx kmask=xxxxxxx]
    {
      LICE_pixel cmask = getoption_LICE_pixel(lp,1,"cmask",LICE_RGBA(255,255,255,0));
      LICE_pixel kmask = getoption_LICE_pixel(lp,1,"kmask",LICE_RGBA(0,0,0,0));
      int x = (int)parsecoord(lp->gettoken_str(1),xscale,true);
      int y = (int)parsecoord(lp->gettoken_str(2),yscale,true);

      LICE_SimpleFill(bm,x,y,state->m_color,cmask,kmask);
    }
  }
  else if (!stricmp(tok,"rect"))
  {
    if (numtok>=5) // rect x y w h [dcdx=xxxxxxxx dcdy=xxxxxxxxx dcdxscale=1.0 dcdyscale=1.0]
    {
      LICE_pixel dcdx = getoption_LICE_pixel(lp,1,"dcdx",LICE_RGBA(0x80,0x80,0x80,0x80));
      LICE_pixel dcdy = getoption_LICE_pixel(lp,1,"dcdy",LICE_RGBA(0x80,0x80,0x80,0x80));
      double dcdxsc = getoption_double(lp,1,"dcdxscale",1.0);
      double dcdysc = getoption_double(lp,1,"dcdyscale",1.0);

      // todo: any options?
      int x = (int)parsecoord(lp->gettoken_str(1),xscale,true);
      int y = (int)parsecoord(lp->gettoken_str(2),yscale,true);
      int w = (int)parsecoord(lp->gettoken_str(3),xscale,true);
      int h = (int)parsecoord(lp->gettoken_str(4),yscale,true);
      if (w>0 && h>0) 
      {
        if (dcdx!=LICE_RGBA(0x80,0x80,0x80,0x80) || 
            dcdy!=LICE_RGBA(0x80,0x80,0x80,0x80))
        {
          LICE_pixel sc = state->m_color;
          dcdxsc /= w*128.0;
          dcdysc /= h*128.0;
          LICE_GradRect(bm,x,y,w,h,
                          (float)(LICE_GETR(sc)/255.0), 
                          (float)(LICE_GETG(sc)/255.0), 
                          (float)(LICE_GETB(sc)/255.0), 
                          (float)(LICE_GETA(sc)/255.0*state->m_alpha),
                          (float)(((int)LICE_GETR(dcdx)-0x80)*dcdxsc),
                          (float)(((int)LICE_GETG(dcdx)-0x80)*dcdxsc),
                          (float)(((int)LICE_GETB(dcdx)-0x80)*dcdxsc),
                          (float)(((int)LICE_GETA(dcdx)-0x80)*dcdxsc),
                          (float)(((int)LICE_GETR(dcdy)-0x80)*dcdysc),
                          (float)(((int)LICE_GETG(dcdy)-0x80)*dcdysc),
                          (float)(((int)LICE_GETB(dcdy)-0x80)*dcdysc),
                          (float)(((int)LICE_GETA(dcdy)-0x80)*dcdysc),
                          state->m_blend);
        }
        else
          LICE_FillRect(bm,x,y,w,h,state->m_color,state->m_alpha,state->m_blend);
      }
    }
  }
  else if (!stricmp(tok,"rerender"))
  {
    if (numtok>=2)
    {
      int forcew=getoption_int(lp,1,"w",0),forceh=getoption_int(lp,1,"h",0);
      bool useState=getoption_bool(lp,1,"usestate",false);

      lvgImageCtx *scan = this;
      while (scan)
      {
        lvgImageCtx *p = scan->m_images.Get(lp->gettoken_str(1));
        if (p)
        {
          if (!p->m_in_render)
          {
            p->m_in_render=true;
            p->render(useState ? state : NULL,forcew,forceh);
            p->m_in_render=false;
          }
          break;
        }
        scan=scan->m_par;
      }
    }
  }
  else if (!stricmp(tok,"blit"))
  {
    if (numtok>=3) // blit image x y [options]
    {
      LICE_IBitmap *src=NULL;
      lvgImageCtx *scan = this;
      const char *rd = lp->gettoken_str(1);
      
      while (!strnicmp(rd,"parent:",7)) { scan = scan ? scan->m_par : NULL; rd += 7; }
      
      if (!stricmp(rd,"parent")) 
      { 
        if (scan) scan=scan->m_par;
        if (scan) src=scan->m_cachedImage;
      }
      else if (!stricmp(rd,"self")) 
      {
        if (scan) src=scan->m_cachedImage;
      }
      else while (scan&&!src)
      {
        lvgImageCtx *p = scan->m_images.Get(rd);
        if (p)
        {
          if (!p->m_cachedImage && !p->m_in_render) 
          {
            p->m_in_render=true;
            p->render(NULL,0,0);
            p->m_in_render=false;
          }
          src = p->m_cachedImage;
          break;
        }
        scan=scan->m_par;
      }
      if (src)
      {
        int x = (int)parsecoord(lp->gettoken_str(2),xscale,true);
        int y = (int)parsecoord(lp->gettoken_str(3),yscale,true);

        // these will be options filter= srcalpha= w= h= scale=
        bool filter=getoption_bool(lp,1,"filter",true);
        bool usesrcalpha = getoption_bool(lp,1,"srcalpha",true);
        int w = getoption_int(lp,1,"w",src->getWidth());
        int h = getoption_int(lp,1,"h",src->getHeight());
        double sc = getoption_double(lp,1,"scale",1.0f);
        if (fabs(sc-1.0)>0.0000000001) 
        {
          w = (int)(w*sc+0.5);
          h = (int)(h*sc+0.5);
        }
//        double ang = getoption_double(lp,1,"rotate",0.0) * PI / 180.0;
        float sx=(float)getoption_double(lp,1,"srcx",0.0);
        float sy=(float)getoption_double(lp,1,"srcy",0.0);
        float sw=(float)getoption_double(lp,1,"srcw",src->getWidth());
        float sh=(float)getoption_double(lp,1,"srch",src->getHeight());
//        if (fabs(ang)>0.0001) LICE_RotatedBlit(bm,src,x,y,w,h,sx,sy,sw,sh,ang,true,state->m_alpha,state->m_blend,0,0);
        //else 
        LICE_ScaledBlit(bm,src,x,y,w,h,sx,sy,sw,sh,
                      state->m_alpha,state->m_blend|(filter ? LICE_BLIT_FILTER_BILINEAR : 0)|(usesrcalpha ? LICE_BLIT_USE_ALPHA : 0));
      }
    }
  }
}

void lvgImageCtx::render(lvgRenderState *rstate, int wantw, int wanth)
{
  if (wantw<1) wantw = m_base_w;
  if (wanth<1) wanth = m_base_h;

  if (wantw<1||wanth<1) 
  {
    if (m_cachedImage) m_cachedImage->resize(0,0);
    return;
  }

  if (!m_cachedImage) m_cachedImage = new LICE_MemBitmap(wantw,wanth);
  else m_cachedImage->resize(wantw,wanth);

  LICE_Clear(m_cachedImage,LICE_RGBA(0,0,0,0));

  lvgRenderState rs;
  if (rstate) rs = *rstate;
 
  double xscale = wantw / lice_max(m_base_w,1);
  double yscale = wanth / lice_max(m_base_h,1);
  int x;
  bool comment_state=false;
  LineParser lp(comment_state);
  for (x=0;x<m_lines.GetSize();x++)
  {
    if (!lp.parse(m_lines.Get(x)) && lp.getnumtokens()>0)
      processLvgLine(&lp,&rs,m_cachedImage,xscale,yscale);
  }
}

void *LICE_GetSubLVG(void *lvg, const char *subname)
{
  if (!lvg||!subname||!*subname) return NULL;
  lvgImageCtx *t = (lvgImageCtx *)lvg;
  return t->m_images.Get(subname);
}

LICE_IBitmap *LICE_RenderLVG(void *lvg, int reqw, int reqh, LICE_IBitmap *bmOut)
{
  lvgImageCtx *t = (lvgImageCtx *)lvg;
  if (!t || !t->m_lines.GetSize() || t->m_in_render) return NULL;

  if (bmOut)
  {
    delete t->m_cachedImage;
    t->m_cachedImage = bmOut;
  }  
  else if (!t->m_cachedImage) t->m_cachedImage = new LICE_MemBitmap;

  t->m_in_render=true;
  t->render(NULL,reqw,reqh);
  t->m_in_render=false;

  LICE_IBitmap *ret = t->m_cachedImage;
  
  t->m_cachedImage=NULL;

  return ret;
}

void LICE_DestroyLVG(void *lvg)
{
  lvgImageCtx *t = (lvgImageCtx *)lvg;
  if (t && !t->m_par) delete t;
}

class lvgRdContext : public ProjectStateContext
{
public:
  lvgRdContext(FILE *fp) { m_fp=fp; }
  virtual ~lvgRdContext() {  }

  virtual void AddLine(const char *fmt, ...) {};
  virtual int GetLine(char *buf, int buflen) // returns -1 on eof
  {
    if (!m_fp) return -1;
    for (;;)
    {
      buf[0]=0;
      fgets(buf,buflen,m_fp);
      if (!buf[0]) return -1;

      char *p=buf;
      while (*p) p++;
      while (p>buf && (p[-1] == '\r' || p[-1]=='\n')) p--;
      *p=0;
      if (*buf) return 0;
    }
  }

  virtual WDL_INT64 GetOutputSize() { return 0; }
  virtual int GetTempFlag() { return 0; }
  virtual void SetTempFlag(int flag) {}

  FILE *m_fp;
};

void *LICE_LoadLVGFromContext(ProjectStateContext *ctx, const char *nameInfo, int defw, int defh)
{
  if (!ctx) return NULL;

  bool err=false;
  int ignoreBlockCnt=0;

  lvgImageCtx *retimg = new lvgImageCtx(NULL);
  lvgImageCtx *curimg = NULL;

  if (nameInfo)
  {
    curimg = retimg;
    curimg->m_base_w = defw;
    curimg->m_base_h = defh;
  }

  while (!err)
  {
    char line[4096];
    line[0]=0;
    if (ctx->GetLine(line,sizeof(line))) break;

    char *p=line;
    while (*p == ' ' || *p == '\t') p++;
    if (!*p) continue;

    if (ignoreBlockCnt>0)
    {
      if (*p == '<') ignoreBlockCnt++;
      else if (*p == '>') ignoreBlockCnt--;
    }
    else
    {
      if (*p == '<') 
      {
        bool comment_state=false;
        LineParser lp(comment_state);
        if (!lp.parse(p)&&lp.getnumtokens()>=2 && !strcmp(lp.gettoken_str(0),"<LVG")) 
        {
          if (!curimg) 
          {
            // lp.gettoken_str(1) = version info string?
            curimg = retimg;
          }
          else
          {
            lvgImageCtx *img = new lvgImageCtx(curimg);
            curimg->m_images.Insert(lp.gettoken_str(1),img);
            curimg = img;
          }
          curimg->m_base_w = lp.gettoken_int(2);
          curimg->m_base_h = lp.gettoken_int(3);
        }
        else ignoreBlockCnt++;
      }
      else if (curimg)
      {
        if (*p == '>') 
        {
          curimg = curimg->m_par;
          if (!curimg) break; // success!
        }
        else
        {
          curimg->m_lines.Add(strdup(p));
        }
      }

      if (!curimg) err=true; // <LVG must be first non-whitespace line
    }
  }

  if (err)
  {
    delete retimg;
    return 0;
  }
  return retimg;

}

void *LICE_LoadLVG(const char *filename)
{
  FILE *fp=NULL;
#if defined(_WIN32) && !defined(WDL_NO_SUPPORT_UTF8)
  #ifdef WDL_SUPPORT_WIN9X
  if (GetVersion()<0x80000000)
  #endif
  {
    WCHAR wf[2048];
    if (MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,filename,-1,wf,2048))
      fp = _wfopen(wf,L"rb");
  }
#endif
  if (!fp) fp = WDL_fopenA(filename,"rb");

  if (fp)
  {
    lvgRdContext ctx(fp);

    void *p = LICE_LoadLVGFromContext(&ctx,NULL,0,0);
    
    fclose(fp);

    return p;
  }

  return 0;
}
