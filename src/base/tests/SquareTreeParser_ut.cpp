/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/SquareTreeParser.h"

// must be last due to assert() over-write
#include "base/UtAssert.h"

void SquareTreeTest() {
    static Str keyValueData[] = {
        Str(kUtf8Bom "key = value"),  Str(kUtf8Bom "key = value"),    Str(kUtf8Bom "key=value"),
        Str(kUtf8Bom " key =value "), Str(kUtf8Bom "  key= value  "), Str(kUtf8Bom "key: value"),
        Str(kUtf8Bom "key : value"),  Str(kUtf8Bom "key :value"),     Str(kUtf8Bom "# key and value:\n\tkey value\n"),
        Str(kUtf8Bom "key\t\tvalue"),
    };

    for (size_t i = 0; i < dimof(keyValueData); i++) {
        Str data = keyValueData[i];
        SquareTreeNode* root = ParseSquareTree(data);
        utassert(root && 1 == len(root->data));
        SquareTreeNode::DataItem* item = root->data[0];
        utassert(!item->child && str::Eq(item->key, StrL("key")) && str::Eq(item->str, StrL("value")));
        utassert(!root->GetChild(StrL("key")));
        utassert(str::Eq(root->GetValue(StrL("KEY")), StrL("value")));
        int off = 0;
        utassert(str::Eq(root->GetValue(StrL("key"), &off), StrL("value")));
        utassert(len(root->GetValue(StrL("key"), &off)) == 0);
        delete root;
    }

    static Str nodeData[] = {
        Str(kUtf8Bom "node [\nkey = value\n]"),
        Str(kUtf8Bom "node[ # ignore comment\n\tkey: value\n] # end of node\n"),
        Str(kUtf8Bom "node\n[\nkey:value"),
        Str(kUtf8Bom "node\n# node content:\n\t[\n\tkey: value\n\t]\n"),
        Str(kUtf8Bom "node [\n  key : value\n]\n]"),
        Str(kUtf8Bom "node[\nkey=value\n]]]"),
        Str(kUtf8Bom "[node]\nkey = value\n"),
        Str(kUtf8Bom "[ node ]\nkey = value\n"),
    };

    for (size_t i = 0; i < dimof(nodeData); i++) {
        Str s = nodeData[i];
        SquareTreeNode* root = ParseSquareTree(s);
        utassert(root && 1 == len(root->data));
        SquareTreeNode::DataItem* item = root->data[0];
        utassert(item->child && str::Eq(item->key, StrL("node")));
        utassert(item->child == root->GetChild(StrL("NODE")));
        int off = 0;
        utassert(item->child == root->GetChild(StrL("node"), &off));
        utassert(!root->GetChild(StrL("node"), &off));
        utassert(str::Eq(item->child->GetValue(StrL("key")), StrL("value")));
        delete root;
    }

    static Str arrayData[] = {
        Str(kUtf8Bom "array [\n item = 0 \n] [\n item = 1 \n]"),
        Str(kUtf8Bom "array [\n item = 0 \n]\n array [\n item = 1 \n]"),
        Str(kUtf8Bom "[array]\n item = 0 \n[array]\n item = 1 \n"),
        Str(kUtf8Bom "array [\n item = 0 \n]\n [array]\n item = 1 \n"),
    };

    for (size_t i = 0; i < dimof(arrayData); i++) {
        Str s = arrayData[i];
        SquareTreeNode* root = ParseSquareTree(s);
        utassert(root && 2 == len(root->data));
        int off = 0;
        SquareTreeNode* node = root->GetChild(StrL("array"), &off);
        utassert(node && 1 == len(node->data) && str::Eq(node->GetValue(StrL("item")), StrL("0")));
        node = root->GetChild(StrL("array"), &off);
        utassert(node && 1 == len(node->data) && str::Eq(node->GetValue(StrL("item")), StrL("1")));
        node = root->GetChild(StrL("array"), &off);
        utassert(!node && 2 == off);
        delete root;
    }

    static Str serArrayData[] = {
        Str(kUtf8Bom "array [\n[\n item = 0 \n]\n[\n item = 1 \n]\n]\n"),
        Str(kUtf8Bom "array [\n[\n item = 0 \n] [\n item = 1 \n]]"),
        Str(kUtf8Bom "array \n# serialized array with two items: \n[\n"
                     "# first item: \n[\n item = 0 \n] # end of first item\n"
                     "# second item: \n[\n item = 1 \n] # end of second item\n"
                     "] # end of array"),
        Str(kUtf8Bom "array [\n[\n item = 0 \n] [\n item = 1"),
        Str(kUtf8Bom "[array]\n[\n item = 0 \n] [\n item = 1 \n]"),
    };

    for (Str s : serArrayData) {
        SquareTreeNode* root = ParseSquareTree(s);
        utassert(root && 1 == len(root->data));
        SquareTreeNode* array = root->GetChild(StrL("array"));
        utassert(2 == len(array->data));
        int off = 0;
        SquareTreeNode* node = array->GetChild(StrL(""), &off);
        utassert(node && 1 == len(node->data) && str::Eq(node->GetValue(StrL("item")), StrL("0")));
        node = array->GetChild(StrL(""), &off);
        utassert(node && 1 == len(node->data) && str::Eq(node->GetValue(StrL("item")), StrL("1")));
        node = array->GetChild(StrL(""), &off);
        utassert(!node && 2 == off);
        delete root;
    }

    static Str valueArrayData[] = {
        Str(kUtf8Bom "count = 0\ncount = 1"),
        Str(kUtf8Bom "count:0\ncount:1\n"),
        Str(kUtf8Bom "# first:\n count : 0 \n#second:\n count : 1 \n"),
    };

    for (Str s : valueArrayData) {
        SquareTreeNode* root = ParseSquareTree(s);
        utassert(root && 2 == len(root->data));
        int off = 0;
        Str value = root->GetValue(StrL("count"), &off);
        utassert(str::Eq(value, StrL("0")) && 1 == off);
        value = root->GetValue(StrL("count"), &off);
        utassert(str::Eq(value, StrL("1")) && 2 == off);
        value = root->GetValue(StrL("count"), &off);
        utassert(len(value) == 0 && 2 == off);
        delete root;
    }

    static Str emptyNodeData[] = {
        Str(kUtf8Bom "node [\n]"), Str(kUtf8Bom "node \n [ \n ] \n"), Str(kUtf8Bom "node ["), Str(kUtf8Bom "[node] \n"),
        Str(kUtf8Bom "[node]"),    Str(kUtf8Bom "  [  node  ]  "),
    };

    for (Str s : emptyNodeData) {
        SquareTreeNode* root = ParseSquareTree(s);
        utassert(root && 1 == len(root->data));
        utassert(root->GetChild(StrL("node")));
        utassert(0 == len(root->GetChild(StrL("node"))->data));
        delete root;
    }

    static Str halfBrokenData[] = {
        StrL("node [\n child = \n]\n key = value"),
        StrL("node [\nchild\n]\n]\n key = value"),
        StrL("node[\n[node\nchild\nchild [ node\n]\n key = value"),
        StrL("node [\r key = value\n node [\nchild\r\n] key = value"),
    };

    for (Str s : halfBrokenData) {
        SquareTreeNode* root = ParseSquareTree(s);
        utassert(root && 2 == len(root->data));
        utassert(root->GetChild(StrL("node")) == root->data[0]->child);
        SquareTreeNode* node = root->GetChild(StrL("Node"));
        utassert(node && 1 == len(node->data) && str::Eq(node->GetValue(StrL("child")), StrL("")));
        utassert(str::Eq(root->GetValue(StrL("key")), StrL("value")));
        utassert(len(root->GetValue(StrL("node"))) == 0 && !root->GetChild(StrL("key")));
        delete root;
    }

    {
        Str s;
        SquareTreeNode* root = ParseSquareTree(s);
        utassert(!root);
    }
    {
        Str s = StrL("");
        SquareTreeNode* root = ParseSquareTree(s);
        utassert(root && 0 == len(root->data));
        delete root;
    }

    {
        Str s = Str(kUtf8Bom);
        SquareTreeNode* root = ParseSquareTree(s);
        utassert(root && 0 == len(root->data));
        delete root;
    }

    {
        Str s = Str(kUtf8Bom "node [\n node [\n node [\n node [\n node [\n depth 5 \n]\n]\n]\n]\n]");
        SquareTreeNode* root = ParseSquareTree(s);
        SquareTreeNode* node = root;
        for (size_t i = 0; i < 5; i++) {
            utassert(node && 1 == len(node->data));
            node = node->GetChild(StrL("node"));
        }
        utassert(node && 1 == len(node->data) && str::Eq(node->GetValue(StrL("depth")), StrL("5")));
        delete root;
    }

    {
        Str s = Str(kUtf8Bom "node1 [\n [node2] \n key:value");
        SquareTreeNode* root = ParseSquareTree(s);
        utassert(root && root->GetChild(StrL("node1")) && root->GetChild(StrL("node2")));
        utassert(0 == len(root->GetChild(StrL("node1"))->data));
        utassert(str::Eq(root->GetChild(StrL("node2"))->GetValue(StrL("Key")), StrL("value")));
        delete root;
    }

    // EOF without trailing newline / separator: must not read past data.len
    {
        Str s = Str(kUtf8Bom "key");
        SquareTreeNode* root = ParseSquareTree(s);
        utassert(root && 1 == len(root->data));
        utassert(!root->data[0]->child && str::Eq(root->data[0]->key, StrL("key")));
        utassert(str::Eq(root->data[0]->str, StrL("")));
        delete root;
    }
    {
        Str s = Str(kUtf8Bom "key=");
        SquareTreeNode* root = ParseSquareTree(s);
        utassert(root && 1 == len(root->data));
        utassert(str::Eq(root->data[0]->key, StrL("key")) && str::Eq(root->data[0]->str, StrL("")));
        delete root;
    }
    {
        Str s = Str(kUtf8Bom "key=value");
        SquareTreeNode* root = ParseSquareTree(s);
        utassert(root && str::Eq(root->GetValue(StrL("key")), StrL("value")));
        delete root;
    }

    // serialize -> parse round-trip (space indent / \n and tab / \r\n styles)
    {
        Str s = Str(kUtf8Bom "key = value\nnode [\n  nested = x\n  empty = \n]\ncount = 1\ncount = 2\n");
        SquareTreeNode* a = ParseSquareTree(s);
        TempStr ser = SerializeSquareTreeNodeTemp(a);
        SquareTreeNode* b = ParseSquareTree(ser);
        utassert(a && b && 4 == len(a->data) && 4 == len(b->data));
        utassert(str::Eq(b->GetValue(StrL("key")), StrL("value")));
        SquareTreeNode* node = b->GetChild(StrL("node"));
        utassert(node && str::Eq(node->GetValue(StrL("nested")), StrL("x")));
        utassert(str::Eq(node->GetValue(StrL("empty")), StrL("")));
        int off = 0;
        utassert(str::Eq(b->GetValue(StrL("count"), &off), StrL("1")));
        utassert(str::Eq(b->GetValue(StrL("count"), &off), StrL("2")));
        delete a;
        delete b;
    }
    {
        Str s = Str(kUtf8Bom "top = 1\nchild [\n  a = b\n]\n");
        SquareTreeNode* a = ParseSquareTree(s);
        str::Builder out;
        SerializeSquareTreeNode(out, a, StrL("\t"), StrL("\r\n"), 0);
        TempStr ser = ToStrTemp(out);
        SquareTreeNode* b = ParseSquareTree(ser);
        utassert(str::Eq(b->GetValue(StrL("top")), StrL("1")));
        utassert(str::Eq(b->GetChild(StrL("child"))->GetValue(StrL("a")), StrL("b")));
        delete a;
        delete b;
    }

    {
        Str body = StrL(R"([SumatraPDF]
Latest: 21929
BuiltOn: 2026-08-31
Installer64: https://www.sumatrapdfreader.org/dl/prerel/21929/SumatraPDF-prerel-64-install.exe
InstallerArm64: https://www.sumatrapdfreader.org/dl/prerel/21929/SumatraPDF-prerel-arm64-install.exe
Installer32: https://www.sumatrapdfreader.org/dl/prerel/21929/SumatraPDF-prerel-32-install.exe
PortableExe64: https://www.sumatrapdfreader.org/dl/prerel/21929/SumatraPDF-prerel-64.exe
PortableExeArm64: https://www.sumatrapdfreader.org/dl/prerel/21929/SumatraPDF-prerel-arm64.exe
PortableExe32: https://www.sumatrapdfreader.org/dl/prerel/21929/SumatraPDF-prerel-32.exe
)");
        utassert(len(body) == 611);

        SquareTreeNode* root = ParseSquareTree(body);
        SquareTreeNode* node = root ? root->GetChild(StrL("SumatraPDF")) : nullptr;
        utassert(node);

        Str host = StrL("https://www.sumatrapdfreader.org/");
        Str keys[] = {
            StrL("Installer64"),   StrL("InstallerArm64"),   StrL("Installer32"),
            StrL("PortableExe64"), StrL("PortableExeArm64"), StrL("PortableExe32"),
        };
        for (Str key : keys) {
            Str url = node->GetValue(key);
            utassert(len(url) > 0);
            utassert(str::StartsWith(url, host));
        }
        utassert(str::Eq(node->GetValue(StrL("Installer64")),
                         StrL("https://www.sumatrapdfreader.org/dl/prerel/21929/SumatraPDF-prerel-64-install.exe")));
        delete root;
    }
}
