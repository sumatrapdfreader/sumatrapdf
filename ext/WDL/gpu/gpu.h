/*
    WDL - gpu.h
    Copyright (C) 2007 Cockos Incorporated
  
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
    
*/

#ifndef _WDL_GPU_H
#define _WDL_GPU_H

#ifdef _WIN32

#include <windows.h>
#include <gl/gl.h>
#include "wglext.h"

#include "../wingui/membitmap.h"

class WDL_GPU_Surface;

class WDL_GPU
{
public:
  WDL_GPU();
  ~WDL_GPU();

  int init(HWND hwnd);
  void release();
  int isInited() { return m_glDll != NULL; }

  WDL_GPU_Surface *createSurface(WDL_WinMemBitmap *bm, int w, int h);

  HGLRC (WINAPI *wglCreateContext)(HDC dc);
  BOOL  (WINAPI *wglMakeCurrent)(HDC dc, HGLRC rc);
  BOOL  (WINAPI *wglDeleteContext)(HGLRC rc);
  PROC (WINAPI *wglGetProcAddress)(LPCSTR name);

  void (WINAPI *glClearColor)(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
  void (WINAPI *glClear)(GLbitfield mask);
  void (WINAPI *glEnable)(GLenum cap);
  void (WINAPI *glDisable)(GLenum cap);
  void (WINAPI *glBlendFunc)(GLenum sfactor, GLenum dfactor);
  void (WINAPI *glLineWidth)(GLfloat width);
  void (WINAPI *glColor3f)(GLfloat red, GLfloat green, GLfloat blue);
  void (WINAPI *glBegin)(GLenum mode);
  void (WINAPI *glEnd)();
  void (WINAPI *glVertex2f)(GLfloat x, GLfloat y);
  void (WINAPI *glFlush)();
  void (WINAPI *glFinish)();
  const GLubyte *(WINAPI *glGetString)(GLenum name);
  void (WINAPI *glReadPixels)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);

  // WGL_ARB_extensions_string
  PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB;
  
  // WGL_ARB_pbuffer
  PFNWGLCREATEPBUFFERARBPROC    wglCreatePbufferARB;
  PFNWGLGETPBUFFERDCARBPROC     wglGetPbufferDCARB;
  PFNWGLRELEASEPBUFFERDCARBPROC wglReleasePbufferDCARB;
  PFNWGLDESTROYPBUFFERARBPROC   wglDestroyPbufferARB;
  PFNWGLQUERYPBUFFERARBPROC     wglQueryPbufferARB;
  
  // WGL_ARB_pixel_format
  PFNWGLGETPIXELFORMATATTRIBIVARBPROC wglGetPixelFormatAttribivARB;
  PFNWGLGETPIXELFORMATATTRIBFVARBPROC wglGetPixelFormatAttribfvARB;
  PFNWGLCHOOSEPIXELFORMATARBPROC      wglChoosePixelFormatARB;

  HINSTANCE m_glDll;
  HWND m_hwnd;
  HGLRC m_rc;
};

class WDL_GPU_Surface
{
public:
  WDL_GPU_Surface(WDL_GPU *parent, WDL_WinMemBitmap *bm, int w, int h);
  ~WDL_GPU_Surface();

  void clear(float cr, float cg, float cb);
  void beginScene();
  void setLineAA(int on);
  void beginLine(float cr, float cg, float cb);
  void end();
  void drawLine(int x1, int y1, int x2, int y2);
  void flush();
  void blit();
  
private:
  WDL_GPU *m_parent;
  WDL_WinMemBitmap *m_bm;
  HBITMAP m_oldbm, m_bmp;
  int m_w, m_h;

  HPBUFFERARB m_hPBuffer;
  HDC         m_hDC;
  HGLRC       m_hRC;
  void *m_bits;
};

#else

//todo

#endif

#endif