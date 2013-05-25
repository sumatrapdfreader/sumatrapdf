#include "rar.hpp"

static const uint MaxVolumes=65535;

RecVolumes5::RecVolumes5()
{
  RealReadBuffer=NULL;

  DataCount=0;
  RecCount=0;
  TotalCount=0;
  RecBufferSize=0;

  for (uint I=0;I<ASIZE(ThreadData);I++)
  {
    ThreadData[I].RecRSPtr=this;
    ThreadData[I].RS=NULL;
  }
#ifdef RAR_SMP
  RecThreadPool=CreateThreadPool();
#endif

  RealBuf=NULL; // Might be needed in case of exception.
  RealBuf=new byte[TotalBufferSize+SSE_ALIGNMENT];
  Buf=(byte *)ALIGN_VALUE(RealBuf,SSE_ALIGNMENT);
}


RecVolumes5::~RecVolumes5()
{
  delete[] RealBuf;
  delete[] RealReadBuffer;
  for (uint I=0;I<RecItems.Size();I++)
    delete RecItems[I].f;
  for (uint I=0;I<ASIZE(ThreadData);I++)
    delete ThreadData[I].RS;
#ifdef RAR_SMP
  DestroyThreadPool(RecThreadPool);
#endif
}




#ifdef RAR_SMP
THREAD_PROC(RecThreadRS)
{
  RecRSThreadData *td=(RecRSThreadData *)Data;
  td->RecRSPtr->ProcessAreaRS(td);
}
#endif


void RecVolumes5::ProcessRS(RAROptions *Cmd,uint DataNum,const byte *Data,uint MaxRead,bool Encode)
{
/*
  RSCoder16 RS;
  RS.Init(DataCount,RecCount,Encode ? NULL:ValidFlags);
  uint Count=Encode ? RecCount : MissingVolumes;
  for (uint I=0;I<Count;I++)
    RS.UpdateECC(DataNum, I, Data, Buf+I*RecBufferSize, MaxRead);
*/

#ifdef RAR_SMP
  uint ThreadNumber=Cmd->Threads;
#else
  uint ThreadNumber=1;
#endif

  const uint MinThreadBlock=0x1000;
  ThreadNumber=Min(ThreadNumber,MaxRead/MinThreadBlock);

  if (ThreadNumber<1)
    ThreadNumber=1;
  uint ThreadDataSize=MaxRead/ThreadNumber;
  ThreadDataSize+=(ThreadDataSize&1); // Must be even for 16-bit RS coder.
#ifdef USE_SSE
  ThreadDataSize=ALIGN_VALUE(ThreadDataSize,SSE_ALIGNMENT); // Alignment for SSE operations.
#endif
  if (ThreadDataSize<MinThreadBlock)
    ThreadDataSize=MinThreadBlock;

  for (size_t I=0,CurPos=0;I<ThreadNumber && CurPos<MaxRead;I++)
  {
    RecRSThreadData *td=ThreadData+I;
    if (td->RS==NULL)
    {
      td->RS=new RSCoder16;
      td->RS->Init(DataCount,RecCount,Encode ? NULL:ValidFlags);
    }
    td->DataNum=DataNum;
    td->Data=Data;
    td->Encode=Encode;
    td->StartPos=CurPos;

    size_t EndPos=CurPos+ThreadDataSize;
    if (EndPos>MaxRead || I==ThreadNumber-1)
      EndPos=MaxRead;

    td->Size=EndPos-CurPos;

    CurPos=EndPos;

#ifdef RAR_SMP
    if (ThreadNumber>1)
      RecThreadPool->AddTask(RecThreadRS,(void*)td);
    else
      ProcessAreaRS(td);
#else
    ProcessAreaRS(td);
#endif
  }
#ifdef RAR_SMP
    RecThreadPool->WaitDone();
#endif // RAR_SMP
}


