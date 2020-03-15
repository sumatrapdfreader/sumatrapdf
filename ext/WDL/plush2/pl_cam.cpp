/******************************************************************************
Plush Version 1.2
cam.c
Camera and Rendering
Copyright (c) 1996-2000, Justin Frankel
******************************************************************************/

#include "plush.h"


#include "../lice/lice_extended.h"

#include "../mergesort.h"

#define MACRO_plMatrixApply(m,x,y,z,outx,outy,outz) \
      ( outx ) = ( x )*( m )[0] + ( y )*( m )[1] + ( z )*( m )[2] + ( m )[3];\
      ( outy ) = ( x )*( m )[4] + ( y )*( m )[5] + ( z )*( m )[6] + ( m )[7];\
      ( outz ) = ( x )*( m )[8] + ( y )*( m )[9] + ( z )*( m )[10] + ( m )[11]

#define MACRO_plDotProduct(x1,y1,z1,x2,y2,z2) \
      ((( x1 )*( x2 ))+(( y1 )*( y2 ))+(( z1 )*( z2 )))

#define MACRO_plNormalizeVector(x,y,z) { \
  double length; \
  length = ( x )*( x )+( y )*( y )+( z )*( z ); \
  if (length > 0.0000000001) { \
    double __l = 1.0/sqrt(length); \
    ( x ) *= __l; \
    ( y ) *= __l; \
    ( z ) *= __l; \
  } \
}


static void _FindNormal(double x2, double x3,double y2, double y3,
                        double zv, double *res) {
  res[0] = zv*(y2-y3);
  res[1] = zv*(x3-x2);
  res[2] = x2*y3 - y2*x3;
}


void pl_Cam::SetTarget(pl_Float x, pl_Float y, pl_Float z) {
  double dx, dy, dz;
  dx = x - X;
  dy = y - Y;
  dz = z - Z;
  Roll = 0;
  if (dz > 0.0001f) {
    Pan = (pl_Float) (-atan(dx/dz)*(180.0/PL_PI));
    dz /= cos(Pan*(PL_PI/180.0));
    Pitch = (pl_Float) (atan(dy/dz)*(180.0/PL_PI));
  } else if (dz < -0.0001f) { 
    Pan = (pl_Float) (180.0-atan(dx/dz)*(180.0/PL_PI));
    dz /= cos((Pan-180.0f)*(PL_PI/180.0));
    Pitch = (pl_Float) (-atan(dy/dz)*(180.0/PL_PI));
  } else {
    Pan = 0.0f;
    Pitch = -90.0f;
  }
}

