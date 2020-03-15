/******************************************************************************
  plush.h
  PLUSH 3D VERSION 2.0  MAIN HEADER
  Copyright (c) 1996-2000 Justin Frankel
  Copyright (c) 1998-2000 Nullsoft, Inc.
  Copyright (c) 2008 Cockos Incorporated

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


******************************************************************************/

#ifndef _PLUSH_H_
#define _PLUSH_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../lice/lice.h" // using LICE for images
#include "../ptrlist.h"
#include "../wdltypes.h"

typedef float pl_ZBuffer;              /* z-buffer type (must be float) */
typedef double pl_Float;               /* General floating point */
typedef float pl_IEEEFloat32;          /* IEEE 32 bit floating point */ 
typedef signed int pl_sInt32;          /* signed 32 bit integer */
typedef unsigned int pl_uInt32;        /* unsigned 32 bit integer */
typedef signed short int pl_sInt16;    /* signed 16 bit integer */
typedef unsigned short int pl_uInt16;  /* unsigned 16 bit integer */
typedef signed int pl_sInt;            /* signed optimal integer */
typedef unsigned int pl_uInt;          /* unsigned optimal integer */
typedef bool pl_Bool;                  /* boolean */
typedef unsigned char pl_uChar;        /* unsigned 8 bit integer */
typedef signed char pl_sChar;          /* signed 8 bit integer */



/* pi! */
#define PL_PI 3.141592653589793238

/* Utility min() and max() functions */
#define plMin(x,y) (( ( x ) > ( y ) ? ( y ) : ( x )))
#define plMax(x,y) (( ( x ) < ( y ) ? ( y ) : ( x )))



/*
** Light modes. Used with plLight.Type or plLightSet().
** Note that PL_LIGHT_POINT_ANGLE assumes no falloff and uses the angle between
** the light and the point, PL_LIGHT_POINT_DISTANCE has falloff with proportion
** to distance**2 (see plLightSet() for setting it), PL_LIGHT_POINT does both.
*/
#define PL_LIGHT_NONE (0x0)
#define PL_LIGHT_VECTOR (0x1)
#define PL_LIGHT_POINT (0x2|0x4)
#define PL_LIGHT_POINT_DISTANCE (0x2)
#define PL_LIGHT_POINT_ANGLE (0x4)


#define PLUSH_MAX_MAPCOORDS 3 // 2 + envmap slot


class pl_Mat
{
public:
  pl_Mat()
  {
    memset(Ambient,0,sizeof(Ambient));
    Diffuse[0]=Diffuse[1]=Diffuse[2]=1.0;
    SolidOpacity=1.0;
    SolidCombineMode= LICE_BLIT_MODE_COPY;
    
    Texture=NULL;
    TexCombineMode = LICE_BLIT_MODE_ADD; //LICE_BLIT_USE_SOURCE_ALPHA?
    TexOpacity=1.0;
    TexScaling[0]=TexScaling[1]=1.0;
    TexMapIdx=0;

    Texture2=NULL;
    Tex2CombineMode = LICE_BLIT_MODE_ADD;
    Tex2Opacity=1.0;
    Tex2Scaling[0]=Tex2Scaling[1]=1.0;
    Tex2MapIdx=-1;

    FadeDist=0.0;
    Smoothing=Lightable=true;
    zBufferable=true;
    PerspectiveCorrect=16;
    BackfaceCull=true;
    BackfaceIllumination=0.0;

    cachedTexture=cachedTexture2=0;
    cachesInvalid=true;
  }

  ~pl_Mat()
  {
    delete cachedTexture;
    delete cachedTexture2;
  }


  pl_Bool Smoothing; // smoothing of lighting
  pl_Bool Lightable; // affected by lights
  pl_Bool zBufferable;         /* Can this material be zbuffered? */
  pl_uChar PerspectiveCorrect; /* Correct texture perspective every n pixels */

  pl_Float FadeDist WDL_FIXALIGN;            /* For distance fading, distance at which intensity is 0. set to 0.0 for no distance shading */

  pl_Bool BackfaceCull;               /* Are backfacing polys drawn? */
  pl_Float BackfaceIllumination WDL_FIXALIGN;       /* Illuminated by lights behind them, and by how much of a factor? */ 

