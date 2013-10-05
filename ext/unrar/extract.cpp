#include "rar.hpp"

CmdExtract::CmdExtract(CommandData *Cmd)
{
  *ArcName=0;

  *DestFileName=0;

  TotalFileCount=0;
  Password.Set(L"");
  Unp=new Unpack(&DataIO);
#ifdef RAR_SMP
  Unp->SetThreads(Cmd->Threads);
#endif
}


CmdExtract::~CmdExtract()
{
  delete Unp;
}


void CmdExtract::DoExtract(CommandData *Cmd)
{
  PasswordCancelled=false;
  DataIO.SetCurrentCommand(Cmd->Command[0]);

  FindData FD;
  while (Cmd->GetArcName(ArcName,ASIZE(ArcName)))
    if (FindFile::FastFind(ArcName,&FD))
      DataIO.TotalArcSize+=FD.Size;

  Cmd->ArcNames.Rewind();
  while (Cmd->GetArcName(ArcName,ASIZE(ArcName)))
  {
    while (true)
    {
      SecPassword PrevCmdPassword;
      PrevCmdPassword=Cmd->Password;

      EXTRACT_ARC_CODE Code=ExtractArchive(Cmd);

      // Restore Cmd->Password, which could be changed in IsArchive() call
      // for next header encrypted archive.
      Cmd->Password=PrevCmdPassword;

      if (Code!=EXTRACT_ARC_REPEAT)
        break;
    }
    if (FindFile::FastFind(ArcName,&FD))
      DataIO.ProcessedArcSize+=FD.Size;
  }

  if (TotalFileCount==0 && Cmd->Command[0]!='I' && 
      ErrHandler.GetErrorCode()!=RARX_BADPWD) // Not in case of wrong archive password.
  {
    if (!PasswordCancelled)
    {
      mprintf(St(MExtrNoFiles));
    }
    ErrHandler.SetErrorCode(RARX_NOFILES);
  }
#ifndef GUI
  else
    if (!Cmd->DisableDone)
      if (Cmd->Command[0]=='I')
        mprintf(St(MDone));
      else
        if (ErrHandler.GetErrorCount()==0)
          mprintf(St(MExtrAllOk));
        else
          mprintf(St(MExtrTotalErr),ErrHandler.GetErrorCount());
#endif
}


void CmdExtract::ExtractArchiveInit(CommandData *Cmd,Archive &Arc)
{
  DataIO.UnpArcSize=Arc.FileLength();

  FileCount=0;
  MatchedArgs=0;
#ifndef SFX_MODULE
  FirstFile=true;
#endif

  PasswordAll=(Cmd->Password.IsSet());
  if (PasswordAll)
    Password=Cmd->Password;

  DataIO.UnpVolume=false;

  PrevExtracted=false;
  AllMatchesExact=true;
  ReconstructDone=false;
  AnySolidDataUnpackedWell=false;

  StartTime.SetCurrentTime();
}


EXTRACT_ARC_CODE CmdExtract::ExtractArchive(CommandData *Cmd)
{
  Archive Arc(Cmd);
  if (!Arc.WOpen(ArcName))
  {
    ErrHandler.SetErrorCode(RARX_OPEN);
    return EXTRACT_ARC_NEXT;
  }

  if (!Arc.IsArchive(true))
  {
#ifndef GUI
    mprintf(St(MNotRAR),ArcName);
#endif
    if (CmpExt(ArcName,L"rar"))
      ErrHandler.SetErrorCode(RARX_WARNING);
    return EXTRACT_ARC_NEXT;
  }

  if (Arc.FailedHeaderDecryption) // Bad archive password.
    return EXTRACT_ARC_NEXT;

#ifndef SFX_MODULE
  if (Arc.Volume && !Arc.FirstVolume)
  {
    wchar FirstVolName[NM];
    VolNameToFirstName(ArcName,FirstVolName,Arc.NewNumbering);

    // If several volume names from same volume set are specified
    // and current volume is not first in set and first volume is present
    // and specified too, let's skip the current volume.
    if (wcsicomp(ArcName,FirstVolName)!=0 && FileExist(FirstVolName) &&
        Cmd->ArcNames.Search(FirstVolName,false))
      return EXTRACT_ARC_NEXT;
  }
#endif

  int64 VolumeSetSize=0; // Total size of volumes after the current volume.

  if (Arc.Volume)
  {
    // Calculate the total size of all accessible volumes.
    // This size is necessary to display the correct total progress indicator.

    wchar NextName[NM];
    wcscpy(NextName,Arc.FileName);

    while (true)
    {
      // First volume is already added to DataIO.TotalArcSize 
      // in initial TotalArcSize calculation in DoExtract.
      // So we skip it and start from second volume.
      NextVolumeName(NextName,ASIZE(NextName),!Arc.NewNumbering);
      FindData FD;
      if (FindFile::FastFind(NextName,&FD))
        VolumeSetSize+=FD.Size;
      else
        break;
    }
    DataIO.TotalArcSize+=VolumeSetSize;
  }

  ExtractArchiveInit(Cmd,Arc);

  if (*Cmd->Command=='T' || *Cmd->Command=='I')
    Cmd->Test=true;


#ifndef GUI
  if (*Cmd->Command=='I')
    Cmd->DisablePercentage=true;
  else
    if (Cmd->Test)
      mprintf(St(MExtrTest),ArcName);
    else
      mprintf(St(MExtracting),ArcName);
#endif

  Arc.ViewComment();


  while (1)
  {
    size_t Size=Arc.ReadHeader();


    bool Repeat=false;
    if (!ExtractCurrentFile(Cmd,Arc,Size,Repeat))
      if (Repeat)
      {
        // If we started extraction from not first volume and need to
        // restart it from first, we must correct DataIO.TotalArcSize
        // for correct total progress display. We subtract the size
        // of current volume and all volumes after it and add the size
        // of new (first) volume.
        FindData OldArc,NewArc;
        if (FindFile::FastFind(Arc.FileName,&OldArc) &&
            FindFile::FastFind(ArcName,&NewArc))
          DataIO.TotalArcSize-=VolumeSetSize+OldArc.Size-NewArc.Size;
        return EXTRACT_ARC_REPEAT;
      }
      else
        break;
  }


  return EXTRACT_ARC_NEXT;
}


