

#if !defined(SFX_MODULE) && defined(_WIN_ALL)
void ExtractStreams20(Archive &Arc,const wchar *FileName)
{
  if (Arc.BrokenHeader)
  {
#ifndef SILENT
    Log(Arc.FileName,St(MStreamBroken),FileName);
#endif
    ErrHandler.SetErrorCode(RARX_CRC);
    return;
  }

  if (Arc.StreamHead.Method<0x31 || Arc.StreamHead.Method>0x35 || Arc.StreamHead.UnpVer>VER_PACK)
  {
#ifndef SILENT
    Log(Arc.FileName,St(MStreamUnknown),FileName);
#endif
    ErrHandler.SetErrorCode(RARX_WARNING);
    return;
  }

  wchar StreamName[NM+2];
  if (FileName[0]!=0 && FileName[1]==0)
  {
    wcscpy(StreamName,L".\\");
    wcscpy(StreamName+2,FileName);
  }
  else
    wcscpy(StreamName,FileName);
  if (wcslen(StreamName)+strlen(Arc.StreamHead.StreamName)>=ASIZE(StreamName) ||
      Arc.StreamHead.StreamName[0]!=':')
  {
#ifndef SILENT
    Log(Arc.FileName,St(MStreamBroken),FileName);
#endif
    ErrHandler.SetErrorCode(RARX_CRC);
    return;
  }

  wchar StoredName[NM];
  CharToWide(Arc.StreamHead.StreamName,StoredName,ASIZE(StoredName));
  ConvertPath(StoredName+1,StoredName+1);

  wcsncatz(StreamName,StoredName,ASIZE(StreamName));

  FindData fd;
  bool Found=FindFile::FastFind(FileName,&fd);

  if ((fd.FileAttr & FILE_ATTRIBUTE_READONLY)!=0)
    SetFileAttr(FileName,fd.FileAttr & ~FILE_ATTRIBUTE_READONLY);

  File CurFile;
  if (CurFile.WCreate(StreamName))
  {
    ComprDataIO DataIO;
    Unpack Unpack(&DataIO);
    Unpack.Init(0x10000,false);

    DataIO.SetPackedSizeToRead(Arc.StreamHead.DataSize);
    DataIO.EnableShowProgress(false);
    DataIO.SetFiles(&Arc,&CurFile);
    DataIO.UnpHash.Init(HASH_CRC32,1);
    Unpack.SetDestSize(Arc.StreamHead.UnpSize);
    Unpack.DoUnpack(Arc.StreamHead.UnpVer,false);

    if (Arc.StreamHead.StreamCRC!=DataIO.UnpHash.GetCRC32())
    {
#ifndef SILENT
      Log(Arc.FileName,St(MStreamBroken),StreamName);
#endif
      ErrHandler.SetErrorCode(RARX_CRC);
    }
    else
      CurFile.Close();
  }
  File HostFile;
  if (Found && HostFile.Open(FileName,FMF_OPENSHARED|FMF_UPDATE))
    SetFileTime(HostFile.GetHandle(),&fd.ftCreationTime,&fd.ftLastAccessTime,
                &fd.ftLastWriteTime);
  if ((fd.FileAttr & FILE_ATTRIBUTE_READONLY)!=0)
    SetFileAttr(FileName,fd.FileAttr);
}
#endif


#ifdef _WIN_ALL
void ExtractStreams(Archive &Arc,const wchar *FileName)
{
  wchar FullName[NM+2];
  if (FileName[0]!=0 && FileName[1]==0)
  {
    wcscpy(FullName,L".\\");
    wcsncpyz(FullName+2,FileName,ASIZE(FullName)-2);
  }
  else
    wcsncpyz(FullName,FileName,ASIZE(FullName));

  byte *Data=&Arc.SubHead.SubData[0];
  size_t DataSize=Arc.SubHead.SubData.Size();

  wchar StreamName[NM];
  GetStreamNameNTFS(Arc,StreamName,ASIZE(StreamName));
  if (*StreamName!=':')
  {
#if !defined(SILENT) && !defined(SFX_MODULE)
    Log(Arc.FileName,St(MStreamBroken),FileName);
#endif
    ErrHandler.SetErrorCode(RARX_CRC);
    return;
  }

  wcsncatz(FullName,StreamName,ASIZE(FullName));

  FindData fd;
  bool Found=FindFile::FastFind(FileName,&fd);

  if ((fd.FileAttr & FILE_ATTRIBUTE_READONLY)!=0)
    SetFileAttr(FileName,fd.FileAttr & ~FILE_ATTRIBUTE_READONLY);
  File CurFile;
  if (CurFile.WCreate(FullName) && Arc.ReadSubData(NULL,&CurFile))
    CurFile.Close();
  File HostFile;
  if (Found && HostFile.Open(FileName,FMF_OPENSHARED|FMF_UPDATE))
    SetFileTime(HostFile.GetHandle(),&fd.ftCreationTime,&fd.ftLastAccessTime,
                &fd.ftLastWriteTime);

  // Restoring original file attributes. Important if file was read only
  // or did not have "Archive" attribute
  SetFileAttr(FileName,fd.FileAttr);
}
#endif


void GetStreamNameNTFS(Archive &Arc,wchar *StreamName,size_t MaxSize)
{
  byte *Data=&Arc.SubHead.SubData[0];
  size_t DataSize=Arc.SubHead.SubData.Size();
  if (Arc.Format==RARFMT15)
  {
    size_t DestSize=Min(DataSize/2,MaxSize-1);
    RawToWide(Data,StreamName,DestSize);
    StreamName[DestSize]=0;
  }
  else
  {
    char UtfString[NM*4];
    size_t DestSize=Min(DataSize,ASIZE(UtfString)-1);
    memcpy(UtfString,Data,DestSize);
    UtfString[DestSize]=0;
    UtfToWide(UtfString,StreamName,MaxSize);
  }
}
