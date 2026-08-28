/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/SquareTreeParser.h"

/*
A 'square tree' is a format for representing string values contained in a
tree structure. Consumers may parse these string values further (as integers,
etc.) as required.

Each non-empty line which doesn't start with either '#' or ';' (comment line)
contains either a key and a single-line string value separated by '=' or ':' or
it contains a key followed by a single '[' which causes the following lines to be
parsed as a nested child node until a line starts with ']':

key = value
key2 = [ value containing square brackets (length: 49) ]
child [
  depth = 1
  subchild [
    depth = 2
  ]
]

If tree nodes are either concatenated or if their names are reused for a different
node, these nodes form a list which can be accessed by index (cf. SquareTreeNode::GetChild).

list [
  value 1
] [
  value 2
]
list [
  value 3
]

The below parser always tries to recover from errors, usually by ignoring
faulty lines. One intentional error handling is the parsing of INI-style headers which
allows to parse INI files mostly as expected as well. E.g.

[Section]
key = value

is read as if it were written as

Section [
  key = value
]

Final note: Whitespace at the start and end of a line as well as around key-value
separators is always ignored.
*/

// Safe read: '\0' when i is outside [0, data.len). Never use data.s[i] without a bound.
static char Peek(Str data, int i) {
    if (i < 0 || i >= data.len) {
        return '\0';
    }
    return data.s[i];
}

static int SkipWs(Str data, int off, bool stopAtLineEnd = false) {
    for (;;) {
        char c = Peek(data, off);
        if (!c || !str::IsWs(c) || (stopAtLineEnd && c == '\n')) {
            break;
        }
        off++;
    }
    return off;
}

static int SkipWsRev(Str data, int begin, int off) {
    for (; off > begin && str::IsWs(Peek(data, off - 1)); off--) {
        ;
    }
    return off;
}

static int SkipWsAndComments(Str data, int off) {
    for (;;) {
        off = SkipWs(data, off);
        char c = Peek(data, off);
        if (c == '#' || c == ';') {
            for (; Peek(data, off) && Peek(data, off) != '\n'; off++) {
                ;
            }
        } else {
            break;
        }
    }
    return off;
}

static bool IsBracketLine(Str data, int off) {
    if (Peek(data, off) != '[') {
        return false;
    }
    for (off++; Peek(data, off) && Peek(data, off) != '\n' && Peek(data, off) != '#' && Peek(data, off) != ';'; off++) {
        if (!str::IsWs(Peek(data, off))) {
            return false;
        }
    }
    return true;
}

static Str ExtractTrimmed(Str data, int begin, int end) {
    ReportIf(begin < 0 || end < begin || end > data.len);
    end = SkipWsRev(data, begin, end);
    while (begin < end && str::IsWs(Peek(data, begin))) {
        begin++;
    }
    return Str(data.s + begin, end - begin);
}

// One allocation: sizeof(DataItem) + key bytes + NUL [+ value bytes + NUL].
// key.s / str.s point into the same block as the DataItem (like AllocStrNode).
// Either child is set (value ignored) or str is set from value (child null).
static SquareTreeNode::DataItem* AllocDataItem(Str key, Str val, SquareTreeNode* child) {
    int klen = std::max(0, key.len);
    int vlen = child ? 0 : std::max(0, val.len);
    int cb = sizeofi(SquareTreeNode::DataItem) + klen + 1 + (child ? 0 : vlen + 1);
    auto* item = (SquareTreeNode::DataItem*)Alloc(nullptr, cb);
    if (!item) {
        return nullptr;
    }
    char* p = (char*)item + sizeofi(SquareTreeNode::DataItem);
    if (klen > 0 && key.s) {
        memcpy(p, key.s, (size_t)klen);
    }
    p[klen] = 0;
    item->key = Str(p, klen);
    p += klen + 1;
    if (child) {
        item->str = {};
        item->child = child;
    } else {
        if (vlen > 0 && val.s) {
            memcpy(p, val.s, (size_t)vlen);
        }
        p[vlen] = 0;
        item->str = Str(p, vlen);
        item->child = nullptr;
    }
    return item;
}

static void FreeDataItem(SquareTreeNode::DataItem* item) {
    if (!item) {
        return;
    }
    delete item->child;
    Free(nullptr, item);
}

SquareTreeNode::~SquareTreeNode() {
    for (int i = 0; i < len(data); i++) {
        FreeDataItem(data[i]);
    }
}

void SquareTreeNode::RemoveDataAt(int idx) {
    FreeDataItem(data[idx]);
    VecRemoveAt(data, idx);
}

