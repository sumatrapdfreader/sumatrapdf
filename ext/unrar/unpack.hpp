#ifndef _RAR_UNPACK_
#define _RAR_UNPACK_

// Maximum allowed number of compressed bits processed in quick mode.
#define MAX_QUICK_DECODE_BITS      10

// Maximum number of filters per entire data block.
#define MAX_UNPACK_FILTERS       8192

// Maximum number of filters per entire data block for RAR3 unpack.
#define MAX3_FILTERS             1024

// Write data in 4 MB or smaller blocks.
#define UNPACK_MAX_WRITE     0x400000

// Decode compressed bit fields to alphabet numbers.
struct DecodeTable
{
  // Real size of DecodeNum table.
  uint MaxNum;

  // Left aligned start and upper limit codes defining code space 
  // ranges for bit lengths. DecodeLen[BitLength-1] defines the start of
  // range for bit length and DecodeLen[BitLength] defines next code
  // after the end of range or in other words the upper limit code
  // for specified bit length.
  uint DecodeLen[16]; 

  // Every item of this array contains the sum of all preceding items.
  // So it contains the start position in code list for every bit length. 
  uint DecodePos[16];

  // Number of compressed bits processed in quick mode.
  // Must not exceed MAX_QUICK_DECODE_BITS.
  uint QuickBits;

  // Translates compressed bits (up to QuickBits length)
  // to bit length in quick mode.
  byte QuickLen[1<<MAX_QUICK_DECODE_BITS];

  // Translates compressed bits (up to QuickBits length)
  // to position in alphabet in quick mode.
  // 'ushort' saves some memory and even provides a little speed gain
  // comparting to 'uint' here.
  ushort QuickNum[1<<MAX_QUICK_DECODE_BITS];

  // Translate the position in code list to position in alphabet.
  // We do not allocate it dynamically to avoid performance overhead
  // introduced by pointer, so we use the largest possible table size
  // as array dimension. Real size of this array is defined in MaxNum.
  // We use this array if compressed bit field is too lengthy
  // for QuickLen based translation.
  // 'ushort' saves some memory and even provides a little speed gain
  // comparting to 'uint' here.
  ushort DecodeNum[LARGEST_TABLE_SIZE];
};


struct UnpackBlockHeader
{
  int BlockSize;
  int BlockBitSize;
  int BlockStart;
  int HeaderSize;
  bool LastBlockInFile;
  bool TablePresent;
};


struct UnpackBlockTables
{
  DecodeTable LD;  // Decode literals.
  DecodeTable DD;  // Decode distances.
  DecodeTable LDD; // Decode lower bits of distances.
  DecodeTable RD;  // Decode repeating distances.
  DecodeTable BD;  // Decode bit lengths in Huffman table.
};


#ifdef RAR_SMP
enum UNP_DEC_TYPE {
  UNPDT_LITERAL,UNPDT_MATCH,UNPDT_FULLREP,UNPDT_REP,UNPDT_FILTER
};

struct UnpackDecodedItem
{
  UNP_DEC_TYPE Type;
  ushort Length;
  union
  {
    uint Distance;
    byte Literal[4];
  };
};


struct UnpackThreadData
{
  Unpack *UnpackPtr;
  BitInput Inp;
  bool HeaderRead;
  UnpackBlockHeader BlockHeader;
  bool TableRead;
  UnpackBlockTables BlockTables;
  int DataSize;    // Data left in buffer. Can be less than block size.
  bool DamagedData;
  bool LargeBlock;
  bool NoDataLeft; // 'true' if file is read completely.
  bool Incomplete; // Not entire block was processed, need to read more data.

  UnpackDecodedItem *Decoded;
  uint DecodedSize;
  uint DecodedAllocated;
  uint ThreadNumber; // For debugging.

  UnpackThreadData()
  :Inp(false)
  {
    Decoded=NULL;
  }
  ~UnpackThreadData()
  {
    if (Decoded!=NULL)
      free(Decoded);
  }
};
#endif


struct UnpackFilter
{
  byte Type;
  uint BlockStart;
  uint BlockLength;
  byte Channels;
  uint Width;
  byte PosR;
  bool NextWindow;
};


