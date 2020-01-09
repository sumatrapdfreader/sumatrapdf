
/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: include BaseUtil.h instead of including directly

#define UTF8_BOM "\xEF\xBB\xBF"
#define UTF16_BOM "\xFF\xFE"
#define UTF16BE_BOM "\xFE\xFF"

bool isLegalUTF8Sequence(const u8* source, const u8* sourceEnd);
bool isLegalUTF8String(const u8** source, const u8* sourceEnd);

namespace str {

enum class TrimOpt { Left, Right, Both };

size_t Len(const char* s);
char* Dup(const char* s);

void ReplacePtr(char** s, const char* snew);
void ReplacePtr(const char** s, const char* snew);

char* Join(const char* s1, const char* s2, const char* s3 = nullptr);
char* Join(const char* s1, const char* s2, const char* s3, Allocator* allocator);

bool Eq(const char* s1, const char* s2);
bool Eq(std::string_view s1, const char* s2);
bool EqI(const char* s1, const char* s2);
bool EqI(std::string_view s1, const char* s2);
bool EqIS(const char* s1, const char* s2);
bool EqN(const char* s1, const char* s2, size_t len);
bool EqNI(const char* s1, const char* s2, size_t len);
bool IsEmpty(const char* s);
bool StartsWith(const char* str, const char* txt);
bool StartsWith(std::string_view s, const char* txt);

#if OS_WIN
size_t Len(const WCHAR*);
WCHAR* Dup(const WCHAR*);
void ReplacePtr(WCHAR** s, const WCHAR* snew);
WCHAR* Join(const WCHAR*, const WCHAR*, const WCHAR* s3 = nullptr);
bool Eq(const WCHAR*, const WCHAR*);
bool EqI(const WCHAR*, const WCHAR*);
bool EqIS(const WCHAR*, const WCHAR*);
bool EqN(const WCHAR*, const WCHAR*, size_t);
bool EqNI(const WCHAR*, const WCHAR*, size_t);
bool IsEmpty(const WCHAR*);
bool StartsWith(const WCHAR* str, const WCHAR* txt);
#endif

bool StartsWithI(const char* str, const char* txt);
bool EndsWith(const char* txt, const char* end);
bool EndsWithI(const char* txt, const char* end);
bool EqNIx(const char* s, size_t len, const char* s2);

char* DupN(const char* s, size_t lenCch);
char* Dup(const std::string_view sv);
char* ToLowerInPlace(char* s);
char* ToLower(const char* s);

void Free(const char* s);

#if OS_WIN
bool StartsWithI(const WCHAR* str, const WCHAR* txt);
bool EndsWith(const WCHAR* txt, const WCHAR* end);
bool EndsWithI(const WCHAR* txt, const WCHAR* end);
WCHAR* DupN(const WCHAR* s, size_t lenCch);
void Free(const WCHAR* s);
WCHAR* ToLowerInPlace(WCHAR* s);
WCHAR* ToLower(const WCHAR* s);

void Utf8Encode(char*& dst, int c);
#endif

bool IsDigit(char c);
bool IsWs(char c);
bool IsAlNum(char c);

const char* FindChar(const char* str, const char c);
char* FindChar(char* str, const char c);
const char* FindCharLast(const char* str, const char c);
char* FindCharLast(char* str, const char c);
const char* Find(const char* str, const char* find);
const char* FindI(const char* str, const char* find);

bool Contains(std::string_view s, const char* txt);

bool BufFmtV(char* buf, size_t bufCchSize, const char* fmt, va_list args);
char* FmtV(const char* fmt, va_list args);
char* Format(const char* fmt, ...);

#if OS_WIN
const WCHAR* FindChar(const WCHAR* str, const WCHAR c);
WCHAR* FindChar(WCHAR* str, const WCHAR c);
const WCHAR* FindCharLast(const WCHAR* str, const WCHAR c);
WCHAR* FindCharLast(WCHAR* str, const WCHAR c);
const WCHAR* Find(const WCHAR* str, const WCHAR* find);

const WCHAR* FindI(const WCHAR* str, const WCHAR* find);
bool BufFmtV(WCHAR* buf, size_t bufCchSize, const WCHAR* fmt, va_list args);
WCHAR* FmtV(const WCHAR* fmt, va_list args);
WCHAR* Format(const WCHAR* fmt, ...);

bool IsWs(WCHAR c);
bool IsDigit(WCHAR c);
bool IsNonCharacter(WCHAR c);

size_t TrimWS(WCHAR* s, TrimOpt opt);
#endif

size_t TrimWS(char* s, TrimOpt opt);
void TrimWsEnd(char* s, char*& e);

size_t TransChars(char* str, const char* oldChars, const char* newChars);
char* Replace(const char* s, const char* toReplace, const char* replaceWith);

size_t NormalizeWS(char* str);
size_t NormalizeNewlinesInPlace(char* s, char* e);
size_t NormalizeNewlinesInPlace(char* s);
size_t RemoveChars(char* str, const char* toRemove);

size_t BufSet(char* dst, size_t dstCchSize, const char* src);
size_t BufAppend(char* dst, size_t dstCchSize, const char* s);

char* MemToHex(const unsigned char* buf, size_t len);
bool HexToMem(const char* s, unsigned char* buf, size_t bufLen);

const char* Parse(const char* str, const char* format, ...);
const char* Parse(const char* str, size_t len, const char* format, ...);

int CmpNatural(const char*, const char*);

#if OS_WIN
size_t TransChars(WCHAR* str, const WCHAR* oldChars, const WCHAR* newChars);
WCHAR* Replace(const WCHAR* s, const WCHAR* toReplace, const WCHAR* replaceWith);
size_t NormalizeWS(WCHAR* str);
size_t RemoveChars(WCHAR* str, const WCHAR* toRemove);
size_t BufSet(WCHAR* dst, size_t dstCchSize, const WCHAR* src);
size_t BufAppend(WCHAR* dst, size_t dstCchSize, const WCHAR* s);

WCHAR* FormatFloatWithThousandSep(double number, LCID locale = LOCALE_USER_DEFAULT);
WCHAR* FormatNumWithThousandSep(size_t num, LCID locale = LOCALE_USER_DEFAULT);
WCHAR* FormatRomanNumeral(int number);

int CmpNatural(const WCHAR*, const WCHAR*);

const WCHAR* Parse(const WCHAR* str, const WCHAR* format, ...);

#endif
} // namespace str

namespace url {

void DecodeInPlace(char* urlUtf8);

#if OS_WIN
bool IsAbsolute(const WCHAR* url);
void DecodeInPlace(WCHAR* url);
WCHAR* GetFullPath(const WCHAR* url);
WCHAR* GetFileName(const WCHAR* url);
#endif

} // namespace url

namespace seqstrings {
bool SkipStr(char*& s);
bool SkipStr(const char*& s);
int StrToIdx(const char* strings, const char* toFind);
const char* IdxToStr(const char* strings, int idx);

#if OS_WIN
bool SkipStr(const WCHAR*& s);
int StrToIdx(const char* strings, const WCHAR* toFind);
#endif
} // namespace seqstrings

#define _MemToHex(ptr) str::MemToHex((const unsigned char*)(ptr), sizeof(*ptr))
#define _HexToMem(txt, ptr) str::HexToMem(txt, (unsigned char*)(ptr), sizeof(*ptr))
