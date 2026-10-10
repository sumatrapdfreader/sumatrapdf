/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Pixmap.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/SettingsUtil.h"
#include "base/UITask.h"
#include "gui/UIModels.h"
#include "Settings.h"
#include "AppSettings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "MainWindow.h"
#include "Theme.h"
#include "WindowTab.h"
#include "SumatraConfig.h"
#include "Commands.h"
#include "SumatraPDF.h"
#include "TableOfContents.h"
#include "Favorites.h"
#include "FileHistory.h"
#include "Translations.h"
#include "PdfDarkMode.h"
#include "EngineAll.h"
#include "CommandAvailability.h"
#include "Accelerators.h"
#include "FilterHighlightDraw.h"
#include "Annotation.h"
#include "AnnotSearch.h"
#include "AnnotEditToolbar.h"
#include "AnnotPlacement.h"
#include "CommandPalette.h"
#include "CommandPaletteCommon.h"

bool IsSettingRow(const ItemDataCP* d) {
    return d->settingType != SettingType::Comment;
}

const u8* SettingRowPtr(const ItemDataCP* d) {
    return SettingFieldPtr(d->settingOffset);
}

bool IsCmdInList(i32 cmdId, i32* ids) {
    while (*ids) {
        if (cmdId == *ids) {
            return true;
        }
        ids++;
    }
    return false;
}

Str CommandPaletteSkipWS(Str s) {
    if (!s.s) {
        return {};
    }
    str::TrimWs(s);
    return s;
}

int PaletteHelpKind(Str filter, bool smartTab) {
    if (smartTab) {
        return kHelpSmartTab;
    }
    if (str::StartsWith(filter, Str(kPalettePrefixEverything))) {
        return kHelpEverything;
    }
    if (str::StartsWith(filter, Str(kPalettePrefixTabs))) {
        return kHelpTabs;
    }
    if (str::StartsWith(filter, Str(kPalettePrefixFileHistory))) {
        return kHelpHistory;
    }
    if (str::StartsWith(filter, Str(kPalettePrefixTOC))) {
        return kHelpToc;
    }
    if (str::StartsWith(filter, Str(kPalettePrefixFavorites))) {
        return kHelpFavorites;
    }
    if (str::StartsWith(filter, Str(kPalettePrefixAnnotations))) {
        return kHelpAnnotations;
    }
    if (str::TrimPrefix(filter, Str(kPalettePrefixBoolSettings))) {
        Str path, value;
        return SplitSettingValueQuery(filter, path, value) ? kHelpSettingValue : kHelpSettings;
    }
    if (str::StartsWith(filter, Str(kPalettePrefixThumbnails))) {
        return kHelpThumbnails;
    }
    return kHelpCommands;
}

bool AllowCommand(const AppCommandCtx& ctx, i32 cmdId) {
    return CommandShouldShow(GetCommandVisibility(cmdId, ctx, CommandSurface::Palette));
}

TempStr ConvertPathForDisplayTemp(Str s) {
    return path::GetBaseNameTemp(s);
}

TempStr RemovePrefixFromString(Str s) {
    return str::ReplaceTemp(s, StrL("&"), StrL(""));
}

void CollectTocRec(StrVecCP& toc, TocItem* ti, int indent, int currPageNo, int& bestIdx, int& bestPageNo) {
    while (ti) {
        Str title = ti->title ? ti->title : StrL("");
        ItemDataCP data;
        data.tocItem = ti;
        data.indent = indent;
        data.pageNo = ti->pageNo;
        if (len(title) > 0) {
            toc.Append(title, data);
        }
        int pageNo = ti->pageNo;
        if (len(title) > 0 && pageNo > 0 && pageNo <= currPageNo && pageNo > bestPageNo) {
            bestPageNo = pageNo;
            bestIdx = len(toc) - 1;
        }
        if (ti->child) {
            CollectTocRec(toc, ti->child, indent + 1, currPageNo, bestIdx, bestPageNo);
        }
        ti = ti->next;
    }
}

void AppendFavoritesForFile(StrVecCP& favorites, FileState* fs, bool isCurrent) {
    if (!fs || !fs->favorites) {
        return;
    }
    for (Favorite* fav : *fs->favorites) {
        TempStr rn = FavReadableNameTemp(fav);
        TempStr disp;
        if (isCurrent) {
            disp = rn;
        } else {
            TempStr base = path::GetBaseNameTemp(fs->filePath);
            disp = fmt("%s : %s", base, rn);
        }
        if (len(disp) == 0) {
            continue;
        }
        ItemDataCP data;
        data.favFs = fs;
        data.fav = fav;
        favorites.Append(disp, data);
    }
}

