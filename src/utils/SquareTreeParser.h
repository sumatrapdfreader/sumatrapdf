/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct SquareTreeNode {
    SquareTreeNode() = default;
    ~SquareTreeNode();

    struct DataItem {
        const char* key = nullptr;
        // only one of str or child are set
        const char* str = nullptr;
        SquareTreeNode* child = nullptr;

        DataItem() = default;
        DataItem(const char* k, const char* string) {
            this->key = k;
            this->str = string;
            this->child = nullptr;
        }
        DataItem(const char* k, SquareTreeNode* node) {
            this->key = k;
            this->str = nullptr;
            this->child = node;
        }
    };
    Vec<DataItem> data;

    const char* GetValue(const char* key, size_t* startIdx = nullptr) const;
    SquareTreeNode* GetChild(const char* key, size_t* startIdx = nullptr) const;
};

SquareTreeNode* ParseSquareTree(const char* s);
