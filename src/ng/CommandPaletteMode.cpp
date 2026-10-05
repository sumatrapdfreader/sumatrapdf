/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: the prefix half of orig's CommandPalette.cpp. orig decides the mode
// inline in FilterStringsForQuery(); here it is one function in the `app`
// library, so the palette view and `test_util` agree on what a prefix means.

#include "base/Base.h"

#include "CommandPalette.h"

PaletteMode PaletteModeFromQuery(Str query, Str* restOut) {
    Str rest = query;
    PaletteMode mode = PaletteMode::Commands;
    if (str::TrimPrefix(rest, Str(kPalettePrefixEverything))) {
        mode = PaletteMode::Everything;
    } else if (str::TrimPrefix(rest, Str(kPalettePrefixTabs))) {
        mode = PaletteMode::Tabs;
    } else if (str::TrimPrefix(rest, Str(kPalettePrefixFileHistory))) {
        mode = PaletteMode::FileHistory;
    } else if (str::TrimPrefix(rest, Str(kPalettePrefixTOC))) {
        mode = PaletteMode::Toc;
    } else if (str::TrimPrefix(rest, Str(kPalettePrefixFavorites))) {
        mode = PaletteMode::Favorites;
    } else if (str::TrimPrefix(rest, Str(kPalettePrefixAnnotations))) {
        mode = PaletteMode::Annotations;
    } else if (str::TrimPrefix(rest, Str(kPalettePrefixBoolSettings))) {
        mode = PaletteMode::Settings;
    } else if (str::TrimPrefix(rest, Str(kPalettePrefixThumbnails))) {
        mode = PaletteMode::Thumbnails;
    } else {
        str::TrimPrefix(rest, Str(kPalettePrefixCommands));
    }
    if (restOut) {
        *restOut = rest;
    }
    return mode;
}
