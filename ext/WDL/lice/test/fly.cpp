#include "../lice.h"
#include "../../plush2/plush.h"

#ifndef _WIN32
#include "../../swell/swell.h"
#endif


/* Physical size of land */
#define LAND_SIZE 65000
/* Number of divisions of the land. Higher number == more polygons */
#define LAND_DIV 32

 // These are the amount the mouse has moved since the last frame
 // mouse_b = button status, mouse_avail is whether or not mouse is
 // available, mouse_buttons is number of buttons
static float mouse_sens = 8192.0/32768.0;

static  int draw_sky = 1;                 // do we draw the sky?
static  int wait_vsync = 0;               // do we wait for vsync?
static pl_Mat *mat[3+1];                 // our materials, we have 1 extra for null
                                    // termination for plMatMakeOptPal2()
static pl_Cam *cam;                      // our camera
static pl_Obj *land;                     // the land object
static pl_Obj *sky, *sky2;               // the two skies
static pl_Light lights[16];

static void setup_materials(pl_Mat **mat);
 // Sets up the landscape and skies
static pl_Obj *setup_landscape(pl_Mat *m, pl_Mat *sm, pl_Mat *sm2);

void doFlyEffect(LICE_IBitmap *fb, HWND hwnd)
{
  static int initted;
  if (!initted)
  {
    initted=1;
    cam = new pl_Cam;
    cam->Fov=90.0;
    cam->WantZBuffer=true;
    if (cam->WantZBuffer) cam->Sort = -1;
    cam->Y = 800; // move the camera up from the ground
    cam->Pitch = 180.0;

    setup_materials(mat); // intialize materials and palette

    land = setup_landscape(mat[0],mat[1],mat[2]); // create landscape
    sky = land->Children.Get(0); // unhierarchicalize the sky from the land
    land->Children.Delete(0);
    sky2 = land->Children.Get(0);
    land->Children.Delete(0);

    int x;
    for(x=0;x<sizeof(lights)/sizeof(lights[0]);x++)
    {
      lights[x].Set(PL_LIGHT_POINT,(x%4 - 1.5) * LAND_SIZE /4.0,
                                   500+(rand()%1000),
                                   (x/4-1.5)*LAND_SIZE/4.0,(rand()%1000)/700.0,(rand()%1000)/700.0,(rand()%1000)/700.0,LAND_SIZE*1.0);
    }

  }

  LICE_Clear(fb,0);
      cam->Begin(fb);

      int x;
      for(x=0;x<sizeof(lights)/sizeof(lights[0]);x++)
        cam->RenderLight(&lights[x]);

  // lots of rendering special casing
  if (draw_sky) { // if we're drawing the sky
    if (cam->Y > 2000) { // if above the sky, only render the skies, with 
                         // no far clip plane

      cam->RenderObject(sky);
      cam->RenderObject(sky2);
    } else {           // otherwise, render the sky (but not the second sky),
                       // and the land, with a far clip plane


      cam->RenderObject(sky);
      cam->RenderObject(land);
    }
  } else { // not drawing sky, just render the land

    cam->RenderObject(land);
  }
  cam->End(); // finish rendering

  static POINT lpos;
  POINT p;
  GetCursorPos(&p);
  int mouse_x  = 0;
  int mouse_y  = 0;
  int mouse_b=0;

  if (hwnd)
  {
    mouse_x = p.x-lpos.x;
    mouse_y = p.y-lpos.y;
    if (GetAsyncKeyState(VK_LBUTTON)&0x8000) mouse_b|=2;

    RECT r;
    GetWindowRect(hwnd,&r);
    p.x=(r.right+r.left)/2;
    p.y=(r.bottom+r.top)/2;
    SetCursorPos(p.x,p.y);
  }
  lpos=p;
    // We calculate the amount of time in thousanths of seconds this frame took
    double prevtime = 10; //((uclock() / (float) UCLOCKS_PER_SEC) - prevtime)*1000.0;

    if (mouse_b & 2) { // if right button hit, we go forward quickly
      cam->X -=
        prevtime*4*sin(cam->Pan*PL_PI/180.0)*cos(cam->Pitch*PL_PI/180.0);
      cam->Z += 
        prevtime*4*cos(cam->Pan*PL_PI/180.0)*cos(cam->Pitch*PL_PI/180.0);
      cam->Y += 
        prevtime*4*sin(cam->Pitch*PL_PI/180.0);
    } else if (mouse_b & 1) { // if left button hit, we go forward slowly
      cam->X -= 
        prevtime*2*sin(cam->Pan*PL_PI/180.0)*cos(cam->Pitch*PL_PI/180.0);
      cam->Z += 
        prevtime*2*cos(cam->Pan*PL_PI/180.0)*cos(cam->Pitch*PL_PI/180.0);
      cam->Y += 
        prevtime*2*sin(cam->Pitch*PL_PI/180.0);
    }
    cam->Pitch += (mouse_y*mouse_sens); // update pitch and pan of ship
    cam->Pan += (mouse_x*mouse_sens)*(-cos(cam->Pitch*PL_PI/180.0));
    
    if (cam->X > LAND_SIZE/2) cam->X = LAND_SIZE/2; // make sure we don't go 
    if (cam->X < -LAND_SIZE/2) cam->X = -LAND_SIZE/2; // too far away
    if (cam->Z > LAND_SIZE/2) cam->Z = LAND_SIZE/2;
    if (cam->Z < -LAND_SIZE/2) cam->Z = -LAND_SIZE/2;
    if (cam->Y < 0) cam->Y = 0;
    if (cam->Y > 8999) cam->Y = 8999;
#if 0 

    while (kbhit()) switch(getch()) { // handle keystrokes
      case 27: done++; break;    // ESC == quit
        // + is for zooming in.
      case '=': case '+': cam->Fov -= 1.0; if (cam->Fov < 1.0) cam->Fov = 1.0;
        sprintf(lastmessage,"FOV: %2.f",cam->Fov);
      break;
        // - is for zooming out
      case '-': cam->Fov += 1.0; if (cam->Fov > 179.0) cam->Fov = 179.0;
        sprintf(lastmessage,"FOV: %2.f",cam->Fov);
      break;
        // [ decreases mouse sensitivity
      case '[': mouse_sens /= 1.1; 
        sprintf(lastmessage,"MouseSens: %.3f",mouse_sens);
      break;
        // ] increases mouse sensitivity
      case ']': mouse_sens *= 1.1; 
        sprintf(lastmessage,"MouseSens: %.3f",mouse_sens);
      break;
        // v toggles vsync
      case 'v': wait_vsync ^= 1;
        sprintf(lastmessage,"VSync %s",wait_vsync ? "on" : "off");
      break;
        // s toggles sky
      case 's': draw_sky ^= 1;
        sprintf(lastmessage,"Sky %s",draw_sky ? "on" : "off");
      break;
    } 
#endif

    //LICE_ScaledBlit(fb,mat[2]->Texture,0,0,fb->getWidth(),fb->getHeight(),0,0,mat[2]->Texture->getWidth(),mat[2]->Texture->getHeight(),1.0f,0);
}




