/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/File.h"
#include "base/HtmlParserLookup.h"
#include "base/TrivialHtmlParser.h"
#include "base/Win.h"

// must be last due to assert() over-write
#include "base/UtAssert.h"

static void HtmlParser00() {
    HtmlParser p;
    HtmlElement* root = p.Parse("<a></A>");
    utassert(p.ElementsCount() == 1);
    utassert(root);
    utassert(Tag_A == root->tag && !root->name);
    utassert(root->NameIs(StrL("a")));

    root = p.Parse("<b></B>");
    utassert(p.ElementsCount() == 1);
    utassert(root);
    utassert(Tag_B == root->tag && !root->name);
    utassert(root->NameIs(StrL("b")));
}

static void HtmlParser01() {
    HtmlParser p;
    HtmlElement* root = p.Parse("<A><bAh></a>");
    utassert(p.ElementsCount() == 2);
    utassert(Tag_A == root->tag && !root->name);
    utassert(nullptr == root->up);
    utassert(nullptr == root->next);
    HtmlElement* el = root->down;
    utassert(nullptr == el->firstAttr);
    utassert(el->NameIs(StrL("bah")) && el->NameIs(StrL("BAH")));
    utassert(Tag_NotFound == el->tag && str::Eq("bAh", el->name));
    utassert(el->up == root);
    utassert(nullptr == el->down);
    utassert(nullptr == el->next);
}

static void HtmlParser05() {
    HtmlParser p;
    HtmlElement* root = p.Parse(
        "<!doctype><html><HEAD><meta name=foo></head><body><object t=la><param name=foo "
        "val=bar></object><ul><li></ul></object></body></Html>");
    utassert(8 == p.ElementsCount());
    utassert(4 == p.TotalAttrCount());
    utassert(root->NameIs(StrL("html")));
    utassert(nullptr == root->up);
    utassert(nullptr == root->next);
    HtmlElement* el = root->down;
    utassert(el->NameIs(StrL("head")));
    HtmlElement* el2 = el->down;
    utassert(el2->NameIs(StrL("meta")));
    utassert(nullptr == el2->next);
    utassert(nullptr == el2->down);
    el2 = el->next;
    utassert(el2->NameIs(StrL("body")));
    utassert(nullptr == el2->next);
    el2 = el2->down;
    utassert(el2->NameIs(StrL("object")));
    el = p.FindElementByName(StrL("html"));
    utassert(el);
    el = p.FindElementByName(StrL("head"), el);
    utassert(el);
    utassert(el->NameIs(StrL("head")));
    el = p.FindElementByName(StrL("ul"), el);
    utassert(el);
}

static void HtmlParser04() {
    HtmlParser p;
    HtmlElement* root = p.Parse("<el att=  va&apos;l></ el >");
    utassert(1 == p.ElementsCount());
    utassert(1 == p.TotalAttrCount());
    utassert(root->NameIs(StrL("el")));
    utassert(nullptr == root->next);
    utassert(nullptr == root->up);
    utassert(nullptr == root->down);
    TempStr val = root->GetAttributeTemp("att");
    utassert(str::Eq(val, "va'l"));
    utassert(!root->firstAttr->next);
}

static void HtmlParser03() {
    HtmlParser p;
    HtmlElement* root = p.Parse("<el   att  =v&quot;al/>");
    utassert(1 == p.ElementsCount());
    utassert(1 == p.TotalAttrCount());
    utassert(root->NameIs(StrL("el")));
    utassert(nullptr == root->next);
    utassert(nullptr == root->up);
    utassert(nullptr == root->down);
    TempStr val = root->GetAttributeTemp("att");
    utassert(str::Eq(val, "v\"al"));
    utassert(!root->firstAttr->next);
}

static void HtmlParser02() {
    HtmlParser p;
    HtmlElement* root = p.Parse(
        "<a><b/><c></c  ><d at1=\"&lt;quo&amp;ted&gt;\" at2='also quoted'   att3=notquoted att4=&#101;&#x6e;d/></a>");
    utassert(4 == p.ElementsCount());
    utassert(4 == p.TotalAttrCount());
    utassert(root->NameIs(StrL("a")));
    utassert(nullptr == root->next);
    HtmlElement* el = root->down;
    utassert(el->NameIs(StrL("b")));
    utassert(root == el->up);
    el = el->next;
    utassert(el->NameIs(StrL("c")));
    utassert(root == el->up);
    el = el->next;
    utassert(el->NameIs(StrL("d")));
    utassert(nullptr == el->next);
    utassert(root == el->up);
    TempStr val = el->GetAttributeTemp("at1");
    utassert(str::Eq(val, "<quo&ted>"));
    val = el->GetAttributeTemp("at2");
    utassert(str::Eq(val, "also quoted"));
    val = el->GetAttributeTemp("att3");
    utassert(str::Eq(val, "notquoted"));
    val = el->GetAttributeTemp("att4");
    utassert(str::Eq(val, "end"));
}

static void HtmlParser06() {
    HtmlParser p;
    HtmlElement* root = p.Parse("<ul><p>ignore<li><br><meta><li><ol><li></ul><dropme>");
    utassert(9 == p.ElementsCount());
    utassert(0 == p.TotalAttrCount());
    utassert(root->NameIs(StrL("ul")));
    utassert(!root->next);
    HtmlElement* el = root->GetChildByTag(Tag_Li);
    utassert(el);
    utassert(el->down->NameIs(StrL("br")));
    utassert(el->down->next->NameIs(StrL("meta")));
    utassert(!el->down->next->next);
    el = root->GetChildByTag(Tag_Li, 1);
    utassert(el);
    utassert(!el->next);
    el = el->GetChildByTag(Tag_Ol);
    utassert(!el->next);
    utassert(el->down->NameIs(StrL("li")));
    utassert(!el->down->down);
}

