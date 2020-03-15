/******************************************************************************
Plush Version 1.2
make.c
Object Primitives
Copyright (c) 1996-2000, Justin Frankel
*******************************************************************************
 Notes:
   Most of these routines are highly unoptimized.
   They could all use some work, such as more capable divisions (Box is
   most notable), etc... The mapping coordinates are all set up nicely, 
   though.
******************************************************************************/

#include "plush.h"

pl_Obj *plMakeTorus(pl_Float r1, pl_Float r2, pl_uInt divrot, pl_uInt divrad, 
                    pl_Mat *m) {
  pl_Obj *o;
  pl_Vertex *v;
  pl_Face *f;
  pl_uInt x, y;
  double ravg, rt, a, da, al, dal;
  pl_Float U,V,dU,dV;
  if (divrot < 3) divrot = 3;
  if (divrad < 3) divrad = 3;
  ravg = (r1+r2)*0.5;
  rt = (r2-r1)*0.5;
  o = new pl_Obj(divrad*divrot,divrad*divrot*2);
  if (!o) return 0;
  v = o->Vertices.Get();
  a = 0.0;
  da = 2*PL_PI/divrot;
  for (y = 0; y < divrot; y ++) {
    al = 0.0;
    dal = 2*PL_PI/divrad;
    for (x = 0; x < divrad; x ++) {
      v->x = (pl_Float) (cos((double) a)*(ravg + cos((double) al)*rt));
      v->z = (pl_Float) (sin((double) a)*(ravg + cos((double) al)*rt));
      v->y = (pl_Float) (sin((double) al)*rt);
      v++;
      al += dal;
    }
    a += da;
  }
  v = o->Vertices.Get();
  f = o->Faces.Get();
  dV = 1.0/divrad;
  dU = 1.0/divrot;
  U = 0;
  for (y = 0; y < divrot; y ++) {
    V = -0.5;
    for (x = 0; x < divrad; x ++) {
      f->VertexIndices[0] = v+x+y*divrad - o->Vertices.Get();
      f->MappingU[0][0] = U;
      f->MappingV[0][0] = V;
      f->VertexIndices[1] = v+(x+1==divrad?0:x+1)+y*divrad - o->Vertices.Get();
      f->MappingU[0][1] = U;
      f->MappingV[0][1] = V+dV;
      f->VertexIndices[2] = v+x+(y+1==divrot?0:(y+1)*divrad) - o->Vertices.Get();
      f->MappingU[0][2] = U+dU;
      f->MappingV[0][2] = V;
      f->Material = m;
      f++;
      f->VertexIndices[0] = v+x+(y+1==divrot?0:(y+1)*divrad) - o->Vertices.Get();
      f->MappingU[0][0] = U+dU;
      f->MappingV[0][0] = V;
      f->VertexIndices[1] = v+(x+1==divrad?0:x+1)+y*divrad - o->Vertices.Get();
      f->MappingU[0][1] = U;
      f->MappingV[0][1] = V+dV;
      f->VertexIndices[2] = v+(x+1==divrad?0:x+1)+(y+1==divrot?0:(y+1)*divrad) - o->Vertices.Get();
      f->MappingU[0][2] = U+dU;
      f->MappingV[0][2] = V+dV;
      f->Material = m;
      f++;
      V += dV;
    }
    U += dU;
  }
  o->CalculateNormals();
  return (o);
}

