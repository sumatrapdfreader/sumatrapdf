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
  

    This file provides basic win32 GDI-->Quartz translation. It uses features that require OS X 10.4+

*/

#ifndef SWELL_PROVIDED_BY_APP

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#import <CoreFoundation/CFDictionary.h>
#import <objc/objc-runtime.h>
#include "swell.h"
#include "swell-internal.h"

#include "../mutex.h"
#include "../assocarray.h"
#include "../wdlcstring.h"

#ifdef __SSE__
#include <xmmintrin.h>
#endif

#ifdef SWELL_SUPPORT_OPENGL_BLIT
#include <OpenGL/gl.h>
#endif

#ifndef SWELL_NO_METAL
void SWELL_Metal_FillRect(void *_tex, int x, int y, int w, int h, int color);
#endif

// reimplement here so that swell-gdi isn't dependent on swell-misc, and vice-versa
static int SWELL_GDI_GetOSXVersion()
{
  static SInt32 v;
  if (!v)
  {
    if (NSAppKitVersionNumber >= 1266.0) 
    {
      if (NSAppKitVersionNumber >= 1670.0)  // unsure if this is correct (10.14.1 is 1671.1)
        v = 0x10d0;
      else if (NSAppKitVersionNumber >= 1404.0)
        v = 0x10b0;
      else
        v=0x10a0; // 10.10+ Gestalt(gsv) return 0x109x, so we bump this to 0x10a0
    }
    else 
    {
      SInt32 a = 0x1040;
      Gestalt(gestaltSystemVersion,&a);
      v=a;
    }
  }
  return v;
}

#ifdef __AVX__
#include <immintrin.h>
#endif

#ifndef MAC_OS_X_VERSION_10_6
// 10.5 SDK doesn't include CGContextSetAllowsFontSmoothing() in header (but apparently does in libs)
CG_EXTERN void CGContextSetAllowsFontSmoothing(CGContextRef c, bool) AVAILABLE_MAC_OS_X_VERSION_10_2_AND_LATER;
#endif

#ifndef SWELL_NO_CORETEXT
static bool IsCoreTextSupported()
{
#ifdef SWELL_ATSUI_TEXT_SUPPORT
  return SWELL_GDI_GetOSXVersion() >= 0x1050 && CTFontCreateWithName && CTLineDraw && CTFramesetterCreateWithAttributedString && CTFramesetterCreateFrame && 
         CTFrameGetLines && CTLineGetTypographicBounds && CTLineCreateWithAttributedString && CTFontCopyPostScriptName
         ;
#else
  // no ATSUI, targetting 10.5+, CT is always valid
  return true;
#endif
}

static CTFontRef GetCoreTextDefaultFont()
{
  static CTFontRef deffr;
  static bool ok;
  if (!ok)
  {
    ok=true;
    if (IsCoreTextSupported())
    {
      deffr=(CTFontRef) [[NSFont labelFontOfSize:10.0] retain]; 
    }
  }
  return deffr;
}
#endif // !SWELL_NO_CORETEXT
  

static NSString *CStringToNSString(const char *str)
{
  if (!str) str="";
  NSString *ret;
  
  ret=(NSString *)CFStringCreateWithCString(NULL,str,kCFStringEncodingUTF8);
  if (ret) return ret;
  ret=(NSString *)CFStringCreateWithCString(NULL,str,kCFStringEncodingASCII);
  return ret;
}
CGColorSpaceRef __GetBitmapColorSpace()
{
  static CGColorSpaceRef cs;
  if (!cs) cs = CGColorSpaceCreateDeviceRGB();
  return cs;
}

CGColorSpaceRef __GetDisplayColorSpace()
{
  static CGColorSpaceRef cs;
  if (!cs)
  {
    // use monitor profile for 10.7+
    if (SWELL_GDI_GetOSXVersion() >= 0x1070)
    {

#ifdef MAC_OS_X_VERSION_10_11
      // OSX 10.11 SDK removes CMGetSystemProfile
      // this may be preferable on older SDKs as well, need to test (though CGDisplayCopyColorSpace is only available on 10.5+)
      cs = CGDisplayCopyColorSpace(CGMainDisplayID());
#else
      CMProfileRef systemMonitorProfile = NULL;
      CMError getProfileErr = CMGetSystemProfile(&systemMonitorProfile);
      if(noErr == getProfileErr)
      {
        cs = CGColorSpaceCreateWithPlatformColorSpace(systemMonitorProfile);
        CMCloseProfile(systemMonitorProfile);
      }
#endif
    }
  }
  if (!cs) 
    cs = CGColorSpaceCreateDeviceRGB();
  return cs;
}

static CGColorRef CreateColor(int col, float alpha=1.0f)
{
  CGFloat cols[4]={GetRValue(col)/255.0f,GetGValue(col)/255.0f,GetBValue(col)/255.0f,alpha};
  CGColorRef color=CGColorCreate(__GetBitmapColorSpace(),cols);
  return color;
}


#include "swell-gdi-internalpool.h"

char g_swell_disable_retina;

int SWELL_IsRetinaHWND(HWND hwnd)
{
  if (g_swell_disable_retina) return 0;
  if (!hwnd || SWELL_GDI_GetOSXVersion() < 0x1070) return 0;

  NSWindow *w=NULL;
  if ([(id)hwnd isKindOfClass:[NSView class]]) w = [(NSView *)hwnd window];
  else if ([(id)hwnd isKindOfClass:[NSWindow class]]) w = (NSWindow *)hwnd;

  if (w)
  {
    NSRect r=NSMakeRect(0,0,1,1);
    NSRect (*tmp)(id receiver, SEL operation, NSRect) = (NSRect (*)(id, SEL, NSRect))objc_msgSend_stret;
    NSRect str = tmp(w,sel_getUid("convertRectToBacking:"),r);

    if (str.size.width > 1.9) return 1;
  }
  return 0;
}

int SWELL_IsRetinaDC(HDC hdc)
{
  if (g_swell_disable_retina) return 0;
  HDC__ *src=(HDC__*)hdc;
  if (!src || !HDC_VALID(src)) return 0;
  
  if (!src->ctx) 
  {
#ifndef SWELL_NO_METAL
    if (src->metal_ctx)
    {
      SWELL_hwndChild *ctx = (SWELL_hwndChild*)src->metal_ctx;
      if (ctx->m_metal_dc_dirty) return ctx->m_metal_retina ? 1 : 0;

      return SWELL_IsRetinaHWND((HWND)src->metal_ctx);
    }
#endif
    return 0;
  }
  return CGContextConvertSizeToDeviceSpace((CGContextRef)src->ctx, CGSizeMake(1,1)).width > 1.9 ? 1 : 0;
}


HDC SWELL_CreateGfxContext(void *c)
{
  HDC__ *ctx=SWELL_GDP_CTX_NEW();
  NSGraphicsContext *nsc = (NSGraphicsContext *)c;
//  if (![nsc isFlipped])
//    nsc = [NSGraphicsContext graphicsContextWithGraphicsPort:[nsc graphicsPort] flipped:YES];

  ctx->ctx=(CGContextRef)[nsc graphicsPort];
//  CGAffineTransform f={1,0,0,-1,0,0};
  //CGContextSetTextMatrix(ctx->ctx,f);
  //SetTextColor(ctx,0);
  
 // CGContextSelectFont(ctx->ctx,"Arial",12.0,kCGEncodingMacRoman);
  return ctx;
}

#ifndef SWELL_NO_METAL
HDC SWELL_CreateMetalDC(SWELL_hwndChild *tex)
{
  HDC__ *ctx=SWELL_GDP_CTX_NEW();
  ctx->metal_ctx = tex;
  return ctx;
}
#endif

#define ALIGN_EXTRA 63
static void *ALIGN_FBUF(void *inbuf)
{
  const UINT_PTR extra = ALIGN_EXTRA;
  return (void *) (((UINT_PTR)inbuf+extra)&~extra); 
}

HDC SWELL_CreateMemContext(HDC hdc, int w, int h)
{
  void *buf=calloc(w*4*h+ALIGN_EXTRA,1);
  if (!buf) return 0;
  CGContextRef c=CGBitmapContextCreate(ALIGN_FBUF(buf),w,h,8,w*4, __GetBitmapColorSpace(), kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Host);
  if (!c)
  {
    free(buf);
    return 0;
  }


  CGContextTranslateCTM(c,0.0,h);
  CGContextScaleCTM(c,1.0,-1.0);
  CGContextSetAllowsFontSmoothing(c,0); // we may wish to enable this for some contexts eventually, but this is to match previous behavior

  HDC__ *ctx=SWELL_GDP_CTX_NEW();
  ctx->ctx=(CGContextRef)c;
  ctx->ownedData=buf;
  // CGContextSelectFont(ctx->ctx,"Arial",12.0,kCGEncodingMacRoman);
  
  SetTextColor(ctx,0);
  return ctx;
}

