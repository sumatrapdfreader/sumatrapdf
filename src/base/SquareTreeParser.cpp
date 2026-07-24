/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "SquareTreeParser.h"

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
faulty lines. One intentional error hanling is the parsing of INI-style headers which
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

static int SkipWs(Str data, int off, bool stopAtLineEnd = false) {
    for (; off < data.len && str::IsWs(data.s[off]) && (!stopAtLineEnd || data.s[off] != '\n'); off++) {
        ;
    }
    return off;
}

static int SkipWsRev(Str data, int begin, int off) {
    for (; off > begin && str::IsWs(data.s[off - 1]); off--) {
        ;
    }
    return off;
}

static int SkipWsAndComments(Str data, int off) {
    for (;;) {
        off = SkipWs(data, off);
        if (off < data.len && (data.s[off] == '#' || data.s[off] == ';')) {
            for (; off < data.len && data.s[off] != '\n'; off++) {
                ;
            }
        } else {
            break;
        }
    }
    return off;
}

static bool IsBracketLine(Str data, int off) {
    if (off >= data.len || data.s[off] != '[') {
        return false;
    }
    for (off++; off < data.len && data.s[off] != '\n' && data.s[off] != '#' && data.s[off] != ';'; off++) {
        if (!str::IsWs(data.s[off])) {
            return false;
        }
    }
    return true;
}

static Str ExtractTrimmed(Str data, int begin, int end) {
    begin = std::max(0, std::min(begin, data.len));
    end = std::max(begin, std::min(end, data.len));
    end = SkipWsRev(data, begin, end);
    while (begin < end && str::IsWs(data.s[begin])) {
        begin++;
    }
    return Str(data.s + begin, end - begin);
}

SquareTreeNode::~SquareTreeNode() {
    for (int i = 0; i < len(data); i++) {
        DataItem& item = data[i];
        str::Free(item.key);
        str::Free(item.str);
        delete item.child;
    }
}

Str SquareTreeNode::GetValue(Str key, size_t* startIdx) const {
    int start = startIdx ? (int)*startIdx : 0;
    int n = len(data);
    for (int i = start; i < n; i++) {
        DataItem& item = data[i];
        if (str::EqI(key, item.key) && !item.child) {
            if (startIdx) {
                *startIdx = (size_t)(i + 1);
            }
            return item.str;
        }
    }
    return {};
}

SquareTreeNode* SquareTreeNode::GetChild(Str key, size_t* startIdx) const {
    int start = startIdx ? (int)*startIdx : 0;
    int n = len(data);
    for (int i = start; i < n; i++) {
        DataItem& item = data[i];
        if (str::EqI(key, item.key) && item.child) {
            if (startIdx) {
                *startIdx = (size_t)(i + 1);
            }
            return item.child;
        }
    }
    return nullptr;
}

