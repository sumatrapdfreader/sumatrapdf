/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/SquareTreeParser.h"

// must be last due to assert() over-write
#include "base/UtAssert.h"

void SquareTreeTest() {
    static Str keyValueData[] = {
        UTF8_BOM "key = value",  UTF8_BOM "key = value",    UTF8_BOM "key=value",
        UTF8_BOM " key =value ", UTF8_BOM "  key= value  ", UTF8_BOM "key: value",
        UTF8_BOM "key : value",  UTF8_BOM "key :value",     UTF8_BOM "# key and value:\n\tkey value\n",
        UTF8_BOM "key\t\tvalue",
    };

    for (size_t i = 0; i < dimof(keyValueData); i++) {
        Str data = keyValueData[i];
        SquareTreeNode* root = ParseSquareTree(data);
        utassert(root && 1 == len(root->data));
        SquareTreeNode::DataItem& item = root->data[0];
        utassert(!item.child && str::Eq(item.key, "key") && str::Eq(item.str, "value"));
        utassert(!root->GetChild(StrL("key")));
        utassert(str::Eq(root->GetValue(StrL("KEY")), "value"));
        size_t off = 0;
        utassert(str::Eq(root->GetValue(StrL("key"), &off), "value"));
        utassert(!root->GetValue(StrL("key"), &off));
        delete root;
    }

    static Str nodeData[] = {
        UTF8_BOM "node [\nkey = value\n]",      UTF8_BOM "node[ # ignore comment\n\tkey: value\n] # end of node\n",
        UTF8_BOM "node\n[\nkey:value",          UTF8_BOM "node\n# node content:\n\t[\n\tkey: value\n\t]\n",
        UTF8_BOM "node [\n  key : value\n]\n]", UTF8_BOM "node[\nkey=value\n]]]",
        UTF8_BOM "[node]\nkey = value\n",       UTF8_BOM "[ node ]\nkey = value\n",
    };

    for (size_t i = 0; i < dimof(nodeData); i++) {
        Str s = nodeData[i];
        SquareTreeNode* root = ParseSquareTree(s);
        utassert(root && 1 == len(root->data));
        SquareTreeNode::DataItem& item = root->data[0];
        utassert(item.child && str::Eq(item.key, "node"));
        utassert(item.child == root->GetChild(StrL("NODE")));
        size_t off = 0;
        utassert(item.child == root->GetChild(StrL("node"), &off));
        utassert(!root->GetChild(StrL("node"), &off));
        utassert(str::Eq(item.child->GetValue(StrL("key")), "value"));
        delete root;
    }

    static Str arrayData[] = {
        UTF8_BOM "array [\n item = 0 \n] [\n item = 1 \n]",
        UTF8_BOM "array [\n item = 0 \n]\n array [\n item = 1 \n]",
        UTF8_BOM "[array]\n item = 0 \n[array]\n item = 1 \n",
        UTF8_BOM "array [\n item = 0 \n]\n [array]\n item = 1 \n",
    };

    for (size_t i = 0; i < dimof(arrayData); i++) {
        Str s = arrayData[i];
        SquareTreeNode* root = ParseSquareTree(s);
        utassert(root && 2 == len(root->data));
        size_t off = 0;
        SquareTreeNode* node = root->GetChild(StrL("array"), &off);
        utassert(node && 1 == len(node->data) && str::Eq(node->GetValue(StrL("item")), "0"));
        node = root->GetChild(StrL("array"), &off);
        utassert(node && 1 == len(node->data) && str::Eq(node->GetValue(StrL("item")), "1"));
        node = root->GetChild(StrL("array"), &off);
        utassert(!node && 2 == off);
        delete root;
    }

    static Str serArrayData[] = {
        UTF8_BOM "array [\n[\n item = 0 \n]\n[\n item = 1 \n]\n]\n",
        UTF8_BOM "array [\n[\n item = 0 \n] [\n item = 1 \n]]",
        UTF8_BOM
        "array \n# serialized array with two items: \n[\n"
        "# first item: \n[\n item = 0 \n] # end of first item\n"
        "# second item: \n[\n item = 1 \n] # end of second item\n"
        "] # end of array",
        UTF8_BOM "array [\n[\n item = 0 \n] [\n item = 1",
        UTF8_BOM "[array]\n[\n item = 0 \n] [\n item = 1 \n]",
    };

    for (Str s : serArrayData) {
        SquareTreeNode* root = ParseSquareTree(s);
        utassert(root && 1 == len(root->data));
        SquareTreeNode* array = root->GetChild(StrL("array"));
        utassert(2 == len(array->data));
        size_t off = 0;
        SquareTreeNode* node = array->GetChild(StrL(""), &off);
        utassert(node && 1 == len(node->data) && str::Eq(node->GetValue(StrL("item")), "0"));
        node = array->GetChild(StrL(""), &off);
        utassert(node && 1 == len(node->data) && str::Eq(node->GetValue(StrL("item")), "1"));
        node = array->GetChild(StrL(""), &off);
        utassert(!node && 2 == off);
        delete root;
    }

    static Str valueArrayData[] = {
        UTF8_BOM "count = 0\ncount = 1",
        UTF8_BOM "count:0\ncount:1\n",
        UTF8_BOM "# first:\n count : 0 \n#second:\n count : 1 \n",
    };

    for (Str s : valueArrayData) {
        SquareTreeNode* root = ParseSquareTree(s);
        utassert(root && 2 == len(root->data));
        size_t off = 0;
        Str value = root->GetValue(StrL("count"), &off);
        utassert(str::Eq(value, "0") && 1 == off);
        value = root->GetValue(StrL("count"), &off);
        utassert(str::Eq(value, "1") && 2 == off);
        value = root->GetValue(StrL("count"), &off);
        utassert(!value && 2 == off);
        delete root;
    }

    static Str emptyNodeData[] = {
        UTF8_BOM "node [\n]", UTF8_BOM "node \n [ \n ] \n", UTF8_BOM "node [", UTF8_BOM "[node] \n",
        UTF8_BOM "[node]",    UTF8_BOM "  [  node  ]  ",
    };

    for (Str s : emptyNodeData) {
        SquareTreeNode* root = ParseSquareTree(s);
        utassert(root && 1 == len(root->data));
        utassert(root->GetChild(StrL("node")));
        utassert(0 == len(root->GetChild(StrL("node"))->data));
        delete root;
    }

    static Str halfBrokenData[] = {
        "node [\n child = \n]\n key = value",
        "node [\nchild\n]\n]\n key = value",
        "node[\n[node\nchild\nchild [ node\n]\n key = value",
        "node [\r key = value\n node [\nchild\r\n] key = value",
    };

    for (Str s : halfBrokenData) {
        SquareTreeNode* root = ParseSquareTree(s);
        utassert(root && 2 == len(root->data));
        utassert(root->GetChild(StrL("node")) == root->data[0].child);
        SquareTreeNode* node = root->GetChild(StrL("Node"));
        utassert(node && 1 == len(node->data) && str::Eq(node->GetValue(StrL("child")), ""));
        utassert(str::Eq(root->GetValue(StrL("key")), "value"));
        utassert(!root->GetValue(StrL("node")) && !root->GetChild(StrL("key")));
        delete root;
    }

    {
        Str s;
        SquareTreeNode* root = ParseSquareTree(s);
        utassert(!root);
    }
    {
        Str s = "";
        SquareTreeNode* root = ParseSquareTree(s);
        utassert(root && 0 == len(root->data));
        delete root;
    }

    {
        Str s = UTF8_BOM;
        SquareTreeNode* root = ParseSquareTree(s);
        utassert(root && 0 == len(root->data));
        delete root;
    }

    {
        Str s = UTF8_BOM "node [\n node [\n node [\n node [\n node [\n depth 5 \n]\n]\n]\n]\n]";
        SquareTreeNode* root = ParseSquareTree(s);
        SquareTreeNode* node = root;
        for (size_t i = 0; i < 5; i++) {
            utassert(node && 1 == len(node->data));
            node = node->GetChild(StrL("node"));
        }
        utassert(node && 1 == len(node->data) && str::Eq(node->GetValue(StrL("depth")), "5"));
        delete root;
    }

    {
        Str s = UTF8_BOM "node1 [\n [node2] \n key:value";
        SquareTreeNode* root = ParseSquareTree(s);
        utassert(root && root->GetChild(StrL("node1")) && root->GetChild(StrL("node2")));
        utassert(0 == len(root->GetChild(StrL("node1"))->data));
        utassert(str::Eq(root->GetChild(StrL("node2"))->GetValue(StrL("Key")), "value"));
        delete root;
    }
}
