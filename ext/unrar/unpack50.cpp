void Unpack::Unpack5(bool Solid)
{
  FileExtracted=true;

  if (!Suspended)
  {
    UnpInitData(Solid);
    if (!UnpReadBuf())
      return;
    if (!ReadBlockHeader(Inp,BlockHeader) || !ReadTables(Inp,BlockHeader,BlockTables))
      return;
  }

  while (true)
  {
    UnpPtr&=MaxWinMask;

    if (Inp.InAddr>=ReadBorder)
    {
      bool FileDone=false;

      // We use 'while', because for empty block containing only Huffman table,
      // we'll be on the block border once again just after reading the table.
      while (Inp.InAddr>BlockHeader.BlockStart+BlockHeader.BlockSize-1 || 
             Inp.InAddr==BlockHeader.BlockStart+BlockHeader.BlockSize-1 && 
             Inp.InBit>=BlockHeader.BlockBitSize)
      {
        if (BlockHeader.LastBlockInFile)
        {
          FileDone=true;
          break;
        }
        if (!ReadBlockHeader(Inp,BlockHeader) || !ReadTables(Inp,BlockHeader,BlockTables))
          return;
      }
      if (FileDone || !UnpReadBuf())
        break;
    }

    if (((WriteBorder-UnpPtr) & MaxWinMask)<MAX_LZ_MATCH+3 && WriteBorder!=UnpPtr)
    {
      UnpWriteBuf();
      if (WrittenFileSize>DestUnpSize)
        return;
      if (Suspended)
      {
        FileExtracted=false;
        return;
      }
    }

    uint MainSlot=DecodeNumber(Inp,&BlockTables.LD);
    if (MainSlot<256)
    {
      if (Fragmented)
        FragWindow[UnpPtr++]=(byte)MainSlot;
      else
        Window[UnpPtr++]=(byte)MainSlot;
      continue;
    }
    if (MainSlot>=262)
    {
      uint Length=SlotToLength(Inp,MainSlot-262);

      uint DBits,Distance=1,DistSlot=DecodeNumber(Inp,&BlockTables.DD);
      if (DistSlot<4)
      {
        DBits=0;
        Distance+=DistSlot;
      }
      else
      {
        DBits=DistSlot/2 - 1;
        Distance+=(2 | (DistSlot & 1)) << DBits;
      }

      if (DBits>0)
      {
        if (DBits>=4)
        {
          if (DBits>4)
          {
            Distance+=((Inp.getbits32()>>(36-DBits))<<4);
            Inp.addbits(DBits-4);
          }
          uint LowDist=DecodeNumber(Inp,&BlockTables.LDD);
          Distance+=LowDist;
        }
        else
        {
          Distance+=Inp.getbits32()>>(32-DBits);
          Inp.addbits(DBits);
        }
      }

      if (Distance>0x100)
      {
        Length++;
        if (Distance>0x2000)
        {
          Length++;
          if (Distance>0x40000)
            Length++;
        }
      }

      InsertOldDist(Distance);
      LastLength=Length;
      if (Fragmented)
        FragWindow.CopyString(Length,Distance,UnpPtr,MaxWinMask);
      else
        CopyString(Length,Distance);
      continue;
    }
    if (MainSlot==256)
    {
      UnpackFilter Filter;
      if (!ReadFilter(Inp,Filter) || !AddFilter(Filter))
        break;
      continue;
    }
    if (MainSlot==257)
    {
      if (LastLength!=0)
        if (Fragmented)
          FragWindow.CopyString(LastLength,OldDist[0],UnpPtr,MaxWinMask);
        else
          CopyString(LastLength,OldDist[0]);
      continue;
    }
    if (MainSlot<262)
    {
      uint DistNum=MainSlot-258;
      uint Distance=OldDist[DistNum];
      for (uint I=DistNum;I>0;I--)
        OldDist[I]=OldDist[I-1];
      OldDist[0]=Distance;

      uint LengthSlot=DecodeNumber(Inp,&BlockTables.RD);
      uint Length=SlotToLength(Inp,LengthSlot);
      LastLength=Length;
      if (Fragmented)
        FragWindow.CopyString(Length,Distance,UnpPtr,MaxWinMask);
      else
        CopyString(Length,Distance);
      continue;
    }
  }
  UnpWriteBuf();
}


