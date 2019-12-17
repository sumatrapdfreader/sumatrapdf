/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace sv {

bool StartsWith(std::string_view s, std::string_view prefix);
bool StartsWith(std::string_view s, const char* prefix);
Vec<std::string_view> Split(std::string_view sv, char split, size_t max = 0);
std::string_view TrimSpace(std::string_view str);

} // namespace sv
