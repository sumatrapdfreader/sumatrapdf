/*
  Cockos WDL - LICE - Lightweight Image Compositing Engine
  Copyright (C) 2007 and later, Cockos Incorporated
  File: lice_svg.cpp (SVG support for LICE)
  See lice.h for license and other information
*/


#ifndef _WIN32
#include "../swell/swell.h"
#endif
#include "lice.h"
#include "lice_text.h"

#include <stdio.h>
#include <assert.h>
#include <math.h>

#include "../tinyxml/tinyxml.h"
#include "../wdlcstring.h"

extern "C" int LICE_RGBA_from_SVG(const char* s, int len);


const double SVG_ID_MAT[6] = { 1.0, 0.0, 0.0, 0.0, 1.0, 0.0 };  // acebdf
const int SVG_MAT_SZ = sizeof(SVG_ID_MAT);

#define SVGINT(x) ((int)(x+0.5))

static void SVGMSet(double m[], double a, double b, double c, double d, double e, double f)
{
  m[0] = a;
  m[3] = b;
  m[1] = c;
  m[4] = d;
  m[2] = e;
  m[5] = f;
}

static void SVGMMult(double m[], double a, double b, double c, double d, double e, double f)
{
  SVGMSet(m, 
    m[0]*a+m[1]*b,        // 0  
    m[3]*a+m[4]*b,        // 3
    m[0]*c+m[1]*d,        // 1
    m[3]*c+m[4]*d,        // 4
    m[0]*e+m[1]*f+m[2],   // 2
    m[3]*e+m[4]*f+m[5]);  // 5
}

static void SVGMTransform(double m[], double* x, double* y)
{
  if (memcmp(m, SVG_ID_MAT, SVG_MAT_SZ))
  {
    double tx = *x;
    double ty = *y;
    *x = tx*m[0]+ty*m[1]+m[2];
    *y = tx*m[3]+ty*m[4]+m[5];
  }
}

static void SVGMScale(double m[], double* w, double* h)
{
  if (memcmp(m, SVG_ID_MAT, SVG_MAT_SZ))
  {
    *w *= m[0];
    *h *= m[4];
  }
}

static void SVGDrawLine(LICE_IBitmap* bmp, double x1, double y1, double x2, double y2, LICE_pixel color, float alpha, bool aa, int linewid=1)
{
  int i;
  for (i = 0; i < linewid; ++i)
  {
    LICE_FLine(bmp, x1, y1, x2, y2, color, alpha, LICE_BLIT_MODE_COPY, aa);

    if (fabs(y2-y1) > fabs(x2-x1)) 
    {
      x1 += 1.0;
      x2 += 1.0;
    }
    else
    {
      y1 += 1.0;
      y2 += 1.0;
    }
  }
}

static void SVGDrawRect(LICE_IBitmap* bmp, double x, double y, int w, int h, LICE_pixel color, float alpha, int linewid=1)
{
  int xi = SVGINT(x), yi = SVGINT(y);
  int wi = SVGINT(w), hi = SVGINT(h);

  int i;
  for (i = 0; i < linewid; ++i)
  {
    LICE_DrawRect(bmp, xi, yi, wi, hi, color, alpha, LICE_BLIT_MODE_COPY);
    ++xi;
    ++yi;
  }
}

static void SVGDrawCircle(LICE_IBitmap* bmp, double cx, double cy, double r, LICE_pixel color, float alpha, bool aa, int linewid=1)
{
  int i;
  for (i = 0; i < linewid; ++i)
  {
    LICE_Circle(bmp, cx, cy, r, color, alpha, LICE_BLIT_MODE_COPY, aa);
    r += 1.0f;
  }
}

static void SVGDrawQBezier(LICE_IBitmap* bmp, double x1, double y1, double x2, double y2, double x3, double y3,
  LICE_pixel color, float alpha, bool aa, int linewid=1)
{
  int i;
  for (i = 0; i < linewid; ++i)
  {
    LICE_DrawQBezier(bmp, x1, y1, x2, y2, x3, y3, color, alpha, LICE_BLIT_MODE_COPY, aa);

    if (fabs(y3-y1) > fabs(x3-x1)) 
    {
      x1 += 1.0;
      x2 += 1.0;
      x3 += 1.0;
    }
    else
    {
      y1 += 1.0;
      y2 += 1.0;
      y3 += 1.0;
    }
  }
}