  // colors
  pl_Float Ambient[3];          /* RGB of surface (0-1 is a good range) */
  pl_Float Diffuse[3];          /* RGB of diffuse (0-1 is a good range) */
  pl_Float SolidOpacity;
  int SolidCombineMode;           /* LICE combine mode for first pass (color), default should be replace (or add-black for transparent) */

  // textures
  LICE_IBitmap *Texture;         /* Texture map (not owned by Material but a reference)*/
  pl_Float TexScaling[2] WDL_FIXALIGN;         /* Texture map scaling */
  pl_Float TexOpacity;
  int TexCombineMode;           /* Texture combine mode (generally should be additive) */
  int TexMapIdx; // -1 for env

  LICE_IBitmap *Texture2;     
  pl_Float Tex2Scaling[2] WDL_FIXALIGN;         
  pl_Float Tex2Opacity;
  int Tex2CombineMode;             
  int Tex2MapIdx; // -1 for env


  void InvalidateTextureCaches() { cachesInvalid=true; } // call this if you change Texture or Texture2 after rendering

private:
  bool cachesInvalid;
  LICE_IBitmap *cachedTexture,*cachedTexture2; // these may need to be LICE_GL_MemBitmaps etc

} WDL_FIXALIGN;


class pl_Vertex {
public:
  pl_Vertex() { }
  ~pl_Vertex () { }

  pl_Float x, y, z;              /* Vertex coordinate (objectspace) */
  pl_Float nx, ny, nz;           /* Unit vertex normal (objectspace) */
  
  pl_Float xformedx, xformedy, xformedz;   /* Transformed vertex  coordinate (cameraspace) */
  pl_Float xformednx, xformedny, xformednz;  /* Transformed unit vertex normal  (cameraspace) */
};

class pl_Face {
public:
  pl_Face()
  {    
  }
  ~pl_Face()
  {
  }

  pl_Mat *Material;            /* Material of triangle */
  int VertexIndices[3];      /* Vertices of triangle */

  pl_Float nx WDL_FIXALIGN;
  pl_Float ny;
  pl_Float nz;         /* Normal of triangle (object space) */

  pl_Float MappingU[PLUSH_MAX_MAPCOORDS][3], MappingV[PLUSH_MAX_MAPCOORDS][3];  /* Texture mapping coordinates */ 

  pl_Float sLighting[3];          /* Face static lighting. Should usually be 0.0 */
  pl_Float vsLighting[3][3];      /* Vertex static lighting. Should usually be 0.0 */


  // calculated:
  pl_Float Shades[3][3];          /* colors (first 3 used for flat, all for Gouraud) */
  pl_Float Scrx[3], Scry[3];  /* Projected screen coordinates */
  pl_Float Scrz[3];            /* Projected 1/Z coordinates */

};


class pl_Obj {
public:
  pl_Obj(int nv=0, int nf=0) 
  {
    if (nv) memset(Vertices.Resize(nv),0,nv*sizeof(pl_Vertex));
    if (nf) memset(Faces.Resize(nf),0,nf*sizeof(pl_Face));

    GenMatrix=true;
    Xp=Yp=Zp=Xa=Ya=Za=0.0;
  }
  ~pl_Obj() { Children.Empty(true); }

  pl_Obj *Clone();
  void Scale(pl_Float sc);
  void Stretch(pl_Float x, pl_Float y, pl_Float z); // scales but preserves normals
  void Translate(pl_Float x, pl_Float y, pl_Float z);
  void FlipNormals();

  void SetMaterial(pl_Mat *m, pl_Bool recurse=true);
  void CalculateNormals();


  WDL_TypedBuf<pl_Vertex> Vertices;
  WDL_TypedBuf<pl_Face> Faces;
  WDL_PtrList<pl_Obj> Children;
                                      /* Children */
  pl_Bool GenMatrix;                  /* Generate Matrix from the following
                                         if set */
  pl_Float Xp WDL_FIXALIGN;
  pl_Float Yp, Zp, Xa, Ya, Za;    /* Position and rotation of object:
                                         Note: rotations are around 
                                         X then Y then Z. Measured in degrees */
  pl_Float Matrix[16];                /* Transformation matrix */
  pl_Float RotMatrix[16];             /* Rotation only matrix (for normals) */
};


