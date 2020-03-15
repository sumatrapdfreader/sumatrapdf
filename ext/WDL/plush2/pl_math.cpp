/******************************************************************************
Plush Version 1.2
math.c
Math and Matrix Control
Copyright (c) 1996-2000, Justin Frankel
******************************************************************************/

#include "plush.h"

void plMatrixRotate(pl_Float matrix[], pl_uChar m, pl_Float Deg) {
  pl_uChar m1, m2;
  double c,s;
  double d= Deg * PL_PI / 180.0;
  memset(matrix,0,sizeof(pl_Float)*16);
  matrix[((m-1)<<2)+m-1] = matrix[15] = 1.0;
  m1 = (m % 3);
  m2 = ((m1+1) % 3);
  c = cos(d); s = sin(d);
  matrix[(m1<<2)+m1]=(pl_Float)c; matrix[(m1<<2)+m2]=(pl_Float)s;
  matrix[(m2<<2)+m2]=(pl_Float)c; matrix[(m2<<2)+m1]=(pl_Float)-s;
}

void plMatrixTranslate(pl_Float m[], pl_Float x, pl_Float y, pl_Float z) {
  memset(m,0,sizeof(pl_Float)*16);
  m[0] = m[4+1] = m[8+2] = m[12+3] = 1.0;
  m[0+3] = x; m[4+3] = y; m[8+3] = z;
}

void plMatrixMultiply(pl_Float *dest, pl_Float src[]) {
  pl_Float temp[16];
  pl_uInt i;
  memcpy(temp,dest,sizeof(pl_Float)*16);
  for (i = 0; i < 16; i += 4) {
    *dest++ = src[i+0]*temp[(0<<2)+0]+src[i+1]*temp[(1<<2)+0]+
              src[i+2]*temp[(2<<2)+0]+src[i+3]*temp[(3<<2)+0];
    *dest++ = src[i+0]*temp[(0<<2)+1]+src[i+1]*temp[(1<<2)+1]+
              src[i+2]*temp[(2<<2)+1]+src[i+3]*temp[(3<<2)+1];
    *dest++ = src[i+0]*temp[(0<<2)+2]+src[i+1]*temp[(1<<2)+2]+
              src[i+2]*temp[(2<<2)+2]+src[i+3]*temp[(3<<2)+2];
    *dest++ = src[i+0]*temp[(0<<2)+3]+src[i+1]*temp[(1<<2)+3]+
              src[i+2]*temp[(2<<2)+3]+src[i+3]*temp[(3<<2)+3];
  }
}

void plMatrixApply(pl_Float *m, pl_Float x, pl_Float y, pl_Float z,
                   pl_Float *outx, pl_Float *outy, pl_Float *outz) {
  *outx = x*m[0] + y*m[1] + z*m[2] + m[3];
  *outy	= x*m[4] + y*m[5] + z*m[6] + m[7];
  *outz = x*m[8] + y*m[9] + z*m[10] + m[11];
}

pl_Float plDotProduct(pl_Float x1, pl_Float y1, pl_Float z1,
                      pl_Float x2, pl_Float y2, pl_Float z2) {
  return ((x1*x2)+(y1*y2)+(z1*z2));
}

void plNormalizeVector(pl_Float *x, pl_Float *y, pl_Float *z) {
  double length;
  length = (*x)*(*x)+(*y)*(*y)+(*z)*(*z);
  if (length > 0.0000000001) {
    pl_Float t = (pl_Float)sqrt(length);
    *x /= t;
    *y /= t;
    *z /= t;
  } else *x = *y = *z = 0.0;
}

