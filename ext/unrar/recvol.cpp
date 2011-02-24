#include "rar.hpp"

// Buffer size for all volumes involved.
static const size_t TotalBufferSize=0x4000000;

RecVolumes::RecVolumes()
{
  Buf.Alloc(TotalBufferSize);
  memset(SrcFile,0,sizeof(SrcFile));
}


RecVolumes::~RecVolumes()
{
  for (int I=0;I<sizeof(SrcFile)/sizeof(SrcFile[0]);I++)
    delete SrcFile[I];
}




bool RecVolumes::Restore(RAROptions *Cmd,const char *Name,
                         const wchar *NameW,bool Silent)
{
  char ArcName[NM];
  wchar ArcNameW[NM];
  strcpy(ArcName,Name);
  wcscpy(ArcNameW,NameW);
  char *Ext=GetExt(ArcName);
  bool NewStyle=false;
  bool RevName=Ext!=NULL && stricomp(Ext,".rev")==0;
  if (RevName)
  {
    for (int DigitGroup=0;Ext>ArcName && DigitGroup<3;Ext--)
      if (!IsDigit(*Ext))
        if (IsDigit(*(Ext-1)) && (*Ext=='_' || DigitGroup<2))
          DigitGroup++;
        else
          if (DigitGroup<2)
          {
            NewStyle=true;
            break;
          }
    while (IsDigit(*Ext) && Ext>ArcName+1)
      Ext--;
    strcpy(Ext,"*.*");

    if (*ArcNameW!=0)
    {
      wchar *ExtW=GetExt(ArcNameW);
      for (int DigitGroup=0;ExtW>ArcNameW && DigitGroup<3;ExtW--)
        if (!IsDigit(*ExtW))
          if (IsDigit(*(ExtW-1)) && (*ExtW=='_' || DigitGroup<2))
            DigitGroup++;
          else
            if (DigitGroup<2)
            {
              NewStyle=true;
              break;
            }
      while (IsDigit(*ExtW) && ExtW>ArcNameW+1)
        ExtW--;
      wcscpy(ExtW,L"*.*");
    }
    
    FindFile Find;
    Find.SetMask(ArcName);
    Find.SetMaskW(ArcNameW);
    FindData fd;
    while (Find.Next(&fd))
    {
      Archive Arc(Cmd);
      if (Arc.WOpen(fd.Name,fd.NameW) && Arc.IsArchive(true))
      {
        strcpy(ArcName,fd.Name);
        wcscpy(ArcNameW,fd.NameW);
        break;
      }
    }
  }

  Archive Arc(Cmd);
  if (!Arc.WCheckOpen(ArcName,ArcNameW))
    return(false);
  if (!Arc.Volume)
  {
#ifndef SILENT
    Log(ArcName,St(MNotVolume),ArcName);
#endif
    return(false);
  }
  bool NewNumbering=(Arc.NewMhd.Flags & MHD_NEWNUMBERING)!=0;
  Arc.Close();

  char *VolNumStart=VolNameToFirstName(ArcName,ArcName,NewNumbering);
  char RecVolMask[NM];
  strcpy(RecVolMask,ArcName);
  size_t BaseNamePartLength=VolNumStart-ArcName;
  strcpy(RecVolMask+BaseNamePartLength,"*.rev");

  wchar RecVolMaskW[NM];
  size_t BaseNamePartLengthW=0;
  *RecVolMaskW=0;
  if (*ArcNameW!=0)
  {
    wchar *VolNumStartW=VolNameToFirstName(ArcNameW,ArcNameW,NewNumbering);
    wcscpy(RecVolMaskW,ArcNameW);
    BaseNamePartLengthW=VolNumStartW-ArcNameW;
    wcscpy(RecVolMaskW+BaseNamePartLengthW,L"*.rev");
  }


#ifndef SILENT
  int64 RecFileSize=0;
#endif

  // We cannot display "Calculating CRC..." message here, because we do not
  // know if we'll find any recovery volumes. We'll display it after finding
  // the first recovery volume.
  bool CalcCRCMessageDone=false;

  FindFile Find;
  Find.SetMask(RecVolMask);
  Find.SetMaskW(RecVolMaskW);
  FindData RecData;
  int FileNumber=0,RecVolNumber=0,FoundRecVolumes=0,MissingVolumes=0;
  char PrevName[NM];
  wchar PrevNameW[NM];
  while (Find.Next(&RecData))
  {
    char *CurName=RecData.Name;
    wchar *CurNameW=RecData.NameW;
    int P[3];
    if (!RevName && !NewStyle)
    {
      NewStyle=true;

      char *Dot=GetExt(CurName);
      if (Dot!=NULL)
      {
        int LineCount=0;
        Dot--;
        while (Dot>CurName && *Dot!='.')
        {
          if (*Dot=='_')
            LineCount++;
          Dot--;
        }
        if (LineCount==2)
          NewStyle=false;
      }

      wchar *DotW=GetExt(CurNameW);
      if (DotW!=NULL)
      {
        int LineCount=0;
        DotW--;
        while (DotW>CurNameW && *DotW!='.')
        {
          if (*DotW=='_')
            LineCount++;
          DotW--;
        }
        if (LineCount==2)
          NewStyle=false;
      }
    }
    if (NewStyle)
    {
      if (!CalcCRCMessageDone)
      {
#ifndef SILENT
        mprintf(St(MCalcCRCAllVol));
#endif
        CalcCRCMessageDone=true;
      }
      
      File CurFile;
      CurFile.TOpen(CurName,CurNameW);
      CurFile.Seek(0,SEEK_END);
      int64 Length=CurFile.Tell();
      CurFile.Seek(Length-7,SEEK_SET);
      for (int I=0;I<3;I++)
        P[2-I]=CurFile.GetByte()+1;
      uint FileCRC=0;
      for (int I=0;I<4;I++)
        FileCRC|=CurFile.GetByte()<<(I*8);
      if (FileCRC!=CalcFileCRC(&CurFile,Length-4))
      {
#ifndef SILENT
        mprintf(St(MCRCFailed),CurName);
#endif
        continue;
      }
    }
    else
    {
      char *Dot=GetExt(CurName);
      if (Dot==NULL)
        continue;
      bool WrongParam=false;
      for (int I=0;I<ASIZE(P);I++)
      {
        do
        {
          Dot--;
        } while (IsDigit(*Dot) && Dot>=CurName+BaseNamePartLength);
        P[I]=atoi(Dot+1);
        if (P[I]==0 || P[I]>255)
          WrongParam=true;
      }
      if (WrongParam)
        continue;
    }
    if (P[1]+P[2]>255)
      continue;
    if (RecVolNumber!=0 && RecVolNumber!=P[1] || FileNumber!=0 && FileNumber!=P[2])
    {
#ifndef SILENT
      Log(NULL,St(MRecVolDiffSets),CurName,PrevName);
#endif
      return(false);
    }
    RecVolNumber=P[1];
    FileNumber=P[2];
    strcpy(PrevName,CurName);
    wcscpy(PrevNameW,CurNameW);
    File *NewFile=new File;
    NewFile->TOpen(CurName,CurNameW);
    SrcFile[FileNumber+P[0]-1]=NewFile;
    FoundRecVolumes++;
#ifndef SILENT
    if (RecFileSize==0)
      RecFileSize=NewFile->FileLength();
#endif
  }
#ifndef SILENT
  if (!Silent || FoundRecVolumes!=0)
  {
    mprintf(St(MRecVolFound),FoundRecVolumes);
  }
#endif
  if (FoundRecVolumes==0)
    return(false);

  bool WriteFlags[256];
  memset(WriteFlags,0,sizeof(WriteFlags));

  char LastVolName[NM];
  *LastVolName=0;
  wchar LastVolNameW[NM];
  *LastVolNameW=0;

  for (int CurArcNum=0;CurArcNum<FileNumber;CurArcNum++)
  {
    Archive *NewFile=new Archive;
    bool ValidVolume=FileExist(ArcName,ArcNameW);
    if (ValidVolume)
    {
      NewFile->TOpen(ArcName,ArcNameW);
      ValidVolume=NewFile->IsArchive(false);
      if (ValidVolume)
      {
        while (NewFile->ReadHeader()!=0)
        {
          if (NewFile->GetHeaderType()==ENDARC_HEAD)
          {
            if ((NewFile->EndArcHead.Flags&EARC_DATACRC)!=0 && 
                NewFile->EndArcHead.ArcDataCRC!=CalcFileCRC(NewFile,NewFile->CurBlockPos))
            {
              ValidVolume=false;
#ifndef SILENT
              mprintf(St(MCRCFailed),ArcName);
#endif
            }
            break;
          }
          NewFile->SeekToNext();
        }
      }
      if (!ValidVolume)
      {
        NewFile->Close();
        char NewName[NM];
        strcpy(NewName,ArcName);
        strcat(NewName,".bad");

        wchar NewNameW[NM];
        wcscpy(NewNameW,ArcNameW);
        if (*NewNameW!=0)
          wcscat(NewNameW,L".bad");
#ifndef SILENT
        mprintf(St(MBadArc),ArcName);
        mprintf(St(MRenaming),ArcName,NewName);
#endif
        RenameFile(ArcName,ArcNameW,NewName,NewNameW);
      }
      NewFile->Seek(0,SEEK_SET);
    }
    if (!ValidVolume)
    {
      // It is important to return 'false' instead of aborting here,
      // so if we are called from extraction, we will be able to continue
      // extracting. It may happen if .rar and .rev are on read-only disks
      // like CDs.
      if (!NewFile->Create(ArcName,ArcNameW))
      {
        // We need to display the title of operation before the error message,
        // to make clear for user that create error is related to recovery 
        // volumes. This is why we cannot use WCreate call here. Title must be
        // before create error, not after that.
#ifndef SILENT
        mprintf(St(MReconstructing));
#endif
        ErrHandler.CreateErrorMsg(ArcName,ArcNameW);
        return false;
      }

      WriteFlags[CurArcNum]=true;
      MissingVolumes++;

      if (CurArcNum==FileNumber-1)
      {
        strcpy(LastVolName,ArcName);
        wcscpy(LastVolNameW,ArcNameW);
      }

#ifndef SILENT
      mprintf(St(MAbsNextVol),ArcName);
#endif
    }
    SrcFile[CurArcNum]=(File*)NewFile;
    NextVolumeName(ArcName,ArcNameW,ASIZE(ArcName),!NewNumbering);
  }

#ifndef SILENT
  mprintf(St(MRecVolMissing),MissingVolumes);
#endif

  if (MissingVolumes==0)
  {
#ifndef SILENT
    mprintf(St(MRecVolAllExist));
#endif
    return(false);
  }

  if (MissingVolumes>FoundRecVolumes)
  {
#ifndef SILENT
    mprintf(St(MRecVolCannotFix));
#endif
    return(false);
  }
#ifndef SILENT
  mprintf(St(MReconstructing));
#endif

  RSCoder RSC(RecVolNumber);

  int TotalFiles=FileNumber+RecVolNumber;
  int Erasures[256],EraSize=0;

  for (int I=0;I<TotalFiles;I++)
    if (WriteFlags[I] || SrcFile[I]==NULL)
      Erasures[EraSize++]=I;

#ifndef SILENT
  int64 ProcessedSize=0;
#ifndef GUI
  int LastPercent=-1;
  mprintf("     ");
#endif
#endif
  // Size of per file buffer.
  size_t RecBufferSize=TotalBufferSize/TotalFiles;

  while (true)
  {
    Wait();
    int MaxRead=0;
    for (int I=0;I<TotalFiles;I++)
      if (WriteFlags[I] || SrcFile[I]==NULL)
        memset(&Buf[I*RecBufferSize],0,RecBufferSize);
      else
      {
        int ReadSize=SrcFile[I]->Read(&Buf[I*RecBufferSize],RecBufferSize);
        if (ReadSize!=RecBufferSize)
          memset(&Buf[I*RecBufferSize+ReadSize],0,RecBufferSize-ReadSize);
        if (ReadSize>MaxRead)
          MaxRead=ReadSize;
      }
    if (MaxRead==0)
      break;
#ifndef SILENT
    int CurPercent=ToPercent(ProcessedSize,RecFileSize);
    if (!Cmd->DisablePercentage && CurPercent!=LastPercent)
    {
      mprintf("\b\b\b\b%3d%%",CurPercent);
      LastPercent=CurPercent;
    }
    ProcessedSize+=MaxRead;
#endif
    for (int BufPos=0;BufPos<MaxRead;BufPos++)
    {
      if ((BufPos&0xffff)==0)
        Wait();
      byte Data[256];
      for (int I=0;I<TotalFiles;I++)
        Data[I]=Buf[I*RecBufferSize+BufPos];
      RSC.Decode(Data,TotalFiles,Erasures,EraSize);
      for (int I=0;I<EraSize;I++)
        Buf[Erasures[I]*RecBufferSize+BufPos]=Data[Erasures[I]];
/*
      for (int I=0;I<FileNumber;I++)
        Buf[I*RecBufferSize+BufPos]=Data[I];
*/
    }
    for (int I=0;I<FileNumber;I++)
      if (WriteFlags[I])
        SrcFile[I]->Write(&Buf[I*RecBufferSize],MaxRead);
  }
  for (int I=0;I<RecVolNumber+FileNumber;I++)
    if (SrcFile[I]!=NULL)
    {
      File *CurFile=SrcFile[I];
      if (NewStyle && WriteFlags[I])
      {
        int64 Length=CurFile->Tell();
        CurFile->Seek(Length-7,SEEK_SET);
        for (int J=0;J<7;J++)
          CurFile->PutByte(0);
      }
      CurFile->Close();
      SrcFile[I]=NULL;
    }
  if (*LastVolName!=0 || *LastVolNameW!=0)
  {
    // Truncate the last volume to its real size.
    Archive Arc(Cmd);
    if (Arc.Open(LastVolName,LastVolNameW,false,true) && Arc.IsArchive(true) &&
        Arc.SearchBlock(ENDARC_HEAD))
    {
      Arc.Seek(Arc.NextBlockPos,SEEK_SET);
      char Buf[8192];
      int ReadSize=Arc.Read(Buf,sizeof(Buf));
      int ZeroCount=0;
      while (ZeroCount<ReadSize && Buf[ZeroCount]==0)
        ZeroCount++;
      if (ZeroCount==ReadSize)
      {
        Arc.Seek(Arc.NextBlockPos,SEEK_SET);
        Arc.Truncate();
      }
    }
  }
#if !defined(GUI) && !defined(SILENT)
  if (!Cmd->DisablePercentage)
    mprintf("\b\b\b\b100%%");
  if (!Silent && !Cmd->DisableDone)
    mprintf(St(MDone));
#endif
  return(true);
}