struct UnpackFilter30
{
  unsigned int BlockStart;
  unsigned int BlockLength;
  unsigned int ExecCount;
  bool NextWindow;

  // position of parent filter in Filters array used as prototype for filter
  // in PrgStack array. Not defined for filters in Filters array.
  unsigned int ParentFilter;

  VM_PreparedProgram Prg;
};


struct AudioVariables // For RAR 2.0 archives only.
{
  int K1,K2,K3,K4,K5;
  int D1,D2,D3,D4;
  int LastDelta;
  unsigned int Dif[11];
  unsigned int ByteCount;
  int LastChar;
};


// We can use the fragmented dictionary in case heap does not have the single
// large enough memory block. It is slower than normal dictionary.
class FragmentedWindow
{
  private:
    enum {MAX_MEM_BLOCKS=32};
    byte *Mem[MAX_MEM_BLOCKS];
    size_t MemSize[MAX_MEM_BLOCKS];
  public:
    FragmentedWindow();
    ~FragmentedWindow();
    void Init(size_t WinSize);
    byte& operator [](size_t Item);
    void CopyString(uint Length,uint Distance,size_t &UnpPtr,size_t MaxWinMask);
    void CopyData(byte *Dest,size_t WinPos,size_t Size);
    size_t GetBlockSize(size_t StartPos,size_t RequiredSize);
};


class Unpack
{
  private:

    void Unpack5(bool Solid);
    void Unpack5MT(bool Solid);
    bool UnpReadBuf();
    void UnpWriteBuf();
    uint FilterItanium_GetBits(byte *Data,int BitPos,int BitCount);
    void FilterItanium_SetBits(byte *Data,uint BitField,int BitPos,int BitCount);
    byte* ApplyFilter(byte *Data,uint DataSize,UnpackFilter *Flt);
    void UnpWriteArea(size_t StartPtr,size_t EndPtr);
    void UnpWriteData(byte *Data,size_t Size);
    _forceinline uint SlotToLength(BitInput &Inp,uint Slot);
    bool ReadBlockHeader(BitInput &Inp,UnpackBlockHeader &Header);
    bool ReadTables(BitInput &Inp,UnpackBlockHeader &Header,UnpackBlockTables &Tables);
    void MakeDecodeTables(byte *LengthTable,DecodeTable *Dec,uint Size);
    _forceinline uint DecodeNumber(BitInput &Inp,DecodeTable *Dec);
    void CopyString();
    inline void InsertOldDist(unsigned int Distance);
    void UnpInitData(bool Solid);
    _forceinline void CopyString(uint Length,uint Distance);
    uint ReadFilterData(BitInput &Inp);
    bool ReadFilter(BitInput &Inp,UnpackFilter &Filter);
    bool AddFilter(UnpackFilter &Filter);
    bool AddFilter();
    void InitFilters();

    ComprDataIO *UnpIO;
    BitInput Inp;

#ifdef RAR_SMP
    void InitMT();
    bool UnpackLargeBlock(UnpackThreadData &D);
    bool ProcessDecoded(UnpackThreadData &D);

    ThreadPool *UnpThreadPool;
    UnpackThreadData *UnpThreadData;
    uint MaxUserThreads;
    byte *ReadBufMT;
#endif

    Array<byte> FilterSrcMemory;
    Array<byte> FilterDstMemory;

    // Filters code, one entry per filter.
    Array<UnpackFilter> Filters;

    uint OldDist[4],OldDistPtr;
    uint LastLength;

    // LastDist is necessary only for RAR2 and older with circular OldDist
    // array. In RAR3 last distance is always stored in OldDist[0].
    uint LastDist;

    size_t UnpPtr,WrPtr;
    
    // Top border of read packed data.
    int ReadTop; 

    // Border to call UnpReadBuf. We use it instead of (ReadTop-C)
    // for optimization reasons. Ensures that we have C bytes in buffer
    // unless we are at the end of file.
    int ReadBorder;

    UnpackBlockHeader BlockHeader;
    UnpackBlockTables BlockTables;

    size_t WriteBorder;

    byte *Window;

    FragmentedWindow FragWindow;
    bool Fragmented;


    int64 DestUnpSize;

