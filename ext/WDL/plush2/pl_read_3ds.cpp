/******************************************************************************
Plush Version 1.2
read_3ds.c
3DS Object Reader
Copyright (c) 1996-2000, Justin Frankel
******************************************************************************/

#include "plush.h"

typedef struct 
{
  pl_uInt16 id;
  void (*func)(pl_uChar *ptr, pl_uInt32 p);
} _pl_3DSChunk;

static pl_Obj *obj;
static pl_Obj *bobj;
static pl_Obj *lobj;
static pl_sInt16 currentobj;
static pl_Mat *_m;

static pl_Float _pl3DSReadFloat(pl_uChar **ptr);
static pl_uInt32 _pl3DSReadDWord(pl_uChar **ptr);
static pl_uInt16 _pl3DSReadWord(pl_uChar **ptr);
static void _pl3DSChunkReader(pl_uChar *ptr, int len);
static void _pl3DSRGBFReader(pl_uChar *f, pl_uInt32 p);
static void _pl3DSRGBBReader(pl_uChar *f, pl_uInt32 p);
static int _pl3DSASCIIZReader(pl_uChar *ptr, pl_uInt32 p, char *as);
static void _pl3DSObjBlockReader(pl_uChar *ptr, pl_uInt32 p);
static void _pl3DSTriMeshReader(pl_uChar *f, pl_uInt32 p);
static void _pl3DSVertListReader(pl_uChar *f, pl_uInt32 p);
static void _pl3DSFaceListReader(pl_uChar *f, pl_uInt32 p);
static void _pl3DSFaceMatReader(pl_uChar *f, pl_uInt32 p);
static void MapListReader(pl_uChar *f, pl_uInt32 p);
static pl_sInt16 _pl3DSFindChunk(pl_uInt16 id);

static _pl_3DSChunk _pl3DSChunkNames[] = {
    {0x4D4D,NULL}, /* Main */
    {0x3D3D,NULL}, /* Object Mesh */
    {0x4000,_pl3DSObjBlockReader},
    {0x4100,_pl3DSTriMeshReader},
    {0x4110,_pl3DSVertListReader},
    {0x4120,_pl3DSFaceListReader},
    {0x4130,_pl3DSFaceMatReader},
    {0x4140,MapListReader},
    {0xAFFF,NULL}, /* Material */
    {0xA010,NULL}, /* Ambient */
    {0xA020,NULL}, /* Diff */
    {0xA030,NULL}, /* Specular */
    {0xA200,NULL}, /* Texture */
    {0x0010,_pl3DSRGBFReader},
    {0x0011,_pl3DSRGBBReader},
};

pl_Obj *plRead3DSObjFromFile(char *fn, pl_Mat *m) 
{
  FILE *f = fopen(fn, "rb");
  if (!f) return 0;
  fseek(f, 0, 2);
  pl_uInt32 p = ftell(f);
  rewind(f);

  WDL_HeapBuf buf;
  buf.Resize(p);
  int s = fread(buf.Get(), 1, p, f);
  fclose(f);

  if(!s) return 0;
  
  return plRead3DSObj(buf.Get(), s, m);
}

pl_Obj *plRead3DSObjFromResource(HINSTANCE hInst, int resid, pl_Mat *m)
{
#ifdef _WIN32
  HRSRC hResource = FindResource(hInst, MAKEINTRESOURCE(resid), "3DS");
  if(!hResource) return NULL;
  
  DWORD imageSize = SizeofResource(hInst, hResource);
  if(imageSize < 6) return NULL;
  
  HGLOBAL res = LoadResource(hInst, hResource);
  const void* pResourceData = LockResource(res);
  if(!pResourceData) return NULL;
  
  unsigned char *data = (unsigned char *)pResourceData;

  pl_Obj *o = plRead3DSObj(data, imageSize, m);

  DeleteObject(res);

  return o;
#else
  return 0;
#endif
}

pl_Obj *plRead3DSObj(void *ptr, int size, pl_Mat *m)
{
  _m = m;
  obj = bobj = lobj = 0;
  currentobj = 0;

  _pl3DSChunkReader((pl_uChar *)ptr, size);

  return bobj;
}

static pl_Float _pl3DSReadFloat(pl_uChar **ptr) {
  pl_uInt32 *i;
  pl_IEEEFloat32 c;
  i = (pl_uInt32 *) &c;
  *i = _pl3DSReadDWord(ptr);
  return ((pl_Float) c);
}

static pl_uInt32 _pl3DSReadDWord(pl_uChar **ptr) 
{
  pl_uInt32 r;
  pl_uChar *p = *ptr;
  r = *p++;
  r |= (*p++)<<8;
  r |= (*p++)<<16;
  r |= (*p++)<<24;
  *ptr += 4;
  return r;
}

static pl_uInt16 _pl3DSReadWord(pl_uChar **ptr) 
{
  pl_uInt16 r;
  pl_uChar *p = *ptr;
  r = *p++;
  r |= (*p++)<<8;
  *ptr += 2;
  return r;
}

static void _pl3DSRGBFReader(pl_uChar *f, pl_uInt32 p) 
{
  pl_Float c[3];
  if(p < 3*4) return;
  c[0] = _pl3DSReadFloat(&f);
  c[1] = _pl3DSReadFloat(&f);
  c[2] = _pl3DSReadFloat(&f);
}

static void _pl3DSRGBBReader(pl_uChar *f, pl_uInt32 p) 
{
  unsigned char c[3];
  if(p < 3) return;
  memcpy(c, f, sizeof(c));
}

