#include "rar.hpp"

// Buffer size for all volumes involved.
static const size_t TotalBufferSize=0x4000000;

class RSEncode // Encode or decode data area, one object per one thread.
{
  private:
    RSCoder RSC;
  public:
    void EncodeBuf();
    void DecodeBuf();

    void Init(int RecVolNumber) {RSC.Init(RecVolNumber);}
    byte *Buf;
    byte *OutBuf;
    int BufStart;
    int BufEnd;
    int FileNumber;
    int RecVolNumber;
    size_t RecBufferSize;
    int *Erasures;
    int EraSize;
};


#ifdef RAR_SMP
THREAD_PROC(RSEncodeThread)
{
  RSEncode *rs=(RSEncode *)Data;
  rs->EncodeBuf();
}

THREAD_PROC(RSDecodeThread)
{
  RSEncode *rs=(RSEncode *)Data;
  rs->DecodeBuf();
}
#endif

RecVolumes3::RecVolumes3()
{
  Buf.Alloc(TotalBufferSize);
  memset(SrcFile,0,sizeof(SrcFile));
#ifdef RAR_SMP
  RSThreadPool=CreateThreadPool();
#endif
}


RecVolumes3::~RecVolumes3()
{
  for (size_t I=0;I<ASIZE(SrcFile);I++)
    delete SrcFile[I];
#ifdef RAR_SMP
  DestroyThreadPool(RSThreadPool);
#endif
}




void RSEncode::EncodeBuf()
{
  for (int BufPos=BufStart;BufPos<BufEnd;BufPos++)
  {
    byte Data[256],Code[256];
    for (int I=0;I<FileNumber;I++)
      Data[I]=Buf[I*RecBufferSize+BufPos];
    RSC.Encode(Data,FileNumber,Code);
    for (int I=0;I<RecVolNumber;I++)
      OutBuf[I*RecBufferSize+BufPos]=Code[I];
  }
}


bool RecVolumes3::Restore(RAROptions *Cmd,const wchar *Name,bool Silent)
{
  wchar ArcName[NM];
  wcscpy(ArcName,Name);
  wchar *Ext=GetExt(ArcName);
  bool NewStyle=false;
  bool RevName=Ext!=NULL && wcsicomp(Ext,L".rev")==0;
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
    wcscpy(Ext,L"*.*");
    
    FindFile Find;
    Find.SetMask(ArcName);
    FindData fd;
    while (Find.Next(&fd))
    {
      Archive Arc(Cmd);
      if (Arc.WOpen(fd.Name) && Arc.IsArchive(true))
      {
        wcscpy(ArcName,fd.Name);
        break;
      }
    }
  }

  Archive Arc(Cmd);
  if (!Arc.WCheckOpen(ArcName))
    return false;
  if (!Arc.Volume)
  {
#ifndef SILENT
    Log(ArcName,St(MNotVolume),ArcName);
#endif
    return false;
  }
  bool NewNumbering=Arc.NewNumbering;
  Arc.Close();

  wchar *VolNumStart=VolNameToFirstName(ArcName,ArcName,NewNumbering);
  wchar RecVolMask[NM];
  wcscpy(RecVolMask,ArcName);
  size_t BaseNamePartLength=VolNumStart-ArcName;
  wcscpy(RecVolMask+BaseNamePartLength,L"*.rev");

#ifndef SILENT
  int64 RecFileSize=0;
