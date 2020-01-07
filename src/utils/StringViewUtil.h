/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace sv {

struct ParsedKV {
    char* key = nullptr;
    char* val = nullptr;
    bool ok = false;

    ParsedKV() = default;
    ParsedKV(ParsedKV&&) noexcept;
    ParsedKV& operator=(ParsedKV&& other) noexcept;
    ParsedKV& operator=(ParsedKV& other) = delete;
    ~ParsedKV();
};

bool StartsWith(std::string_view s, std::string_view prefix);
bool StartsWith(std::string_view s, const char* prefix);
std::string_view TrimSpace(std::string_view str);
Vec<std::string_view> Split(std::string_view sv, char split, size_t max = 0);

std::string_view NormalizeNewlines(std::string_view s);

size_t SkipN(std::string_view& sv, size_t n);
size_t SkipTo(std::string_view& sv, const char* end);
size_t SkipChars(std::string_view& sv, char c);
std::string_view ParseUntil(std::string_view& sv, char delim);
std::string_view ParseUntilBack(std::string_view& sv, char delim);

void AppendQuoted(std::string_view sv, str::Str& out);
bool AppendMaybeQuoted(std::string_view sv, str::Str& out);
bool ParseMaybeQuoted(std::string_view& sv, str::Str& out, bool full);
ParsedKV ParseKV(std::string_view& sv, bool full);
ParsedKV ParseValueOfKey(std::string_view& sv, std::string_view key, bool full);

} // namespace sv
