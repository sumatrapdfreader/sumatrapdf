#ifndef _RAR_RAWREAD_
#define _RAR_RAWREAD_

class RawRead
{
  private:
    Array<byte> Data;
    File *SrcFile;
    size_t DataSize;
    size_t ReadPos;
#ifndef SHELL_EXT
    CryptData *Crypt;
#endif
  public:
    RawRead(File *SrcFile);
    void Reset();
    size_t Read(size_t Size);
    void Read(byte *SrcData,size_t Size);
    byte   Get1();
    ushort Get2();
    uint   Get4();
    uint64 Get8();
    uint64 GetV();
    uint   GetVSize(size_t Pos);
    size_t GetB(void *Field,size_t Size);
    void GetW(wchar *Field,size_t Size);
    uint GetCRC15(bool ProcessedOnly);
    uint GetCRC50();
    byte* GetDataPtr() {return &Data[0];}
    size_t Size() {return DataSize;}
    size_t PaddedSize() {return Data.Size()-DataSize;}
    size_t DataLeft() {return DataSize-ReadPos;}
    size_t GetPos() {return ReadPos;}
    void SetPos(size_t Pos) {ReadPos=Pos;}
#ifndef SHELL_EXT
    void SetCrypt(CryptData *Crypt) {RawRead::Crypt=Crypt;}
#endif
};

uint64 RawGetV(const byte *Data,uint &ReadPos,uint DataSize,bool &Overflow);

inline uint RawGet2(const byte *D)
{
  return D[0]+(D[1]<<8);
}

inline uint RawGet4(const byte *D)
{
  return D[0]+(D[1]<<8)+(D[2]<<16)+(D[3]<<24);
}

inline uint64 RawGet8(const byte *D)
{
  return INT32TO64(RawGet4(D+4),RawGet4(D));
}


// We need these "put" functions also in UnRAR code. This is why they are
// in rawread.hpp file even though they are "write" functions.
inline void RawPut2(uint Field,byte *Data)
{
  Data[0]=(byte)(Field);
  Data[1]=(byte)(Field>>8);
}


inline void RawPut4(uint Field,byte *Data)
{
  Data[0]=(byte)(Field);
  Data[1]=(byte)(Field>>8);
  Data[2]=(byte)(Field>>16);
  Data[3]=(byte)(Field>>24);
}


inline void RawPut8(uint64 Field,byte *Data)
{
  Data[0]=(byte)(Field);
  Data[1]=(byte)(Field>>8);
  Data[2]=(byte)(Field>>16);
  Data[3]=(byte)(Field>>24);
  Data[4]=(byte)(Field>>32);
  Data[5]=(byte)(Field>>40);
  Data[6]=(byte)(Field>>48);
  Data[7]=(byte)(Field>>56);
}

#endif
