/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"

// utilities for string view
namespace sv {

bool StartsWith(std::string_view s, std::string_view prefix) {
    auto plen = prefix.length();
    auto slen = s.length();
    if (plen > slen) {
        return false;
    }
    return str::EqN(s.data(), prefix.data(), plen);
}

bool StartsWith(std::string_view s, const char* prefix) {
    auto p = std::string_view(prefix);
    return StartsWith(s, p);
}

Vec<std::string_view> Split(std::string_view sv, char split, size_t max) {
    Vec<std::string_view> res;
    const char* s = sv.data();
    const char* end = s + sv.size();
    if (max > 0) {
        // we want to stop at max - 1 because we also add
        max--;
    }
    const char* prev = s;
    while (s < end) {
        char c = *s;
        if (c == split) {
            size_t size = s - prev;
            res.push_back({prev, size});
            prev = s + 1;
            if (max != 0 && max == res.size()) {
                break;
            }
        }
        ++s;
    }
    // add the rest if non-empty
    size_t size = end - prev;
    if (size > 0) {
        res.push_back({prev, size});
    }
    return res;
}

std::string_view TrimSpace(std::string_view str) {
    const char* s = str.data();
    const char* end = s + str.size();
    while (s < end && str::IsWs(*s)) {
        ++s;
    }
    while (end > s) {
        char c = end[-1];
        if (!str::IsWs(c)) {
            break;
        }
        --end;
    }
    size_t size = end - s;
    return {s, size};
}

} // namespace sv
