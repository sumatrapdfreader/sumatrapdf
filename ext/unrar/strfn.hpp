#ifndef _RAR_STRFN_
#define _RAR_STRFN_

const char* NullToEmpty(const char *Str);
const wchar* NullToEmpty(const wchar *Str);
void IntToExt(const char *Src,char *Dest,size_t DestSize);
int stricomp(const char *s1,const char *s2);
int strnicomp(const char *s1,const char *s2,size_t n);
wchar* RemoveEOL(wchar *Str);
wchar* RemoveLF(wchar *Str);
unsigned char loctolower(unsigned char ch);
unsigned char loctoupper(unsigned char ch);

char* strncpyz(char *dest, const char *src, size_t maxlen);
wchar* wcsncpyz(wchar *dest, const wchar *src, size_t maxlen);
char* strncatz(char* dest, const char* src, size_t maxlen);
wchar* wcsncatz(wchar* dest, const wchar* src, size_t maxlen);

unsigned char etoupper(unsigned char ch);
wchar etoupperw(wchar ch);

bool IsDigit(int ch);
bool IsSpace(int ch);
bool IsAlpha(int ch);

void BinToHex(const byte *Bin,size_t BinSize,char *Hex,wchar *HexW,size_t HexSize);

#ifndef SFX_MODULE
uint GetDigits(uint Number);
#endif

bool LowAscii(const char *Str);
bool LowAscii(const wchar *Str);

int wcsicompc(const wchar *Str1,const wchar *Str2);

void itoa(int64 n,char *Str);
void itoa(int64 n,wchar *Str);
const wchar* GetWide(const char *Src);
const wchar* GetCmdParam(const wchar *CmdLine,wchar *Param,size_t MaxSize);
#ifndef SILENT
void PrintfPrepareFmt(const wchar *Org,wchar *Cvt,size_t MaxSize);
#endif

enum PASSWORD_TYPE {PASSWORD_GLOBAL,PASSWORD_FILE,PASSWORD_ARCHIVE};
bool GetPassword(PASSWORD_TYPE Type,const wchar *FileName,SecPassword *Password);

#endif
