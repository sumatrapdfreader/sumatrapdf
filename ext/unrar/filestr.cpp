#include "rar.hpp"

static bool IsUnicode(byte *Data,int Size);

bool ReadTextFile(
  const wchar *Name,
  StringList *List,
  bool Config,
  bool AbortOnError,
  RAR_CHARSET SrcCharset,
  bool Unquote,
  bool SkipComments,
  bool ExpandEnvStr)
{
  wchar FileName[NM];
  *FileName=0;

  if (Name!=NULL)
    if (Config)
      GetConfigName(Name,FileName,ASIZE(FileName),true,false);
    else
      wcsncpyz(FileName,Name,ASIZE(FileName));

  File SrcFile;
  if (FileName!=NULL && *FileName!=0)
  {
    bool OpenCode=AbortOnError ? SrcFile.WOpen(FileName):SrcFile.Open(FileName,0);

    if (!OpenCode)
    {
      if (AbortOnError)
        ErrHandler.Exit(RARX_OPEN);
      return false;
    }
  }
  else
    SrcFile.SetHandleType(FILE_HANDLESTD);

  unsigned int DataSize=0,ReadSize;
  const int ReadBlock=1024;
  Array<char> Data(ReadBlock+5);
  while ((ReadSize=SrcFile.Read(&Data[DataSize],ReadBlock))!=0)
  {
    DataSize+=ReadSize;
    Data.Add(ReadSize);
  }

  memset(&Data[DataSize],0,5);

  Array<wchar> WideStr;

  if (SrcCharset==RCH_UNICODE ||
      SrcCharset==RCH_DEFAULT && IsUnicode((byte *)&Data[0],DataSize))
  {
    // Unicode in native system format, can be more than 2 bytes per character.
    Array<wchar> DataW(Data.Size()/2+1);
    for (size_t I=2;I<Data.Size()-1;I+=2)
    {
      // Need to convert Data to (byte) first to prevent the sign extension
      // to higher bytes.
      DataW[(I-2)/2]=(wchar)((byte)Data[I])+(wchar)((byte)Data[I+1])*256;
    }

    wchar *CurStr=&DataW[0];

    while (*CurStr!=0)
    {
      wchar *NextStr=CurStr,*CmtPtr=NULL;
      while (*NextStr!='\r' && *NextStr!='\n' && *NextStr!=0)
      {
        if (SkipComments && NextStr[0]=='/' && NextStr[1]=='/')
        {
          *NextStr=0;
          CmtPtr=NextStr;
        }
        NextStr++;
      }
      *NextStr=0;
      for (wchar *SpacePtr=(CmtPtr ? CmtPtr:NextStr)-1;SpacePtr>=CurStr;SpacePtr--)
      {
        if (*SpacePtr!=' ' && *SpacePtr!='\t')
          break;
        *SpacePtr=0;
      }
      if (*CurStr!=0)
      {
        size_t Length=wcslen(CurStr);

        if (Unquote && *CurStr=='\"' && CurStr[Length-1]=='\"')
        {
          CurStr[Length-1]=0;
          CurStr++;
        }

        bool Expanded=false;
#ifdef _WIN_ALL
        if (ExpandEnvStr && *CurStr=='%')
        {
          // Expanding environment variables in Windows version.

          wchar ExpName[NM];
          *ExpName=0;
          DWORD Result=ExpandEnvironmentStrings(CurStr,ExpName,ASIZE(ExpName));
          Expanded=Result!=0 && Result<ASIZE(ExpName);
          if (Expanded)
            List->AddString(ExpName);
        }
#endif
        if (!Expanded)
          List->AddString(CurStr);
      }
      CurStr=NextStr+1;
      while (*CurStr=='\r' || *CurStr=='\n')
        CurStr++;
    }
  }
  else
  {
    char *CurStr=&Data[0];
    while (*CurStr!=0)
    {
      char *NextStr=CurStr,*CmtPtr=NULL;
      while (*NextStr!='\r' && *NextStr!='\n' && *NextStr!=0)
      {
        if (SkipComments && NextStr[0]=='/' && NextStr[1]=='/')
        {
          *NextStr=0;
          CmtPtr=NextStr;
        }
        NextStr++;
      }
      *NextStr=0;
      for (char *SpacePtr=(CmtPtr ? CmtPtr:NextStr)-1;SpacePtr>=CurStr;SpacePtr--)
      {
        if (*SpacePtr!=' ' && *SpacePtr!='\t')
          break;
        *SpacePtr=0;
      }
      if (*CurStr)
      {
        if (Unquote && *CurStr=='\"')
        {
          size_t Length=strlen(CurStr);
          if (CurStr[Length-1]=='\"')
          {
            CurStr[Length-1]=0;
            CurStr++;
          }
        }
#if defined(_WIN_ALL)
        if (SrcCharset==RCH_OEM)
          OemToCharA(CurStr,CurStr);
#endif

        bool Expanded=false;

        WideStr.Alloc(strlen(CurStr)+1);
        CharToWide(CurStr,&WideStr[0],WideStr.Size());
#ifdef _WIN_ALL
        if (ExpandEnvStr && *CurStr=='%')
        {
          // Expanding environment variables in Windows version.
          wchar ExpName[NM];
          DWORD Result=ExpandEnvironmentStringsW(&WideStr[0],ExpName,ASIZE(ExpName));
          Expanded=Result!=0 && Result<ASIZE(ExpName);
          if (Expanded)
            List->AddString(ExpName);
        }
#endif
        if (!Expanded)
          List->AddString(&WideStr[0]);
      }
      CurStr=NextStr+1;
      while (*CurStr=='\r' || *CurStr=='\n')
        CurStr++;
    }
  }
  return true;
}


bool IsUnicode(byte *Data,int Size)
{
  if (Size<4 || Data[0]!=0xff || Data[1]!=0xfe)
    return false;
  for (int I=2;I<Size;I++)
    if (Data[I]<32 && Data[I]!='\r' && Data[I]!='\n')
      return true;
  return false;
}
