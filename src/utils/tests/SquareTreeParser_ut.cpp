/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/SquareTreeParser.h"

// must be last due to assert() over-write
#include "utils/UtAssert.h"

void SquareTreeTest() {
    static const char* keyValueData[] = {
        UTF8_BOM "key = value",  UTF8_BOM "key = value",    UTF8_BOM "key=value",
        UTF8_BOM " key =value ", UTF8_BOM "  key= value  ", UTF8_BOM "key: value",
        UTF8_BOM "key : value",  UTF8_BOM "key :value",     UTF8_BOM "# key and value:\n\tkey value\n",
        UTF8_BOM "key\t\tvalue",
    };

    for (size_t i = 0; i < dimof(keyValueData); i++) {
        const char* data = keyValueData[i];
        SquareTreeNode* root = ParseSquareTree(data);
        utassert(root && 1 == root->data.size());
        SquareTreeNode::DataItem& item = root->data.at(0);
        utassert(!item.child && str::Eq(item.key, "key") && str::Eq(item.str, "value"));
        utassert(!root->GetChild("key"));
        utassert(str::Eq(root->GetValue("KEY"), "value"));
        size_t off = 0;
        utassert(str::Eq(root->GetValue("key", &off), "value"));
        utassert(!root->GetValue("key", &off));
        delete root;
    }

    static const char* nodeData[] = {
        UTF8_BOM "node [\nkey = value\n]",      UTF8_BOM "node[ # ignore comment\n\tkey: value\n] # end of node\n",
        UTF8_BOM "node\n[\nkey:value",          UTF8_BOM "node\n# node content:\n\t[\n\tkey: value\n\t]\n",
        UTF8_BOM "node [\n  key : value\n]\n]", UTF8_BOM "node[\nkey=value\n]]]",
        UTF8_BOM "[node]\nkey = value\n",       UTF8_BOM "[ node ]\nkey = value\n",
    };

    for (size_t i = 0; i < dimof(nodeData); i++) {
        const char* s = nodeData[i];
        SquareTreeNode* root = ParseSquareTree(s);
        utassert(root && 1 == root->data.size());
        SquareTreeNode::DataItem& item = root->data.at(0);
        utassert(item.child && str::Eq(item.key, "node"));
        utassert(item.child == root->GetChild("NODE"));
        size_t off = 0;
        utassert(item.child == root->GetChild("node", &off));
        utassert(!root->GetChild("node", &off));
        utassert(str::Eq(item.child->GetValue("key"), "value"));
        delete root;
    }

    static const char* arrayData[] = {
        UTF8_BOM "array [\n item = 0 \n] [\n item = 1 \n]",
        UTF8_BOM "array [\n item = 0 \n]\n array [\n item = 1 \n]",
        UTF8_BOM "[array]\n item = 0 \n[array]\n item = 1 \n",
        UTF8_BOM "array [\n item = 0 \n]\n [array]\n item = 1 \n",
    };

    for (size_t i = 0; i < dimof(arrayData); i++) {
        const char* s = arrayData[i];
        SquareTreeNode* root = ParseSquareTree(s);
        utassert(root && 2 == root->data.size());
        size_t off = 0;
        SquareTreeNode* node = root->GetChild("array", &off);
        utassert(node && 1 == node->data.size() && str::Eq(node->GetValue("item"), "0"));
        node = root->GetChild("array", &off);
        utassert(node && 1 == node->data.size() && str::Eq(node->GetValue("item"), "1"));
        node = root->GetChild("array", &off);
        utassert(!node && 2 == off);
        delete root;
    }

    static const char* serArrayData[] = {
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

    for (const char* s : serArrayData) {
        SquareTreeNode* root = ParseSquareTree(s);
        utassert(root && 1 == root->data.size());
        SquareTreeNode* array = root->GetChild("array");
        utassert(2 == array->data.size());
        size_t off = 0;
        SquareTreeNode* node = array->GetChild("", &off);
        utassert(node && 1 == node->data.size() && str::Eq(node->GetValue("item"), "0"));
        node = array->GetChild("", &off);
        utassert(node && 1 == node->data.size() && str::Eq(node->GetValue("item"), "1"));
        node = array->GetChild("", &off);
        utassert(!node && 2 == off);
        delete root;
    }

    static const char* valueArrayData[] = {
        UTF8_BOM "count = 0\ncount = 1",
        UTF8_BOM "count:0\ncount:1\n",
        UTF8_BOM "# first:\n count : 0 \n#second:\n count : 1 \n",
    };

    for (const char* s : valueArrayData) {
        SquareTreeNode* root = ParseSquareTree(s);
        utassert(root && 2 == root->data.size());
        size_t off = 0;
        const char* value = root->GetValue("count", &off);
        utassert(str::Eq(value, "0") && 1 == off);
        value = root->GetValue("count", &off);
        utassert(str::Eq(value, "1") && 2 == off);
        value = root->GetValue("count", &off);
        utassert(!value && 2 == off);
        delete root;
    }

    static const char* emptyNodeData[] = {
        UTF8_BOM "node [\n]", UTF8_BOM "node \n [ \n ] \n", UTF8_BOM "node [", UTF8_BOM "[node] \n",
        UTF8_BOM "[node]",    UTF8_BOM "  [  node  ]  ",
    };

    for (const char* s : emptyNodeData) {
        SquareTreeNode* root = ParseSquareTree(s);
        utassert(root && 1 == root->data.size());
        utassert(root->GetChild("node"));
        utassert(0 == root->GetChild("node")->data.size());
        delete root;
    }

    static const char* halfBrokenData[] = {
        "node [\n child = \n]\n key = value",
        "node [\nchild\n]\n]\n key = value",
        "node[\n[node\nchild\nchild [ node\n]\n key = value",
        "node [\r key = value\n node [\nchild\r\n] key = value",
    };

    for (const char* s : halfBrokenData) {
        SquareTreeNode* root = ParseSquareTree(s);
        utassert(root && 2 == root->data.size());
        utassert(root->GetChild("node") == root->data.at(0).child);
        SquareTreeNode* node = root->GetChild("Node");
        utassert(node && 1 == node->data.size() && str::Eq(node->GetValue("child"), ""));
        utassert(str::Eq(root->GetValue("key"), "value"));
        utassert(!root->GetValue("node") && !root->GetChild("key"));
        delete root;
    }

    {
        const char* s = nullptr;
        SquareTreeNode* root = ParseSquareTree(s);
        utassert(!root);
    }
    {
        const char* s = "";
        SquareTreeNode* root = ParseSquareTree(s);
        utassert(root && 0 == root->data.size());
        delete root;
    }

    {
        const char* s = UTF8_BOM;
        SquareTreeNode* root = ParseSquareTree(s);
        utassert(root && 0 == root->data.size());
        delete root;
    }

    {
        const char* s = UTF8_BOM "node [\n node [\n node [\n node [\n node [\n depth 5 \n]\n]\n]\n]\n]";
        SquareTreeNode* root = ParseSquareTree(s);
        SquareTreeNode* node = root;
        for (size_t i = 0; i < 5; i++) {
            utassert(node && 1 == node->data.size());
            node = node->GetChild("node");
        }
        utassert(node && 1 == node->data.size() && str::Eq(node->GetValue("depth"), "5"));
        delete root;
    }

    {
        const char* s = UTF8_BOM "node1 [\n [node2] \n key:value";
        SquareTreeNode* root = ParseSquareTree(s);
        utassert(root && root->GetChild("node1") && root->GetChild("node2"));
        utassert(0 == root->GetChild("node1")->data.size());
        utassert(str::Eq(root->GetChild("node2")->GetValue("Key"), "value"));
        delete root;
    }
}