static void HtmlParser07() {
    HtmlParser p;
    HtmlElement* root = p.Parse("<test umls=&auml;\xC3\xB6&#xFC; Zero=&#1;&#0;&#-1;>", CP_UTF8);
    utassert(1 == p.ElementsCount());
    TempStr val = root->GetAttributeTemp("umls");
    utassert(str::Eq(val, "\xC3\xA4\xC3\xB6\xC3\xBC")); // utf8 for äöü
    val = root->GetAttributeTemp("zerO");
    utassert(str::Eq(val, "\x01??"));
}

static void HtmlParser08() {
    WStr val = DecodeHtmlEntities("&auml&test;&&ouml-", CP_ACP);
    utassert(wstr::Eq(val, WStr(L"\xE4&test;&\xF6-")));
    wstr::Free(val);
}

static void HtmlParser09() {
    HtmlParser p;
    HtmlElement* root = p.Parse("<?xml version='1.0'?><!-- <html><body></html> --><root attr='<!-- comment -->' />");
    utassert(1 == p.ElementsCount());
    utassert(1 == p.TotalAttrCount());
    utassert(root->NameIs(StrL("root")));
    TempStr val = root->GetAttributeTemp("attr");
    utassert(str::Eq(val, "<!-- comment -->"));

    root = p.Parse("<!-- comment with \" and \' --><main />");
    utassert(1 == p.ElementsCount());
    utassert(0 == p.TotalAttrCount());
    utassert(root->NameIs(StrL("main")));
}

static void HtmlParser10() {
    HtmlParser p;
    HtmlElement* root = p.Parse("<!xml version='1.0'?><x:a xmlns:x='http://example.org/ns/x'><x:b attr='val'/></x:a>");
    utassert(2 == p.ElementsCount());
    utassert(2 == p.TotalAttrCount());
    utassert(root->NameIs(StrL("x:a")) && root->NameIsNS(StrL("a"), StrL("http://example.org/ns/x")));

    HtmlElement* node = p.FindElementByName(StrL("b"));
    utassert(!node);
    node = p.FindElementByNameNS(StrL("b"), StrL("http://example.org/ns/x"));
    utassert(node);
    utassert(node->NameIs(StrL("x:b")) && node->NameIsNS(StrL("b"), StrL("http://example.org/ns/x")));
    TempStr val = node->GetAttributeTemp("attr");
    utassert(str::Eq(val, "val"));
    // TODO: XML tags are case sensitive (HTML tags aren't)
    node = p.FindElementByName(StrL("X:B"));
    utassert(node && node->NameIs(StrL("X:B")));
}

static void HtmlParser11() {
    HtmlParser p;
    HtmlElement* root = p.Parse("<root/><!-- comment -->");
    utassert(1 == p.ElementsCount());
    utassert(0 == p.TotalAttrCount());
    utassert(root && root->NameIs(StrL("root")));

    root = p.Parse("<root><!---></root>");
    utassert(!root);
}

static void HtmlParserFile() {
    Str fileName = "HtmlParseTest00.html";
    // We assume we're being run from obj-[dbg|rel], so the test
    // files are in ..\src\utils directory relative to exe's dir
    TempStr exePath = GetSelfExePathTemp();
    TempStr exeDir = path::GetBaseNameTemp(exePath);
    TempStr p1 = path::JoinTemp(exeDir, "..\\src\\utils");
    TempStr p2 = path::JoinTemp(p1, fileName);
    Str d = file::ReadFile(p2);
    // it's ok if we fail - we assume we were not run from the
    // right location
    if (!d) {
        return;
    }
    HtmlParser p;
    HtmlElement* root = p.ParseInPlace(d);
    str::Free(d);
    utassert(root);
    utassert(709 == p.ElementsCount());
    utassert(955 == p.TotalAttrCount());
    utassert(root->NameIs(StrL("html")));
    HtmlElement* el = root->down;
    utassert(el->NameIs(StrL("head")));
    el = el->next;
    utassert(el->NameIs(StrL("body")));
    el = el->down;
    utassert(el->NameIs(StrL("object")));
    el = el->next;
    utassert(el->NameIs(StrL("ul")));
    el = el->down;
    utassert(el->NameIs(StrL("li")));
    el = el->down;
    utassert(el->NameIs(StrL("object")));
    TempStr val = el->GetAttributeTemp("type");
    utassert(str::Eq(val, "text/sitemap"));
    el = el->down;
    utassert(el->NameIs(StrL("param")));
    utassert(!el->down);
    utassert(el->next->NameIs(StrL("param")));
    el = p.FindElementByName(StrL("body"));
    utassert(el);
    el = p.FindElementByName(StrL("ul"), el);
    utassert(el);
    int count = 0;
    while (el) {
        ++count;
        el = p.FindElementByName(StrL("ul"), el);
    }
    utassert(18 == count);
}

void TrivialHtmlParser_UnitTests() {
    HtmlParserFile();
    HtmlParser11();
    HtmlParser10();
    HtmlParser09();
    HtmlParser08();
    HtmlParser07();
    HtmlParser06();
    HtmlParser05();
    HtmlParser04();
    HtmlParser03();
    HtmlParser02();
    HtmlParser00();
    HtmlParser01();
}
