#ifndef _LICE_EXTENDED_
#define _LICE_EXTENDED_

#include "lice.h"

#define DISABLE_LICE_EXTENSIONS

// stuff to pass to LICE_IBitmap::Extended

enum // IDs
{
  LICE_EXT_SUPPORTS_ID,   // data = ID, returns 1 if that extension ID is supported
  LICE_EXT_CLEAR_ACCEL,
  LICE_EXT_LINE_ACCEL, 
  LICE_EXT_FILLRECT_ACCEL,
  LICE_EXT_DRAWCBEZIER_ACCEL,
  LICE_EXT_DRAWGLYPH_ACCEL,
  LICE_EXT_BLIT_ACCEL,
  LICE_EXT_SCALEDBLIT_ACCEL,
  LICE_EXT_GETFBOTEX_ACCEL, // if the bitmap is implemented as an openGL framebuffer object, get its texture backing store
  LICE_EXT_DASHEDLINE_ACCEL,
  LICE_EXT_GETPIXEL_ACCEL,
  LICE_EXT_PUTPIXEL_ACCEL,
  LICE_EXT_SETCLIP, // data == 0 to clear clip
  LICE_EXT_WINDOW_BLIT,
  LICE_EXT_FORGET,  // optimizations can sometimes happen if a bitmap can be told it doesn't need to retain data after it's accessed
  LICE_EXT_DRAWTRIANGLE_ACCEL,
}; 

struct LICE_Ext_Line_acceldata
{
  float x1, y1, x2, y2;
  LICE_pixel color;
  float alpha; 
  int mode;
  bool aa; 

  LICE_Ext_Line_acceldata(float _x1, float _y1, float _x2, float _y2, LICE_pixel _color, float _alpha, int _mode, bool _aa)
  : x1(_x1), y1(_y1), x2(_x2), y2(_y2), color(_color), alpha(_alpha), mode(_mode), aa(_aa) {}
};

struct LICE_Ext_FillRect_acceldata
{
  int x, y, w, h;
  LICE_pixel color;
  float alpha; 
  int mode;

  LICE_Ext_FillRect_acceldata(int _x, int _y, int _w, int _h, LICE_pixel _color, float _alpha, int _mode)
  : x(_x), y(_y), w(_w), h(_h), color(_color), alpha(_alpha), mode(_mode) {}
};

struct LICE_Ext_DrawCBezier_acceldata
{
  float xstart, ystart, xctl1, yctl1, xctl2, yctl2, xend, yend;
  LICE_pixel color;
  float alpha;
  int mode;
  bool aa;

  LICE_Ext_DrawCBezier_acceldata(float _xstart, float _ystart, float _xctl1, float _yctl1, float _xctl2, float _yctl2, float _xend, float _yend, 
    LICE_pixel _color, float _alpha, int _mode, bool _aa)
  : xstart(_xstart), ystart(_ystart), xctl1(_xctl1), yctl1(_yctl1), xctl2(_xctl2), yctl2(_yctl2), xend(_xend), yend(_yend),
    color(_color), alpha(_alpha), mode(_mode), aa(_aa) {} 
};

struct LICE_Ext_DrawGlyph_acceldata
{
  int x;
  int y;
  LICE_pixel color;
  const LICE_pixel_chan* alphas;
  int glyph_w, glyph_h;
  float alpha;
  int mode;

  LICE_Ext_DrawGlyph_acceldata(int _x, int _y, LICE_pixel _color, LICE_pixel_chan* _alphas, int _glyph_w, int _glyph_h, float _alpha, int _mode)
    : x(_x), y(_y), color(_color), alphas(_alphas), glyph_w(_glyph_w), glyph_h(_glyph_h), alpha(_alpha), mode(_mode) {}
};

struct LICE_Ext_Blit_acceldata
{
  LICE_IBitmap* src;
  int dstx, dsty, srcx, srcy, srcw, srch;
  float alpha;
  int mode;

  LICE_Ext_Blit_acceldata(LICE_IBitmap* _src, int _dstx, int _dsty, int _srcx, int _srcy, int _srcw, int _srch, float _alpha, int _mode)
  : src(_src), dstx(_dstx), dsty(_dsty), srcx(_srcx), srcy(_srcy), srcw(_srcw), srch(_srch), alpha(_alpha), mode(_mode) {}  
};

struct LICE_Ext_ScaledBlit_acceldata
{
  LICE_IBitmap* src;
  int dstx, dsty, dstw, dsth;
  float srcx, srcy, srcw, srch;
  float alpha;
  int mode;

  LICE_Ext_ScaledBlit_acceldata(LICE_IBitmap* _src, int _dstx, int _dsty, int _dstw, int _dsth, float _srcx, float _srcy, float _srcw, float _srch, float _alpha, int _mode)
  :  src(_src), dstx(_dstx), dsty(_dsty), dstw(_dstw), dsth(_dsth), srcx(_srcx), srcy(_srcy), srcw(_srcw), srch(_srch), alpha(_alpha), mode(_mode) {}
};

struct LICE_Ext_DashedLine_acceldata
{
  float x1, y1, x2, y2;
  int pxon, pxoff;
  LICE_pixel color;
  float alpha; 
  int mode;
  bool aa; 

  LICE_Ext_DashedLine_acceldata(float _x1, float _y1, float _x2, float _y2, int _pxon, int _pxoff, LICE_pixel _color, float _alpha, int _mode, bool _aa)
  : x1(_x1), y1(_y1), x2(_x2), y2(_y2), pxon(_pxon), pxoff(_pxoff), color(_color), alpha(_alpha), mode(_mode), aa(_aa) {}
};

struct LICE_Ext_GetPixel_acceldata
{
  int x, y;
  LICE_pixel px;  // return

  LICE_Ext_GetPixel_acceldata(int _x, int _y)
  : x(_x), y(_y), px(0) {}
};

struct LICE_Ext_PutPixel_acceldata
{
  int x, y;
  LICE_pixel color;
  float alpha; 
  int mode;

  LICE_Ext_PutPixel_acceldata(int _x, int _y, LICE_pixel _color, float _alpha, int _mode)
  : x(_x), y(_y), color(_color), alpha(_alpha), mode(_mode) {}
};

struct LICE_Ext_SetClip_data
{
  int x, y, w, h;

  LICE_Ext_SetClip_data(int _x, int _y, int _w, int _h)
  : x(_x), y(_y), w(_w), h(_h) {}
};

class pl_Mat;

struct LICE_Ext_DrawTriangle_acceldata
{
  pl_Mat *mat; // will need to include plush.h to access this
  double VertexShades[3][3]; // for solid element
  float scrx[3], scry[3], scrz[3]; // scrz = 1/Zdist
  double mapping_coords[2][3][2]; // [texture or texture2][vertex][uv]
};

struct LICE_Ext_WindowBlit_data
{
  HWND hwnd;
  int destx, desty, srcx, srcy, w, h;

  LICE_Ext_WindowBlit_data(HWND _hwnd, int _destx, int _desty, int _srcx, int _srcy, int _w, int _h)
 : hwnd(_hwnd), destx(_destx), desty(_desty), srcx(_srcx), srcy(_srcy), w(_w), h(_h) {}
};

#endif