pl_Obj *plMakeSphere(pl_Float r, pl_uInt divr, pl_uInt divh, pl_Mat *m) {
  pl_Obj *o;
  pl_Vertex *v;
  pl_Face *f;
  pl_uInt x, y;
  double a, da, yp, ya, yda, yf;
  pl_Float U,V,dU,dV;
  if (divh < 3) divh = 3;
  if (divr < 3) divr = 3;
  o = new pl_Obj(2+(divh-2)*(divr),2*divr+(divh-3)*divr*2);
  if (!o) return 0;
  v = o->Vertices.Get();
  v->x = v->z = 0.0; v->y = r; v++;
  v->x = v->z = 0.0; v->y = -r; v++;
  ya = 0.0;
  yda = PL_PI/(divh-1);
  da = (PL_PI*2.0)/divr;
  for (y = 0; y < divh - 2; y ++) {
    ya += yda;
    yp = cos((double) ya)*r;
    yf = sin((double) ya)*r;
    a = 0.0;
    for (x = 0; x < divr; x ++) {
      v->y = (pl_Float) yp;
      v->x = (pl_Float) (cos((double) a)*yf);
      v->z = (pl_Float) (sin((double) a)*yf);
      v++;
      a += da;
    }
  }
  f = o->Faces.Get();
  v = o->Vertices.Get() + 2;
  a = 0.0; 
  U = 0;
  dU = 1.0/divr;
  dV = V = 1.0/divh;
  for (x = 0; x < divr; x ++) {
    f->VertexIndices[0] = 0;
    f->VertexIndices[1] = v + (x+1==divr ? 0 : x+1) - o->Vertices.Get();
    f->VertexIndices[2] = v + x - o->Vertices.Get();
    f->MappingU[0][0] = U;
    f->MappingV[0][0] = 0;
    f->MappingU[0][1] = U+dU;
    f->MappingV[0][1] = V;
    f->MappingU[0][2] = U;
    f->MappingV[0][2] = V;
    f->Material = m;
    f++;
    U += dU;
  }
  da = 1.0/(divr+1);
  v = o->Vertices.Get() + 2;
  for (x = 0; x < (divh-3); x ++) {
    U = 0;
    for (y = 0; y < divr; y ++) {
      f->VertexIndices[0] = v+y - o->Vertices.Get();
      f->VertexIndices[1] = v+divr+(y+1==divr?0:y+1) - o->Vertices.Get();
      f->VertexIndices[2] = v+y+divr - o->Vertices.Get();
      f->MappingU[0][0] = U;
      f->MappingV[0][0] = V;
      f->MappingU[0][1] = U+dU;
      f->MappingV[0][1] = V+dV;
      f->MappingU[0][2] = U;
      f->MappingV[0][2] = V+dV;
      f->Material = m; f++;
      f->VertexIndices[0] = v+y - o->Vertices.Get();
      f->VertexIndices[1] = v+(y+1==divr?0:y+1) - o->Vertices.Get();
      f->VertexIndices[2] = v+(y+1==divr?0:y+1)+divr - o->Vertices.Get();
      f->MappingU[0][0] = U;
      f->MappingV[0][0] = V;
      f->MappingU[0][1] = U+dU;
      f->MappingV[0][1] = V;
      f->MappingU[0][2] = U+dU;
      f->MappingV[0][2] = V+dV;
      f->Material = m; f++;
      U += dU;
    }
    V += dV;
    v += divr;
  }
  v = o->Vertices.Get() + o->Vertices.GetSize() - divr;
  U = 0;
  for (x = 0; x < divr; x ++) {
    f->VertexIndices[0] = 1;
    f->VertexIndices[1] = v + x - o->Vertices.Get();
    f->VertexIndices[2] = v + (x+1==divr ? 0 : x+1) - o->Vertices.Get();
    f->MappingU[0][0] = U;
    f->MappingV[0][0] = 1.0;
    f->MappingU[0][1] = U;
    f->MappingV[0][1] = V;
    f->MappingU[0][2] = U+dU;
    f->MappingV[0][2] = V;
    f->Material = m;
    f++;
    U += dU;
  }
  o->CalculateNormals();
  return (o);
}

