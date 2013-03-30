/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "SquareTreeParser.h"

/*
A 'square tree' is a format for representing string values contained in a
tree structure. Consumers may parse these string values further (as integers,
etc.) as required.

Each non-empty line which doesn't start with either '#' or ';' (comment line)
contains a key and a value, separated usually by '=' or ':'. Values which aren't
a single '[' are parsed as string values to the end of the line; if OTOH a value
is just '[' then the following lines are parsed as a child node until a ']' is
found on a line by itself:

key = value
key2 = [ value containing square brackets (length: 49) ]
child = [
  depth = 1
  subchild: [
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
*/

static inline char *SkipWsRev(char *begin, char *s)
{
    for (; s > begin && str::IsWs(*(s - 1)); s--);
    return s;
}

static char *SkipWsAndComments(char *s)
{
    do {
        for (; str::IsWs(*s); s++);
        if ('#' == *s || ';' == *s) {
            // skip entire comment line
            for (; *s && *s != '\n'; s++);
        }
    } while (str::IsWs(*s));
    return s;
}

static inline bool IsBracketLine(char *s, char bracket)
{
    if (*s != bracket)
        return false;
    for (s++; *s && *s != '\n'; s++) {
        if (!str::IsWs(*s))
            return false;
    }
    return true;
}

SquareTreeNode::~SquareTreeNode()
{
    for (size_t i = 0; i < data.Count(); i++) {
        DataItem& item = data.At(i);
        if (item.isChild)
            delete item.value.child;
    }
}

const char *SquareTreeNode::GetValue(const char *key, size_t idx) const
{
    for (size_t i = 0; i < data.Count(); i++) {
        DataItem& item = data.At(i);
        if (str::EqI(key, item.key) && !item.isChild && 0 == idx--)
            return item.value.str;
    }
    return NULL;
}

SquareTreeNode *SquareTreeNode::GetChild(const char *key, size_t idx) const
{
    for (size_t i = 0; i < data.Count(); i++) {
        DataItem& item = data.At(i);
        if (str::EqI(key, item.key) && item.isChild && 0 == idx--)
            return item.value.child;
    }
    return NULL;
}

static SquareTreeNode *ParseSquareTreeRec(char *& data, bool isRootNode=false)
{
    SquareTreeNode *node = new SquareTreeNode();

    while (*(data = SkipWsAndComments(data))) {
        // all non-empty non-comment lines contain a key-value pair
        // where the value is either a string or a list of
        // child nodes (if the string value would be a single '[')
        // and where key and value are usually separated by '=' or ':' 
        char *key = data;
        for (data = key; *data && *data != '=' && *data != ':' && *data != '[' && *data != ']' && *data != '\n'; data++);
        if (!*data || '\n' == *data) {
            // use first whitespace as a fallback separator
            for (data = key; *data && !str::IsWs(*data); data++);
        }
        char *separator = data;
        if (*data && *data != '[' && *data != ']' && *data != '\n') {
            // skip to the first non-whitespace character on the same line (value)
            for (data++; str::IsWs(*data) && *data != '\n'; data++);
        }
        char *value = data;
        if (IsBracketLine(value, '[') ||
            // also tolerate "key \n [ \n ... \n ]" (else the key
            // gets an empty value and the child node an empty key)
            '\n' == *value && IsBracketLine(SkipWsAndComments(data), '[')) {
            // parse child node(s)
            data = SkipWsAndComments(data) + 1;
            node->data.Append(SquareTreeNode::DataItem(key, ParseSquareTreeRec(data)));
            // array are created by either reusing the same key for a different child
            // or by concatenating multiple children ("[ \n ] [ \n ] [ \n ]")
            while ('[' == *(data = SkipWsAndComments(data))) {
                data++;
                node->data.Append(SquareTreeNode::DataItem(key, ParseSquareTreeRec(data)));
            }
        }
        else if (']' == *key || ']' == *separator && '[' == *SkipWsAndComments(data + 1) ||
            // also tolerate "key ]" (else the key gets ']' as value),
            // not however the explicit "key = ]"
            IsBracketLine(separator, ']')) {
            // finish parsing this node
            data++;
            if (key < separator) {
                // interpret "key ]" as "key =" and "]"
                *SkipWsRev(key, separator) = '\0';
                node->data.Append(SquareTreeNode::DataItem(key, ""));
            }
            if (!isRootNode)
                return node;
            // ignore superfluous closing square brackets instead of
            // ignoring all content following them
        }
        else {
            // string value (decoding is left to the consumer)
            for (; *data && *data != '\n'; data++);
            bool hasMoreLines = '\n' == *data;
            *SkipWsRev(value, data) = '\0';
            node->data.Append(SquareTreeNode::DataItem(key, value));
            if (hasMoreLines)
                data++;
        }
        *SkipWsRev(key, separator) = '\0';
    }

    // assume that all square brackets have been properly balanced
    return node;
}

SquareTree::SquareTree(const char *data) : root(NULL)
{
    // convert the file content to UTF-8
    if (str::StartsWith(data, UTF8_BOM))
        dataUtf8.Set(str::Dup(data + 3));
    else if (str::StartsWith(data, UTF16_BOM))
        dataUtf8.Set(str::conv::ToUtf8((const WCHAR *)(data + 2)));
    else if (data)
        dataUtf8.Set(str::conv::ToUtf8(ScopedMem<WCHAR>(str::conv::FromAnsi(data))));
    if (!dataUtf8)
        return;

    char *start = dataUtf8.Get();
    root = ParseSquareTreeRec(start, true);
    CrashIf(*start);
}
