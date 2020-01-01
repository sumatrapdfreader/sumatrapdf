/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
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
        SquareTree keyValue(keyValueData[i]);
        utassert(keyValue.root && 1 == keyValue.root->data.size());
        SquareTreeNode::DataItem& item = keyValue.root->data.at(0);
        utassert(!item.isChild && str::Eq(item.key, "key") && str::Eq(item.value.str, "value"));
        utassert(!keyValue.root->GetChild("key"));
        utassert(str::Eq(keyValue.root->GetValue("KEY"), "value"));
        size_t off = 0;
        utassert(str::Eq(keyValue.root->GetValue("key", &off), "value"));
        utassert(!keyValue.root->GetValue("key", &off));
    }

    static const char* nodeData[] = {
        UTF8_BOM "node [\nkey = value\n]",      UTF8_BOM "node[ # ignore comment\n\tkey: value\n] # end of node\n",
        UTF8_BOM "node\n[\nkey:value",          UTF8_BOM "node\n# node content:\n\t[\n\tkey: value\n\t]\n",
        UTF8_BOM "node [\n  key : value\n]\n]", UTF8_BOM "node[\nkey=value\n]]]",
        UTF8_BOM "[node]\nkey = value\n",       UTF8_BOM "[ node ]\nkey = value\n",
    };

    for (size_t i = 0; i < dimof(nodeData); i++) {
        SquareTree node(nodeData[i]);
        utassert(node.root && 1 == node.root->data.size());
        SquareTreeNode::DataItem& item = node.root->data.at(0);
        utassert(item.isChild && str::Eq(item.key, "node"));
        utassert(item.value.child == node.root->GetChild("NODE"));
        size_t off = 0;
        utassert(item.value.child == node.root->GetChild("node", &off));
        utassert(!node.root->GetChild("node", &off));
        utassert(str::Eq(item.value.child->GetValue("key"), "value"));
    }

    static const char* arrayData[] = {
        UTF8_BOM "array [\n item = 0 \n] [\n item = 1 \n]",
        UTF8_BOM "array [\n item = 0 \n]\n array [\n item = 1 \n]",
        UTF8_BOM "[array]\n item = 0 \n[array]\n item = 1 \n",
        UTF8_BOM "array [\n item = 0 \n]\n [array]\n item = 1 \n",
    };

    for (size_t i = 0; i < dimof(arrayData); i++) {
        SquareTree array(arrayData[i]);
        utassert(array.root && 2 == array.root->data.size());
        size_t off = 0;
        SquareTreeNode* node = array.root->GetChild("array", &off);
        utassert(node && 1 == node->data.size() && str::Eq(node->GetValue("item"), "0"));
        node = array.root->GetChild("array", &off);
        utassert(node && 1 == node->data.size() && str::Eq(node->GetValue("item"), "1"));
        node = array.root->GetChild("array", &off);
        utassert(!node && 2 == off);
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

    for (size_t i = 0; i < dimof(serArrayData); i++) {
        SquareTree serArray(serArrayData[i]);
        utassert(serArray.root && 1 == serArray.root->data.size());
        SquareTreeNode* array = serArray.root->GetChild("array");
        utassert(2 == array->data.size());
        size_t off = 0;
        SquareTreeNode* node = array->GetChild("", &off);
        utassert(node && 1 == node->data.size() && str::Eq(node->GetValue("item"), "0"));
        node = array->GetChild("", &off);
        utassert(node && 1 == node->data.size() && str::Eq(node->GetValue("item"), "1"));
        node = array->GetChild("", &off);
        utassert(!node && 2 == off);
    }

    static const char* valueArrayData[] = {
        UTF8_BOM "count = 0\ncount = 1",
        UTF8_BOM "count:0\ncount:1\n",
        UTF8_BOM "# first:\n count : 0 \n#second:\n count : 1 \n",
    };

    for (size_t i = 0; i < dimof(valueArrayData); i++) {
        SquareTree valueArray(valueArrayData[i]);
        utassert(valueArray.root && 2 == valueArray.root->data.size());
        size_t off = 0;
        const char* value = valueArray.root->GetValue("count", &off);
        utassert(str::Eq(value, "0") && 1 == off);
        value = valueArray.root->GetValue("count", &off);
        utassert(str::Eq(value, "1") && 2 == off);
        value = valueArray.root->GetValue("count", &off);
        utassert(!value && 2 == off);
    }

    static const char* emptyNodeData[] = {
        UTF8_BOM "node [\n]", UTF8_BOM "node \n [ \n ] \n", UTF8_BOM "node [", UTF8_BOM "[node] \n",
        UTF8_BOM "[node]",    UTF8_BOM "  [  node  ]  ",
    };

    for (size_t i = 0; i < dimof(emptyNodeData); i++) {
        SquareTree emptyNode(emptyNodeData[i]);
        utassert(emptyNode.root && 1 == emptyNode.root->data.size());
        utassert(emptyNode.root->GetChild("node"));
        utassert(0 == emptyNode.root->GetChild("node")->data.size());
    }

    static const char* halfBrokenData[] = {
        "node [\n child = \n]\n key = value",
        "node [\nchild\n]\n]\n key = value",
        "node[\n[node\nchild\nchild [ node\n]\n key = value",
        "node [\r key = value\n node [\nchild\r\n] key = value",
    };

    for (size_t i = 0; i < dimof(halfBrokenData); i++) {
        SquareTree halfBroken(halfBrokenData[i]);
        utassert(halfBroken.root && 2 == halfBroken.root->data.size());
        utassert(halfBroken.root->GetChild("node") == halfBroken.root->data.at(0).value.child);
        SquareTreeNode* node = halfBroken.root->GetChild("Node");
        utassert(node && 1 == node->data.size() && str::Eq(node->GetValue("child"), ""));
        utassert(str::Eq(halfBroken.root->GetValue("key"), "value"));
        utassert(!halfBroken.root->GetValue("node") && !halfBroken.root->GetChild("key"));
    }

    SquareTree null(nullptr);
    utassert(!null.root);

    SquareTree empty("");
    utassert(empty.root && 0 == empty.root->data.size());

    SquareTree onlyBom(UTF8_BOM);
    utassert(onlyBom.root && 0 == onlyBom.root->data.size());

    SquareTree nested(UTF8_BOM "node [\n node [\n node [\n node [\n node [\n depth 5 \n]\n]\n]\n]\n]");
    SquareTreeNode* node = nested.root;
    for (size_t i = 0; i < 5; i++) {
        utassert(node && 1 == node->data.size());
        node = node->GetChild("node");
    }
    utassert(node && 1 == node->data.size() && str::Eq(node->GetValue("depth"), "5"));

    SquareTree mixed(UTF8_BOM "node1 [\n [node2] \n key:value");
    utassert(mixed.root && mixed.root->GetChild("node1") && mixed.root->GetChild("node2"));
    utassert(0 == mixed.root->GetChild("node1")->data.size());
    utassert(str::Eq(mixed.root->GetChild("node2")->GetValue("Key"), "value"));
}