pl_Obj *plMakeCylinder(pl_Float r, pl_Float h, pl_uInt divr, pl_Bool captop, 
                       pl_Bool capbottom, pl_Mat *m) {
  pl_Obj *o;
  pl_Vertex *v, *topverts, *bottomverts, *topcapvert=0, *bottomcapvert=0;
  pl_Face *f;
  pl_uInt32 i;
  double a, da;
  if (divr < 3) divr = 3;
  o = new pl_Obj(divr*2+((divr==3)?0:(captop?1:0)+(capbottom?1:0)),
                  divr*2+(divr==3 ? (captop ? 1 : 0) + (capbottom ? 1 : 0) :
                  (captop ? divr : 0) + (capbottom ? divr : 0)));
  if (!o) return 0;
  a = 0.0;
  da = (2.0*PL_PI)/divr;
  v = o->Vertices.Get();
  topverts = v;
  for (i = 0; i < divr; i ++) {
    v->y = h/2.0f; 
    v->x = (pl_Float) (r*cos((double) a)); 
    v->z = (pl_Float)(r*sin(a));
    v->xformedx = (0.5 + (0.5*cos((double) a))); // temp
    v->xformedy = (0.5 + (0.5*sin((double) a))); // use xf
    v++; 
    a += da;
  }
  bottomverts = v;
  a = 0.0;
  for (i = 0; i < divr; i ++) {
    v->y = -h/2.0f; 
    v->x = (pl_Float) (r*cos((double) a)); 
    v->z = (pl_Float) (r*sin(a));
    v->xformedx = (0.5 + (0.5*cos((double) a)));
    v->xformedy = (0.5 + (0.5*sin((double) a)));
    v++; a += da;
  }
  if (captop && divr != 3) {
    topcapvert = v;
    v->y = h / 2.0f; 
    v->x = v->z = 0.0f;
    v++;
  }
  if (capbottom && divr != 3) {
    bottomcapvert = v;
    v->y = -h / 2.0f; 
    v->x = v->z = 0.0f;
    v++;
  }
  f = o->Faces.Get();
  for (i = 0; i < divr; i ++) {
    f->VertexIndices[0] = bottomverts + i - o->Vertices.Get();
    f->VertexIndices[1] = topverts + i - o->Vertices.Get();
    f->VertexIndices[2] = bottomverts + (i == divr-1 ? 0 : i+1) - o->Vertices.Get();
    f->MappingV[0][0] = f->MappingV[0][2] = 1.0; f->MappingV[0][1] = 0;
    f->MappingU[0][0] = f->MappingU[0][1] = i/(double)divr;
    f->MappingU[0][2] = ((i+1))/(double)divr;
    f->Material = m; f++;
    f->VertexIndices[0] = bottomverts + (i == divr-1 ? 0 : i+1) - o->Vertices.Get();
    f->VertexIndices[1] = topverts + i - o->Vertices.Get();
    f->VertexIndices[2] = topverts + (i == divr-1 ? 0 : i+1) - o->Vertices.Get();
    f->MappingV[0][1] = f->MappingV[0][2] = 0; f->MappingV[0][0] = 1.0;
    f->MappingU[0][0] = f->MappingU[0][2] = ((i+1))/(double)divr;
    f->MappingU[0][1] = (i)/(double)divr;
    f->Material = m; f++;
  }
  if (captop) {
    if (divr == 3) {
      f->VertexIndices[0] = topverts + 0 - o->Vertices.Get();
      f->VertexIndices[1] = topverts + 2 - o->Vertices.Get();
      f->VertexIndices[2] = topverts + 1 - o->Vertices.Get();
      f->MappingU[0][0] = topverts[0].xformedx;
      f->MappingV[0][0] = topverts[0].xformedy;
      f->MappingU[0][1] = topverts[1].xformedx;
      f->MappingV[0][1] = topverts[1].xformedy;
      f->MappingU[0][2] = topverts[2].xformedx;
      f->MappingV[0][2] = topverts[2].xformedy;
      f->Material = m; f++;
    } else {
      for (i = 0; i < divr; i ++) {
        f->VertexIndices[0] = topverts + (i == divr-1 ? 0 : i + 1) - o->Vertices.Get();
        f->VertexIndices[1] = topverts + i - o->Vertices.Get();
        f->VertexIndices[2] = topcapvert - o->Vertices.Get();
        f->MappingU[0][0] = topverts[(i==divr-1?0:i+1)].xformedx;
        f->MappingV[0][0] = topverts[(i==divr-1?0:i+1)].xformedy;
        f->MappingU[0][1] = topverts[i].xformedx;
        f->MappingV[0][1] = topverts[i].xformedy;
        f->MappingU[0][2] = f->MappingV[0][2] = 0.5;
        f->Material = m; f++;
      }
    }
  }
  if (capbottom) {
    if (divr == 3) {
      f->VertexIndices[0] = bottomverts + 0 - o->Vertices.Get();
      f->VertexIndices[1] = bottomverts + 1 - o->Vertices.Get();
      f->VertexIndices[2] = bottomverts + 2 - o->Vertices.Get();
      f->MappingU[0][0] = bottomverts[0].xformedx;
      f->MappingV[0][0] = bottomverts[0].xformedy;
      f->MappingU[0][1] = bottomverts[1].xformedx;
      f->MappingV[0][1] = bottomverts[1].xformedy;
      f->MappingU[0][2] = bottomverts[2].xformedx;
      f->MappingV[0][2] = bottomverts[2].xformedy;
      f->Material = m; f++;
    } else {
      for (i = 0; i < divr; i ++) {
        f->VertexIndices[0] = bottomverts + i - o->Vertices.Get();
        f->VertexIndices[1] = bottomverts + (i == divr-1 ? 0 : i + 1) - o->Vertices.Get();
        f->VertexIndices[2] = bottomcapvert - o->Vertices.Get();
        f->MappingU[0][0] = bottomverts[i].xformedx;
        f->MappingV[0][0] = bottomverts[i].xformedy;
        f->MappingU[0][1] = bottomverts[(i==divr-1?0:i+1)].xformedx;
        f->MappingV[0][1] = bottomverts[(i==divr-1?0:i+1)].xformedy;
        f->MappingU[0][2] = f->MappingV[0][2] = 0.5;
        f->Material = m; f++;
      }
    }
  }
  o->CalculateNormals();
  return (o);
}

