static bool IsAnsiEscComment(const wchar *Data,size_t Size);

bool Archive::GetComment(Array<wchar> *CmtData)
{
  if (!MainComment)
    return false;
  SaveFilePos SavePos(*this);

#ifndef SFX_MODULE
  ushort CmtLength;
  if (Format==RARFMT14)
  {
    Seek(SFXSize+SIZEOF_MAINHEAD14,SEEK_SET);
    CmtLength=GetByte();
    CmtLength+=(GetByte()<<8);
  }
  else
#endif
  {
    if (MainHead.CommentInHeader)
    {
      // Old style (RAR 2.9) archive comment embedded into the main 
      // archive header.
      Seek(SFXSize+SIZEOF_MARKHEAD3+SIZEOF_MAINHEAD3,SEEK_SET);
      ReadHeader();
    }
    else
    {
      // Current (RAR 3.0+) version of archive comment.
      Seek(GetStartPos(),SEEK_SET);
      return(SearchSubBlock(SUBHEAD_TYPE_CMT)!=0 && ReadCommentData(CmtData));
    }
#ifndef SFX_MODULE
    // Old style (RAR 2.9) comment header embedded into the main 
    // archive header.
    if (BrokenHeader)
    {
      Log(FileName,St(MLogCommHead));
      return false;
    }
    CmtLength=CommHead.HeadSize-SIZEOF_COMMHEAD;
#endif
  }
#ifndef SFX_MODULE
  if (Format==RARFMT14 && MainHead.PackComment || Format!=RARFMT14 && CommHead.Method!=0x30)
  {
    if (Format!=RARFMT14 && (CommHead.UnpVer < 15 || CommHead.UnpVer > VER_UNPACK || CommHead.Method > 0x35))
      return(false);
    ComprDataIO DataIO;
    DataIO.SetTestMode(true);
    uint UnpCmtLength;
    if (Format==RARFMT14)
    {
#ifdef RAR_NOCRYPT
      return(false);
#else
      UnpCmtLength=GetByte();
      UnpCmtLength+=(GetByte()<<8);
      CmtLength-=2;
      DataIO.SetCmt13Encryption();
      CommHead.UnpVer=15;
#endif
    }
    else
      UnpCmtLength=CommHead.UnpSize;
    DataIO.SetFiles(this,NULL);
    DataIO.EnableShowProgress(false);
    DataIO.SetPackedSizeToRead(CmtLength);
    DataIO.UnpHash.Init(HASH_CRC32,1);

    Unpack CmtUnpack(&DataIO);
    CmtUnpack.Init(0x10000,false);
    CmtUnpack.SetDestSize(UnpCmtLength);
    CmtUnpack.DoUnpack(CommHead.UnpVer,false);

    if (Format!=RARFMT14 && (DataIO.UnpHash.GetCRC32()&0xffff)!=CommHead.CommCRC)
    {
      Log(FileName,St(MLogCommBrk));
      return false;
    }
    else
    {
      byte *UnpData;
      size_t UnpDataSize;
      DataIO.GetUnpackedData(&UnpData,&UnpDataSize);
#ifdef _WIN_ALL
      OemToCharBuffA((char *)UnpData,(char *)UnpData,(DWORD)UnpDataSize);
#endif
      CmtData->Alloc(UnpDataSize+1);
      memset(CmtData->Addr(0),0,CmtData->Size()*sizeof(wchar));
      CharToWide((char *)UnpData,CmtData->Addr(0),UnpDataSize);
      CmtData->Alloc(wcslen(CmtData->Addr(0)));
    }
  }
  else
  {
    Array<byte> CmtRaw(CmtLength);
    Read(&CmtRaw[0],CmtLength);

    if (Format!=RARFMT14 && CommHead.CommCRC!=(~CRC32(0xffffffff,&CmtRaw[0],CmtLength)&0xffff))
    {
      Log(FileName,St(MLogCommBrk));
      return false;
    }
    CmtData->Alloc(CmtLength+1);
    CmtRaw.Push(0);
#ifdef _WIN_ALL
    OemToCharA((char *)&CmtRaw[0],(char *)&CmtRaw[0]);
#endif
    CharToWide((char *)&CmtRaw[0],CmtData->Addr(0),CmtLength);
    CmtData->Alloc(wcslen(CmtData->Addr(0)));
  }
#endif
  return CmtData->Size() > 0;
}


bool Archive::ReadCommentData(Array<wchar> *CmtData)
{
  Array<byte> CmtRaw;
  if (!ReadSubData(&CmtRaw,NULL))
    return false;
  size_t CmtSize=CmtRaw.Size();
  CmtRaw.Push(0);
  CmtData->Alloc(CmtSize+1);
  if (Format==RARFMT50)
    UtfToWide((char *)&CmtRaw[0],CmtData->Addr(0),CmtData->Size());
  else
    if ((SubHead.SubFlags & SUBHEAD_FLAGS_CMT_UNICODE)!=0)
    {
      RawToWide(&CmtRaw[0],CmtData->Addr(0),CmtSize/2);
      (*CmtData)[CmtSize/2]=0;

    }
    else
    {
      CharToWide((char *)&CmtRaw[0],CmtData->Addr(0),CmtData->Size());
    }
  CmtData->Alloc(wcslen(CmtData->Addr(0))); // Set buffer size to actual comment length.
  return true;
}


void Archive::ViewComment()
{
#ifndef GUI
  if (Cmd->DisableComment)
    return;
  Array<wchar> CmtBuf;
  if (GetComment(&CmtBuf))
  {
    size_t CmtSize=CmtBuf.Size();
    wchar *ChPtr=wcschr(&CmtBuf[0],0x1A);
    if (ChPtr!=NULL)
      CmtSize=ChPtr-&CmtBuf[0];
    mprintf(L"\n");
    OutComment(&CmtBuf[0],CmtSize);
  }
#endif
}