static void setup_materials(pl_Mat **mat) {
  // create our 3 materials, make the fourth null so that plMatMakeOptPal2() 
  // knows where to stop
  mat[0] = new pl_Mat;
  mat[1] = new pl_Mat;
  mat[2] = new pl_Mat;
  mat[3] = 0;

  extern LICE_IBitmap *bmp;
  // set up material 0 (the ground)
  mat[0]->Smoothing = true;
  mat[0]->Lightable=true;
  mat[0]->Ambient[0] = 0.0; // these calculations are to get the
  mat[0]->Ambient[1] = 0.0; // distance shading to work right
  mat[0]->Ambient[2] = 0.0; 
  mat[0]->Diffuse[0] = 1.0;
  mat[0]->Diffuse[1] = 1.0;
  mat[0]->Diffuse[2] = 1.0;
  mat[0]->FadeDist = 10000.0;
  mat[0]->Texture2 = LICE_LoadPCX("c:\\ground.pcx");
  if (!mat[0]->Texture2)
  {
    mat[0]->Texture2  = new LICE_MemBitmap(400,400);
//    LICE_TexGen_Marble(mat[0]->Texture2,NULL,1.0,0.8,0.8,1.0f);
    LICE_TexGen_Noise(mat[0]->Texture2,NULL,1.0,1.0,1.0,1.0f,NOISE_MODE_WOOD,8);
  }

  mat[0]->Texture=mat[0]->Texture2;
  mat[0]->Texture2=bmp;
  mat[0]->TexCombineMode=LICE_BLIT_MODE_MUL|LICE_BLIT_FILTER_BILINEAR;
  mat[0]->Tex2CombineMode=LICE_BLIT_MODE_DODGE|LICE_BLIT_USE_ALPHA|LICE_BLIT_FILTER_BILINEAR;
  mat[0]->Tex2Scaling[1]=
  mat[0]->Tex2Scaling[0] = 40.0*LAND_SIZE/50000;
  mat[0]->TexScaling[1]=
  mat[0]->TexScaling[0] = 40.0*LAND_SIZE/50000;
  mat[0]->Tex2MapIdx=0;
  mat[0]->PerspectiveCorrect = 16;
  mat[0]->BackfaceIllumination=1.0;

  // set up material 1 (the sky)
  mat[1]->Lightable=true;
  mat[1]->Ambient[0] = 0.4; // these calculations are to get the
  mat[1]->Ambient[1] = 0.4; // distance shading to work right
  mat[1]->Ambient[2] = 0.4; 
  mat[1]->Diffuse[0] = 1.0;
  mat[1]->Diffuse[1] = 1.0;
  mat[1]->Diffuse[2] = 1.0;
  mat[1]->FadeDist = 10000.0;
  mat[1]->Texture = LICE_LoadPCX("c:\\sky.pcx");
  mat[1]->TexCombineMode=LICE_BLIT_MODE_MUL;
  mat[1]->TexScaling[1] = mat[1]->TexScaling[0] = 45.0*LAND_SIZE/50000;
  mat[1]->PerspectiveCorrect = 32;
  mat[1]->BackfaceCull=false;
  mat[1]->BackfaceIllumination=1.0;

  if (!mat[1]->Texture)
  {
    mat[1]->Texture = new LICE_MemBitmap(400,400);
    LICE_TexGen_Noise(mat[1]->Texture,NULL,0.0,0.0,1.0,1.0f,NOISE_MODE_WOOD,8);
  }
  mat[1]->Texture2=bmp;
  mat[1]->Tex2CombineMode= LICE_BLIT_MODE_HSVADJ|LICE_BLIT_FILTER_BILINEAR|LICE_BLIT_USE_ALPHA;
  mat[1]->Tex2Scaling[1] = mat[1]->Tex2Scaling[0] = 45.0*LAND_SIZE/50000*0.37;
  mat[1]->Tex2MapIdx=0;

  // set up material 2 (the second sky)
  mat[2]->Lightable=true;
  mat[2]->Smoothing=false;
  mat[2]->Ambient[0] = 0.2; // these calculations are to get the
  mat[2]->Ambient[1] = 0.2; // distance shading to work right
  mat[2]->Ambient[2] = 0.2; 
  mat[2]->Diffuse[0] = 0.0;
  mat[2]->Diffuse[1] = 0.0;
  mat[2]->Diffuse[2] = 0.0;
  mat[2]->Texture = LICE_LoadPCX("c:\\sky2.pcx");
  if (!mat[2]->Texture)
  {
    mat[2]->Texture = new LICE_MemBitmap(400,400);
    LICE_TexGen_Marble(mat[2]->Texture,NULL,0.3,0,0.4,0.4);
  }
  mat[2]->TexScaling[0] = mat[2]->TexScaling[1] = 10.0; //200.0*LAND_SIZE/50000;
  mat[2]->TexCombineMode=LICE_BLIT_MODE_COPY;
  mat[2]->PerspectiveCorrect = 16;
    
  mat[2]->Texture2=bmp;
  mat[2]->Tex2CombineMode= LICE_BLIT_MODE_HSVADJ|LICE_BLIT_FILTER_BILINEAR|LICE_BLIT_USE_ALPHA;
  mat[2]->Tex2Scaling[1] = mat[1]->Tex2Scaling[0] = 45.0*LAND_SIZE/50000*3.37;
  mat[2]->Tex2MapIdx=0;

}

pl_Obj *setup_landscape(pl_Mat *m, pl_Mat *sm, pl_Mat *sm2) {
  int i;
  // make our root object the land
  pl_Obj *o = plMakePlane(LAND_SIZE,LAND_SIZE,LAND_DIV-1,m); 
  // give it a nice random bumpy effect
  for (i = 0; i < o->Vertices.GetSize(); i ++)
    o->Vertices.Get()[i].y += (float) (rand()%1400)-700;
  // gotta recalculate normals for backface culling to work right
  o->CalculateNormals();

  // Make our first child the first sky
  o->Children.Add(plMakePlane(LAND_SIZE,LAND_SIZE,30,sm));
  o->Children.Get(0)->Yp = 2000;

  // and the second the second sky
  o->Children.Add(plMakeSphere(LAND_SIZE,10,10,sm2));
  o->Children.Get(1)->Yp = 2000;
  o->Children.Get(1)->FlipNormals();

  return (o);
}

