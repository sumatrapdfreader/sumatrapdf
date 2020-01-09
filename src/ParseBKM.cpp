/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/BitManip.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/Log.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/TreeModel.h"
#include "wingui/TreeCtrl.h"

#include "EngineBase.h"
#include "ParseBKM.h"

/*
Creating and parsing of .bkm files that contain alternative bookmarks view
for PDF files.
*/

using sv::ParsedKV;

VbkmForFile::~VbkmForFile() {
    delete toc;
    delete engine;
}

VbkmFile::~VbkmFile() {
    DeleteVecMembers(vbkms);
}

static void SerializeKeyVal(char* key, WCHAR* val, str::Str& s) {
    CrashIf(!key);
    if (!val) {
        return;
    }
    s.AppendFmt(" %s:", key);
    AutoFree str = strconv::WstrToUtf8(val);
    sv::AppendMaybeQuoted(str.as_view(), s);
}

static void SerializeDest(PageDestination* dest, str::Str& s) {
    if (!dest) {
        return;
    }
    s.AppendFmt(" destkind:%s", dest->kind);
    SerializeKeyVal("destname", dest->GetName(), s);
    SerializeKeyVal("destvalue", dest->GetValue(), s);
    // Note: not serializing dest->pageno because it's redundant with
    // TocItem::pageNo
    RectD r = dest->rect;
    if (r.empty()) {
        return;
    }
    // TODO: using %g is not great, because it's scientific notian 1e6
    // but this should not happen often.
    // Unlike %f it doesn't serialize trailing zeros
    if (r.dx == DEST_USE_DEFAULT || r.dy == DEST_USE_DEFAULT) {
        s.AppendFmt(" pos:%g,%g", r.x, r.y);
        return;
    }
    s.AppendFmt(" rect:%g,%g,%g,%g", r.x, r.y, r.dx, r.dy);
}

static void SerializeBookmarksRec(TocItem* node, int level, str::Str& s) {
    while (node) {
        for (int i = 0; i < level; i++) {
            s.Append("  ");
        }
        WCHAR* title = node->Text();
        AutoFree titleA = strconv::WstrToUtf8(title);
        sv::AppendMaybeQuoted(titleA.as_view(), s);
        auto flags = node->fontFlags;
        str::Str fontVal;
        if (bit::IsSet(flags, fontBitItalic)) {
            fontVal.Append("italic");
        }
        if (bit::IsSet(flags, fontBitBold)) {
            if (fontVal.size() > 0) {
                fontVal.Append(",");
            }
            fontVal.Append("bold");
        }
        if (fontVal.size() > 0) {
            s.Append(" font:");
            s.AppendView(fontVal.as_view());
        }
        if (node->color != ColorUnset) {
            s.Append(" color:");
            SerializeColor(node->color, s);
        }
        if (node->pageNo != 0) {
            s.AppendFmt(" page:%d", node->pageNo);
        }
        if (node->isOpenDefault) {
            s.AppendFmt(" open-default");
        }
        if (node->isOpenDefault) {
            s.AppendFmt(" open-toggled");
        }
        if (node->isUnchecked) {
            s.AppendFmt(" unchecked");
        }

        CrashIf(!node->PageNumbersMatch());
        SerializeDest(node->GetPageDestination(), s);
        s.Append("\n");

        SerializeBookmarksRec(node->child, level + 1, s);
        node = node->next;
    }
}

// indentation "quoted title" additional-metadata* destination
static TocItem* parseTocLine(std::string_view line, size_t* indentOut) {
    auto origLine = line; // save for debugging

    // lines might start with an indentation, 2 spaces for one level
    // TODO: maybe also count tabs as one level?
    size_t indent = sv::SkipChars(line, ' ');
    // must be multiple of 2
    if (indent % 2 != 0) {
        return nullptr;
    }
    *indentOut = indent / 2;

    // first item on the line is a title
    str::Str title;
    bool ok = sv::ParseMaybeQuoted(line, title, false);
    TocItem* res = new TocItem();
    res->title = strconv::Utf8ToWstr(title.as_view());
    PageDestination* dest = nullptr;

    // parse meta-data and page destination
    while (line.size() > 0) {
        ParsedKV kv = sv::ParseKV(line, false);
        if (!kv.ok) {
            return nullptr;
        }
        char* key = kv.key;
        char* val = kv.val;
        CrashIf(!key);
        if (str::EqI(key, "font")) {
            if (!val) {
                logf("parseBookmarksLine: got 'font' without value in line '%s'\n", origLine.data());
                return nullptr;
            }
            if (str::EqI(val, "bold")) {
                bit::Set(res->fontFlags, fontBitBold);
                continue;
            }
            if (str::EqI(val, "italic")) {
                bit::Set(res->fontFlags, fontBitItalic);
                continue;
            }
        }

        // TODO: fail if those have values
        if (str::EqI(key, "open-default")) {
            res->isOpenDefault = true;
            continue;
        }

        if (str::EqI(key, "open-toggled")) {
            res->isOpenToggled = true;
            continue;
        }

        if (str::Eq(key, "unchecked")) {
            res->isUnchecked = true;
            continue;
        }

        if (str::Eq(key, "page")) {
            if (!val) {
                return nullptr;
            }
            str::Parse(val, "%d", &res->pageNo);
            continue;
        }

        if (str::Eq(key, "color")) {
            COLORREF c = 0;
            ok = ParseColor(&c, val);
            if (ok) {
                res->color = c;
            }
            continue;
        }

        // the values here are for destination
        if (!dest) {
            dest = new PageDestination();
            dest->value = str::Dup(res->title);
        }

        if (str::Eq(key, "destkind")) {
            dest->kind = resolveDestKind(val);
            continue;
        }

        if (str::Eq(key, "destname")) {
            dest->name = strconv::Utf8ToWstr(val);
            continue;
        }

        if (str::Eq(key, "destval")) {
            dest->value = strconv::Utf8ToWstr(val);
            continue;
        }

        if (str::Eq(key, "rect")) {
            float x, y, dx, dy;
            str::Parse(val, "%g,%g,%g,%g", &x, &y, &dx, &dy);
            dest->rect = RectD(x, y, dx, dy);
            continue;
        }

        if (str::Eq(key, "pos")) {
            float x, y;
            str::Parse(val, "%g,%g", &x, &y);
            dest->rect = RectD(x, y, DEST_USE_DEFAULT, DEST_USE_DEFAULT);
            continue;
        }
    }
    if (dest) {
        dest->pageNo = res->pageNo;
    }
    return res;
}

