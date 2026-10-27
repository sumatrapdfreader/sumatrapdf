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

// The color a "= settings" row shows a swatch of: a color setting's value, or
// the value typed for one in the value stage
bool PaletteRowColor(const ItemDataCP* d, Str rowText, Color* colOut) {
    if (!IsSettingRow(d) || d->settingType != SettingType::Color) {
        return false;
    }
    Str val = len(d->settingPath) > 0 ? rowText : FormatSettingValueTemp(d->settingType, SettingRowPtr(d));
    return ParseColor(colOut, val);
}

// Delete on a "= settings" row puts the setting's default back. rowText is the
// row's text: the setting's path in the picking stage, a candidate value once
// settingPath names the setting. Returns the path, empty if nothing changed.
TempStr RestorePaletteSettingDefault(const ItemDataCP* d, Str rowText) {
    // applying the value reloads the settings, which rebuilds the rows d and
    // rowText live in
    TempStr path = str::DupTemp(len(d->settingPath) > 0 ? d->settingPath : rowText);
    TempStr def = FormatSettingDefaultTemp(d->settingType, d->settingDefault);
    if (!SetSettingsValueFromStr(path, def)) {
        return {};
    }
    return path;
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

// commands that act at the mouse position (annotation create, read aloud from cursor).
// Placement-mode tools (ink, line, stamp, ...) start a mode; a point would skip
// that and create at the remembered cursor, like the context menu.
bool CmdUsesCursorPos(int cmdId) {
    if (CommandUsesPlacementMode(cmdId)) {
        return false;
    }
    if (cmdId >= CmdCreateAnnotFirst && cmdId <= CmdCreateAnnotLast) {
        return true;
    }
    return cmdId == CmdCreateAnnotImageFromClipboard || cmdId == CmdReadAloudFromCursorPosition;
}

// --- what the apps' palettes had in common -----------------------------------

// The setting at a full dotted path, or an unambiguous leaf ("Units" for
// "FixedPageUI.PageGrid.Units") so the name can be typed by hand
ItemDataCP* FindPaletteSetting(PaletteSources& src, Str path, Str& foundPath) {
    ItemDataCP* found = nullptr;
    int nLeaf = 0;
    for (int i = 0; i < len(src.settings); i++) {
        Str s = src.settings[i];
        if (str::EqI(s, path)) {
            found = src.settings.AtData(i);
            foundPath = s;
            nLeaf = 1;
            break;
        }
        Str leaf = str::SliceFromCharLast(s, '.');
        if (len(leaf) > 1 && str::EqI(Str(leaf.s + 1, leaf.len - 1), path)) {
            nLeaf++;
            found = src.settings.AtData(i);
            foundPath = s;
        }
    }
    if (nLeaf != 1) {
        return nullptr;
    }
    return found;
}

// Rows for the value stage: an enum offers its allowed values, anything else
// offers the one value being typed. Enter on a row applies it
void FillSettingValueRows(PaletteSources& src, Str path, Str value, StrVecCP& out) {
    Str foundPath;
    ItemDataCP* found = FindPaletteSetting(src, path, foundPath);
    if (!found || found->settingType == SettingType::Bool) {
        return;
    }
    ItemDataCP data = *found;
    data.settingPath = foundPath;
    const char** enumValues = GetSettingsEnumValues(foundPath);
    if (!enumValues) {
        // clearing a string is meaningful, an empty number is not
        bool isStr = found->settingType != SettingType::Int && found->settingType != SettingType::Float;
        if (len(value) > 0 || isStr) {
            out.Append(value, data);
        }
        return;
    }
    for (const char** v = enumValues; *v; v++) {
        Str s(*v);
        // the empty choice means "unset"; it can't be a row you pick, so leave
        // it to the advanced settings dialog
        if (len(s) == 0 || (len(value) > 0 && !FilterMatches(s, src.filterWords))) {
            continue;
        }
        out.Append(s, data);
    }
}

// the rows the query selects, and the words to underlay in them
void FilterPaletteStrings(PaletteSources& src, Str filter, StrVecCP& out) {
    out.Reset();
    if (len(filter) == 0) {
        filter = StrL("");
    }

    PaletteMode mode = ParsePaletteMode(filter);
    if (mode == PaletteMode::Thumbnails) {
        return;
    }
    bool searchTabs = mode == PaletteMode::Tabs || mode == PaletteMode::Everything;
    bool searchHistory = mode == PaletteMode::FileHistory || mode == PaletteMode::Everything;
    bool searchCommands = mode == PaletteMode::Commands || mode == PaletteMode::Everything;
    bool searchToc = mode == PaletteMode::Toc;
    bool searchFavorites = mode == PaletteMode::Favorites;
    bool searchAnnotations = mode == PaletteMode::Annotations;
    bool searchSettings = mode == PaletteMode::Settings;

    src.filterWords.Reset();
    if (searchSettings) {
        Str path, value;
        if (SplitSettingValueQuery(filter, path, value)) {
            SplitFilterToWords(value, src.filterWords);
            FillSettingValueRows(src, path, value, out);
            // the rows are the values themselves: nothing to highlight
            src.filterWords.Reset();
            return;
        }
    }
    if (searchAnnotations) {
        AnnotMatchOpts opts;
        if (!ParseAnnotSearch(filter, opts)) {
            opts.Reset();
            StrVec words;
            SplitFilterToWords(filter, words);
            for (Str w : words) {
                AnnotSearchAddContentWord(opts, w);
            }
        }
        AnnotSearchContentWords(opts, src.filterWords);
        int n = len(src.annotations);
        for (int i = 0; i < n; i++) {
            ItemDataCP* data = src.annotations.AtData(i);
            if (data && AnnotMatches(data->annot, opts)) {
                out.AppendFrom(&src.annotations, i);
            }
        }
        return;
    }

    SplitFilterToWords(filter, src.filterWords);

    if (searchTabs) {
        FilterStrings(src.tabs, src.filterWords, out);
    }
    if (searchHistory) {
        FilterStrings(src.fileHistory, src.filterWords, out);
    }
    if (searchCommands) {
        FilterStrings(src.commands, src.filterWords, out);
    }
    if (searchToc) {
        FilterStrings(src.toc, src.filterWords, out);
    }
    if (searchFavorites) {
        FilterStrings(src.favorites, src.filterWords, out);
    }
    if (searchSettings) {
        FilterStrings(src.settings, src.filterWords, out);
    }
}

// how many rows the query's mode has before filtering, for "26 of 212"
int PaletteSourceCount(PaletteSources& src, Str filter) {
    PaletteMode mode = ParsePaletteMode(filter);
    switch (mode) {
        case PaletteMode::Tabs:
            return len(src.tabs);
        case PaletteMode::FileHistory:
            return len(src.fileHistory);
        case PaletteMode::Commands:
            return len(src.commands);
        case PaletteMode::Everything:
            return len(src.tabs) + len(src.fileHistory) + len(src.commands);
        case PaletteMode::Toc:
            return len(src.toc);
        case PaletteMode::Favorites:
            return len(src.favorites);
        case PaletteMode::Annotations:
            return len(src.annotations);
        case PaletteMode::Settings:
            return len(src.settings);
        default:
            return 0;
    }
}

// "212" when nothing is filtered out, "26 of 212" when the query narrows it
TempStr PaletteCountTemp(int shown, int total) {
    if (total <= 0) {
        return {};
    }
    if (shown >= total) {
        return fmt("%d", total);
    }
    return fmt(Tr("%d of %d").s, shown, total);
}

// the switches a palette offers: the modes with rows, in orig's order
int PaletteSwitches(PaletteSources& src, bool hasPageThumbnails, PaletteSwitch* out) {
    int n = 0;
    out[n++] = {Tr("> Commands"), Str(kPalettePrefixCommands)};
    out[n++] = {Tr("@ Tabs"), Str(kPalettePrefixTabs)};
    out[n++] = {Tr("# History"), Str(kPalettePrefixFileHistory)};
    if (len(src.favorites) > 0) {
        out[n++] = {Tr("$ Favorites"), Str(kPalettePrefixFavorites)};
    }
    if (len(src.toc) > 0) {
        out[n++] = {Tr("% TOC"), Str(kPalettePrefixTOC)};
    }
    if (hasPageThumbnails) {
        out[n++] = {Tr("& Thumbnails"), Str(kPalettePrefixThumbnails)};
    }
    if (len(src.annotations) > 0) {
        out[n++] = {Tr("* Annotations"), Str(kPalettePrefixAnnotations)};
    }
    out[n++] = {Tr("= Settings"), Str(kPalettePrefixBoolSettings)};
    ReportIf(n > kPaletteMaxSwitches);
    return n;
}

// the switch of the mode the query is in; an empty query lists commands
bool PaletteSwitchIsActive(Str filter, Str prefix) {
    if (len(filter) == 0) {
        return str::Eq(prefix, Str(kPalettePrefixCommands));
    }
    return len(prefix) > 0 && filter.s[0] == prefix.s[0];
}

static Str RestoreDefaultHelp() {
#if OS_DARWIN
    return Tr("Cmd+Backspace restore default");
#else
    return Tr("Del restore default");
#endif
}

// "Ctrl+Tab navigate" → "Ctrl+Tab" and "navigate"; the key is the first word,
// or the first two for "Release Ctrl select"
static PaletteHint SplitHint(Str s, int keyWords = 1) {
    int keyLen = 0;
    for (int w = 0; w < keyWords && keyLen < len(s); w++) {
        keyLen += (w > 0);
        while (keyLen < len(s) && s.s[keyLen] != ' ') {
            keyLen++;
        }
    }
    PaletteHint h;
    h.key = Str(s.s, keyLen);
    h.desc = keyLen < len(s) ? Str(s.s + keyLen + 1, len(s) - keyLen - 1) : Str();
    return h;
}

// the key hints under the list for a mode (PaletteHelpKind)
int PaletteHelpHints(int kind, PaletteHint* out) {
    int n = 0;
    switch (kind) {
        case kHelpSmartTab:
            out[n++] = SplitHint(Tr("Ctrl+Tab navigate"));
            out[n++] = SplitHint(Tr("Release Ctrl select"), 2);
            out[n++] = SplitHint(Tr("Space for sticky mode"));
            out[n++] = SplitHint(Tr("Del close tab"));
            break;
        case kHelpHistory:
            out[n++] = SplitHint(Tr("Enter open file"));
            out[n++] = SplitHint(Tr("Del remove from history"));
            out[n++] = SplitHint(Tr("Esc close"));
            break;
        case kHelpTabs:
            out[n++] = SplitHint(Tr("Enter switch to tab"));
            out[n++] = SplitHint(Tr("Del close tab"));
            out[n++] = SplitHint(Tr("Esc close"));
            break;
        case kHelpFavorites:
            out[n++] = SplitHint(Tr("Enter go to favorite"));
            out[n++] = SplitHint(Tr("Del remove favorite"));
            out[n++] = SplitHint(Tr("Esc close"));
            break;
        case kHelpAnnotations:
        case kHelpThumbnails:
        case kHelpToc:
            out[n++] = SplitHint(Tr("Enter go to"));
            out[n++] = SplitHint(Tr("Esc close"));
            break;
        case kHelpSettings:
            out[n++] = SplitHint(Tr("Enter change"));
            out[n++] = SplitHint(RestoreDefaultHelp());
            out[n++] = SplitHint(Tr("Esc close"));
            break;
        case kHelpSettingValue:
            out[n++] = SplitHint(Tr("Enter apply"));
            out[n++] = SplitHint(RestoreDefaultHelp());
            out[n++] = SplitHint(Tr("Esc go back"));
            break;
        case kHelpEverything:
            out[n++] = SplitHint(Tr("Enter select"));
            out[n++] = SplitHint(Tr("Esc close"));
            break;
        default:
            out[n++] = SplitHint(Tr("Enter run command"));
            out[n++] = SplitHint(Tr("Esc close"));
            break;
    }
    return n;
}

// what a row shows on its right: the shortcut, a setting's value, the page or
// chapter a toc entry or annotation goes to, a file's directory
TempStr PaletteRightColumnTemp(ItemDataCP* data) {
    if (data->cmdId != 0) {
        return CommandPaletteShortcutTemp(data->cmdId);
    }
    if (IsSettingRow(data) && len(data->settingPath) == 0) {
        return FormatSettingValueTemp(data->settingType, SettingRowPtr(data));
    }
    if (data->annot || data->pageNo > 0) {
        return fmt("p%d", data->pageNo);
    }
    if (data->tocItem && data->tocItem->loc.chapter >= 1) {
        return fmt("ch%d", data->tocItem->loc.chapter);
    }
    if (data->filePath) {
        return path::GetDirTemp(data->filePath);
    }
    return {};
}
