struct CryptKeyCacheItem
{
  CryptKeyCacheItem()
  {
    Password.Set(L"");
  }

  ~CryptKeyCacheItem()
  {
    cleandata(AESKey,sizeof(AESKey));
    cleandata(AESInit,sizeof(AESInit));
    cleandata(&Password,sizeof(Password));
  }

  byte AESKey[16],AESInit[16];
  SecPassword Password;
  bool SaltPresent;
  byte Salt[SIZE_SALT30];
};

static CryptKeyCacheItem Cache[4];
static int CachePos=0;

void CryptData::SetKey30(bool Encrypt,SecPassword *Password,const wchar *PwdW,const byte *Salt)
{
  byte AESKey[16],AESInit[16];

  bool Cached=false;
  for (uint I=0;I<ASIZE(Cache);I++)
    if (Cache[I].Password==*Password &&
        (Salt==NULL && !Cache[I].SaltPresent || Salt!=NULL &&
        Cache[I].SaltPresent && memcmp(Cache[I].Salt,Salt,SIZE_SALT30)==0))
    {
      memcpy(AESKey,Cache[I].AESKey,sizeof(AESKey));
      memcpy(AESInit,Cache[I].AESInit,sizeof(AESInit));
      Cached=true;
      break;
    }

  if (!Cached)
  {
    byte RawPsw[2*MAXPASSWORD+SIZE_SALT30];
    WideToRaw(PwdW,RawPsw,ASIZE(RawPsw));
    size_t RawLength=2*wcslen(PwdW);
    if (Salt!=NULL)
    {
      memcpy(RawPsw+RawLength,Salt,SIZE_SALT30);
      RawLength+=SIZE_SALT30;
    }
    hash_context c;
    hash_initial(&c);

    const int HashRounds=0x40000;
    for (int I=0;I<HashRounds;I++)
    {
      hash_process( &c, RawPsw, RawLength, false);
      byte PswNum[3];
      PswNum[0]=(byte)I;
      PswNum[1]=(byte)(I>>8);
      PswNum[2]=(byte)(I>>16);
      hash_process( &c, PswNum, 3, false);
      if (I%(HashRounds/16)==0)
      {
        hash_context tempc=c;
        uint32 digest[5];
        hash_final( &tempc, digest, false);
        AESInit[I/(HashRounds/16)]=(byte)digest[4];
      }
    }
    uint32 digest[5];
    hash_final( &c, digest, false);
    for (int I=0;I<4;I++)
      for (int J=0;J<4;J++)
        AESKey[I*4+J]=(byte)(digest[I]>>(J*8));

    Cache[CachePos].Password=*Password;
    if ((Cache[CachePos].SaltPresent=(Salt!=NULL))==true)
      memcpy(Cache[CachePos].Salt,Salt,SIZE_SALT30);
    memcpy(Cache[CachePos].AESKey,AESKey,sizeof(AESKey));
    memcpy(Cache[CachePos].AESInit,AESInit,sizeof(AESInit));
    CachePos=(CachePos+1)%ASIZE(Cache);

    cleandata(RawPsw,sizeof(RawPsw));
  }
  rin.Init(Encrypt, AESKey, 128, AESInit);
  cleandata(AESKey,sizeof(AESKey));
  cleandata(AESInit,sizeof(AESInit));
}

