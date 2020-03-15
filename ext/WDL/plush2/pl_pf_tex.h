
static void 
#ifdef PL_PF_MULTITEX
PLMTexTri
#else
PLTexTri
#endif
(LICE_pixel *gmem, int swidth, pl_Face *TriFace, pl_ZBuffer *zbuf, int zfb_width,
                     int solidalpha, int solidcomb, LICE_IBitmap *tex, pl_Float *texscales, int texalpha, int texcomb, int texmap
#ifdef PL_PF_MULTITEX
                     , LICE_IBitmap *tex2, int tex2alpha, int tex2comb, int texmap2
#endif
                     
                     ) 
{
  pl_sInt32 C1[3], C3[3], C2[3], dC2[3]={0}, dCL[3]={0},dC1[3]={0},  dX2=0, dX1=0;
  pl_Float dZ2=0, dZL=0,dZ1=0;

  PUTFACE_SORT(); 


  solidcomb&=LICE_BLIT_MODE_MASK;
  if (solidcomb == 0 && solidalpha == 256) solidcomb=-1;
  else if (solidalpha==0) solidcomb=-2; // ignore
  int solidalpha2=(256-solidalpha)*256;


  pl_Float dU2=0,dV2=0,dUL=0,dVL=0;
  pl_Float dU1=0, dV1=0;
  bool bilinear = (texcomb&LICE_BLIT_FILTER_MASK)==LICE_BLIT_FILTER_BILINEAR;
  texcomb&=LICE_BLIT_MODE_MASK|LICE_BLIT_USE_ALPHA;
  if (texcomb==LICE_BLIT_MODE_COPY && texalpha==256) texcomb=-1;
  else if (texalpha==0) texcomb=-2;
  int texalpha2=(256-texalpha);

  int tex_rowspan=0;
  LICE_pixel *texture=NULL;
  int tex_w=16,tex_h=16;  

#if defined(PLUSH_NO_SOLIDGOURAUD) || defined(PLUSH_NO_TEXTURE)
  if (tex)
#endif
  {
    texture=tex->getBits();
    tex_rowspan = tex->getRowSpan();
    if (tex->isFlipped())
    {
      texture += tex_rowspan*(tex->getHeight()-1);
      tex_rowspan=-tex_rowspan;
    }
    tex_w=tex->getWidth();
    tex_h=tex->getHeight();  
    tex_rowspan *= 4;
  }
  pl_sInt32 MappingU_Max=tex_w<<16,  MappingV_Max=tex_h<<16;
  texscales[0]*=MappingU_Max;
  texscales[1]*=MappingV_Max;
  tex_w *= 4;

#ifdef PL_PF_MULTITEX

  pl_Float dU2_2=0,dV2_2=0,dUL_2=0,dVL_2=0;
  pl_Float dU1_2=0, dV1_2=0;
  int tex_w_2=16,tex_h_2=16; 
  bool bilinear2 = (tex2comb&LICE_BLIT_FILTER_MASK)==LICE_BLIT_FILTER_BILINEAR;
  tex2comb&=LICE_BLIT_MODE_MASK|LICE_BLIT_USE_ALPHA;
  if (tex2comb==LICE_BLIT_MODE_COPY && tex2alpha==256) tex2comb=-1;
  else if (tex2alpha==0) tex2comb=-2;
  int tex2alpha2=(256-tex2alpha);
  LICE_pixel *texture_2=NULL;
  int tex_rowspan_2 = 0;
  pl_sInt32 MappingU_Max_2,  MappingV_Max_2;

#ifdef PLUSH_NO_TEXTURE
  if (tex2)
#endif
  {
    tex_w_2=tex2->getWidth();
    tex_h_2=tex2->getHeight();  
    texture_2=tex2->getBits();
    tex_rowspan_2 = tex2->getRowSpan();
    if (tex2->isFlipped())
    {
      texture_2 += tex_rowspan_2*(tex2->getHeight()-1);
      tex_rowspan_2=-tex_rowspan_2;
    }
    tex_rowspan_2 *= 4;
  }
  MappingU_Max_2=tex_w_2<<16;
  MappingV_Max_2=tex_h_2<<16;
  texscales[2]*=MappingU_Max_2;
  texscales[3]*=MappingV_Max_2;
  tex_w_2 *= 4;
#endif


  pl_uChar nm = TriFace->Material->PerspectiveCorrect;
  pl_uChar nmb = 0; while (nm) { nmb++; nm >>= 1; }
  nmb = plMin(6,nmb);
  nm = 1<<nmb;


  pl_Float Z1 = TriFace->Scrz[i0];
  pl_Float Z2 = TriFace->Scrz[i1];
  pl_Float Z3 = TriFace->Scrz[i2];

  pl_Float MappingU1=TriFace->MappingU[texmap][i0]*texscales[0]*Z1;
  pl_Float MappingV1=TriFace->MappingV[texmap][i0]*texscales[1]*Z1;
  pl_Float MappingU2=TriFace->MappingU[texmap][i1]*texscales[0]*Z2;
  pl_Float MappingV2=TriFace->MappingV[texmap][i1]*texscales[1]*Z2;
  pl_Float MappingU3=TriFace->MappingU[texmap][i2]*texscales[0]*Z3;
  pl_Float MappingV3=TriFace->MappingV[texmap][i2]*texscales[1]*Z3;

#ifdef PL_PF_MULTITEX
  pl_Float MappingU1_2=TriFace->MappingU[texmap2][i0]*texscales[2]*Z1;
  pl_Float MappingV1_2=TriFace->MappingV[texmap2][i0]*texscales[3]*Z1;
  pl_Float MappingU2_2=TriFace->MappingU[texmap2][i1]*texscales[2]*Z2;
  pl_Float MappingV2_2=TriFace->MappingV[texmap2][i1]*texscales[3]*Z2;
  pl_Float MappingU3_2=TriFace->MappingU[texmap2][i2]*texscales[2]*Z3;
  pl_Float MappingV3_2=TriFace->MappingV[texmap2][i2]*texscales[3]*Z3;
#endif

  int a;
  for(a=0;a<3;a++)
  {
    C1[a] = (pl_sInt32) (TriFace->Shades[i0][a]*(1<<24));
    C2[a] = (pl_sInt32) (TriFace->Shades[i1][a]*(1<<24));
    C3[a] = (pl_sInt32) (TriFace->Shades[i2][a]*(1<<24));
  }
  
  pl_Float U1, U2;
  U1 = U2 = MappingU1;
  pl_Float V1,V2;
  V1 = V2 = MappingV1;
#ifdef PL_PF_MULTITEX
  pl_Float U1_2, U2_2;
  U1_2 = U2_2 = MappingU1_2;
  pl_Float V1_2, V2_2;
  V1_2 = V2_2 = MappingV1_2;
#endif

  pl_sInt32 X2,X1;
  X2 = X1 = Scrx[i0];
  pl_sInt32 Y0 = Scry[i0];
  pl_sInt32 Y1 = Scry[i1];
  pl_sInt32 Y2 = Scry[i2];


  {
    pl_sInt32 dY = Y2-Y0;
    if (dY) {
      dX2 = (Scrx[i2] - X1) / dY;

      pl_Float v = 1.0/dY;
      for(a=0;a<3;a++) dC2[a] = (pl_sInt32) ((C3[a] - C1[a]) * v);
      dZ2 = (Z3 - Z1) * v;
      dU2 = (MappingU3 - U1) * v;
      dV2 = (MappingV3 - V1) * v;
  #ifdef PL_PF_MULTITEX
      dU2_2 = (MappingU3_2 - U1_2) * v;
      dV2_2 = (MappingV3_2 - V1_2) * v;
  #endif
    }
    dY = Y1-Y0;
    if (dY) {
      dX1 = (Scrx[i1] - X1) / dY;
      pl_Float v=1.0/dY;
      dZ1 = (Z2 - Z1) * v;
      for(a=0;a<3;a++) dC1[a] = (pl_sInt32) ((C2[a] - C1[a]) * v);
      dU1 = (MappingU2 - U1) * v;
      dV1 = (MappingV2 - V1) * v;
  #ifdef PL_PF_MULTITEX
      dU1_2 = (MappingU2_2 - U1_2) * v;
      dV1_2 = (MappingV2_2 - V1_2) * v;
  #endif
      if (dX2 < dX1) {
        SWAP(dX1,dX2,pl_sInt32);
        SWAP(dU1,dU2,pl_Float);
        SWAP(dV1,dV2,pl_Float);
  #ifdef PL_PF_MULTITEX
        SWAP(dU1_2,dU2_2,pl_Float);
        SWAP(dV1_2,dV2_2,pl_Float);
  #endif
        SWAP(dZ1,dZ2,pl_Float);
        for(a=0;a<3;a++)
          SWAP(dC1[a],dC2[a],pl_sInt32);
        stat = 2;
      } else stat = 1;
      Z2 = Z1;
      C2[0] = C1[0];
      C2[1] = C1[1];
      C2[2] = C1[2];

    } else {
      if (Scrx[i1] > X1) {
        X2 = Scrx[i1];
        U2 = MappingU2;
        V2 = MappingV2;
  #ifdef PL_PF_MULTITEX
        U2_2 = MappingU2_2;
        V2_2 = MappingV2_2;
  #endif
        stat = 2|4;
      } else {
        X1 = Scrx[i1];
        SWAP(Z1,Z2,pl_Float)
        for(a=0;a<3;a++) SWAP(C1[a],C2[a],pl_sInt32);
        U1 = MappingU2;
        V1 = MappingV2;
  #ifdef PL_PF_MULTITEX
        U1_2 = MappingU2_2;
        V1_2 = MappingV2_2;
  #endif
        stat = 1|8;
      }
    }
    pl_sInt32 tmp = (dX1-dX2)*dY;
    if (tmp) {
      pl_Float v=(1<<XPOS_BITS)/(double)tmp;
      dUL = ((dU1-dU2)*dY)*v;
      dVL = ((dV1-dV2)*dY)*v;
  #ifdef PL_PF_MULTITEX
      dUL_2 = ((dU1_2-dU2_2)*dY)*v;
      dVL_2 = ((dV1_2-dV2_2)*dY)*v;
  #endif
      dZL = ((dZ1-dZ2)*dY)*v;
      for(a=0;a<3;a++) dCL[a] = (pl_sInt32) ( ((dC1[a]-dC2[a])*dY)*v);
    } else {
      tmp = X2-X1;
      if (tmp) {
        pl_Float v=(1<<XPOS_BITS)/(double)tmp;
        dUL = (U2-U1)*v;
        dVL = (V2-V1)*v;
  #ifdef PL_PF_MULTITEX
        dUL_2 = (U2_2-U1_2)*v;
        dVL_2 = (V2_2-V1_2)*v;
  #endif
        dZL = (Z2-Z1)*v;
        for(a=0;a<3;a++) dCL[a] = (pl_sInt32) ((C2[a]-C1[a])*v);
      }
    }
  }

  gmem += (Y0 * swidth);
  zbuf += (Y0 * zfb_width);


  pl_Float pdZL = dZL * nm;
  dUL *= nm;
  dVL *= nm;
#ifdef PL_PF_MULTITEX
  dUL_2 *= nm;
  dVL_2 *= nm;
#endif
  Y1 -= Y0;
  Y0 = Y2-Y0;
  while (Y0--) {
    if (!Y1--) {
      pl_sInt32 dY = Y2-Scry[i1];
      if (dY) {
        DO_STAT_XDELTAS

        pl_Float tmp=1.0/dY;
        dZ1 = (Z3-Z1)*tmp;
        for(a=0;a<3;a++) dC1[a] = (pl_sInt32)((C3[a]-C1[a])*tmp);
        dV1 = (MappingV3 - V1)*tmp;
        dU1 = (MappingU3 - U1)*tmp;
#ifdef PL_PF_MULTITEX
        dV1_2 = (MappingV3_2 - V1_2)*tmp;
        dU1_2 = (MappingU3_2 - U1_2)*tmp;
#endif
      }
    }
    pl_sInt32 XL1 = (X1+(1<<(XPOS_BITS-1)))>>XPOS_BITS;
    pl_sInt32 Xlen = ((X2+(1<<(XPOS_BITS-1)))>>XPOS_BITS) - XL1;
    if (Xlen > 0) { 
      pl_sInt32 iUL, iVL, idUL, idVL, iULnext, iVLnext;
      pl_Float UL = U1;
      pl_Float VL = V1;
#ifdef PL_PF_MULTITEX
      pl_sInt32 iUL_2, iVL_2, idUL_2, idVL_2, iULnext_2, iVLnext_2;
      pl_Float UL_2 = U1_2;
      pl_Float VL_2 = V1_2;
#endif
      pl_sInt32 CL[3] = {C1[0],C1[1], C1[2]};
      pl_Float pZL,ZL;
      pl_Float t = 1.0f / (pZL = ZL = Z1);     
      gmem += XL1;
      zbuf += XL1;

      XL1 += Xlen; // update to new line end pos so we can adjust gmem/zbuf later
      iULnext = ((pl_sInt32) (UL*t));
      iVLnext = ((pl_sInt32) (VL*t));
#ifdef PL_PF_MULTITEX
      iULnext_2 = ((pl_sInt32) (UL_2*t));
      iVLnext_2 = ((pl_sInt32) (VL_2*t));
#endif
      do {
        UL += dUL;
        VL += dVL;
        iUL = iULnext;
        iVL = iVLnext;
        pZL += pdZL;
        t = 1.0f/pZL;
        iULnext = ((pl_sInt32) (UL*t));
        iVLnext = ((pl_sInt32) (VL*t));
        idUL = (iULnext - iUL)>>nmb;
        idVL = (iVLnext - iVL)>>nmb;
        if (idUL>MappingU_Max) idUL=MappingU_Max;
        else if (idUL<-MappingU_Max) idUL=-MappingU_Max;
        if (idVL>MappingV_Max) idVL=MappingV_Max;
        else if (idVL<-MappingV_Max) idVL=-MappingV_Max;

        // todo: this is slow as shit, should we force textures to be powers of two? hehe
        if (iUL<0)  do iUL+=MappingU_Max; while (iUL<0);
        else while (iUL >= MappingU_Max) iUL-=MappingU_Max;
        if (iVL<0)  do iVL+=MappingV_Max; while (iVL<0);
        else while (iVL >= MappingV_Max) iVL-=MappingV_Max;

#ifdef PL_PF_MULTITEX
        UL_2 += dUL_2;
        VL_2 += dVL_2;
        iUL_2 = iULnext_2;
        iVL_2 = iVLnext_2;
        iULnext_2 = ((pl_sInt32) (UL_2*t));
        iVLnext_2 = ((pl_sInt32) (VL_2*t));
        idUL_2 = (iULnext_2 - iUL_2)>>nmb;
        idVL_2 = (iVLnext_2 - iVL_2)>>nmb;
        if (idUL_2>MappingU_Max_2) idUL_2=MappingU_Max_2;
        else if (idUL_2<-MappingU_Max_2) idUL_2=-MappingU_Max_2;
        if (idVL_2>MappingV_Max_2) idVL_2=MappingV_Max_2;
        else if (idVL_2<-MappingV_Max_2) idVL_2=-MappingV_Max_2;

        // todo: this is slow as shit, should we force textures to be powers of two? hehe
        if (iUL_2<0)  do iUL_2+=MappingU_Max_2; while (iUL_2<0);
        else while (iUL_2 >= MappingU_Max_2) iUL_2-=MappingU_Max_2;
        if (iVL_2<0)  do iVL_2+=MappingV_Max_2; while (iVL_2<0);
        else while (iVL_2 >= MappingV_Max_2) iVL_2-=MappingV_Max_2;

#endif

        pl_uInt n = nm;
        Xlen -= n;  
        if (Xlen < 0) n += Xlen;
        if (zfb_width) do {
            if (*zbuf < ZL) {
              *zbuf = (pl_ZBuffer) ZL;

 #ifdef PL_PF_MULTITEX
             TextureMakePixel2((LICE_pixel_chan *)gmem,solidcomb,solidalpha,solidalpha2,CL, bilinear, 
               iUL,iVL,tex_w,tex_h,texture,tex_rowspan,texcomb,texalpha,texalpha2,
                bilinear2, iUL_2,iVL_2,
                tex_w_2,tex_h_2,
                texture_2,tex_rowspan_2,tex2comb,tex2alpha,tex2alpha2);
 #else
            TextureMakePixel((LICE_pixel_chan *)gmem,solidcomb,solidalpha,solidalpha2,CL, bilinear, iUL,iVL,
              tex_w,tex_h,
              texture,tex_rowspan,texcomb,texalpha,texalpha2);
 #endif
 
            }
            zbuf++;
            gmem++;
            ZL += dZL;
            CL[0] += dCL[0];
            CL[1] += dCL[1];
            CL[2] += dCL[2];
            iUL += idUL;
            iVL += idVL;

            if (iUL<0) iUL+=MappingU_Max;
            else if (iUL >= MappingU_Max) iUL -= MappingU_Max;
            if (iVL<0) iVL+=MappingV_Max;
            else if (iVL >= MappingV_Max) iVL -= MappingV_Max;
#ifdef PL_PF_MULTITEX
            iUL_2 += idUL_2;
            iVL_2 += idVL_2;

            if (iUL_2<0) iUL_2+=MappingU_Max_2;
            else if (iUL_2 >= MappingU_Max_2) iUL_2 -= MappingU_Max_2;
            if (iVL_2<0) iVL_2+=MappingV_Max_2;
            else if (iVL_2 >= MappingV_Max_2) iVL_2 -= MappingV_Max_2;
#endif
          } while (--n);
        else do {

 #ifdef PL_PF_MULTITEX
             TextureMakePixel2((LICE_pixel_chan *)gmem,solidcomb,solidalpha,solidalpha2,CL, bilinear, 
               iUL,iVL,tex_w,tex_h,texture,tex_rowspan,texcomb,texalpha,texalpha2,
                bilinear2, iUL_2,iVL_2,
                tex_w_2,tex_h_2,
                texture_2,tex_rowspan_2,tex2comb,tex2alpha,tex2alpha2);
 #else
            TextureMakePixel((LICE_pixel_chan *)gmem,solidcomb,solidalpha,solidalpha2,CL, bilinear, iUL,iVL,
              tex_w,tex_h,
              texture,tex_rowspan,texcomb,texalpha,texalpha2);
 #endif


            gmem++;
            CL[0] += dCL[0];
            CL[1] += dCL[1];
            CL[2] += dCL[2];
            iUL += idUL;
            iVL += idVL;

            if (iUL<0) iUL+=MappingU_Max;
            else if (iUL >= MappingU_Max) iUL -= MappingU_Max;
            if (iVL<0) iVL+=MappingV_Max;
            else if (iVL >= MappingV_Max) iVL -= MappingV_Max;

#ifdef PL_PF_MULTITEX
            iUL_2 += idUL_2;
            iVL_2 += idVL_2;

            if (iUL_2<0) iUL_2+=MappingU_Max_2;
            else if (iUL_2 >= MappingU_Max_2) iUL_2 -= MappingU_Max_2;
            if (iVL_2<0) iVL_2+=MappingV_Max_2;
            else if (iVL_2 >= MappingV_Max_2) iVL_2 -= MappingV_Max_2;
#endif

          } while (--n);
      } while (Xlen > 0);
      gmem += swidth-XL1;
      zbuf += zfb_width-XL1;
    } else { // xlen <=0 ,no drawing
      gmem += swidth;
      zbuf += zfb_width;
    }
    Z1 += dZ1;
    U1 += dU1;
    V1 += dV1;
#ifdef PL_PF_MULTITEX
    U1_2 += dU1_2;
    V1_2 += dV1_2;
#endif
    X1 += dX1;
    X2 += dX2;
    C1[0] += dC1[0];
    C1[1] += dC1[1];
    C1[2] += dC1[2];
  }
}



