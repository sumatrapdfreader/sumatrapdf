/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/BitManip.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/Log.h"

#include "wingui/WinGui.h"
#include "wingui/TreeModel.h"
#include "wingui/TreeCtrl.h"

#include "EngineBase.h"
#include "ParseBKM.h"

/*
Creating and parsing of .bkm files that contain alternative bookmarks view
for PDF files.
*/

Bookmarks::~Bookmarks() {
    free(filePath);
    delete toc;
}

// TODO: serialize open state
void SerializeBookmarksRec(DocTocItem* node, int level, str::Str& s) {
    if (level == 0) {
        s.Append("title: default view\n");
    }

    while (node) {
        for (int i = 0; i < level; i++) {
            s.Append("  ");
        }
        WCHAR* title = node->Text();
        AutoFree titleA = strconv::WstrToUtf8(title);
        sv::AppendQuotedString(titleA.as_view(), s);
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
        if (node->pageNo != 0) {
            s.AppendFmt(" page:%d", node->pageNo);
        }
        PageDestination* dest = node->GetPageDestination();
        if (dest) {
            int pageNo = dest->GetPageNo();
            CrashIf(node->pageNo != pageNo);
            auto ws = dest->GetValue();
            if (ws != nullptr) {
                s.Append(" dest:");
                AutoFree str = strconv::WstrToUtf8(ws);
                sv::AppendQuotedString(str.as_view(), s);
            }
        }
        s.Append("\n");

        SerializeBookmarksRec(node->child, level + 1, s);
        node = node->next;
    }
}

// parses "quoted string"
static str::Str parseLineTitle(std::string_view& sv) {
    str::Str res;
    size_t n = sv.size();
    // must be at least: ""
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
        char c = *s;
        if (c == '"') {
            // the end
            sv::SkipTo(sv, s + 1);
            return res;
        }
        if (c != '\\') {
            res.Append(c);
            s++;
            continue;
        }
        // potentially un-escape
        s++;
        if (s >= e) {
            break;
        }
        char c2 = *s;
        bool unEscape = (c2 == '\\') || (c2 == '"');
        if (!unEscape) {
            res.Append(c);
            continue;
        }
        res.Append(c2);
        s++;
    }

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
    if (!str::StartsWith(sv, "page:")) {
        return {nullptr, false};
    }
    // TODO: actually parse the info
    auto* res = new ParsedDest{1};
    return {res, true};
}

// a single line in .bmk file is:
// indentation "quoted title" additional-metadata* destination
static DocTocItem* parseBookmarksLine(std::string_view line, size_t* indentOut) {
    auto origLine = line; // save for debugging

    // lines might start with an indentation, 2 spaces for one level
    // TODO: maybe also count tabs as one level?
    size_t indent = sv::SkipChars(line, ' ');
    // must be multiple of 2
    if (indent % 2 != 0) {
        return nullptr;
    }
    *indentOut = indent / 2;
    sv::SkipChars(line, ' ');
    // TODO: no way to indicate an error
    str::Str title = parseLineTitle(line);
    DocTocItem* res = new DocTocItem();
    res->title = strconv::Utf8ToWchar(title.AsView());

    // parse meta-data and page destination
    std::string_view part;
    while (line.size() > 0) {
        sv::SkipChars(line, ' ');
        part = sv::ParseUntil(line, ' ');

        if (str::Eq(part, "font:bold")) {
            bit::Set(res->fontFlags, fontBitBold);
            continue;
        }

        if (str::Eq(part, "font:italic")) {
            bit::Set(res->fontFlags, fontBitItalic);
            continue;
        }

        auto [color, ok] = parseColor(part);
        if (ok) {
            res->color = color;
            continue;
        }

        auto [dest, ok2] = parseDestination(part);
        if (ok2) {
            res->pageNo = dest->pageNo;
            if (dest->pageNo == 0) {
                dbglogf("has pageNo of 0\n");
            }
            delete dest;
            // TODO: parse destination and set values
            continue;
        }
    }
    return res;
}

struct DocTocItemWithIndent {
    DocTocItem* item = nullptr;
    size_t indent = 0;

    DocTocItemWithIndent() = default;
    ~DocTocItemWithIndent() = default;
};

// TODO: read more than one
static bool parseBookmarks(std::string_view sv, Vec<Bookmarks*>* bkms) {
    Vec<DocTocItemWithIndent> items;

    // first line should be "file: $file"
    auto line = sv::ParseUntil(sv, '\n');
    auto file = sv::ParseKV(line, "file");
    if (file.empty()) {
        return false;
    }

    // this line should be "title: $title"
    line = sv::ParseUntil(sv, '\n');
    auto title = sv::ParseKV(line, "title");
    if (title.empty()) {
        return false;
    }
    auto tree = new DocTocTree();
    tree->name = str::Dup(title);
    size_t indent = 0;
    while (true) {
        line = sv::ParseUntil(sv, '\n');
        if (line.empty()) {
            break;
        }
        auto* item = parseBookmarksLine(line, &indent);
        if (item == nullptr) {
            for (auto& el : items) {
                delete el.item;
            }
            delete tree;
            return false;
        }
        DocTocItemWithIndent iwl = {item, indent};
        items.Append(iwl);
    }
    size_t nItems = items.size();
    if (nItems == 0) {
        for (auto& el : items) {
            delete el.item;
        }
        delete tree;
        return false;
    }

    tree->root = items[0].item;

    /* We want to reconstruct tree from array
        a
         b1
         b2
        a2
         b3
\    */
    for (size_t i = 1; i < nItems; i++) {
        const auto& curr = items[i];
        auto& prev = items[i - 1];
        auto item = curr.item;
        if (prev.indent == curr.indent) {
            // b1 -> b2
            prev.item->next = item;
        } else if (curr.indent > prev.indent) {
            // a2 -> b3
            prev.item->child = item;
        } else {
            // a -> a2
            bool didFound = false;
            for (int j = (int)i - 1; j >= 0; j--) {
                prev = items[j];
                if (prev.indent == curr.indent) {
                    prev.item->AddSibling(item);
                    didFound = true;
                    break;
                }
            }
            if (!didFound) {
                tree->root->AddSibling(item);
            }
        }
    }

    auto* bkm = new Bookmarks();
    bkm->toc = tree;
    bkms->Append(bkm);

    return true;
}

bool ParseBookmarksFile(std::string_view path, Vec<Bookmarks*>* bkms) {
    AutoFree d = file::ReadFile(path);
    if (!d.data) {
        return false;
    }
    return parseBookmarks(d.as_view(), bkms);
}

Vec<Bookmarks*>* LoadAlterenativeBookmarks(std::string_view baseFileName) {
    str::Str path = baseFileName;
    path.Append(".bkm");

    auto* res = new Vec<Bookmarks*>();

    auto ok = ParseBookmarksFile(path.AsView(), res);
    if (!ok) {
        DeleteVecMembers(*res);
        delete res;
        return nullptr;
    }

    // TODO: read more than one
    return res;
}

bool ExportBookmarksToFile(DocTocTree* tocTree, const char* path) {
    str::Str s;
    s.AppendFmt("file:%s\n", path);
    SerializeBookmarksRec(tocTree->root, 0, s);
    // dbglogf("%s\n", s.Get());
    str::Str fileName = path;
    fileName.Append(".bkm");
    return file::WriteFile(fileName.Get(), s.as_view());
}