static void SVGDrawCBezier(LICE_IBitmap* bmp, double x1, double y1, double x2, double y2, double x3, double y3, double x4, double y4,
  LICE_pixel color, float alpha, bool aa, int linewid=1)
{
  int i;
  for (i = 0; i < linewid; ++i)
  {
    LICE_DrawCBezier(bmp, x1, y1, x2, y2, x3, y3, x4, y4, color, alpha, LICE_BLIT_MODE_COPY, aa);

    if (fabs(y4-y1) > fabs(x4-x1)) 
    {
      x1 += 1.0;
      x2 += 1.0;
      x3 += 1.0;
      x4 += 1.0;
    }
    else
    {
      y1 += 1.0;
      y2 += 1.0;
      y3 += 1.0;
      y4 += 1.0;
    }
  }
}

class LICE_SVGState
{
public:

  LICE_IBitmap* m_bmp;
  LICE_IBitmap* m_tmpbmp;
  double m_x, m_y;

  LICE_pixel m_strokecolor;
  float m_strokealpha;
  int m_strokewidth;

  LICE_pixel m_fillcolor;
  float m_fillalpha;

  LICE_CachedFont* m_font;
  LOGFONT m_logfont;
  bool m_fontdirty;
  int m_textflags;

  double m_ctm[6];  // current transform matrix

  LICE_SVGState(LICE_IBitmap* bmp);
  ~LICE_SVGState();

  void SetXY(double x, double y);

  void DrawText(LICE_IBitmap* bmp, const char* str);

  bool ParseNode(TiXmlNode* xmlnode);
  bool ParseLine(TiXmlElement* xmlelem);
  bool ParseRect(TiXmlElement* xmlelem);
  bool ParseCircle(TiXmlElement* xmlelem);
  bool ParsePolyline(TiXmlElement* xmlelem, bool close);
  bool ParseText(TiXmlElement* xmlelem);

  bool ParsePosition(TiXmlElement* xmlelem);
  bool ParseTransform(TiXmlElement* xmlelem, double prevctm[]);
  bool ParseColor(TiXmlElement* xmlelem);
  bool ParseFont(TiXmlElement* xmlelem);

  bool ParsePath(TiXmlElement* xmlelem);
};


LICE_SVGState::LICE_SVGState(LICE_IBitmap* bmp)
{
  m_bmp = bmp;
  m_tmpbmp = 0;

  m_x = 0.0;
  m_y = 0.0;

  m_strokecolor = 0;
  m_strokealpha = 1.0f;
  m_strokewidth = 1;

  m_fillcolor = 0;
  m_fillalpha = 1.0f;

  m_font = 0;
  memset(&m_logfont, 0, sizeof(LOGFONT));
  m_fontdirty = true;
  m_textflags = 0;

  memcpy(m_ctm, SVG_ID_MAT, SVG_MAT_SZ);
}

LICE_SVGState::~LICE_SVGState()
{
  delete(m_tmpbmp);
  delete(m_font);
}

void LICE_SVGState::SetXY(double x, double y)
{
  m_x = x;
  m_y = y;
}

void LICE_SVGState::DrawText(LICE_IBitmap* bmp, const char* str)
{
  if (!m_font) m_font = new LICE_CachedFont;

  if (m_fontdirty)
  {
    m_fontdirty = false;
    HFONT hf = CreateFontIndirect(&m_logfont);
    m_font->SetFromHFont(hf, LICE_FONT_FLAG_OWNS_HFONT);
  }
  m_font->SetTextColor(m_fillcolor);

  RECT r = { SVGINT(m_x), SVGINT(m_y), SVGINT(m_x), SVGINT(m_y) };
  m_font->DrawText(m_bmp, str, -1, &r, m_textflags|DT_NOCLIP);
}

