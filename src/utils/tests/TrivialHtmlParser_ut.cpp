/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/FileUtil.h"
#include "utils/HtmlParserLookup.h"
#include "utils/TrivialHtmlParser.h"
#include "utils/WinUtil.h"

// must be last due to assert() over-write
#include "utils/UtAssert.h"

static void HtmlParser00() {
    HtmlParser p;
    HtmlElement* root = p.Parse("<a></A>");
    utassert(p.ElementsCount() == 1);
    utassert(root);
    utassert(Tag_A == root->tag && !root->name);
    utassert(root->NameIs("a"));

    root = p.Parse("<b></B>");
    utassert(p.ElementsCount() == 1);
    utassert(root);
    utassert(Tag_B == root->tag && !root->name);
    utassert(root->NameIs("b"));
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
    utassert(el->NameIs("bah") && el->NameIs("BAH"));
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
    utassert(root->NameIs("html"));
    utassert(nullptr == root->up);
    utassert(nullptr == root->next);
    HtmlElement* el = root->down;
    utassert(el->NameIs("head"));
    HtmlElement* el2 = el->down;
    utassert(el2->NameIs("meta"));
    utassert(nullptr == el2->next);
    utassert(nullptr == el2->down);
    el2 = el->next;
    utassert(el2->NameIs("body"));
    utassert(nullptr == el2->next);
    el2 = el2->down;
    utassert(el2->NameIs("object"));
    el = p.FindElementByName("html");
    utassert(el);
    el = p.FindElementByName("head", el);
    utassert(el);
    utassert(el->NameIs("head"));
    el = p.FindElementByName("ul", el);
    utassert(el);
}

static void HtmlParser04() {
    HtmlParser p;
    HtmlElement* root = p.Parse("<el att=  va&apos;l></ el >");
    utassert(1 == p.ElementsCount());
    utassert(1 == p.TotalAttrCount());
    utassert(root->NameIs("el"));
    utassert(nullptr == root->next);
    utassert(nullptr == root->up);
    utassert(nullptr == root->down);
    AutoFreeWstr val(root->GetAttribute("att"));
    utassert(str::Eq(val, L"va'l"));
    utassert(!root->firstAttr->next);
}

static void HtmlParser03() {
    HtmlParser p;
    HtmlElement* root = p.Parse("<el   att  =v&quot;al/>");
    utassert(1 == p.ElementsCount());
    utassert(1 == p.TotalAttrCount());
    utassert(root->NameIs("el"));
    utassert(nullptr == root->next);
    utassert(nullptr == root->up);
    utassert(nullptr == root->down);
    AutoFreeWstr val(root->GetAttribute("att"));
    utassert(str::Eq(val, L"v\"al"));
    utassert(!root->firstAttr->next);
}

static void HtmlParser02() {
    HtmlParser p;
    HtmlElement* root = p.Parse(
        "<a><b/><c></c  ><d at1=\"&lt;quo&amp;ted&gt;\" at2='also quoted'   att3=notquoted att4=&#101;&#x6e;d/></a>");
    utassert(4 == p.ElementsCount());
    utassert(4 == p.TotalAttrCount());
    utassert(root->NameIs("a"));
    utassert(nullptr == root->next);
    HtmlElement* el = root->down;
    utassert(el->NameIs("b"));
    utassert(root == el->up);
    el = el->next;
    utassert(el->NameIs("c"));
    utassert(root == el->up);
    el = el->next;
    utassert(el->NameIs("d"));
    utassert(nullptr == el->next);
    utassert(root == el->up);
    AutoFreeWstr val(el->GetAttribute("at1"));
    utassert(str::Eq(val, L"<quo&ted>"));
    val.Set(el->GetAttribute("at2"));
    utassert(str::Eq(val, L"also quoted"));
    val.Set(el->GetAttribute("att3"));
    utassert(str::Eq(val, L"notquoted"));
    val.Set(el->GetAttribute("att4"));
    utassert(str::Eq(val, L"end"));
}

static void HtmlParser06() {
    HtmlParser p;
    HtmlElement* root = p.Parse("<ul><p>ignore<li><br><meta><li><ol><li></ul><dropme>");
    utassert(9 == p.ElementsCount());
    utassert(0 == p.TotalAttrCount());
    utassert(root->NameIs("ul"));
    utassert(!root->next);
    HtmlElement* el = root->GetChildByTag(Tag_Li);
    utassert(el);
    utassert(el->down->NameIs("br"));
    utassert(el->down->next->NameIs("meta"));
    utassert(!el->down->next->next);
    el = root->GetChildByTag(Tag_Li, 1);
    utassert(el);
    utassert(!el->next);
    el = el->GetChildByTag(Tag_Ol);
    utassert(!el->next);
    utassert(el->down->NameIs("li"));
    utassert(!el->down->down);
}