void SWELL_DeleteGfxContext(HDC ctx)
{
  HDC__ *ct=(HDC__ *)ctx;
  if (HDC_VALID(ct))
  {   
    if (ct->ownedData)
    {
      CGContextRelease(ct->ctx);
      free(ct->ownedData);
    }
    if (ct->curtextcol) CFRelease(ct->curtextcol); 
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
  pen->color=CreateColor(col,alpha);
  pen->color_int = col;
  return pen;
}
HBRUSH  CreateSolidBrushAlpha(int col, float alpha)
{
  HGDIOBJ__ *brush=GDP_OBJECT_NEW();
  brush->type=TYPE_BRUSH;
  brush->color=CreateColor(col,alpha);
  brush->color_int = col;
  brush->wid=0; 
  return brush;
}


HFONT CreateFontIndirect(LOGFONT *lf)
{
  return CreateFont(lf->lfHeight, lf->lfWidth,lf->lfEscapement, lf->lfOrientation, lf->lfWeight, lf->lfItalic, 
                    lf->lfUnderline, lf->lfStrikeOut, lf->lfCharSet, lf->lfOutPrecision,lf->lfClipPrecision, 
                    lf->lfQuality, lf->lfPitchAndFamily, lf->lfFaceName);
}

static HGDIOBJ__ global_objs[2];

void DeleteObject(HGDIOBJ pen)
{
  HGDIOBJ__ *p=(HGDIOBJ__ *)pen;
  if (p >= global_objs && p < global_objs + sizeof(global_objs)/sizeof(global_objs[0])) return;

  if (HGDIOBJ_VALID(p))
  {
    if (--p->additional_refcnt < 0)
    {
      if (p->type == TYPE_PEN || p->type == TYPE_BRUSH || p->type == TYPE_FONT || p->type == TYPE_BITMAP)
      {
        if (p->type == TYPE_PEN || p->type == TYPE_BRUSH)
          if (p->wid<0) return;
        if (p->color) CGColorRelease(p->color);

        if (p->ct_FontRef) CFRelease(p->ct_FontRef);

#ifdef SWELL_ATSUI_TEXT_SUPPORT
        if (p->atsui_font_style) ATSUDisposeStyle(p->atsui_font_style);
#endif

        if (p->wid && p->bitmapptr) [p->bitmapptr release]; 
        GDP_OBJECT_DELETE(p);
      }
      // JF> don't free unknown objects, this shouldn't ever happen anyway: else free(p);
    }
  }
}


HGDIOBJ SelectObject(HDC ctx, HGDIOBJ pen)
{
  HDC__ *c=(HDC__ *)ctx;
  HGDIOBJ__ *p=(HGDIOBJ__*) pen;
  HGDIOBJ__ **mod=0;
  if (!HDC_VALID(c)) return 0;
  
  if (p == (HGDIOBJ__*)TYPE_PEN) mod=&c->curpen;
  else if (p == (HGDIOBJ__*)TYPE_BRUSH) mod=&c->curbrush;
  else if (p == (HGDIOBJ__*)TYPE_FONT) mod=&c->curfont;

  if (mod) // clearing a particular thing
  {
    HGDIOBJ__ *np=*mod;
    *mod=0;
    return HGDIOBJ_VALID(np,(int)(INT_PTR)p)?np:p;
  }

  if (!HGDIOBJ_VALID(p)) return 0;
  
  if (p->type == TYPE_PEN) mod=&c->curpen;
  else if (p->type == TYPE_BRUSH) mod=&c->curbrush;
  else if (p->type == TYPE_FONT) mod=&c->curfont;
  
  if (!mod) return 0;
  
  HGDIOBJ__ *op=*mod;
  if (!HGDIOBJ_VALID(op,p->type)) op=(HGDIOBJ__*)(INT_PTR)p->type;
  if (op != p) *mod=p;  
  return op;
}



void SWELL_FillRect(HDC ctx, const RECT *r, HBRUSH br)
{
  HDC__ *c=(HDC__ *)ctx;
  HGDIOBJ__ *b=(HGDIOBJ__*) br;
  if (!HDC_VALID(c) || !HGDIOBJ_VALID(b,TYPE_BRUSH) || b == (HGDIOBJ__*)TYPE_BRUSH || b->type != TYPE_BRUSH) return;

#ifndef SWELL_NO_METAL
  if (c->metal_ctx)
  {
    if (b->wid>=0)
      SWELL_Metal_FillRect(c->metal_ctx, r->left, r->top, r->right-r->left,r->bottom-r->top, b->color_int);
    return;
  }
#endif

  if (!c->ctx) return;

  if (b->wid<0) return;
  
  CGRect rect=CGRectMake(r->left,r->top,r->right-r->left,r->bottom-r->top);
  CGContextSetFillColorWithColor(c->ctx,b->color);
  CGContextFillRect(c->ctx,rect);	

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
  if (!HDC_VALID(c)) return;
  if (!c->ctx) return;
  
  CGRect rect=CGRectMake(l,t,r-l,b-t);
  
  if (HGDIOBJ_VALID(c->curbrush,TYPE_BRUSH) && c->curbrush->wid >=0)
  {
    CGContextSetFillColorWithColor(c->ctx,c->curbrush->color);
    CGContextFillEllipseInRect(c->ctx,rect);	
  }
  if (HGDIOBJ_VALID(c->curpen,TYPE_PEN) && c->curpen->wid >= 0)
  {
    CGContextSetStrokeColorWithColor(c->ctx,c->curpen->color);
    CGContextStrokeEllipseInRect(c->ctx, rect); //, (float)wdl_max(1,c->curpen->wid));
  }
}

void Rectangle(HDC ctx, int l, int t, int r, int b)
{
  HDC__ *c=(HDC__ *)ctx;
  if (!HDC_VALID(c)) return;
#ifndef SWELL_NO_METAL
  if (c->metal_ctx)
  {
    if (HGDIOBJ_VALID(c->curbrush,TYPE_BRUSH) && c->curbrush->wid >= 0)
    {
      SWELL_Metal_FillRect(c->metal_ctx, l,t,r-l,b-t, c->curbrush->color_int);
    }
    if (HGDIOBJ_VALID(c->curpen,TYPE_PEN) && c->curpen->wid >= 0)
    {
      const int wid = wdl_max(1,c->curpen->wid);
      SWELL_Metal_FillRect(c->metal_ctx, l,t,r-l,wid, c->curpen->color_int);
      SWELL_Metal_FillRect(c->metal_ctx, l,b-wid,r-l,wid, c->curpen->color_int);
      SWELL_Metal_FillRect(c->metal_ctx, l,t+wid,wid,b-t-wid*2, c->curpen->color_int);
      SWELL_Metal_FillRect(c->metal_ctx, r-wid,t+wid,wid,b-t-wid*2, c->curpen->color_int);
    }
    return;
  }
#endif
  if (!c->ctx) return;
  
  CGRect rect=CGRectMake(l,t,r-l,b-t);
  
  if (HGDIOBJ_VALID(c->curbrush,TYPE_BRUSH) && c->curbrush->wid >= 0)
  {
    CGContextSetFillColorWithColor(c->ctx,c->curbrush->color);
    CGContextFillRect(c->ctx,rect);	
  }
  if (HGDIOBJ_VALID(c->curpen,TYPE_PEN) && c->curpen->wid >= 0)
  {
    CGContextSetStrokeColorWithColor(c->ctx,c->curpen->color);
    CGContextStrokeRectWithWidth(c->ctx, rect, (float)wdl_max(1,c->curpen->wid));
  }
}


HGDIOBJ GetStockObject(int wh)
{
  switch (wh)
  {
    case NULL_BRUSH:
    {
      HGDIOBJ__ *p = &global_objs[0];
      p->type=TYPE_BRUSH;
      p->wid=-1;
      return p;
    }
    case NULL_PEN:
    {
      HGDIOBJ__ *p = &global_objs[1];
      p->type=TYPE_PEN;
      p->wid=-1;
      return p;
    }
  }
  return 0;
}

void Polygon(HDC ctx, POINT *pts, int npts)
{
  HDC__ *c=(HDC__ *)ctx;
  if (!HDC_VALID(c)) return;
  if (!c->ctx) return;
  if (((!HGDIOBJ_VALID(c->curbrush,TYPE_BRUSH)||c->curbrush->wid<0) && (!HGDIOBJ_VALID(c->curpen,TYPE_PEN)||c->curpen->wid<0)) || npts<2) return;

  CGContextBeginPath(c->ctx);
  CGContextMoveToPoint(c->ctx,(float)pts[0].x,(float)pts[0].y);
  int x;
  for (x = 1; x < npts; x ++)
  {
    CGContextAddLineToPoint(c->ctx,(float)pts[x].x,(float)pts[x].y);
  }
  if (HGDIOBJ_VALID(c->curbrush,TYPE_BRUSH) && c->curbrush->wid >= 0)
  {
    CGContextSetFillColorWithColor(c->ctx,c->curbrush->color);
  }
  if (HGDIOBJ_VALID(c->curpen,TYPE_PEN) && c->curpen->wid>=0)
  {
    CGContextSetLineWidth(c->ctx,(float)wdl_max(c->curpen->wid,1));
    CGContextSetStrokeColorWithColor(c->ctx,c->curpen->color);	
  }
  CGContextDrawPath(c->ctx,HGDIOBJ_VALID(c->curpen,TYPE_PEN) && c->curpen->wid>=0 && HGDIOBJ_VALID(c->curbrush,TYPE_BRUSH) && c->curbrush->wid>=0 ?  kCGPathFillStroke : HGDIOBJ_VALID(c->curpen,TYPE_PEN) && c->curpen->wid>=0 ? kCGPathStroke : kCGPathFill);
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
  if (!c->ctx) return;
  
  CGContextSetLineWidth(c->ctx,(float)wdl_max(c->curpen->wid,1));
  CGContextSetStrokeColorWithColor(c->ctx,c->curpen->color);
	
  CGContextBeginPath(c->ctx);
  CGContextMoveToPoint(c->ctx,c->lastpos_x,c->lastpos_y);
  int x; 
  float xp,yp;
  for (x = 0; x < np-2; x += 3)
  {
    CGContextAddCurveToPoint(c->ctx,
      (float)pts[x].x,(float)pts[x].y,
      (float)pts[x+1].x,(float)pts[x+1].y,
      xp=(float)pts[x+2].x,yp=(float)pts[x+2].y);    
  }
  c->lastpos_x=(float)xp;
  c->lastpos_y=(float)yp;
  CGContextStrokePath(c->ctx);
}


void SWELL_LineTo(HDC ctx, int x, int y)
{
  HDC__ *c=(HDC__ *)ctx;
  if (!HDC_VALID(c)||!HGDIOBJ_VALID(c->curpen,TYPE_PEN)||c->curpen->wid<0) return;
#ifndef SWELL_NO_METAL
  if (c->metal_ctx)
  {
    if (x == c->lastpos_x)
    {
      const int my=wdl_min(y,c->lastpos_y);
      SWELL_Metal_FillRect(c->metal_ctx, x, my, 1, wdl_max(y,c->lastpos_y)-my+1, c->curpen->color_int);
    }
    else if (y == c->lastpos_y)
    {
      const int mx = wdl_min(x,c->lastpos_x);
      SWELL_Metal_FillRect(c->metal_ctx, mx, y, wdl_max(x,c->lastpos_x)-mx+1,1, c->curpen->color_int);
    }
    c->lastpos_x = x;
    c->lastpos_y = y;
    return;
  }
#endif
  if (!c->ctx) return;

  float w = (float)wdl_max(c->curpen->wid,1);
  CGContextSetLineWidth(c->ctx,w);
  CGContextSetStrokeColorWithColor(c->ctx,c->curpen->color);
	
  CGContextBeginPath(c->ctx);
  CGContextMoveToPoint(c->ctx,c->lastpos_x + w * 0.5,c->lastpos_y + w*0.5);
  float fx=(float)x,fy=(float)y;
  
  CGContextAddLineToPoint(c->ctx,fx+w*0.5,fy+w*0.5);
  c->lastpos_x=fx;
  c->lastpos_y=fy;
  CGContextStrokePath(c->ctx);
}

void PolyPolyline(HDC ctx, POINT *pts, DWORD *cnts, int nseg)
{
  HDC__ *c=(HDC__ *)ctx;
  if (!HDC_VALID(c)||!HGDIOBJ_VALID(c->curpen,TYPE_PEN)||c->curpen->wid<0||nseg<1) return;
  if (!c->ctx) return;

  float w = (float)wdl_max(c->curpen->wid,1);
  CGContextSetLineWidth(c->ctx,w);
  CGContextSetStrokeColorWithColor(c->ctx,c->curpen->color);
	
  CGContextBeginPath(c->ctx);
  
  while (nseg-->0)
  {
    DWORD cnt=*cnts++;
    if (!cnt) continue;
    if (!--cnt) { pts++; continue; }
    
    CGContextMoveToPoint(c->ctx,(float)pts->x+w*0.5,(float)pts->y+w*0.5);
    pts++;
    
    while (cnt--)
    {
      CGContextAddLineToPoint(c->ctx,(float)pts->x+w*0.5,(float)pts->y+w*0.5);
      pts++;
    }
  }
  CGContextStrokePath(c->ctx);
}
void *SWELL_GetCtxGC(HDC ctx)
{
  HDC__ *ct=(HDC__ *)ctx;
  if (!HDC_VALID(ct)) return 0;
  return ct->ctx;
}


void SWELL_SetPixel(HDC ctx, int x, int y, int c)
{
  HDC__ *ct=(HDC__ *)ctx;
  if (!HDC_VALID(ct)) return;
#ifndef SWELL_NO_METAL
  if (ct->metal_ctx)
  {
    SWELL_Metal_FillRect(ct->metal_ctx, x, y, 1, 1,c);
    return;
  }
#endif

  if (!ct->ctx) return;
  CGContextBeginPath(ct->ctx);
  CGContextMoveToPoint(ct->ctx,(float)x-0.5,(float)y-0.5);
  CGContextAddLineToPoint(ct->ctx,(float)x+0.5,(float)y+0.5);
  CGContextSetLineWidth(ct->ctx,(float)1.0);
  CGContextSetRGBStrokeColor(ct->ctx,GetRValue(c)/255.0,GetGValue(c)/255.0,GetBValue(c)/255.0,1.0);
  CGContextStrokePath(ct->ctx);	
}


static WDL_Mutex s_fontnamecache_mutex;

#ifdef SWELL_CLEANUP_ON_UNLOAD
static void releaseString(NSString *s) { [s release]; }
#endif
static WDL_StringKeyedArray<NSString *> s_fontnamecache(true,
#ifdef SWELL_CLEANUP_ON_UNLOAD
      releaseString
#else
      NULL
#endif
      );

static NSString *SWELL_GetCachedFontName(const char *nm)
{
  NSString *ret = NULL;
  if (nm && *nm)
  {
    s_fontnamecache_mutex.Enter();
    ret = s_fontnamecache.Get(nm);
    s_fontnamecache_mutex.Leave();
    if (!ret)
    {
      ret = CStringToNSString(nm);
      if (ret)
      {
#ifndef SWELL_NO_CORETEXT
        // only do postscript name lookups on 10.9+
        if (floor(NSFoundationVersionNumber) > 945.00) // NSFoundationVersionNumber10_8
        {
          NSFont *font = [NSFont fontWithName:ret size:10];
          NSString *nr = font ? (NSString *)CTFontCopyPostScriptName((CTFontRef)font) : NULL;
          if (nr) 
          {
            [ret release];
            ret = nr;
          }
        }
#endif

        s_fontnamecache_mutex.Enter();
        s_fontnamecache.Insert(nm,ret);
        s_fontnamecache_mutex.Leave();
      }
    }
  }
  return ret ? ret : @"";
}

HFONT CreateFont(int lfHeight, int lfWidth, int lfEscapement, int lfOrientation, int lfWeight, char lfItalic, 
                 char lfUnderline, char lfStrikeOut, char lfCharSet, char lfOutPrecision, char lfClipPrecision, 
                 char lfQuality, char lfPitchAndFamily, const char *lfFaceName)
{
  HGDIOBJ__ *font=GDP_OBJECT_NEW();
  font->type=TYPE_FONT;
  float fontwid=lfHeight;
  
  if (!fontwid) fontwid=lfWidth;
  if (fontwid<0)fontwid=-fontwid;
  
  if (fontwid < 2 || fontwid > 8192) fontwid=10;
  
  font->font_rotation = lfOrientation/10.0;

#ifndef SWELL_NO_CORETEXT
  if (IsCoreTextSupported())
  {
    char buf[1024];
    lstrcpyn_safe(buf,lfFaceName,900);
    if (lfWeight >= FW_BOLD) strcat(buf," Bold");
    if (lfItalic) strcat(buf," Italic");

    font->ct_FontRef = (void*)CTFontCreateWithName((CFStringRef)SWELL_GetCachedFontName(buf),fontwid,NULL);
    if (!font->ct_FontRef) font->ct_FontRef = (void*)[[NSFont labelFontOfSize:fontwid] retain]; 

    // might want to make this conditional (i.e. only return font if created successfully), but I think we'd rather fallback to a system font than use ATSUI
    return font;
  }
#endif
  
#ifdef SWELL_ATSUI_TEXT_SUPPORT
  ATSUFontID fontid=kATSUInvalidFontID;
  if (lfFaceName && lfFaceName[0])
  {
    ATSUFindFontFromName(lfFaceName,strlen(lfFaceName),kFontFullName /* kFontFamilyName? */ ,(FontPlatformCode)kFontNoPlatform,kFontNoScriptCode,kFontNoLanguageCode,&fontid);
    //    if (fontid==kATSUInvalidFontID) printf("looked up %s and got %d\n",lfFaceName,fontid);
  }
  
  if (ATSUCreateStyle(&font->atsui_font_style) == noErr && font->atsui_font_style)
  {    
    Fixed fsize=Long2Fix(fontwid);
    
    Boolean isBold=lfWeight >= FW_BOLD;
    Boolean isItal=!!lfItalic;
    Boolean isUnder=!!lfUnderline;
    
    ATSUAttributeTag        theTags[] = { kATSUQDBoldfaceTag, kATSUQDItalicTag, kATSUQDUnderlineTag,kATSUSizeTag,kATSUFontTag };
    ByteCount               theSizes[] = { sizeof(Boolean),sizeof(Boolean),sizeof(Boolean), sizeof(Fixed),sizeof(ATSUFontID)  };
    ATSUAttributeValuePtr   theValues[] =  {&isBold, &isItal, &isUnder,  &fsize, &fontid  } ;
    
    int attrcnt=sizeof(theTags)/sizeof(theTags[0]);
    if (fontid == kATSUInvalidFontID) attrcnt--;    
    
    if (ATSUSetAttributes (font->atsui_font_style,                       
                       attrcnt,                       
                       theTags,                      
                       theSizes,                      
                       theValues)!=noErr)
    {
      ATSUDisposeStyle(font->atsui_font_style);
      font->atsui_font_style=0;
    }
  }
  else
    font->atsui_font_style=0;
  
#endif
  
  
  return font;
}

int GetTextFace(HDC ctx, int nCount, LPTSTR lpFaceName)
{
  HDC__ *ct=(HDC__*)ctx;
  if (!HDC_VALID(ct) || !nCount || !lpFaceName) return 0;
  
#ifndef SWELL_NO_CORETEXT
  CTFontRef fr=NULL;
  if (HGDIOBJ_VALID(ct->curfont,TYPE_FONT)) fr=(CTFontRef)ct->curfont->ct_FontRef;
  if (!fr)  fr=GetCoreTextDefaultFont();
  
  if (fr)
  {
    CFStringRef name=CTFontCopyDisplayName(fr);
    const char* p=[(NSString*)name UTF8String];
    if (p)
    {
      lstrcpyn_safe(lpFaceName, p, nCount);
      return (int)strlen(lpFaceName);
    }
  }
#endif
  
  return 0;
}

BOOL GetTextMetrics(HDC ctx, TEXTMETRIC *tm)
{
  HDC__ *ct=(HDC__ *)ctx;
  if (tm) // give some sane defaults
  {
    tm->tmInternalLeading=3;
    tm->tmAscent=12;
    tm->tmDescent=4;
    tm->tmHeight=16;
    tm->tmAveCharWidth = 10;
  }
  if (!HDC_VALID(ct)||!tm) return 0;

  bool curfont_valid=HGDIOBJ_VALID(ct->curfont,TYPE_FONT);

#ifdef SWELL_ATSUI_TEXT_SUPPORT
  if (curfont_valid && ct->curfont->atsui_font_style)
  {
    ATSUTextMeasurement ascent=Long2Fix(10);
    ATSUTextMeasurement descent=Long2Fix(3);
    ATSUTextMeasurement sz=Long2Fix(0);
    ATSUTextMeasurement width =Long2Fix(12);
    ATSUGetAttribute(ct->curfont->atsui_font_style,  kATSUAscentTag, sizeof(ATSUTextMeasurement), &ascent,NULL);
    ATSUGetAttribute(ct->curfont->atsui_font_style,  kATSUDescentTag, sizeof(ATSUTextMeasurement), &descent,NULL);
    ATSUGetAttribute(ct->curfont->atsui_font_style,  kATSUSizeTag, sizeof(ATSUTextMeasurement), &sz,NULL);
    ATSUGetAttribute(ct->curfont->atsui_font_style, kATSULineWidthTag, sizeof(ATSUTextMeasurement),&width,NULL);
    
    float asc=Fix2X(ascent);
    float desc=Fix2X(descent);
    float size = Fix2X(sz);
    
    if (size < (asc+desc)*0.2) size=asc+desc;
            
    tm->tmAscent = (int)ceil(asc);
    tm->tmDescent = (int)ceil(desc);
    tm->tmInternalLeading=(int)ceil(asc+desc-size);
    if (tm->tmInternalLeading<0)tm->tmInternalLeading=0;
    tm->tmHeight=(int) ceil(asc+desc);
    tm->tmAveCharWidth = (int) (ceil(asc+desc)*0.65); // (int)ceil(Fix2X(width));
    
    return 1;
  }
#endif

#ifndef SWELL_NO_CORETEXT
  CTFontRef fr = curfont_valid ? (CTFontRef)ct->curfont->ct_FontRef : NULL;
  if (!fr)  fr=GetCoreTextDefaultFont();

  if (fr)
  {
    tm->tmInternalLeading = CTFontGetLeading(fr);
    tm->tmAscent = CTFontGetAscent(fr);
    tm->tmDescent = CTFontGetDescent(fr);
    tm->tmHeight = (tm->tmInternalLeading + tm->tmAscent + tm->tmDescent);
    tm->tmAveCharWidth = tm->tmHeight*2/3; // todo

    if (tm->tmHeight)  tm->tmHeight++;
    
    return 1;
  }
#endif

  
  return 1;
}



#ifdef SWELL_ATSUI_TEXT_SUPPORT

static int DrawTextATSUI(HDC ctx, CFStringRef strin, RECT *r, int align, bool *err)
{
  HDC__ *ct=(HDC__ *)ctx;
  HGDIOBJ__ *font=ct->curfont; // caller must specify a valid font
  
  UniChar strbuf[4096];
  int strbuf_len;
 
  {
    strbuf[0]=0;
    CFRange r = {0,CFStringGetLength(strin)};
    if (r.length > 4095) r.length=4095;
    strbuf_len=CFStringGetBytes(strin,r,kCFStringEncodingUTF16,' ',false,(UInt8*)strbuf,sizeof(strbuf)-2,NULL);
    if (strbuf_len<0)strbuf_len=0;
    else if (strbuf_len>4095) strbuf_len=4095;
    strbuf[strbuf_len]=0;
  }
  
  {
    ATSUAttributeTag        theTags[] = { kATSUColorTag,   };
    ByteCount               theSizes[] = { sizeof(RGBColor),  };

    RGBColor tcolor;
    ATSUAttributeValuePtr   theValues[] =  {&tcolor,  } ;
    
    tcolor.red = GetRValue(ct->cur_text_color_int)*256;
    tcolor.green = GetGValue(ct->cur_text_color_int)*256;
    tcolor.blue = GetBValue(ct->cur_text_color_int)*256;
    
    // error check this? we can live with the wrong color maybe?
    ATSUSetAttributes(font->atsui_font_style,  sizeof(theTags)/sizeof(theTags[0]), theTags, theSizes, theValues);
  }
  
  UniCharCount runLengths[1]={kATSUToTextEnd};
  ATSUTextLayout layout; 
  if (ATSUCreateTextLayoutWithTextPtr(strbuf, kATSUFromTextBeginning, kATSUToTextEnd, strbuf_len, 1, runLengths, &font->atsui_font_style, &layout)!=noErr)
  {
    *err=true;
    return 0;
  }
  
  {
    Fixed frot = X2Fix(font->font_rotation);
    
    ATSULineTruncation tv = (align & DT_END_ELLIPSIS) ? kATSUTruncateEnd : kATSUTruncateNone;
    ATSUAttributeTag        theTags[] = { kATSUCGContextTag, kATSULineTruncationTag, kATSULineRotationTag };
    ByteCount               theSizes[] = { sizeof (CGContextRef), sizeof(ATSULineTruncation), sizeof(Fixed)};
    ATSUAttributeValuePtr   theValues[] =  { &ct->ctx, &tv, &frot } ;
    
    
    if (ATSUSetLayoutControls (layout,
                           
                           sizeof(theTags)/sizeof(theTags[0]),
                           
                           theTags,
                           
                           theSizes,
                           
                           theValues)!=noErr)
    {
      *err=true;
      ATSUDisposeTextLayout(layout);   
      return 0;
    }
  }
  
  
  ATSUTextMeasurement leftFixed, rightFixed, ascentFixed, descentFixed;

  if (ATSUGetUnjustifiedBounds(layout, kATSUFromTextBeginning, kATSUToTextEnd, &leftFixed, &rightFixed, &ascentFixed, &descentFixed)!=noErr) 
  {
    *err=true;
    ATSUDisposeTextLayout(layout);   
    return 0;
  }

  int w=Fix2Long(rightFixed);
  int descent=Fix2Long(descentFixed);
  int h=descent + Fix2Long(ascentFixed);
  if (align&DT_CALCRECT)
  {
    ATSUDisposeTextLayout(layout);   
    r->right=r->left+w;
    r->bottom=r->top+h;
    return h;  
  }
  CGContextSaveGState(ct->ctx);    

  if (!(align & DT_NOCLIP))
    CGContextClipToRect(ct->ctx,CGRectMake(r->left,r->top,r->right-r->left,r->bottom-r->top));

  int l=r->left, t=r->top;
    
  if (fabs(font->font_rotation)<45.0)
  {
    if (align & DT_RIGHT) l = r->right-w;
    else if (align & DT_CENTER) l = (r->right+r->left)/2 - w/2;      
  }
  else l+=Fix2Long(ascentFixed); // 90 degree special case (we should generalize this to be correct throughout the rotation range, but oh well)
    
  if (align & DT_BOTTOM) t = r->bottom-h;
  else if (align & DT_VCENTER) t = (r->bottom+r->top)/2 - h/2;
    
  CGContextTranslateCTM(ct->ctx,0,t);
  CGContextScaleCTM(ct->ctx,1,-1);
  CGContextTranslateCTM(ct->ctx,0,-t-h);
    
  if (ct->curbkmode == OPAQUE)
  {      
    CGRect bgr = CGRectMake(l, t, w, h);
    CGColorRef bgc = CreateColor(ct->curbkcol);
    CGContextSetFillColorWithColor(ct->ctx, bgc);
    CGContextFillRect(ct->ctx, bgr);
    CGColorRelease(bgc);	
  }
 
  if (ATSUDrawText(layout,kATSUFromTextBeginning,kATSUToTextEnd,Long2Fix(l),Long2Fix(t+descent))!=noErr)
    *err=true;
  
  CGContextRestoreGState(ct->ctx);    
  
  ATSUDisposeTextLayout(layout);   
  
  return h;
}

#endif

int DrawText(HDC ctx, const char *buf, int buflen, RECT *r, int align)
{
  HDC__ *ct=(HDC__ *)ctx;
  if (!HDC_VALID(ct)) return 0;
  if (!(align & DT_CALCRECT) && !ct->ctx) return 0;
  
  bool has_ml=false;
  char tmp[4096];
  const char *p=buf;
  char *op=tmp;
  while (*p && (op-tmp)<sizeof(tmp)-1 && (buflen<0 || (p-buf)<buflen))
  {
    if (*p == '&' && !(align&DT_NOPREFIX)) p++; 

    if (*p == '\r')  p++; 
    else if (*p == '\n' && (align&DT_SINGLELINE)) { *op++ = ' '; p++; }
    else 
    {
      if (*p == '\n') has_ml=true;
      *op++=*p++;
    }
  }
  *op=0;
  
  if (!tmp[0]) return 0; // dont draw empty strings
  
  NSString *str=CStringToNSString(tmp);
  
  if (!str) return 0;
  
  bool curfont_valid = HGDIOBJ_VALID(ct->curfont,TYPE_FONT);
#ifdef SWELL_ATSUI_TEXT_SUPPORT
  if (curfont_valid && ct->curfont->atsui_font_style)
  {
    bool err=false;
    int ret =  DrawTextATSUI(ctx,(CFStringRef)str,r,align,&err);
    [str release];
    
    if (!err) return ret;
    return 0;
  }
#endif  
  
#ifndef SWELL_NO_CORETEXT
  CTFontRef fr = curfont_valid ? (CTFontRef)ct->curfont->ct_FontRef : NULL;
  if (!fr)  fr=GetCoreTextDefaultFont();
  if (fr)
  {
    // Initialize string, font, and context
    CFStringRef keys[] = { kCTFontAttributeName,kCTForegroundColorAttributeName };
    CFTypeRef values[] = { fr,ct->curtextcol };
    
    int nk= sizeof(keys) / sizeof(keys[0]);
    if (!values[1]) nk--;
    
    CFDictionaryRef attributes = CFDictionaryCreate(kCFAllocatorDefault, (const void**)&keys, (const void**)&values, nk,
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks);
       
    CFAttributedStringRef attrString =
          CFAttributedStringCreate(kCFAllocatorDefault, (CFStringRef)str, attributes);
    CFRelease(attributes);
    [str release];


    CTFrameRef frame = NULL;
    CFArrayRef lines = NULL;
    CTLineRef line = NULL;
    CGFloat asc=0;
    int line_w=0,line_h=0;
    if (has_ml)
    {
      CTFramesetterRef framesetter = CTFramesetterCreateWithAttributedString(attrString);
      if (framesetter)
      {
        CGMutablePathRef path=CGPathCreateMutable();
        CGPathAddRect(path,NULL,CGRectMake(0,0,100000,100000));
        frame = CTFramesetterCreateFrame(framesetter,CFRangeMake(0,0),path,NULL);
        CFRelease(framesetter);
        CFRelease(path);
      }
      if (frame)
      {
        lines = CTFrameGetLines(frame);
        const int n = (int)CFArrayGetCount(lines);
        for (int x=0;x<n;x++)
        {
          CTLineRef l = (CTLineRef)CFArrayGetValueAtIndex(lines,x);
          if (l)
          {
            CGFloat desc=0,lead=0;
            int w = (int) floor(CTLineGetTypographicBounds(l,&asc,&desc,&lead)+0.5);
            int h =(int) floor(asc+desc+lead+1.5);
            line_h+=h;
            if (line_w < w) line_w=w;
          }
        }
      }
    }
    else
    {
      line = CTLineCreateWithAttributedString(attrString);
       
      if (line)
      {
        CGFloat desc=0,lead=0;
        line_w = (int) floor(CTLineGetTypographicBounds(line,&asc,&desc,&lead)+0.5);
        line_h =(int) floor(asc+desc+lead+1.5);
      }
    }
    if (line_h) line_h++;

    CFRelease(attrString);
    
    if (align & DT_CALCRECT)
    {
      r->right = r->left+line_w;
      r->bottom = r->top+line_h;
      if (line) CFRelease(line);
      if (frame) CFRelease(frame);
      return line_h;
    }

    float xo=r->left,yo=r->top;
    if (align & DT_RIGHT) xo += (r->right-r->left) - line_w;
    else if (align & DT_CENTER) xo += (r->right-r->left)/2 - line_w/2;

    if (align & DT_BOTTOM) yo += (r->bottom-r->top) - line_h;
    else if (align & DT_VCENTER) yo += (r->bottom-r->top)/2 - line_h/2;

    
    CGContextSaveGState(ct->ctx);

    CGAffineTransform f={1,0,0,-1,0,0};
    CGContextSetTextMatrix(ct->ctx, f);

    if (!(align & DT_NOCLIP))
    {
      CGContextClipToRect(ct->ctx,CGRectMake(r->left,r->top,r->right-r->left,r->bottom-r->top));          
    }
    
    CGColorRef bgc = NULL;
    if (ct->curbkmode == OPAQUE)
    {      
      bgc = CreateColor(ct->curbkcol);
    }

    if (line) 
    {
      if (bgc)
      {
        CGContextSetFillColorWithColor(ct->ctx, bgc);
        CGContextFillRect(ct->ctx, CGRectMake(xo,yo,line_w,line_h));
      }
      CGContextSetTextPosition(ct->ctx, xo, yo + asc);
      CTLineDraw(line,ct->ctx);

    }
    if (lines)
    {
      const int n = (int)CFArrayGetCount(lines);
      for (int x=0;x<n;x++)
      {
        CTLineRef l = (CTLineRef)CFArrayGetValueAtIndex(lines,x);
        if (l)
        {
          CGFloat desc=0.0,lead=0.0;
          asc=0.0;
          float lw=CTLineGetTypographicBounds(l,&asc,&desc,&lead);

          if (bgc)
          {
            CGContextSetFillColorWithColor(ct->ctx, bgc);
            CGContextFillRect(ct->ctx, CGRectMake(xo,yo,lw,asc+desc+lead));
          }
          CGContextSetTextPosition(ct->ctx, xo, yo + asc);
          CTLineDraw(l,ct->ctx);
          
          yo += floor(asc+desc+lead+0.5);          
        }
      }
    }
    
    CGContextRestoreGState(ct->ctx);
    if (bgc) CGColorRelease(bgc);
    if (line) CFRelease(line);
    if (frame) CFRelease(frame);
    
    return line_h;
  }
#endif
  
  
  [str release];
  return 0;  
}


int GetGlyphIndicesW(HDC ctx, wchar_t *buf, int len, unsigned short *indices, int flags)
{
  HDC__ *ct=(HDC__*)ctx;
  if (HDC_VALID(ct) && HGDIOBJ_VALID(ct->curfont, TYPE_FONT))
  {
#ifndef SWELL_NO_CORETEXT
    CTFontRef f=(CTFontRef)ct->curfont->ct_FontRef;
    if (f && CTFontGetGlyphsForCharacters(f, (const UniChar*)buf, (CGGlyph*)indices, (CFIndex)len)) return len;
#endif
  }
  
  int i;
  for (i=0; i < len; ++i) indices[i]=(flags == GGI_MARK_NONEXISTING_GLYPHS ? 0xFFFF : 0);
  return 0;
}




void SetBkColor(HDC ctx, int col)
{
  HDC__ *ct=(HDC__ *)ctx;
  if (!HDC_VALID(ct)) return;
  ct->curbkcol=col;
}

void SetBkMode(HDC ctx, int col)
{
  HDC__ *ct=(HDC__ *)ctx;
  if (!HDC_VALID(ct)) return;
  ct->curbkmode=col;
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
  ct->cur_text_color_int = col;
  
  if (ct->curtextcol) CFRelease(ct->curtextcol);

  ct->curtextcol = CreateColor(col);
}


HICON CreateIconIndirect(ICONINFO* iconinfo)
{
  if (!iconinfo || !iconinfo->fIcon) return 0;  
  HGDIOBJ__* i=iconinfo->hbmColor;
  if (!HGDIOBJ_VALID(i,TYPE_BITMAP) || !i->bitmapptr) return 0;
  NSImage* img=i->bitmapptr;
  if (!img) return 0;
    
  HGDIOBJ__* icon=GDP_OBJECT_NEW();
  icon->type=TYPE_BITMAP;
  icon->wid=1;
  [img retain];
  icon->bitmapptr=img;
  return icon;   
}

HICON LoadNamedImage(const char *name, bool alphaFromMask)
{
  NSImage *img=0;
  NSString *str=CStringToNSString(name); 
  if (strstr(name,"/"))
  {
    img=[[NSImage alloc] initWithContentsOfFile:str];
  }
  if (!img) 
  {
    img=[NSImage imageNamed:str];
    if (img) [img retain];
  }
  [str release];
  if (!img) 
  {
    return 0;
  }
    
  [img setFlipped:YES];
  if (alphaFromMask)
  {
    const NSSize sz=[img size];
    const int w = (int)sz.width, h=(int)sz.height;
    HDC hdc;
    if (w>0 && h>0 && NULL != (hdc=SWELL_CreateMemContext(NULL,w,h)))
    {
      [NSGraphicsContext saveGraphicsState];
      NSGraphicsContext *gc=[NSGraphicsContext graphicsContextWithGraphicsPort:((struct HDC__*)hdc)->ctx flipped:NO];
      [NSGraphicsContext setCurrentContext:gc];
      [img drawInRect:NSMakeRect(0,0,w,h) fromRect:NSZeroRect operation:NSCompositeCopy fraction:1.0];
      [NSGraphicsContext restoreGraphicsState];

      // on yosemite, calling [img TIFFRepresentation] seems to change img somehow for some images, ouch.
      // in this case, we should always replace img with newImage (set rcnt=1), but in general
      // maybe we shoulnt use alphaFromMask anyhow
      NSData *data = [img TIFFRepresentation];
      if (!data)
      {
        SWELL_DeleteGfxContext(hdc);
        goto return_img;
      }

      NSImage *newImage=[[NSImage alloc] initWithData:data];
      [newImage setFlipped:YES];

      const int *fb = (const int *)SWELL_GetCtxFrameBuffer(hdc);
      int y,rcnt=0;
      [newImage lockFocus];
      CGContextRef myContext = (CGContextRef) [[NSGraphicsContext currentContext] graphicsPort];
      for (y=0; y < h; y ++)
      {
        int x;
        for (x = 0; x < w; x++)
        {
          if ((*fb++ & 0xffffff) == 0xff00ff)
          {
            CGContextClearRect(myContext,CGRectMake(x,y,1,1));
            rcnt++;
          }
        }
      }
      [newImage unlockFocus];

      SWELL_DeleteGfxContext(hdc);

      if (rcnt)
      {
        [img release];
        img=newImage;    
      }
      else
        [newImage release];
    }
  }
  
return_img:
  HGDIOBJ__ *i=GDP_OBJECT_NEW();
  i->type=TYPE_BITMAP;
  i->wid=1;
  i->bitmapptr = img;
  return i;
}

void DrawImageInRect(HDC ctx, HICON img, const RECT *r)
{
  HGDIOBJ__ *i = (HGDIOBJ__ *)img;
  HDC__ *ct=(HDC__*)ctx;
  if (!HDC_VALID(ct) || !HGDIOBJ_VALID(i,TYPE_BITMAP) || !i->bitmapptr) return;
  if (!ct->ctx) return;
  //CGContextDrawImage(ct->ctx,CGRectMake(r->left,r->top,r->right-r->left,r->bottom-r->top),(CGImage*)i->bitmapptr);
  // probably a better way since this ignores the ctx
  [NSGraphicsContext saveGraphicsState];
  NSGraphicsContext *gc=[NSGraphicsContext graphicsContextWithGraphicsPort:ct->ctx flipped:NO];
  [NSGraphicsContext setCurrentContext:gc];
  NSImage *nsi=i->bitmapptr;
  NSRect rr=NSMakeRect(r->left,r->top,r->right-r->left,r->bottom-r->top);
  [nsi setFlipped:YES];
  [nsi drawInRect:rr fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1.0];
  [nsi setFlipped:NO]; // todo: restore old flippedness?
  [NSGraphicsContext restoreGraphicsState];
//  [gc release];
}


BOOL GetObject(HICON icon, int bmsz, void *_bm)
{
  memset(_bm,0,bmsz);
  if (bmsz < 2*(int)sizeof(LONG)) return false;
  BITMAP *bm=(BITMAP *)_bm;
  HGDIOBJ__ *i = (HGDIOBJ__ *)icon;
  if (!HGDIOBJ_VALID(i,TYPE_BITMAP)) return false;
  NSImage *img = i->bitmapptr;
  if (!img) return false;
  bm->bmWidth = (int) ([img size].width+0.5);
  bm->bmHeight = (int) ([img size].height+0.5);
  if (bmsz >= (int)sizeof(BITMAP))
  {
    bm->bmWidthBytes = bm->bmWidth * 4;
    bm->bmPlanes = 1;
    bm->bmBitsPixel = 32;
    bm->bmBits = NULL;
  }

  return true;
}


void *GetNSImageFromHICON(HICON ico)
{
  HGDIOBJ__ *i = (HGDIOBJ__ *)ico;
  if (!HGDIOBJ_VALID(i,TYPE_BITMAP)) return 0;
  return i->bitmapptr;
}

#if 0
static int ColorFromNSColor(NSColor *color, int valifnul)
{
  if (!color) return valifnul;
  float r,g,b;
  NSColor *color2=[color colorUsingColorSpaceName:NSCalibratedRGBColorSpace];
  if (!color2) 
  {
    NSLog(@"error converting colorspace from: %@\n",[color colorSpaceName]);
    return valifnul;
  }
  
  [color2 getRed:&r green:&g blue:&b alpha:NULL];
  return RGB((int)(r*255.0),(int)(g*255.0),(int)(b*255.0));
}
#else
#define ColorFromNSColor(a,b) (b)
#endif
int GetSysColor(int idx)
{
 // NSColors that seem to be valid: textBackgroundColor, selectedTextBackgroundColor, textColor, selectedTextColor
  
  switch (idx)
  {
    case COLOR_WINDOW: return ColorFromNSColor([NSColor controlColor],RGB(192,192,192));
    case COLOR_3DFACE: 
    case COLOR_BTNFACE: return ColorFromNSColor([NSColor controlColor],RGB(192,192,192));
    case COLOR_SCROLLBAR: return ColorFromNSColor([NSColor controlColor],RGB(32,32,32));
    case COLOR_3DSHADOW: return ColorFromNSColor([NSColor selectedTextBackgroundColor],RGB(96,96,96));
    case COLOR_3DHILIGHT: return ColorFromNSColor([NSColor selectedTextBackgroundColor],RGB(224,224,224));
    case COLOR_BTNTEXT: return ColorFromNSColor([NSColor selectedTextBackgroundColor],RGB(0,0,0));
    case COLOR_3DDKSHADOW: return (ColorFromNSColor([NSColor selectedTextBackgroundColor],RGB(96,96,96))>>1)&0x7f7f7f;
    case COLOR_INFOBK: return RGB(255,240,200);
    case COLOR_INFOTEXT: return RGB(0,0,0);
      
  }
  return 0;
}


void BitBlt(HDC hdcOut, int x, int y, int w, int h, HDC hdcIn, int xin, int yin, int mode)
{
  StretchBlt(hdcOut,x,y,w,h,hdcIn,xin,yin,w,h,mode);
}

void StretchBlt(HDC hdcOut, int x, int y, int destw, int desth, HDC hdcIn, int xin, int yin, int w, int h, int mode)
{
  HDC__ *src=(HDC__*)hdcIn;
  HDC__ *dest=(HDC__*)hdcOut;
  if (w<1 || h<1 || !HDC_VALID(src) || !HDC_VALID(dest) || !src->ownedData || !src->ctx) return;
  
  const int sw = (int)CGBitmapContextGetWidth(src->ctx);
  const int sh = (int)CGBitmapContextGetHeight(src->ctx);
  
  const int preclip_w=w;
  const int preclip_h=h;
  
  if (xin<0) 
  { 
    x-=(xin*destw)/w;
    w+=xin; 
    xin=0; 
  }
  if (yin<0) 
  { 
    y-=(yin*desth)/h;
    h+=yin; 
    yin=0; 
  }
  if (xin+w > sw) w=sw-xin;
  if (yin+h > sh) h=sh-yin;
  
  if (w<1||h<1) return;

  if (destw==preclip_w) destw=w; // no scaling, keep width the same
  else if (w != preclip_w) destw = (w*destw)/preclip_w;
  
  if (desth == preclip_h) desth=h;
  else if (h != preclip_h) desth = (h*desth)/preclip_h;
  
  const bool use_alphachannel = mode == (int)SRCCOPY_USEALPHACHAN;

  unsigned char *p = (unsigned char *)ALIGN_FBUF(src->ownedData);
  p += (xin + sw*yin)*4;

#ifndef SWELL_NO_METAL

  if (dest->metal_ctx)
  {
    void SWELL_Metal_Blit(void *tex, unsigned char *buf, int x, int y, int w, int h, int span, bool retina_hint);

    if (w == destw && h == desth)
      SWELL_Metal_Blit(hdcOut->metal_ctx,p,x,y,w,h,sw,false);
    else if (WDL_NORMALLY(w == destw*2) && WDL_NORMALLY(h == desth*2))
    {
      SWELL_Metal_Blit(hdcOut->metal_ctx,p,x*2,y*2,w,h,sw,true);
    }

    return;
  }

#endif

  if (!dest->ctx) return;

  CGContextRef output = (CGContextRef)dest->ctx;
  CGRect outputr = CGRectMake(x,-desth-y,destw,desth);
  
  
#ifdef SWELL_SUPPORT_OPENGL_BLIT
  if (dest->GLgfxctx)
  {
    NSOpenGLContext *glCtx = (NSOpenGLContext*) dest->GLgfxctx;
    NSOpenGLContext *cCtx = [NSOpenGLContext currentContext];
    if (glCtx != cCtx)
    {
      [glCtx makeCurrentContext];
    }
    
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_TEXTURE_RECTANGLE_EXT);
    
    GLuint texid=0;
    glGenTextures(1, &texid);
    glBindTexture(GL_TEXTURE_RECTANGLE_EXT, texid);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, sw);
    glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MIN_FILTER,  GL_LINEAR);
    glTexImage2D(GL_TEXTURE_RECTANGLE_EXT,0,GL_RGBA8,w,h,0,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV, p);
    
    glViewport(x,[[glCtx view] bounds].size.height-desth-y,destw,desth);
    glBegin(GL_QUADS);
    
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(-1,1);
    
    glTexCoord2f(0.0f, h);
    glVertex2f(-1,-1);
    
    glTexCoord2f(w,h);
    glVertex2f(1,-1);
    
    glTexCoord2f(w, 0.0f);
    glVertex2f(1,1);
    glEnd();
    
    glDeleteTextures(1,&texid);
    glFlush();

    if (glCtx != cCtx) [cCtx makeCurrentContext];
    return;
  }