void pl_Cam::RecalcFrustum() 
{
  int fbw=frameBuffer->getWidth();
  int fbh=frameBuffer->getHeight();
  int cx = CenterX + fbw/2;
  int cy = CenterY + fbh/2;

  m_adj_asp = 1.0 / AspectRatio;
  m_fovfactor = fbw/tan(plMin(plMax(Fov,1.0),179.0)*(PL_PI/360.0));
  memset(m_clipPlanes,0,sizeof(m_clipPlanes));

  /* Back */
  m_clipPlanes[0][2] = -1.0; 
  m_clipPlanes[0][3] = -ClipBack;

  /* Left */
  m_clipPlanes[1][3] = 0.00000001;
  if (cx == 0) m_clipPlanes[1][0] = 1.0;
  else 
  {
    _FindNormal(-100,-100, 
                100, -100,
                m_fovfactor*100.0/cx,
                m_clipPlanes[1]);
    if (cx < 0) 
    {
      m_clipPlanes[1][0] = -m_clipPlanes[1][0];
      m_clipPlanes[1][1] = -m_clipPlanes[1][1];
      m_clipPlanes[1][2] = -m_clipPlanes[1][2];
    }
  }

  /* Right */
  m_clipPlanes[2][3] = 0.00000001;
  if (fbw == cx) m_clipPlanes[2][0] = -1.0;
  else 
  {
    _FindNormal(100,100, 
                -100, 100,
                m_fovfactor*100.0/(fbw-cx),
                m_clipPlanes[2]);
    if (cx > fbw) 
    {
      m_clipPlanes[2][0] = -m_clipPlanes[2][0];
      m_clipPlanes[2][1] = -m_clipPlanes[2][1];
      m_clipPlanes[2][2] = -m_clipPlanes[2][2];
    }
  }

  /* Top */
  m_clipPlanes[3][3] = 0.00000001;
  if (cy == 0) m_clipPlanes[3][1] = 1.0;
  else 
  {
    _FindNormal(100, -100, 
                100, 100,
                m_fovfactor*m_adj_asp*-100.0/(cy),
                m_clipPlanes[3]);
    if (cy < 0) 
    {
      m_clipPlanes[3][0] = -m_clipPlanes[3][0];
      m_clipPlanes[3][1] = -m_clipPlanes[3][1];
      m_clipPlanes[3][2] = -m_clipPlanes[3][2];
    }
  }
 
  /* Bottom */
  m_clipPlanes[4][3] = 0.00000001;
  if (cy == fbh) m_clipPlanes[4][1] = -1.0;
  else 
  {
    _FindNormal(-100, 100, 
                -100, -100,
                m_fovfactor*m_adj_asp*100.0/(cy-fbh),
                m_clipPlanes[4]);
    if (cy > fbh) 
    {
      m_clipPlanes[4][0] = -m_clipPlanes[4][0];
      m_clipPlanes[4][1] = -m_clipPlanes[4][1];
      m_clipPlanes[4][2] = -m_clipPlanes[4][2];
    }
  }

}


 /* Returns: 0 if nothing gets in,  1 or 2 if pout1 & pout2 get in */
pl_uInt pl_Cam::_ClipToPlane(pl_uInt numVerts, pl_Float  *plane)
{
  pl_uInt i, nextvert, curin, nextin;
  double curdot, nextdot, scale;
  pl_uInt invert, outvert;
  invert = 0;
  outvert = 0;
  curdot = m_cl[0].newVertices[0].xformedx*plane[0] +
           m_cl[0].newVertices[0].xformedy*plane[1] +
           m_cl[0].newVertices[0].xformedz*plane[2];
  curin = (curdot >= plane[3]);

  for (i=0 ; i < numVerts; i++) {
    nextvert = (i + 1) % numVerts;
    if (curin) {
      memcpy(&m_cl[1].ShadeInfos[outvert][0],&m_cl[0].ShadeInfos[invert][0],3*sizeof(pl_Float));
      int a;
      for(a=0;a<PLUSH_MAX_MAPCOORDS;a++)
      {
        m_cl[1].MappingU[a][outvert] = m_cl[0].MappingU[a][invert];
        m_cl[1].MappingV[a][outvert] = m_cl[0].MappingV[a][invert];
      }
      m_cl[1].newVertices[outvert++] = m_cl[0].newVertices[invert];
    }
    nextdot = m_cl[0].newVertices[nextvert].xformedx*plane[0] +
              m_cl[0].newVertices[nextvert].xformedy*plane[1] +
              m_cl[0].newVertices[nextvert].xformedz*plane[2];
    nextin = (nextdot >= plane[3]);
    if (curin != nextin) {
      scale = (plane[3] - curdot) / (nextdot - curdot);
      m_cl[1].newVertices[outvert].xformedx = (pl_Float) (m_cl[0].newVertices[invert].xformedx +
           (m_cl[0].newVertices[nextvert].xformedx - m_cl[0].newVertices[invert].xformedx)
             * scale);
      m_cl[1].newVertices[outvert].xformedy = (pl_Float) (m_cl[0].newVertices[invert].xformedy +
           (m_cl[0].newVertices[nextvert].xformedy - m_cl[0].newVertices[invert].xformedy)
             * scale);
      m_cl[1].newVertices[outvert].xformedz = (pl_Float) (m_cl[0].newVertices[invert].xformedz +
           (m_cl[0].newVertices[nextvert].xformedz - m_cl[0].newVertices[invert].xformedz)
             * scale);

      m_cl[1].ShadeInfos[outvert][0] = m_cl[0].ShadeInfos[invert][0] + (m_cl[0].ShadeInfos[nextvert][0] - m_cl[0].ShadeInfos[invert][0]) * scale;
      m_cl[1].ShadeInfos[outvert][1] = m_cl[0].ShadeInfos[invert][1] + (m_cl[0].ShadeInfos[nextvert][1] - m_cl[0].ShadeInfos[invert][1]) * scale;
      m_cl[1].ShadeInfos[outvert][2] = m_cl[0].ShadeInfos[invert][2] + (m_cl[0].ShadeInfos[nextvert][2] - m_cl[0].ShadeInfos[invert][2]) * scale;

      int a;
      for(a=0;a<PLUSH_MAX_MAPCOORDS;a++)
      {
        m_cl[1].MappingU[a][outvert] = m_cl[0].MappingU[a][invert] + 
             (m_cl[0].MappingU[a][nextvert] - m_cl[0].MappingU[a][invert]) * scale;
        m_cl[1].MappingV[a][outvert] = m_cl[0].MappingV[a][invert] + 
             (m_cl[0].MappingV[a][nextvert] - m_cl[0].MappingV[a][invert]) * scale;
      }
      outvert++;
    }
    curdot = nextdot;
    curin = nextin;
    invert++;
  }
  return outvert;
}





