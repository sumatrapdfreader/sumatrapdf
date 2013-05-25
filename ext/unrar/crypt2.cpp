#define NROUNDS 32

#define substLong(t) ( (uint)SubstTable20[(uint)t&255] | \
           ((uint)SubstTable20[(int)(t>> 8)&255]<< 8) | \
           ((uint)SubstTable20[(int)(t>>16)&255]<<16) | \
           ((uint)SubstTable20[(int)(t>>24)&255]<<24) )


static byte InitSubstTable20[256]={
  215, 19,149, 35, 73,197,192,205,249, 28, 16,119, 48,221,  2, 42,
  232,  1,177,233, 14, 88,219, 25,223,195,244, 90, 87,239,153,137,
  255,199,147, 70, 92, 66,246, 13,216, 40, 62, 29,217,230, 86,  6,
   71, 24,171,196,101,113,218,123, 93, 91,163,178,202, 67, 44,235,
  107,250, 75,234, 49,167,125,211, 83,114,157,144, 32,193,143, 36,
  158,124,247,187, 89,214,141, 47,121,228, 61,130,213,194,174,251,
   97,110, 54,229,115, 57,152, 94,105,243,212, 55,209,245, 63, 11,
  164,200, 31,156, 81,176,227, 21, 76, 99,139,188,127, 17,248, 51,
  207,120,189,210,  8,226, 41, 72,183,203,135,165,166, 60, 98,  7,
  122, 38,155,170, 69,172,252,238, 39,134, 59,128,236, 27,240, 80,
  131,  3, 85,206,145, 79,154,142,159,220,201,133, 74, 64, 20,129,
  224,185,138,103,173,182, 43, 34,254, 82,198,151,231,180, 58, 10,
  118, 26,102, 12, 50,132, 22,191,136,111,162,179, 45,  4,148,108,
  161, 56, 78,126,242,222, 15,175,146, 23, 33,241,181,190, 77,225,
    0, 46,169,186, 68, 95,237, 65, 53,208,253,168,  9, 18,100, 52,
  116,184,160, 96,109, 37, 30,106,140,104,150,  5,204,117,112, 84
};


void CryptData::SetKey20(const char *Password)
{
  InitCRC32(CRCTab);

  char Psw[MAXPASSWORD];
  strncpyz(Psw,Password,ASIZE(Psw)); // We'll need to modify it below.
  size_t PswLength=strlen(Psw);

  Key20[0]=0xD3A3B879L;
  Key20[1]=0x3F6D12F7L;
  Key20[2]=0x7515A235L;
  Key20[3]=0xA4E7F123L;

  memcpy(SubstTable20,InitSubstTable20,sizeof(SubstTable20));
  for (int J=0;J<256;J++)
    for (size_t I=0;I<PswLength;I+=2)
    {
      uint N1=(byte)CRCTab [ (byte(Password[I])   - J) &0xff];
      uint N2=(byte)CRCTab [ (byte(Password[I+1]) + J) &0xff];
      for (int K=1;N1!=N2;N1=(N1+1)&0xff,K++)
        Swap20(&SubstTable20[N1],&SubstTable20[(N1+I+K)&0xff]);
    }
  
  // Incomplete last block of password must be zero padded.
  if ((PswLength & CRYPT_BLOCK_MASK)!=0)
    for (size_t I=PswLength;I<=(PswLength|CRYPT_BLOCK_MASK);I++)
       Psw[I]=0;

  for (size_t I=0;I<PswLength;I+=CRYPT_BLOCK_SIZE)
    EncryptBlock20((byte *)Psw+I);
}


