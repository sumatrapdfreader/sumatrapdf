/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

class SquareTreeNode {
  public:
    SquareTreeNode() {
    }
    ~SquareTreeNode();

    struct DataItem {
        const char* key = nullptr;
        union {
            const char* str;
            SquareTreeNode* child;
        } value;
        bool isChild = false;

        DataItem() = default;
        DataItem(const char* k, const char* string) {
            key = k;
            value.str = string;
        }
        DataItem(const char* k, SquareTreeNode* node) {
            key = k;
            isChild = true;
            value.child = node;
        }
    };
    Vec<DataItem> data;

    const char* GetValue(const char* key, size_t* startIdx = nullptr) const;
    SquareTreeNode* GetChild(const char* key, size_t* startIdx = nullptr) const;
};

class SquareTree {
    AutoFree dataUtf8;

  public:
    explicit SquareTree(const char* data);
    ~SquareTree() {
        delete root;
    }

    SquareTreeNode* root;
};
