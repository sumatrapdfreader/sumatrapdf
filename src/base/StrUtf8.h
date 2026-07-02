/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: include Base.h instead of including directly

bool isLegalUTF8Sequence(const u8* source, const u8* sourceEnd);
bool isLegalUTF8String(const u8** source, const u8* sourceEnd);
int utf8StrLen(const u8* s);
int utf8RuneLen(const u8* s);

namespace str {
void Utf8Encode(char* buf, int& off, int c);
int VsnprintfUtf8(Str buf, const char* fmt, va_list args);
} // namespace str

int Utf8CodepointCount(Str s);
int Utf8CodepointAtByte(Str s, int byteIdx, int* bytesOut = nullptr);
int Utf8CodepointNext(Str s, int& byteIdx);
int Utf8CodepointPrev(Str s, int& byteIdx);
int Utf8CodepointToByteIndex(Str s, int codepointIdx);
int Utf8AdvanceCodepoints(Str s, int byteIdx, int nCodepoints);
Str Utf8SliceByCodepoints(Str s, int startCodepoint, int nCodepoints);

TempStr ShortenStringUtf8Temp(Str s, int maxRunes);
TempStr ShortenStringUtf8InTheMiddleTemp(Str s, int maxRunes);

WStr ToWStrTemp(Str s);
Str ToUtf8(Arena* arena, WStr wide);
Str ToUtf8Temp(WStr wide);
WCHAR* CWStrTemp(Str s);
WCHAR* CWStrTemp(Str s, int& cch);
