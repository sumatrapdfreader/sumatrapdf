#define SYMLINK_FLAG_RELATIVE 1

typedef struct _REPARSE_DATA_BUFFER {
  ULONG  ReparseTag;
  USHORT ReparseDataLength;
  USHORT Reserved;
  union {
    struct {
      USHORT SubstituteNameOffset;
      USHORT SubstituteNameLength;
      USHORT PrintNameOffset;
      USHORT PrintNameLength;
      ULONG  Flags;
      WCHAR  PathBuffer[1];
    } SymbolicLinkReparseBuffer;
    struct {
      USHORT SubstituteNameOffset;
      USHORT SubstituteNameLength;
      USHORT PrintNameOffset;
      USHORT PrintNameLength;
      WCHAR  PathBuffer[1];
    } MountPointReparseBuffer;
    struct {
      UCHAR DataBuffer[1];
    } GenericReparseBuffer;
  };
} REPARSE_DATA_BUFFER, *PREPARSE_DATA_BUFFER;


void GetReparsePoint(const wchar *Name,FileHeader *hd)
{
  static bool PrivSet=false;
  if (!PrivSet)
  {
    SetPrivilege(SE_BACKUP_NAME);
    PrivSet=true;
  }

  WIN32_FIND_DATA FindData;
  HANDLE hFind=FindFirstFile(Name,&FindData);
  if (hFind==INVALID_HANDLE_VALUE)
    return;
  FindClose(hFind);

  if ((FindData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)==0 ||
      FindData.dwReserved0!=IO_REPARSE_TAG_MOUNT_POINT &&
      FindData.dwReserved0!=IO_REPARSE_TAG_SYMLINK)
    return;

  HANDLE hFile=CreateFile(
    Name,FILE_READ_EA,FILE_SHARE_READ,NULL,OPEN_EXISTING,
    FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL);
  if (hFile==INVALID_HANDLE_VALUE)
  {
    ErrHandler.OpenErrorMsg(NULL,Name);
    ErrHandler.SetErrorCode(RARX_OPEN);
    return;
  }

  const DWORD BufSize=MAXIMUM_REPARSE_DATA_BUFFER_SIZE;
  Array<byte> Buf(BufSize);
  REPARSE_DATA_BUFFER *rdb=(REPARSE_DATA_BUFFER *)&Buf[0];

  DWORD BytesReturned;
  BOOL DevResult=DeviceIoControl(hFile,FSCTL_GET_REPARSE_POINT,NULL,0,rdb,BufSize,&BytesReturned,NULL);
  CloseHandle(hFile);
  if (!DevResult)
  {
    ErrHandler.ReadErrorMsg(Name);
    return;
  }

  wchar TargetName[NM];
  if (rdb->ReparseTag==IO_REPARSE_TAG_MOUNT_POINT)
  {
    uint SubstOffset=rdb->MountPointReparseBuffer.SubstituteNameOffset/sizeof(WCHAR);
    uint SubstLength=rdb->MountPointReparseBuffer.SubstituteNameLength/sizeof(WCHAR);
    wchar *SubstName=rdb->MountPointReparseBuffer.PathBuffer+SubstOffset;

    if (SubstLength>=ASIZE(TargetName))
      return;
    wcsncpy(TargetName,SubstName,SubstLength);
    TargetName[SubstLength]=0;
    hd->RedirType=FSREDIR_JUNCTION;
  }
  if (rdb->ReparseTag==IO_REPARSE_TAG_SYMLINK)
  {
    uint SubstOffset=rdb->SymbolicLinkReparseBuffer.SubstituteNameOffset/sizeof(WCHAR);
    uint SubstLength=rdb->SymbolicLinkReparseBuffer.SubstituteNameLength/sizeof(WCHAR);
    wchar *SubstName=rdb->SymbolicLinkReparseBuffer.PathBuffer+SubstOffset;

    if (SubstLength>=ASIZE(TargetName))
      return;
    wcsncpy(TargetName,SubstName,SubstLength);
    TargetName[SubstLength]=0;
    hd->RedirType=FSREDIR_WINSYMLINK;
  }
  if (hd->RedirType==FSREDIR_NONE)
    return;
  wcsncpyz(hd->RedirName,TargetName,ASIZE(hd->RedirName));
  hd->DirTarget=(FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)!=0;
}


