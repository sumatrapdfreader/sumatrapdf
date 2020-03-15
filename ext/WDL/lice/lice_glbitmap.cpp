#include "lice_glbitmap.h"
#include "lice_gl_ctx.h"

#include "../plush2/plush.h"

LICE_GLBitmap::LICE_GLBitmap()
{
  m_bmp = 0;
  m_fbo = 0;
  m_tex = 0;
  m_bufloc = EMPTY;  
}

// This is separate from the constructor for initialization order reasons
void LICE_GLBitmap::Init(LICE_IBitmap* bmp, int w, int h)
{
  m_bmp = bmp;
  if (w > 0 && h > 0) CreateFBO(w, h);
}

bool LICE_GLBitmap::CreateFBO(int w, int h)
{
  if (LICE_GL_IsValid())
  {   
    if (m_fbo) ReleaseFBO();
    glGenFramebuffersEXT(1, &m_fbo);  // create a new empty FBO
    if (m_fbo)
    {
      glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);  // bind this FBO so it is the rendering target
      glEnable(GL_TEXTURE_RECTANGLE_ARB); // enable texturing
      glGenTextures(1, &m_tex); // create a new texture to be the backing store
      if (m_tex)
      {
        glBindTexture(GL_TEXTURE_RECTANGLE_ARB, m_tex); // bind this texture so it is the texturing target
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST); // we won't be scaling it for this purpose
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);  // size the texture
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, m_tex, 0);  // attach the texture as the backing store for the FBO
      }
      glDisable(GL_TEXTURE_RECTANGLE_ARB);  // done texturing
    }
  
    return BindFBO();  // this tests the FBO for validity
  } 
  return false;
}

LICE_GL_SysBitmap::LICE_GL_SysBitmap(int w, int h)
: m_sysbmp(w, h)
{
  Init(&m_sysbmp, w, h);
}

LICE_GL_MemBitmap::LICE_GL_MemBitmap(int w, int h)
: m_membmp(w, h)
{
  Init(&m_membmp, w, h);
}

HDC LICE_GL_SysBitmap::getDC()
{
  // no known way to get a DC directly from an offscreen openGL render context, sadly
  FramebufferFromGPU();
  OutputDebugString("GL to screen");
  return m_sysbmp.getDC();  
}


LICE_GL_SubBitmap::LICE_GL_SubBitmap(LICE_IBitmap *parent, int x, int y, int w, int h)
{
  m_parent = parent;
  m_x = x;
  m_y = y;
  m_w = w;
  m_h = h;
}
  
LICE_pixel* LICE_GL_SubBitmap::getBits()
{
  if (!m_parent) return 0;
  return m_parent->getBits()+m_y*m_parent->getRowSpan()+m_x;
}

bool LICE_GL_SubBitmap::resize(int w, int h)
{
  if (w == m_w && h == m_h) return false;
  m_w = w;
  m_h = h; 
  return true;
}

INT_PTR LICE_GL_SubBitmap::Extended(int id, void* data)
{
  if (!m_parent) return 0;

  if (id == LICE_EXT_SUPPORTS_ID) return m_parent->Extended(id, data);

  LICE_Ext_SetClip_data clipdata(m_x, m_y, m_w, m_h);
  if (!m_parent->Extended(LICE_EXT_SETCLIP, &clipdata)) return 0;
  INT_PTR ret = m_parent->Extended(id, data);
  m_parent->Extended(LICE_EXT_SETCLIP, 0);

  return ret;
}

