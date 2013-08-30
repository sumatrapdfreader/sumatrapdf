

static bool UnixSymlink(const char *Target,const wchar *LinkName)
{
  CreatePath(LinkName,true);
  DelFile(LinkName);
  char LinkNameA[NM];
  WideToChar(LinkName,LinkNameA,ASIZE(LinkNameA));
  if (symlink(Target,LinkNameA)==-1) // Error.
  {
    if (errno==EEXIST)
      Log(NULL,St(MSymLinkExists),LinkName);
    else
    {
      Log(NULL,St(MErrCreateLnkS),LinkName);
      ErrHandler.SetErrorCode(RARX_WARNING);
    }
    return false;
  }
  // We do not set time of created symlink, because utime changes
  // time of link target and lutimes is not available on all Linux
  // systems at the moment of writing this code.
  return true;
}


bool ExtractUnixLink30(ComprDataIO &DataIO,Archive &Arc,const wchar *LinkName)
{
  char Target[NM];
  if (IsLink(Arc.FileHead.FileAttr))
  {
    size_t DataSize=Min(Arc.FileHead.PackSize,ASIZE(Target)-1);
    DataIO.UnpRead((byte *)Target,DataSize);
    Target[DataSize]=0;

    DataIO.UnpHash.Init(Arc.FileHead.FileHash.Type,1);
    DataIO.UnpHash.Update(Target,strlen(Target));
    DataIO.UnpHash.Result(&Arc.FileHead.FileHash);

    // Return true in case of bad checksum, so link will be processed further
    // and extraction routine will report the checksum error.
    if (!DataIO.UnpHash.Cmp(&Arc.FileHead.FileHash,Arc.FileHead.UseHashKey ? Arc.FileHead.HashKey:NULL))
      return true;

    return UnixSymlink(Target,LinkName);
  }
  return false;
}


bool ExtractUnixLink50(const wchar *Name,FileHeader *hd)
{
  char Target[NM];
  WideToChar(hd->RedirName,Target,ASIZE(Target));
  if (hd->RedirType==FSREDIR_WINSYMLINK || hd->RedirType==FSREDIR_JUNCTION)
  {
    // Cannot create Windows absolute path symlinks in Unix. Only relative path
    // Windows symlinks can be created here.
    if (strncmp(Target,"\\??\\",4)==0)
      return false;
    DosSlashToUnix(Target,Target,ASIZE(Target));
  }
  return UnixSymlink(Target,Name);
}
