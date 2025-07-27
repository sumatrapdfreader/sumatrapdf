/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

constexpr const char* kPalettePrefixCommands = ">";
constexpr const char* kPalettePrefixFileHistory = "#";
constexpr const char* kPalettePrefixTabs = "@";
constexpr const char* kPalettePrefixAll = ":";

void RunCommandPallette(MainWindow*, const char* prefix, int smartTabAdvance);