// wantChild: match items with a child node; otherwise match value items (no child).
// On match, advances *startIdx to i+1 so the next call continues the search.
static SquareTreeNode::DataItem* FindDataItem(const SquareTreeNode* node, Str key, bool wantChild, int* startIdx) {
    int start = startIdx ? *startIdx : 0;
    int n = len(node->data);
    for (int i = start; i < n; i++) {
        SquareTreeNode::DataItem* item = node->data[i];
        if (!str::EqI(key, item->key)) {
            continue;
        }
        if (wantChild != (item->child != nullptr)) {
            continue;
        }
        if (startIdx) {
            *startIdx = i + 1;
        }
        return item;
    }
    return nullptr;
}

// Returned Str aliases the node's storage; valid until the node is mutated or destroyed.
Str SquareTreeNode::GetValue(Str key, int* startIdx) const {
    DataItem* item = FindDataItem(this, key, false, startIdx);
    return item ? item->str : Str{};
}

SquareTreeNode* SquareTreeNode::GetChild(Str key, int* startIdx) const {
    DataItem* item = FindDataItem(this, key, true, startIdx);
    return item ? item->child : nullptr;
}

// Line classification after scanning key / separator / value span.
enum class LineKind {
    NodeOpen,     // key [ ... ]  or  key \n [
    CloseBracket, // ]
    IniSection,   // [Section]  (top-level only; nested ends current node)
    KeyValue,     // key = value
    Invalid,      // ignore line
};

struct LineScan {
    int keyOff = 0;
    int sepOff = 0;
    int valOff = 0;
    int lineEnd = 0; // index of '\n' or data.len
    LineKind kind = LineKind::Invalid;
};

static bool IsNodeOpenLine(Str data, int sepOff, int valOff, int lineEnd) {
    if (IsBracketLine(data, sepOff)) {
        return true;
    }
    // also tolerate "key \n [ \n ... \n ]" (else the key gets an empty value
    // and the child node an empty key)
    return str::IsWs(Peek(data, sepOff)) && Peek(data, valOff) == '\n' &&
           IsBracketLine(data, SkipWsAndComments(data, lineEnd));
}

static bool IsIniSectionLine(Str data, int keyOff, int valOff, int lineEnd) {
    if (Peek(data, keyOff) != '[') {
        return false;
    }
    int closeOff = SkipWsRev(data, valOff, lineEnd) - 1;
    return Peek(data, closeOff) == ']';
}

// Scans one non-empty line starting at off (already past comments/ws).
static LineScan ScanLine(Str data, int off) {
    LineScan line;
    line.keyOff = off;
    for (;;) {
        char c = Peek(data, off);
        if (!c || c == '=' || c == ':' || c == '[' || c == ']' || c == '\n') {
            break;
        }
        off++;
    }
    if (!Peek(data, off) || Peek(data, off) == '\n') {
        // use first whitespace as a fallback separator
        for (off = line.keyOff; Peek(data, off) && !str::IsWs(Peek(data, off)); off++) {
            ;
        }
    }
    line.sepOff = off;
    if (Peek(data, off) && Peek(data, off) != '\n') {
        // skip to the first non-whitespace character on the same line (value)
        off = SkipWs(data, off + 1, true);
    }
    line.valOff = off;
    for (; Peek(data, off) && Peek(data, off) != '\n'; off++) {
        ;
    }
    line.lineEnd = off;

    if (IsNodeOpenLine(data, line.sepOff, line.valOff, line.lineEnd)) {
        line.kind = LineKind::NodeOpen;
    } else if (Peek(data, line.keyOff) == ']') {
        line.kind = LineKind::CloseBracket;
    } else if (IsIniSectionLine(data, line.keyOff, line.valOff, line.lineEnd)) {
        line.kind = LineKind::IniSection;
    } else if ((line.lineEnd < data.len && Peek(data, line.sepOff) == '[') || Peek(data, line.sepOff) == ']') {
        line.kind = LineKind::Invalid;
    } else {
        line.kind = LineKind::KeyValue;
    }
    return line;
}

static SquareTreeNode* ParseSquareTreeRec(Str data, int& off, bool isTopLevel, int depth);

// Appends one or more child nodes under keyView. off is the first char after '['.
static void AppendChildNodes(SquareTreeNode* node, Str data, Str keyView, int& off, int depth) {
    VecAppend(node->data, AllocDataItem(keyView, {}, ParseSquareTreeRec(data, off, false, depth + 1)));
    // arrays: reuse key for more children, or concatenate ("[ \n ] [ \n ]")
    while (IsBracketLine(data, (off = SkipWsAndComments(data, off)))) {
        off++;
        VecAppend(node->data, AllocDataItem(keyView, {}, ParseSquareTreeRec(data, off, false, depth + 1)));
    }
}