bool LICE_GLBitmap::SetClip_ext(LICE_Ext_SetClip_data* p)
{
  if (!FramebufferToGPU()) return false;

  if (p)
  {
    GLdouble c0[4] = { 1.0, 0.0, 0.0, -p->x };
    GLdouble c1[4] = { -1.0, 0.0, 0.0, p->x+p->w  };
    GLdouble c2[4] = { 0.0, 1.0, 0.0, -p->y};
    GLdouble c3[4] = { 0.0, -1.0, 0.0, p->y+p->h };
    glClipPlane(GL_CLIP_PLANE0, c0);
    glClipPlane(GL_CLIP_PLANE1, c1);
    glClipPlane(GL_CLIP_PLANE2, c2);
    glClipPlane(GL_CLIP_PLANE3, c3);
    glEnable(GL_CLIP_PLANE0);
    glEnable(GL_CLIP_PLANE1);
    glEnable(GL_CLIP_PLANE2);
    glEnable(GL_CLIP_PLANE3);
  }
  else
  {
    glDisable(GL_CLIP_PLANE0);
    glDisable(GL_CLIP_PLANE1);
    glDisable(GL_CLIP_PLANE2);
    glDisable(GL_CLIP_PLANE3);
  }

  return true;
}

LICE_GLBitmap::~LICE_GLBitmap()
{
  ReleaseFBO();
}

int LICE_GLBitmap::getWidth()
{
  return m_bmp->getWidth();
}

int LICE_GLBitmap::getHeight()
{
  return m_bmp->getHeight();
}

int LICE_GLBitmap::getRowSpan()
{
  return m_bmp->getRowSpan();
}

LICE_pixel* LICE_GLBitmap::getBits()
{
  FramebufferFromGPU();
  return m_bmp->getBits();
}

bool LICE_GLBitmap::resize(int w, int h)
{
  int oldw = getWidth();
  int oldh = getHeight();
  if (w == oldw && h == oldh) return false;

  if (oldw == 0 && oldh == 0 && w > 0 && h > 0) 
  {
    m_bmp->resize(w, h);
    CreateFBO(w, h);
    return true;
  }
    
  if (!m_fbo || !m_tex) return m_bmp->resize(w, h);  

  OutputDebugString("GL resizing");

  // the framebuffer/GPU transfer overhead is per-call, so it's important to be able
  // to move the entire bitmap back and forth in one call, which means keeping m_sysbmp width == rowspan

  int oldloc = m_bufloc;

  FramebufferFromGPU(); // copy out the GPU data (this binds the FBO so it is the rendering target)

  glEnable(GL_TEXTURE_RECTANGLE_ARB); // enable texturing
  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, m_tex); // bind this texture so it is the texturing target
  glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST); // we won't be scaling it for this purpose
  glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);  // size the texture
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, m_tex, 0);  // attach the texture as the backing store for the FBO
  glDisable(GL_TEXTURE_RECTANGLE_ARB);  // done texturing
    
  if (!BindFBO()) return m_bmp->resize(w, h); // this tests the FBO for validity

  static LICE_MemBitmap tmpbmp;
  tmpbmp.resize(w, h);
  LICE_Blit(&tmpbmp, m_bmp, 0, 0, 0, 0, oldw, oldh, 1.0f, LICE_BLIT_MODE_COPY);
  m_bmp->resize(0, 0);  // force it to resize down
  LICE_Copy(m_bmp, &tmpbmp);

  if (oldloc == EMPTY) m_bufloc = EMPTY;  // else bufloc remains INMEM
  
  return true;
}

bool LICE_GLBitmap::BindFBO()
{
  bool valid = false;
  if (m_fbo)
  {
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);  // bind this FBO so it is the rendering target
    valid = (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT);
    if (valid)
    {
      int w = getWidth();
      int h = getHeight();
      glViewport(0, 0, w, h);
      glMatrixMode(GL_PROJECTION);
      glLoadIdentity();
      gluOrtho2D(0.0, w, 0.0, h);
    }
    else
    {
      ReleaseFBO();
      LICE_GL_CloseCtx(); // if we fail once we're done with GL
    }
  }
  return valid; 
}

void LICE_GLBitmap::ReleaseFBO()
{
  if (m_fbo)
  {
    glDeleteFramebuffersEXT(1, &m_fbo);
    m_fbo = 0;
  }
  if (m_tex)
  {
    glDeleteTextures(1, &m_tex);
    m_tex = 0;
  }
}