bool CmdExtract::ExtractCurrentFile(CommandData *Cmd,Archive &Arc,size_t HeaderSize,bool &Repeat)
{
  wchar Command=Cmd->Command[0];
  if (HeaderSize==0)
    if (DataIO.UnpVolume)
    {
#ifdef NOVOLUME
      return false;
#else
      if (!MergeArchive(Arc,&DataIO,false,Command))
      {
        ErrHandler.SetErrorCode(RARX_WARNING);
        return false;
      }
#endif
    }
    else
      return false;
  HEADER_TYPE HeaderType=Arc.GetHeaderType();
  if (HeaderType!=HEAD_FILE)
  {
#ifndef SFX_MODULE
    if (HeaderType==HEAD3_OLDSERVICE && PrevExtracted)
      SetExtraInfo20(Cmd,Arc,DestFileName);
#endif
    if (HeaderType==HEAD_SERVICE && PrevExtracted)
      SetExtraInfo(Cmd,Arc,DestFileName);
    if (HeaderType==HEAD_ENDARC)
      if (Arc.EndArcHead.NextVolume)
      {
#ifndef NOVOLUME
        if (!MergeArchive(Arc,&DataIO,false,Command))
        {
          ErrHandler.SetErrorCode(RARX_WARNING);
          return false;
        }
#endif
        Arc.Seek(Arc.CurBlockPos,SEEK_SET);
        return true;
      }
      else
        return false;
    Arc.SeekToNext();
    return true;
  }
  PrevExtracted=false;

  if (!Cmd->Recurse && MatchedArgs>=Cmd->FileArgs.ItemsCount() && AllMatchesExact)
    return false;

  int MatchType=MATCH_WILDSUBPATH;

  bool EqualNames=false;
  int MatchNumber=Cmd->IsProcessFile(Arc.FileHead,&EqualNames,MatchType);
  bool ExactMatch=MatchNumber!=0;
#ifndef SFX_MODULE
  if (Cmd->ExclPath==EXCL_BASEPATH)
  {
    *Cmd->ArcPath=0;
    if (ExactMatch)
    {
      Cmd->FileArgs.Rewind();
      if (Cmd->FileArgs.GetString(Cmd->ArcPath,ASIZE(Cmd->ArcPath),MatchNumber-1))
        *PointToName(Cmd->ArcPath)=0;
    }
  }
#endif
  if (ExactMatch && !EqualNames)
    AllMatchesExact=false;

  Arc.ConvertAttributes();

#if !defined(SFX_MODULE) && !defined(RARDLL)
  if (Arc.FileHead.SplitBefore && FirstFile)
  {
    wchar CurVolName[NM];
    wcsncpyz(CurVolName,ArcName,ASIZE(CurVolName));
    VolNameToFirstName(ArcName,ArcName,Arc.NewNumbering);

    if (wcsicomp(ArcName,CurVolName)!=0 && FileExist(ArcName))
    {
      // If first volume name does not match the current name and if such
      // volume name really exists, let's unpack from this first volume.
      Repeat=true;
      return false;
    }
#ifndef RARDLL
    if (!ReconstructDone)
    {
      ReconstructDone=true;

      if (RecVolumesRestore(Cmd,Arc.FileName,true))
      {
        Repeat=true;
        return false;
      }
    }
#endif
    wcsncpyz(ArcName,CurVolName,ASIZE(ArcName));
  }
#endif

  wchar ArcFileName[NM];
  ConvertPath(Arc.FileHead.FileName,ArcFileName);

  if (Arc.FileHead.Version)
  {
    if (Cmd->VersionControl!=1 && !EqualNames)
    {
      if (Cmd->VersionControl==0)
        ExactMatch=false;
      int Version=ParseVersionFileName(ArcFileName,false);
      if (Cmd->VersionControl-1==Version)
        ParseVersionFileName(ArcFileName,true);
      else
        ExactMatch=false;
    }
  }
  else
    if (!Arc.IsArcDir() && Cmd->VersionControl>1)
      ExactMatch=false;

  DataIO.UnpVolume=Arc.FileHead.SplitAfter;
  DataIO.NextVolumeMissing=false;

  Arc.Seek(Arc.NextBlockPos-Arc.FileHead.PackSize,SEEK_SET);

  bool ExtrFile=false;
  bool SkipSolid=false;

#ifndef SFX_MODULE
  if (FirstFile && (ExactMatch || Arc.Solid) && Arc.FileHead.SplitBefore)
  {
    if (ExactMatch)
    {
      Log(Arc.FileName,St(MUnpCannotMerge),ArcFileName);
#ifdef RARDLL
      Cmd->DllError=ERAR_BAD_DATA;
#endif
      ErrHandler.SetErrorCode(RARX_OPEN);
    }
    ExactMatch=false;
  }

  FirstFile=false;
#endif

  if (ExactMatch || (SkipSolid=Arc.Solid)!=0)
  {

    ExtrPrepareName(Cmd,Arc,ArcFileName,DestFileName,ASIZE(DestFileName));

    // DestFileName can be set empty in case of excessive -ap switch.
    ExtrFile=!SkipSolid && *DestFileName!=0 && !Arc.FileHead.SplitBefore;

    if ((Cmd->FreshFiles || Cmd->UpdateFiles) && (Command=='E' || Command=='X'))
    {
      FindData FD;
      if (FindFile::FastFind(DestFileName,&FD))
      {
        if (FD.mtime >= Arc.FileHead.mtime)
        {
          // If directory already exists and its modification time is newer 
          // than start of extraction, it is likely it was created 
          // when creating a path to one of already extracted items. 
          // In such case we'll better update its time even if archived 
          // directory is older.

          if (!FD.IsDir || FD.mtime<StartTime)
            ExtrFile=false;
        }
      }
      else
        if (Cmd->FreshFiles)
          ExtrFile=false;
    }

    if (Arc.FileHead.Encrypted)
    {
#ifdef RARDLL
      if (!ExtrDllGetPassword(Cmd))
        return false;
#else
      if (!ExtrGetPassword(Cmd,Arc,ArcFileName))
      {
        PasswordCancelled=true;
        return false;
      }
#endif
      // Skip only the current encrypted file if empty password is entered.
      if (!Password.IsSet())
      {
        ErrHandler.SetErrorCode(RARX_WARNING);
#ifdef RARDLL
        Cmd->DllError=ERAR_MISSING_PASSWORD;
#endif
        ExtrFile=false;
      }
    }

#ifdef RARDLL
    if (*Cmd->DllDestName!=0)
    {
      wcsncpyz(DestFileName,Cmd->DllDestName,ASIZE(DestFileName));

//      Do we need this code?
//      if (Cmd->DllOpMode!=RAR_EXTRACT)
//        ExtrFile=false;
    }
#endif

    if (!CheckUnpVer(Arc,ArcFileName))
    {
      ExtrFile=false;
      ErrHandler.SetErrorCode(RARX_WARNING);
#ifdef RARDLL
      Cmd->DllError=ERAR_UNKNOWN_FORMAT;
#endif
    }

    File CurFile;

    bool LinkEntry=Arc.FileHead.RedirType!=FSREDIR_NONE;
    if (LinkEntry && Arc.FileHead.RedirType!=FSREDIR_FILECOPY)
    {
      if (ExtrFile && Command!='P' && !Cmd->Test)
      {
        // Overwrite prompt for symbolic and hard links.
        bool UserReject=false;
        if (FileExist(DestFileName) && !UserReject)
          FileCreate(Cmd,NULL,DestFileName,ASIZE(DestFileName),Cmd->Overwrite,&UserReject,Arc.FileHead.UnpSize,&Arc.FileHead.mtime);
        if (UserReject)
          ExtrFile=false;
      }
    }
    else
      if (Arc.IsArcDir())
      {
        if (!ExtrFile || Command=='P' || Command=='I' || Command=='E' || Cmd->ExclPath==EXCL_SKIPWHOLEPATH)
          return true;
        TotalFileCount++;
        ExtrCreateDir(Cmd,Arc,ArcFileName);
        return true;
      }
      else
        if (ExtrFile) // Create files and file copies (FSREDIR_FILECOPY).
          ExtrFile=ExtrCreateFile(Cmd,Arc,CurFile);

    if (!ExtrFile && Arc.Solid)
    {
      SkipSolid=true;
      ExtrFile=true;

    }
    if (ExtrFile)
    {
      bool TestMode=Cmd->Test || SkipSolid; // Unpack to memory, not to disk.

      if (!SkipSolid)
      {
        if (!TestMode && Command!='P' && CurFile.IsDevice())
        {
          Log(Arc.FileName,St(MInvalidName),DestFileName);
          ErrHandler.WriteError(Arc.FileName,DestFileName);
        }
        TotalFileCount++;
      }
      FileCount++;
#ifndef GUI
      if (Command!='I')
        if (SkipSolid)
          mprintf(St(MExtrSkipFile),ArcFileName);
        else
          switch(Cmd->Test ? 'T':Command) // "Test" can be also enabled by -t switch.
          {
            case 'T':
              mprintf(St(MExtrTestFile),ArcFileName);
              break;
#ifndef SFX_MODULE
            case 'P':
              mprintf(St(MExtrPrinting),ArcFileName);
              break;
#endif
            case 'X':
            case 'E':
              mprintf(St(MExtrFile),DestFileName);
              break;
          }
      if (!Cmd->DisablePercentage)
        mprintf(L"     ");
#endif

      SecPassword FilePassword=Password;
#if defined(_WIN_ALL) && !defined(SFX_MODULE)
      ConvertDosPassword(Arc,FilePassword);
#endif

      byte PswCheck[SIZE_PSWCHECK];
      DataIO.SetEncryption(false,Arc.FileHead.CryptMethod,&FilePassword,
             Arc.FileHead.SaltSet ? Arc.FileHead.Salt:NULL,
             Arc.FileHead.InitV,Arc.FileHead.Lg2Count,
             PswCheck,Arc.FileHead.HashKey);
      bool WrongPassword=false;

      // If header is damaged, we cannot rely on password check value,
      // because it can be damaged too.
      if (Arc.FileHead.Encrypted && Arc.FileHead.UsePswCheck &&
          memcmp(Arc.FileHead.PswCheck,PswCheck,SIZE_PSWCHECK)!=0 &&
          !Arc.BrokenHeader)
      {
        Log(Arc.FileName,St(MWrongPassword));
        ErrHandler.SetErrorCode(RARX_BADPWD);
        WrongPassword=true;
      }
      DataIO.CurUnpRead=0;
      DataIO.CurUnpWrite=0;
      DataIO.UnpHash.Init(Arc.FileHead.FileHash.Type,Cmd->Threads);
      DataIO.PackedDataHash.Init(Arc.FileHead.FileHash.Type,Cmd->Threads);
      DataIO.SetPackedSizeToRead(Arc.FileHead.PackSize);
      DataIO.SetFiles(&Arc,&CurFile);
      DataIO.SetTestMode(TestMode);
      DataIO.SetSkipUnpCRC(SkipSolid);
      if (!TestMode && !WrongPassword && !Arc.BrokenHeader &&
          (Arc.FileHead.PackSize<<11)>Arc.FileHead.UnpSize &&
          (Arc.FileHead.UnpSize<100000000 || Arc.FileLength()>Arc.FileHead.PackSize))
        CurFile.Prealloc(Arc.FileHead.UnpSize);

      CurFile.SetAllowDelete(!Cmd->KeepBroken);

      bool FileCreateMode=!TestMode && !SkipSolid && Command!='P';
      bool ShowChecksum=true; // Display checksum verification result.

      bool LinkSuccess=true; // Assume success for test mode.
      if (LinkEntry)
      {
        FILE_SYSTEM_REDIRECT Type=Arc.FileHead.RedirType;

        if (Type==FSREDIR_HARDLINK || Type==FSREDIR_FILECOPY)
        {
          wchar NameExisting[NM];
          ExtrPrepareName(Cmd,Arc,Arc.FileHead.RedirName,NameExisting,ASIZE(NameExisting));
          if (FileCreateMode && *NameExisting!=0) // *NameExisting can be 0 in case of excessive -ap switch.
            if (Type==FSREDIR_HARDLINK)
              LinkSuccess=ExtractHardlink(DestFileName,NameExisting,ASIZE(NameExisting));
            else
              LinkSuccess=ExtractFileCopy(CurFile,Arc.FileName,DestFileName,NameExisting,ASIZE(NameExisting));
        }
        else
          if (Type==FSREDIR_UNIXSYMLINK || Type==FSREDIR_WINSYMLINK || Type==FSREDIR_JUNCTION)
          {
            if (FileCreateMode)
              LinkSuccess=ExtractSymlink(Cmd,DataIO,Arc,DestFileName);
          }
          else
          {
#ifndef SFX_MODULE
            Log(Arc.FileName,St(MUnknownExtra),DestFileName);
#endif
            LinkSuccess=false;
          }
          
          if (!LinkSuccess || Arc.Format==RARFMT15 && !FileCreateMode)
          {
            // RAR 5.x links have a valid data checksum even in case of
            // failure, because they do not store any data.
            // We do not want to display "OK" in this case.
            // For 4.x symlinks we verify the checksum only when extracting,
            // but not when testing an archive.
            ShowChecksum=false;
          }
          PrevExtracted=FileCreateMode && LinkSuccess;
      }
      else
        if (!Arc.FileHead.SplitBefore && !WrongPassword)
          if (Arc.FileHead.Method==0)
            UnstoreFile(DataIO,Arc.FileHead.UnpSize);
          else
          {
            Unp->Init(Arc.FileHead.WinSize,Arc.FileHead.Solid);
            Unp->SetDestSize(Arc.FileHead.UnpSize);
#ifndef SFX_MODULE
            if (Arc.Format!=RARFMT50 && Arc.FileHead.UnpVer<=15)
              Unp->DoUnpack(15,FileCount>1 && Arc.Solid);
            else
#endif
              Unp->DoUnpack(Arc.FileHead.UnpVer,Arc.FileHead.Solid);
          }

      Arc.SeekToNext();

      bool ValidCRC=DataIO.UnpHash.Cmp(&Arc.FileHead.FileHash,Arc.FileHead.UseHashKey ? Arc.FileHead.HashKey:NULL);

      // We set AnySolidDataUnpackedWell to true if we found at least one
      // valid non-zero solid file in preceding solid stream. If it is true
      // and if current encrypted file is broken, we do not need to hint
      // about a wrong password and can report CRC error only.
      if (!Arc.FileHead.Solid)
        AnySolidDataUnpackedWell=false; // Reset the flag, because non-solid file is found.
      else
        if (Arc.FileHead.Method!=0 && Arc.FileHead.UnpSize>0 && ValidCRC)
          AnySolidDataUnpackedWell=true;
 
      bool BrokenFile=false;
      
      // Checksum is not calculated in skip solid mode for performance reason.
      if (!SkipSolid)
      {
        if (!WrongPassword && ValidCRC)
        {
#ifndef GUI
          if (Command!='P' && Command!='I' && ShowChecksum)
            mprintf(L"%s%s ",Cmd->DisablePercentage ? L" ":L"\b\b\b\b\b ",
              Arc.FileHead.FileHash.Type==HASH_NONE ? L"  ?":St(MOk));
#endif
        }
        else
        {
          if (!WrongPassword)
            if (Arc.FileHead.Encrypted && (!Arc.FileHead.UsePswCheck || 
                Arc.BrokenHeader) && !AnySolidDataUnpackedWell)
            {
              Log(Arc.FileName,St(MEncrBadCRC),ArcFileName);
            }
            else
            {
              Log(Arc.FileName,St(MCRCFailed),ArcFileName);
            }
          BrokenFile=true;
          ErrHandler.SetErrorCode(RARX_CRC);
#ifdef RARDLL
          // If we already have ERAR_EOPEN as result of missing volume,
          // we should not replace it with less precise ERAR_BAD_DATA.
          if (Cmd->DllError!=ERAR_EOPEN)
            Cmd->DllError=ERAR_BAD_DATA;
#endif
        }
      }
#ifndef GUI
      else
        mprintf(L"\b\b\b\b\b     ");
#endif

      if (!TestMode && !WrongPassword && (Command=='X' || Command=='E') &&
          (!LinkEntry || Arc.FileHead.RedirType==FSREDIR_FILECOPY && LinkSuccess) && 
          (!BrokenFile || Cmd->KeepBroken))
      {
        // We could preallocate more space that really written to broken file.
        if (BrokenFile)
          CurFile.Truncate();

#if defined(_WIN_ALL) || defined(_EMX)
        if (Cmd->ClearArc)
          Arc.FileHead.FileAttr&=~FILE_ATTRIBUTE_ARCHIVE;
#endif


        CurFile.SetOpenFileTime(
          Cmd->xmtime==EXTTIME_NONE ? NULL:&Arc.FileHead.mtime,
          Cmd->xctime==EXTTIME_NONE ? NULL:&Arc.FileHead.ctime,
          Cmd->xatime==EXTTIME_NONE ? NULL:&Arc.FileHead.atime);
        CurFile.Close();
#if defined(_WIN_ALL) && !defined(SFX_MODULE)
        if (Cmd->SetCompressedAttr &&
            (Arc.FileHead.FileAttr & FILE_ATTRIBUTE_COMPRESSED)!=0)
          SetFileCompression(CurFile.FileName,true);
#endif
#ifdef _UNIX
        if (Cmd->ProcessOwners && Arc.Format==RARFMT50 && Arc.FileHead.UnixOwnerSet)
          SetUnixOwner(Arc,CurFile.FileName);
#endif

        CurFile.SetCloseFileTime(
          Cmd->xmtime==EXTTIME_NONE ? NULL:&Arc.FileHead.mtime,
          Cmd->xatime==EXTTIME_NONE ? NULL:&Arc.FileHead.atime);
        if (!Cmd->IgnoreGeneralAttr)
          SetFileAttr(CurFile.FileName,Arc.FileHead.FileAttr);
        PrevExtracted=true;
      }
    }
  }
  if (ExactMatch)
    MatchedArgs++;
  if (DataIO.NextVolumeMissing)
    return false;
  if (!ExtrFile)
    if (!Arc.Solid)
      Arc.SeekToNext();
    else
      if (!SkipSolid)
        return false;
  return true;
}