uint Unpack::ReadFilterData(BitInput &Inp)
{
  uint ByteCount=(Inp.fgetbits()>>14)+1;
  Inp.addbits(2);

  uint Data=0;
  for (uint I=0;I<ByteCount;I++)
  {
    Data+=(Inp.fgetbits()>>8)<<(I*8);
    Inp.addbits(8);
  }
  return Data;
}


bool Unpack::ReadFilter(BitInput &Inp,UnpackFilter &Filter)
{
  if (!Inp.ExternalBuffer && Inp.InAddr>ReadTop-16)
    if (!UnpReadBuf())
      return false;

  Filter.BlockStart=ReadFilterData(Inp);
  Filter.BlockLength=ReadFilterData(Inp);

  Filter.Type=Inp.fgetbits()>>13;
  Inp.faddbits(3);

  if (Filter.Type==FILTER_DELTA || Filter.Type==FILTER_AUDIO)
  {
    Filter.Channels=(Inp.fgetbits()>>11)+1;
    Inp.faddbits(5);
  }

  if (Filter.Type==FILTER_RGB)
  {
    Filter.Channels=3;
    Filter.Width=Inp.fgetbits()+1;
    Inp.faddbits(16);
    Filter.PosR=Inp.fgetbits()>>14;
    Inp.faddbits(2);
  }

  return true;
}


bool Unpack::AddFilter(UnpackFilter &Filter)
{
  if (Filters.Size()>=MAX_UNPACK_FILTERS-1)
    UnpWriteBuf(); // Write data, apply and flush filters.

  // If distance to filter start is that large that due to circular dictionary
  // mode it points to old not written yet data, then we set 'NextWindow'
  // flag and process this filter only after processing that older data.
  Filter.NextWindow=WrPtr!=UnpPtr && ((WrPtr-UnpPtr)&MaxWinMask)<=Filter.BlockStart;

  Filter.BlockStart=uint((Filter.BlockStart+UnpPtr)&MaxWinMask);
  Filters.Push(Filter);
  return true;
}


bool Unpack::UnpReadBuf()
{
  int DataSize=ReadTop-Inp.InAddr; // Data left to process.
  if (DataSize<0)
    return false;
  BlockHeader.BlockSize-=Inp.InAddr-BlockHeader.BlockStart;
  if (Inp.InAddr>BitInput::MAX_SIZE/2)
  {
    // If we already processed more than half of buffer, let's move
    // remaining data into beginning to free more space for new data
    // and ensure that calling function does not cross the buffer border
    // even if we did not read anything here. Also it ensures that read size
    // is not less than CRYPT_BLOCK_SIZE, so we can align it without risk
    // to make it zero.
    if (DataSize>0)
      memmove(Inp.InBuf,Inp.InBuf+Inp.InAddr,DataSize);
    Inp.InAddr=0;
    ReadTop=DataSize;
  }
  else
    DataSize=ReadTop;
  int ReadCode=UnpIO->UnpRead(Inp.InBuf+DataSize,BitInput::MAX_SIZE-DataSize);
  if (ReadCode>0)
    ReadTop+=ReadCode;
  ReadBorder=ReadTop-30;
  BlockHeader.BlockStart=Inp.InAddr;
  if (BlockHeader.BlockSize!=-1) // '-1' means not defined yet.
    ReadBorder=Min(ReadBorder,BlockHeader.BlockStart+BlockHeader.BlockSize-1);
  return ReadCode!=-1;
}