bool LICE_GLBitmap::FramebufferToGPU()
{
  if (BindFBO())
  {
    if (m_bufloc == INMEM) 
    {
      OutputDebugString("GL to GPU");
      glDisable(GL_BLEND);
      glRasterPos2i(0, 0);
      glDrawPixels(getWidth(), getHeight(), GL_RGBA, GL_UNSIGNED_BYTE, m_bmp->getBits());
    }
    m_bufloc = INGPU;      
  }
  return (m_bufloc == INGPU);
}

void LICE_GLBitmap::FramebufferFromGPU()
{
  if (m_bufloc == INGPU && BindFBO()) 
  {
    OutputDebugString("GL to mem");
    glFinish();
    glReadPixels(0, 0, getWidth(), getHeight(), GL_RGBA, GL_UNSIGNED_BYTE, m_bmp->getBits());
  }
  m_bufloc = INMEM;
}

INT_PTR LICE_GLBitmap::Extended(int id, void* data)
{
  if (id == LICE_EXT_SUPPORTS_ID)
  {
    int extid = (int) data;
    if (extid == LICE_EXT_LINE_ACCEL) return 1;
    if (extid == LICE_EXT_FILLRECT_ACCEL) return 1;
    if (extid == LICE_EXT_DRAWCBEZIER_ACCEL) return 1;
    if (extid == LICE_EXT_DRAWGLYPH_ACCEL) return 1;
    if (extid == LICE_EXT_BLIT_ACCEL) return 1;
    if (extid == LICE_EXT_SCALEDBLIT_ACCEL) return 1;
    if (extid == LICE_EXT_GETFBOTEX_ACCEL) return 1;
    if (extid == LICE_EXT_CLEAR_ACCEL) return 1;
    if (extid == LICE_EXT_DASHEDLINE_ACCEL) return 1;
    if (extid == LICE_EXT_GETPIXEL_ACCEL) return 1;
    if (extid == LICE_EXT_PUTPIXEL_ACCEL) return 1;
    if (extid == LICE_EXT_SETCLIP) return 1;
    if (extid == LICE_EXT_WINDOW_BLIT) return 1;
    if (extid == LICE_EXT_FORGET) return 1;
    if (extid == LICE_EXT_DRAWTRIANGLE_ACCEL) return 1;
    return 0;
  }

  if (id == LICE_EXT_CLEAR_ACCEL) return Clear_accel((LICE_pixel*)data);
  if (id == LICE_EXT_LINE_ACCEL) return Line_accel((LICE_Ext_Line_acceldata*)data);
  if (id == LICE_EXT_FILLRECT_ACCEL) return FillRect_accel((LICE_Ext_FillRect_acceldata*) data);
  if (id == LICE_EXT_DRAWCBEZIER_ACCEL) return DrawCBezier_accel((LICE_Ext_DrawCBezier_acceldata*)data);
  if (id == LICE_EXT_DRAWGLYPH_ACCEL) return DrawGlyph_accel((LICE_Ext_DrawGlyph_acceldata*)data);
  if (id == LICE_EXT_BLIT_ACCEL) return Blit_accel((LICE_Ext_Blit_acceldata*)data);
  if (id == LICE_EXT_SCALEDBLIT_ACCEL) return ScaledBlit_accel((LICE_Ext_ScaledBlit_acceldata*)data);
  if (id == LICE_EXT_DASHEDLINE_ACCEL) return DashedLine_accel((LICE_Ext_DashedLine_acceldata*)data);
  if (id == LICE_EXT_GETPIXEL_ACCEL) return GetPixel_accel((LICE_Ext_GetPixel_acceldata*)data);
  if (id == LICE_EXT_PUTPIXEL_ACCEL) return PutPixel_accel((LICE_Ext_PutPixel_acceldata*)data);
  if (id == LICE_EXT_SETCLIP) return SetClip_ext((LICE_Ext_SetClip_data*)data);
  if (id == LICE_EXT_DRAWTRIANGLE_ACCEL) return DrawTriangle_accel((LICE_Ext_DrawTriangle_acceldata*)data);
  if (id == LICE_EXT_WINDOW_BLIT) return WindowBlit((LICE_Ext_WindowBlit_data*)data);  

  if (id == LICE_EXT_FORGET)
  {
    m_bufloc = EMPTY;
    return 1;
  }

  if (id == LICE_EXT_GETFBOTEX_ACCEL) 
  {
    if (FramebufferToGPU()) return m_tex;  
    return 0;
  }

  return 0;
}  

