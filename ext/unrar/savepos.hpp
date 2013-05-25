#ifndef _RAR_SAVEPOS_
#define _RAR_SAVEPOS_

class SaveFilePos
{
  private:
    File *SaveFile;
    int64 SavePos;
  public:
    SaveFilePos(File &Src)
    {
      SaveFile=&Src;
      SavePos=Src.Tell();
    }
    ~SaveFilePos()
    {
      SaveFile->Seek(SavePos,SEEK_SET);
    }
};

#endif