void Unpack::UnpWriteBuf()
{
  size_t WrittenBorder=WrPtr;
  size_t FullWriteSize=(UnpPtr-WrittenBorder)&MaxWinMask;
  size_t WriteSizeLeft=FullWriteSize;
  bool NotAllFiltersProcessed=false;
  for (size_t I=0;I<Filters.Size();I++)
  {
    // Here we apply filters to data which we need to write.
    // We always copy data to virtual machine memory before processing.
    // We cannot process them just in place in Window buffer, because
    // these data can be used for future string matches, so we must
    // preserve them in original form.

    UnpackFilter *flt=&Filters[I];
    if (flt->Type==FILTER_NONE)
      continue;
    if (flt->NextWindow)
    {
      // Here we skip filters which have block start in current data range
      // due to address warp around in circular dictionary, but actually
      // belong to next dictionary block. If such filter start position
      // is included to current write range, then we reset 'NextWindow' flag.
      // In fact we can reset it even without such check, because current
      // implementation seems to guarantee 'NextWindow' flag reset after
      // buffer writing for all existing filters. But let's keep this check
      // just in case. Compressor guarantees that distance between
      // filter block start and filter storing position cannot exceed
      // the dictionary size. So if we covered the filter block start with
      // our write here, we can safely assume that filter is applicable
      // to next block on no further wrap arounds is possible.
      if (((flt->BlockStart-WrPtr)&MaxWinMask)<=FullWriteSize)
        flt->NextWindow=false;
      continue;
    }
    uint BlockStart=flt->BlockStart;
    uint BlockLength=flt->BlockLength;
    if (((BlockStart-WrittenBorder)&MaxWinMask)<WriteSizeLeft)
    {
      if (WrittenBorder!=BlockStart)
      {
        UnpWriteArea(WrittenBorder,BlockStart);
        WrittenBorder=BlockStart;
        WriteSizeLeft=(UnpPtr-WrittenBorder)&MaxWinMask;
      }
      if (BlockLength<=WriteSizeLeft)
      {
        if (BlockLength>0)
        {
          uint BlockEnd=(BlockStart+BlockLength)&MaxWinMask;

          FilterSrcMemory.Alloc(BlockLength);
          byte *Mem=&FilterSrcMemory[0];
          if (BlockStart<BlockEnd || BlockEnd==0)
          {
            if (Fragmented)
              FragWindow.CopyData(Mem,BlockStart,BlockLength);
            else
              memcpy(Mem,Window+BlockStart,BlockLength);
          }
          else
          {
            size_t FirstPartLength=size_t(MaxWinSize-BlockStart);
            if (Fragmented)
            {
              FragWindow.CopyData(Mem,BlockStart,FirstPartLength);
              FragWindow.CopyData(Mem+FirstPartLength,0,BlockEnd);
            }
            else
            {
              memcpy(Mem,Window+BlockStart,FirstPartLength);
              memcpy(Mem+FirstPartLength,Window,BlockEnd);
            }
          }

          byte *OutMem=ApplyFilter(Mem,BlockLength,flt);

          Filters[I].Type=FILTER_NONE;

          if (OutMem!=NULL)
            UnpIO->UnpWrite(OutMem,BlockLength);

          UnpSomeRead=true;
          WrittenFileSize+=BlockLength;
          WrittenBorder=BlockEnd;
          WriteSizeLeft=(UnpPtr-WrittenBorder)&MaxWinMask;
        }
      }
      else
      {
        // Current filter intersects the window write border, so we adjust
        // the window border to process this filter next time, not now.
        WrPtr=WrittenBorder;

        // Since Filter start position can only increase, we quit processing
        // all following filters for this data block and reset 'NextWindow'
        // flag for them.
        for (size_t J=I;J<Filters.Size();J++)
        {
          UnpackFilter *flt=&Filters[J];
          if (flt->Type!=FILTER_NONE)
            flt->NextWindow=false;
        }

        // Do not write data left after current filter now.
        NotAllFiltersProcessed=true;
        break;
      }
    }
  }

  // Remove processed filters from queue.
  size_t EmptyCount=0;
  for (size_t I=0;I<Filters.Size();I++)
  {
    if (EmptyCount>0)
      Filters[I-EmptyCount]=Filters[I];
    if (Filters[I].Type==FILTER_NONE)
      EmptyCount++;
  }
  if (EmptyCount>0)
    Filters.Alloc(Filters.Size()-EmptyCount);

  if (!NotAllFiltersProcessed) // Only if all filters are processed.
  {
    // Write data left after last filter.
    UnpWriteArea(WrittenBorder,UnpPtr);
    WrPtr=UnpPtr;
  }

  // We prefer to write data in blocks not exceeding UNPACK_MAX_WRITE
  // instead of potentially huge MaxWinSize blocks.
  WriteBorder=(UnpPtr+Min(MaxWinSize,UNPACK_MAX_WRITE))&MaxWinMask;

  // Choose the nearest among WriteBorder and WrPtr actual written border.
  // If border is equal to UnpPtr, it means that we have MaxWinSize data ahead.
  if (WriteBorder==UnpPtr || 
      WrPtr!=UnpPtr && ((WrPtr-UnpPtr)&MaxWinMask)<((WriteBorder-UnpPtr)&MaxWinMask))
    WriteBorder=WrPtr;
}