void CmdExtract::UnstoreFile(ComprDataIO &DataIO,int64 DestUnpSize)
{
  Array<byte> Buffer(0x100000);
  while (1)
  {
    uint Code=DataIO.UnpRead(&Buffer[0],Buffer.Size());
    if (Code==0 || (int)Code==-1)
      break;
    Code=Code<DestUnpSize ? Code:(uint)DestUnpSize;
    DataIO.UnpWrite(&Buffer[0],Code);
    if (DestUnpSize>=0)
      DestUnpSize-=Code;
  }
}


bool CmdExtract::ExtractFileCopy(File &New,wchar *ArcName,wchar *NameNew,wchar *NameExisting,size_t NameExistingSize)
{
#ifdef _WIN_ALL
  UnixSlashToDos(NameExisting,NameExisting,NameExistingSize);
#elif defined(_UNIX)
  DosSlashToUnix(NameExisting,NameExisting,NameExistingSize);
#endif
  File Existing;
  if (!Existing.Open(NameExisting))
  {
    ErrHandler.OpenErrorMsg(ArcName,NameExisting);
    Log(ArcName,St(MCopyError),NameExisting,NameNew);
    Log(ArcName,St(MCopyErrorHint));
    return false;
  }

  Array<char> Buffer(0x100000);
  int64 CopySize=0;

  while (true)
  {
    Wait();
    int ReadSize=Existing.Read(&Buffer[0],Buffer.Size());
    if (ReadSize==0)
      break;
    New.Write(&Buffer[0],ReadSize);
    CopySize+=ReadSize;
  }

  return true;
}