struct TocItemWithIndent {
    TocItem* item = nullptr;
    size_t indent = 0;

    TocItemWithIndent() = default;
    TocItemWithIndent(TocItem* item, size_t indent);
    ~TocItemWithIndent() = default;
};

TocItemWithIndent::TocItemWithIndent(TocItem* item, size_t indent) {
    this->item = item;
    this->indent = indent;
}

static bool parseVbkmSection(std::string_view sv, Vec<VbkmForFile*>& bkmsOut) {
    Vec<TocItemWithIndent> items;

    auto* bkm = new VbkmForFile();

    // first line should be "file: $file"
    auto file = sv::ParseValueOfKey(sv, "file", true);
    if (!file.ok) {
        return false;
    }

#if 0
    // this line should be "title: $title"
    auto title = sv::ParseValueOfKey(sv, "title", true);
    if (!title.ok) {
        return false;
    }
#endif
    auto tree = new TocTree();
    // tree->name = str::Dup(title.val);
    size_t indent = 0;
    std::string_view line;
    while (true) {
        line = sv::ParseUntil(sv, '\n');
        if (line.empty()) {
            break;
        }
        auto* item = parseTocLine(line, &indent);
        if (item == nullptr) {
            for (auto& el : items) {
                delete el.item;
            }
            delete tree;
            return false;
        }
        TocItemWithIndent iwl = {item, indent};
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

    bkm->filePath = file.val;
    file.val = nullptr; // take ownership

    bkm->toc = tree;
    bkmsOut.Append(bkm);

    return true;
}

// TODO: read more than one VbkmFile by trying multiple .1.bkm, .2.bkm etc.
bool LoadAlterenativeBookmarks(std::string_view baseFileName, VbkmFile& vbkm) {
    str::Str path = baseFileName;
    path.Append(".bkm");

    AutoFree d = file::ReadFile(path.as_view());
    if (d.empty()) {
        return false;
    }
    std::string_view sv = d.as_view();
    AutoFree dataNormalized = sv::NormalizeNewlines(sv);

    std::string_view svd = dataNormalized.as_view();

    // first line could be name
    auto name = sv::ParseValueOfKey(svd, "name", true);
    if (name.ok) {
        vbkm.name = name.val;
        name.val = nullptr;
    }

    bool ok = parseVbkmSection(svd, vbkm.vbkms);
    return ok;
}

bool ExportBookmarksToFile(const Vec<VbkmForFile*>& bookmarks, const char* name, const char* bkmPath) {
    str::Str s;
    if (str::IsEmpty(name)) {
        name = "default view";
    }
    s.AppendFmt("name: %s\n", name);
    for (auto&& vbkm : bookmarks) {
        const char* path = vbkm->filePath;
        CrashIf(!path);
        s.AppendFmt("file: %s\n", path);
        TocTree* tocTree = vbkm->toc;
        SerializeBookmarksRec(tocTree->root, 0, s);
    }
    return file::WriteFile(bkmPath, s.as_view());
}

// each logical record starts with "file:" line
// we split s into list of records for each file
// TODO: should we fail if the first line is not "file:" ?
// Currently we ignore everything from the beginning
// until first "file:" line
static Vec<std::string_view> SplitVbkmIntoSectons(std::string_view s) {
    Vec<std::string_view> res;
    auto tmp = s;
    Vec<const char*> addrs;

    // find indexes of lines that start with "file:"
    while (!tmp.empty()) {
        auto line = sv::ParseUntil(tmp, '\n');
        if (sv::StartsWith(line, "file:")) {
            addrs.push_back(line.data());
        }
    }

    size_t n = addrs.size();
    if (n == 0) {
        return res;
    }
    addrs.push_back(s.data() + s.size());
    for (size_t i = 0; i < n; i++) {
        const char* start = addrs[i];
        const char* end = addrs[i + 1];
        size_t size = end - start;
        auto sv = std::string_view{start, size};
        res.push_back(sv);
    }
    return res;
}

bool ParseVbkmFile(std::string_view d, VbkmFile& vbkm) {
    AutoFree s = sv::NormalizeNewlines(d);

    std::string_view sv = s;

    ParsedKV name = sv::ParseValueOfKey(sv, "name", true);
    if (!name.ok) {
        return false;
    }
    vbkm.name = name.val;
    name.val = nullptr;

    auto records = SplitVbkmIntoSectons(sv);
    auto n = records.size();
    if (n == 0) {
        return false;
    }

    for (size_t i = 0; i < n; i++) {
        std::string_view rd = records[i];
        bool ok = parseVbkmSection(rd, vbkm.vbkms);
        if (!ok) {
            return false;
        }
    }
    return true;
}

bool LoadVbkmFile(const char* filePath, VbkmFile& vbkm) {
    std::string_view sv = file::ReadFile(filePath);
    if (sv.empty()) {
        return false;
    }
    AutoFree svFree = sv;
    bool ok = ParseVbkmFile(sv, vbkm);
    return ok;
}
