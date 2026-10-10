/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// The build can pass these defines (msbuild /p:SumatraBuildDefines=..., see
// build_defines() in premake5.lua):
//   PRE_RELEASE_VER 10175
//   GIT_COMMIT_ID 70cdc024f79167b607f59b77ea0b29dd155925cc

// CURR_VERSION can be over-written externally
#ifndef CURR_VERSION
#define CURR_VERSION 3.7
#endif
#ifndef CURR_VERSION_COMMA
#define CURR_VERSION_COMMA 3, 7, 0
#endif

// this is sth. like "3.5"
#define CURR_VERSION_MAJOR_STRA QM(CURR_VERSION)

#define _QUOTEME(x) #x
#define QM(x) _QUOTEME(x)
#define _QUOTEME3(x, y, z) _QUOTEME(x##y##z)
#define QM3(x, y, z) _QUOTEME3(x, y, z)

// version as displayed in UI and included in resources
// CURR_VERSION is 3.6.16105 for pre-release builds
#ifndef PRE_RELEASE_VER
#define CURR_VERSION_STRA QM(CURR_VERSION)
#define VER_RESOURCE_STR CURR_VERSION_STRA
#define VER_RESOURCE CURR_VERSION_COMMA, 0
#define UPDATE_CHECK_VER TEXT(QM(CURR_VERSION))
#define UPDATE_CHECK_VERA QM(CURR_VERSION)
#else
#define CURR_VERSION_STRA QM3(CURR_VERSION, ., PRE_RELEASE_VER)
#define VER_RESOURCE_STR QM3(CURR_VERSION, .0., PRE_RELEASE_VER)
#define VER_RESOURCE CURR_VERSION_COMMA, PRE_RELEASE_VER
#define UPDATE_CHECK_VER TEXT(QM(PRE_RELEASE_VER))
#define UPDATE_CHECK_VERA QM(PRE_RELEASE_VER)
#endif
#define CURR_VERSION_STR TEXT(CURR_VERSION_STRA)

#define kCopyrightStr "Copyright 2006-2026 all authors (GPLv3)"
#define kPublisherStr "Krzysztof Kowalczyk"

#define kAppName "SumatraPDF"
