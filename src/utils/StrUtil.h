/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: include BaseUtil.h instead of including directly

namespace str {

enum TrimOpt {
    TrimLeft,
    TrimRight,
    TrimBoth
};

size_t Len(const char *s);
size_t Len(const WCHAR *s);

char *  Dup(const char *s);
WCHAR * Dup(const WCHAR *s);

void ReplacePtr(char **s, const char *snew);
void ReplacePtr(const char **s, const char *snew);
void ReplacePtr(WCHAR **s, const WCHAR *snew);

char *  Join(const char *s1, const char *s2, const char *s3=NULL);
char *  Join(const char *s1, const char *s2, const char *s3, Allocator *allocator);
WCHAR * Join(const WCHAR *s1, const WCHAR *s2, const WCHAR *s3=NULL);

bool Eq(const char *s1, const char *s2);
bool Eq(const WCHAR *s1, const WCHAR *s2);
bool EqI(const char *s1, const char *s2);
bool EqI(const WCHAR *s1, const WCHAR *s2);
bool EqIS(const char *s1, const char *s2);
bool EqIS(const WCHAR *s1, const WCHAR *s2);
bool EqN(const char *s1, const char *s2, size_t len);
bool EqN(const WCHAR *s1, const WCHAR *s2, size_t len);
bool EqNI(const char *s1, const char *s2, size_t len);
bool EqNI(const WCHAR *s1, const WCHAR *s2, size_t len);

template <typename T>
inline bool IsEmpty(T *s) {
    return !s || (0 == *s);
}

template <typename T>
inline bool StartsWith(const T* str, const T* txt) {
    return EqN(str, txt, Len(txt));
}

bool StartsWithI(const char *str, const char *txt);
bool StartsWithI(const WCHAR *str, const WCHAR *txt);
bool EndsWith(const char *txt, const char *end);
bool EndsWith(const WCHAR *txt, const WCHAR *end);
bool EndsWithI(const char *txt, const char *end);
bool EndsWithI(const WCHAR *txt, const WCHAR *end);

static inline bool EqNIx(const char *s, size_t len, const char *s2) {
    return str::Len(s2) == len && str::StartsWithI(s, s2);
}

char *  DupN(const char *s, size_t lenCch);
WCHAR * DupN(const WCHAR *s, size_t lenCch);

void ToLower(char *s);
void ToLower(WCHAR *s);

char *  ToMultiByte(const WCHAR *txt, UINT CodePage, int cchTxtLen=-1);
char *  ToMultiByte(const char *src, UINT CodePageSrc, UINT CodePageDest);
WCHAR * ToWideChar(const char *src, UINT CodePage, int cbSrcLen=-1);
void    Utf8Encode(char *& dst, int c);

inline const char * FindChar(const char *str, const char c) {
    return strchr(str, c);
}
inline const WCHAR * FindChar(const WCHAR *str, const WCHAR c) {
    return wcschr(str, c);
}
inline char * FindChar(char *str, const char c) {
    return strchr(str, c);
}
inline WCHAR * FindChar(WCHAR *str, const WCHAR c) {
    return wcschr(str, c);
}

inline const char * FindCharLast(const char *str, const char c) {
    return strrchr(str, c);
}
inline const WCHAR * FindCharLast(const WCHAR *str, const WCHAR c) {
    return wcsrchr(str, c);
}
inline char * FindCharLast(char *str, const char c) {
    return strrchr(str, c);
}
inline WCHAR * FindCharLast(WCHAR *str, const WCHAR c) {
    return wcsrchr(str, c);
}

inline const char * Find(const char *str, const char *find) {
    return strstr(str, find);
}
inline const WCHAR * Find(const WCHAR *str, const WCHAR *find) {
    return wcsstr(str, find);
}

const char * FindI(const char *str, const char *find);
const WCHAR * FindI(const WCHAR *str, const WCHAR *find);

bool    BufFmtV(char *buf, size_t bufCchSize, const char *fmt, va_list args);
char *  FmtV(const char *fmt, va_list args);
char *  Format(const char *fmt, ...);
bool    BufFmtV(WCHAR *buf, size_t bufCchSize, const WCHAR *fmt, va_list args);
WCHAR * FmtV(const WCHAR *fmt, va_list args);
WCHAR * Format(const WCHAR *fmt, ...);

inline bool IsWs(char c) { return ' ' == c || '\t' <= c && c <= '\r'; }
inline bool IsWs(WCHAR c) { return iswspace(c); }

// Note: I tried an optimization: return (unsigned)(c - '0') < 10;
// but it seems to mis-compile in release builds
inline bool IsDigit(char c) {
    return '0' <= c && c <= '9';
}

inline bool IsDigit(WCHAR c) {
    return '0' <= c && c <= '9';
}

size_t  TrimWS(WCHAR *s, TrimOpt opt=TrimBoth);
void    TrimWsEnd(char *s, char *&e);

size_t  TransChars(char *str, const char *oldChars, const char *newChars);
size_t  TransChars(WCHAR *str, const WCHAR *oldChars, const WCHAR *newChars);
char *  Replace(const char *s, const char *toReplace, const char *replaceWith);
WCHAR * Replace(const WCHAR *s, const WCHAR *toReplace, const WCHAR *replaceWith);

size_t  NormalizeWS(char *str);
size_t  NormalizeWS(WCHAR *str);
size_t  NormalizeNewlinesInPlace(char *s, char *e);
size_t  NormalizeNewlinesInPlace(char *s);
size_t  RemoveChars(char *str, const char *toRemove);
size_t  RemoveChars(WCHAR *str, const WCHAR *toRemove);

size_t  BufSet(char *dst, size_t dstCchSize, const char *src);
size_t  BufSet(WCHAR *dst, size_t dstCchSize, const WCHAR *src);
size_t  BufAppend(char *dst, size_t dstCchSize, const char *s);
size_t  BufAppend(WCHAR *dst, size_t dstCchSize, const WCHAR *s);

char *  MemToHex(const unsigned char *buf, size_t len);
bool    HexToMem(const char *s, unsigned char *buf, size_t bufLen);

WCHAR * FormatFloatWithThousandSep(double number, LCID locale=LOCALE_USER_DEFAULT);
WCHAR * FormatNumWithThousandSep(size_t num, LCID locale=LOCALE_USER_DEFAULT);
WCHAR * FormatRomanNumeral(int number);

int     CmpNatural(const WCHAR *a, const WCHAR *b);

const char  *   Parse(const char *str, const char *format, ...);
const char  *   Parse(const char *str, size_t len, const char *format, ...);
const WCHAR *   Parse(const WCHAR *str, const WCHAR *format, ...);

size_t Utf8ToWcharBuf(const char *s, size_t sLen, WCHAR *bufOut, size_t cchBufOutSize);
size_t WcharToUtf8Buf(const WCHAR *s, char *bufOut, size_t cbBufOutSize);

namespace conv {

inline WCHAR *  FromCodePage(const char *src, UINT cp) { return ToWideChar(src, cp); }
inline char *   ToCodePage(const WCHAR *src, UINT cp) { return ToMultiByte(src, cp); }

inline WCHAR *  FromUtf8(const char *src, size_t cbSrcLen) { return ToWideChar(src, CP_UTF8, (int)cbSrcLen); }
inline WCHAR *  FromUtf8(const char *src) { return ToWideChar(src, CP_UTF8); }
inline char *   ToUtf8(const WCHAR *src, size_t cchSrcLen) { return ToMultiByte(src, CP_UTF8, (int)cchSrcLen); }
inline char *   ToUtf8(const WCHAR *src) { return ToMultiByte(src, CP_UTF8); }
inline WCHAR *  FromAnsi(const char *src, size_t cbSrcLen=(size_t)-1) { return ToWideChar(src, CP_ACP, (int)cbSrcLen); }
inline char *   ToAnsi(const WCHAR *src) { return ToMultiByte(src, CP_ACP); }
char *          UnknownToUtf8(const char *src, size_t len = 0);

size_t ToCodePageBuf(char *buf, int cbBufSize, const WCHAR *s, UINT cp);
size_t FromCodePageBuf(WCHAR *buf, int cchBufSize, const char *s, UINT cp);

} // namespace str::conv

}  // namespace str

namespace url {

bool IsAbsolute(const WCHAR *url);
void DecodeInPlace(char *urlUtf8);
void DecodeInPlace(WCHAR *url);
WCHAR *GetFullPath(const WCHAR *url);
WCHAR *GetFileName(const WCHAR *url);

} // namespace url

namespace seqstrings {
void         SkipStr(char *& s);
void         SkipStr(const char *& s);
int          StrToIdx(const char *strings, const char *toFind);
int          StrToIdx(const char *strings, const WCHAR *toFind);
const char * IdxToStr(const char *strings, int idx);

} // namespace seqstrings

#define _MemToHex(ptr) str::MemToHex((const unsigned char *)(ptr), sizeof(*ptr))
#define _HexToMem(txt, ptr) str::HexToMem(txt, (unsigned char *)(ptr), sizeof(*ptr))

#define UTF8_BOM    "\xEF\xBB\xBF"
#define UTF16_BOM   "\xFF\xFE"
#define UTF16BE_BOM "\xFE\xFF"
