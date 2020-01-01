/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
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

static inline char* SkipWs(char* s, bool stopAtLineEnd = false) {
    for (; str::IsWs(*s) && (!stopAtLineEnd || *s != '\n'); s++)
        ;
    return s;
}
static inline char* SkipWsRev(char* begin, char* s) {
    for (; s > begin && str::IsWs(*(s - 1)); s--)
        ;
    return s;
}

static char* SkipWsAndComments(char* s) {
    do {
        s = SkipWs(s);
        if ('#' == *s || ';' == *s) {
            // skip entire comment line
            for (; *s && *s != '\n'; s++)
                ;
        }
    } while (str::IsWs(*s));
    return s;
}

static bool IsBracketLine(char* s) {
    if (*s != '[')
        return false;
    // the line may only contain whitespace and a comment
    for (s++; *s && *s != '\n' && *s != '#' && *s != ';'; s++) {
        if (!str::IsWs(*s))
            return false;
    }
    return true;
}

SquareTreeNode::~SquareTreeNode() {
    for (size_t i = 0; i < data.size(); i++) {
        DataItem& item = data.at(i);
        if (item.isChild)
            delete item.value.child;
    }
}

const char* SquareTreeNode::GetValue(const char* key, size_t* startIdx) const {
    for (size_t i = startIdx ? *startIdx : 0; i < data.size(); i++) {
        DataItem& item = data.at(i);
        if (str::EqI(key, item.key) && !item.isChild) {
            if (startIdx)
                *startIdx = i + 1;
            return item.value.str;
        }
    }
    return nullptr;
}

SquareTreeNode* SquareTreeNode::GetChild(const char* key, size_t* startIdx) const {
    for (size_t i = startIdx ? *startIdx : 0; i < data.size(); i++) {
        DataItem& item = data.at(i);
        if (str::EqI(key, item.key) && item.isChild) {
            if (startIdx)
                *startIdx = i + 1;
            return item.value.child;
        }
    }
    return nullptr;
}

static SquareTreeNode* ParseSquareTreeRec(char*& data, bool isTopLevel = false) {
    SquareTreeNode* node = new SquareTreeNode();

    while (*(data = SkipWsAndComments(data))) {
        // all non-empty non-comment lines contain a key-value pair
        // where the value is either a string (separated by '=' or ':')
        // or a list of child nodes (if the key is followed by '[' alone)
        char* key = data;
        for (data = key; *data && *data != '=' && *data != ':' && *data != '[' && *data != ']' && *data != '\n'; data++)
            ;
        if (!*data || '\n' == *data) {
            // use first whitespace as a fallback separator
            for (data = key; *data && !str::IsWs(*data); data++)
                ;
        }
        char* separator = data;
        if (*data && *data != '\n') {
            // skip to the first non-whitespace character on the same line (value)
            data = SkipWs(data + 1, true);
        }
        char* value = data;
        // skip to the end of the line
        for (; *data && *data != '\n'; data++)
            ;
        if (IsBracketLine(separator) ||
            // also tolerate "key \n [ \n ... \n ]" (else the key
            // gets an empty value and the child node an empty key)
            str::IsWs(*separator) && '\n' == *value && IsBracketLine(SkipWsAndComments(data))) {
            // parse child node(s)
            data = SkipWsAndComments(separator) + 1;
            *SkipWsRev(key, separator) = '\0';
            node->data.Append(SquareTreeNode::DataItem(key, ParseSquareTreeRec(data)));
            // arrays are created by either reusing the same key for a different child
            // or by concatenating multiple children ("[ \n ] [ \n ] [ \n ]")
            while (IsBracketLine((data = SkipWsAndComments(data)))) {
                data++;
                node->data.Append(SquareTreeNode::DataItem(key, ParseSquareTreeRec(data)));
            }
        } else if (']' == *key) {
            // finish parsing child node
            data = key + 1;
            if (!isTopLevel)
                return node;
            // ignore superfluous closing square brackets instead of
            // ignoring all content following them
        } else if ('[' == *key && ']' == SkipWsRev(value, data)[-1]) {
            // treat INI section headers as top-level node names
            // (else "[Section]" would be ignored)
            if (!isTopLevel) {
                data = key;
                return node;
            }
            // trim whitespace around section name (for consistency with GetPrivateProfileString)
            key = SkipWs(key + 1);
            *SkipWsRev(key, SkipWsRev(value, data) - 1) = '\0';
            node->data.Append(SquareTreeNode::DataItem(key, ParseSquareTreeRec(data)));
        } else if ('[' == *separator || ']' == *separator) {
            // invalid line (ignored)
        } else {
            // string value (decoding is left to the consumer)
            bool hasMoreLines = '\n' == *data;
            *SkipWsRev(key, separator) = '\0';
            *SkipWsRev(value, data) = '\0';
            node->data.Append(SquareTreeNode::DataItem(key, value));
            if (hasMoreLines)
                data++;
        }
    }

    // assume that all square brackets have been properly balanced
    return node;
}

SquareTree::SquareTree(const char* data) : root(nullptr) {
    // convert the file content to UTF-8
    if (str::StartsWith(data, UTF8_BOM)) {
        dataUtf8.SetCopy(data + 3);
    } else if (str::StartsWith(data, UTF16_BOM)) {
        auto tmp = strconv::WstrToUtf8((const WCHAR*)(data + 2));
        dataUtf8.Set(tmp.data());
    } else if (data) {
        AutoFreeWstr tmp(strconv::FromAnsi(data));
        auto tmp2 = strconv::WstrToUtf8(tmp.Get());
        dataUtf8.Set(tmp2.data());
    }
    if (!dataUtf8) {
        return;
    }

    char* start = dataUtf8.Get();
    root = ParseSquareTreeRec(start, true);
    CrashIf(*start || !root);
}
