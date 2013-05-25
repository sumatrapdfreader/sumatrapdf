#ifndef _RAR_QOPEN_
#define _RAR_QOPEN_

struct QuickOpenItem
{
  byte *Header;
  size_t HeaderSize;
  uint64 ArcPos;
  QuickOpenItem *Next;
};


class Archive;
class RawRead;

class QuickOpen
{
  private:
    void Close();


    uint ReadBuffer();
    bool ReadRaw(RawRead &Raw);
    bool ReadNext();

    Archive *Arc;
    bool WriteMode;

    QuickOpenItem *ListStart;
    QuickOpenItem *ListEnd;
    
    byte *Buf;
    static const size_t MaxBufSize=0x10000; // Must be multiple of CRYPT_BLOCK_SIZE.
    size_t CurBufSize;
#ifndef RAR_NOCRYPT // For shell extension.
    CryptData Crypt;
#endif

    bool Loaded;
    uint64 QLHeaderPos;
    uint64 RawDataStart;
    uint64 RawDataSize;
    uint64 RawDataPos;
    size_t ReadBufSize;
    size_t ReadBufPos;
    Array<byte> LastReadHeader;
    uint64 LastReadHeaderPos;
    uint64 SeekPos;
    bool UnsyncSeekPos; // QOpen SeekPos does not match an actual file pointer.
  public:
    QuickOpen();
    ~QuickOpen();
    void Init(Archive *Arc,bool WriteMode);
    void Load(uint64 BlockPos);
    void Unload() { Loaded=false; }
    bool Read(void *Data,size_t Size,size_t &Result);
    bool Seek(int64 Offset,int Method);
    bool Tell(int64 *Pos);
};

#endif