#endif
  
  
  
  CGDataProviderRef provider = CGDataProviderCreateWithData(NULL,p,4*sw*h,NULL);
  CGImageRef img = CGImageCreate(w,h,8,32,4*sw,__GetDisplayColorSpace(),
      (use_alphachannel?kCGImageAlphaFirst:kCGImageAlphaNoneSkipFirst)|kCGBitmapByteOrder32Host,
      provider,NULL,NO,kCGRenderingIntentDefault);
  CGDataProviderRelease(provider);
  
  if (img)
  {
    CGContextSaveGState(output);
    CGContextScaleCTM(output,1.0,-1.0);
  
    CGContextSetInterpolationQuality(output,kCGInterpolationNone);
    CGContextDrawImage(output,outputr,img);
    CGContextRestoreGState(output);
  
    CGImageRelease(img);
  }
}

void SWELL_PushClipRegion(HDC ctx)
{
  HDC__ *ct=(HDC__ *)ctx;
  if (HDC_VALID(ct) && ct->ctx) CGContextSaveGState(ct->ctx);
}

void SWELL_SetClipRegion(HDC ctx, const RECT *r)
{
  HDC__ *ct=(HDC__ *)ctx;
  if (HDC_VALID(ct) && ct->ctx) CGContextClipToRect(ct->ctx,CGRectMake(r->left,r->top,r->right-r->left,r->bottom-r->top));

}

