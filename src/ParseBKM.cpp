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

// TODO: serialize open state
void SerializeBookmarksRec(DocTocItem* node, int level, str::Str<char>& s) {
    if (level == 0) {
        s.Append(":default bookmarks view\n");
    }

    while (node) {
        for (int i = 0; i < level; i++) {
            s.Append("  ");
        }
        WCHAR* title = node->Text();
        auto titleA = str::conv::ToUtf8(title);
        s.AppendView(titleA.AsView());
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

#if 0
static DocTocItem* parseBookmarksLine(std::string_view line, int* indentOut) {
    const char* s = line.data();
    const char* e = s + line.size();
    if (s == nullptr) {
        return nullptr;
    }
    int indent = 0;
    while (s < e) {
        if (*s != ' ') {
            break;
        }
        indent++;
    }
    
    // must be multiple of 2
    if (indent % 2 == 1) {
        return nullptr;
    }
    *indentOut = indent / 2;
    return false;
}
#endif

static DocTocTree* parseBookmarks(std::string_view sv) {
    // extract first line with title like ":foo"
    auto line = str::IterString(sv, '\n');
    const char* s = line.data();
    size_t n = line.size();
    if (s == nullptr) {
        return nullptr;
    }
    if (n < 2) {
        return nullptr;
    }
    if (s[0] != ':') {
        return nullptr;
    }
    s++;
    n -= 1;
    auto tree = new DocTocTree();
    tree->name = str::DupN(s, n);
    DocTocItem* curr = nullptr;
    size_t currIdent = 0;
    while (true) {
        line = str::IterString(sv, '\n');
        s = line.data();
        if (s == nullptr) {
            break;
        }
        if (curr == nullptr) {
            tree->root = new DocTocItem();
            //parseBookmarksLine
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