static void SetGLAliasing(bool aa)
{
  if (aa)
  {
    glEnable(GL_LINE_SMOOTH);
  }
  else
  {
    glDisable(GL_LINE_SMOOTH);
  }
}

static void SetGLColor(LICE_pixel color, float alpha, int licemode)
{
  int a = 255;
  if (licemode&LICE_BLIT_USE_ALPHA) a = LICE_GETA(color);
  a = (float)a*alpha;

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_BLEND);

  glColor4ub(LICE_GETB(color), LICE_GETG(color), LICE_GETR(color), a);
  //glColor4ub(LICE_GETR(color), LICE_GETG(color), LICE_GETB(color), a);
}

bool LICE_GLBitmap::Clear_accel(LICE_pixel* color)
{
  if (!color) return false;
  LICE_pixel col = *color;

  if (!FramebufferToGPU()) return false;

  float r = (float)LICE_GETR(col)/255.0f;
  float g = (float)LICE_GETG(col)/255.0f;
  float b = (float)LICE_GETB(col)/255.0f;
  float a = (float)LICE_GETA(col)/255.0f;
  glClearColor(b, g, r, a);
  //glClearColor(r, g, b, a);
  glClear(GL_COLOR_BUFFER_BIT);

  return true;
}

bool LICE_GLBitmap::Line_accel(LICE_Ext_Line_acceldata* p)
{
  if (!p) return false;
  if (!FramebufferToGPU()) return false;

  SetGLColor(p->color, p->alpha, p->mode);
  SetGLAliasing(p->aa);

  glBegin(GL_LINES);

  glVertex2f(p->x1, p->y1);
  glVertex2f(p->x2, p->y2);

  glEnd();

  return true;
}

bool LICE_GLBitmap::DashedLine_accel(LICE_Ext_DashedLine_acceldata* p)
{
  if (!p) return false;
  if (!FramebufferToGPU()) return false;

  SetGLColor(p->color, p->alpha, p->mode);
  SetGLAliasing(p->aa);

  glEnable(GL_LINE_STIPPLE);
  int n = (p->pxon+p->pxoff)/2; // todo properly when pxon != pxoff
  glLineStipple(n, 0xAAAA);

  glBegin(GL_LINES);

  glVertex2f(p->x1, p->y1);
  glVertex2f(p->x2, p->y2);

  glEnd();

  glLineStipple(1, 65535);
  glDisable(GL_LINE_STIPPLE);

  return true;
}


bool LICE_GLBitmap::FillRect_accel(LICE_Ext_FillRect_acceldata* p)
{
  if (!p) return false;
  if (!FramebufferToGPU()) return false;

  SetGLColor(p->color, p->alpha, p->mode);

  glBegin(GL_POLYGON);

  glVertex2i(p->x, p->y);
  glVertex2i(p->x+p->w, p->y);
  glVertex2i(p->x+p->w, p->y+p->h);
  glVertex2i(p->x, p->y+p->h);

  glEnd();

  return true;
}

bool LICE_GLBitmap::DrawCBezier_accel(LICE_Ext_DrawCBezier_acceldata* p)
{
  if (!p) return false;
  if (!FramebufferToGPU()) return false;

  GLUnurbsObj* nurbs = LICE_GL_GetNurbsObj();
  if (!nurbs) return false;

p->color = LICE_RGBA(255,255,255,255);  // temp for easy ID of GL rendering

  SetGLColor(p->color, p->alpha, p->mode);
  SetGLAliasing(p->aa);

  float ctlpts[] = 
  {
    p->xstart, p->ystart, 0.0f, 
    p->xctl1, p->yctl1, 0.0f,
    p->xctl2, p->yctl2, 0.0f,
    p->xend, p->yend, 0.0f 
  };
  float knots[] = { 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f };

  gluBeginCurve(nurbs);
  gluNurbsCurve(nurbs, 8, knots, 3, ctlpts, 4, GL_MAP1_VERTEX_3);
  gluEndCurve(nurbs);

  return true;
}