static int _pl3DSASCIIZReader(pl_uChar *ptr, pl_uInt32 p, char *as) 
{
  int l = 0;
  while (*ptr && p>0)
  {
    if(as) *as++ = *ptr;
    ptr++;
    l++;
    p--;
  }
  if(as) *as = 0;
  return l+1;
}

static void _pl3DSObjBlockReader(pl_uChar *ptr, pl_uInt32 p) 
{
  int l = _pl3DSASCIIZReader(ptr, p, 0);
  ptr += l;
  p -= l;
  _pl3DSChunkReader(ptr, p);
}

static void _pl3DSTriMeshReader(pl_uChar *ptr, pl_uInt32 p) 
{
  pl_uInt32 i; 
  pl_Face *face;
  obj = new pl_Obj;
  _pl3DSChunkReader(ptr, p);
  i = obj->Faces.GetSize();
  face = obj->Faces.Get();
  while (i--) 
  {
    pl_Vertex *vp=obj->Vertices.Get();
    pl_Vertex *fVertices[3] = {
      vp+face->VertexIndices[0],
      vp+face->VertexIndices[1],
      vp+face->VertexIndices[2],
    };
    face->MappingU[0][0] = fVertices[0]->xformedx;
    face->MappingV[0][0] = fVertices[0]->xformedy;
    face->MappingU[0][1] = fVertices[1]->xformedx;
    face->MappingV[0][1] = fVertices[1]->xformedy;
    face->MappingU[0][2] = fVertices[2]->xformedx;
    face->MappingV[0][2] = fVertices[2]->xformedy;
    face++;
  }
  obj->CalculateNormals();
  if (currentobj == 0) 
  {
    currentobj = 1;
    lobj = bobj = obj;
  } 
  else 
  {
    lobj->Children.Add(obj);
    lobj = obj;
  }
}

static void _pl3DSVertListReader(pl_uChar *f, pl_uInt32 p) 
{
  pl_uInt16 nv;
  pl_Vertex *v;
  int len = (int)p;
  nv = _pl3DSReadWord(&f);
  len -= 2;
  if(len <= 0) return;
  obj->Vertices.Resize(nv);
  v = obj->Vertices.Get();
  while (nv--) 
  {
    memset(v,0,sizeof(pl_Vertex));
    v->x = _pl3DSReadFloat(&f);
    v->y = _pl3DSReadFloat(&f);
    v->z = _pl3DSReadFloat(&f);
    len -= 3*4;
    if(len < 0) return;
    v++;
  }
}

static void _pl3DSFaceListReader(pl_uChar *f, pl_uInt32 p) 
{
  pl_uInt16 nv;
  pl_uInt16 c[3];
  pl_uInt16 flags;
  pl_Face *face;
  int len = (int)p;

  nv = _pl3DSReadWord(&f);
  len -= 2;
  if(len <= 0) return;
  obj->Faces.Resize(nv);
  face = obj->Faces.Get();
  while (nv--) 
  {
    memset(face,0,sizeof(pl_Face));
    c[0] = _pl3DSReadWord(&f);
    c[1] = _pl3DSReadWord(&f);
    c[2] = _pl3DSReadWord(&f);
    flags = _pl3DSReadWord(&f);
    len -= 4*2;
    if(len < 0) return;

    face->VertexIndices[0] = (c[0]&0x0000FFFF);
    face->VertexIndices[1] = (c[1]&0x0000FFFF);
    face->VertexIndices[2] = (c[2]&0x0000FFFF);
    face->Material = _m;
    face++;
  }
  if(len) _pl3DSChunkReader(f, len);
}

static void _pl3DSFaceMatReader(pl_uChar *ptr, pl_uInt32 p) 
{
  pl_uInt16 n, nf;

  int l = _pl3DSASCIIZReader(ptr, p, 0);
  ptr += l;
  p -= l;

  n = _pl3DSReadWord(&ptr);
  while (n--) 
  {
    nf = _pl3DSReadWord(&ptr);
  }
}

static void MapListReader(pl_uChar *f, pl_uInt32 p) 
{
  pl_uInt16 nv;
  pl_Float c[2];
  pl_Vertex *v;
  int len = (int) p;
  nv = _pl3DSReadWord(&f);
  len -= 2;
  v = obj->Vertices.Get();
  if (nv == obj->Vertices.GetSize()) while (nv--) 
  {
    c[0] = _pl3DSReadFloat(&f);
    c[1] = _pl3DSReadFloat(&f);
    len -= 2*4; 
    if (len < 0) return;
    v->xformedx = c[0];
    v->xformedy = c[1];
    v++;
  }
}

static pl_sInt16 _pl3DSFindChunk(pl_uInt16 id) 
{
  pl_sInt16 i;
  for (i = 0; i < sizeof(_pl3DSChunkNames)/sizeof(_pl3DSChunkNames[0]); i++)
    if (id == _pl3DSChunkNames[i].id) return i;
  return -1;
}

static void _pl3DSChunkReader(pl_uChar *ptr, int len) 
{
  pl_uInt32 hlen;
  pl_uInt16 hid;
  pl_sInt16 n;

  while (len > 0) 
  {
    hid = _pl3DSReadWord(&ptr);
    len -= 2;
    if(len <= 0) return;
    hlen = _pl3DSReadDWord(&ptr);
    len -= 4;
    if(len <= 0) return;
    if (hlen == 0) return;
    hlen -= 6;
    n = _pl3DSFindChunk(hid);
    if (n < 0) 
    {
      ptr += hlen;
      len -= hlen;
    }
    else 
    {
      pl_uChar *p = ptr;
      if (_pl3DSChunkNames[n].func != NULL) 
        _pl3DSChunkNames[n].func(p, hlen);
      else 
        _pl3DSChunkReader(p, hlen);

      ptr += hlen;
      len -= hlen;
    }
  }
}

