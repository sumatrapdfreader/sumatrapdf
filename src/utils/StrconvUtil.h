/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace strconv {

WStr Utf8ToWStr(Str s, Arena* a = nullptr);
Str WStrToUtf8(WStr s, Arena* a = nullptr);

Str WStrToCodePage(uint codePage, WStr s, Arena* a = nullptr);
TempStr ToMultiByteTemp(Str src, uint codePageSrc, uint codePageDest);
WStr StrCPToWStr(Str src, uint codePage);
TempWStr StrCPToWStrTemp(Str src, uint codePage);
TempStr StrToUtf8Temp(Str src, uint codePage);

TempStr UnknownToUtf8Temp(Str s);

Str WStrToAnsi(WStr src);
Str Utf8ToAnsi(Str s);

TempWStr AnsiToWStrTemp(Str src);
Str AnsiToUtf8(Str src);
TempStr AnsiToUtf8Temp(Str src);
} // namespace strconv

// shorter names
// TODO: eventually we want to migrate all strconv:: to them
Str ToUtf8(WStr s, Arena* a = nullptr);
WStr ToWStr(Str s, Arena* a = nullptr);