#endif

  // We cannot display "Calculating CRC..." message here, because we do not
  // know if we'll find any recovery volumes. We'll display it after finding
  // the first recovery volume.
  bool CalcCRCMessageDone=false;

  FindFile Find;
  Find.SetMask(RecVolMask);
  FindData RecData;
  int FileNumber=0,RecVolNumber=0,FoundRecVolumes=0,MissingVolumes=0;
  wchar PrevName[NM];
  while (Find.Next(&RecData))
  {
    wchar *CurName=RecData.Name;
    int P[3];
    if (!RevName && !NewStyle)
    {
      NewStyle=true;

      wchar *Dot=GetExt(CurName);
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
      
#ifndef SILENT
        mprintf(L"\n%s",CurName);
#endif

      File CurFile;
      CurFile.TOpen(CurName);
      CurFile.Seek(0,SEEK_END);
      int64 Length=CurFile.Tell();
      CurFile.Seek(Length-7,SEEK_SET);
      for (int I=0;I<3;I++)
        P[2-I]=CurFile.GetByte()+1;
      uint FileCRC=0;
      for (int I=0;I<4;I++)
        FileCRC|=CurFile.GetByte()<<(I*8);
      uint CalcCRC;
      CalcFileSum(&CurFile,&CalcCRC,NULL,Cmd->Threads,Length-4);
      if (FileCRC!=CalcCRC)
      {
#ifndef SILENT
        mprintf(St(MCRCFailed),CurName);
#endif
        continue;
      }
    }
    else
    {
      wchar *Dot=GetExt(CurName);
      if (Dot==NULL)
        continue;
      bool WrongParam=false;
      for (size_t I=0;I<ASIZE(P);I++)
      {
        do
        {
          Dot--;
        } while (IsDigit(*Dot) && Dot>=CurName+BaseNamePartLength);
        P[I]=atoiw(Dot+1);
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
    wcscpy(PrevName,CurName);
    File *NewFile=new File;
    NewFile->TOpen(CurName);
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

  wchar LastVolName[NM];
  *LastVolName=0;

  for (int CurArcNum=0;CurArcNum<FileNumber;CurArcNum++)
  {
    Archive *NewFile=new Archive(Cmd);
    bool ValidVolume=FileExist(ArcName);
    if (ValidVolume)
    {
      NewFile->TOpen(ArcName);
      ValidVolume=NewFile->IsArchive(false);
      if (ValidVolume)
      {
        while (NewFile->ReadHeader()!=0)
        {
          if (NewFile->GetHeaderType()==HEAD_ENDARC)
          {
#ifndef SILENT
            mprintf(L"\n%s",ArcName);
#endif
            if (NewFile->EndArcHead.DataCRC)
            {
              uint CalcCRC;
              CalcFileSum(NewFile,&CalcCRC,NULL,Cmd->Threads,NewFile->CurBlockPos);
              if (NewFile->EndArcHead.ArcDataCRC!=CalcCRC)
              {
                ValidVolume=false;
#ifndef SILENT
                mprintf(St(MCRCFailed),ArcName);
#endif
              }
            }
            break;
          }
          NewFile->SeekToNext();
        }
      }
      if (!ValidVolume)
      {
        NewFile->Close();
        wchar NewName[NM];
        wcscpy(NewName,ArcName);
        wcscat(NewName,L".bad");
#ifndef SILENT
        mprintf(St(MBadArc),ArcName);
        mprintf(St(MRenaming),ArcName,NewName);
#endif
        RenameFile(ArcName,NewName);
      }
      NewFile->Seek(0,SEEK_SET);
    }
    if (!ValidVolume)
    {
      // It is important to return 'false' instead of aborting here,
      // so if we are called from extraction, we will be able to continue
      // extracting. It may happen if .rar and .rev are on read-only disks
      // like CDs.
      if (!NewFile->Create(ArcName))
      {
        // We need to display the title of operation before the error message,
        // to make clear for user that create error is related to recovery 
        // volumes. This is why we cannot use WCreate call here. Title must be
        // before create error, not after that.
#ifndef SILENT
        mprintf(St(MReconstructing));
#endif
        ErrHandler.CreateErrorMsg(NULL,ArcName);
        return false;
      }

      WriteFlags[CurArcNum]=true;
      MissingVolumes++;

      if (CurArcNum==FileNumber-1)
        wcscpy(LastVolName,ArcName);

#ifndef SILENT
      mprintf(St(MAbsNextVol),ArcName);
#endif
    }
    SrcFile[CurArcNum]=(File*)NewFile;
    NextVolumeName(ArcName,ASIZE(ArcName),!NewNumbering);
  }

#ifndef SILENT
  mprintf(St(MRecVolMissing),MissingVolumes);
#endif

  if (MissingVolumes==0)
  {
#ifndef SILENT
    mprintf(St(MRecVolAllExist));
#endif
    return false;
  }

  if (MissingVolumes>FoundRecVolumes)
  {
#ifndef SILENT
    mprintf(St(MRecVolCannotFix));
#endif
    return false;
  }
#ifndef SILENT
  mprintf(St(MReconstructing));
#endif

  int TotalFiles=FileNumber+RecVolNumber;
  int Erasures[256],EraSize=0;

  for (int I=0;I<TotalFiles;I++)
    if (WriteFlags[I] || SrcFile[I]==NULL)
      Erasures[EraSize++]=I;

#ifndef SILENT
  int64 ProcessedSize=0;
#ifndef GUI
  int LastPercent=-1;
  mprintf(L"     ");
#endif
#endif
  // Size of per file buffer.
  size_t RecBufferSize=TotalBufferSize/TotalFiles;

#ifdef RAR_SMP
  uint ThreadNumber=Cmd->Threads;
  RSEncode rse[MaxPoolThreads];
#else
  uint ThreadNumber=1;
  RSEncode rse[1];
#endif
  for (uint I=0;I<ThreadNumber;I++)
    rse[I].Init(RecVolNumber);

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
        if ((size_t)ReadSize!=RecBufferSize)
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
      mprintf(L"\b\b\b\b%3d%%",CurPercent);
      LastPercent=CurPercent;
    }
    ProcessedSize+=MaxRead;
#endif

    int BlockStart=0;
    int BlockSize=MaxRead/ThreadNumber;
    if (BlockSize<0x100)
      BlockSize=MaxRead;
    
    for (uint CurThread=0;BlockStart<MaxRead;CurThread++)
    {
      // Last thread processes all left data including increasement
      // from rounding error.
      if (CurThread==ThreadNumber-1)
        BlockSize=MaxRead-BlockStart;

      RSEncode *curenc=rse+CurThread;
      curenc->Buf=&Buf[0];
      curenc->BufStart=BlockStart;
      curenc->BufEnd=BlockStart+BlockSize;
      curenc->FileNumber=TotalFiles;
      curenc->RecBufferSize=RecBufferSize;
      curenc->Erasures=Erasures;
      curenc->EraSize=EraSize;

#ifdef RAR_SMP
      if (ThreadNumber>1)
        RSThreadPool->AddTask(RSDecodeThread,(void*)curenc);
      else
        curenc->DecodeBuf();
#else
      curenc->DecodeBuf();
#endif

      BlockStart+=BlockSize;
    }

#ifdef RAR_SMP
    RSThreadPool->WaitDone();
#endif // RAR_SMP
    
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
  if (*LastVolName!=0)
  {
    // Truncate the last volume to its real size.
    Archive Arc(Cmd);
    if (Arc.Open(LastVolName,FMF_UPDATE) && Arc.IsArchive(true) &&
        Arc.SearchBlock(HEAD_ENDARC))
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
    mprintf(L"\b\b\b\b100%%");
  if (!Silent && !Cmd->DisableDone)
    mprintf(St(MDone));
#endif
  return true;
}


void RSEncode::DecodeBuf()
{
  for (int BufPos=BufStart;BufPos<BufEnd;BufPos++)
  {
    byte Data[256];
    for (int I=0;I<FileNumber;I++)
      Data[I]=Buf[I*RecBufferSize+BufPos];
    RSC.Decode(Data,FileNumber,Erasures,EraSize);
    for (int I=0;I<EraSize;I++)
      Buf[Erasures[I]*RecBufferSize+BufPos]=Data[Erasures[I]];
  }
}