class pl_Spline {
public:
  pl_Spline() { cont=1.0; bias=0.3; tens=0.3; keyWidth=1; }
  ~pl_Spline () { }
  void GetPoint(pl_Float frame, pl_Float *out);
  WDL_TypedBuf<pl_Float> keys;              /* Key data, keyWidth*numKeys */
  pl_sInt keyWidth;            /* Number of floats per key */

  pl_Float cont WDL_FIXALIGN;               /* Continuity. Should be -1.0 -> 1.0 */
  pl_Float bias;               /* Bias. -1.0 -> 1.0 */
  pl_Float tens;               /* Tension. -1.0 -> 1.0 */
};


class pl_Light {
public:
  pl_Light() { Type = PL_LIGHT_VECTOR; Xp=Yp=0.0; Zp=1.0; Intensity[0]=Intensity[1]=Intensity[2]=1.0; }
  ~pl_Light() { }

/*
  Set() sets up a light:
    mode: the mode of the light (PL_LIGHT_*)
    x,y,z: either the position of the light (PL_LIGHT_POINT*) or the angle
           in degrees of the light (PL_LIGHT_VECTOR)
    intensity: the intensity of the light (0.0-1.0)
    halfDist: the distance at which PL_LIGHT_POINT_DISTANCE is 1/2 intensity
*/
  void Set(pl_uChar mode, pl_Float x, pl_Float y, pl_Float z, pl_Float intensity_r, pl_Float intensity_g, pl_Float intensity_b, pl_Float halfDist);


  // privatestuff
  pl_uChar Type;               /* Type of light: PL_LIGHT_* */
  pl_Float Xp WDL_FIXALIGN;
  pl_Float Yp, Zp;         /* If Type=PL_LIGHT_POINT*,
                                  this is Position (PL_LIGHT_POINT_*),
                                  otherwise if PL_LIGHT_VECTOR,
                                  Unit vector */
  pl_Float Intensity[3];           /* Intensity. 0.0 is off, 1.0 is full */
  pl_Float HalfDistSquared;     /* Distance squared at which 
                                   PL_LIGHT_POINT_DISTANCE is 50% */
};


class pl_Cam {
public:
  pl_Cam() 
  {
    frameBuffer=0;
    Fov=90.0;
    AspectRatio=1.0;
    Sort=1;
    ClipBack=-1.0;
    CenterX=CenterY=0;
    X=Y=Z=0.0;
    WantZBuffer=false;
    Pitch=Pan=Roll=0.0;
  }
  ~pl_Cam()
  {
  }


  void SetTarget(pl_Float x, pl_Float y, pl_Float z); 


  pl_Float Fov;                  /* FOV in degrees valid range is 1-179 */
  pl_Float AspectRatio;          /* Aspect ratio (usually 1.0) */
  pl_sChar Sort;                 /* Sort polygons, -1 f-t-b, 1 b-t-f, 0 no */
  pl_Float ClipBack WDL_FIXALIGN;             /* Far clipping ( < 0.0 is none) */
  pl_sInt CenterX, CenterY;      /* Offset center of screen from actual center by this much... */
  pl_Float X WDL_FIXALIGN;
  pl_Float Y, Z;              /* Camera position in worldspace */

  pl_Float Pitch, Pan, Roll;     /* Camera angle in degrees in worldspace */
  
  bool WantZBuffer;

  void Begin(LICE_IBitmap *fb, bool want_zbclear=true, pl_ZBuffer zbclear=0.0);
  void RenderLight(pl_Light *light);
  void RenderObject(pl_Obj *obj, pl_Float *bmatrix=NULL, pl_Float *bnmatrix=NULL);
  void SortToCurrent(); // sorts all faces added since Begin() or last SortToCurrent() call. useful for if you use zbuffering with transparent objects (draw them last)

  LICE_IBitmap *GetFrameBuffer() { return frameBuffer; }
  WDL_TypedBuf<pl_ZBuffer> zBuffer;           /* Z Buffer (validate size before using)*/

  void End();

  int RenderTrisIn;
  int RenderTrisCulled;
  int RenderTrisOut;

  double RenderPixelsOut WDL_FIXALIGN;