static SquareTreeNode* ParseSquareTreeRec(Str data, int& off, bool isTopLevel = false) {
    SquareTreeNode* node = new SquareTreeNode();

    while (off < data.len && data.s[off]) {
        off = SkipWsAndComments(data, off);
        if (off >= data.len || !data.s[off]) {
            break;
        }
        // all non-empty non-comment lines contain a key-value pair
        // where the value is either a string (separated by '=' or ':')
        // or a list of child nodes (if the key is followed by '[' alone)
        int keyOff = off;
        for (; off < data.len && data.s[off] != '=' && data.s[off] != ':' && data.s[off] != '[' && data.s[off] != ']' &&
               data.s[off] != '\n';
             off++) {
            ;
        }
        if (off >= data.len || data.s[off] == '\n') {
            // use first whitespace as a fallback separator
            for (off = keyOff; off < data.len && !str::IsWs(data.s[off]); off++) {
                ;
            }
        }
        int sepOff = off;
        if (off < data.len && data.s[off] != '\n') {
            // skip to the first non-whitespace character on the same line (value)
            off = SkipWs(data, off + 1, true);
        }
        int valOff = off;
        // skip to the end of the line
        for (; off < data.len && data.s[off] != '\n'; off++) {
            ;
        }
        if (IsBracketLine(data, sepOff) ||
            // also tolerate "key \n [ \n ... \n ]" (else the key
            // gets an empty value and the child node an empty key)
            (str::IsWs(data.s[sepOff]) && valOff < data.len && data.s[valOff] == '\n' &&
             IsBracketLine(data, SkipWsAndComments(data, off)))) {
            // parse child node(s)
            int childOff = SkipWsAndComments(data, sepOff) + 1;
            Str key = str::Dup(ExtractTrimmed(data, keyOff, sepOff));
            node->data.Append(SquareTreeNode::DataItem(key, ParseSquareTreeRec(data, childOff)));
            off = childOff;
            // arrays are created by either reusing the same key for a different child
            // or by concatenating multiple children ("[ \n ] [ \n ] [ \n ]")
            while (IsBracketLine(data, (off = SkipWsAndComments(data, off)))) {
                off++;
                // each DataItem owns its key (freed in ~SquareTreeNode), so
                // repeated array children each need their own copy
                node->data.Append(SquareTreeNode::DataItem(str::Dup(key), ParseSquareTreeRec(data, off)));
            }
        } else if (data.s[keyOff] == ']') {
            // finish parsing child node
            off = keyOff + 1;
            if (!isTopLevel) {
                return node;
            }
            // ignore superfluous closing square brackets instead of
            // ignoring all content following them
        } else if (data.s[keyOff] == '[' && data.s[SkipWsRev(data, valOff, off) - 1] == ']') {
            // treat INI section headers as top-level node names
            // (else "[Section]" would be ignored)
            if (!isTopLevel) {
                off = keyOff;
                return node;
            }
            // trim whitespace around section name (for consistency with GetPrivateProfileString)
            int closeOff = SkipWsRev(data, valOff, off) - 1;
            int nameStart = SkipWs(data, keyOff + 1);
            int nameEnd = SkipWsRev(data, nameStart, closeOff);
            Str sectionKey = str::Dup(Str(data.s + nameStart, nameEnd - nameStart));
            int sectionChildOff = off;
            node->data.Append(SquareTreeNode::DataItem(sectionKey, ParseSquareTreeRec(data, sectionChildOff)));
            off = sectionChildOff;
        } else if ((off < data.len && data.s[sepOff] == '[') || data.s[sepOff] == ']') {
            // invalid line (ignored)
        } else {
            // string value (decoding is left to the consumer)
            bool hasMoreLines = off < data.len && data.s[off] == '\n';
            Str key = str::Dup(ExtractTrimmed(data, keyOff, sepOff));
            Str val = str::Dup(ExtractTrimmed(data, valOff, off));
            node->data.Append(SquareTreeNode::DataItem(key, val));
            if (hasMoreLines) {
                off++;
            }
        }
    }

    // assume that all square brackets have been properly balanced
    return node;
}

static void SerializeRec(SquareTreeNode* node, str::Builder& s, int indent) {
    int n = len(node->data);
    for (int i = 0; i < n; i++) {
        SquareTreeNode::DataItem& item = node->data[i];
        for (int j = 0; j < indent; j++) {
            s.AppendChar(' ');
            s.AppendChar(' ');
        }
        if (item.child) {
            s.Append(item.key);
            s.Append(" [\n");
            SerializeRec(item.child, s, indent + 1);
            for (int j = 0; j < indent; j++) {
                s.AppendChar(' ');
                s.AppendChar(' ');
            }
            s.Append("]\n");
        } else {
            s.Append(item.key);
            s.Append(" = ");
            if (item.str) {
                s.Append(item.str);
            }
            s.AppendChar('\n');
        }
    }
}

TempStr SerializeSquareTreeNodeTemp(SquareTreeNode* node) {
    if (!node) {
        return {};
    }
    str::Builder s;
    SerializeRec(node, s, 0);
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
    return ParseSquareTreeRec(data, off, true);
}
