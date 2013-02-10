/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "FileModifications.h"

#include "BaseEngine.h"
#include "CssParser.h"
#include "FileUtil.h"

/*
The following format (SumatraPDF Modifications eXtensible) is used for
storing file modifications for file formats which don't allow to save
such modifications portably within the file structure (i.e. currently
any format but PDF). The format uses CSS syntax for brevity:

@meta { version: 2.3; filesize: 98765 }
highlight { page: 1; rect: 10 10 100 100; color: #FF0000 }
annotType { page: no; rect: x y w h; color: #rrggbb }
...

(Currently, the only supported modifications are adding annotations.)
*/

#define SMX_FILE_EXT        L".smx"
#define SMX_CURR_VERSION    "2.3a1"

static inline bool IsSelector(const CssSelector *sel, const char *s) {
    return str::Len(s) == sel->sLen && str::StartsWithI(sel->s, s);
}

Vec<PageAnnotation> *LoadFileModifications(const WCHAR *filepath)
{
    ScopedMem<WCHAR> modificationsPath(str::Join(filepath, SMX_FILE_EXT));
    size_t len;
    ScopedMem<char> data(file::ReadAll(modificationsPath, &len));
    if (!data)
        return NULL;

    CssPullParser parser(data, len);
    if (!parser.NextRule())
        return NULL;
    const CssSelector *sel = parser.NextSelector();
    if (!sel || !IsSelector(sel, "@meta") || parser.NextSelector())
        return NULL;
    const CssProperty *prop = parser.NextProperty();
    if (!prop || Css_Version != prop->type || !str::Parse(prop->s, prop->sLen, SMX_CURR_VERSION "%$"))
        return NULL;

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
        RectT<float> rect;
        COLORREF color = (COLORREF)-1;
        while ((prop = parser.NextProperty())) {
            switch (prop->type) {
            case Css_Page:
                if (!str::Parse(prop->s, prop->sLen, "%d%$", &pageNo))
                    pageNo = 0;
                break;
            case Css_Rect:
                if (!str::Parse(prop->s, prop->sLen, "%f %f %f %f%$",
                                &rect.x, &rect.y, &rect.dx, &rect.dy))
                    rect = RectT<float>();
                break;
            case Css_Color:
                int r, g, b;
                if (str::Parse(prop->s, prop->sLen, "#%2x%2x%2x%$", &r, &g, &b))
                    color = RGB(r, g, b);
                break;
            }
        }
        if (pageNo <= 0 || rect.IsEmpty())
            continue;
        list->Append(PageAnnotation(type, pageNo, rect.Convert<double>(), color));
    }

    return list;
}

bool SaveFileModifictions(const WCHAR *filepath, Vec<PageAnnotation> *list)
{
    if (!list)
        return false;

    str::Str<char> data;
    data.AppendFmt("/* SumatraPDF: modifications to \"%S\" */\r\n", path::GetBaseName(filepath));
    data.AppendFmt("@meta { version: %s", SMX_CURR_VERSION);
    int64 size = file::GetSize(filepath);
    if (0 <= size && size <= UINT_MAX)
        data.AppendFmt("; filesize: %u", (UINT)size);
    data.Append(" }\r\n\r\n");

    for (size_t i = 0; i < list->Count(); i++) {
        PageAnnotation& annot = list->At(i);
        switch (annot.type) {
        case Annot_Highlight: data.Append("highlight"); break;
        case Annot_Underline: data.Append("underline"); break;
        case Annot_StrikeOut: data.Append("strikeout"); break;
        case Annot_Squiggly:  data.Append("squiggly "); break;
        default: continue;
        }
        data.AppendFmt(" { page: %d; rect: %.2f %.2f %.2f %.2f; color: #%02X%02X%02X }\r\n",
                       annot.pageNo, annot.rect.x, annot.rect.y,
                       annot.rect.dx, annot.rect.dy, GetRValue(annot.color),
                       GetGValue(annot.color), GetBValue(annot.color));
    }

    ScopedMem<WCHAR> modificationsPath(str::Join(filepath, SMX_FILE_EXT));
    return file::WriteAll(modificationsPath, data.LendData(), data.Size());
}