uint Unpack::FilterItanium_GetBits(byte *Data,int BitPos,int BitCount)
{
  int InAddr=BitPos/8;
  int InBit=BitPos&7;
  uint BitField=(uint)Data[InAddr++];
  BitField|=(uint)Data[InAddr++] << 8;
  BitField|=(uint)Data[InAddr++] << 16;
  BitField|=(uint)Data[InAddr] << 24;
  BitField >>= InBit;
  return(BitField & (0xffffffff>>(32-BitCount)));
}


void Unpack::FilterItanium_SetBits(byte *Data,uint BitField,int BitPos,int BitCount)
{
  int InAddr=BitPos/8;
  int InBit=BitPos&7;
  uint AndMask=0xffffffff>>(32-BitCount);
  AndMask=~(AndMask<<InBit);

  BitField<<=InBit;

  for (uint I=0;I<4;I++)
  {
    Data[InAddr+I]&=AndMask;
    Data[InAddr+I]|=BitField;
    AndMask=(AndMask>>8)|0xff000000;
    BitField>>=8;
  }
}


inline uint GetFiltData32(byte *Data)
{
#if defined(BIG_ENDIAN) || !defined(ALLOW_NOT_ALIGNED_INT) || !defined(PRESENT_INT32)
  uint Value=GET_UINT32((uint)Data[0]|((uint)Data[1]<<8)|((uint)Data[2]<<16)|((uint)Data[3]<<24));
#else
  uint Value=GET_UINT32(*(uint32 *)Data);
#endif
  return Value;
}


inline void SetFiltData32(byte *Data,uint Value)
{
#if defined(BIG_ENDIAN) || !defined(ALLOW_NOT_ALIGNED_INT) || !defined(PRESENT_INT32)
   Data[0]=(byte)Value;
   Data[1]=(byte)(Value>>8);
   Data[2]=(byte)(Value>>16);
   Data[3]=(byte)(Value>>24);
#else
   *(int32 *)Data=Value;
#endif
}


