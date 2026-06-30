/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct SquareTreeNode {
    SquareTreeNode() = default;
    ~SquareTreeNode();

    struct DataItem {
        Str key = {};
        // only one of str or child are set
        Str str = {};
        SquareTreeNode* child = nullptr;

        DataItem() = default;
        DataItem(Str k, Str string) {
            this->key = k;
            this->str = string;
            this->child = nullptr;
        }
        DataItem(Str k, SquareTreeNode* node) {
            this->key = k;
            this->str = {};
            this->child = node;
        }
    };
    Vec<DataItem> data;

    Str GetValue(Str key, size_t* startIdx = nullptr) const;
    SquareTreeNode* GetChild(Str key, size_t* startIdx = nullptr) const;
};

SquareTreeNode* ParseSquareTree(Str s);
Str SerializeSquareTreeNode(SquareTreeNode*);