static const char* GetSVGStyleStr(const char* str, const char* name, int* len)
{
  const char* s = strstr(str, name);
  if (s)
  {
    s += strlen(name);
    while (*s == ' ') ++s;
    int i;
    const int MAXLEN = 256;
    for (i = 0; i < MAXLEN && s[i] && s[i] != ';'; ++i) 
    {
      // run through
    }
    if (i && i < MAXLEN) *len = i;
    else s = 0;
  }
  return s;
}

bool LICE_SVGState::ParseTransform(TiXmlElement* xmlelem, double prevctm[])
{
  const char* str = xmlelem->Attribute("transform");
  while (str && *str == ' ') ++str;

  int ntrans = 0;
  while (str && *str)
  {
    double a = 1.0, b = 0.0, c = 0.0, d = 1.0, e = 0.0, f = 0.0;
    int n = 0;

    bool ok = (sscanf(str, "matrix(%lf,%lf,%lf,%lf,%lf,%lf) %n", &a, &b, &c, &d, &e, &f, &n) == 6);
    if (!ok) ok = (sscanf(str, "translate(%lf,%lf) %n", &e, &f, &n) == 2);
    if (!ok) ok = (sscanf(str, "scale(%lf,%lf) %n", &a, &d, &n) == 2);
    // todo rotate, skew
    if (!ok || !n) break;

    if (!ntrans++) memcpy(prevctm, m_ctm, SVG_MAT_SZ);
    SVGMMult(m_ctm, a, b, c, d, e, f);
    str += n;
    while (*str == ' ') ++str;
  }

  return !!ntrans;
}

bool LICE_SVGState::ParsePosition(TiXmlElement* xmlelem)
{
  double xf;
  double yf;
  if (xmlelem->Attribute("x", &xf) && xmlelem->Attribute("y", &yf))
  {
    SVGMTransform(m_ctm, &xf, &yf);
    SetXY(xf, yf);
  }
  return true;
}

bool LICE_SVGState::ParseColor(TiXmlElement* xmlelem)
{
  const char* s;
  int len;

  const char* str = xmlelem->Attribute("style");
  if (str)
  {
    s = GetSVGStyleStr(str, "fill:", &len);
    if (s) m_fillcolor = LICE_RGBA_from_SVG(s, len);
    else m_fillcolor = 0;

    s = GetSVGStyleStr(str, "stroke:", &len);
    if (s) m_strokecolor = LICE_RGBA_from_SVG(s, len);
    else m_strokecolor = 0;

    s = GetSVGStyleStr(str, "stroke-width:", &len);
    if (s && *s >= '0' && *s <= '9') m_strokewidth = *s-'0';
    else m_strokewidth = 1;
  } 
  return true;
}

bool LICE_SVGState::ParseFont(TiXmlElement* xmlelem)
{
  const char* s;
  int len;

  m_textflags = DT_LEFT|DT_TOP;

  const char* str = xmlelem->Attribute("style");
  if (str)
  {
    s = GetSVGStyleStr(str, "font-family:", &len);
    if (s) 
    {
      lstrcpyn_safe(m_logfont.lfFaceName, s, len);
      m_fontdirty = true;
    }

    s = GetSVGStyleStr(str, "font-size:", &len);
    if (s)
    {
      double sz = atof(s);
      if (sz > 0.0)
      {
        double xsz = sz, ysz = sz;
        SVGMScale(m_ctm, &xsz, &ysz);
        sz = lice_max(xsz, ysz);
        m_logfont.lfHeight = -abs((int)sz);
        m_fontdirty = true;
      }
    }

    s = GetSVGStyleStr(str, "text-align:", &len);
    if (s)
    {
      if (!strnicmp(s, "center", len))
      {
        m_textflags &= ~(DT_LEFT|DT_TOP);
        m_textflags |= (DT_CENTER|DT_VCENTER);
      }
      else if (!strnicmp(s, "start", len))
      {
        m_textflags &= ~DT_TOP;
        m_textflags |= DT_BOTTOM;
      }
    }
  }

  return true;
}