byte* Unpack::ApplyFilter(byte *Data,uint DataSize,UnpackFilter *Flt)
{
  byte *SrcData=Data;
  switch(Flt->Type)
  {
    case FILTER_E8:
    case FILTER_E8E9:
      {
        uint FileOffset=(uint)WrittenFileSize;

        const int FileSize=0x1000000;
        byte CmpByte2=Flt->Type==FILTER_E8E9 ? 0xe9:0xe8;
        for (uint CurPos=0;(int)CurPos<(int)DataSize-4;)
        {
          byte CurByte=*(Data++);
          CurPos++;
          if (CurByte==0xe8 || CurByte==CmpByte2)
          {
            uint Offset=(CurPos+FileOffset)%FileSize;
            uint Addr=GetFiltData32(Data);

            // We check 0x80000000 bit instead of '< 0' comparison
            // not assuming int32 presence or uint size and endianness.
            if ((Addr & 0x80000000)!=0)              // Addr<0
            {
              if (((Addr+Offset) & 0x80000000)==0)   // Addr+Offset>=0
                SetFiltData32(Data,Addr+FileSize);
            }
            else
              if (((Addr-FileSize) & 0x80000000)!=0) // Addr<FileSize
                SetFiltData32(Data,Addr-Offset);

            Data+=4;
            CurPos+=4;
          }
        }
      }
      return SrcData;
    case FILTER_ARM:
      {
        uint FileOffset=(uint)WrittenFileSize;
        for (uint CurPos=0;(int)CurPos<(int)DataSize-3;CurPos+=4)
        {
          byte *D=Data+CurPos;
          if (D[3]==0xeb) // BL command with '1110' (Always) condition.
          {
            uint Offset=D[0]+uint(D[1])*0x100+uint(D[2])*0x10000;
            Offset-=(FileOffset+CurPos)/4;
            D[0]=(byte)Offset;
            D[1]=(byte)(Offset>>8);
            D[2]=(byte)(Offset>>16);
          }
        }
      }
      return SrcData;
    case FILTER_ITANIUM:
      {
        uint FileOffset=(uint)WrittenFileSize;

        uint CurPos=0;

        FileOffset>>=4;

        while ((int)CurPos<(int)DataSize-21)
        {
          int Byte=(Data[0]&0x1f)-0x10;
          if (Byte>=0)
          {
            static byte Masks[16]={4,4,6,6,0,0,7,7,4,4,0,0,4,4,0,0};
            byte CmdMask=Masks[Byte];
            if (CmdMask!=0)
              for (int I=0;I<=2;I++)
                if (CmdMask & (1<<I))
                {
                  int StartPos=I*41+5;
                  int OpType=FilterItanium_GetBits(Data,StartPos+37,4);
                  if (OpType==5)
                  {
                    int Offset=FilterItanium_GetBits(Data,StartPos+13,20);
                    FilterItanium_SetBits(Data,(Offset-FileOffset)&0xfffff,StartPos+13,20);
                  }
                }
          }
          Data+=16;
          CurPos+=16;
          FileOffset++;
        }
      }
      return SrcData;
    case FILTER_AUDIO:
      {
        uint Channels=Flt->Channels;

        byte *SrcData=Data;

        FilterDstMemory.Alloc(DataSize);
        byte *DstData=&FilterDstMemory[0];
        
        for (uint CurChannel=0;CurChannel<Channels;CurChannel++)
        {
          uint PrevByte=0,PrevDelta=0,Dif[7];
          int D1=0,D2=0,D3;
          int K1=0,K2=0,K3=0;
          memset(Dif,0,sizeof(Dif));

          for (uint I=CurChannel,ByteCount=0;I<DataSize;I+=Channels,ByteCount++)
          {
            D3=D2;
            D2=PrevDelta-D1;
            D1=PrevDelta;

            uint Predicted=8*PrevByte+K1*D1+K2*D2+K3*D3;
            Predicted=(Predicted>>3) & 0xff;

            uint CurByte=*(SrcData++);

            Predicted-=CurByte;
            DstData[I]=Predicted;
            PrevDelta=(signed char)(Predicted-PrevByte);
            PrevByte=Predicted;

            int D=((signed char)CurByte)<<3;

            Dif[0]+=abs(D);
            Dif[1]+=abs(D-D1);
            Dif[2]+=abs(D+D1);
            Dif[3]+=abs(D-D2);
            Dif[4]+=abs(D+D2);
            Dif[5]+=abs(D-D3);
            Dif[6]+=abs(D+D3);

            if ((ByteCount & 0x1f)==0)
            {
              uint MinDif=Dif[0],NumMinDif=0;
              Dif[0]=0;
              for (uint J=1;J<ASIZE(Dif);J++)
              {
                if (Dif[J]<MinDif)
                {
                  MinDif=Dif[J];
                  NumMinDif=J;
                }
                Dif[J]=0;
              }
              switch(NumMinDif)
              {
                case 1: if (K1>=-16) K1--; break;
                case 2: if (K1 < 16) K1++; break;
                case 3: if (K2>=-16) K2--; break;
                case 4: if (K2 < 16) K2++; break;
                case 5: if (K3>=-16) K3--; break;
                case 6: if (K3 < 16) K3++; break;
              }
            }
          }
        }
        return DstData;
      }
    case FILTER_DELTA:
      {
        uint Channels=Flt->Channels,SrcPos=0;

        FilterDstMemory.Alloc(DataSize);
        byte *DstData=&FilterDstMemory[0];

        // Bytes from same channels are grouped to continual data blocks,
        // so we need to place them back to their interleaving positions.
        for (uint CurChannel=0;CurChannel<Channels;CurChannel++)
        {
          byte PrevByte=0;
          for (uint DestPos=CurChannel;DestPos<DataSize;DestPos+=Channels)
            DstData[DestPos]=(PrevByte-=Data[SrcPos++]);
        }
        return DstData;
      }
    case FILTER_RGB:
      {
        uint Width=Flt->Width,PosR=Flt->PosR;

        byte *SrcData=Data;

        FilterDstMemory.Alloc(DataSize);
        byte *DstData=&FilterDstMemory[0];
        
        const int Channels=3;

        for (uint CurChannel=0;CurChannel<Channels;CurChannel++)
        {
          uint PrevByte=0;

          for (uint I=CurChannel;I<DataSize;I+=Channels)
          {
            uint Predicted;
            int UpperPos=I-Width;
            if (UpperPos>=3)
            {
              byte *UpperData=DstData+UpperPos;
              uint UpperByte=*UpperData;
              uint UpperLeftByte=*(UpperData-3);
              Predicted=PrevByte+UpperByte-UpperLeftByte;
              int pa=abs((int)(Predicted-PrevByte));
              int pb=abs((int)(Predicted-UpperByte));
              int pc=abs((int)(Predicted-UpperLeftByte));
              if (pa<=pb && pa<=pc)
                Predicted=PrevByte;
              else
                if (pb<=pc)
                  Predicted=UpperByte;
                else
                  Predicted=UpperLeftByte;
            }
            else
              Predicted=PrevByte;
            DstData[I]=PrevByte=(byte)(Predicted-*(SrcData++));
          }
        }
        for (uint I=PosR,Border=DataSize-2;I<Border;I+=3)
        {
          byte G=DstData[I+1];
          DstData[I]+=G;
          DstData[I+2]+=G;
        }
        return DstData;
      }

  }
  return NULL;
}