static void AppendKeyValue(SquareTreeNode* node, Str data, const LineScan& line) {
    Str keyView = ExtractTrimmed(data, line.keyOff, line.sepOff);
    Str valView = ExtractTrimmed(data, line.valOff, line.lineEnd);
    VecAppend(node->data, AllocDataItem(keyView, valView, nullptr));
}

// [Section] at top level becomes Section [ ... ] until the next section or EOF.
static void AppendIniSection(SquareTreeNode* node, Str data, const LineScan& line, int& off, int depth) {
    int closeOff = SkipWsRev(data, line.valOff, line.lineEnd) - 1;
    int nameStart = SkipWs(data, line.keyOff + 1);
    int nameEnd = SkipWsRev(data, nameStart, closeOff);
    Str sectionKey = Str(data.s + nameStart, nameEnd - nameStart);
    int sectionChildOff = line.lineEnd;
    VecAppend(node->data, AllocDataItem(sectionKey, {}, ParseSquareTreeRec(data, sectionChildOff, false, depth + 1)));
    off = sectionChildOff;
}

static SquareTreeNode* ParseSquareTreeRec(Str data, int& off, bool isTopLevel, int depth) {
    SquareTreeNode* node = new SquareTreeNode();
    if (depth >= 64) {
        off = data.len;
        return node;
    }

    while (Peek(data, off)) {
        off = SkipWsAndComments(data, off);
        if (!Peek(data, off)) {
            break;
        }

        LineScan line = ScanLine(data, off);
        switch (line.kind) {
            case LineKind::NodeOpen: {
                int childOff = SkipWsAndComments(data, line.sepOff) + 1;
                Str keyView = ExtractTrimmed(data, line.keyOff, line.sepOff);
                AppendChildNodes(node, data, keyView, childOff, depth);
                off = childOff;
                break;
            }
            case LineKind::CloseBracket:
                // finish parsing child node; ignore extra ] at top level
                off = line.keyOff + 1;
                if (!isTopLevel) {
                    return node;
                }
                break;
            case LineKind::IniSection:
                // nested: section header ends the current node (INI only at top level)
                if (!isTopLevel) {
                    off = line.keyOff;
                    return node;
                }
                AppendIniSection(node, data, line, off, depth);
                break;
            case LineKind::Invalid:
                // leave off at lineEnd; next SkipWsAndComments advances past the newline
                off = line.lineEnd;
                break;
            case LineKind::KeyValue:
                AppendKeyValue(node, data, line);
                off = line.lineEnd;
                if (Peek(data, off) == '\n') {
                    off++;
                }
                break;
        }
    }

    return node;
}

static void AppendIndent(str::Builder& out, Str indentUnit, int depth) {
    for (int i = 0; i < depth; i++) {
        out.Append(indentUnit);
    }
}

// Append a dump of node. indentUnit is one level (e.g. "\t" or "  "); lineEnd is "\r\n" or "\n".
void SerializeSquareTreeNode(str::Builder& out, SquareTreeNode* node, Str indentUnit, Str lineEnd, int depth) {
    if (!node) {
        return;
    }
    for (int i = 0; i < len(node->data); i++) {
        SquareTreeNode::DataItem* item = node->data[i];
        AppendIndent(out, indentUnit, depth);
        out.Append(item->key);
        if (item->child) {
            out.Append(StrL(" ["));
            out.Append(lineEnd);
            SerializeSquareTreeNode(out, item->child, indentUnit, lineEnd, depth + 1);
            AppendIndent(out, indentUnit, depth);
            out.Append(StrL("]"));
            out.Append(lineEnd);
        } else {
            out.Append(StrL(" = "));
            if (item->str) {
                out.Append(item->str);
            }
            out.Append(lineEnd);
        }
    }
}

TempStr SerializeSquareTreeNodeTemp(SquareTreeNode* node) {
    if (!node) {
        return {};
    }
    str::Builder s;
    SerializeSquareTreeNode(s, node, StrL("  "), StrL("\n"), 0);
    return ToStrTemp(s);
}

SquareTreeNode* ParseSquareTree(Str s) {
    if (str::IsNull(s)) {
        return nullptr;
    }
    TempStr data = strconv::UnknownToUtf8Temp(s);
    if (str::IsNull(data)) {
        return nullptr;
    }
    int off = 0;
    return ParseSquareTreeRec(data, off, true, 0);
}