void pl_Cam::ClipRenderFace(pl_Face *face, pl_Obj *obj) {
  int cx = CenterX + (frameBuffer->getWidth())/2;
  int cy = CenterY + (frameBuffer->getHeight())/2;

  {
    pl_Vertex *vlist=obj->Vertices.Get();
    int a;
    for (a = 0; a < 3; a ++) {
      m_cl[0].newVertices[a] = vlist[face->VertexIndices[a]];

      memcpy(&m_cl[0].ShadeInfos[a][0],&face->Shades[a][0],3*sizeof(pl_Float));
      int b;
      for(b=0;b<PLUSH_MAX_MAPCOORDS;b++)
      {
        m_cl[0].MappingU[b][a] = face->MappingU[b][a];
        m_cl[0].MappingV[b][a] = face->MappingV[b][a];
      }
    }
  }

  pl_uInt numVerts = 3;
  {
    int a = (m_clipPlanes[0][3] < 0.0 ? 0 : 1);
    while (a < PL_NUM_CLIP_PLANES && numVerts > 2)
    {
      numVerts = _ClipToPlane(numVerts, m_clipPlanes[a]);
      memcpy(&m_cl[0],&m_cl[1],sizeof(m_cl[0]));
      a++;
    }
  }
  if (numVerts > 2) {
    pl_Face newface;
    memcpy(&newface,face,sizeof(pl_Face));
    int k;
    for (k = 2; k < (int)numVerts; k ++) {
      int a;
      for (a = 0; a < 3; a ++) {
        int w;
        if (a == 0) w = 0;
        else w = a+(k-2);        ;
        pl_Vertex *thisv=m_cl[0].newVertices+w;
        newface.Shades[a][0] = m_cl[0].ShadeInfos[w][0];
        newface.Shades[a][1] = m_cl[0].ShadeInfos[w][1];
        newface.Shades[a][2] = m_cl[0].ShadeInfos[w][2];
        int b;
        for(b=0;b<PLUSH_MAX_MAPCOORDS;b++)
        {
          newface.MappingU[b][a] = m_cl[0].MappingU[b][w];
          newface.MappingV[b][a] = m_cl[0].MappingV[b][w];
        }
        newface.Scrz[a] = 1.0f/thisv->xformedz;
        double ytmp = m_fovfactor * newface.Scrz[a];
        double xtmp = ytmp*thisv->xformedx;
        ytmp *= thisv->xformedy*m_adj_asp;
        newface.Scrx[a] = xtmp+cx;
        newface.Scry[a] = ytmp+cy;
      }
      RenderTrisOut++;

      // quick approx of triangle area
      RenderPixelsOut += 0.5*fabs( 
        (newface.Scrx[1] - newface.Scrx[0]) * 
        (newface.Scry[2] - newface.Scry[0]) - 
        (newface.Scrx[2] - newface.Scrx[0]) * 
        (newface.Scry[1] - newface.Scry[0]) );

      if (frameBuffer->Extended(LICE_EXT_SUPPORTS_ID,(void*)(INT_PTR)LICE_EXT_DRAWTRIANGLE_ACCEL))
      {
        LICE_Ext_DrawTriangle_acceldata ac;
        ac.mat = newface.Material;
        int x,y;
        for(x=0;x<3;x++) for(y=0;y<3;y++) ac.VertexShades[x][y]=newface.Shades[x][y];
        for(x=0;x<3;x++) 
        {
          ac.scrx[x]=newface.Scrx[x];
          ac.scry[x]=newface.Scry[x];
          ac.scrz[x]=newface.Scrz[x];
        }
        for(x=0;x<2;x++)
        {
          int tidx=x?newface.Material->TexMapIdx : newface.Material->Tex2MapIdx;
          if (tidx<0 || tidx>=PLUSH_MAX_MAPCOORDS)tidx=PLUSH_MAX_MAPCOORDS-1;
          for(y=0;y<3;y++)
          {
            ac.mapping_coords[x][y][0]=newface.MappingU[tidx][y];
            ac.mapping_coords[x][y][1]=newface.MappingV[tidx][y];
          }
        }
          
        frameBuffer->Extended(LICE_EXT_DRAWTRIANGLE_ACCEL,&ac);
      }
      else PutFace(&newface);
    }
  }
}