bool LICE_GLBitmap::DrawGlyph_accel(LICE_Ext_DrawGlyph_acceldata* p)
{
  if (!p) return false;
  if (!FramebufferToGPU()) return false;

  glEnable(GL_TEXTURE_RECTANGLE_ARB);

  int texID = LICE_GL_GetTexFromGlyph(p->alphas, p->glyph_w, p->glyph_h);
  if (!texID) return false;
  
  SetGLColor(p->color, p->alpha, p->mode);

  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texID);
  glBegin(GL_POLYGON);

  glTexCoord2i(0, 0);
  glVertex2i(p->x, p->y);
  glTexCoord2i(p->glyph_w, 0);
  glVertex2i(p->x+p->glyph_w, p->y);
  glTexCoord2i(p->glyph_w, p->glyph_h);
  glVertex2i(p->x+p->glyph_w, p->y+p->glyph_h);
  glTexCoord2i(0, p->glyph_h);
  glVertex2i(p->x, p->y+p->glyph_h);

  glEnd();

  glDisable(GL_TEXTURE_RECTANGLE_ARB);

  return true;
}

bool LICE_GLBitmap::Blit_accel(LICE_Ext_Blit_acceldata* p)
{
  if (!p || !p->src) return false;
  if (!FramebufferToGPU()) return false;

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_BLEND);

  GLuint src_tex = p->src->Extended(LICE_EXT_GETFBOTEX_ACCEL, 0); // this binds the src's FBO
  if (src_tex)
  {
    if (!BindFBO()) return false; // re-bind dest FBO

    glEnable(GL_TEXTURE_RECTANGLE_ARB);
    
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, src_tex);
    glBegin(GL_POLYGON);

    glTexCoord2i(p->srcx, p->srcy);
    glVertex2i(p->dstx, p->dsty);
    glTexCoord2i(p->srcx+p->srcw, p->srcy);
    glVertex2i(p->dstx+p->srcw, p->dsty);
    glTexCoord2i(p->srcx+p->srcw, p->srcy+p->srch);
    glVertex2i(p->dstx+p->srcw, p->dsty+p->srch);
    glTexCoord2i(p->srcx, p->srcy+p->srch);
    glVertex2i(p->dstx, p->dsty+p->srch);

    glEnd();

    glDisable(GL_TEXTURE_RECTANGLE_ARB);

    return true;
  }
    
  int srcspan = p->src->getRowSpan();
  if (p->srcx == 0 && p->srcy == 0 && p->srcw == srcspan)
  {
    glRasterPos2i(p->dstx, p->dsty);
    glDrawPixels(p->srcw, p->srch, GL_RGBA, GL_UNSIGNED_BYTE, p->src->getBits());
    return true;
  }

  LICE_pixel* srcrow = p->src->getBits()+p->srcy*srcspan+p->srcx;
  int y;
  for (y = 0; y < p->srch; ++y, srcrow += srcspan)
  {
    glRasterPos2i(p->dstx, p->dsty+y);
    glDrawPixels(p->srcw, 1, GL_RGBA, GL_UNSIGNED_BYTE, srcrow);
  }
  return true;
}