bool LICE_SVGState::ParseText(TiXmlElement* xmlelem)
{
  bool ok = true;

  ParseFont(xmlelem);

  const char* str = xmlelem->GetText();
  if (str && *str) DrawText(m_bmp, str);

  TiXmlElement* xmlchild;
  for (xmlchild = xmlelem->FirstChildElement("tspan"); xmlchild; xmlchild = xmlchild->NextSiblingElement("tspan"))
  {
    // no transform for tspan
    ParsePosition(xmlchild);
    ParseColor(xmlchild);
    if (!ParseText(xmlchild)) ok = false;
  }
 
  return ok;
}

bool LICE_SVGState::ParseRect(TiXmlElement* xmlelem)
{
  bool ok = false;
  double w, h;

  double rx = 0.0, ry = 0.0;
  xmlelem->Attribute("rx", &rx);
  xmlelem->Attribute("ry", &ry);

  ok = (xmlelem->Attribute("width", &w) && xmlelem->Attribute("height", &h));

  if (ok)
  {
    SVGMScale(m_ctm, &w, &h);
    SVGMScale(m_ctm, &rx, &ry);

    if (m_fillcolor && m_fillalpha > 0.0f)
    {
      LICE_FillRect(m_bmp, SVGINT(m_x), SVGINT(m_y), SVGINT(w), SVGINT(h), m_fillcolor, m_fillalpha, LICE_BLIT_MODE_COPY);
    }
    if (m_strokecolor && m_strokealpha > 0.0f)
    {
      if (rx > 0.0)
      {
        LICE_RoundRect(m_bmp, SVGINT(m_x), SVGINT(m_y), SVGINT(w), SVGINT(h), (int) rx, m_strokecolor, m_strokealpha, LICE_BLIT_MODE_COPY, true);
      }
      else
      {
        SVGDrawRect(m_bmp, m_x, m_y, w, h, m_strokecolor, m_strokealpha, m_strokewidth);
      }
    }
  }

  return ok;
}

bool LICE_SVGState::ParseCircle(TiXmlElement* xmlelem)
{
  bool ok = false;

  double cx, cy, r = 0.0;
  ok = !!xmlelem->Attribute("cx", &cx);
  if (ok) ok = !!xmlelem->Attribute("cy", &cy);
  if (ok) ok = (!!xmlelem->Attribute("r", &r) && r > 0.0);

  if (ok)
  {
    SVGMTransform(m_ctm, &cx, &cy);
    r = 0.5*floor(r*2.0+0.5);

    if (m_fillcolor && m_fillalpha > 0.0f)
    {
      LICE_FillCircle(m_bmp, cx, cy, r, m_fillcolor, m_fillalpha, LICE_BLIT_MODE_COPY, true);
    }
    if (m_strokecolor && m_strokealpha > 0.0f)
    {
      //LICE_Circle(m_bmp, cx, cy, r, m_strokecolor, m_strokealpha, LICE_BLIT_MODE_COPY, true);
      SVGDrawCircle(m_bmp, cx, cy, r, m_strokecolor, m_strokealpha, true, m_strokewidth);
    }
  }

  return ok;
}

bool LICE_SVGState::ParseLine(TiXmlElement* xmlelem)
{
  bool ok = false;
  double x1, y1, x2, y2;

  ok = (xmlelem->Attribute("x1", &x1) && xmlelem->Attribute("y1", &y1)
   && xmlelem->Attribute("x2", &x2) && xmlelem->Attribute("y2", &y2));

  if (ok)
  {
    SVGMTransform(m_ctm, &x1, &y1);
    SVGMTransform(m_ctm, &x2, &y2);
    //LICE_FLine(m_bmp, x1, y1, x2, y2, m_strokecolor, m_strokealpha, LICE_BLIT_MODE_COPY, true);
    SVGDrawLine(m_bmp, x1, y1, x2, y2, m_strokecolor, m_strokealpha, true, m_strokewidth);
    SetXY(x2, y2);
  }

  return ok;
}