void SWELL_PopClipRegion(HDC ctx)
{
  HDC__ *ct=(HDC__ *)ctx;
  if (HDC_VALID(ct) && ct->ctx) CGContextRestoreGState(ct->ctx);
}

void *SWELL_GetCtxFrameBuffer(HDC ctx)
{
  HDC__ *ct=(HDC__ *)ctx;
  if (HDC_VALID(ct)) return ALIGN_FBUF(ct->ownedData);
  return 0;
}


HDC GetDC(HWND h)
{
  if (h && [(id)h isKindOfClass:[NSWindow class]])
  {
    if ([(id)h respondsToSelector:@selector(getSwellPaintInfo:)]) 
    {
      PAINTSTRUCT ps={0,}; 
      [(id)h getSwellPaintInfo:(PAINTSTRUCT *)&ps];
      if (ps.hdc) 
      {
        if ((ps.hdc)->ctx) CGContextSaveGState((ps.hdc)->ctx);
        return ps.hdc;
      }
    }
    h=(HWND)[(id)h contentView];
  }
  
  if (h && [(id)h isKindOfClass:[NSView class]])
  {
    if ([(id)h respondsToSelector:@selector(getSwellPaintInfo:)]) 
    {
      PAINTSTRUCT ps={0,}; 
      [(id)h getSwellPaintInfo:(PAINTSTRUCT *)&ps];
      if (HDC_VALID((HDC__*)ps.hdc)) 
      {
        if (((HDC__*)ps.hdc)->ctx) CGContextSaveGState((ps.hdc)->ctx);
        return ps.hdc;
      }
    }

#ifndef SWELL_NO_METAL
    if ([(id)h isKindOfClass:[SWELL_hwndChild class]] && [(SWELL_hwndChild *)h swellWantsMetal])
    {
      SWELL_hwndChild *wnd = (SWELL_hwndChild*)h;

      wnd->m_metal_dc_dirty = 2;
      return SWELL_CreateMetalDC(wnd);
    }
#endif
    
    if ([(NSView*)h lockFocusIfCanDraw])
    {
      HDC ret= SWELL_CreateGfxContext([NSGraphicsContext currentContext]);
      if (ret)
      {
        if (ret->ctx) CGContextSaveGState(ret->ctx);
        if (!ret->GLgfxctx && [(id)h respondsToSelector:@selector(swellGetGLContext)])
        {
          NSOpenGLContext *glctx = (NSOpenGLContext*)[(id)h swellGetGLContext];
          ret->GLgfxctx = glctx;
          if (glctx) [glctx setView:(NSView *)h];
        }
      }
      return ret;
    }
  }
  return 0;
}