void CmdExtract::ExtrPrepareName(CommandData *Cmd,Archive &Arc,const wchar *ArcFileName,wchar *DestName,size_t DestSize)
{
  wcsncpyz(DestName,Cmd->ExtrPath,DestSize);

  // We need IsPathDiv check here to correctly handle Unix forward slash
  // in the end of destination path in Windows: rar x arc dest/
  if (*Cmd->ExtrPath!=0 && !IsPathDiv(*PointToLastChar(Cmd->ExtrPath)))
  {
    // Destination path can be without trailing slash if it come from GUI shell.
    AddEndSlash(DestName,DestSize);
  }

#ifndef SFX_MODULE
  if (Cmd->AppendArcNameToPath)
  {
    wcsncatz(DestName,PointToName(Arc.FirstVolumeName),DestSize);
    SetExt(DestName,NULL);
    AddEndSlash(DestName,DestSize);
  }
#endif

#ifndef SFX_MODULE
  size_t ArcPathLength=wcslen(Cmd->ArcPath);
  if (ArcPathLength>0)
  {
    size_t NameLength=wcslen(ArcFileName);
    ArcFileName+=Min(ArcPathLength,NameLength);
    while (*ArcFileName==CPATHDIVIDER)
      ArcFileName++;
    if (*ArcFileName==0) // Excessive -ap switch.
    {
      *DestName=0;
      return;
    }
  }
#endif

  wchar Command=Cmd->Command[0];
  // Use -ep3 only in systems, where disk letters are exist, not in Unix.
  bool AbsPaths=Cmd->ExclPath==EXCL_ABSPATH && Command=='X' && IsDriveDiv(':');

  // We do not use any user specified destination paths when extracting
  // absolute paths in -ep3 mode.
  if (AbsPaths)
    *DestName=0;

  if (Command=='E' || Cmd->ExclPath==EXCL_SKIPWHOLEPATH)
    wcsncatz(DestName,PointToName(ArcFileName),DestSize);
  else
    wcsncatz(DestName,ArcFileName,DestSize);

  wchar DiskLetter=toupperw(DestName[0]);

  if (AbsPaths)
  {
    if (DestName[1]=='_' && IsPathDiv(DestName[2]) &&
        DiskLetter>='A' && DiskLetter<='Z')
      DestName[1]=':';
    else
      if (DestName[0]=='_' && DestName[1]=='_')
      {
        // Convert __server\share to \\server\share.
        DestName[0]=CPATHDIVIDER;
        DestName[1]=CPATHDIVIDER;
      }
  }
}


