/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "SquareTreeParser.h"
#include "UtAssert.h"

void SquareTreeTest()
{
    static const char *keyValueData[] = {
        UTF8_BOM "key = value",
        UTF8_BOM "key = value",
        UTF8_BOM "key=value",
        UTF8_BOM " key =value ",
        UTF8_BOM "  key= value  ",
        UTF8_BOM "key: value",
        UTF8_BOM "key : value",
        UTF8_BOM "key :value",
        UTF8_BOM "# key and value:\n\tkey value\n",
        UTF8_BOM "key\t\tvalue",
    };

    for (size_t i = 0; i < dimof(keyValueData); i++) {
        SquareTree keyValue(keyValueData[i]);
        assert(keyValue.root && 1 == keyValue.root->data.Count());
        SquareTreeNode::DataItem& item = keyValue.root->data.At(0);
        assert(!item.isChild && str::Eq(item.key, "key") && str::Eq(item.value.str, "value"));
        assert(!keyValue.root->GetChild("key"));
        assert(str::Eq(keyValue.root->GetValue("KEY"), "value"));
        size_t off = 0;
        assert(str::Eq(keyValue.root->GetValue("key", &off), "value"));
        assert(!keyValue.root->GetValue("key", &off));
    }

    static const char *nodeData[] = {
        UTF8_BOM "node [\nkey = value\n]",
        UTF8_BOM "node[ # ignore comment\n\tkey: value\n] # end of node\n",
        UTF8_BOM "node\n[\nkey:value",
        UTF8_BOM "node\n# node content:\n\t[\n\tkey: value\n\t]\n",
        UTF8_BOM "node [\n  key : value\n]\n]",
        UTF8_BOM "node[\nkey=value\n]]]",
        UTF8_BOM "[node]\nkey = value\n",
        UTF8_BOM "[ node ]\nkey = value\n",
    };

    for (size_t i = 0; i < dimof(nodeData); i++) {
        SquareTree node(nodeData[i]);
        assert(node.root && 1 == node.root->data.Count());
        SquareTreeNode::DataItem& item = node.root->data.At(0);
        assert(item.isChild && str::Eq(item.key, "node"));
        assert(item.value.child == node.root->GetChild("NODE"));
        size_t off = 0;
        assert(item.value.child == node.root->GetChild("node", &off));
        assert(!node.root->GetChild("node", &off));
        assert(str::Eq(item.value.child->GetValue("key"), "value"));
    }

    static const char *arrayData[] = {
        UTF8_BOM "array [\n item = 0 \n] [\n item = 1 \n]",
        UTF8_BOM "array [\n item = 0 \n]\n array [\n item = 1 \n]",
        UTF8_BOM "[array]\n item = 0 \n[array]\n item = 1 \n",
        UTF8_BOM "array [\n item = 0 \n]\n [array]\n item = 1 \n",
    };

    for (size_t i = 0; i < dimof(arrayData); i++) {
        SquareTree array(arrayData[i]);
        assert(array.root && 2 == array.root->data.Count());
        size_t off = 0;
        SquareTreeNode *node = array.root->GetChild("array", &off);
        assert(node && 1 == node->data.Count() && str::Eq(node->GetValue("item"), "0"));
        node = array.root->GetChild("array", &off);
        assert(node && 1 == node->data.Count() && str::Eq(node->GetValue("item"), "1"));
        node = array.root->GetChild("array", &off);
        assert(!node && 2 == off);
    }

    static const char *serArrayData[] = {
        UTF8_BOM "array [\n[\n item = 0 \n]\n[\n item = 1 \n]\n]\n",
        UTF8_BOM "array [\n[\n item = 0 \n] [\n item = 1 \n]]",
        UTF8_BOM "array \n# serialized array with two items: \n[\n"
            "# first item: \n[\n item = 0 \n] # end of first item\n"
            "# second item: \n[\n item = 1 \n] # end of second item\n"
            "] # end of array",
        UTF8_BOM "array [\n[\n item = 0 \n] [\n item = 1",
        UTF8_BOM "[array]\n[\n item = 0 \n] [\n item = 1 \n]",
    };

    for (size_t i = 0; i < dimof(serArrayData); i++) {
        SquareTree serArray(serArrayData[i]);
        assert(serArray.root && 1 == serArray.root->data.Count());
        SquareTreeNode *array = serArray.root->GetChild("array");
        assert(2 == array->data.Count());
        size_t off = 0;
        SquareTreeNode *node = array->GetChild("", &off);
        assert(node && 1 == node->data.Count() && str::Eq(node->GetValue("item"), "0"));
        node = array->GetChild("", &off);
        assert(node && 1 == node->data.Count() && str::Eq(node->GetValue("item"), "1"));
        node = array->GetChild("", &off);
        assert(!node && 2 == off);
    }

    static const char *valueArrayData[] = {
        UTF8_BOM "count = 0\ncount = 1",
        UTF8_BOM "count:0\ncount:1\n",
        UTF8_BOM "# first:\n count : 0 \n#second:\n count : 1 \n",
    };

    for (size_t i = 0; i < dimof(valueArrayData); i++) {
        SquareTree valueArray(valueArrayData[i]);
        assert(valueArray.root && 2 == valueArray.root->data.Count());
        size_t off = 0;
        const char *value = valueArray.root->GetValue("count", &off);
        assert(str::Eq(value, "0") && 1 == off);
        value = valueArray.root->GetValue("count", &off);
        assert(str::Eq(value, "1") && 2 == off);
        value = valueArray.root->GetValue("count", &off);
        assert(!value && 2 == off);
    }

    static const char *emptyNodeData[] = {
        UTF8_BOM "node [\n]",
        UTF8_BOM "node \n [ \n ] \n",
        UTF8_BOM "node [",
        UTF8_BOM "[node] \n",
        UTF8_BOM "[node]",
        UTF8_BOM "  [  node  ]  ",
    };

    for (size_t i = 0; i < dimof(emptyNodeData); i++) {
        SquareTree emptyNode(emptyNodeData[i]);
        assert(emptyNode.root && 1 == emptyNode.root->data.Count());
        assert(emptyNode.root->GetChild("node"));
        assert(0 == emptyNode.root->GetChild("node")->data.Count());
    }

    static const char *halfBrokenData[] = {
        "node [\n child = \n]\n key = value",
        "node [\nchild\n]\n]\n key = value",
        "node[\n[node\nchild\nchild [ node\n]\n key = value",
        "node [\r key = value\n node [\nchild\r\n] key = value",
    };

    for (size_t i = 0; i < dimof(halfBrokenData); i++) {
        SquareTree halfBroken(halfBrokenData[i]);
        assert(halfBroken.root && 2 == halfBroken.root->data.Count());
        assert(halfBroken.root->GetChild("node") == halfBroken.root->data.At(0).value.child);
        SquareTreeNode *node = halfBroken.root->GetChild("Node");
        assert(node && 1 == node->data.Count() && str::Eq(node->GetValue("child"), ""));
        assert(str::Eq(halfBroken.root->GetValue("key"), "value"));
        assert(!halfBroken.root->GetValue("node") && !halfBroken.root->GetChild("key"));
    }

    SquareTree null(NULL);
    assert(!null.root);

    SquareTree empty("");
    assert(empty.root && 0 == empty.root->data.Count());

    SquareTree onlyBom(UTF8_BOM);
    assert(onlyBom.root && 0 == onlyBom.root->data.Count());

    SquareTree nested(UTF8_BOM "node [\n node [\n node [\n node [\n node [\n depth 5 \n]\n]\n]\n]\n]");
    SquareTreeNode *node = nested.root;
    for (size_t i = 0; i < 5; i++) {
        assert(node && 1 == node->data.Count());
        node = node->GetChild("node");
    }
    assert(node && 1 == node->data.Count() && str::Eq(node->GetValue("depth"), "5"));

    SquareTree mixed(UTF8_BOM "node1 [\n [node2] \n key:value");
    assert(mixed.root && mixed.root->GetChild("node1") && mixed.root->GetChild("node2"));
    assert(0 == mixed.root->GetChild("node1")->data.Count());
    assert(str::Eq(mixed.root->GetChild("node2")->GetValue("Key"), "value"));
}
