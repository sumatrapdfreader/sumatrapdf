#include "lice.h"
#include "lice_extended.h"

// interface class for LICE_GL_SysBitmap and LICE_GL_MemBitmap
class LICE_GLBitmap : public LICE_IBitmap
{
public:

  LICE_GLBitmap();
  ~LICE_GLBitmap();

  LICE_pixel* getBits();

  int getWidth();
  int getHeight();
  int getRowSpan();
  
  bool resize(int w, int h);

  virtual HDC getDC() = 0;  // re-virtualize this to prevent instantiating LICE_GLBitmap directly

  INT_PTR Extended(int id, void* data);

private:

  bool Clear_accel(LICE_pixel* color);
  bool Line_accel(LICE_Ext_Line_acceldata* p);
  bool FillRect_accel(LICE_Ext_FillRect_acceldata* p);  
  bool DrawCBezier_accel(LICE_Ext_DrawCBezier_acceldata* p);
  bool DrawGlyph_accel(LICE_Ext_DrawGlyph_acceldata* p);
  bool Blit_accel(LICE_Ext_Blit_acceldata* p);
  bool ScaledBlit_accel(LICE_Ext_ScaledBlit_acceldata* p);
  bool DashedLine_accel(LICE_Ext_DashedLine_acceldata* p);
  bool GetPixel_accel(LICE_Ext_GetPixel_acceldata* p);
  bool PutPixel_accel(LICE_Ext_PutPixel_acceldata* p);
  bool SetClip_ext(LICE_Ext_SetClip_data* p);
  bool DrawTriangle_accel(LICE_Ext_DrawTriangle_acceldata *p);
  // etc 

  bool WindowBlit(LICE_Ext_WindowBlit_data* p);

  bool CreateFBO(int w, int h);
  bool BindFBO(); // bind this FBO so it is the current rendering target, and test for validity
  void ReleaseFBO();

  unsigned int m_fbo;  // framebuffer object: rendering target for drawing on this glbitmap
  unsigned int m_tex;  // texture object: backing store for this framebuffer object

  enum { EMPTY, INGPU, INMEM };
  int m_bufloc; // where is the current framebuffer?
  LICE_IBitmap* m_bmp;

protected:

  void Init(LICE_IBitmap* bmp, int w, int h);
  bool FramebufferToGPU();
  void FramebufferFromGPU();
};

class LICE_GL_SysBitmap : public LICE_GLBitmap
{
public:

  LICE_GL_SysBitmap(int w=0, int h=0);

  HDC getDC();

private:

  LICE_SysBitmap m_sysbmp;
};


class LICE_GL_MemBitmap : public LICE_GLBitmap
{
public:

  LICE_GL_MemBitmap(int w=0, int h=0);

  HDC getDC() { return 0; }

private:

  LICE_MemBitmap m_membmp;
};


class LICE_GL_SubBitmap : public LICE_IBitmap
{
public:

  LICE_GL_SubBitmap(LICE_IBitmap *parent, int x, int y, int w, int h);

  LICE_pixel* getBits();

  int getWidth() { return m_w; }
  int getHeight() { return m_h; }
  int getRowSpan() { return (m_parent ? m_parent->getRowSpan() : 0); }
  
  bool resize(int w, int h);

  HDC getDC() { return (m_parent ? m_parent->getDC() : 0); }

  INT_PTR Extended(int id, void* data);

private:

  LICE_IBitmap* m_parent;
  int m_x, m_y, m_w, m_h;
};