bool CreateReparsePoint(CommandData *Cmd,const wchar *Name,FileHeader *hd)
{
  static bool PrivSet=false;
  if (!PrivSet)
  {
    SetPrivilege(SE_RESTORE_NAME);
    // Not sure if we really need it, but let's request anyway.
    SetPrivilege(SE_CREATE_SYMBOLIC_LINK_NAME);
    PrivSet=true;
  }

  CreatePath(Name,true);

  // 'DirTarget' check is important for Unix symlinks to directories.
  // Unix symlinks do not have their own 'directory' attribute.
  if (hd->Dir || hd->DirTarget)
  {
    if (!CreateDirectory(Name,NULL))
      return false;
  }
  else
  {
    HANDLE hFile=CreateFile(Name,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if (hFile == INVALID_HANDLE_VALUE)
      return false;
    CloseHandle(hFile);
  }

  const DWORD BufSize=sizeof(REPARSE_DATA_BUFFER)+2*NM+1024;
  Array<byte> Buf(BufSize);
  REPARSE_DATA_BUFFER *rdb=(REPARSE_DATA_BUFFER *)&Buf[0];

  wchar SubstName[NM];
  wcsncpyz(SubstName,hd->RedirName,ASIZE(SubstName));
  size_t SubstLength=wcslen(SubstName);

  wchar PrintName[NM],*PrintNameSrc=SubstName,*PrintNameDst=PrintName;
  bool WinPrefix=wcsncmp(PrintNameSrc,L"\\??\\",4)==0;
  if (WinPrefix)
    PrintNameSrc+=4;
  if (WinPrefix && wcsncmp(PrintNameSrc,L"UNC\\",4)==0)
  {
    *(PrintNameDst++)='\\'; // Insert second \ in beginning of share name.
    PrintNameSrc+=3;
  }
  wcscpy(PrintNameDst,PrintNameSrc);

  size_t PrintLength=wcslen(PrintName);

  bool AbsPath=WinPrefix;

  if (hd->RedirType==FSREDIR_JUNCTION)
  {
    rdb->ReparseTag=IO_REPARSE_TAG_MOUNT_POINT;
    rdb->ReparseDataLength=USHORT(
      sizeof(rdb->MountPointReparseBuffer.SubstituteNameOffset)+
      sizeof(rdb->MountPointReparseBuffer.SubstituteNameLength)+
      sizeof(rdb->MountPointReparseBuffer.PrintNameOffset)+
      sizeof(rdb->MountPointReparseBuffer.PrintNameLength)+
      (SubstLength+1)*sizeof(WCHAR)+(PrintLength+1)*sizeof(WCHAR));
    rdb->Reserved=0;

    rdb->MountPointReparseBuffer.SubstituteNameOffset=0;
    rdb->MountPointReparseBuffer.SubstituteNameLength=USHORT(SubstLength*sizeof(WCHAR));
    wcscpy(rdb->MountPointReparseBuffer.PathBuffer,SubstName);

    rdb->MountPointReparseBuffer.PrintNameOffset=USHORT((SubstLength+1)*sizeof(WCHAR));
    rdb->MountPointReparseBuffer.PrintNameLength=USHORT(PrintLength*sizeof(WCHAR));
    wcscpy(rdb->MountPointReparseBuffer.PathBuffer+SubstLength+1,PrintName);
  }
  else
    if (hd->RedirType==FSREDIR_WINSYMLINK || hd->RedirType==FSREDIR_UNIXSYMLINK)
    {
      rdb->ReparseTag=IO_REPARSE_TAG_SYMLINK;
      rdb->ReparseDataLength=USHORT(
        sizeof(rdb->SymbolicLinkReparseBuffer.SubstituteNameOffset)+
        sizeof(rdb->SymbolicLinkReparseBuffer.SubstituteNameLength)+
        sizeof(rdb->SymbolicLinkReparseBuffer.PrintNameOffset)+
        sizeof(rdb->SymbolicLinkReparseBuffer.PrintNameLength)+
        sizeof(rdb->SymbolicLinkReparseBuffer.Flags)+
        (SubstLength+1)*sizeof(WCHAR)+(PrintLength+1)*sizeof(WCHAR));
      rdb->Reserved=0;

      rdb->SymbolicLinkReparseBuffer.SubstituteNameOffset=0;
      rdb->SymbolicLinkReparseBuffer.SubstituteNameLength=USHORT(SubstLength*sizeof(WCHAR));
      wcscpy(rdb->SymbolicLinkReparseBuffer.PathBuffer,SubstName);

      rdb->SymbolicLinkReparseBuffer.PrintNameOffset=USHORT((SubstLength+1)*sizeof(WCHAR));
      rdb->SymbolicLinkReparseBuffer.PrintNameLength=USHORT(PrintLength*sizeof(WCHAR));
      wcscpy(rdb->SymbolicLinkReparseBuffer.PathBuffer+SubstLength+1,PrintName);

      rdb->SymbolicLinkReparseBuffer.Flags=AbsPath ? 0:SYMLINK_FLAG_RELATIVE;
    }
    else
      return false;

  HANDLE hFile=CreateFile(Name,GENERIC_READ|GENERIC_WRITE,0,NULL,
               OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT| 
               FILE_FLAG_BACKUP_SEMANTICS,NULL);
  if (hFile==INVALID_HANDLE_VALUE)
    return false;

  DWORD Returned;
  if (!DeviceIoControl(hFile,FSCTL_SET_REPARSE_POINT,rdb, 
      FIELD_OFFSET(REPARSE_DATA_BUFFER,GenericReparseBuffer)+
      rdb->ReparseDataLength,NULL,0,&Returned,NULL))
  { 
    CloseHandle(hFile);
    Log(NULL,St(MErrCreateLnkS),Name);
    if (GetLastError()==ERROR_PRIVILEGE_NOT_HELD)
      Log(NULL,St(MNeedAdmin));
    ErrHandler.SysErrMsg();
    ErrHandler.SetErrorCode(RARX_CREATE);

    if (hd->Dir)
      RemoveDirectory(Name);
    else
      DeleteFile(Name);
    return false;
  }
  File LinkFile;
  LinkFile.SetHandle(hFile);
  LinkFile.SetOpenFileTime(
    Cmd->xmtime==EXTTIME_NONE ? NULL:&hd->mtime,
    Cmd->xctime==EXTTIME_NONE ? NULL:&hd->ctime,
    Cmd->xatime==EXTTIME_NONE ? NULL:&hd->atime);
  LinkFile.Close();
  if (!Cmd->IgnoreGeneralAttr)
    SetFileAttr(Name,hd->FileAttr);
  return true;
}
