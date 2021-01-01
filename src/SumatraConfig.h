/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// those are defined in SumatraStartup.cpp
// those are set based on various pre-processor defines
// but we prefer to use variables. this way ensure
// the code compiles
extern bool gIsDebugBuild;
extern bool gIsAsanBuild;
extern bool gIsDailyBuild;
extern bool gIsPreReleaseBuild;
extern bool gWithTocEditor;
extern bool gIsRaMicroBuild;
extern bool gOwnerDrawMenu;
extern bool gShowDebugMenu;
extern bool gDisableDocumentRestrictions;
extern const char* builtOn;
extern const char* currentVersion; // e.g. "3.2.1138"
extern const char* gitSha1;
extern const char* preReleaseVersion;

const WCHAR* GetAppName();
const WCHAR* GetExeName();

int GetAppIconID();