pl_sInt pl_Cam::ClipNeeded(pl_Face *face, pl_Obj *obj) {
  double dr,dl,db,dt; 
  double f;
  int fbw=(frameBuffer->getWidth());
  int fbh=(frameBuffer->getHeight());
  int cx = CenterX + fbw/2;
  int cy = CenterY + fbh/2;
  dr = (fbw-cx);
  dl = (-cx);
  db = (fbh-cy);
  dt = (-cy);
  f = m_fovfactor*m_adj_asp;
  pl_Vertex *vlist=obj->Vertices.Get();
  pl_Vertex *v0=vlist+face->VertexIndices[0];
  pl_Vertex *v1=vlist+face->VertexIndices[1];
  pl_Vertex *v2=vlist+face->VertexIndices[2];

  return ((ClipBack <= 0.0 ||
           v0->xformedz <= ClipBack ||
           v1->xformedz <= ClipBack ||
           v2->xformedz <= ClipBack) &&
          (v0->xformedz >= 0 ||
           v1->xformedz >= 0 || 
           v2->xformedz >= 0) &&
          (v0->xformedx*m_fovfactor<=dr*v0->xformedz ||
           v1->xformedx*m_fovfactor<=dr*v1->xformedz ||
           v2->xformedx*m_fovfactor<=dr*v2->xformedz) &&
          (v0->xformedx*m_fovfactor>=dl*v0->xformedz ||
           v1->xformedx*m_fovfactor>=dl*v1->xformedz ||
           v2->xformedx*m_fovfactor>=dl*v2->xformedz) &&
          (v0->xformedy*f<=db*v0->xformedz ||
           v1->xformedy*f<=db*v1->xformedz ||
           v2->xformedy*f<=db*v2->xformedz) &&
          (v0->xformedy*f>=dt*v0->xformedz ||
           v1->xformedy*f>=dt*v1->xformedz ||
           v2->xformedy*f>=dt*v2->xformedz));
}






void pl_Cam::Begin(LICE_IBitmap *fb, bool want_zbclear, pl_ZBuffer zbclear) {
  if (frameBuffer||!fb) return;

  if (WantZBuffer)
  {
    int zbsz=fb->getWidth()*fb->getHeight();
    pl_ZBuffer *zb=zBuffer.Resize(zbsz);
    if (want_zbclear)
    {
      if (!zbclear) memset(zb,0,zbsz*sizeof(pl_ZBuffer));
      else
      {
        int i=zbsz;
        while(i--) *zb++=zbclear;
      }
    }
  }
  else zBuffer.Resize(0);
  pl_Float tempMatrix[16];
  _numlights = 0;
  _numfaces = _numfaces_sorted = 0;
  frameBuffer = fb;
  plMatrixRotate(_cMatrix,2,-Pan);
  plMatrixRotate(tempMatrix,1,-Pitch);
  plMatrixMultiply(_cMatrix,tempMatrix);
  plMatrixRotate(tempMatrix,3,-Roll);
  plMatrixMultiply(_cMatrix,tempMatrix);
  
  RecalcFrustum();

  RenderTrisIn=RenderTrisCulled=RenderTrisOut=0;
  RenderPixelsOut=0.0;

}