    bool Suspended;
    bool UnpAllBuf;
    bool UnpSomeRead;
    int64 WrittenFileSize;
    bool FileExtracted;


/***************************** Unpack v 1.5 *********************************/
    void Unpack15(bool Solid);
    void ShortLZ();
    void LongLZ();
    void HuffDecode();
    void GetFlagsBuf();
    void UnpInitData15(int Solid);
    void InitHuff();
    void CorrHuff(ushort *CharSet,byte *NumToPlace);
    void CopyString15(uint Distance,uint Length);
    uint DecodeNum(uint Num,uint StartPos,uint *DecTab,uint *PosTab);

    ushort ChSet[256],ChSetA[256],ChSetB[256],ChSetC[256];
    byte NToPl[256],NToPlB[256],NToPlC[256];
    uint FlagBuf,AvrPlc,AvrPlcB,AvrLn1,AvrLn2,AvrLn3;
    int Buf60,NumHuf,StMode,LCount,FlagsCnt;
    uint Nhfb,Nlzb,MaxDist3;
/***************************** Unpack v 1.5 *********************************/

/***************************** Unpack v 2.0 *********************************/
    void Unpack20(bool Solid);

    DecodeTable MD[4]; // Decode multimedia data, up to 4 channels.

    unsigned char UnpOldTable20[MC20*4];
    int UnpAudioBlock,UnpChannels,UnpCurChannel,UnpChannelDelta;
    void CopyString20(uint Length,uint Distance);
    bool ReadTables20();
    void UnpWriteBuf20();
    void UnpInitData20(int Solid);
    void ReadLastTables();
    byte DecodeAudio(int Delta);
    struct AudioVariables AudV[4];
/***************************** Unpack v 2.0 *********************************/

/***************************** Unpack v 3.0 *********************************/
    enum BLOCK_TYPES {BLOCK_LZ,BLOCK_PPM};

    void UnpInitData30(bool Solid);
    void Unpack29(bool Solid);
    void InitFilters30();
    bool ReadEndOfBlock();
    bool ReadVMCode();
    bool ReadVMCodePPM();
    bool AddVMCode(uint FirstByte,byte *Code,int CodeSize);
    int SafePPMDecodeChar();
    bool ReadTables30();
    bool UnpReadBuf30();
    void UnpWriteBuf30();
    void ExecuteCode(VM_PreparedProgram *Prg);

    int PrevLowDist,LowDistRepCount;

    ModelPPM PPM;
    int PPMEscChar;

    byte UnpOldTable[HUFF_TABLE_SIZE];
    int UnpBlockType;

    bool TablesRead;

    // Virtual machine to execute filters code.
    RarVM VM;
  
    // Buffer to read VM filters code. We moved it here from AddVMCode
    // function to reduce time spent in BitInput constructor.
    BitInput VMCodeInp;

    // Filters code, one entry per filter.
    Array<UnpackFilter30 *> Filters30;

    // Filters stack, several entrances of same filter are possible.
    Array<UnpackFilter30 *> PrgStack;

    // Lengths of preceding data blocks, one length of one last block
    // for every filter. Used to reduce the size required to write
    // the data block length if lengths are repeating.
    Array<int> OldFilterLengths;

    int LastFilter;
/***************************** Unpack v 3.0 *********************************/

  public:
    Unpack(ComprDataIO *DataIO);
    ~Unpack();
    void Init(size_t WinSize,bool Solid);
    void DoUnpack(int Method,bool Solid);
    bool IsFileExtracted() {return(FileExtracted);}
    void SetDestSize(int64 DestSize) {DestUnpSize=DestSize;FileExtracted=false;}
    void SetSuspended(bool Suspended) {Unpack::Suspended=Suspended;}

#ifdef RAR_SMP
    // More than 8 threads are unlikely to provide a noticeable gain
    // for unpacking, but would use the additional memory.
    void SetThreads(uint Threads) {MaxUserThreads=Min(Threads,8);}

    void UnpackDecode(UnpackThreadData &D);
#endif

    size_t MaxWinSize;
    size_t MaxWinMask;

    uint GetChar()
    {
      if (Inp.InAddr>BitInput::MAX_SIZE-30)
        UnpReadBuf();
      return(Inp.InBuf[Inp.InAddr++]);
    }
};

#endif
