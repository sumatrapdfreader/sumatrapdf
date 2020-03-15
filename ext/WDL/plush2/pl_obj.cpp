/******************************************************************************
Plush Version 1.2
obj.c
Object control
Copyright (c) 1996-2000, Justin Frankel
******************************************************************************/

#include "plush.h"

void pl_Obj::Scale(pl_Float s) {
  int i = Vertices.GetSize();
  pl_Vertex *v = Vertices.Get();
  while (i--) {
    v->x *= s; v->y *= s; v->z *= s; v++;
  }
  for (i = 0; i < Children.GetSize(); i ++) 
    if (Children.Get(i)) Children.Get(i)->Scale(s);
}

void pl_Obj::Stretch(pl_Float x, pl_Float y, pl_Float z) {
  int i = Vertices.GetSize();
  pl_Vertex *v = Vertices.Get();
  while (i--) {
    v->x *= x; v->y *= y; v->z *= z; v++;
  }
  for (i = 0; i < Children.GetSize(); i ++) 
    if (Children.Get(i)) Children.Get(i)->Stretch(x,y,z);
}

void pl_Obj::Translate(pl_Float x, pl_Float y, pl_Float z) {
  int i = Vertices.GetSize();
  pl_Vertex *v = Vertices.Get();
  while (i--) {
    v->x += x; v->y += y; v->z += z; v++;
  }
  for (i = 0; i < Children.GetSize(); i ++) 
    if (Children.Get(i)) Children.Get(i)->Translate(x,y,z);
}

void pl_Obj::FlipNormals() {
  int i = Vertices.GetSize();
  pl_Vertex *v = Vertices.Get();
  pl_Face *f = Faces.Get();
  while (i--) {
    v->nx = - v->nx; v->ny = - v->ny; v->nz = - v->nz; v++;
  } 
  i = Faces.GetSize();
  while (i--) {
    f->nx = - f->nx; f->ny = - f->ny; f->nz = - f->nz;
    f++;
  }
  for (i = 0; i < Children.GetSize(); i ++) 
    if (Children.Get(i)) Children.Get(i)->FlipNormals();
}


pl_Obj *pl_Obj::Clone() {
  int i;
  pl_Obj *out;
  if (!(out = new pl_Obj(Vertices.GetSize(),Faces.GetSize()))) return 0;
  for (i = 0; i < Children.GetSize(); i ++) 
    out->Children.Add(Children.Get(i) ? Children.Get(i)->Clone() : NULL);

  out->Xa = Xa; out->Ya = Ya; out->Za = Za;
  out->Xp = Xp; out->Yp = Yp; out->Zp = Zp;
  out->GenMatrix = GenMatrix;
  memcpy(out->Vertices.Get(), Vertices.Get(), sizeof(pl_Vertex) * Vertices.GetSize());
  memcpy(out->Faces.Get(),Faces.Get(),sizeof(pl_Face) * Faces.GetSize());
  return out;
}

void pl_Obj::SetMaterial(pl_Mat *m, pl_Bool th) {
  pl_sInt32 i = Faces.GetSize();
  pl_Face *f = Faces.Get();
  while (i--) (f++)->Material = m; 
  if (th) for (i = 0; i < Children.GetSize(); i++) 
    if (Children.Get(i)) Children.Get(i)->SetMaterial(m,true);
}

void pl_Obj::CalculateNormals() {
  int i;
  pl_Vertex *v = Vertices.Get();
  pl_Face *f = Faces.Get();
  double x1, x2, y1, y2, z1, z2;
  i = Vertices.GetSize();
  while (i--) {
    v->nx = 0.0; v->ny = 0.0; v->nz = 0.0;
    v++;
  }
  i = Faces.GetSize();
  while (i--) { 
    pl_Vertex *vp=Vertices.Get();
    pl_Vertex *fVertices[3] = {
      vp+f->VertexIndices[0],
      vp+f->VertexIndices[1],
      vp+f->VertexIndices[2],
    };
    x1 = fVertices[0]->x-fVertices[1]->x;
    x2 = fVertices[0]->x-fVertices[2]->x;
    y1 = fVertices[0]->y-fVertices[1]->y;
    y2 = fVertices[0]->y-fVertices[2]->y;
    z1 = fVertices[0]->z-fVertices[1]->z;
    z2 = fVertices[0]->z-fVertices[2]->z;
    f->nx = (pl_Float) (y1*z2 - z1*y2);
    f->ny = (pl_Float) (z1*x2 - x1*z2);
    f->nz = (pl_Float) (x1*y2 - y1*x2);
    plNormalizeVector(&f->nx, &f->ny, &f->nz);
    fVertices[0]->nx += f->nx;
    fVertices[0]->ny += f->ny;
    fVertices[0]->nz += f->nz;
    fVertices[1]->nx += f->nx;
    fVertices[1]->ny += f->ny;
    fVertices[1]->nz += f->nz;
    fVertices[2]->nx += f->nx;
    fVertices[2]->ny += f->ny;
    fVertices[2]->nz += f->nz;
    f++;
  }
  v = Vertices.Get();
  i = Vertices.GetSize();
  do {
    plNormalizeVector(&v->nx, &v->ny, &v->nz);
    v++;
  } while (--i);

  for (i = 0; i < Children.GetSize(); i ++) 
    if (Children.Get(i)) Children.Get(i)->CalculateNormals();
}