void pl_Cam::RenderLight(pl_Light *light) {
  if (!light||!frameBuffer) return;

  pl_Float *pl, xp, yp, zp;
  if (light->Type == PL_LIGHT_NONE) return;
  if (_lights.GetSize()<=_numlights) _lights.Resize(_numlights+1);
  pl = _lights.Get()[_numlights].l;
  if (light->Type == PL_LIGHT_VECTOR) {
    xp = light->Xp;
    yp = light->Yp;
    zp = light->Zp;
    MACRO_plMatrixApply(_cMatrix,xp,yp,zp,pl[0],pl[1],pl[2]);
  } else if (light->Type & PL_LIGHT_POINT) {
    xp = light->Xp-X;
    yp = light->Yp-Y;
    zp = light->Zp-Z;
    MACRO_plMatrixApply(_cMatrix,xp,yp,zp,pl[0],pl[1],pl[2]);
  }
  _lights.Get()[_numlights++].light = light;
}

void pl_Cam::RenderObject(pl_Obj *obj, pl_Float *bmatrix, pl_Float *bnmatrix) {
  if (!obj||!frameBuffer) return;

  pl_Float oMatrix[16], nMatrix[16], tempMatrix[16];
  
  if (obj->GenMatrix) {
    plMatrixRotate(nMatrix,1,obj->Xa);
    plMatrixRotate(tempMatrix,2,obj->Ya);
    plMatrixMultiply(nMatrix,tempMatrix);
    plMatrixRotate(tempMatrix,3,obj->Za);
    plMatrixMultiply(nMatrix,tempMatrix);
    memcpy(oMatrix,nMatrix,sizeof(pl_Float)*16);
  } else memcpy(nMatrix,obj->RotMatrix,sizeof(pl_Float)*16);

  if (bnmatrix) plMatrixMultiply(nMatrix,bnmatrix);

  if (obj->GenMatrix) {
    plMatrixTranslate(tempMatrix, obj->Xp, obj->Yp, obj->Zp);
    plMatrixMultiply(oMatrix,tempMatrix);
  } else memcpy(oMatrix,obj->Matrix,sizeof(pl_Float)*16);
  if (bmatrix) plMatrixMultiply(oMatrix,bmatrix);

  {
    int i;
    for (i = 0; i < obj->Children.GetSize(); i ++)
      if (obj->Children.Get(i)) RenderObject(obj->Children.Get(i),oMatrix,nMatrix);
  }
  if (!obj->Faces.GetSize() || !obj->Vertices.GetSize()) return;

  plMatrixTranslate(tempMatrix, -X, -Y, -Z);
  plMatrixMultiply(oMatrix,tempMatrix);
  plMatrixMultiply(oMatrix,_cMatrix);
  plMatrixMultiply(nMatrix,_cMatrix);
  
  {
    pl_Vertex *vertex = obj->Vertices.Get();
    int i = obj->Vertices.GetSize();

    while (i--)
    {
      MACRO_plMatrixApply(oMatrix,vertex->x,vertex->y,vertex->z, 
                    vertex->xformedx, vertex->xformedy, vertex->xformedz); 
      MACRO_plMatrixApply(nMatrix,vertex->nx,vertex->ny,vertex->nz,
                    vertex->xformednx,vertex->xformedny,vertex->xformednz);
      vertex++;
    }
  }

  if (_faces.GetSize() < _numfaces + obj->Faces.GetSize()) _faces.Resize(_numfaces + obj->Faces.GetSize());


  _faceInfo *facelistout = _faces.Get() + _numfaces;

  pl_Face *face = obj->Faces.Get();
  int facecnt = obj->Faces.GetSize();

  RenderTrisIn += facecnt;
  _numfaces += facecnt;
  pl_Vertex *vlist = obj->Vertices.Get();

  while (facecnt--) 
  {
    double nx,ny,nz;
    pl_Mat *mat=face->Material;
    if (mat->BackfaceCull || (mat->Lightable && !mat->Smoothing))
    {
      MACRO_plMatrixApply(nMatrix,face->nx,face->ny,face->nz,nx,ny,nz);
    }
    pl_Vertex *v0=vlist+face->VertexIndices[0];
    pl_Vertex *v1=vlist+face->VertexIndices[1];
    pl_Vertex *v2=vlist+face->VertexIndices[2];

    if (!mat->BackfaceCull || (MACRO_plDotProduct(nx,ny,nz, v0->xformedx, v0->xformedy, v0->xformedz) < 0.0000001)) {
      if (ClipNeeded(face,obj)) {
        if (!mat->Smoothing && (mat->Lightable||mat->FadeDist)) {
          pl_Float val[3];
          memcpy(val,face->sLighting,3*sizeof(pl_Float));
          if (mat->Lightable) {
            _lightInfo *inf = _lights.Get();
            int i=_numlights;
            while (i--)
            {
              pl_Light *light = inf->light;
              double lightsc=0.0;
              if (light->Type & PL_LIGHT_POINT_ANGLE) {
                double nx2 = inf->l[0] - v0->xformedx; 
                double ny2 = inf->l[1] - v0->xformedy; 
                double nz2 = inf->l[2] - v0->xformedz;
                MACRO_plNormalizeVector(nx2,ny2,nz2);
                lightsc = MACRO_plDotProduct(nx,ny,nz,nx2,ny2,nz2);
              } 
              if (light->Type & PL_LIGHT_POINT_DISTANCE) {
                double nx2 = inf->l[0] - v0->xformedx; 
                double ny2 = inf->l[1] - v0->xformedy; 
                double nz2 = inf->l[2] - v0->xformedz;
                if (light->Type & PL_LIGHT_POINT_ANGLE) {
                   nx2 = (1.0 - 0.5*((nx2*nx2+ny2*ny2+nz2*nz2)/
                           light->HalfDistSquared));
                  lightsc *= plMax(0,plMin(1.0,nx2));
                } else { 
                  lightsc = (1.0 - 0.5*((nx2*nx2+ny2*ny2+nz2*nz2)/
                    light->HalfDistSquared));
                  lightsc = plMax(0,plMin(1.0,lightsc));
                }
              } 
              if (light->Type == PL_LIGHT_VECTOR) 
                lightsc = MACRO_plDotProduct(nx,ny,nz,inf->l[0],inf->l[1],inf->l[2]);

              if (lightsc>0.0)
              {
                val[0] += light->Intensity[0]*lightsc;
                val[1] += light->Intensity[1]*lightsc;
                val[2] += light->Intensity[2]*lightsc;
              }
              else if (mat->BackfaceIllumination) 
              {
                val[0] -= light->Intensity[0]*lightsc*mat->BackfaceIllumination;
                val[1] -= light->Intensity[1]*lightsc*mat->BackfaceIllumination;
                val[2] -= light->Intensity[2]*lightsc*mat->BackfaceIllumination;
              }
              inf++;
            } /* End of light loop */ 
          } /* End of flat shading if */

          if (mat->FadeDist)
          {
            double lightsc = 1.0 - (v0->xformedz+v1->xformedz+v2->xformedz) / (mat->FadeDist*3.0);
            if (lightsc<0.0) lightsc=0.0;
            else if (lightsc>1.0)lightsc=1.0;
            if (mat->Lightable)
            {
              val[0] *= lightsc;
              val[1] *= lightsc;
              val[2] *= lightsc;
            }
            else
            {
              val[0]+=lightsc;
              val[1]+=lightsc;
              val[2]+=lightsc;
            }
          }
          face->Shades[0][0]=mat->Ambient[0] + mat->Diffuse[0]*val[0];
          face->Shades[0][1]=mat->Ambient[1] + mat->Diffuse[1]*val[1];
          face->Shades[0][2]=mat->Ambient[2] + mat->Diffuse[2]*val[2];
        } 
        else memcpy(face->Shades,mat->Ambient,sizeof(mat->Ambient)); // flat shading

        if ((mat->Texture && mat->TexMapIdx<0)||(mat->Texture2 && mat->Tex2MapIdx<0)) {
          face->MappingU[PLUSH_MAX_MAPCOORDS-1][0] = 0.5 + (v0->xformednx);
          face->MappingV[PLUSH_MAX_MAPCOORDS-1][0] = 0.5 - (v0->xformedny);
          face->MappingU[PLUSH_MAX_MAPCOORDS-1][1] = 0.5 + (v1->xformednx);
          face->MappingV[PLUSH_MAX_MAPCOORDS-1][1] = 0.5 - (v1->xformedny);
          face->MappingU[PLUSH_MAX_MAPCOORDS-1][2] = 0.5 + (v2->xformednx);
          face->MappingV[PLUSH_MAX_MAPCOORDS-1][2] = 0.5 - (v2->xformedny);
        } 

        if (mat->Smoothing && (mat->Lightable || mat->FadeDist)) 
        {
          int a;
          for (a = 0; a < 3; a ++) {
            pl_Float val[3];
            memcpy(val,face->vsLighting[a],sizeof(val));
            pl_Vertex *thisvert  = obj->Vertices.Get()+face->VertexIndices[a];

            if (mat->Lightable) 
            {
              int i=_numlights;
              _lightInfo *inf = _lights.Get();
              while (i--)
              {
                double lightsc = 0.0;
                pl_Light *light = inf->light;
                if (light->Type & PL_LIGHT_POINT_ANGLE) {
                  double nx2 = inf->l[0] - thisvert->xformedx; 
                  double ny2 = inf->l[1] - thisvert->xformedy; 
                  double nz2 = inf->l[2] - thisvert->xformedz;
                  MACRO_plNormalizeVector(nx2,ny2,nz2);
                  lightsc = MACRO_plDotProduct(thisvert->xformednx,
                                      thisvert->xformedny,
                                      thisvert->xformednz,
                                      nx2,ny2,nz2);
                } 
                if (light->Type & PL_LIGHT_POINT_DISTANCE) {
                  double nx2 = inf->l[0] - thisvert->xformedx; 
                  double ny2 = inf->l[1] - thisvert->xformedy; 
                  double nz2 = inf->l[2] - thisvert->xformedz;
                  if (light->Type & PL_LIGHT_POINT_ANGLE) {
                     double t= (1.0 - 0.5*((nx2*nx2+ny2*ny2+nz2*nz2)/light->HalfDistSquared));
                     lightsc *= plMax(0,plMin(1.0,t));
                  } else {
                    lightsc = (1.0 - 0.5*((nx2*nx2+ny2*ny2+nz2*nz2)/light->HalfDistSquared));
                    lightsc = plMax(0,plMin(1.0,lightsc));
                  }
                }

                if (light->Type == PL_LIGHT_VECTOR)
                  lightsc = MACRO_plDotProduct(thisvert->xformednx,
                                      thisvert->xformedny,
                                      thisvert->xformednz,
                                      inf->l[0],inf->l[1],inf->l[2]);
                if (lightsc > 0.0) 
                {
                  val[0] += lightsc * light->Intensity[0];
                  val[1] += lightsc * light->Intensity[1];
                  val[2] += lightsc * light->Intensity[2];
                }
                else if (mat->BackfaceIllumination) 
                {
                  val[0] -= lightsc * light->Intensity[0]*mat->BackfaceIllumination;
                  val[1] -= lightsc * light->Intensity[1]*mat->BackfaceIllumination;
                  val[2] -= lightsc * light->Intensity[2]*mat->BackfaceIllumination;
                }
                inf++;
              } /* End of light loop */
            } /* End of gouraud shading if */
            if (mat->FadeDist)
            {
              double lightsc = 1.0-thisvert->xformedz/mat->FadeDist;
              if (lightsc<0.0) lightsc=0.0;
              else if (lightsc>1.0)lightsc=1.0;
              if (mat->Lightable)
              {
                val[0] *= lightsc;
                val[1] *= lightsc;
                val[2] *= lightsc;
              }
              else
              {
                val[0] += lightsc;
                val[1] += lightsc;
                val[2] += lightsc;
              }
            }
            face->Shades[a][0] = mat->Ambient[0] + mat->Diffuse[0]*val[0];
            face->Shades[a][1] = mat->Ambient[1] + mat->Diffuse[1]*val[1];
            face->Shades[a][2] = mat->Ambient[2] + mat->Diffuse[2]*val[2];
          } /* End of vertex loop for */ 
        } /* End of gouraud shading mask if */
        else // flat modes, shade all vertices
        {
          memcpy(&face->Shades[1][0],&face->Shades[0][0],sizeof(pl_Float)*3);
          memcpy(&face->Shades[2][0],&face->Shades[0][0],sizeof(pl_Float)*3);
        }

        facelistout->zd = v0->xformedz+v1->xformedz+v2->xformedz;
        facelistout->obj=obj;
        facelistout->face = face;
        facelistout++;

        
        RenderTrisCulled++;

      } /* Is it in our area Check */
    } /* Backface Check */
    face++;
  }
  _numfaces = facelistout-_faces.Get();
}
void pl_Cam::SortToCurrent()
{
  if (Sort && _numfaces > _numfaces_sorted+1)
  {
    WDL_mergesort(_faces.Get()+_numfaces_sorted,
                  _numfaces-_numfaces_sorted,sizeof(_faceInfo),
                  Sort > 0 ? sortFwdFunc : sortRevFunc,
                  (char*)_sort_tmpspace.Resize((_numfaces-_numfaces_sorted)*sizeof(_faceInfo),false));
  }
  _numfaces_sorted=_numfaces;
}