HDC GetWindowDC(HWND h)
{
  HDC ret=GetDC(h);
  if (ret)
  {
    NSView *v=NULL;
    if ([(id)h isKindOfClass:[NSWindow class]]) v=[(id)h contentView];
    else if ([(id)h isKindOfClass:[NSView class]]) v=(NSView *)h;
    
    if (v)
    {
      NSRect b=[v bounds];
      float xsc=b.origin.x;
      float ysc=b.origin.y;
      if ((xsc || ysc) && (ret)->ctx) CGContextTranslateCTM((ret)->ctx,xsc,ysc);
    }
  }
  return ret;
}

void ReleaseDC(HWND h, HDC hdc)
{
  if (hdc)
  {
    if ((hdc)->ctx) CGContextRestoreGState((hdc)->ctx);
  }
  if (h && [(id)h isKindOfClass:[NSWindow class]])
  {
    if ([(id)h respondsToSelector:@selector(getSwellPaintInfo:)]) 
    {
      PAINTSTRUCT ps={0,}; 
      [(id)h getSwellPaintInfo:(PAINTSTRUCT *)&ps];
      if (ps.hdc && ps.hdc==hdc) return;
    }
    h=(HWND)[(id)h contentView];
  }
  bool isView=h && [(id)h isKindOfClass:[NSView class]];
  if (isView)
  {
    if ([(id)h respondsToSelector:@selector(getSwellPaintInfo:)]) 
    {
      PAINTSTRUCT ps={0,}; 
      [(id)h getSwellPaintInfo:(PAINTSTRUCT *)&ps];
      if (ps.hdc && ps.hdc==hdc) return;
    }
  }    
  if (hdc && hdc->GLgfxctx)
  {
    if ([NSOpenGLContext currentContext] == hdc->GLgfxctx) [NSOpenGLContext clearCurrentContext]; 
    hdc->GLgfxctx = NULL;
  }
    
  if (hdc) SWELL_DeleteGfxContext(hdc);
  if (isView && hdc)
  {
#ifndef SWELL_NO_METAL
    if ([(id)h isKindOfClass:[SWELL_hwndChild class]] && [(SWELL_hwndChild *)h swellWantsMetal])
    {
      SWELL_hwndChild *wnd = (SWELL_hwndChild*)h;
      if (wnd->m_metal_dc_dirty == 1)
      {
        if (WDL_NOT_NORMALLY(wnd->m_use_metal == 1))
        {
          NSLog(@"swell-cocoa: metal(1) surface %p had write in GetDC()/ReleaseDC(), this is unsupported, use a metal(2) surface\n",wnd);
        }
        swell_addMetalDirty(wnd,NULL,true);
      }
    }
    else
#endif
      [(NSView *)h unlockFocus];
  }
}