void RecVolumes5::ProcessAreaRS(RecRSThreadData *td)
{
  uint Count=td->Encode ? RecCount : MissingVolumes;
  for (uint I=0;I<Count;I++)
    td->RS->UpdateECC(td->DataNum, I, td->Data+td->StartPos, Buf+I*RecBufferSize+td->StartPos, td->Size);
}




bool RecVolumes5::Restore(RAROptions *Cmd,const wchar *Name,bool Silent)
{
  wchar ArcName[NM];
  wcscpy(ArcName,Name);

  wchar *Num=GetVolNumPart(ArcName);
  while (Num>ArcName && IsDigit(*(Num-1)))
    Num--;
  wcsncpyz(Num,L"*.*",ASIZE(ArcName)-(Num-ArcName));
  
  wchar FirstVolName[NM];
  *FirstVolName=0;

  int64 RecFileSize=0;

  FindFile VolFind;
  VolFind.SetMask(ArcName);
  FindData fd;
  uint FoundRecVolumes=0;
  while (VolFind.Next(&fd))
  {
    Wait();

    Archive *Vol=new Archive(Cmd);
    int ItemPos=-1;
    if (Vol->WOpen(fd.Name))
    {
      if (CmpExt(fd.Name,L"rev"))
      {
        uint RecNum=ReadHeader(Vol,FoundRecVolumes==0);
        if (RecNum!=0)
        {
          if (FoundRecVolumes==0)
            RecFileSize=Vol->FileLength();

          ItemPos=RecNum;
          FoundRecVolumes++;
        }
      }
      else
        if (Vol->IsArchive(true) && (Vol->SFXSize>0 || CmpExt(fd.Name,L"rar")))
        {
          if (!Vol->Volume && !Vol->BrokenHeader)
          {
            Log(ArcName,St(MNotVolume),ArcName);
            return false;
          }
          // We work with archive as with raw data file, so we do not want
          // to spend time to QOpen I/O redirection.
          Vol->QOpenUnload();
      
          Vol->Seek(0,SEEK_SET);

          // RAR volume found. Get its number, store the handle in appropriate
          // array slot, clean slots in between if we had to grow the array.
          wchar *Num=GetVolNumPart(fd.Name);
          uint VolNum=0;
          for (uint K=1;Num>=fd.Name && IsDigit(*Num);K*=10,Num--)
            VolNum+=(*Num-'0')*K;
          if (VolNum==0 || VolNum>MaxVolumes)
            continue;
          size_t CurSize=RecItems.Size();
          if (VolNum>CurSize)
          {
            RecItems.Alloc(VolNum);
            for (size_t I=CurSize;I<VolNum;I++)
              RecItems[I].f=NULL;
          }
          ItemPos=VolNum-1;

          if (*FirstVolName==0)
            VolNameToFirstName(fd.Name,FirstVolName,true);
        }
    }
    if (ItemPos==-1)
      delete Vol; // Skip found file, it is not RAR or REV volume.
    else
      if ((uint)ItemPos<RecItems.Size()) // Check if found more REV than needed.
      {
        // Store found RAR or REV volume.
        RecVolItem *Item=RecItems+ItemPos;
        Item->f=Vol;
        Item->New=false;
        wcsncpyz(Item->Name,fd.Name,ASIZE(Item->Name));
      }
  }

#ifndef SILENT
  if (!Silent || FoundRecVolumes!=0)
  {
    mprintf(St(MRecVolFound),FoundRecVolumes);
  }
#endif
  if (FoundRecVolumes==0)
    return false;

  mprintf(St(MCalcCRCAllVol));

  MissingVolumes=0;
  for (uint I=0;I<TotalCount;I++)
  {
    RecVolItem *Item=&RecItems[I];
    if (Item->f!=NULL)
    {
      mprintf(L"\n%s",Item->Name);
      uint RevCRC;
      CalcFileSum(Item->f,&RevCRC,NULL,Cmd->Threads,INT64NDF,CALCFSUM_CURPOS);
      Item->Valid=RevCRC==Item->CRC;
      if (!Item->Valid)
      {
        mprintf(St(MCRCFailed),Item->Name);
        // Close only corrupt REV volumes here. We'll close and rename corrupt
        // RAR volumes later, if we'll know that recovery is possible.
        if (I>=DataCount)
        {
          Item->f->Close();
          Item->f=NULL;
          FoundRecVolumes--;
        }
      }
    }
    if (I<DataCount && (Item->f==NULL || !Item->Valid))
      MissingVolumes++;
  }

  mprintf(St(MRecVolMissing),MissingVolumes);

  if (MissingVolumes==0)
  {
    mprintf(St(MRecVolAllExist));
    return false;
  }

  if (MissingVolumes>FoundRecVolumes)
  {
    mprintf(St(MRecVolCannotFix));
    return false;
  }

  mprintf(St(MReconstructing));

  // Create missing and rename bad volumes.
  for (uint I=0;I<DataCount;I++)
  {
    RecVolItem *Item=&RecItems[I];
    if (Item->f!=NULL && !Item->Valid)
    {
      Item->f->Close();

      wchar NewName[NM];
      wcscpy(NewName,Item->Name);
      wcscat(NewName,L".bad");
#ifndef SILENT
      mprintf(St(MBadArc),Item->Name);
      mprintf(St(MRenaming),Item->Name,NewName);
#endif
      RenameFile(Item->Name,NewName);
      delete Item->f;
      Item->f=NULL;
    }

    if (Item->New=(Item->f==NULL))
    {
      wcsncpyz(Item->Name,FirstVolName,ASIZE(Item->Name));
      mprintf(St(MCreating),Item->Name);
      File *NewVol=new File;
      bool UserReject;
      if (!FileCreate(Cmd,NewVol,Item->Name,ASIZE(Item->Name),Cmd->Overwrite,&UserReject))
      {
        if (!UserReject)
          ErrHandler.CreateErrorMsg(NULL,Item->Name);
        ErrHandler.Exit(UserReject ? RARX_USERBREAK:RARX_CREATE);
      }
      NewVol->Prealloc(Item->FileSize);
      Item->f=NewVol;
      Item->New=true;
    }
    NextVolumeName(FirstVolName,ASIZE(FirstVolName),false);
  }


  int64 ProcessedSize=0;
#ifndef GUI
  int LastPercent=-1;
  mprintf(L"     ");
#endif

  // Even though we already preliminary calculated missing volume number,
  // let's do it again now, when we have the final and exact information.
  MissingVolumes=0;

  ValidFlags=new bool[TotalCount];
  for (uint I=0;I<TotalCount;I++)
  {
    ValidFlags[I]=RecItems[I].f!=NULL && !RecItems[I].New;
    if (I<DataCount && !ValidFlags[I])
      MissingVolumes++;
  }

  // Size of per file buffer.
  RecBufferSize=TotalBufferSize/MissingVolumes;
  if ((RecBufferSize&1)==1) // Must be even for our RS16 codec.
    RecBufferSize--;
#ifdef USE_SSE
  RecBufferSize&=~(SSE_ALIGNMENT-1); // Align for SSE.
#endif

  uint *Data=new uint[TotalCount];

  RSCoder16 RS;
  if (!RS.Init(DataCount,RecCount,ValidFlags))
    return false; // Should not happen, we check parameter validity above.

  RealReadBuffer=new byte[RecBufferSize+SSE_ALIGNMENT];
  byte *ReadBuf=(byte *)ALIGN_VALUE(RealReadBuffer,SSE_ALIGNMENT);

  while (true)
  {
    Wait();

    int MaxRead=0;
    for (uint I=0,J=DataCount;I<DataCount;I++)
    {
      uint VolNum=I;
      if (!ValidFlags[I]) // If next RAR volume is missing or invalid.
      {
        while (!ValidFlags[J]) // Find next valid REV volume.
          J++;
        VolNum=J++; // Use next valid REV volume data instead of RAR.
      }
      RecVolItem *Item=RecItems+VolNum;

      byte *B=&ReadBuf[0];
      int ReadSize=0;
      if (Item->f!=NULL && !Item->New)
        ReadSize=Item->f->Read(B,RecBufferSize);
      if (ReadSize!=RecBufferSize)
        memset(B+ReadSize,0,RecBufferSize-ReadSize);
      if (ReadSize>MaxRead)
        MaxRead=ReadSize;

      ProcessRS(Cmd,I,B,MaxRead,false);
    }
    if (MaxRead==0)
      break;

    for (uint I=0,J=0;I<DataCount;I++)
      if (!ValidFlags[I])
      {
        RecVolItem *Item=RecItems+I;
        size_t WriteSize=(size_t)Min(MaxRead,Item->FileSize);
        Item->f->Write(Buf+(J++)*RecBufferSize,WriteSize);
        Item->FileSize-=WriteSize;
      }

#ifndef SILENT
    int CurPercent=ToPercent(ProcessedSize,RecFileSize);
    if (!Cmd->DisablePercentage && CurPercent!=LastPercent)
    {
      mprintf(L"\b\b\b\b%3d%%",CurPercent);
      LastPercent=CurPercent;
    }
    ProcessedSize+=MaxRead;
#endif
  }

  for (uint I=0;I<TotalCount;I++)
    if (RecItems[I].f!=NULL)
      RecItems[I].f->Close();

  delete[] ValidFlags;
  delete[] Data;
#if !defined(GUI) && !defined(SILENT)
  if (!Cmd->DisablePercentage)
    mprintf(L"\b\b\b\b100%%");
  if (!Silent && !Cmd->DisableDone)
    mprintf(St(MDone));
#endif
  return(true);
}