bool LICE_SVGState::ParsePolyline(TiXmlElement* xmlelem, bool close)
{
  bool ok = false;

  double firstx = m_x;
  double firsty = m_y;
  int npts = 0;

  const char* str = xmlelem->Attribute("points");
  while (str && *str == ' ') ++str;
  
  while (str && *str)
  {    
    double x, y;
    int n = 0;

    ok = (sscanf(str, "%lf,%lf %n", &x, &y, &n) == 2);
    if (ok)
    {
      SVGMTransform(m_ctm, &x, &y);
      if (!npts++) 
      {
        firstx = x;
        firsty = y;
      }
      else 
      {
        //LICE_FLine(m_bmp, m_x, m_y, x, y, m_strokecolor, m_strokealpha, LICE_BLIT_MODE_COPY, true);
        SVGDrawLine(m_bmp, m_x, m_y, x, y, m_strokecolor, m_strokealpha, true, m_strokewidth);
      }
      SetXY(x, y);
    }

    if (!ok || !n) break;
    str += n;
    while (*str == ' ') ++str;
  }

  if (close && npts)
  {
    //LICE_FLine(m_bmp, m_x, m_y, firstx, firsty, m_strokecolor, m_strokealpha, LICE_BLIT_MODE_COPY, true);
    SVGDrawLine(m_bmp, m_x, m_y, firstx, firsty, m_strokecolor, m_strokealpha, true, m_strokewidth);
    SetXY(firstx, firsty);
  }

  return ok;
}

static void AdjRelXY(double* lastx, double* lasty, double* x, double* y, double m[], bool isrel, RECT* r)
{
  if (isrel)
  {
    *x += *lastx;
    *y += *lasty;
  }
  *lastx = *x;
  *lasty = *y;
  SVGMTransform(m, x, y);

  if (r)
  {
    if (r->left < 0 || *x < r->left) r->left = *x-2;
    if (r->right < 0 || *x >= r->right) r->right = *x+3;
    if (r->top < 0 || *y < r->top) r->top = *y-2;
    if (r->bottom < 0 || *y >= r->bottom) r->bottom = *y+3;
  }
}

static bool RenderSVGPath(const char* str, double m[], RECT* r, 
  LICE_IBitmap* bmp=0, LICE_pixel color=0xFFFFFFFF, float alpha=1.0f, bool aa=false, int strokewid=1,
  double* lastx=0, double* lasty=0)
{
  bool allok = true;
  
  double firstx, firsty;  // first point on subpath, global coords
  double prevx, prevy;    // previous point, local coords
  double x0, y0;          // point at end of previous move, global coords

  while (str && *str)
  {
    double x1, y1, x2, y2, x3, y3;
    int n = 0;
    bool ok = false;

    char c = str[0];
    bool isrel = (c == tolower(c));
    ++str;
    while (*str == ' ') ++str;

    switch (toupper(c))
    {
      case 'M':
      case 'L':
        ok = (sscanf(str, "%lf,%lf %n", &x1, &y1, &n) == 2);
        if (ok)
        {          
          AdjRelXY(&prevx, &prevy, &x1, &y1, m, isrel, r);
          if (c == 'M')
          {
            firstx = x1;
            firsty = y1;
          }
          else if (bmp)
          {
            //LICE_FLine(bmp, x0, y0, x1, y1, color, alpha, LICE_BLIT_MODE_COPY, aa);
            SVGDrawLine(bmp, x0, y0, x1, y1, color, alpha, aa, strokewid);
          }
          x0 = x1;
          y0 = y1;          
        }
      break;

      case 'Z':
        ok = true;
        --str;
        n = 1;
        if (ok)
        {
          if (bmp && x0 != firstx && y0 != firsty)
          {
            //LICE_FLine(bmp, x0, y0, firstx, firsty, color, alpha, LICE_BLIT_MODE_COPY, aa);
            SVGDrawLine(bmp, x0, y0, firstx, firsty, color, alpha, aa, strokewid);
            x0 = firstx;
            y0 = firsty;
          }
        }
      break;

      case 'Q':
        ok = (sscanf(str, "%lf,%lf %lf,%lf %n", &x1, &y1, &x2, &y2, &n) == 4);
        if (ok)
        {
          AdjRelXY(&prevx, &prevy, &x1, &y1, m, isrel, r);
          AdjRelXY(&prevx, &prevy, &x2, &y2, m, isrel, r);
          if (bmp)
          {
            //LICE_DrawQBezier(bmp, x0, y0, x1, y1, x2, y2, color, alpha, LICE_BLIT_MODE_COPY, aa);
            SVGDrawQBezier(bmp, x0, y1, x1, y1, x2, y2, color, alpha, aa, strokewid);
            x0 = x2;
            y0 = y2;
          }         
        }
      break;

      case 'C':
        ok = (sscanf(str, "%lf,%lf %lf,%lf %lf,%lf %n", &x1, &y1, &x2, &y2, &x3, &y3, &n) == 6);
        if (ok)
        {
          AdjRelXY(&prevx, &prevy, &x1, &y1, m, isrel, r);
          AdjRelXY(&prevx, &prevy, &x2, &y2, m, isrel, r);
          AdjRelXY(&prevx, &prevy, &x3, &y3, m, isrel, r);
          if (bmp)
          {
            //LICE_DrawCBezier(bmp, x0, y0, x1, y1, x2, y2, x3, y3, color, alpha, LICE_BLIT_MODE_COPY, aa);
            SVGDrawCBezier(bmp, x0, y0, x1, y1, x2, y2, x3, y3, color, alpha, aa, strokewid);
            x0 = x3;
            y0 = y3;
          }
        }
      break;
    }

    if (!ok) allok = false;
    if (!ok || !n) break;    
    str += n;
    while (*str == ' ') ++str;
  }

  if (lastx) *lastx = x0;
  if (lasty) *lasty = y0;
  return allok;
}