void SWELL_FillDialogBackground(HDC hdc, const RECT *r, int level)
{
  CGContextRef ctx=(CGContextRef)SWELL_GetCtxGC(hdc);
  if (ctx)
  {
    bool ok = false;
    if (SWELL_GDI_GetOSXVersion()>=0x10d0)
    {
      NSColor *c = [NSColor windowBackgroundColor];
      if ([c respondsToSelector:@selector(CGColor)])
      {
        CGContextSetFillColorWithColor(ctx, [c CGColor]);
        ok = true;
      }
    }

    if (!ok)
      HIThemeSetFill(kThemeBrushDialogBackgroundActive,NULL,ctx,kHIThemeOrientationNormal);

    CGRect rect=CGRectMake(r->left,r->top,r->right-r->left,r->bottom-r->top);
    CGContextFillRect(ctx,rect);	         
  }
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


HBITMAP CreateBitmap(int width, int height, int numplanes, int bitsperpixel, unsigned char* bits)
{
  int spp = bitsperpixel/8;
  Boolean hasa = (bitsperpixel == 32);
  Boolean hasp = (numplanes > 1); // won't actually work yet for planar data
  NSBitmapImageRep* rep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:0 pixelsWide:width pixelsHigh:height 
                                                               bitsPerSample:8 samplesPerPixel:spp
                                                               hasAlpha:hasa isPlanar:hasp
                                                               colorSpaceName:NSDeviceRGBColorSpace
                                                                bitmapFormat:NSAlphaFirstBitmapFormat 
                                                                 bytesPerRow:0 bitsPerPixel:0];    
  if (!rep) return 0;
  unsigned char* p = [rep bitmapData];
  const int pspan = (int)[rep bytesPerRow]; // might not be the same as width
  
  for (int y=0;y<height;y ++)
  {
#ifdef __ppc__
    memcpy(p,bits,width*4);
#else
    unsigned char *wr = p;
    const unsigned char *rd = bits;
    int x = width;
    // convert BGRA to ARGB
    while (x--)
    {
      wr[0] = rd[3];
      wr[1] = rd[2];
      wr[2] = rd[1];
      wr[3] = rd[0];
      wr+=4;
      rd+=4;
    }
#endif
    p+=pspan;
    bits += width*4;
  }

  NSImage* img = [[NSImage alloc] init];
  [img addRepresentation:rep]; 
  [rep release];
  
  HGDIOBJ__* obj = GDP_OBJECT_NEW();
  obj->type = TYPE_BITMAP;
  obj->wid = 1; // need free
  obj->bitmapptr = img;
  return obj;
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
  ImageList_Remove(list, -1);
  delete (WDL_PtrList<HGDIOBJ__>*)list;
}