// the scalar settings the palette can edit; arrays and compact structs need the
// advanced settings dialog or the settings file
static bool IsPaletteSettingType(SettingType t) {
    switch (t) {
        case SettingType::Bool:
        case SettingType::Int:
        case SettingType::Float:
        case SettingType::String:
        case SettingType::Color:
            return true;
        default:
            return false;
    }
}

// field.value holds the default: the value itself for Bool/Int, a string
// pointer for Float/String/Color. It is NOT a pointer for Bool/Int, so only
// deref it for the string-backed types.
static TempStr FormatSettingDefaultTemp(SettingType type, intptr_t def) {
    switch (type) {
        case SettingType::Bool:
            return str::DupTemp(def != 0 ? StrL("true") : StrL("false"));
        case SettingType::Int:
            return fmt("%d", (int)def);
        default:
            return str::DupTemp(Str((const char*)def));
    }
}

TempStr FormatSettingValueTemp(SettingType type, const u8* p) {
    switch (type) {
        case SettingType::Bool:
            return str::DupTemp(*(const bool*)p ? StrL("true") : StrL("false"));
        case SettingType::Int:
            return fmt("%d", *(const int*)p);
        case SettingType::Float:
            return fmt("%g", *(const float*)p);
        default:
            // Color is a ParsedColor whose first member is the text
            return str::DupTemp(*(const Str*)p);
    }
}

bool SettingDiffersFromDefault(const ItemDataCP* d) {
    if (d->settingType == SettingType::Float) {
        float def = 0;
        str::Parse(Str((const char*)d->settingDefault), "%f", &def);
        return *(const float*)SettingRowPtr(d) != def;
    }
    TempStr val = FormatSettingValueTemp(d->settingType, SettingRowPtr(d));
    return !str::Eq(val, FormatSettingDefaultTemp(d->settingType, d->settingDefault));
}

// one "= settings" row per scalar setting; compact structs and arrays need
// the advanced settings dialog
void CollectSettingRows(StrVecCP& out) {
    Vec<SettingField> fields;
    CollectSettingFields(fields);
    for (const SettingField& sf : fields) {
        if (!IsPaletteSettingType(sf.field->type) || len(sf.path) == 0) {
            continue;
        }
        ItemDataCP data;
        data.settingType = sf.field->type;
        data.settingOffset = sf.offset;
        data.settingDefault = sf.field->value;
        data.settingComment = sf.comment;
        out.Append(sf.path, data);
    }
}

// Return the same effective shortcut text that is painted on the right side
// of a command row, without the menu separator tab.
TempStr CommandPaletteShortcutTemp(i32 cmdId) {
    if (cmdId == 0) {
        return {};
    }
    TempStr withAccel = AppendAccelKeyToMenuStringTemp(StrL(""), cmdId);
    if (len(withAccel) == 0 || withAccel.s[0] != '\t') {
        return {};
    }
    return Str(withAccel.s + 1, len(withAccel) - 1);
}

void FilterStrings(StrVecCP& strs, const StrVec& words, StrVecCP& matchedOut) {
    int n = len(strs);
    for (int i = 0; i < n; i++) {
        Str s = strs[i];
        if (len(s) == 0) {
            continue;
        }
        bool matches = FilterMatches(s, words);
        ItemDataCP* data = strs.AtData(i);
        if (!matches && data && data->cmdId != 0) {
            TempStr shortcut = CommandPaletteShortcutTemp(data->cmdId);
            matches = FilterMatches(shortcut, words);
        }
        if (!matches && data && IsSettingRow(data)) {
            TempStr val = FormatSettingValueTemp(data->settingType, SettingRowPtr(data));
            matches = FilterMatches(val, words);
        }
        if (!matches) {
            continue;
        }
        matchedOut.AppendFrom(&strs, i);
    }
}

// "ZoomIncrement = 25" -> path "ZoomIncrement", value "25". False when the
// query is still naming a setting, so the list keeps filtering settings.
bool SplitSettingValueQuery(Str query, Str& path, Str& value) {
    int at = str::IndexOfChar(query, kPaletteSettingValueSep[0]);
    if (at < 0) {
        return false;
    }
    path = Str(query.s, at);
    value = Str(query.s + at + 1, query.len - at - 1);
    str::TrimWsBoth(path);
    str::TrimWsBoth(value);
    return len(path) > 0;
}

void AppendTab(StrVecCP& tabs, WindowTab* tab, WindowTab* currTab, int& currTabIdx) {
    ItemDataCP data;
    data.tab = tab;
    if (tab->IsAboutTab()) {
        tabs.Append(Tr("Home"), data);
    } else {
        auto name = path::GetBaseNameTemp(tab->filePath);
        if (len(name) == 0) {
            return;
        }
        tabs.Append(name, data);
    }
    if (tab == currTab) {
        currTabIdx = len(tabs) - 1;
    }
}
