/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 (see COPYING) */

#include "BaseUtil.h"
#include "Vec.h"
#include "Scoped.h"
#include "ZipUtil.h"
#include "StrUtil.h"
#include "TrivialHtmlParser.h"

#include "EpubDoc.h"

/*
Basic EPUB format support:
- ensures basic format integrity ("mimetype" file and content)
- extracts all HTML content in the proper order

TODO:
- extract metadata if available (from the .opf file linked from "META-INF/container.xml")
- support further HTML features such as hyperlinks
- create ToC from .opf or .ncx file
*/

class CEpubDoc : public EpubDoc {
    friend EpubDoc;

    ScopedMem<TCHAR> fileName;
    ZipFile zip;
    str::Str<char> htmlData;

    bool Load();

public:
    CEpubDoc(const TCHAR *fileName) : zip(fileName), fileName(str::Dup(fileName)) { }

    virtual const char *GetBookHtmlData(size_t& lenOut) {
        lenOut = htmlData.Size();
        return htmlData.Get();
    }
};

bool CEpubDoc::Load()
{
    ScopedMem<char> firstFileData(zip.GetFileData(_T("mimetype")));
    if (!str::Eq(zip.GetFileName(0), _T("mimetype")) ||
        !str::Eq(firstFileData, "application/epub+zip")) {
        // no proper EPUB document
        return false;
    }

    ScopedMem<char> container(zip.GetFileData(_T("META-INF/container.xml")));
    HtmlParser parser;
    HtmlElement *node = parser.ParseInPlace(container);
    if (!node)
        return false;
    // only consider the first <rootfile> element (default rendition)
    node = parser.FindElementByName("rootfile");
    if (!node)
        return false;
    ScopedMem<TCHAR> contentPath(node->GetAttribute("full-path"));
    if (!contentPath)
        return false;

    ScopedMem<char> content(zip.GetFileData(contentPath));
    if (!content)
        return false;
    node = parser.ParseInPlace(content);
    if (!node)
        return false;
    node = parser.FindElementByName("manifest");
    if (!node)
        return false;

    if (str::FindChar(contentPath, '/'))
        *(TCHAR *)(str::FindCharLast(contentPath, '/') + 1) = '\0';
    else
        *contentPath = '\0';
    for (node = node->down; node; node = node->next) {
        ScopedMem<TCHAR> mediatype(node->GetAttribute("media-type"));
        if (str::Eq(mediatype, _T("application/xhtml+xml"))) {
            ScopedMem<TCHAR> htmlPath(node->GetAttribute("href"));
            if (!htmlPath)
                continue;
            htmlPath.Set(str::Join(contentPath, htmlPath));

            ScopedMem<char> html(zip.GetFileData(htmlPath));
            if (!html)
                continue;
            if (htmlData.Count() > 0) {
                // insert explicit page-breaks between sections
                htmlData.Append("<pagebreak />");
            }
            // TODO: merge/remove <head>s and drop everything else outside of <body>s(?)
            htmlData.Append(html);
        }
    }

    return htmlData.Count() > 0;
}

EpubDoc *EpubDoc::ParseFile(const TCHAR *fileName)
{
    CEpubDoc *doc = new CEpubDoc(fileName);
    if (doc && !doc->Load()) {
        delete doc;
        doc = NULL;
    }
    return doc;
}

bool EpubDoc::IsSupported(const TCHAR *fileName)
{
    return str::EndsWithI(fileName, _T(".epub"));
}
