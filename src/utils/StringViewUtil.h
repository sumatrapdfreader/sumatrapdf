/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace sv {

bool StartsWith(std::string_view s, std::string_view prefix);
bool StartsWith(std::string_view s, const char* prefix);

size_t SkipN(std::string_view& sv, size_t n);
size_t SkipTo(std::string_view& sv, const char* end);
size_t SkipChars(std::string_view& sv, char c);
std::string_view ParseUntil(std::string_view& sv, char delim);
std::string_view ParseUntilBack(std::string_view& sv, char delim);
std::string_view ParseKV(std::string_view sv, const char* key);

std::string_view TrimSpace(std::string_view str);
Vec<std::string_view> Split(std::string_view sv, char split, size_t max = 0);

// TODO: need a file for str::Str which is not Vec.h
void AppendQuotedString(std::string_view sv, str::Str& out);
std::string_view NormalizeNewlines(std::string_view s);

} // namespace sv
