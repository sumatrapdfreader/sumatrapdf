/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
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

#include "Annotation.h"
#include "EngineBase.h"
#include "ParseBKM.h"

// Version of .bkm and .vbkm files. Allows us to change the mind
// TODO: this is provisional version while in development
constexpr const char* kBkmVersion = "998";

// static Kind kindTocItem = "bkmTreeItem";

/*
Creating and parsing of .bkm files that contain alternative bookmarks view
for PDF files.
*/

using sv::ParsedKV;

VbkmFile::~VbkmFile() {
    delete tree;
}

static std::string_view readFileNormalized(std::string_view path) {
    AutoFree d = file::ReadFile(path);
    return sv::NormalizeNewlines(d.AsView());
}

static void SerializeKeyVal(const char* key, const WCHAR* val, str::Str& s) {
    CrashIf(!key);
    if (!val) {
        return;
    }
    s.AppendFmt(" %s:", key);
    AutoFree str = strconv::WstrToUtf8(val);
    sv::AppendMaybeQuoted(str.AsView(), s);
}

static void SerializeDest(PageDestination* dest, str::Str& s) {
    if (!dest) {
        return;
    }
    CrashIf(!dest->kind);
    s.AppendFmt(" destkind:%s", dest->kind);
    SerializeKeyVal("destname", dest->GetName(), s);
    SerializeKeyVal("destvalue", dest->GetValue(), s);
    // Note: not serializing dest->pageno because it's redundant with
    // TocItem::pageNo
    RectF r = dest->rect;
    if (r.IsEmpty()) {
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

static const char maxIndentStr[] = "                                                                             ";
constexpr int maxIndent = (dimof(maxIndentStr) - 1) / 2;

static std::string_view getIndentStr(int n) {
    CrashIf(n > maxIndent);
    return {maxIndentStr, (size_t)n * 2};
}

static void SerializeBookmarksRec(TocItem* node, int level, str::Str& s) {
    std::string_view indentStr = getIndentStr(level);
    while (node) {
        if (node->engineFilePath) {
            s.AppendView(indentStr);
            s.AppendFmt("file: %s\n", node->engineFilePath);
            CrashIf(node->nPages == 0);
            s.AppendView(indentStr);
            s.AppendFmt("pages: %d\n", node->nPages);
        }

        s.AppendView(indentStr);
        WCHAR* title = node->Text();
        AutoFree titleA = strconv::WstrToUtf8(title);
        sv::AppendMaybeQuoted(titleA.AsView(), s);
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
            s.AppendView(fontVal.AsView());
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
        if (node->isOpenToggled) {
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

    int n = sv::ParseIndent(line);
    if (n < 0) {
        return nullptr;
    }
    *indentOut = n;

    // first item on the line is a title
    str::Str title;
    bool ok = sv::ParseMaybeQuoted(line, title, false);
    if (!ok) {
        return nullptr;
    }
    TocItem* res = new TocItem();
    res->title = strconv::Utf8ToWstr(title.AsView());
    PageDestination* dest = nullptr;

    // parse meta-data and page destination
    while (line.size() > 0) {
        ParsedKV kv = sv::ParseKV(line, false);
        if (!kv.ok) {
            delete res;
            delete dest;
            return nullptr;
        }
        char* key = kv.key;
        char* val = kv.val;
        CrashIf(!key);
        if (str::EqI(key, "font")) {
            if (!val) {
                logf("parseBookmarksLine: got 'font' without value in line '%s'\n", origLine.data());
                delete res;
                delete dest;
                return nullptr;
            }
            // TODO: for max correctness should split by "," but this works just as well
            if (str::Contains(val, "bold")) {
                bit::Set(res->fontFlags, fontBitBold);
            }
            if (str::Contains(val, "italic")) {
                bit::Set(res->fontFlags, fontBitItalic);
            }
            continue;
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

        if (str::EqI(key, "unchecked")) {
            res->isUnchecked = true;
            continue;
        }

        if (str::EqI(key, "page")) {
            if (!val) {
                delete res;
                delete dest;
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
            float x = 0, y = 0, dx = 0, dy = 0;
            str::Parse(val, "%g,%g,%g,%g", &x, &y, &dx, &dy);
            dest->rect = RectF(x, y, dx, dy);
            continue;
        }

        if (str::Eq(key, "pos")) {
            float x = 0, y = 0;
            str::Parse(val, "%g,%g", &x, &y);
            dest->rect = RectF(x, y, DEST_USE_DEFAULT, DEST_USE_DEFAULT);
            continue;
        }
    }
    if (dest) {
        CrashIf(dest->kind == nullptr);
        res->dest = dest;
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

static TocTree* parseVbkm(std::string_view sv) {
    Vec<TocItemWithIndent> items;

#if 0
    // this line should be "title: $title"
    auto title = sv::ParseValueOfKey(sv, "title", true);
    if (!title.ok) {
        return nullptr;
    }
#endif
    size_t indent = 0;
    std::string_view line;
    while (true) {
        // optional line "file: $file"
        ParsedKV file = sv::TryParseValueOfKey(sv, "file", true);
        // second line should be "pages: $nPages"
        ParsedKV nPagesV = sv::TryParseValueOfKey(sv, "pages", true);
        int nPages = 0;
        if (nPagesV.ok) {
            str::Parse(nPagesV.val, "%d", &nPages);
            if (nPages == 0) {
                return nullptr;
            }
        }

        line = sv::ParseUntil(sv, '\n');
        if (line.empty()) {
            break;
        }
        TocItem* item = parseTocLine(line, &indent);
        if (item == nullptr) {
            for (auto& el : items) {
                delete el.item;
            }
            return nullptr;
        }
        if (file.ok) {
            item->engineFilePath = file.val;
            file.val = nullptr;
        }
        item->nPages = nPages;
        TocItemWithIndent iwl = {item, indent};
        items.Append(iwl);
    }
    size_t nItems = items.size();
    if (nItems == 0) {
        for (auto& el : items) {
            delete el.item;
        }
        return nullptr;
    }

    TocTree* tree = new TocTree();
    // tree->name = str::Dup(title.val);
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
                    prev.item->AddSiblingAtEnd(item);
                    didFound = true;
                    break;
                }
            }
            if (!didFound) {
                tree->root->AddSiblingAtEnd(item);
            }
        }
    }

#if 0
    bkm->filePath = file.val;
    file.val = nullptr; // take ownership
#endif

    return tree;
}

// TODO: read more than one VbkmFile by trying multiple .1.bkm, .2.bkm etc.
bool LoadAlterenativeBookmarks(std::string_view baseFileName, VbkmFile& vbkm) {
    str::Str path = baseFileName;
    path.Append(".bkm");

    AutoFree d = readFileNormalized(path.AsView());
    if (d.empty()) {
        return false;
    }

    std::string_view sv = d.AsView();
    ParsedKV ver = sv::ParseValueOfKey(sv, "version", true);
    if (!ver.ok) {
        return false;
    }
    if (!str::Eq(ver.val, kBkmVersion)) {
        return false;
    }

    // first line could be name
    auto name = sv::ParseValueOfKey(sv, "name", true);
    if (name.ok) {
        vbkm.name = name.val;
        name.val = nullptr;
    }

    vbkm.tree = parseVbkm(sv);
    return vbkm.tree != nullptr;
}

bool ExportBookmarksToFile(TocTree* bookmarks, const char* name, const char* bkmPath) {
    str::Str s;
    s.AppendFmt("version: %s\n", kBkmVersion);
    if (str::IsEmpty(name)) {
        name = "default view";
    }
    s.AppendFmt("name: %s\n", name);
    SerializeBookmarksRec(bookmarks->root, 0, s);
    return file::WriteFile(bkmPath, s.AsSpan());
}

bool ParseVbkmFile(std::string_view sv, VbkmFile& vbkm) {
    ParsedKV ver = sv::ParseValueOfKey(sv, "version", true);
    if (!ver.ok) {
        return false;
    }
    if (!str::Eq(ver.val, kBkmVersion)) {
        return false;
    }

    ParsedKV name = sv::ParseValueOfKey(sv, "name", true);
    if (!name.ok) {
        return false;
    }
    vbkm.name = name.val;
    name.val = nullptr;

    vbkm.tree = parseVbkm(sv);
    return vbkm.tree != nullptr;
}

bool LoadVbkmFile(const char* filePath, VbkmFile& vbkm) {
    AutoFree d = readFileNormalized(filePath);
    std::string_view sv = d.AsView();
    bool ok = ParseVbkmFile(sv, vbkm);
    return ok;
}
