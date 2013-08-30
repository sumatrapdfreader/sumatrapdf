#include "rar.hpp"



#ifndef RARDLL
const wchar *St(MSGID StringId)
{
  static wchar StrTable[8][512];
  static int StrNum=0;
  if (++StrNum >= sizeof(StrTable)/sizeof(StrTable[0]))
    StrNum=0;
  wchar *Str=StrTable[StrNum];
  *Str=0;
  CharToWide(StringId,Str,ASIZE(StrTable[0]));
  return Str;
}
#endif