uint RecVolumes5::ReadHeader(File *RecFile,bool FirstRev)
{
  const size_t FirstReadSize=REV5_SIGN_SIZE+8;
  byte ShortBuf[FirstReadSize];
  if (RecFile->Read(ShortBuf,FirstReadSize)!=FirstReadSize)
    return 0;
  if (memcmp(ShortBuf,REV5_SIGN,REV5_SIGN_SIZE)!=0)
    return 0;
  uint HeaderSize=RawGet4(ShortBuf+REV5_SIGN_SIZE+4);
  if (HeaderSize>0x100000 || HeaderSize<=5)
    return 0;
  uint BlockCRC=RawGet4(ShortBuf+REV5_SIGN_SIZE);

  RawRead Raw(RecFile);
  if (Raw.Read(HeaderSize)!=HeaderSize)
    return 0;

  // Calculate CRC32 of entire header including 4 byte size field.
  uint CalcCRC=CRC32(0xffffffff,ShortBuf+REV5_SIGN_SIZE+4,4);
  if ((CRC32(CalcCRC,Raw.GetDataPtr(),HeaderSize)^0xffffffff)!=BlockCRC)
    return 0;

  if (Raw.Get1()!=1) // Version check.
    return 0;
  DataCount=Raw.Get2();
  RecCount=Raw.Get2();
  TotalCount=DataCount+RecCount;
  uint RecNum=Raw.Get2(); // Number of recovery volume.
  if (RecNum>=TotalCount || TotalCount>MaxVolumes)
    return 0;
  uint RevCRC=Raw.Get4(); // CRC of current REV volume.

  if (FirstRev)
  {
    // If we have read the first valid REV file, init data structures
    // using information from REV header.
    size_t CurSize=RecItems.Size();
    RecItems.Alloc(TotalCount);
    for (size_t I=CurSize;I<TotalCount;I++)
      RecItems[I].f=NULL;
    for (uint I=0;I<DataCount;I++)
    {
      RecItems[I].FileSize=Raw.Get8();
      RecItems[I].CRC=Raw.Get4();
    }
  }

  RecItems[RecNum].CRC=RevCRC; // Assign it here, after allocating RecItems.

  return RecNum;
}
