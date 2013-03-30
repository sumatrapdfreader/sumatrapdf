/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef SquareTreeParser_h
#define SquareTreeParser_h

class SquareTreeNode {
public:
    SquareTreeNode() { }
    ~SquareTreeNode();

    struct DataItem {
        const char *key;
        union {
            const char *str;
            SquareTreeNode *child;
        } value;
        bool isChild;

        DataItem() : key(NULL) { }
        DataItem(const char *key, const char *string) : key(key), isChild(false) { value.str = string; }
        DataItem(const char *key, SquareTreeNode *node) : key(key), isChild(true) { value.child = node; }
    };
    Vec<DataItem> data;

    const char *GetValue(const char *key, size_t idx=0) const;
    SquareTreeNode *GetChild(const char *key, size_t idx=0) const;
};

class SquareTree {
    ScopedMem<char> dataUtf8;

public:
    SquareTree(const char *data);
    ~SquareTree() { delete root; }

    SquareTreeNode *root;
};

#endif