#ifdef RARDLL
bool CmdExtract::ExtrDllGetPassword(CommandData *Cmd)
{
  if (!Cmd->Password.IsSet())
  {
    if (Cmd->Callback!=NULL)
    {
      wchar PasswordW[MAXPASSWORD];
      *PasswordW=0;
      if (Cmd->Callback(UCM_NEEDPASSWORDW,Cmd->UserData,(LPARAM)PasswordW,ASIZE(PasswordW))==-1)
        *PasswordW=0;
      if (*PasswordW==0)
      {
        char PasswordA[MAXPASSWORD];
        *PasswordA=0;
        if (Cmd->Callback(UCM_NEEDPASSWORD,Cmd->UserData,(LPARAM)PasswordA,ASIZE(PasswordA))==-1)
          *PasswordA=0;
        GetWideName(PasswordA,NULL,PasswordW,ASIZE(PasswordW));
        cleandata(PasswordA,sizeof(PasswordA));
      }
      Cmd->Password.Set(PasswordW);
      cleandata(PasswordW,sizeof(PasswordW));
    }
    if (!Cmd->Password.IsSet())
      return false;
  }
  Password=Cmd->Password;
  return true;
}
#endif


#ifndef RARDLL
bool CmdExtract::ExtrGetPassword(CommandData *Cmd,Archive &Arc,const wchar *ArcFileName)
{
  if (!Password.IsSet())
  {
    if (!GetPassword(PASSWORD_FILE,ArcFileName,&Password))
    {
      return false;
    }
  }
#if !defined(GUI) && !defined(SILENT)
  else
    if (!PasswordAll && !Arc.FileHead.Solid)
    {
      eprintf(St(MUseCurPsw),ArcFileName);
      switch(Cmd->AllYes ? 1:Ask(St(MYesNoAll)))
      {
        case -1:
          ErrHandler.Exit(RARX_USERBREAK);
        case 2:
          if (!GetPassword(PASSWORD_FILE,ArcFileName,&Password))
            return false;
          break;
        case 3:
          PasswordAll=true;
          break;
      }
    }
#endif
  return true;
}
#endif


