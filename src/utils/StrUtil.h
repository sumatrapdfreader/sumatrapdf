/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: include BaseUtil.h instead of including directly

namespace str {

enum TrimOpt {
    TrimLeft,
    TrimRight,
    TrimBoth
};

inline size_t Len(const char *s) { return strlen(s); }
inline size_t Len(const WCHAR *s) { return wcslen(s); }

inline char *  Dup(const char *s) { return s ? _strdup(s) : NULL; }
inline WCHAR * Dup(const WCHAR *s) { return s ? _wcsdup(s) : NULL; }

void ReplacePtr(char **s, const char *snew);
void ReplacePtr(WCHAR **s, const WCHAR *snew);

char *  Join(const char *s1, const char *s2, const char *s3=NULL);
WCHAR * Join(const WCHAR *s1, const WCHAR *s2, const WCHAR *s3=NULL);

bool Eq(const char *s1, const char *s2);
bool Eq(const WCHAR *s1, const WCHAR *s2);
bool EqI(const char *s1, const char *s2);
bool EqI(const WCHAR *s1, const WCHAR *s2);
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

char *  DupN(const char *s, size_t lenCch);
WCHAR * DupN(const WCHAR *s, size_t lenCch);

void ToLower(char *s);
void ToLower(WCHAR *s);

char *  ToMultiByte(const WCHAR *txt, UINT CodePage);
char *  ToMultiByte(const char *src, UINT CodePageSrc, UINT CodePageDest);
WCHAR * ToWideChar(const char *src, UINT CodePage);
void    Utf8Encode(char *& dst, int c);

inline const char * FindChar(const char *str, const char c) {
    return strchr(str, c);
}
inline const WCHAR * FindChar(const WCHAR *str, const WCHAR c) {
    return wcschr(str, c);
}

inline const char * FindCharLast(const char *str, const char c) {
    return strrchr(str, c);
}
inline const WCHAR * FindCharLast(const WCHAR *str, const WCHAR c) {
    return wcsrchr(str, c);
}

inline const char * Find(const char *str, const char *find) {
    return strstr(str, find);
}
inline const WCHAR * Find(const WCHAR *str, const WCHAR *find) {
    return wcsstr(str, find);
}

bool    BufFmtV(char *buf, size_t bufCchSize, const char *fmt, va_list args);
char *  FmtV(const char *fmt, va_list args);
char *  Format(const char *fmt, ...);
bool    BufFmtV(WCHAR *buf, size_t bufCchSize, const WCHAR *fmt, va_list args);
WCHAR * FmtV(const WCHAR *fmt, va_list args);
WCHAR * Format(const WCHAR *fmt, ...);

inline bool IsWs(char c) { return ' ' == c || '\t' <= c && c <= '\r'; }
inline bool IsWs(WCHAR c) { return iswspace(c); }
inline bool IsDigit(char c) { return '0' <= c && c <= '9'; }
inline bool IsDigit(WCHAR c) { return '0' <= c && c <= '9'; }
size_t  TrimWS(WCHAR *s, TrimOpt opt=TrimBoth);

size_t  TransChars(char *str, const char *oldChars, const char *newChars);
size_t  TransChars(WCHAR *str, const WCHAR *oldChars, const WCHAR *newChars);
char *  Replace(const char *s, const char *toReplace, const char *replaceWith);

size_t  NormalizeWS(WCHAR *str);
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

size_t Utf8ToWcharBuf(const char *s, size_t sLen, WCHAR *bufOut, size_t bufOutMax);

void UrlDecodeInPlace(char *url);
void UrlDecodeInPlace(WCHAR *url);
// TODO: a better name
WCHAR *ToPlainUrl(const WCHAR *url);

namespace conv {

inline WCHAR *  FromCodePage(const char *src, UINT cp) { return ToWideChar(src, cp); }
inline char *   ToCodePage(const WCHAR *src, UINT cp) { return ToMultiByte(src, cp); }

inline WCHAR *  FromUtf8(const char *src) { return FromCodePage(src, CP_UTF8); }
inline char *   ToUtf8(const WCHAR *src) { return ToCodePage(src, CP_UTF8); }
inline WCHAR *  FromAnsi(const char *src) { return FromCodePage(src, CP_ACP); }
inline char *   ToAnsi(const WCHAR *src) { return ToCodePage(src, CP_ACP); }

size_t ToCodePageBuf(char *buf, size_t cbBufSize, const char *s, UINT cp);
size_t FromCodePageBuf(char *buf, size_t cchBufSize, const char *s, UINT cp);
size_t ToCodePageBuf(char *buf, size_t cbBufSize, const WCHAR *s, UINT cp);
size_t FromCodePageBuf(WCHAR *buf, size_t cchBufSize, const char *s, UINT cp);

} // namespace str::conv

}  // namespace str

namespace seqstrings {

int          GetStrIdx(const char *strings, const char *toFind, int max);
const char * GetByIdx(const char *strings, int idx);

} // namespace seqstrings

#define _MemToHex(ptr) str::MemToHex((const unsigned char *)(ptr), sizeof(*ptr))
#define _HexToMem(txt, ptr) str::HexToMem(txt, (unsigned char *)(ptr), sizeof(*ptr))

#define UTF8_BOM    "\xEF\xBB\xBF"
#define UTF16_BOM   "\xFF\xFE"
#define UTF16BE_BOM "\xFE\xFF"
