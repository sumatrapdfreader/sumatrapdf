/* Copyright 2018 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/BitManip.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"

#include "wingui/WinGui.h"
#include "wingui/TreeModel.h"
#include "wingui/TreeCtrl.h"

#include "BaseEngine.h"

/*
Creating and parsing of .bkm files that contain alternative bookmarks view
for PDF files.
*/

static void appendQuotedString(std::string_view sv, str::Str<char>& out) {
    out.Append('"');
    const char* s = sv.data();
    const char* end = s + sv.size();
    while (s < end) {
        auto c = *s;
        switch (c) {
            case '"':
            case '\\':
                out.Append('\\');
                out.Append(c);
                out.Append('\\');
                break;
            default:
                out.Append(c);
        }
        s++;
    }
    out.Append('"');
}

// TODO: serialize open state
void SerializeBookmarksRec(DocTocItem* node, int level, str::Str<char>& s) {
    if (level == 0) {
        s.Append(":default view\n");
    }

    while (node) {
        for (int i = 0; i < level; i++) {
            s.Append("  ");
        }
        WCHAR* title = node->Text();
        auto titleA = str::conv::ToUtf8(title);
        appendQuotedString(titleA.AsView(), s);
        auto flags = node->fontFlags;
        if (bit::IsSet(flags, fontBitItalic)) {
            s.Append(" font:italic");
        }
        if (bit::IsSet(flags, fontBitBold)) {
            s.Append(" font:bold");
        }
        if (node->color != ColorUnset) {
            s.Append(" ");
            SerializeColor(node->color, s);
        }
        PageDestination* dest = node->GetLink();
        if (dest) {
            int pageNo = dest->GetDestPageNo();
            s.AppendFmt(" page:%d", pageNo);
            auto ws = dest->GetDestValue();
            if (ws != nullptr) {
                auto str = str::conv::ToUtf8(ws);
                s.Append(",dest:");
                s.AppendView(str.AsView());
            }
        }
        s.Append("\n");

        SerializeBookmarksRec(node->child, level + 1, s);
        node = node->next;
    }
}

// update sv to skip first n characters
static size_t skipN(std::string_view& sv, size_t n) {
    CrashIf(n > sv.size());
    const char* s = sv.data() + n;
    size_t newSize = sv.size() - n;
    sv = {s, newSize};
    return n;
}

static size_t skipUntil(std::string_view& sv, const char* end) {
    const char* s = sv.data();
    CrashIf(end < s);
    size_t toSkip = end - s;
    CrashIf(toSkip > sv.size());
    skipN(sv, toSkip);
    return toSkip;
}

// returns a substring of sv until c. updates sv to reflect the rest of the string
static std::string_view parseUntil(std::string_view& sv, char c) {
    const char* s = sv.data();
    const char* start = s;
    const char* e = s + sv.size();
    while (s < e) {
        if (*s == c) {
            break;
        }
        s++;
    }
    size_t n = skipUntil(sv, e);
    return {start, n};
}

// skips all c chars in the beginning of sv
// returns number of chars skipped
// TODO: rename trimLeft?
static size_t skipChars(std::string_view& sv, char c) {
    const char* s = sv.data();
    const char* e = s + sv.size();
    while (s < e) {
        if (*s != c) {
            break;
        }
        s++;
    }
    return skipUntil(sv, e);
}

// first line should look like:
// :title of the bookmarks view
// returns { nullptr, 0 } on error
static std::string_view parseBookmarksTitle(const std::string_view sv) {
    size_t n = sv.size();
    // must have at least ":" at the beginning
    if (n < 1) {
        return {nullptr, 0};
    }
    const char* s = sv.data();
    if (s[0] != ':') {
        return {nullptr, 0};
    }
    return {s + 1, n - 1};
}