#if defined(_WIN_ALL) && !defined(SFX_MODULE)
void CmdExtract::ConvertDosPassword(Archive &Arc,SecPassword &DestPwd)
{
  if (Arc.Format==RARFMT15 && Arc.FileHead.HostOS==HOST_MSDOS)
  {
    // We need the password in OEM encoding if file was encrypted by
    // native RAR/DOS (not extender based). Let's make the conversion.
    wchar PlainPsw[MAXPASSWORD];
    Password.Get(PlainPsw,ASIZE(PlainPsw));
    char PswA[MAXPASSWORD];
    CharToOemBuffW(PlainPsw,PswA,ASIZE(PswA));
    PswA[ASIZE(PswA)-1]=0;
    CharToWide(PswA,PlainPsw,ASIZE(PlainPsw));
    DestPwd.Set(PlainPsw);
    cleandata(PlainPsw,sizeof(PlainPsw));
    cleandata(PswA,sizeof(PswA));
  }
}
#endif


void CmdExtract::ExtrCreateDir(CommandData *Cmd,Archive &Arc,const wchar *ArcFileName)
{
  if (Cmd->Test)
  {
#ifndef GUI
    mprintf(St(MExtrTestFile),ArcFileName);
    mprintf(L" %s",St(MOk));
#endif
    return;
  }

  MKDIR_CODE MDCode=MakeDir(DestFileName,!Cmd->IgnoreGeneralAttr,Arc.FileHead.FileAttr);
  bool DirExist=false;
  if (MDCode!=MKDIR_SUCCESS)
  {
    DirExist=FileExist(DestFileName);
    if (DirExist && !IsDir(GetFileAttr(DestFileName)))
    {
      // File with name same as this directory exists. Propose user
      // to overwrite it.
      bool UserReject;
      FileCreate(Cmd,NULL,DestFileName,ASIZE(DestFileName),Cmd->Overwrite,&UserReject,Arc.FileHead.UnpSize,&Arc.FileHead.mtime);
      DirExist=false;
    }
    if (!DirExist)
    {
      CreatePath(DestFileName,true);
      MDCode=MakeDir(DestFileName,!Cmd->IgnoreGeneralAttr,Arc.FileHead.FileAttr);
    }
  }
  if (MDCode==MKDIR_SUCCESS)
  {
#ifndef GUI
    mprintf(St(MCreatDir),DestFileName);
    mprintf(L" %s",St(MOk));
#endif
    PrevExtracted=true;
  }
  else
    if (DirExist)
    {
      if (!Cmd->IgnoreGeneralAttr)
        SetFileAttr(DestFileName,Arc.FileHead.FileAttr);
      PrevExtracted=true;
    }
    else
    {
      Log(Arc.FileName,St(MExtrErrMkDir),DestFileName);
      ErrHandler.CheckLongPathErrMsg(DestFileName);
      ErrHandler.SysErrMsg();
#ifdef RARDLL
      Cmd->DllError=ERAR_ECREATE;
#endif
      ErrHandler.SetErrorCode(RARX_CREATE);
    }
  if (PrevExtracted)
  {
#if defined(_WIN_ALL) && !defined(SFX_MODULE)
    if (Cmd->SetCompressedAttr &&
        (Arc.FileHead.FileAttr & FILE_ATTRIBUTE_COMPRESSED)!=0 && WinNT())
      SetFileCompression(DestFileName,true);
#endif
    SetDirTime(DestFileName,
      Cmd->xmtime==EXTTIME_NONE ? NULL:&Arc.FileHead.mtime,
      Cmd->xctime==EXTTIME_NONE ? NULL:&Arc.FileHead.ctime,
      Cmd->xatime==EXTTIME_NONE ? NULL:&Arc.FileHead.atime);
  }
}


