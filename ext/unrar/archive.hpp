#ifndef _RAR_ARCHIVE_
#define _RAR_ARCHIVE_

class PPack;
class RawRead;
class RawWrite;

enum NOMODIFY_FLAGS 
{
  NMDF_ALLOWLOCK=1,NMDF_ALLOWANYVOLUME=2,NMDF_ALLOWFIRSTVOLUME=4
};

enum RARFORMAT {RARFMT_NONE,RARFMT14,RARFMT15,RARFMT50,RARFMT_FUTURE};

enum ADDSUBDATA_FLAGS
{
  ASDF_SPLIT          = 1, // Allow to split archive just before header if necessary.
  ASDF_COMPRESS       = 2, // Allow to compress data following subheader.
  ASDF_CRYPT          = 4, // Encrypt data after subheader if password is set.
  ASDF_CRYPTIFHEADERS = 8  // Encrypt data after subheader only in -hp mode.
};

class Archive:public File
{
  private:
    void UpdateLatestTime(FileHeader *CurBlock);
    void ConvertNameCase(wchar *Name);
    void ConvertFileHeader(FileHeader *hd);
    void WriteBlock50(HEADER_TYPE HeaderType,BaseBlock *wb,bool OnlySetSize,bool NonFinalWrite);
    size_t ReadHeader14();
    size_t ReadHeader15();
    size_t ReadHeader50();
    void ProcessExtra50(RawRead *Raw,size_t ExtraSize,BaseBlock *bb);
    void RequestArcPassword();
    void UnexpEndArcMsg();
    void BrokenHeaderMsg();
    void UnkEncVerMsg(const wchar *Name);
    void UnkEncVerMsg();
    bool ReadCommentData(Array<wchar> *CmtData);

#if !defined(SHELL_EXT) && !defined(RAR_NOCRYPT)
    CryptData HeadersCrypt;
#endif
#ifndef SHELL_EXT
    ComprDataIO SubDataIO;
#endif
    bool DummyCmd;
    RAROptions *Cmd;

    int64 RecoverySize;
    int RecoveryPercent;

    RarTime LatestTime;
    int LastReadBlock;
    HEADER_TYPE CurHeaderType;

    bool SilentOpen;
#ifdef USE_QOPEN
    QuickOpen QOpen;
#endif
  public:
    Archive(RAROptions *InitCmd=NULL);
    ~Archive();
    RARFORMAT IsSignature(const byte *D,size_t Size);
    bool IsArchive(bool EnableBroken);
    size_t SearchBlock(HEADER_TYPE HeaderType);
    size_t SearchSubBlock(const wchar *Type);
    size_t SearchRR();
    void WriteBlock(HEADER_TYPE HeaderType,BaseBlock *wb=NULL,bool OnlySetSize=false,bool NonFinalWrite=false);
    void SetBlockSize(HEADER_TYPE HeaderType,BaseBlock *wb=NULL) {WriteBlock(HeaderType,wb,true);}
    size_t ReadHeader();
    void CheckArc(bool EnableBroken);
    void CheckOpen(const wchar *Name);
    bool WCheckOpen(const wchar *Name);
    bool GetComment(Array<wchar> *CmtData);
    void ViewComment();
    void SetLatestTime(RarTime *NewTime);
    void SeekToNext();
    bool CheckAccess();
    bool IsArcDir();
    void ConvertAttributes();
    void VolSubtractHeaderSize(size_t SubSize);
    uint FullHeaderSize(size_t Size);
    int64 GetStartPos();
    void AddSubData(byte *SrcData,uint64 DataSize,File *SrcFile,
         const wchar *Name,uint Flags);
    bool ReadSubData(Array<byte> *UnpData,File *DestFile);
    HEADER_TYPE GetHeaderType() {return(CurHeaderType);};
    void WriteCommentData(byte *Data,size_t DataSize,bool FileComment);
    RAROptions* GetRAROptions() {return(Cmd);}
    void SetSilentOpen(bool Mode) {SilentOpen=Mode;}
#ifdef USE_QOPEN
    int Read(void *Data,size_t Size);
    void Seek(int64 Offset,int Method);
    int64 Tell();
    void QOpenUnload() {QOpen.Unload();}
#endif

    BaseBlock ShortBlock;
    MarkHeader MarkHead;
    MainHeader MainHead;
    CryptHeader CryptHead;
    FileHeader FileHead;
    EndArcHeader EndArcHead;
    SubBlockHeader SubBlockHead;
    FileHeader SubHead;
    CommentHeader CommHead;
    ProtectHeader ProtectHead;
    AVHeader AVHead;
    SignHeader SignHead;
    UnixOwnersHeader UOHead;
    MacFInfoHeader MACHead;
    EAHeader EAHead;
    StreamHeader StreamHead;

    int64 CurBlockPos;
    int64 NextBlockPos;

    RARFORMAT Format;
    bool Solid;
    bool Volume;
    bool MainComment;
    bool Locked;
    bool Signed;
    bool FirstVolume;
    bool NewNumbering;
    bool Protected;
    bool Encrypted;
    size_t SFXSize;
    bool BrokenHeader;
    bool FailedHeaderDecryption;

#if !defined(SHELL_EXT) && !defined(RAR_NOCRYPT)
    byte ArcSalt[SIZE_SALT50];
#endif

    bool Splitting;

    uint VolNumber;
    int64 VolWrite;
    uint64 AddingFilesSize;
    uint64 AddingHeadersSize;

    bool NewArchive;

    wchar FirstVolumeName[NM];
};


#endif