void Unpack::UnpWriteArea(size_t StartPtr,size_t EndPtr)
{
  if (EndPtr!=StartPtr)
    UnpSomeRead=true;
  if (EndPtr<StartPtr)
    UnpAllBuf=true;

  if (Fragmented)
  {
    size_t SizeToWrite=(EndPtr-StartPtr) & MaxWinMask;
    while (SizeToWrite>0)
    {
      size_t BlockSize=FragWindow.GetBlockSize(StartPtr,SizeToWrite);
      UnpWriteData(&FragWindow[StartPtr],BlockSize);
      SizeToWrite-=BlockSize;
      StartPtr=(StartPtr+BlockSize) & MaxWinMask;
    }
  }
  else
    if (EndPtr<StartPtr)
    {
      UnpWriteData(Window+StartPtr,MaxWinSize-StartPtr);
      UnpWriteData(Window,EndPtr);
    }
    else
      UnpWriteData(Window+StartPtr,EndPtr-StartPtr);
}


void Unpack::UnpWriteData(byte *Data,size_t Size)
{
  if (WrittenFileSize>=DestUnpSize)
    return;
  size_t WriteSize=Size;
  int64 LeftToWrite=DestUnpSize-WrittenFileSize;
  if ((int64)WriteSize>LeftToWrite)
    WriteSize=(size_t)LeftToWrite;
  UnpIO->UnpWrite(Data,WriteSize);
  WrittenFileSize+=Size;
}