static void HtmlParser07() {
    HtmlParser p;
    HtmlElement* root = p.Parse("<test umls=&auml;\xC3\xB6&#xFC; Zero=&#1;&#0;&#-1;>", CP_UTF8);
    utassert(1 == p.ElementsCount());
    AutoFreeWstr val(root->GetAttribute("umls"));
    utassert(str::Eq(val, L"\xE4\xF6\xFC"));
    val.Set(root->GetAttribute("zerO"));
    utassert(str::Eq(val, L"\x01??"));
}

static void HtmlParser08() {
    AutoFreeWstr val(DecodeHtmlEntitites("&auml&test;&&ouml-", CP_ACP));
    utassert(str::Eq(val, L"\xE4&test;&\xF6-"));
}

static void HtmlParser09() {
    HtmlParser p;
    HtmlElement* root = p.Parse("<?xml version='1.0'?><!-- <html><body></html> --><root attr='<!-- comment -->' />");
    utassert(1 == p.ElementsCount());
    utassert(1 == p.TotalAttrCount());
    utassert(root->NameIs("root"));
    AutoFreeWstr val(root->GetAttribute("attr"));
    utassert(str::Eq(val, L"<!-- comment -->"));

    root = p.Parse("<!-- comment with \" and \' --><main />");
    utassert(1 == p.ElementsCount());
    utassert(0 == p.TotalAttrCount());
    utassert(root->NameIs("main"));
}

static void HtmlParser10() {
    HtmlParser p;
    HtmlElement* root = p.Parse("<!xml version='1.0'?><x:a xmlns:x='http://example.org/ns/x'><x:b attr='val'/></x:a>");
    utassert(2 == p.ElementsCount());
    utassert(2 == p.TotalAttrCount());
    utassert(root->NameIs("x:a") && root->NameIsNS("a", "http://example.org/ns/x"));

    HtmlElement* node = p.FindElementByName("b");
    utassert(!node);
    node = p.FindElementByNameNS("b", "http://example.org/ns/x");
    utassert(node);
    utassert(node->NameIs("x:b") && node->NameIsNS("b", "http://example.org/ns/x"));
    AutoFreeWstr val(node->GetAttribute("attr"));
    utassert(str::Eq(val, L"val"));
    // TODO: XML tags are case sensitive (HTML tags aren't)
    node = p.FindElementByName("X:B");
    utassert(node && node->NameIs("X:B"));
}

static void HtmlParser11() {
    HtmlParser p;
    HtmlElement* root = p.Parse("<root/><!-- comment -->");
    utassert(1 == p.ElementsCount());
    utassert(0 == p.TotalAttrCount());
    utassert(root && root->NameIs("root"));

    root = p.Parse("<root><!---></root>");
    utassert(!root);
}

static void HtmlParserFile() {
    WCHAR* fileName = L"HtmlParseTest00.html";
    // We assume we're being run from obj-[dbg|rel], so the test
    // files are in ..\src\utils directory relative to exe's dir
    AutoFreeWstr exePath(GetExePath());
    const WCHAR* exeDir = path::GetBaseNameNoFree(exePath);
    AutoFreeWstr p1(path::Join(exeDir, L"..\\src\\utils"));
    AutoFreeWstr p2(path::Join(p1, fileName));
    AutoFree d(file::ReadFile(p2));
    // it's ok if we fail - we assume we were not run from the
    // right location
    if (!d.data) {
        return;
    }
    HtmlParser p;
    HtmlElement* root = p.ParseInPlace(d.data);
    utassert(root);
    utassert(709 == p.ElementsCount());
    utassert(955 == p.TotalAttrCount());
    utassert(root->NameIs("html"));
    HtmlElement* el = root->down;
    utassert(el->NameIs("head"));
    el = el->next;
    utassert(el->NameIs("body"));
    el = el->down;
    utassert(el->NameIs("object"));
    el = el->next;
    utassert(el->NameIs("ul"));
    el = el->down;
    utassert(el->NameIs("li"));
    el = el->down;
    utassert(el->NameIs("object"));
    AutoFreeWstr val(el->GetAttribute("type"));
    utassert(str::Eq(val, L"text/sitemap"));
    el = el->down;
    utassert(el->NameIs("param"));
    utassert(!el->down);
    utassert(el->next->NameIs("param"));
    el = p.FindElementByName("body");
    utassert(el);
    el = p.FindElementByName("ul", el);
    utassert(el);
    int count = 0;
    while (el) {
        ++count;
        el = p.FindElementByName("ul", el);
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