pl_Obj *plMakeCone(pl_Float r, pl_Float h, pl_uInt div,
                   pl_Bool cap, pl_Mat *m) {
  pl_Obj *o;
  pl_Vertex *v;
  pl_Face *f;
  pl_uInt32 i;
  double a, da;
  if (div < 3) div = 3;
  o = new pl_Obj(div + (div == 3 ? 1 : (cap ? 2 : 1)),
                  div + (div == 3 ? 1 : (cap ? div : 0)));
  if (!o) return 0;
  v = o->Vertices.Get();
  v->x = v->z = 0; v->y = h/2;
  v->xformedx = 0.5;
  v->xformedy = 0.5;
  v++;
  a = 0.0;
  da = (2.0*PL_PI)/div;
  for (i = 1; i <= div; i ++) {
    v->y = h/-2.0f;
    v->x = (pl_Float) (r*cos((double) a));
    v->z = (pl_Float) (r*sin((double) a));
    v->xformedx = (0.5 + (cos((double) a)*0.5));
    v->xformedy = (0.5 + (sin((double) a)*0.5));
    a += da;
    v++;
  }
  if (cap && div != 3) {
    v->y = h / -2.0f; 
    v->x = v->z = 0.0f;
    v->xformedx = 0.5;
    v->xformedy = 0.5;
    v++;
  }
  f = o->Faces.Get();
  for (i = 1; i <= div; i ++) {
    f->VertexIndices[0] = 0;
    f->VertexIndices[1] = o->Vertices.Get() + (i == div ? 1 : i + 1) - o->Vertices.Get();
    f->VertexIndices[2] = o->Vertices.Get() + i - o->Vertices.Get();
    f->MappingU[0][0] = o->Vertices.Get()[0].xformedx;
    f->MappingV[0][0] = o->Vertices.Get()[0].xformedy;
    f->MappingU[0][1] = o->Vertices.Get()[(i==div?1:i+1)].xformedx;
    f->MappingV[0][1] = o->Vertices.Get()[(i==div?1:i+1)].xformedy;
    f->MappingU[0][2] = o->Vertices.Get()[i].xformedx;
    f->MappingV[0][2] = o->Vertices.Get()[i].xformedy;
    f->Material = m;
    f++;
  }
  if (cap) {
    if (div == 3) {
      f->VertexIndices[0] = 1;
      f->VertexIndices[1] = 2;
      f->VertexIndices[2] = 3;
      f->MappingU[0][0] = o->Vertices.Get()[1].xformedx;
      f->MappingV[0][0] = o->Vertices.Get()[1].xformedy;
      f->MappingU[0][1] = o->Vertices.Get()[2].xformedx;
      f->MappingV[0][1] = o->Vertices.Get()[2].xformedy;
      f->MappingU[0][2] = o->Vertices.Get()[3].xformedx;
      f->MappingV[0][2] = o->Vertices.Get()[3].xformedy;
      f->Material = m;
      f++;
    } else {
      for (i = 1; i <= div; i ++) {
        f->VertexIndices[0] = div + 1;
        f->VertexIndices[1] = i;
        f->VertexIndices[2] = (i==div ? 1 : i+1);
        f->MappingU[0][0] = o->Vertices.Get()[div+1].xformedx;
        f->MappingV[0][0] = o->Vertices.Get()[div+1].xformedy;
        f->MappingU[0][1] = o->Vertices.Get()[i].xformedx;
        f->MappingV[0][1] = o->Vertices.Get()[i].xformedy;
        f->MappingU[0][2] = o->Vertices.Get()[i==div?1:i+1].xformedx;
        f->MappingV[0][2] = o->Vertices.Get()[i==div?1:i+1].xformedy;
        f->Material = m;
        f++;
      }
    }
  }
  o->CalculateNormals();
  return (o);
}