  void PutFace(pl_Face *TriFace);

private:
  LICE_IBitmap *frameBuffer;         /* Framebuffer  - note this is owned by the camera if you set it */

    // internal use
  void ClipRenderFace(pl_Face *face, pl_Obj *obj);
  int ClipNeeded(pl_Face *face, pl_Obj *obj); // 0=no draw, 1=drawing (possibly splitting) necessary
  void RecalcFrustum(); 


  #define PL_NUM_CLIP_PLANES 5

  struct _clipInfo
  {
    pl_Vertex newVertices[8];
    pl_Float ShadeInfos[8][3];
    pl_Float MappingU[PLUSH_MAX_MAPCOORDS][8];
    pl_Float MappingV[PLUSH_MAX_MAPCOORDS][8];
  };

  _clipInfo m_cl[2] WDL_FIXALIGN;
  pl_Float  m_clipPlanes[PL_NUM_CLIP_PLANES][4];
  pl_Float  m_fovfactor, m_adj_asp; // recalculated

   /* Returns: 0 if nothing gets in,  1 or 2 if pout1 & pout2 get in */
  pl_uInt _ClipToPlane(pl_uInt numVerts, pl_Float  *plane);


  struct _faceInfo {
    pl_Float zd;
    pl_Face *face;
    pl_Obj *obj;
  } WDL_FIXALIGN;

  struct _lightInfo {
    pl_Float l[3];
    pl_Light *light;
  } WDL_FIXALIGN;

  static int sortRevFunc(const void *a, const void *b);
  static int sortFwdFunc(const void *a, const void *b);

  int _numfaces,_numfaces_sorted;
  WDL_TypedBuf<_faceInfo> _faces;

  pl_Float _cMatrix[16] WDL_FIXALIGN;

  int _numlights;
  WDL_TypedBuf<_lightInfo> _lights;

  void _RenderObj(pl_Obj *, pl_Float *, pl_Float *);

  WDL_HeapBuf _sort_tmpspace;

};




/******************************************************************************
** Object Primitives Code (pl_make.cpp)
******************************************************************************/

/* 
  plMakePlane() makes a plane centered at the origin facing up the y axis.
  Parameters:
    w: width of the plane (along the x axis)
    d: depth of the plane (along the z axis)
    res: resolution of plane, i.e. subdivisions
    m: material to use
  Returns:
    pointer to object created.
*/
pl_Obj *plMakePlane(pl_Float w, pl_Float d, pl_uInt res, pl_Mat *m);

/*
  plMakeBox() makes a box centered at the origin
  Parameters:
    w: width of the box (x axis)
    d: depth of the box (z axis)
    h: height of the box (y axis)
  Returns:
    pointer to object created.
*/
pl_Obj *plMakeBox(pl_Float w, pl_Float d, pl_Float h, pl_Mat *m);

/* 
  plMakeCone() makes a cone centered at the origin
  Parameters:
    r: radius of the cone (x-z axis)
    h: height of the cone (y axis)
    div: division of cone (>=3)
    cap: close the big end?
    m: material to use
  Returns:
    pointer to object created.
*/
pl_Obj *plMakeCone(pl_Float r, pl_Float h, pl_uInt div, pl_Bool cap, pl_Mat *m);

/*
  plMakeCylinder() makes a cylinder centered at the origin
  Parameters:
    r: radius of the cylinder (x-z axis)
    h: height of the cylinder (y axis)
    divr: division of of cylinder (around the circle) (>=3)
    captop: close the top
    capbottom: close the bottom
    m: material to use
  Returns:
    pointer to object created.
*/
pl_Obj *plMakeCylinder(pl_Float r, pl_Float h, pl_uInt divr, pl_Bool captop, 
                       pl_Bool capbottom, pl_Mat *m);

/*
  plMakeSphere() makes a sphere centered at the origin.
  Parameters:
    r: radius of the sphere
    divr: division of the sphere (around the y axis) (>=3)
    divh: division of the sphere (around the x,z axis) (>=3)
    m: material to use
  Returns:
    pointer to object created.
*/
pl_Obj *plMakeSphere(pl_Float r, pl_uInt divr, pl_uInt divh, pl_Mat *m);