static bool FillSVGScanLine(LICE_IBitmap* dest, LICE_pixel* rowpx, LICE_pixel col, float alpha, RECT* r, int y, int dir)
{
  bool prevon = false;
  bool dofill = false;

  int i, x, xstart;
  int w = r->right-r->left;
  for (i = 0; i < w; ++i)
  {
    x = (dir < 0 ? w-i-1 : i);
    bool curon = !!rowpx[x];
    if (curon != prevon)
    {
      if (!curon)
      {
        xstart = x;
      }
      else
      {
        if (dofill)
        {
          LICE_Line(dest, r->left+xstart/*+dir*/, y, r->left+x/*-dir*/, y, col, alpha, LICE_BLIT_MODE_COPY, false);
        }
        dofill = !dofill;
      }
      prevon = curon;
    }
  }

  return !dofill; // if we ended before hitting the end of a fill, we nicked an edge
}

static bool FillSVGShape(LICE_IBitmap* dest, LICE_IBitmap* src, LICE_pixel col, float alpha, RECT *r)
{
  bool ok = true;
  
  LICE_pixel* rowpx = src->getBits();
  int span = src->getRowSpan();

  int y;
  for (y = r->top; y < r->bottom; ++y, rowpx += span)
  {
    if (!FillSVGScanLine(dest, rowpx, col, alpha, r, y, 1) && 
      !FillSVGScanLine(dest, rowpx, col, alpha, r, y, -1))
    {      
      ok = false;     
    }
  }

  return ok;
}

bool LICE_SVGState::ParsePath(TiXmlElement* xmlelem)
{
  bool ok = true;

  const char* str = xmlelem->Attribute("d");
  while (str && *str == ' ') ++str;

  if (m_fillcolor && m_fillalpha > 0.0f)  // fill
  {
    RECT r = { -1, -1, -1, -1 };  // bounding box
    if (ok) ok = RenderSVGPath(str, m_ctm, &r);  // measure

    double m[6];
    SVGMSet(m, 1.0, 0.0, 0.0, 1.0, -(double)r.left, -(double)r.top);  // translate to tmpbmp on matrix LHS
    SVGMMult(m, m_ctm[0], m_ctm[3], m_ctm[1], m_ctm[4], m_ctm[2], m_ctm[5]);

    if (!m_tmpbmp) m_tmpbmp = new LICE_MemBitmap;
    m_tmpbmp->resize(r.right-r.left, r.bottom-r.top);
    LICE_Clear(m_tmpbmp, 0);

    if (ok) ok = RenderSVGPath(str, m, 0, m_tmpbmp);  // outline to tmpbmp
    // debug LICE_Blit(m_bmp, m_tmpbmp, r.left, r.top, 0, 0, r.right-r.left, r.bottom-r.top, 1.0f, LICE_BLIT_MODE_COPY|LICE_BLIT_USE_ALPHA);
    if (ok) ok = FillSVGShape(m_bmp, m_tmpbmp, m_fillcolor, m_fillalpha, &r);  // raytrace to dest
  }

  if (m_strokecolor && m_strokealpha > 0.0f)  // outline
  {
    if (ok) ok = RenderSVGPath(str, m_ctm, 0, m_bmp, m_strokecolor, m_strokealpha, true, m_strokewidth, &m_x, &m_y);
  }

  return ok;
}

