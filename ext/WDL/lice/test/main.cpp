/*
  Cockos WDL - LICE - Lightweight Image Compositing Engine
  Copyright (C) 2007 and later, Cockos Incorporated
  File: main.cpp (example use of LICE)
  See lice.h for license and other information
*/

#include "../lice.h"
#include "../../plush2/plush.h"
#include "../../MersenneTwister.h"
#include <math.h>
#include <stdio.h>

#ifndef _WIN32
#include <sys/time.h>
#include "../../swell/swell.h"
#endif

static double gettm()
{
#ifndef _WIN32
  struct timeval tm={0,};
   gettimeofday(&tm,NULL);
   return (double)tm.tv_sec + (double)tm.tv_usec/1000000;
#else
  LARGE_INTEGER freq;
  QueryPerformanceFrequency(&freq);

  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  return (double)now.QuadPart / (double)freq.QuadPart;
#endif
}

char g_status[1024];


#include "../lice_text.h"

#include "../lice_glbitmap.h"
//#define GLEW_STATIC
//#include "../glew/include/gl/glew.h"
//#include "../glew/include/gl/wglew.h"

//uncomment this to enable the ffmpeg video output in test 1
//#define FFMPEG_TEST

//#define TIMING
#include "../../timing.c"

#ifdef FFMPEG_TEST
//ffmpeg encoding test
#include "../../ffmpeg.h"
#pragma comment(lib, "../../../sdks/ffmpeg/lib/avcodec.lib")
#pragma comment(lib, "../../../sdks/ffmpeg/lib/avformat.lib")
#pragma comment(lib, "../../../sdks/ffmpeg/lib/avutil.lib")
#pragma comment(lib, "../../../sdks/ffmpeg/lib/swscale.lib")
static WDL_VideoEncode *m_encoder;
static WDL_VideoDecode *m_decoder;
#endif

#include "resource.h"

#define NUM_EFFECTS 25

const char *effect_names[NUM_EFFECTS] =
{
  "Rotated + Scaled blit",
  "Simple alpha blit",
  "Rotated blit",
  "Scaled blit",
  "GradRect",
  "Marble generator",
  "Wood generator",
  "Noise generator",
  "Circular noise generator",
  "GradRect + text + rotated blit",
  "PutPixel",
  "Line",
  "DrawText",
  "Icon loading",
  "Circles, Arc",
  "Rotated + Multiply add blit",
  "Transform blit",
  "Plush 3D",
  "3D Fly (use mouse)",
  "SVG loading (C:\\test.svg)",
  "GL acceleration (disabled)",
  "Bezier curves",
  "Convex polygon fill",
  "Palette generator (C:\\test.png)",
  "Triangle test",
};

HINSTANCE g_hInstance;
LICE_IBitmap *jpg;
LICE_IBitmap *bmp;
LICE_IBitmap *icon;
LICE_IBitmap *framebuffer;
static int m_effect = 11;
static int m_doeff = 0;

static LICE_IBitmap* tmpbmp = 0;

static DWORD m_start_time, m_frame_cnt;
bool m_cap;