/*
  plMakeTorus() makes a torus centered at the origin
  Parameters:
    r1: inner radius of the torus
    r2: outer radius of the torus
    divrot: division of the torus (around the y axis) (>=3)
    divrad: division of the radius of the torus (x>=3)
    m: material to use
  Returns:
    pointer to object created.
*/
pl_Obj *plMakeTorus(pl_Float r1, pl_Float r2, pl_uInt divrot, 
                    pl_uInt divrad, pl_Mat *m);


/******************************************************************************
** File Readers (pl_read_*.cpp)
******************************************************************************/

/* 
  plRead3DSObj() reads a 3DS object
  Parameters:
    fn: filename of object to read
    m: material to assign it
  Returns:
    pointer to object
  Notes:
    This reader organizes multiple objects like so:
      1) the first object is returned
      2) the second object is the first's first child
      3) the third object is the second's first child
      4) etc
*/
pl_Obj *plRead3DSObj(void *ptr, int size, pl_Mat *m);
pl_Obj *plRead3DSObjFromFile(char *fn, pl_Mat *m);
pl_Obj *plRead3DSObjFromResource(HINSTANCE hInst, int resid, pl_Mat *m);

/*
  plReadCOBObj() reads an ascii .COB object
  Parameters:
    fn: filename of object to read
    mat: material to assign it
  Returns:
    pointer to object
  Notes:
    This is Caligari's ASCII object format.
    This reader doesn't handle multiple objects. It just reads the first one.
    Polygons with lots of sides are not always tesselated correctly. Just
      use the "Tesselate" button from within truespace to improve the results.
*/
pl_Obj *plReadCOBObj(char *fn, pl_Mat *mat);

/*
  plReadJAWObj() reads a .JAW object.
  Parameters:
    fn: filename of object to read
    m: material to assign it
  Returns:
    pointer to object
  Notes:
    For information on the .JAW format, please see the jaw3D homepage,
      http://www.tc.umn.edu/nlhome/g346/kari0022/jaw3d/
*/
pl_Obj *plReadJAWObj(char *fn, pl_Mat *m);



/******************************************************************************
** Math Code (pl_math.cpp)
******************************************************************************/

/*
  plMatrixRotate() generates a rotation matrix
  Parameters:
    matrix: an array of 16 pl_Floats that is a 4x4 matrix
    m: the axis to rotate around, 1=X, 2=Y, 3=Z.
    Deg: the angle in degrees to rotate
  Returns: 
    nothing
*/
void plMatrixRotate(pl_Float matrix[], pl_uChar m, pl_Float Deg);

/*
  plMatrixTranslate() generates a translation matrix
  Parameters:
    m: the matrix (see plMatrixRotate for more info)
    x,y,z: the translation coordinates
  Returns:
    nothing
*/
void plMatrixTranslate(pl_Float m[], pl_Float x, pl_Float y, pl_Float z);

/* 
  plMatrixMultiply() multiplies two matrices
  Parameters:
    dest: destination matrix will be multipled by src
    src: source matrix
  Returns:
    nothing
  Notes: 
    this is the same as dest = dest*src (since the order *does* matter);
*/
void plMatrixMultiply(pl_Float *dest, pl_Float src[]);

/*
   plMatrixApply() applies a matrix.
  Parameters:
    m: matrix to apply
    x,y,z: input coordinate
    outx,outy,outz: pointers to output coords.
  Returns:
    nothing
  Notes: 
    applies the matrix to the 3d point to produce the transformed 3d point
*/
void plMatrixApply(pl_Float *m, pl_Float x, pl_Float y, pl_Float z, 
                   pl_Float *outx, pl_Float *outy, pl_Float *outz);

/*
  plNormalizeVector() makes a vector a unit vector
  Parameters:
    x,y,z: pointers to the vector
  Returns:
    nothing
*/
void plNormalizeVector(pl_Float *x, pl_Float *y, pl_Float *z);

/*
  plDotProduct() returns the dot product of two vectors
  Parameters:
    x1,y1,z1: the first vector
    x2,y2,z2: the second vector
  Returns:
    the dot product of the two vectors
*/
pl_Float plDotProduct(pl_Float x1, pl_Float y1, pl_Float z1,
                      pl_Float x2, pl_Float y2, pl_Float z2);




#endif /* !_PLUSH_H_ */