// parses "quoted string"
static str::Str<char> parseLineTitle(std::string_view& sv) {
    str::Str<char> res;
    size_t n = sv.size();
    // must be at least ""
    if (n < 2) {
        return res;
    }
    const char* s = sv.data();
    const char* e = s + n;
    if (s[0] != '"') {
        return res;
    }
    s++;
    while (s < e) {
        if (*s == '"') {
            // the end
            skipUntil(sv, s + 1);
            return res;
        }
    }

    res.Reset();
    return res;
}

static std::tuple<COLORREF, bool> parseColor(std::string_view sv) {
    COLORREF c = 0;
    bool ok = ParseColor(&c, sv);
    return {c, ok};
}

struct ParsedDest {
    int pageNo;
};

std::tuple<ParsedDest*, bool> parseDestination(std::string_view& sv) {
    if (str::StartsWith(sv, "page:")) {
        return {nullptr, false};
    }
    // TODO: actually parse the info
    auto* res = new ParsedDest{1};
    return {res, true};
}

// a single line in .bmk file is:
// indentation "quoted title" additional-metadata* destination
static DocTocItem* parseBookmarksLine(std::string_view line, size_t* indentOut) {
    // lines might start with an indentation, 2 spaces for one level
    // TODO: maybe also count tabs as one level?
    size_t indent = skipChars(line, ' ');
    // must be multiple of 2
    if (indent % 2 != 0) {
        return nullptr;
    }
    *indentOut = indent / 2;
    skipChars(line, ' ');
    // TODO: no way to indicate an error
    str::Str<char> title = parseLineTitle(line);
    DocTocItem* res = new DocTocItem();
    res->title = str::conv::Utf8ToWchar(title.AsView());

    // parse meta-data and page destination
    std::string_view part;
    while (line.size() > 0) {
        skipChars(line, ' ');
        part = parseUntil(line, ' ');

        if (str::Eq(part, "font:bold")) {
            bit::Set(res->fontFlags, fontBitBold);
            continue;
        }

        if (str::Eq(part, "font:italic")) {
            bit::Set(res->fontFlags, fontBitItalic);
            continue;
        }

        auto maybeColor = parseColor(part);
        if (std::get<1>(maybeColor) == true) {
            res->color = std::get<0>(maybeColor);
            continue;
        }

        auto maybeDest = parseDestination(part);
        if (std::get<1>(maybeDest) == true) {
            auto* dest = std::get<0>(maybeDest);
            res->pageNo = dest->pageNo;
            delete dest;
            // TODO: parse destination and set values
            continue;
        }
    }

    // page: must have been missing
    if (res->pageNo == 0) {
        delete res;
        return nullptr;
    }

    return res;
}

static DocTocTree* parseBookmarks(std::string_view sv) {
    constexpr size_t MAX_INDENT = 64;
    // parent node for a given indent level
    DocTocItem* items[MAX_INDENT] = {};
    int currIndent = 0; // index into items

    // extract first line with title like ":foo"
    auto line = str::IterString(sv, '\n');
    auto title = parseBookmarksTitle(line);
    if (title.data() == nullptr) {
        return nullptr;
    }
    auto tree = new DocTocTree();
    tree->name = str::Dup(sv);
    DocTocItem* curr = nullptr;
    size_t indent = 0;
    while (true) {
        line = str::IterString(sv, '\n');
        if (line.data() == nullptr) {
            break;
        }
        auto* item = parseBookmarksLine(line, &indent);
        if (item == nullptr) {
            delete tree;
            return nullptr;
        }

        if (curr == nullptr) {
            tree->root = new DocTocItem();
        }
    }

    return tree;
}

std::tuple<DocTocTree*, error*> ParseBookmarksFile(const char* path) {
    UNUSED(path);
    auto res = file::ReadFile2(path);
    error* err = std::get<1>(res);
    if (err != nullptr) {
        return std::make_tuple(nullptr, err);
    }
#if 0
    OwnedData d = file::ReadFile(path);
    if (d.IsEmpty()) {
        return std::make_tuple(nullptr, NewError("empty file"));
    }
#endif
    const OwnedData& d = std::get<0>(res);
    auto* docTree = parseBookmarks(d.AsView());

    return std::make_tuple(docTree, nullptr);
}