static void DoPaint(HWND hwndDlg, HDC dc)
{
  RECT r;
  GetClientRect(hwndDlg, &r);
  
#ifdef _WIN32
  r.top+=40;
  if (r.top >= r.bottom) r.top=r.bottom-1;
#endif
  
  if (framebuffer->resize(r.right-r.left,r.bottom-r.top))
  {
    m_doeff=1;
   // memset(framebuffer->getBits(),0,framebuffer->getWidth()*framebuffer->getHeight()*4);
  }
  
  int x=rand()%(r.right+300)-150;
  int y=rand()%(r.bottom+300)-150;

  static int frame_cnt;
  static int s_preveff = -1;
  if (m_effect != s_preveff)
  {
    frame_cnt=0;
    s_preveff = m_effect;
    LICE_Clear(framebuffer, 0);
  }

  static MTRand s_rng;
  double t2=gettm();

  switch(m_effect)
  {
    case 23:
    {
      const int palnumcols=256;

      static int init=0;
      static LICE_IBitmap* srcbmp=0;
      static LICE_IBitmap* palbmp=0;
      static LICE_pixel palette[palnumcols] = { 0 };

      if (!init)
      {
        init=-1;
        srcbmp = LICE_LoadPNG("C:\\test.png");
        if (srcbmp)
        {          
          int n = LICE_BuildPalette(srcbmp, palette, palnumcols);
          palbmp = new LICE_MemBitmap;
          LICE_Copy(palbmp, srcbmp);
          void LICE_TestPalette(LICE_IBitmap*, LICE_pixel*, int);
          LICE_TestPalette(palbmp, palette, n);
          init=1;
        }
      }

      if (init > 0)
      {
        static int lastw=0;
        static int lasth=0;
        if (lastw != framebuffer->getWidth() || lasth != framebuffer->getHeight())
        {
          lastw = framebuffer->getWidth();
          lasth = framebuffer->getHeight();
          int y = framebuffer->getHeight()*3/4;
          LICE_ScaledBlit(framebuffer, srcbmp, 0, 0, lastw/2, y, 0, 0, srcbmp->getWidth(), srcbmp->getHeight(), 1.0f, LICE_BLIT_MODE_COPY|LICE_BLIT_FILTER_BILINEAR);
          LICE_ScaledBlit(framebuffer, palbmp, lastw/2, 0, lastw/2, y, 0, 0, palbmp->getWidth(), palbmp->getHeight(), 1.0f, LICE_BLIT_MODE_COPY|LICE_BLIT_FILTER_BILINEAR);

          int dy = (lasth-y)/4;
          int dx = lastw/(palnumcols/4);
          int i, j, k=0;
          for (i = 0; i < 4; ++i)
          {
            for (j = 0; j < palnumcols/4; ++j)
            {
              LICE_FillRect(framebuffer, j*dx, y+i*dy, dx, dy, palette[k++], 1.0f, LICE_BLIT_MODE_COPY);
            }
          }
        }
      }
    }
    break;
    case 24:
      {
        LICE_MemBitmap bm(128,128);
        LICE_Clear(framebuffer,0);
        LICE_Clear(&bm,0);

        static POINT sp;
        POINT p;
        if (!sp.x&&!sp.y) GetCursorPos(&sp);
        GetCursorPos(&p);
        p.x-=sp.x;
        p.y-=sp.y;
        int th=p.y+16;
        bool flip = th < 0;
        if (flip) th=-th;
        int tw=p.x+16;
        int cx=(tw+1)/2;
        int x=1,y=1;
        LICE_pixel lc=LICE_RGBA(255,0,255,255);
        LICE_Line(&bm,x,y,x+tw,y,lc,1.0f,LICE_BLIT_MODE_COPY);
        LICE_Line(&bm,x,y+th,x+tw,y+th,lc,1.0f,LICE_BLIT_MODE_COPY);
        LICE_Line(&bm,x+tw,y,x+tw,y+th,lc,1.0f,LICE_BLIT_MODE_COPY);
        LICE_Line(&bm,x,y,x,y+th,lc,1.0f,LICE_BLIT_MODE_COPY);

        LICE_FillTriangle(&bm,x+tw,y+(flip?th:0)/*+th/4*/,x+cx,y+(flip?0:th),x,y+(flip?th:0),LICE_RGBA(255,255,255,255),0.75f,LICE_BLIT_MODE_COPY);

        int sc=16;
        LICE_ScaledBlit(framebuffer,&bm,0,0,bm.getWidth()*sc,bm.getHeight()*sc,0,0,bm.getWidth(),bm.getHeight(),1.0,LICE_BLIT_MODE_COPY);
        for (y=0;y<bm.getHeight();y++)
          LICE_Line(framebuffer,0,y*sc,bm.getWidth()*sc,y*sc,LICE_RGBA(255,255,255,255),0.5,LICE_BLIT_MODE_COPY,false);
        for (x=0;x<bm.getWidth();x++)
          LICE_Line(framebuffer,x*sc,0,x*sc,bm.getHeight()*sc,LICE_RGBA(255,255,255,255),0.5,LICE_BLIT_MODE_COPY,false);
      }
    break;
    case 22:
    {
      static int x[16], y[16];

      int i;
      int w = framebuffer->getWidth();
      int h = framebuffer->getHeight();

      static bool init = false;
      if (!init)
      {
        init = true;
        for (i = 0; i < 16; ++i)
        {
          x[i] = s_rng.randInt(w-1); 
          y[i] = s_rng.randInt(h-1);
        }
      }

      for (i = 0; i < 16; ++i)
      {
        x[i] += s_rng.randNorm(0.0, 1.0)+0.5;
        y[i] += s_rng.randNorm(0.0, 1.0)+0.5;
        if (x[i] < 0) x[i] = 0;
        else if (x[i] >= w) x[i] = w-1;
        if (y[i] < 0) y[i] = 0;
        else if (y[i] >= h) y[i] = h-1;
      }

      LICE_Clear(framebuffer, 0);
      LICE_FillConvexPolygon(framebuffer, x, y, 16, LICE_RGBA(96,96,96,255), 0.5f, LICE_BLIT_MODE_ADD);

      for (i = 0; i < 16; ++i)
      {
        LICE_Line(framebuffer, x[i]-1, y[i], x[i]+1, y[i], LICE_RGBA(255,0,0,255), 1.0f, LICE_BLIT_MODE_COPY);
        LICE_Line(framebuffer, x[i], y[i]-1, x[i], y[i]+1, LICE_RGBA(255,0,0,255), 1.0f, LICE_BLIT_MODE_COPY);
      }
    }
    break;

    case 21:
    {
      int w = framebuffer->getWidth();
      int h = framebuffer->getHeight();

      int x0, y0, x1, y1, x2, y2, x3, y3;

      bool aa = true;
      float maxsegmentpx = 0.0f;

      x0 = w*(double)rand()/RAND_MAX;
      y0 = h*(double)rand()/RAND_MAX;
      x1 = w*(double)rand()/RAND_MAX;
      y1 = h*(double)rand()/RAND_MAX;
      x2 = w*(double)rand()/RAND_MAX;
      y2 = h*(double)rand()/RAND_MAX;
      LICE_DrawQBezier(framebuffer, x0, y0, x1, y1, x2, y2, LICE_RGBA(255,0,0,255), 1.0f, LICE_BLIT_MODE_COPY, aa, maxsegmentpx);

      x0 = w*(double)rand()/RAND_MAX;
      y0 = h*(double)rand()/RAND_MAX;
      x1 = w*(double)rand()/RAND_MAX;
      y1 = h*(double)rand()/RAND_MAX;
      x2 = w*(double)rand()/RAND_MAX;
      y2 = h*(double)rand()/RAND_MAX;
      x3 = w*(double)rand()/RAND_MAX;
      y3 = h*(double)rand()/RAND_MAX;
      LICE_DrawCBezier(framebuffer, x0, y0, x1, y1, x2, y2, x3, y3, LICE_RGBA(0,255,0,255), 1.0f, LICE_BLIT_MODE_COPY, aa, maxsegmentpx);
    }
    break;

#ifndef DISABLE_LICE_EXTENSIONS
    case 20:  // GL acceleration
    {
      int w = framebuffer->getWidth();
      int h = framebuffer->getHeight();

      int x, y, tw, th;

      static LICE_IBitmap* glbmp = 0;
      if (!glbmp) 
      {
        glbmp = new LICE_GL_SysBitmap(0, 0);
        glbmp->resize(w, h);

        glbmp = new LICE_GL_SubBitmap(glbmp, 20, 80, 50, 20);

        framebuffer = glbmp;
      }

      if (!tmpbmp)
      {
        tmpbmp = new LICE_GL_MemBitmap(0, 0);
        tmpbmp->resize(20, 20);
        //tmpbmp = new LICE_MemBitmap(20, 20);
      }
       
      LICE_Clear(tmpbmp, LICE_RGBA(255,0,0,255));
      LICE_Line(tmpbmp, 0, 0, 20, 20, LICE_RGBA(255,255,255,255), 1.0f, LICE_BLIT_MODE_COPY, true);      

      static int _n = 0;

      //if (_n < 3)
      {
        //LICE_Clear(glbmp, LICE_RGBA(0,0,0,0));

        x = w*rand()/RAND_MAX;
        y = h*rand()/RAND_MAX;
        LICE_Blit(glbmp, tmpbmp, x, y, 0, 0, 20, 20, 1.0f, LICE_BLIT_MODE_COPY);  // blit one GL bitmap to another

        x = w*rand()/RAND_MAX;
        y = h*rand()/RAND_MAX;
        LICE_ScaledBlit(glbmp, tmpbmp, x, y, 40, 40, 0, 0, 20, 20, 1.0f, LICE_BLIT_MODE_COPY);  // blit one GL bitmap to another

        x = w*rand()/RAND_MAX;
        y = h*rand()/RAND_MAX;
        tw = (w-x)*rand()/RAND_MAX;
        th = (h-y)*rand()/RAND_MAX;
        int color = (_n%2 ? LICE_RGBA(63,63,63,255) : LICE_RGBA(0,0,0,255));
        LICE_FillRect(glbmp, x, y, tw, th, color, 1.0f, LICE_BLIT_MODE_COPY);
  
        x = w*rand()/RAND_MAX;
        y = h*rand()/RAND_MAX;
        tw = (w-x)*rand()/RAND_MAX;
        th = (h-y)*rand()/RAND_MAX;
        LICE_Line(glbmp,  x, y, x+tw, y+th, LICE_RGBA(255,0,0,255), 1.0f, LICE_BLIT_MODE_COPY, true);
  
        int x0 = w*rand()/RAND_MAX;
        int y0 = h*rand()/RAND_MAX;
        int x1 = w*rand()/RAND_MAX;
        int y1 = h*rand()/RAND_MAX;
        int x2 = w*rand()/RAND_MAX;
        int y2 = h*rand()/RAND_MAX;
        int x3 = w*rand()/RAND_MAX;
        int y3 = h*rand()/RAND_MAX;
        LICE_DrawCBezier(glbmp, x0, y0, x1, y1, x2, y2, x3, y3, LICE_RGBA(0,255,0,255), 1.0f, LICE_BLIT_MODE_COPY, true);

        #define A(x) ((LICE_pixel_chan)((x)*255.0+0.5))
  
        LICE_pixel_chan alphas[81] =
        { 
          A(0.00), A(0.12), A(0.69), A(1.00), A(1.00), A(1.00), A(0.69), A(0.12), A(0.00),
          A(0.12), A(0.94), A(0.82), A(0.31), A(0.25), A(0.31), A(0.82), A(0.94), A(0.12),
          A(0.69), A(0.82), A(0.06), A(0.00), A(0.00), A(0.00), A(0.06), A(0.82), A(0.69),
          A(1.00), A(0.31), A(0.00), A(0.00), A(0.00), A(0.00), A(0.00), A(0.31), A(1.00),
          A(1.00), A(0.19), A(0.00), A(0.00), A(0.00), A(0.00), A(0.00), A(0.19), A(1.00),
          A(1.00), A(0.31), A(0.00), A(0.00), A(0.00), A(0.00), A(0.00), A(0.31), A(1.00),
          A(0.69), A(0.82), A(0.06), A(0.00), A(0.00), A(0.00), A(0.06), A(0.82), A(0.69),
          A(0.12), A(0.94), A(0.82), A(0.31), A(0.25), A(0.31), A(0.82), A(0.94), A(0.12),
          A(0.00), A(0.12), A(0.69), A(1.00), A(1.00), A(1.00), A(0.69), A(0.12), A(0.00)
        };
        int gw = 9;
        int gh = 9;
        
        x = w*rand()/RAND_MAX;
        y = h*rand()/RAND_MAX;
        LICE_DrawGlyph(glbmp, x, y, LICE_RGBA(255,255,0,255), alphas, gw, gh, 1.0f, LICE_BLIT_MODE_COPY);

        static LICE_CachedFont* font = 0;
        if (!font)
        {
          font = new LICE_CachedFont;
          LOGFONT lf={ 12, 0, 0, 0, FW_LIGHT, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Arial"};
          HFONT hf = CreateFontIndirect(&lf);
          font->SetFromHFont(hf, 0);
          font->SetTextColor(LICE_RGBA(255,255,0,255));
        } 
        
        x = w*rand()/RAND_MAX;
        y = h*rand()/RAND_MAX;
        RECT r = { x, y, x+40, y+10 };
        font->DrawText(glbmp, "foo bar", -1, &r, 0);

      }
      ++_n;
    }
    break;
#endif

    case 18:
      {
        void doFlyEffect(LICE_IBitmap *fb,HWND);
        doFlyEffect(framebuffer,m_cap ? hwndDlg : NULL);
      }
    break;

    case 17:
      {
        static pl_Obj *obj=NULL,*obj2=NULL;
        static LICE_IBitmap *framebuffer2;
        if (!framebuffer2) framebuffer2=new LICE_MemBitmap;
        LICE_Copy(framebuffer2,framebuffer);
        if (!obj)
        {
          pl_Mat *mat = new pl_Mat;
          pl_Mat *mat2 = new pl_Mat;

          mat2->Smoothing=false;
          mat2->Ambient[0]=mat2->Ambient[1]=mat2->Ambient[2]=0.0;
          mat2->Diffuse[0]=mat2->Diffuse[1]=0.6;
          mat2->Diffuse[2]=1.0;

          mat->Ambient[0]=mat->Ambient[1]=mat->Ambient[2]=0.0;
          mat->Diffuse[0]=mat->Diffuse[1]=1.9;
          mat->Diffuse[2]=0.4;

          mat->PerspectiveCorrect=16;
          mat->SolidCombineMode=LICE_BLIT_MODE_COPY;
          mat->SolidOpacity=1.0;
          mat->Smoothing=true;
          mat->Lightable=true;
          //mat->FadeDist = 300.0;

          mat2->Texture=bmp;
          mat2->TexOpacity=0.5;
          mat2->TexCombineMode=LICE_BLIT_MODE_MUL|LICE_BLIT_FILTER_BILINEAR;
          mat2->SolidOpacity=0.4;
          mat2->BackfaceCull=false;
          mat2->BackfaceIllumination=1.0;
          mat2->Texture2=framebuffer2;
          mat2->Tex2MapIdx=-1;
          mat2->Tex2Opacity=0.75;

          mat->Texture=bmp;
          LICE_TexGen_Marble(mat->Texture = new LICE_MemBitmap(r.right,r.bottom),NULL,0.3,0.4,0.0,1.0f);

          mat->TexOpacity=0.5;
          mat->TexScaling[0]=mat->TexScaling[1]=3.0;
          mat->TexCombineMode=LICE_BLIT_MODE_MUL|LICE_BLIT_FILTER_BILINEAR;

          LICE_TexGen_Noise(mat->Texture2 = new LICE_MemBitmap(r.right,r.bottom),NULL,0.3,0.4,0.0,1.0f);
        //  mat->Texture2=icon;
          mat->Tex2MapIdx=-1;
          mat->Tex2CombineMode=LICE_BLIT_MODE_ADD|LICE_BLIT_FILTER_BILINEAR;
          mat->Tex2Opacity=0.8;
          mat->Tex2Scaling[0]=2.0;
          mat->Tex2Scaling[1]=-2.0;

          mat->BackfaceCull=true;
          mat->BackfaceIllumination=0.0;

          obj=plMakeTorus(100.0,80.0,40,40,mat);          

          int x;
          if (0)for(x=1;x<3;x++)
          {
            pl_Obj *no = obj->Clone();
            no->Translate(0,40.0,-x*35.0);
            obj->Children.Add(no);
            no->Xa += 50.35*x;
            no->Ya -= 30.13*x;
          }
          obj2=plMakeSphere(58,20,20,mat2);
          obj2->Zp -= 30.0;
          obj->Zp += 30.0;

          /*pl_Obj *o = plRead3DSObj("c:\\temp\\suzanne.3ds",mat);
          if (o)
          {
            o->Scale(30.0);
            o->Translate(150.0,0,0);
            obj->Children.Add(o);
          }
          */
        }
        obj2->Xa+=0.3;
        obj2->Ya+=-0.1;
        obj->Ya+=0.1;
        obj->Xa+=0.1;
        obj->Za+=0.1;
        obj->GenMatrix=true;

        if (1) LICE_Clear(framebuffer,0);
        else {
          double a=GetTickCount()/1000.0;
        
          double scale=(1.1+sin(a)*0.3);
      
          LICE_RotatedBlit(framebuffer,framebuffer,0,0,r.right,r.bottom,0+sin(a*0.3)*16.0,0+sin(a*0.21)*16.0,r.right,r.bottom,cos(a*0.5)*0.13,false,254/255.0,LICE_BLIT_MODE_COPY|LICE_BLIT_FILTER_BILINEAR);
        }
        static pl_Cam cam;
        LICE_SubBitmap tmpbm(framebuffer,10,10,framebuffer->getWidth()-20,framebuffer->getHeight()-20);
        //cam.CenterX = (tmpbm.getWidth()/2+80);
        //cam.CenterY = (tmpbm.getHeight()/2+80);
        cam.AspectRatio = 1.0;//cam.frameBuffer->getWidth()* 3.0/4.0 / (double)cam.frameBuffer->getHeight();
        cam.X = cam.Y = 0.0;
        cam.Z = -200.0;
        cam.WantZBuffer=true;
        cam.SetTarget(0,0,0);

        

        static pl_Light light;
        light.Set(PL_LIGHT_POINT,500.0,0,-900.0,1.3f,0.5f,0.5f,1000.0);
        static pl_Light light2;
        light2.Set(PL_LIGHT_POINT,-500.0,0,-700.0,0.0f,1.0f,0.5f,1000.0);
        cam.ClipBack=220.0;

        cam.Begin(&tmpbm);
        cam.RenderLight(&light);
        cam.RenderLight(&light2);
        cam.RenderObject(obj);
        cam.SortToCurrent();
        cam.RenderObject(obj2);
        cam.End();

        char buf[512];
        sprintf(buf,"tri: %d->%d->%d, pix=%.0f",
          cam.RenderTrisIn,
          cam.RenderTrisCulled,
          cam.RenderTrisOut,cam.RenderPixelsOut);
        LICE_DrawText(framebuffer,0,10,buf,LICE_RGBA(255,255,255,255),1.0f,0);

      }
    break;
    case 15:
    case 0:
    {
      double a=.51;//GetTickCount()/1000.0;
      
      double scale=(1.1+sin(a)*0.3);
      
      if (0)  // weirdness
      {
        LICE_RotatedBlit(framebuffer,framebuffer,0,0,r.right,r.bottom,0+sin(a*0.3)*16.0,0+sin(a*0.21)*16.0,r.right,r.bottom,cos(a*0.5)*0.13,false,254/255.0,LICE_BLIT_MODE_COPY|LICE_BLIT_FILTER_BILINEAR);
      }
      else // artifact-free mode
      {
        LICE_MemBitmap framebuffer_back;
        
        LICE_Copy(&framebuffer_back,framebuffer);
        LICE_RotatedBlit(framebuffer,&framebuffer_back,0,0,r.right,r.bottom,0+sin(a*0.3)*16.0,0+sin(a*0.21)*16.0,r.right,r.bottom,cos(a*0.5)*0.13,false,1.0,LICE_BLIT_MODE_COPY|LICE_BLIT_FILTER_BILINEAR);
        timingEnter(0);
        LICE_ScaledBlit(framebuffer,&framebuffer_back,0,0,r.right,r.bottom,-200,-200,3000,3000,0.1,LICE_BLIT_MODE_COPY|LICE_BLIT_FILTER_BILINEAR);
        timingLeave(0);
        timingEnter(1);
        LICE_ScaledBlit(framebuffer,&framebuffer_back,0,0,r.right,r.bottom,0,0,r.right/2,r.bottom/2,0.1,LICE_BLIT_MODE_COPY|LICE_BLIT_FILTER_BILINEAR);
        timingLeave(1);
      }
      //LICE_Clear(framebuffer,0);
      if (bmp) LICE_RotatedBlit(framebuffer,bmp,r.right*scale,r.bottom*scale,r.right*(1.0-scale*2.0),r.bottom*(1.0-scale*2.0),0,0,bmp->getWidth(),bmp->getHeight(),cos(a*0.3)*13.0,false,0.3,LICE_BLIT_MODE_ADD|LICE_BLIT_USE_ALPHA|LICE_BLIT_FILTER_BILINEAR);
      
      if (m_effect==15)
      {
        LICE_MultiplyAddRect(framebuffer,0,0,framebuffer->getWidth(),framebuffer->getHeight(),0.9,0.9,-0.3,1,
                             3,2,200,0);
      }
      

#ifdef FFMPEG_TEST

#if 0
      //ffmpeg encoding test
      if(!m_encoder) m_encoder = new WDL_VideoEncode("flv", framebuffer->getWidth(),framebuffer->getHeight(), 25, 1256, NULL, 44100, 1, 128);
      if(m_encoder->isInited())
      {
        //set the alpha bit to 0xff
        //LICE_FillRect(framebuffer, 0, 0, framebuffer->getWidth(), framebuffer->getHeight(), LICE_RGBA(0,0,0,255), 1.0f, LICE_BLIT_MODE_ADD);
        m_encoder->encodeVideo(framebuffer->getBits());

        static short audiodata[2000]={0,};
        static int initaudio=0;
        if(!initaudio)
        {
          float t = 0;
          for(int j=0;j<2000;j++) 
          {
            audiodata[j] = (int)(sin(t) * 10000);
            t += 2 * M_PI * 440.0 / 44100;
          }          
          initaudio = 1;
        }
        m_encoder->encodeAudio(audiodata, 44100/25);

        static WDL_HeapBuf h;
        h.Resize(256*1024);
        unsigned char *p = (unsigned char *)h.Get();
        int s = m_encoder->getBytes(p, 256*1024);
        if(s)
        {
          FILE *fh = fopen("c:\\temp\\out.flv", "ab");
          fwrite(p, s, 1, fh);
          fclose(fh);
        }
      }
#else
      //ffmpeg decoding test
      if(!m_decoder) m_decoder = new WDL_VideoDecode("c:\\test.avi");
      if(m_decoder->isInited())
      {
        static LICE_IBitmap *m_tmpframe;
        static double t = 0;
        if(!m_tmpframe)
        {
          m_tmpframe = new LICE_MemBitmap(framebuffer->getWidth(), framebuffer->getHeight());
        }
        m_decoder->GetVideoFrameAtTime(m_tmpframe, t, NULL, &t, true);
        t+=0.0001;
        LICE_Blit(framebuffer, m_tmpframe, 0, 0, 0, 0, m_tmpframe->getWidth(), m_tmpframe->getHeight(), 1.0f, 0);
      }
#endif

#endif
      

    }
      break;
    case 1:
      if (rand()%6==0)
        LICE_Blit(framebuffer,bmp,x,y,NULL,-1.4,LICE_BLIT_MODE_ADD|LICE_BLIT_USE_ALPHA);
      else
        LICE_Blit(framebuffer,bmp,x,y,NULL,0.6,LICE_BLIT_MODE_COPY|LICE_BLIT_USE_ALPHA);
      break;
    case 2:
    {
      LICE_Clear(framebuffer,0);
      double a=GetTickCount()/1000.0;
      
      double scale=(1.1+sin(a)*0.3);
      if (bmp) LICE_RotatedBlit(framebuffer,bmp,r.right*scale,r.bottom*scale,r.right*(1.0-scale*2.0),r.bottom*(1.0-scale*2.0),0,0,bmp->getWidth(),bmp->getHeight(),cos(a*0.3)*13.0,false,1.0,LICE_BLIT_MODE_ADD|LICE_BLIT_USE_ALPHA|LICE_BLIT_FILTER_BILINEAR,0.0,-bmp->getHeight()/2);
    }
      break;
    case 3:
    {
      LICE_Clear(framebuffer,LICE_RGBA(128,128,128,128));
      static double a;
      a+=0.003;
      int xsize=sin(a*1.1)*r.right*10.5;
      int ysize=sin(a*1.7)*r.bottom*10.5;
      int xp = sin(a*0.3+1515851)*r.right*0.5;
      int yp = sin(a*0.3+15853)*r.bottom*0.5;
      
      if (bmp)
      {
//        if (rand()%3==0)
  //        LICE_ScaledBlit(framebuffer,bmp,r.right/2-xsize/2,r.bottom/2-ysize/2,xsize,ysize,0.0,0.0,bmp->getWidth(),bmp->getHeight(),-0.7,LICE_BLIT_USE_ALPHA|LICE_BLIT_MODE_ADD|LICE_BLIT_FILTER_BILINEAR);
    ///    else
          LICE_ScaledBlit(framebuffer,bmp,xp + r.right/2-xsize/2,yp + r.bottom/2-ysize/2,xsize,ysize,-300,-300,600+bmp->getWidth(),600+bmp->getHeight(),1,LICE_BLIT_MODE_COPY|LICE_BLIT_FILTER_BILINEAR);
      }
    }
      break;
    case 4:
    case 9:
      
    {
      static double a;
      a+=0.003;
      
      LICE_GradRect(framebuffer,0,0,framebuffer->getWidth(),framebuffer->getHeight(),
                    0.5*sin(a*14.0),0.5*cos(a*2.0+1.3),0.5*sin(a*4.0),1.0,
                    (cos(a*37.0))/framebuffer->getWidth()*0.5,(sin(a*17.0))/framebuffer->getWidth()*0.5,(cos(a*7.0))/framebuffer->getWidth()*0.5,0,
                    (sin(a*12.0))/framebuffer->getHeight()*0.5,(cos(a*4.0))/framebuffer->getHeight()*0.5,(cos(a*3.0))/framebuffer->getHeight()*0.5,0,
                    LICE_BLIT_MODE_COPY);
      
      
      if (m_effect==9)
      {
        /*            LOGFONT lf={
        140,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,
        "Times New Roman"
        };
        HFONT font=CreateFontIndirect(&lf);
        
        */
        
        
        LICE_SysBitmap bm(60,60);
        LICE_Clear(&bm,LICE_RGBA(0,0,0,0));
        SetTextColor(bm.getDC(),RGB(255,255,255));
        SetBkMode(bm.getDC(),TRANSPARENT);
        //            HGDIOBJ of=SelectObject(bm.getDC(),font);
        RECT r={0,0,bm.getWidth(),bm.getHeight()};
        DrawText(bm.getDC(),"LICE",-1,&r,DT_LEFT|DT_TOP|DT_SINGLELINE);
        //        SelectObject(bm.getDC(),of);
        //          DeleteObject(font);
        
        LICE_Blit(&bm,&bm,0,0,NULL,1.0,LICE_BLIT_MODE_CHANCOPY|LICE_PIXEL_R|(LICE_PIXEL_A<<2));
        
        int bmw=bm.getWidth();
        int bmh=bm.getHeight();
        LICE_FillRect(framebuffer,framebuffer->getWidth()/2,framebuffer->getHeight()/2,bmh,bmw,0,0.5,LICE_BLIT_MODE_COPY);
        LICE_RotatedBlit(framebuffer,&bm,
          framebuffer->getWidth()/2,framebuffer->getHeight()/2,
              bmh,bmw,0,0,bmw,bmh,
          3.14159*0.5,false,.4,LICE_BLIT_MODE_COPY|LICE_BLIT_USE_ALPHA|LICE_BLIT_FILTER_BILINEAR,-bm.getWidth()/4,-bm.getHeight()/4);
      }
      
      break;
    }
    case 5:
      if(m_doeff)
      {
        LICE_TexGen_Marble(framebuffer, NULL, 1, 1, 1, 1);
      }
      break;
    case 6:
      if(m_doeff)
      {
        LICE_TexGen_Noise(framebuffer, NULL, 0.9, 0.3, 0.6, 6.0f, NOISE_MODE_WOOD, 2);
      }
      break;
    case 7:
      if(m_doeff)
      {
        LICE_TexGen_Noise(framebuffer, NULL, 1,1,1, 8.0f, NOISE_MODE_NORMAL, 8);
      }
      break;
    case 8:
      if(m_doeff)
      {
        LICE_TexGen_CircNoise(framebuffer, NULL, 0.5f,0.5f,0.5f, 12.0f, 0.1f, 32);
      }
      break;
    case 10:
    {
      int x;
      static double a;
      double sc=sin(a)*0.24;
      a+=0.03;
      for (x = 0; x < 10000; x ++)
        LICE_PutPixel(framebuffer,rand()%framebuffer->getWidth(),rand()%framebuffer->getHeight(),LICE_RGBA(255,255,255,255),sc,LICE_BLIT_MODE_ADD);
    }
      break;
    case 11:
      //line test
    {

      LICE_pixel goodCol=LICE_RGBA(192,0,192,64);
      LICE_Clear(framebuffer,goodCol);
      int subx=30,suby=30,subw=framebuffer->getWidth()-60,subh=framebuffer->getHeight()-60;
      LICE_SubBitmap bm(framebuffer,subx,suby,subw,subh);
      LICE_Clear(&bm,LICE_RGBA(80,80,80,255));

      int n;
      int w = framebuffer->getWidth(), h = framebuffer->getHeight();      
      for(n=0;n<1000;n++)
      {
        LICE_FLine(&bm, rand()%(w*3/2)-w/4, rand()%(h*3/2)-h/4, rand()%(w*3/2)-w/4, rand()%(h*3/2)-h/4, LICE_RGBA(rand()%255,rand()%255,rand()%255,255));
      }
      int y;
      if (0) for(y=0;y<h;y++)
      {
        int x;
        for(x=0;x<w;x++)
        {
          if (x<subx||y<suby||x>=subx+subw||y>=suby+subh)
          {
            if (LICE_GetPixel(framebuffer,x,y)!=goodCol)
            {
              LICE_Clear(framebuffer,LICE_RGBA(255,255,255,255));
              y=h;
              break;
            }
          }
        }
      }

  //    LICE_Line(framebuffer, rand()%(w*3/2)-w/4, rand()%(h*3/2)-h/4, rand()%(w*3/2)-w/4, rand()%(h*3/2)-h/4, LICE_RGBA(rand()%255,rand()%255,rand()%255,255));
    }
      break;
    case 12:
      //lice draw text test
    {
      static double a;
      a+=0.001;
      LICE_DrawText(framebuffer,0.5*(1+sin(a))*(framebuffer->getWidth()-30),0.5*(1+sin(a*7.0+1.3))*(framebuffer->getHeight()-16),"LICE RULEZ",LICE_RGBA(255,0,0,0),sin(a*0.7),LICE_BLIT_MODE_ADD);
    }
      break;
    case 13:
      //icon loading test
    {
      LICE_Clear(framebuffer, LICE_RGBA(255,255,255,255));
      LICE_Blit(framebuffer,icon,0,0,NULL,1.0f,LICE_BLIT_MODE_COPY|LICE_BLIT_USE_ALPHA);
    }
      break;
    case 14:
      // circles/arcs
    {
      int w = framebuffer->getWidth(), h = framebuffer->getHeight();
      const double _PI = acos(-1.0);
      static int m_init, m_x, m_y;
      if (!m_init) {
        m_init = true;
        m_x = w/2; m_y = h/2;
      }
      int r = rand()%w;
      float alpha = 1.0f; //(float) r / (float) w;
      float aLo = 2*_PI*rand()/RAND_MAX;
      float aHi = 2*_PI*rand()/RAND_MAX;
      
      //LICE_Clear(framebuffer, LICE_RGBA(0,0,0,0));
      LICE_Arc(framebuffer, m_x, m_y, r, aLo, aHi, LICE_RGBA(rand()%255,rand()%255,rand()%255,255),alpha);
      //LICE_Circle(framebuffer, m_x, m_y, r, LICE_RGBA(rand()%255,rand()%255,rand()%255,255));
    }
      break;
    case 16:
    {
      int sw=framebuffer->getWidth();
      int sh=framebuffer->getHeight();
      
      LICE_MemBitmap framebuffer_back;
      {
        static double a;
        a+=0.003;
        
        static int turd;
        if ((turd++&511) < 12)
          LICE_GradRect(framebuffer,sw/4,sh/4,sw/2,sh/2,
                        0.5*sin(a*14.0),0.5*cos(a*2.0+1.3),0.5*sin(a*4.0),0.1,
                        (cos(a*37.0))/framebuffer->getWidth()*0.5,(sin(a*17.0))/framebuffer->getWidth()*0.5,(cos(a*7.0))/framebuffer->getWidth()*0.5,0,
                        (sin(a*12.0))/framebuffer->getHeight()*0.5,(cos(a*4.0))/framebuffer->getHeight()*0.5,(cos(a*3.0))/framebuffer->getHeight()*0.5,0,
                        LICE_BLIT_MODE_ADD);
      }
      //LICE_TexGen_Marble(framebuffer, NULL, 1, 1, 1, 1);
      LICE_Copy(&framebuffer_back,framebuffer);
      
      
      const int divw=10;
      const int divh=5;
      float pts[2*divw*divh];
      static float angs[2*divw*divh];
      static float dangs[2*divw*divh];
      static int turd;
      if (!turd)
      {
        turd++;
        int a;
        for (a = 0; a  < 2*divw*divh; a ++)
        {
          dangs[a]=((rand()%1000)-500)*0.0001;
          angs[a]=((rand()%1000)-500)*0.1;
        }
      }
      int x,y;
      for (y=0;y<divh; y++)
      {
        for (x=0;x<divw; x ++)
        {
          int idx=(y*divw+x)*2;
          float ang=angs[idx]+=dangs[idx];
          float ang2=angs[idx+1]+=dangs[idx+1];
          pts[idx]=sw*(float)x/(float)(divw-1) + (cos(ang))*sw*0.01;
          pts[idx+1]=sh*(float)y/(float)(divh-1) + (sin(ang2))*sh*0.01;
        }
      }
      
      
      LICE_TransformBlit(framebuffer,&framebuffer_back,0,0,framebuffer->getWidth(),
                         framebuffer->getHeight(),pts,divw,divh,0.8,LICE_BLIT_MODE_COPY|LICE_BLIT_FILTER_BILINEAR);
    }
      
      break;
    case 19:
      //SVG loading
      {
        static LICE_IBitmap* svgbmp = 0;

        if (!svgbmp) svgbmp = LICE_LoadSVG("c:\\test.svg", 0);
        if (svgbmp) LICE_Blit(framebuffer, svgbmp, 0, 0, 0, 0, svgbmp->getWidth(), svgbmp->getHeight(), 1.0f, LICE_BLIT_MODE_COPY);
      }
      break;
  }
  
  if(jpg)
  {
    LICE_ScaledBlit(framebuffer,jpg,0,0,framebuffer->getWidth(),framebuffer->getHeight(),0,0,jpg->getWidth(),jpg->getHeight(),0.5,LICE_BLIT_MODE_COPY);
  }
  t2 = gettm()-t2;
  
  m_frame_cnt++;
  
  double sec=(GetTickCount()-m_start_time)*0.001;
  //if (sec>0.0001)
  if (g_status[0])
  {
    LICE_DrawText(framebuffer,1,1,g_status,LICE_RGBA(0,0,0,0),1,LICE_BLIT_MODE_COPY);
    LICE_DrawText(framebuffer,0,0,g_status,LICE_RGBA(255,255,255,0),1,LICE_BLIT_MODE_COPY);
  }
  
  m_doeff = 0;
  
  double t1=gettm();
  
  BitBlt(dc,r.left,r.top,framebuffer->getWidth(),framebuffer->getHeight(),framebuffer->getDC(),0,0,SRCCOPY);
  //      bmp->blitToDC(dc, NULL, 0, 0);
  t1 = gettm()-t1;

  static double ac,stt,ac2;

  if (!frame_cnt++)
  {
    ac=ac2=0;
    stt=gettm();
  }
  ac+=t1;
  ac2+=t2;
  sprintf(g_status,"blit = %f/%f, %f/%f %dx%d @ %ffps\n",t1,t2,ac/frame_cnt,ac2/frame_cnt,framebuffer->getWidth(),framebuffer->getHeight(),frame_cnt/(gettm()-stt));

#if 0
  if (GetAsyncKeyState(VK_SHIFT)&0x8000)
  if (GetAsyncKeyState(VK_MENU)&0x8000)
  if (GetAsyncKeyState(VK_CONTROL)&0x8000)
  {
    LICE_WritePNG("/tmp/blah.png",framebuffer,false);
    LICE_WriteJPG("/tmp/blah.jpg",framebuffer);
  }
#endif
}


// this is only used on OS X since it's way faster there
LRESULT WINAPI testRenderDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  if (uMsg==WM_PAINT)
  {
    PAINTSTRUCT ps;
    
    HDC dc = BeginPaint(hwndDlg, &ps);
    DoPaint(hwndDlg,dc);
    EndPaint(hwndDlg, &ps);
    return 0;
  }
  if (uMsg == WM_LBUTTONDOWN)
  {
    m_cap=true;
    SetCapture(hwndDlg);
    ShowCursor(FALSE);
  }
  else if (uMsg == WM_LBUTTONUP||uMsg==WM_CAPTURECHANGED)
  {
    m_cap=false;
    ShowCursor(TRUE);
    if (uMsg==WM_LBUTTONUP)ReleaseCapture();
  }
    
   return DefWindowProc(hwndDlg,uMsg,wParam,lParam);
}

