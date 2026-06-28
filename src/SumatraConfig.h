/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// those are defined in SumatraStartup.cpp
// those are set based on various pre-processor defines
// but we prefer to use variables. this way ensure
// the code compiles
extern bool gIsDebugBuild;
extern bool gIsAsanBuild;
extern bool gIsPreReleaseBuild;
extern bool gIsStoreBuild;
extern bool gDisableDocumentRestrictions;
// set by -for-testing cmd-line flag, used for ad-hoc testing by humans
// or agents. Always starts a new instance, doesn't restore a session and
// doesn't save settings
extern bool gForTesting;
extern Str builtOn;
extern Str currentVersion; // e.g. "3.2.1138"
extern Str gitCommidId;
extern Str preReleaseVersion;

constexpr const char* kExeName = "SumatraPDF.exe";

int GetAppIconID();
