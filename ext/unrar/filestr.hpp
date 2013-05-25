#ifndef _RAR_FILESTR_
#define _RAR_FILESTR_

bool ReadTextFile(
  const wchar *Name,
  StringList *List,
  bool Config,
  bool AbortOnError=false,
  RAR_CHARSET SrcCharset=RCH_DEFAULT,
  bool Unquote=false,
  bool SkipComments=false,
  bool ExpandEnvStr=false
);

#endif