WDL_DLGRET WINAPI dlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  if (uMsg == WM_LBUTTONDOWN)
  {
    m_cap=true;
    SetCapture(hwndDlg);
    ShowCursor(FALSE);
  }
  else if (uMsg == WM_LBUTTONUP||uMsg==WM_CAPTURECHANGED)
  {
    m_cap=false;
    ShowCursor(TRUE);
    if (uMsg==WM_LBUTTONUP)ReleaseCapture();
  }

  switch(uMsg)
  {
  case WM_INITDIALOG:
  
    framebuffer = new LICE_SysBitmap(0,0);
    
    //jpg=LICE_LoadJPG("C:/turds.jpg");

#ifdef _WIN32
    bmp = LICE_LoadPNGFromResource(g_hInstance, MAKEINTRESOURCE(IDC_PNG1));
    icon = LICE_LoadIconFromResource(g_hInstance, MAKEINTRESOURCE(IDI_MAIN), 0);
#else
    bmp = LICE_LoadPNGFromNamedResource("image.png");

      
    // uncomment if you want to try GL blits:
    //   SWELL_SetViewGL(GetDlgItem(hwndDlg,IDC_RECT),true);
    SendMessage(hwndDlg,WM_SIZE,0,0);
#endif     
    
    SetTimer(hwndDlg,1,1,NULL);
    {
      int x;
      for (x = 0; x < NUM_EFFECTS; x ++)
      {
        char buf[512];
        wsprintf(buf,"Effect %d - %s",x+1,effect_names[x]);
        SendDlgItemMessage(hwndDlg,IDC_COMBO1,CB_ADDSTRING,0,(LPARAM)buf);
      }
      SendDlgItemMessage(hwndDlg,IDC_COMBO1,CB_SETCURSEL,m_effect,0);

      m_start_time=GetTickCount();
      m_frame_cnt=0;
    }
  return 0;
  case WM_DESTROY:
      
      
    delete icon;
    delete bmp;
    delete framebuffer;    
  return 0;
    
#ifdef _WIN32
  case WM_TIMER:
    InvalidateRect(hwndDlg,NULL,FALSE);
    return 0;
  case WM_PAINT:
    {
      PAINTSTRUCT ps;
      
      HDC dc = BeginPaint(hwndDlg, &ps);

      DoPaint(hwndDlg,dc);
      EndPaint(hwndDlg,&ps);
    }
    break;
#else
  case WM_SIZE:
  {
    RECT r;
    GetClientRect(hwndDlg,&r);
    r.top+=40;
    SetWindowPos(GetDlgItem(hwndDlg,IDC_RECT),NULL,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_NOZORDER|SWP_NOACTIVATE);
  }
  return 0;
  case WM_TIMER:
#if 1
    InvalidateRect(GetDlgItem(hwndDlg,IDC_RECT),NULL,FALSE);
#else
    {
      HWND h = GetDlgItem(hwndDlg,IDC_RECT);
      HDC dc = GetWindowDC(h);
      DoPaint(hwndDlg,dc);
      ReleaseDC(h,dc);
      SWELL_FlushWindow(h);
    }
#endif
  return 0;
#endif
  case WM_COMMAND:
    switch(LOWORD(wParam))
    {
      case IDC_COMBO1:
        m_effect = SendDlgItemMessage(hwndDlg,IDC_COMBO1,CB_GETCURSEL,0,0);
        m_doeff=1;
        m_start_time=GetTickCount();
        m_frame_cnt=0;
      break;
      case IDCANCEL:
#ifndef __APPLE__
        EndDialog(hwndDlg, 0);
#else
        DestroyWindow(hwndDlg); // on mac we run modeless
#endif
      break;
    }
    break;
  }
  return 0;
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdParam, int nShowCmd)
{

  timingInit();
  g_hInstance=hInstance;
  DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, dlgProc);

  timingPrint();

  return 0;
}
#else

