#include "rar.hpp"

void FileHeader::Reset(size_t SubDataSize)
{
  SubData.Alloc(SubDataSize);
  BaseBlock::Reset();
#ifndef SHELL_EXT
  FileHash.Init(HASH_NONE);
#endif
  mtime.Reset();
  atime.Reset();
  ctime.Reset();
  SplitBefore=false;
  SplitAfter=false;

  UnknownUnpSize=0;

  SubFlags=0; // Important for RAR 3.0 subhead.
  
  CryptMethod=CRYPT_NONE;
  Encrypted=false;
  SaltSet=false;
  UsePswCheck=false;
  UseHashKey=false;
  Lg2Count=0;

  Solid=false;
  Dir=false;
  WinSize=0;
  Inherited=false;
  SubBlock=false;
  CommentInHeader=false;
  Version=false;
  LargeFile=false;

  RedirType=FSREDIR_NONE;
  UnixOwnerSet=false;
}


FileHeader& FileHeader::operator = (FileHeader &hd)
{
  SubData.Reset();
  memcpy(this,&hd,sizeof(*this));
  SubData.CleanData();
  SubData=hd.SubData;
  return *this;
}


void MainHeader::Reset()
{
  HighPosAV=0;
  PosAV=0;
  CommentInHeader=false;
  PackComment=false;
  Locator=false;
  QOpenOffset=0;
  QOpenMaxSize=0;
  RROffset=0;
  RRMaxSize=0;
}
