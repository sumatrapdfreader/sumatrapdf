/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"
#include "utils/Log.h"
#include "Translations.h"

// TODO: those will be auto-generated
#define LANGS_COUNT 61
#define STRINGS_COUNT 363

struct StringsParser {
    std::string_view orig{};
    std::string_view rest{};
    char* currOrig{nullptr};
};

std::string_view SkipPast(std::string_view sv, char c) {
    const char* s = sv.data();
    const char* e = s + sv.size();
    while (s < e) {
        if (*s == c) {
            s++;
            size_t len = (e - s);
            return {s, len};
        }
        s++;
    }
    return {};
}

char* GetLineUnescaped(char* s, char* e) {
    char* w = s;
    while (s < e) {
        if (*s == '\n') {
            *s = 0;
            return s + 1;
        }
        if (*s != '\\') {
            *w++ = *s++;
        } else {
            // TODO: implement me
            //&&(s + 1 < e) && (*s)
        }
    }
    return nullptr;
}

void SkipPast(StringsParser& p, char c) {
    p.rest = SkipPast(p.rest, c);
}

bool ParseOrigString(StringsParser& p) {
    // TODO: write me
    // store parsed string in p.orig
    std::string_view sv = p.rest;
    if (sv.empty()) {
        // TODO: need to detect a valid eof
        return false;
    }
    // starts with ':'
    char* s = (char*)sv.data();
    char* e = s + sv.size();
    if (*s != ':') {
        return false;
    }
    s++;
    p.currOrig = s;
    while (s < e) {
        // TODO; write me
    }
    return false;
}

bool ParseTranslations(StringsParser& p) {
    // TODO: write me
    // parses $lang: $translation\n
    return false;
}

void ParseTranslationsTxt(std::string_view s) {
    StringsParser p{};
    p.orig = s;
    p.rest = s;
    // skip AppTranslator: SumatraPDF
    SkipPast(p, '\n');
    // skip 1b35f5aeed9fc7aacdd4ed60c3a3b489943bc7d7
    SkipPast(p, '\n');
}

void ParseTranslationsFromResources() {
    std::span<u8> d = LoadDataResource(2);
    if (d.empty()) {
        return;
    }
    ParseTranslationsTxt({(const char*)d.data(), d.size()});
    free(d.data());
}
