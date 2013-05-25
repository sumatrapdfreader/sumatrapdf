#include "rar.hpp"

#include "recvol3.cpp"
#include "recvol5.cpp"



bool RecVolumesRestore(RAROptions *Cmd,const wchar *Name,bool Silent)
{
  Archive Arc(Cmd);
  if (!Arc.Open(Name))
  {
    if (!Silent)
      ErrHandler.OpenErrorMsg(NULL,Name);
    return false;
  }

  RARFORMAT Fmt=RARFMT15;
  if (Arc.IsArchive(true))
    Fmt=Arc.Format;
  else
  {
    byte Sign[REV5_SIGN_SIZE];
    Arc.Seek(0,SEEK_SET);
    if (Arc.Read(Sign,REV5_SIGN_SIZE)==REV5_SIGN_SIZE && memcmp(Sign,REV5_SIGN,REV5_SIGN_SIZE)==0)
      Fmt=RARFMT50;
  }
  Arc.Close();

  // We define RecVol as local variable for proper stack unwinding when
  // handling exceptions. So it can close and delete files on Cancel.
  if (Fmt==RARFMT15)
  {
    RecVolumes3 RecVol;
    return RecVol.Restore(Cmd,Name,Silent);
  }
  else
  {
    RecVolumes5 RecVol;
    return RecVol.Restore(Cmd,Name,Silent);
  }
}