int pl_Cam::sortRevFunc(const void *a, const void *b)
{
  _faceInfo *aa = (_faceInfo*)a;
  _faceInfo *bb = (_faceInfo*)b;

  if (aa->zd < bb->zd) return -1;
  if (aa->zd > bb->zd) return 1;
  return 0;
}

int pl_Cam::sortFwdFunc(const void *a, const void *b)
{
  _faceInfo *aa = (_faceInfo*)a;
  _faceInfo *bb = (_faceInfo*)b;

  if (aa->zd < bb->zd) return 1;
  if (aa->zd > bb->zd) return -1;
  return 0;
}

void pl_Cam::End() {
  if (!frameBuffer) return;

  SortToCurrent();

  _faceInfo *f = _faces.Get();
  int n=_numfaces;
  while (n-->0) 
  {
    if (f->face->Material)
    {
      ClipRenderFace(f->face,f->obj);
    }
    f++;
  }
  frameBuffer=0;
  _numfaces=0;
  _numlights = 0;
}




void pl_Light::Set(pl_uChar mode, pl_Float x, pl_Float y, pl_Float z, pl_Float intensity_r, pl_Float intensity_g, pl_Float intensity_b, pl_Float halfDist) {
  pl_Float m[16], m2[16];
  Type = mode;
  Intensity[0] = intensity_r;
  Intensity[1] = intensity_g;
  Intensity[2] = intensity_b;
  HalfDistSquared = halfDist*halfDist;
  switch (mode) {
    case PL_LIGHT_VECTOR:
      plMatrixRotate(m,1,x);
      plMatrixRotate(m2,2,y);
      plMatrixMultiply(m,m2);
      plMatrixRotate(m2,3,z);
      plMatrixMultiply(m,m2);
      plMatrixApply(m,0.0,0.0,-1.0,&Xp, &Yp, &Zp);
    break;
    case PL_LIGHT_POINT_ANGLE:
    case PL_LIGHT_POINT_DISTANCE:
    case PL_LIGHT_POINT:
      Xp = x;
      Yp = y; 
      Zp = z;
    break;
  }
}