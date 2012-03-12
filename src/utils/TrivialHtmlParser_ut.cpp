/* Copyright 2006-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: only meant to be #included from TrivialHtmlParser.cpp,
// not compiled on its own

#include <assert.h>
#include "FileUtil.h"
#include "WinUtil.h"

namespace unittests {

static void HtmlParser00()
{
    HtmlParser p;
    HtmlElement *root = p.Parse("<a></A>");
    assert(p.ElementsCount() == 1);
    assert(root);
    assert(str::Eq("a", root->name));

    root = p.Parse("<b></B>");
    assert(p.ElementsCount() == 1);
    assert(root);
    assert(str::Eq("b", root->name));
}

static void HtmlParser01()
{
    HtmlParser p;
    HtmlElement *root = p.Parse("<A><bAh></a>");
    assert(p.ElementsCount() == 2);
    assert(str::Eq("a", root->name));
    assert(NULL == root->up);
    assert(NULL == root->next);
    HtmlElement *el = root->down;
    assert(NULL == el->firstAttr);
    assert(str::Eq("bah", el->name));
    assert(el->up == root);
    assert(NULL == el->down);
    assert(NULL == el->next);
}

static void HtmlParser05()
{
    HtmlParser p;
    HtmlElement *root = p.Parse("<!doctype><html><HEAD><meta name=foo></head><body><object t=la><param name=foo val=bar></object><ul><li></ul></object></body></Html>");
    assert(8 == p.ElementsCount());
    assert(4 == p.TotalAttrCount());
    assert(str::Eq("html", root->name));
    assert(NULL == root->up);
    assert(NULL == root->next);
    HtmlElement *el = root->down;
    assert(str::Eq("head", el->name));
    HtmlElement *el2 = el->down;
    assert(str::Eq("meta", el2->name));
    assert(NULL == el2->next);
    assert(NULL == el2->down);
    el2 = el->next;
    assert(str::Eq("body", el2->name));
    assert(NULL == el2->next);
    el2 = el2->down;
    assert(str::Eq("object", el2->name));
    el = p.FindElementByName("html");
    assert(el);
    el = p.FindElementByName("head", el);
    assert(el);
    assert(str::Eq("head", el->name));
    el = p.FindElementByName("ul", el);
    assert(el);
}

static void HtmlParser04()
{
    HtmlParser p;
    HtmlElement *root = p.Parse("<el att=  va&apos;l></ el >");
    assert(1 == p.ElementsCount());
    assert(1 == p.TotalAttrCount());
    assert(str::Eq("el", root->name));
    assert(NULL == root->next);
    assert(NULL == root->up);
    assert(NULL == root->down);
    ScopedMem<TCHAR> val(root->GetAttribute("att"));
    assert(str::Eq(val, _T("va'l")));
    assert(!root->firstAttr->next);
}

static void HtmlParser03()
{
    HtmlParser p;
    HtmlElement *root = p.Parse("<el   att  =v&quot;al/>");
    assert(1 == p.ElementsCount());
    assert(1 == p.TotalAttrCount());
    assert(str::Eq("el", root->name));
    assert(NULL == root->next);
    assert(NULL == root->up);
    assert(NULL == root->down);
    ScopedMem<TCHAR> val(root->GetAttribute("att"));
    assert(str::Eq(val, _T("v\"al")));
    assert(!root->firstAttr->next);
}

static void HtmlParser02()
{
    HtmlParser p;
    HtmlElement *root = p.Parse("<a><b/><c></c  ><d at1=\"&lt;quo&amp;ted&gt;\" at2='also quoted'   att3=notquoted att4=&#101;&#x6e;d/></a>");
    assert(4 == p.ElementsCount());
    assert(4 == p.TotalAttrCount());
    assert(str::Eq("a", root->name));
    assert(NULL == root->next);
    HtmlElement *el = root->down;
    assert(str::Eq("b", el->name));
    assert(root == el->up);
    el = el->next;
    assert(str::Eq("c", el->name));
    assert(root == el->up);
    el = el->next;
    assert(str::Eq("d", el->name));
    assert(NULL == el->next);
    assert(root == el->up);
    ScopedMem<TCHAR> val(el->GetAttribute("at1"));
    assert(str::Eq(val, _T("<quo&ted>")));
    val.Set(el->GetAttribute("at2"));
    assert(str::Eq(val, _T("also quoted")));
    val.Set(el->GetAttribute("att3"));
    assert(str::Eq(val, _T("notquoted")));
    val.Set(el->GetAttribute("att4"));
    assert(str::Eq(val, _T("end")));
}

static void HtmlParser06()
{
    HtmlParser p;
    HtmlElement *root = p.Parse("<ul><p>ignore<li><br><meta><li><ol><li></ul><dropme>");
    assert(9 == p.ElementsCount());
    assert(0 == p.TotalAttrCount());
    assert(str::Eq("ul", root->name));
    assert(!root->next);
    HtmlElement *el = root->GetChildByName("li");
    assert(el);
    assert(str::Eq(el->down->name, "br"));
    assert(str::Eq(el->down->next->name, "meta"));
    assert(!el->down->next->next);
    el = root->GetChildByName("li", 1);
    assert(el);
    assert(!el->next);
    el = el->GetChildByName("ol");
    assert(!el->next);
    assert(str::Eq(el->down->name, "li"));
    assert(!el->down->down);
}

static void HtmlParser07()
{
    HtmlParser p;
    HtmlElement *root = p.Parse("<test umls=&auml;\xC3\xB6&#xFC; zero=&#1;&#0;&#-1;>", CP_UTF8);
    assert(1 == p.ElementsCount());
    ScopedMem<TCHAR> val(root->GetAttribute("umls"));
#ifdef UNICODE
    assert(str::Eq(val, L"\xE4\xF6\xFC"));
#else
    assert(str::EndsWith(val, "\xFC"));
#endif
    val.Set(root->GetAttribute("zero"));
    assert(str::Eq(val, _T("\x01??")));
}

static void HtmlParser08()
{
    ScopedMem<TCHAR> val(DecodeHtmlEntitites("&auml&test;&&ouml-", CP_ACP));
    assert(str::Eq(val.Get(), _T("\xE4&test;&\xF6-")));
}

static void HtmlParser09()
{
    HtmlParser p;
    HtmlElement *root = p.Parse("<?xml version='1.0'?><!-- <html><body></html> --><root attr='<!-- comment -->' />");
    assert(1 == p.ElementsCount());
    assert(1 == p.TotalAttrCount());
    assert(str::Eq("root", root->name));
    ScopedMem<TCHAR> val(root->GetAttribute("attr"));
    assert(str::Eq(val, _T("<!-- comment -->")));

    root = p.Parse("<!-- comment with \" and \' --><main />");
    assert(1 == p.ElementsCount());
    assert(0 == p.TotalAttrCount());
    assert(str::Eq("main", root->name));
}

static void HtmlParserFile()
{
    TCHAR *fileName = _T("HtmlParseTest00.html");
    // We assume we're being run from obj-[dbg|rel], so the test
    // files are in ..\src\utils directory relative to exe's dir
    ScopedMem<TCHAR> exePath(GetExePath());
    const TCHAR *exeDir = path::GetBaseName(exePath);
    ScopedMem<TCHAR> p1(path::Join(exeDir, _T("..\\src\\utils")));
    ScopedMem<TCHAR> p2(path::Join(p1, fileName));
    char *d = file::ReadAll(p2, NULL);
    // it's ok if we fail - we assume we were not run from the
    // right location
    if (!d)
        return;
    HtmlParser p;
    HtmlElement *root = p.ParseInPlace(d);
    assert(root);
    assert(709 == p.ElementsCount());
    assert(955 == p.TotalAttrCount());
    assert(str::Eq(root->name, "html"));
    HtmlElement *el = root->down;
    assert(str::Eq(el->name, "head"));
    el = el->next;
    assert(str::Eq(el->name, "body"));
    el = el->down;
    assert(str::Eq(el->name, "object"));
    el = el->next;
    assert(str::Eq(el->name, "ul"));
    el = el->down;
    assert(str::Eq(el->name, "li"));
    el = el->down;
    assert(str::Eq(el->name, "object"));
    ScopedMem<TCHAR> val(el->GetAttribute("type"));
    assert(str::Eq(val, _T("text/sitemap")));
    el = el->down;
    assert(str::Eq(el->name, "param"));
    assert(!el->down);
    assert(str::Eq(el->next->name, "param"));
    el = p.FindElementByName("body");
    assert(el);
    el = p.FindElementByName("ul", el);
    assert(el);
    int count = 0;
    while (el) {
        ++count;
        el = p.FindElementByName("ul", el);
    }
    assert(18 == count);
    free(d);
}

}

void TrivialHtmlParser_UnitTests()
{
    unittests::HtmlParserFile();
    unittests::HtmlParser09();
    unittests::HtmlParser08();
    unittests::HtmlParser07();
    unittests::HtmlParser06();
    unittests::HtmlParser05();
    unittests::HtmlParser04();
    unittests::HtmlParser03();
    unittests::HtmlParser02();
    unittests::HtmlParser00();
    unittests::HtmlParser01();
}