bool LICE_GLBitmap::ScaledBlit_accel(LICE_Ext_ScaledBlit_acceldata* p)
{
  if (!p || !p->src) return false;
  if (!FramebufferToGPU()) return false;

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_BLEND);

  glEnable(GL_TEXTURE_RECTANGLE_ARB);

  GLuint src_tex = p->src->Extended(LICE_EXT_GETFBOTEX_ACCEL, 0); // this binds the src's FBO
  bool src_has_tex = (src_tex > 0);
  if (src_has_tex)
  {
    if (!BindFBO()) return false; // re-bind dest FBO
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, src_tex);
  }
  else
  {
    // create texture from src bits
    glGenTextures(1, &src_tex); 
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, src_tex);

    int srcspan = p->src->getRowSpan();
    if (p->srcx == 0 && p->srcy == 0 && srcspan == p->srcw)
    {
      glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, p->srcw, p->srch, 0, GL_RGBA, GL_UNSIGNED_BYTE, p->src->getBits());
    }
    else
    {
      glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, p->srcw, p->srch, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);  // size the texture
      LICE_pixel* srcrow = p->src->getBits()+(int)p->srcy*srcspan+(int)p->srcx;
      int y;
      for (y = 0; y < p->srch; ++y, srcrow += srcspan)
      {
        glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 0, y, p->srcw, 1, GL_RGBA, GL_UNSIGNED_BYTE, srcrow);
      }
    }
  }

  if (!src_tex) return false;
  
  glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // filter
  glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  glBegin(GL_POLYGON);

  glTexCoord2i(p->srcx, p->srcy);
  glVertex2i(p->dstx, p->dsty);
  glTexCoord2i(p->srcx+p->srcw, p->srcy);
  glVertex2i(p->dstx+p->dstw, p->dsty);
  glTexCoord2i(p->srcx+p->srcw, p->srcy+p->srch);
  glVertex2i(p->dstx+p->dstw, p->dsty+p->dsth);
  glTexCoord2i(p->srcx, p->srcy+p->srch);
  glVertex2i(p->dstx, p->dsty+p->dsth);

  glEnd();

  if (!src_has_tex) glDeleteTextures(1, &src_tex);

  glDisable(GL_TEXTURE_RECTANGLE_ARB);

  return true;
}

bool LICE_GLBitmap::GetPixel_accel(LICE_Ext_GetPixel_acceldata* p)
{
  if (!p) return false;
  if (!FramebufferToGPU()) return false;

  p->px = 0;
  glReadPixels(p->x, p->y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &p->px);

  return true;
}

bool LICE_GLBitmap::PutPixel_accel(LICE_Ext_PutPixel_acceldata* p)
{
  if (!p) return false;
  if (!FramebufferToGPU()) return false;

  SetGLColor(p->color, p->alpha, p->mode);

  glBegin(GL_POINTS);
  glVertex2i(p->x, p->y);
  glEnd();

  return true;
}

bool LICE_GLBitmap::DrawTriangle_accel(LICE_Ext_DrawTriangle_acceldata *p)
{
  if (!p) return false;
  if (!FramebufferToGPU()) return false;

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_BLEND);

  // todo: finish implementing zbuffering, (multi)texture mapping, alpha, etc
  glColor4f(p->VertexShades[0][0],
            p->VertexShades[0][1],
            p->VertexShades[0][2],
            p->mat->SolidOpacity);

  glBegin(GL_TRIANGLES);

  int x;
  for(x=0;x<3;x++)
  {
    glVertex2i(p->scrx[x], p->scry[x]);
  }

  glEnd();

  return true;
}

bool LICE_GLBitmap::WindowBlit(LICE_Ext_WindowBlit_data* p)
{
  if (!p) return false;
  if (!FramebufferToGPU()) return false;

  HWND hwnd = p->hwnd;
  if (hwnd != LICE_GL_GetWindow()) return 0;

  glFinish();

  //HDC dc = GetDC(hwnd);
  //HGLRC rc = wglCreateContext(dc);
  //wglMakeCurrent(dc, rc);

  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

/*
  glViewport(0, 0, w, h);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluOrtho2D(0.0, w, 0.0, h);
*/
  glEnable(GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, m_tex);

  glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST); 
  glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

  // the src image is upside down
  // todo positioning
  glBegin(GL_POLYGON);
  glTexCoord2i(0, p->h);
  glVertex2i(0, 0);
  glTexCoord2i(p->w, p->h);
  glVertex2i(p->w, 0);
  glTexCoord2i(p->w, 0);
  glVertex2i(p->w-1, p->h);
  glTexCoord2i(0, 0);
  glVertex2i(0, p->h);
  glEnd();
  glDisable(GL_TEXTURE_RECTANGLE_ARB);

//wglMakeCurrent(0, 0);
//wglDeleteContext(rc);

// ReleaseDC(hwnd, dc);

  return true;
}
