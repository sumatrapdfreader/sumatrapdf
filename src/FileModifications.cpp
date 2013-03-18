/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "FileModifications.h"

#include "BaseEngine.h"
#include "CssParser.h"
#include "FileTransactions.h"
#include "FileUtil.h"

/*
The following format (SumatraPDF Modifications eXtensible) is used for
storing file modifications for file formats which don't allow to save
such modifications portably within the file structure (i.e. currently
any format but PDF). The format uses CSS syntax for brevity:

@meta { version: 2.3; filesize: 98765; timestamp: 2013-03-09T12:34:56Z }
highlight { page: 1; rect: 10 10 100 100; color: #FF0000; opacity: 0.8 }
annotType { page: no; rect: x y w h; color: #rrggbb; opacity: 1.0 }
...
@update { version: 2.3; filesize: 98765; timestamp: 2013-03-10T05:43:21Z }
...

(Currently, the only supported modifications are adding annotations.)
*/

#define SMX_FILE_EXT        L".smx"
#define SMX_CURR_VERSION    "2.3a2"

static inline bool IsSelector(const CssSelector *sel, const char *s) {
    return str::Len(s) == sel->sLen && str::StartsWithI(sel->s, s);
}

static Vec<PageAnnotation> *ParseFileModifications(const char *data, size_t len)
{
    if (!data)
        return NULL;

    CssPullParser parser(data, len);
    if (!parser.NextRule())
        return NULL;
    const CssSelector *sel = parser.NextSelector();
    if (!sel || !IsSelector(sel, "@meta") || parser.NextSelector())
        return NULL;
    const CssProperty *prop = parser.NextProperty();
    if (!prop || Css_Version != prop->type) {
        // don't check the version value - rather extend the format
        // in a way to ensure backwards compatibility
        return NULL;
    }

    Vec<PageAnnotation> *list = new Vec<PageAnnotation>();
    while (parser.NextRule()) {
        sel = parser.NextSelector();
        if (!sel)
            continue;
        PageAnnotType type = IsSelector(sel, "highlight") ? Annot_Highlight :
                             IsSelector(sel, "underline") ? Annot_Underline :
                             IsSelector(sel, "strikeout") ? Annot_StrikeOut :
                             IsSelector(sel, "squiggly")  ? Annot_Squiggly  :
                             Annot_None;
        if (Annot_None == type || parser.NextSelector())
            continue;

        int pageNo = 0;
        geomutil::RectT<float> rect;
        PageAnnotation::Color color;
        float opacity = 1.0f;
        while ((prop = parser.NextProperty())) {
            switch (prop->type) {
            case Css_Page:
                if (!str::Parse(prop->s, prop->sLen, "%d%$", &pageNo))
                    pageNo = 0;
                break;
            case Css_Rect:
                if (!str::Parse(prop->s, prop->sLen, "%f %f %f %f%$",
                                &rect.x, &rect.y, &rect.dx, &rect.dy))
                    rect = geomutil::RectT<float>();
                break;
            case Css_Color:
                int r, g, b;
                if (str::Parse(prop->s, prop->sLen, "#%2x%2x%2x%$", &r, &g, &b))
                    color = PageAnnotation::Color(r, g, b);
                break;
            case Css_Opacity:
                if (!str::Parse(prop->s, prop->sLen, "%f%$", &opacity))
                    opacity = 1.0f;
                break;
            }
        }
        if (pageNo <= 0 || rect.IsEmpty() || 0 == color.a)
            continue;
        if (opacity != 1.0f)
            color.a = (uint8_t)(color.a * opacity);
        list->Append(PageAnnotation(type, pageNo, rect.Convert<double>(), color));
    }

    return list;
}

Vec<PageAnnotation> *LoadFileModifications(const WCHAR *filePath)
{
    ScopedMem<WCHAR> modificationsPath(str::Join(filePath, SMX_FILE_EXT));
    size_t len;
    ScopedMem<char> data(file::ReadAll(modificationsPath, &len));
    return ParseFileModifications(data, len);
}

bool SaveFileModifictions(const WCHAR *filePath, Vec<PageAnnotation> *list)
{
    if (!list)
        return false;

    ScopedMem<WCHAR> modificationsPath(str::Join(filePath, SMX_FILE_EXT));
    str::Str<char> data;
    size_t offset = 0;

    size_t len;
    ScopedMem<char> prevData(file::ReadAll(modificationsPath, &len));
    Vec<PageAnnotation> *prevList = ParseFileModifications(prevData, len);
    if (prevList) {
        // in the case of an update, append changed annotations to the existing ones
        // (don't rewrite the existing ones in case they're by a newer version which
        // added annotation types and properties this version doesn't know anything about)
        for (; offset < prevList->Count() && prevList->At(offset) == list->At(offset); offset++);
        CrashIf(offset != prevList->Count());
        data.AppendAndFree(prevData.StealData());
        data.Append("\r\n");
        delete prevList;
    }
    else {
        data.AppendFmt("/* SumatraPDF: modifications to \"%S\" */\r\n", path::GetBaseName(filePath));
    }

    data.AppendFmt("@%s { version: %s", prevList ? "update" : "meta", SMX_CURR_VERSION);
    int64 size = file::GetSize(filePath);
    if (0 <= size && size <= UINT_MAX)
        data.AppendFmt("; filesize: %u", (UINT)size);
    SYSTEMTIME time;
    GetSystemTime(&time);
    data.AppendFmt("; timestamp: %04d-%02d-%02dT%02d:%02d:%02dZ",
        time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
    data.Append(" }\r\n\r\n");

    for (size_t i = offset; i < list->Count(); i++) {
        PageAnnotation& annot = list->At(i);
        switch (annot.type) {
        case Annot_Highlight: data.Append("highlight"); break;
        case Annot_Underline: data.Append("underline"); break;
        case Annot_StrikeOut: data.Append("strikeout"); break;
        case Annot_Squiggly:  data.Append("squiggly "); break;
        default: continue;
        }
        data.AppendFmt(" { page: %d; rect: %.2f %.2f %.2f %.2f; color: #%02X%02X%02X; opacity: %.2f }\r\n",
                       annot.pageNo, annot.rect.x, annot.rect.y,
                       annot.rect.dx, annot.rect.dy, annot.color.r,
                       annot.color.g, annot.color.b, annot.color.a / 255.f);
    }

    FileTransaction trans;
    return trans.WriteAll(modificationsPath, data.LendData(), data.Size()) && trans.Commit();
}