static HWND ccontrolCreator(HWND parent, const char *cname, int idx, const char *classname, int style, int x, int y, int w, int h)
{
  if (!stricmp(classname,"TestRenderingClass"))
  {
    HWND hw=CreateDialog(NULL,0,parent,(DLGPROC)testRenderDialogProc);
    SetWindowLong(hw,GWL_ID,idx);
    SetWindowPos(hw,HWND_TOP,x,y,w,h,SWP_NOZORDER|SWP_NOACTIVATE);
    ShowWindow(hw,SW_SHOWNA);
    return hw;
  }
  return 0;
}

#include "../../swell/swell-dlggen.h"

// define our dialog box resource!

SWELL_DEFINE_DIALOG_RESOURCE_BEGIN(IDD_DIALOG1,SWELL_DLG_WS_RESIZABLE|SWELL_DLG_WS_FLIPPED,"LICE Test",400,300,1.8)
BEGIN
CONTROL         "",IDC_RECT,"TestRenderingClass",0,7,23,384,239 // we arae creating a custom control here because it will be opaque and therefor a LOT faster drawing
COMBOBOX        IDC_COMBO1,7,7,181,170,CBS_DROPDOWNLIST | WS_VSCROLL | 
WS_TABSTOP

END
SWELL_DEFINE_DIALOG_RESOURCE_END(IDD_DIALOG1)

#if !defined(__APPLE__)
int main(int argc, char **argv)
{
  SWELL_initargs(&argc,&argv);
  SWELL_Internal_PostMessage_Init();
  SWELL_ExtendedAPI("APPNAME",(void*)"LICE test");
  SWELL_RegisterCustomControlCreator(ccontrolCreator);
  //SWELL_ExtendedAPI("INIFILE",(void*)"path/to/ini/file.ini");
  //SWELL_ExtendedAPI("FONTPANGRAM",(void*)"LICE test thingy lbah akbzfshauoh01384u1023");
  DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, dlgProc);

  return 0;
}

INT_PTR SWELLAppMain(int msg, INT_PTR parm1, INT_PTR parm2)
{
  return 0;
}
#endif
#endif