bool Unpack::ReadBlockHeader(BitInput &Inp,UnpackBlockHeader &Header)
{
  Header.HeaderSize=0;

  if (!Inp.ExternalBuffer && Inp.InAddr>ReadTop-7)
    if (!UnpReadBuf())
      return false;
  Inp.faddbits((8-Inp.InBit)&7);
  
  byte BlockFlags=Inp.fgetbits()>>8;
  Inp.faddbits(8);
  uint ByteCount=((BlockFlags>>3)&3)+1; // Block size byte count.
  
  if (ByteCount==4)
    return false;

  Header.HeaderSize=2+ByteCount;

  Header.BlockBitSize=(BlockFlags&7)+1;

  byte SavedCheckSum=Inp.fgetbits()>>8;
  Inp.faddbits(8);

  int BlockSize=0;
  for (uint I=0;I<ByteCount;I++)
  {
    BlockSize+=(Inp.fgetbits()>>8)<<(I*8);
    Inp.addbits(8);
  }

  Header.BlockSize=BlockSize;
  byte CheckSum=byte(0x5a^BlockFlags^BlockSize^(BlockSize>>8)^(BlockSize>>16));
  if (CheckSum!=SavedCheckSum)
    return false;

  Header.BlockStart=Inp.InAddr;
  ReadBorder=Min(ReadBorder,Header.BlockStart+Header.BlockSize-1);

  Header.LastBlockInFile=(BlockFlags & 0x40)!=0;
  Header.TablePresent=(BlockFlags & 0x80)!=0;
  return true;
}


bool Unpack::ReadTables(BitInput &Inp,UnpackBlockHeader &Header,UnpackBlockTables &Tables)
{
  if (!Header.TablePresent)
    return true;

  if (!Inp.ExternalBuffer && Inp.InAddr>ReadTop-25)
    if (!UnpReadBuf())
      return false;

  byte BitLength[BC];
  for (int I=0;I<BC;I++)
  {
    int Length=(byte)(Inp.fgetbits() >> 12);
    Inp.faddbits(4);
    if (Length==15)
    {
      int ZeroCount=(byte)(Inp.fgetbits() >> 12);
      Inp.faddbits(4);
      if (ZeroCount==0)
        BitLength[I]=15;
      else
      {
        ZeroCount+=2;
        while (ZeroCount-- > 0 && I<sizeof(BitLength)/sizeof(BitLength[0]))
          BitLength[I++]=0;
        I--;
      }
    }
    else
      BitLength[I]=Length;
  }

  MakeDecodeTables(BitLength,&Tables.BD,BC);

  byte Table[HUFF_TABLE_SIZE];
  const int TableSize=HUFF_TABLE_SIZE;
  for (int I=0;I<TableSize;)
  {
    if (!Inp.ExternalBuffer && Inp.InAddr>ReadTop-5)
      if (!UnpReadBuf())
        return(false);
    int Number=DecodeNumber(Inp,&Tables.BD);
    if (Number<16)
    {
      Table[I]=Number;
      I++;
    }
    else
      if (Number<18)
      {
        int N;
        if (Number==16)
        {
          N=(Inp.fgetbits() >> 13)+3;
          Inp.faddbits(3);
        }
        else
        {
          N=(Inp.fgetbits() >> 9)+11;
          Inp.faddbits(7);
        }
        if (I>0)
          while (N-- > 0 && I<TableSize)
          {
            Table[I]=Table[I-1];
            I++;
          }
      }
      else
      {
        int N;
        if (Number==18)
        {
          N=(Inp.fgetbits() >> 13)+3;
          Inp.faddbits(3);
        }
        else
        {
          N=(Inp.fgetbits() >> 9)+11;
          Inp.faddbits(7);
        }
        while (N-- > 0 && I<TableSize)
          Table[I++]=0;
      }
  }
  if (!Inp.ExternalBuffer && Inp.InAddr>ReadTop)
    return(false);
  MakeDecodeTables(&Table[0],&Tables.LD,NC);
  MakeDecodeTables(&Table[NC],&Tables.DD,DC);
  MakeDecodeTables(&Table[NC+DC],&Tables.LDD,LDC);
  MakeDecodeTables(&Table[NC+DC+LDC],&Tables.RD,RC);
  return(true);
}


void Unpack::InitFilters()
{
  Filters.Reset();
}
