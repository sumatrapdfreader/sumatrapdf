bool ExtractHardlink(wchar *NameNew,wchar *NameExisting,size_t NameExistingSize)
{
  if (!FileExist(NameExisting))
    return false;
  CreatePath(NameNew,true);

#ifdef _WIN_ALL
  UnixSlashToDos(NameExisting,NameExisting,NameExistingSize);

  bool Success=CreateHardLink(NameNew,NameExisting,NULL)!=0;
  if (!Success)
  {
    Log(NULL,St(MErrCreateLnkH),NameNew);
    ErrHandler.SysErrMsg();
    ErrHandler.SetErrorCode(RARX_CREATE);
  }
  return Success;
#elif defined(_UNIX)
  DosSlashToUnix(NameExisting,NameExisting,NameExistingSize);

  char NameExistingA[NM],NameNewA[NM];
  WideToChar(NameExisting,NameExistingA,ASIZE(NameExistingA));
  WideToChar(NameNew,NameNewA,ASIZE(NameNewA));
  bool Success=link(NameExistingA,NameNewA)==0;
  if (!Success)
  {
    Log(NULL,St(MErrCreateLnkH),NameNew);
    ErrHandler.SysErrMsg();
    ErrHandler.SetErrorCode(RARX_CREATE);
  }
  return Success;
#else
  return false;
#endif
}

