/*
    WDL - gpu.cpp
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

#include "gpu.h"

static int static_disableOpenGl = 0;

WDL_GPU::WDL_GPU()
{
  m_glDll = NULL;
  m_rc = NULL;
}

WDL_GPU::~WDL_GPU()
{
  release();
}

void WDL_GPU::release()
{
  if(m_glDll) 
  {
    if(m_rc)
    {
      wglMakeCurrent(NULL, NULL);
      wglDeleteContext(m_rc);
    }
    FreeLibrary(m_glDll);
    m_glDll = NULL;
  }
}

int WDL_GPU::init(HWND hwnd)
{
  m_hwnd = hwnd;

  if(static_disableOpenGl) return 0;

  m_glDll = LoadLibrary("opengl32.dll");
  if(!m_glDll) return 0;

  *((int *)&wglCreateContext) = (int)GetProcAddress(m_glDll, "wglCreateContext");
  *((int *)&wglDeleteContext) = (int)GetProcAddress(m_glDll, "wglDeleteContext");
  *((int *)&wglMakeCurrent) = (int)GetProcAddress(m_glDll, "wglMakeCurrent");
  *((int *)&wglGetProcAddress) = (int)GetProcAddress(m_glDll, "wglGetProcAddress");
  *((int *)&glClearColor) = (int)GetProcAddress(m_glDll, "glClearColor");
  *((int *)&glClear) = (int)GetProcAddress(m_glDll, "glClear");
  *((int *)&glEnable) = (int)GetProcAddress(m_glDll, "glEnable");
  *((int *)&glDisable) = (int)GetProcAddress(m_glDll, "glDisable");
  *((int *)&glBlendFunc) = (int)GetProcAddress(m_glDll, "glBlendFunc");
  *((int *)&glLineWidth) = (int)GetProcAddress(m_glDll, "glLineWidth");
  *((int *)&glColor3f) = (int)GetProcAddress(m_glDll, "glColor3f");
  *((int *)&glBegin) = (int)GetProcAddress(m_glDll, "glBegin");
  *((int *)&glEnd) = (int)GetProcAddress(m_glDll, "glEnd");
  *((int *)&glFlush) = (int)GetProcAddress(m_glDll, "glFlush");
  *((int *)&glVertex2f) = (int)GetProcAddress(m_glDll, "glVertex2f");
  *((int *)&glFinish) = (int)GetProcAddress(m_glDll, "glFinish");
  *((int *)&glGetString) = (int)GetProcAddress(m_glDll, "glGetString");
  *((int *)&glReadPixels) = (int)GetProcAddress(m_glDll, "glReadPixels");

  if(!wglGetProcAddress)
  {
    FreeLibrary(m_glDll);
    m_glDll = NULL;
    return 0;
  }

  HDC pdc = GetDC(m_hwnd);

  PIXELFORMATDESCRIPTOR pfd;
	ZeroMemory( &pfd, sizeof( pfd ) );
	pfd.nSize = sizeof( pfd );
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 24;
	pfd.cDepthBits = 16;
	pfd.iLayerType = PFD_MAIN_PLANE;
	int format = ChoosePixelFormat( pdc, &pfd );
	SetPixelFormat( pdc, format, &pfd );

  m_rc = wglCreateContext(pdc);

  if(!m_rc)
  {
    ReleaseDC(m_hwnd, pdc);
    FreeLibrary(m_glDll);
    m_glDll = NULL;
    return 0;
  }
  wglMakeCurrent(pdc, m_rc);

  char *rend = (char *)glGetString(GL_RENDERER);
  if(!rend || (rend && strstr(rend, "GDI"))) goto ret; //opengl software rendering is slooooow

  wglGetExtensionsStringARB = (PFNWGLGETEXTENSIONSSTRINGARBPROC)wglGetProcAddress("wglGetExtensionsStringARB");
  
  if(!wglGetExtensionsStringARB)
  {
ret:
    ReleaseDC(m_hwnd, pdc);
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(m_rc);
    FreeLibrary(m_glDll);
    m_glDll = NULL;
    return 0;
  }
  
  char *ext = NULL;
  ext = (char*)wglGetExtensionsStringARB( pdc );
  if(!strstr( ext, "WGL_ARB_pbuffer" )) goto ret;

  wglCreatePbufferARB    = (PFNWGLCREATEPBUFFERARBPROC)wglGetProcAddress("wglCreatePbufferARB");
	wglGetPbufferDCARB     = (PFNWGLGETPBUFFERDCARBPROC)wglGetProcAddress("wglGetPbufferDCARB");
	wglReleasePbufferDCARB = (PFNWGLRELEASEPBUFFERDCARBPROC)wglGetProcAddress("wglReleasePbufferDCARB");
	wglDestroyPbufferARB   = (PFNWGLDESTROYPBUFFERARBPROC)wglGetProcAddress("wglDestroyPbufferARB");
	wglQueryPbufferARB     = (PFNWGLQUERYPBUFFERARBPROC)wglGetProcAddress("wglQueryPbufferARB");
  if( !wglCreatePbufferARB || !wglGetPbufferDCARB || !wglReleasePbufferDCARB || !wglDestroyPbufferARB || !wglQueryPbufferARB ) goto ret;
  
  wglGetPixelFormatAttribivARB = (PFNWGLGETPIXELFORMATATTRIBIVARBPROC)wglGetProcAddress("wglGetPixelFormatAttribivARB");
	wglGetPixelFormatAttribfvARB = (PFNWGLGETPIXELFORMATATTRIBFVARBPROC)wglGetProcAddress("wglGetPixelFormatAttribfvARB");
	wglChoosePixelFormatARB      = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB");

	if( !wglGetExtensionsStringARB || !wglCreatePbufferARB || !wglGetPbufferDCARB ) goto ret;

  ReleaseDC(m_hwnd, pdc);

  return 1;
}

WDL_GPU_Surface *WDL_GPU::createSurface(WDL_WinMemBitmap *bm, int w, int h)
{
  if(!isInited()) return 0;
  return new WDL_GPU_Surface(this, bm, w, h);
}

static LRESULT CALLBACK staticWndProc(
  HWND hwnd,      // handle to window
  UINT uMsg,      // message identifier
  WPARAM wParam,  // first message parameter
  LPARAM lParam   // second message parameter
)
{
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

WDL_GPU_Surface::WDL_GPU_Surface(WDL_GPU *parent, WDL_WinMemBitmap *bm, int w, int h)
{
  m_parent = parent;
  m_bm = bm;
  m_hPBuffer = NULL;
  m_hDC = NULL;
  m_hRC = NULL;

  //m_w = ((w+15)/16)*16; m_h = ((h+15)/16)*16;
  m_w = w; m_h = h;
  
  BITMAPINFO m_bmi;
  memset(&m_bmi, 0, sizeof(BITMAPINFO));
  m_bmi.bmiHeader.biSize		= sizeof(BITMAPINFOHEADER);
  m_bmi.bmiHeader.biWidth		= m_w;
  m_bmi.bmiHeader.biHeight		= m_h;
  m_bmi.bmiHeader.biPlanes		= 1;
  m_bmi.bmiHeader.biBitCount	= 32;
  m_bmi.bmiHeader.biCompression	= BI_RGB;
  m_bmi.bmiHeader.biSizeImage	= m_w * m_h * 4;
  m_bmp = CreateDIBSection(m_bm->GetDC(), &m_bmi, DIB_RGB_COLORS, &m_bits, NULL, 0);
  m_oldbm = (HBITMAP)SelectObject(m_bm->GetDC(), m_bmp);

  //create pbuffer for offscreen rendering
  int pf_attr[] =
	{
		WGL_SUPPORT_OPENGL_ARB, TRUE,       // P-buffer will be used with OpenGL
		WGL_DRAW_TO_PBUFFER_ARB, TRUE,      // Enable render to p-buffer
    WGL_BIND_TO_TEXTURE_RGBA_ARB, TRUE, // some cards need that in order not to crash in wglCreatePbufferARB
		WGL_RED_BITS_ARB, 8,                // At least 8 bits for RED channel
		WGL_GREEN_BITS_ARB, 8,              // At least 8 bits for GREEN channel
		WGL_BLUE_BITS_ARB, 8,               // At least 8 bits for BLUE channel
		WGL_ALPHA_BITS_ARB, 8,              // At least 8 bits for ALPHA channel
		WGL_DEPTH_BITS_ARB, 16,             // At least 16 bits for depth buffer
		WGL_DOUBLE_BUFFER_ARB, FALSE,       // We don't require double buffering
		0                                   // Zero terminates the list
	};

  HDC pdc = GetDC(m_parent->m_hwnd);

  try 
  {
    unsigned int count = 0;
    int pixelFormat;
    m_parent->wglChoosePixelFormatARB( pdc,(const int*)pf_attr, NULL, 1, &pixelFormat, &count);
    if(!count)
    {
      ReleaseDC(m_parent->m_hwnd, pdc);
      m_parent->release();
      return;
    }
    
    m_hPBuffer = m_parent->wglCreatePbufferARB( pdc, pixelFormat, m_w, m_h, NULL );
    if(!m_hPBuffer)
    {
      ReleaseDC(m_parent->m_hwnd, pdc);
      m_parent->release();
      return;
    }
  }
  catch(...)
  {
    //stupid opengl driver crashing, lets make sure we don't retry to reinitialize the whole thing
    static_disableOpenGl = 1;
    ReleaseDC(m_parent->m_hwnd, pdc);
    m_parent->release();
    return;
  }

  m_hDC = m_parent->wglGetPbufferDCARB( m_hPBuffer );
  m_hRC = m_parent->wglCreateContext( m_hDC ); 

  ReleaseDC(m_parent->m_hwnd, pdc);
}

WDL_GPU_Surface::~WDL_GPU_Surface()
{
  if(m_hRC) 
  {
    m_parent->wglMakeCurrent(m_hDC, m_hRC);
    m_parent->wglDeleteContext(m_hRC);
    m_parent->wglReleasePbufferDCARB( m_hPBuffer, m_hDC );
		m_parent->wglDestroyPbufferARB( m_hPBuffer );
    ReleaseDC( m_parent->m_hwnd, m_hDC );
  }
  if(m_parent->isInited()) m_parent->wglMakeCurrent(NULL, NULL);
  SelectObject(m_bm->GetDC(), m_oldbm);
  DeleteObject(m_bmp);
}

void WDL_GPU_Surface::clear(float cr, float cg, float cb)
{
  m_parent->glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
  m_parent->glClear( GL_COLOR_BUFFER_BIT );
}

void WDL_GPU_Surface::beginScene()
{
  if(!m_hRC) return;
  m_parent->wglMakeCurrent(m_hDC, m_hRC);
}

void WDL_GPU_Surface::setLineAA(int on)
{
  if(on)
  {
    m_parent->glEnable(GL_LINE_SMOOTH);
    m_parent->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    m_parent->glEnable(GL_BLEND);
  }
  else
  {
    m_parent->glDisable(GL_LINE_SMOOTH);
  }
}

void WDL_GPU_Surface::beginLine(float cr, float cg, float cb)
{
  m_parent->glLineWidth (1);
  m_parent->glBegin (GL_LINES);
  m_parent->glColor3f (cb, cg, cr);
}

void WDL_GPU_Surface::end()
{
  m_parent->glEnd ();
  m_parent->glFinish();
}

void WDL_GPU_Surface::drawLine(int x1, int y1, int x2, int y2)
{
  float w = (float)m_w;
  float h = (float)m_h;
  float fx1 = ((float)x1/(w/2))-1;
  float fy1 = -((float)y1/(h/2))+1;
  float fx2 = ((float)x2/(w/2))-1;
  float fy2 = -((float)y2/(h/2))+1;
  m_parent->glVertex2f(fx1,fy1);
  m_parent->glVertex2f(fx2,fy2);
}

void WDL_GPU_Surface::flush()
{
  m_parent->glFlush();
}

void WDL_GPU_Surface::blit()
{
  if(!m_hRC) return;
	m_parent->glReadPixels( 0, 0, m_w, m_h, GL_RGBA, GL_UNSIGNED_BYTE, m_bits);
}