bool LICE_SVGState::ParseNode(TiXmlNode* xmlnode)
{
  bool allok = true;

  TiXmlNode* xmlchild;
  for (xmlchild = xmlnode->FirstChild(); xmlchild; xmlchild = xmlchild->NextSibling())
  {
    if (xmlchild->Type() == TiXmlElement::ELEMENT)
    {
      bool ok = true;

      const char* name = xmlchild->Value();
      TiXmlElement* xmlelem = xmlchild->ToElement();

      double prevctm[6];
      bool transform = ParseTransform(xmlelem, prevctm);

      ParsePosition(xmlelem);
      ParseColor(xmlelem);

      if (!stricmp(name, "svg") || !stricmp(name, "g")) ok = ParseNode(xmlelem);
      else if (!stricmp(name, "rect")) ok = ParseRect(xmlelem);      
      else if (!stricmp(name, "circle")) ok = ParseCircle(xmlelem);
      else if (!stricmp(name, "line")) ok = ParseLine(xmlelem);      
      else if (!stricmp(name, "polyline")) ok = ParsePolyline(xmlelem, false);
      else if (!stricmp(name, "polygon")) ok = ParsePolyline(xmlelem, true);
      else if (!stricmp(name, "path")) ok = ParsePath(xmlelem);
      else if (!stricmp(name, "text") || !stricmp(name, "tspan")) ok = ParseText(xmlelem);
      // it's not an error not to recognize a tag

      if (!ok) 
      {
        allok = false;
        // log error and continue
      }

      if (transform) memcpy(m_ctm, prevctm, SVG_MAT_SZ);
    }
  }

  return true; //allok;
}
        
static LICE_IBitmap *LICE_RenderSVG(TiXmlDocument* xmldoc, LICE_IBitmap *bmp)
{
  if (!xmldoc) return 0;

  TiXmlElement* xmlroot = xmldoc->RootElement();
  if (!xmlroot || stricmp(xmlroot->Value(), "svg")) return 0;
  
  int srcw = 0;
  int srch = 0;
  if (!xmlroot->Attribute("width", &srcw) || !xmlroot->Attribute("height", &srch)) return 0;
  if (!srcw || !srch) return 0;

  bool ourbmp = !bmp;
  if (ourbmp) bmp = new LICE_MemBitmap;
  LICE_SVGState svgstate(bmp);

/*
  int destw = bmp->getWidth();
  int desth = bmp->getHeight();

  if (destw && desth) 
  {
    double xscale = (double)destw/(double)srcw;
    double yscale = (double)desth/(double)srch;
    SVGMSet(svgstate.m_ctm, xscale, 0.0, 0.0, yscale, 0.0, 0.0);
  }
  else
*/
  {
    bmp->resize(srcw, srch);
  }
  LICE_Clear(bmp, 0);
  
  if (!svgstate.ParseNode(xmlroot) && ourbmp)
  {
    delete(bmp);
    bmp = 0;
  }
  return bmp;
}

LICE_IBitmap* LICE_LoadSVG(const char* filename, LICE_IBitmap* bmp)
{
  TiXmlDocument xmldoc;
  xmldoc.SetCondenseWhiteSpace(false);
  if (!xmldoc.LoadFile(filename) || xmldoc.Error()) return 0;
  return LICE_RenderSVG(&xmldoc, bmp);
}

LICE_IBitmap* LICE_LoadSVGFromBuffer(const char* buffer, int buflen, LICE_IBitmap* bmp)
{
  TiXmlDocument xmldoc;
  xmldoc.SetCondenseWhiteSpace(false);
  if (!xmldoc.Parse(buffer) || xmldoc.Error()) return 0;
  return LICE_RenderSVG(&xmldoc, bmp);
}

