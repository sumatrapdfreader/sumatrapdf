/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"

#include "utils/FileUtil.h"
#include "utils/SquareTreeParser.h"

#include "wingui/TreeModel.h"
#include "EngineBase.h"
#include "FileModifications.h"
#include "Version.h"

/*
The following format (SumatraPDF Modifications eXtensible) is used for
storing file modifications for file formats which don't allow to save
such modifications portably within the file structure (i.e. currently
any format but PDF). The format uses SquareTree syntax (using its INI
serialization for better interoperability):

[@meta]
version = 2.3
filesize = 98765
timestamp = 2013-03-09T12:34:56Z

[highlight]
page = 1
rect = 10 10 100 100
color = #ff0000
opacity = 0.8

[annotType]
page = no
rect = x y w h
color = #rrggbb
opacity = 1

...

[@update]
version = 2.3
filesize = 98765
timestamp = 2013-03-10T05:43:21Z

...

(Currently, the only supported modifications are adding annotations.)
*/

#define SMX_FILE_EXT L".smx"
#define SMX_CURR_VERSION CURR_VERSION_STRA

static char* PageAnnotTypeToString(PageAnnotType typ) {
    switch (typ) {
        case PageAnnotType::Highlight:
            return "highlight";
        case PageAnnotType::Underline:
            return "underline";
        case PageAnnotType::StrikeOut:
            return "strikeout";
        case PageAnnotType::Squiggly:
            return "squiggly";
    }
    return "";
}

static PageAnnotType PageAnnotTypeFromString(const char* s) {
    if (str::EqI(s, "highlight")) {
        return PageAnnotType::Highlight;
    }
    if (str::EqI(s, "underline")) {
        return PageAnnotType::Underline;
    }
    if (str::EqI(s, "strikeout")) {
        return PageAnnotType::StrikeOut;
    }
    if (str::EqI(s, "squiggly")) {
        return PageAnnotType::Squiggly;
    }
    return PageAnnotType::None;
}

// TODO: change to str::string_view
static Vec<PageAnnotation>* ParseFileModifications(const char* data) {
    if (!data) {
        return nullptr;
    }

    SquareTree sqt(data);
    if (!sqt.root || sqt.root->data.size() == 0) {
        return nullptr;
    }
    SquareTreeNode::DataItem& item = sqt.root->data.at(0);
    if (!item.isChild || !str::EqI(item.key, "@meta")) {
        return nullptr;
    }
    if (!item.value.child->GetValue("version")) {
        // don't check the version value - rather extend the format
        // in a way to ensure backwards compatibility
        return nullptr;
    }

    Vec<PageAnnotation>* list = new Vec<PageAnnotation>();
    for (SquareTreeNode::DataItem& i : sqt.root->data) {
        PageAnnotType type = PageAnnotTypeFromString(i.key);

        CrashIf(!i.isChild);
        if (PageAnnotType::None == type || !i.isChild) {
            continue;
        }

        int pageNo;
        geomutil::RectT<float> rect;
        COLORREF color;
        float opacity;
        int r, g, b;

        SquareTreeNode* node = i.value.child;
        const char* value = node->GetValue("page");
        if (!value || !str::Parse(value, "%d%$", &pageNo)) {
            continue;
        }
        value = node->GetValue("rect");
        if (!value || !str::Parse(value, "%f %f %f %f%$", &rect.x, &rect.y, &rect.dx, &rect.dy)) {
            continue;
        }
        value = node->GetValue("color");
        if (!value || !str::Parse(value, "#%2x%2x%2x%$", &r, &g, &b)) {
            continue;
        }
        value = node->GetValue("opacity");
        if (!value || !str::Parse(value, "%f%$", &opacity)) {
            opacity = 1.0f;
        }
        color = MkRgba((u8)r, (u8)g, (u8)b, (u8)(255 * opacity));
        list->Append(PageAnnotation(type, pageNo, rect.Convert<double>(), color));
    }

    return list;
}

Vec<PageAnnotation>* LoadFileModifications(const WCHAR* filePath) {
    AutoFreeWstr modificationsPath = str::Join(filePath, SMX_FILE_EXT);
    AutoFree data = file::ReadFile(modificationsPath);
    if (data.empty()) {
        return nullptr;
    }
    return ParseFileModifications(data.get());
}

bool SaveFileModifications(const WCHAR* filePath, Vec<PageAnnotation>* newAnnots) {
    if (!newAnnots) {
        return false;
    }

    AutoFreeWstr modificationsPath = str::Join(filePath, SMX_FILE_EXT);
    str::Str data;

    const WCHAR* fileName = path::GetBaseNameNoFree(filePath);
    // WCHAR* fileName = L"a�a.pdf"
    std::string_view fileNameA = strconv::WstrToUtf8(fileName);
    data.Append("# SumatraPDF: modifications to \"");
    data.AppendView(fileNameA);
    data.Append("\"\r\n");

    data.AppendFmt("[@%s]\r\n", "meta");
    data.AppendFmt("version = %s\r\n", SMX_CURR_VERSION);
    AutoFreeStr path = strconv::WstrToUtf8(filePath);
    int64_t size = file::GetSize(path.as_view());
    if (0 <= size && size <= UINT_MAX) {
        data.AppendFmt("filesize = %u\r\n", (UINT)size);
    }

    {
        SYSTEMTIME time;
        GetSystemTime(&time);
        int year = time.wYear;
        int month = time.wMonth;
        int day = time.wDay;
        int hour = time.wHour;
        int min = time.wMinute;
        int sec = time.wSecond;
        data.AppendFmt("timestamp = %04d-%02d-%02dT%02d:%02d:%02dZ\r\n", year, month, day, hour, min, sec);
    }
    data.Append("\r\n");

    for (size_t i = 0; i < newAnnots->size(); i++) {
        PageAnnotation& annot = newAnnots->at(i);
        char* s = PageAnnotTypeToString(annot.type);
        if (str::IsEmpty(s)) {
            continue;
        }
        data.AppendFmt("[%s]\r\n", s);
        data.AppendFmt("page = %d\r\n", annot.pageNo);
        data.AppendFmt("rect = %g %g %g %g\r\n", annot.rect.x, annot.rect.y, annot.rect.dx, annot.rect.dy);
        data.AppendFmt("color = ");
        SerializeColorRgb(annot.color, data);
        data.Append("\r\n");
        u8 r, g, b, a;
        UnpackRgba(annot.color, r, g, b, a);
        // TODO: should serialize as rgba in hex
        data.AppendFmt("opacity = %g\r\n", (float)a / 255.f);
        data.Append("\r\n");
    }
    data.RemoveAt(data.size() - 2, 2);

    return file::WriteFile(modificationsPath, data.as_view());
}

bool IsModificationsFile(const WCHAR* filePath) {
    if (!str::EndsWithI(filePath, SMX_FILE_EXT)) {
        return false;
    }
    AutoFreeWstr origPath(str::DupN(filePath, str::Len(filePath) - str::Len(SMX_FILE_EXT)));
    return file::Exists(origPath);
}