static pl_uChar verts[6*6] = { 
  0,4,1, 1,4,5, 0,1,2, 3,2,1, 2,3,6, 3,7,6,
  6,7,4, 4,7,5, 1,7,3, 7,1,5, 2,6,0, 4,0,6
};
static pl_uChar map[24*2*3] = {
  1,0, 1,1, 0,0, 0,0, 1,1, 0,1,
  0,0, 1,0, 0,1, 1,1, 0,1, 1,0,
  0,0, 1,0, 0,1, 1,0, 1,1, 0,1,
  0,0, 1,0, 0,1, 0,1, 1,0, 1,1,
  1,0, 0,1, 0,0, 0,1, 1,0, 1,1,
  1,0, 1,1, 0,0, 0,1, 0,0, 1,1
};


pl_Obj *plMakeBox(pl_Float w, pl_Float d, pl_Float h, pl_Mat *m) {
  pl_uChar *mm = map;
  pl_uChar *vv = verts;
  pl_Obj *o;
  pl_Vertex *v;
  pl_Face *f;
  pl_uInt x;
  o = new pl_Obj(8,12);
  if (!o) return 0;
  v = o->Vertices.Get();
  v->x = -w/2; v->y = h/2; v->z = d/2; v++;
  v->x = w/2; v->y = h/2; v->z = d/2; v++;
  v->x = -w/2; v->y = h/2; v->z = -d/2; v++;
  v->x = w/2; v->y = h/2; v->z = -d/2; v++;
  v->x = -w/2; v->y = -h/2; v->z = d/2; v++;
  v->x = w/2; v->y = -h/2; v->z = d/2; v++;
  v->x = -w/2; v->y = -h/2; v->z = -d/2; v++;
  v->x = w/2; v->y = -h/2; v->z = -d/2; v++;
  f = o->Faces.Get();
  for (x = 0; x < 12; x ++) {
    f->VertexIndices[0] = *vv++;
    f->VertexIndices[1] = *vv++;
    f->VertexIndices[2] = *vv++;
    f->MappingU[0][0] = (pl_Float) *mm++;
    f->MappingV[0][0] = (pl_Float) *mm++;
    f->MappingU[0][1] = (pl_Float) *mm++;
    f->MappingV[0][1] = (pl_Float) *mm++;
    f->MappingU[0][2] = (pl_Float) *mm++;
    f->MappingV[0][2] = (pl_Float) *mm++;
    f->Material = m;
    f++;
  }

  o->CalculateNormals();
  return (o);
}

pl_Obj *plMakePlane(pl_Float w, pl_Float d, pl_uInt res, pl_Mat *m) {
  pl_Obj *o;
  pl_Vertex *v;
  pl_Face *f;
  pl_uInt x, y;
  o = new pl_Obj((res+1)*(res+1),res*res*2);
  if (!o) return 0;
  v = o->Vertices.Get();
  for (y = 0; y <= res; y ++) {
    for (x = 0; x <= res; x ++) {
      v->y = 0;
      v->x = ((x*w)/res) - w/2;
      v->z = ((y*d)/res) - d/2;
      v++;
    }
  }
  f = o->Faces.Get();
  for (y = 0; y < res; y ++) {
    for (x = 0; x < res; x ++) {
      f->VertexIndices[0] = x+(y*(res+1));
      f->MappingU[0][0] = (x)/(double)res;
      f->MappingV[0][0] = (y)/(double)res;
      f->VertexIndices[2] = x+1+(y*(res+1));
      f->MappingU[0][2] = ((x+1))/(double)res;
      f->MappingV[0][2] = (y)/(double)res;
      f->VertexIndices[1] = x+((y+1)*(res+1));
      f->MappingU[0][1] = (x)/(double)res;
      f->MappingV[0][1] = ((y+1))/(double)res;
      f->Material = m;
      f++;
      f->VertexIndices[0] = x+((y+1)*(res+1));
      f->MappingU[0][0] = (x)/(double)res;
      f->MappingV[0][0] = ((y+1))/(double)res;
      f->VertexIndices[2] = x+1+(y*(res+1));
      f->MappingU[0][2] = ((x+1))/(double)res;
      f->MappingV[0][2] = (y)/(double)res;
      f->VertexIndices[1] = x+1+((y+1)*(res+1));
      f->MappingU[0][1] = ((x+1))/(double)res;
      f->MappingV[0][1] = ((y+1))/(double)res;
      f->Material = m;
      f++;
    }
  }
  o->CalculateNormals();
  return (o);
}