void CryptData::EncryptBlock20(byte *Buf)
{
  uint A,B,C,D,T,TA,TB;
#if defined(BIG_ENDIAN) || !defined(PRESENT_INT32) || !defined(ALLOW_NOT_ALIGNED_INT)
  A=((uint)Buf[0]|((uint)Buf[1]<<8)|((uint)Buf[2]<<16)|((uint)Buf[3]<<24))^Key20[0];
  B=((uint)Buf[4]|((uint)Buf[5]<<8)|((uint)Buf[6]<<16)|((uint)Buf[7]<<24))^Key20[1];
  C=((uint)Buf[8]|((uint)Buf[9]<<8)|((uint)Buf[10]<<16)|((uint)Buf[11]<<24))^Key20[2];
  D=((uint)Buf[12]|((uint)Buf[13]<<8)|((uint)Buf[14]<<16)|((uint)Buf[15]<<24))^Key20[3];
#else
  uint32 *BufPtr=(uint32 *)Buf;
  A=BufPtr[0]^Key20[0];
  B=BufPtr[1]^Key20[1];
  C=BufPtr[2]^Key20[2];
  D=BufPtr[3]^Key20[3];
#endif
  for(int I=0;I<NROUNDS;I++)
  {
    T=((C+rol(D,11,32))^Key20[I&3]);
    TA=A^substLong(T);
    T=((D^rol(C,17,32))+Key20[I&3]);
    TB=B^substLong(T);
    A=C;
    B=D;
    C=TA;
    D=TB;
  }
#if defined(BIG_ENDIAN) || !defined(PRESENT_INT32) || !defined(ALLOW_NOT_ALIGNED_INT)
  C^=Key20[0];
  Buf[0]=(byte)C;
  Buf[1]=(byte)(C>>8);
  Buf[2]=(byte)(C>>16);
  Buf[3]=(byte)(C>>24);
  D^=Key20[1];
  Buf[4]=(byte)D;
  Buf[5]=(byte)(D>>8);
  Buf[6]=(byte)(D>>16);
  Buf[7]=(byte)(D>>24);
  A^=Key20[2];
  Buf[8]=(byte)A;
  Buf[9]=(byte)(A>>8);
  Buf[10]=(byte)(A>>16);
  Buf[11]=(byte)(A>>24);
  B^=Key20[3];
  Buf[12]=(byte)B;
  Buf[13]=(byte)(B>>8);
  Buf[14]=(byte)(B>>16);
  Buf[15]=(byte)(B>>24);
#else
  BufPtr[0]=C^Key20[0];
  BufPtr[1]=D^Key20[1];
  BufPtr[2]=A^Key20[2];
  BufPtr[3]=B^Key20[3];
#endif
  UpdKeys20(Buf);
}


void CryptData::DecryptBlock20(byte *Buf)
{
  byte InBuf[16];
  uint A,B,C,D,T,TA,TB;
#if defined(BIG_ENDIAN) || !defined(PRESENT_INT32) || !defined(ALLOW_NOT_ALIGNED_INT)
  A=((uint)Buf[0]|((uint)Buf[1]<<8)|((uint)Buf[2]<<16)|((uint)Buf[3]<<24))^Key20[0];
  B=((uint)Buf[4]|((uint)Buf[5]<<8)|((uint)Buf[6]<<16)|((uint)Buf[7]<<24))^Key20[1];
  C=((uint)Buf[8]|((uint)Buf[9]<<8)|((uint)Buf[10]<<16)|((uint)Buf[11]<<24))^Key20[2];
  D=((uint)Buf[12]|((uint)Buf[13]<<8)|((uint)Buf[14]<<16)|((uint)Buf[15]<<24))^Key20[3];
#else
  uint32 *BufPtr=(uint32 *)Buf;
  A=BufPtr[0]^Key20[0];
  B=BufPtr[1]^Key20[1];
  C=BufPtr[2]^Key20[2];
  D=BufPtr[3]^Key20[3];
#endif
  memcpy(InBuf,Buf,sizeof(InBuf));
  for(int I=NROUNDS-1;I>=0;I--)
  {
    T=((C+rol(D,11,32))^Key20[I&3]);
    TA=A^substLong(T);
    T=((D^rol(C,17,32))+Key20[I&3]);
    TB=B^substLong(T);
    A=C;
    B=D;
    C=TA;
    D=TB;
  }
#if defined(BIG_ENDIAN) || !defined(PRESENT_INT32) || !defined(ALLOW_NOT_ALIGNED_INT)
  C^=Key20[0];
  Buf[0]=(byte)C;
  Buf[1]=(byte)(C>>8);
  Buf[2]=(byte)(C>>16);
  Buf[3]=(byte)(C>>24);
  D^=Key20[1];
  Buf[4]=(byte)D;
  Buf[5]=(byte)(D>>8);
  Buf[6]=(byte)(D>>16);
  Buf[7]=(byte)(D>>24);
  A^=Key20[2];
  Buf[8]=(byte)A;
  Buf[9]=(byte)(A>>8);
  Buf[10]=(byte)(A>>16);
  Buf[11]=(byte)(A>>24);
  B^=Key20[3];
  Buf[12]=(byte)B;
  Buf[13]=(byte)(B>>8);
  Buf[14]=(byte)(B>>16);
  Buf[15]=(byte)(B>>24);
#else
  BufPtr[0]=C^Key20[0];
  BufPtr[1]=D^Key20[1];
  BufPtr[2]=A^Key20[2];
  BufPtr[3]=B^Key20[3];
#endif
  UpdKeys20(InBuf);
}


void CryptData::UpdKeys20(byte *Buf)
{
  for (int I=0;I<16;I+=4)
  {
    Key20[0]^=CRCTab[Buf[I]];
    Key20[1]^=CRCTab[Buf[I+1]];
    Key20[2]^=CRCTab[Buf[I+2]];
    Key20[3]^=CRCTab[Buf[I+3]];
  }
}


void CryptData::Swap20(byte *Ch1,byte *Ch2)
{
  byte Ch=*Ch1;
  *Ch1=*Ch2;
  *Ch2=Ch;
}