int ImageList_ReplaceIcon(HIMAGELIST list, int offset, HICON image)
{
  if (!image || !list) return -1;
  WDL_PtrList<HGDIOBJ__> *l=(WDL_PtrList<HGDIOBJ__> *)list;

  HGDIOBJ__ *imgsrc = (HGDIOBJ__*)image;
  if (!HGDIOBJ_VALID(imgsrc,TYPE_BITMAP)) return -1;

  HGDIOBJ__* icon=GDP_OBJECT_NEW();
  icon->type=TYPE_BITMAP;
  icon->wid=1;
  icon->bitmapptr = imgsrc->bitmapptr; // no need to duplicate it, can just retain a copy
  [icon->bitmapptr retain];
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
  icon->type=TYPE_BITMAP;
  icon->wid=1;
  NSImage *nsimg = [imgsrc->bitmapptr copy]; // caller still owns the image
  [nsimg setFlipped:YES];
  icon->bitmapptr = nsimg;
  image = (HICON) icon;
  
  l->Add(image);
  return l->GetSize();
}

int AddFontResourceEx(LPCTSTR str, DWORD fl, void *pdv)
{
  if (SWELL_GDI_GetOSXVersion() < 0x1060)  return 0;
  static bool l;
  static bool (*_CTFontManagerRegisterFontsForURL)( CFURLRef fontURL, uint32_t scope, CFErrorRef *error );
  if (!l)
  {
    CFBundleRef b = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.CoreText"));
    if (b)
    {
      *(void **)&_CTFontManagerRegisterFontsForURL = CFBundleGetFunctionPointerForName(b,CFSTR("CTFontManagerRegisterFontsForURL"));
    }

    l=true;
  }
  
  if (!_CTFontManagerRegisterFontsForURL) return 0;
  
  CFStringRef s=(CFStringRef)CStringToNSString(str); 

  CFURLRef r=CFURLCreateWithFileSystemPath(NULL,s,kCFURLPOSIXPathStyle,true);
  CFErrorRef err=NULL;
  const int v = _CTFontManagerRegisterFontsForURL(r,
      (fl & FR_PRIVATE) ? 1/*kCTFontManagerScopeProcess*/ : 2/*kCTFontManagerScopeUser*/,
      &err)?1:0;

  // release err? don't think so

  CFRelease(s);
  CFRelease(r);
  return v;
}

bool SWELL_osx_is_dark_mode(int mode) // mode=0 for enabled, 1=allowed
{
  static char c;
  if (!c)
  {
    NSUserDefaults *def = SWELL_GetOSXVersion() >= 0x10d0 ? [NSUserDefaults standardUserDefaults] : NULL;
    c = (def && [def objectForKey:@"NSRequiresAquaSystemAppearance"] && [def boolForKey:@"NSRequiresAquaSystemAppearance"] == NO) ? 1 : -1;
  }
  if (c<0) return false;
  if (mode == 1) return true;

  return [[[NSUserDefaults standardUserDefaults] stringForKey:@"AppleInterfaceStyle"] isEqualToString:@"Dark"];
}


#endif