bool CmdExtract::ExtrCreateFile(CommandData *Cmd,Archive &Arc,File &CurFile)
{
  bool Success=true;
  wchar Command=Cmd->Command[0];
#if !defined(GUI) && !defined(SFX_MODULE)
  if (Command=='P')
    CurFile.SetHandleType(FILE_HANDLESTD);
#endif
  if ((Command=='E' || Command=='X') && !Cmd->Test)
  {
    bool UserReject;
    // Specify "write only" mode to avoid OpenIndiana NAS problems
    // with SetFileTime and read+write files.
    if (!FileCreate(Cmd,&CurFile,DestFileName,ASIZE(DestFileName),Cmd->Overwrite,&UserReject,Arc.FileHead.UnpSize,&Arc.FileHead.mtime,true))
    {
      Success=false;
      if (!UserReject)
      {
        ErrHandler.CreateErrorMsg(Arc.FileName,DestFileName);
        ErrHandler.SetErrorCode(RARX_CREATE);
#ifdef RARDLL
        Cmd->DllError=ERAR_ECREATE;
#endif
        if (!IsNameUsable(DestFileName))
        {
          Log(Arc.FileName,St(MCorrectingName));
          wchar OrigName[ASIZE(DestFileName)];
          wcsncpyz(OrigName,DestFileName,ASIZE(OrigName));

          MakeNameUsable(DestFileName,true);

          CreatePath(DestFileName,true);
          if (FileCreate(Cmd,&CurFile,DestFileName,ASIZE(DestFileName),Cmd->Overwrite,&UserReject,Arc.FileHead.UnpSize,&Arc.FileHead.mtime,true))
          {
#ifndef SFX_MODULE
            Log(Arc.FileName,St(MRenaming),OrigName,DestFileName);
#endif
            Success=true;
          }
          else
            ErrHandler.CreateErrorMsg(Arc.FileName,DestFileName);
        }
      }
    }
  }
  return Success;
}


bool CmdExtract::CheckUnpVer(Archive &Arc,const wchar *ArcFileName)
{
  bool WrongVer;
  if (Arc.Format==RARFMT50) // Both SFX and RAR can unpack RAR 5.0 archives.
    WrongVer=Arc.FileHead.UnpVer>VER_UNPACK5;
  else
  {
#ifdef SFX_MODULE   // SFX can unpack only RAR 2.9 archives.
    WrongVer=Arc.FileHead.UnpVer!=VER_UNPACK;
#else               // All formats since 1.3 for RAR.
    WrongVer=Arc.FileHead.UnpVer<13 || Arc.FileHead.UnpVer>VER_UNPACK;
#endif
  }

  // We can unpack stored files regardless of compression version field.
  if (Arc.FileHead.Method==0)
    WrongVer=false;

  if (WrongVer)
  {
#ifndef SILENT
    Log(Arc.FileName,St(MUnknownMeth),ArcFileName);
#ifndef SFX_MODULE
//      Log(Arc.FileName,St(MVerRequired),Arc.FileHead.UnpVer/10,Arc.FileHead.UnpVer%10);
    Log(Arc.FileName,St(MNewerRAR));
#endif
#endif
  }
  return !WrongVer;
}
